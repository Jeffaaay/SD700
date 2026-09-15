"""Conservative file-only cleanup: dry-run by default, exact hashed plan to apply.

Never removes trees, source, firmware, capture evidence, logs or Git data.
The sole tracked-file exception is the verified duplicate root INA240 PDF.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SCOPES = ('output', 'tools/__pycache__', 'Tests/Host/__pycache__')
KEEP_MARKER = re.compile(r'(^|[-_])(final\d*|repro|before|fail|failed|failure)([-_]|$)', re.I)
PDF = 'ina240.pdf'
PDF_REFERENCE = 'Reference/Hardware/ina240.pdf'


def require(ok, message):
    if not ok:
        raise ValueError(message)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args]).decode().strip()


def tracked_files(root):
    return set(filter(None, git(root, 'ls-files', '-z').split('\0')))


def safe_path(root, relative):
    """Reject traversal, aliases, symlinks and Windows junctions at every level."""
    require(isinstance(relative, str) and '\\' not in relative and ':' not in relative,
            'Non-canonical relative path')
    parts = PurePosixPath(relative).parts
    require(parts and not relative.startswith('/') and all(p.lower() not in ('.', '..', '.git') for p in parts)
            and '/'.join(parts) == relative, 'Unsafe relative path')
    at = root.resolve()
    for part in parts:
        at = at / part
        require(not at.is_symlink() and not at.is_junction(), 'Link/junction path rejected: '+relative)
    require(at.resolve().is_relative_to(root.resolve()), 'Path escapes workspace')
    return at


def walk_files(root, relative):
    base = safe_path(root, relative)
    if not base.exists():
        return
    for at, dirs, files in os.walk(base, followlinks=False):
        dirs[:] = sorted(d for d in dirs if d != '.git' and not Path(at, d).is_symlink()
                         and not Path(at, d).is_junction())
        for name in sorted(files):
            p = Path(at, name)
            if not p.is_symlink() and not p.is_junction():
                yield p.relative_to(root).as_posix()


def preservation_roots(root):
    keep = set()
    # Preserve complete explicitly recorded failed runs, including their binaries.
    for relative in walk_files(root, 'output'):
        p = root / relative
        if p.name not in ('test_execution.json', 'results.json'):
            continue
        try:
            data = json.loads(p.read_text(encoding='utf-8-sig'))
            rows = data if isinstance(data, list) else data.get('results', [])
            if any(isinstance(r, dict) and (r.get('result') == 'FAIL' or
                   r.get('exit_code', 0) != 0) for r in rows):
                keep.add(p.parent.relative_to(root).as_posix())
        except (ValueError, TypeError, AttributeError):
            keep.add(p.parent.relative_to(root).as_posix())
    # The current final verification tree is evidence, not a cleanup target.
    manifest = root / 'Docs/BuildToTarget2_CoolingAnchorFix1/verification.json'
    if manifest.exists():
        for row in json.loads(manifest.read_text()):
            keep.add(PurePosixPath(row['log']).parent.as_posix())
    return sorted(keep)


def eligible(root, relative, tracked, keep):
    p = safe_path(root, relative)
    if relative == PDF:
        reference = safe_path(root, PDF_REFERENCE)
        require(reference.is_file() and sha(p) == sha(reference), 'Root PDF is not an exact duplicate')
        return 'duplicate_root_pdf'
    if relative.lower() in {p.lower() for p in tracked} or not any(relative.startswith(s+'/') for s in SCOPES):
        return None
    parts = PurePosixPath(relative).parts
    if 'firmware' in [p.lower() for p in parts] or any(KEEP_MARKER.search(x) for x in parts[:-1]):
        return None
    if any(relative.lower().startswith(k.lower()+'/') for k in keep):
        return None
    if p.suffix.lower() in ('.o', '.obj', '.d') and parts[0] == 'output':
        return 'regenerable_object_or_dependency'
    if p.suffix.lower() == '.pyc' and '__pycache__' in parts:
        return 'python_cache'
    if p.suffix.lower() == '.exe' and parts[0] == 'output' and (
            p.name.startswith('force_servo_') or any('host' in x.lower() or x == 'gates' for x in parts)):
        return 'regenerable_host_test_executable'
    return None


def fingerprint(root, paths):
    lines = []; size = 0
    for relative in sorted(paths):
        p = safe_path(root, relative)
        require(p.is_file(), 'Retained file missing: '+relative)
        lines.append(sha(p)+' *'+relative+'\n'); size += p.stat().st_size
    return dict(files=len(lines), bytes=size,
                path_and_content_sha256=hashlib.sha256(''.join(lines).encode()).hexdigest().upper())


def retained_paths(root, tracked, removed):
    paths = set(tracked) | set(walk_files(root, 'Reference'))
    for scope in SCOPES:
        paths.update(walk_files(root, scope))
    return sorted(paths-set(removed))


def make_plan(root):
    root = root.resolve(); tracked = tracked_files(root); keep = preservation_roots(root)
    paths = set()
    for scope in SCOPES:
        paths.update(walk_files(root, scope))
    if (root/PDF).exists():
        paths.add(PDF)
    entries = []
    for relative in sorted(paths):
        reason = eligible(root, relative, tracked, keep)
        if reason:
            p = safe_path(root, relative)
            entries.append(dict(path=relative, bytes=p.stat().st_size, sha256=sha(p), reason=reason))
    retained = retained_paths(root, tracked, [e['path'] for e in entries])
    return dict(version=1, mode='DRY_RUN', root=str(root), head=git(root, 'rev-parse', 'HEAD'),
                created_utc=datetime.now(timezone.utc).isoformat(), preservation_roots=keep,
                entries=entries, delete_files=len(entries), delete_bytes=sum(e['bytes'] for e in entries),
                retained_fingerprint=fingerprint(root, retained))


def active_tasks(root):
    """Read-only process inventory; never terminates a task or opens a port."""
    require(os.name == 'nt', 'Automatic idle check requires Windows; no apply on unknown platform')
    script = ('Get-CimInstance Win32_Process | Where-Object { $_.ProcessId -ne $PID } | '
              'Select-Object ProcessId,Name,ExecutablePath,CommandLine | ConvertTo-Json -Compress')
    result = subprocess.run(['powershell.exe', '-NoProfile', '-Command', script],
                            capture_output=True, text=True, check=True)
    data = json.loads(result.stdout)
    if isinstance(data, dict):
        data = [data]
    require(isinstance(data, list), 'Cannot establish process inventory')
    matches = []
    task = re.compile(r'(?:tools[/\\](?:verify_|build_|run_host_tests|run_force_servo_tests|capture_|run_force_characterization)|Tests[/\\]Host[/\\]test_)', re.I)
    for proc in data:
        if proc['ProcessId'] == os.getpid():
            continue
        name = (proc['Name'] or '').lower(); cmd = proc['CommandLine'] or ''
        exe = (proc['ExecutablePath'] or '').lower()
        compiler = re.search(r'(?:gcc|cc1(?:plus)?|collect2|objcopy|(?:^|-)as|(?:^|-)ld)\.exe$', name)
        runner = name in ('python.exe', 'python3.exe', 'powershell.exe', 'pwsh.exe') and task.search(cmd)
        local_binary = exe.startswith(str(root.resolve()).lower()+os.sep) and name.endswith('.exe')
        if compiler or runner or local_binary:
            matches.append(dict(pid=proc['ProcessId'], name=proc['Name']))
    return matches


def apply_plan(root, plan, idle_check=active_tasks):
    root = root.resolve()
    require(plan.get('version') == 1 and plan.get('mode') == 'DRY_RUN', 'Invalid plan format')
    require(str(root) == plan['root'] and git(root, 'rev-parse', 'HEAD') == plan['head'], 'Workspace/HEAD changed')
    entries = plan['entries']; paths = [e['path'] for e in entries]
    require(len(paths) == len(set(paths)), 'Duplicate cleanup path')
    require(len(paths) == plan['delete_files'] and sum(e['bytes'] for e in entries) == plan['delete_bytes'], 'Plan totals disagree')
    tracked = tracked_files(root); keep = preservation_roots(root)
    require(keep == plan['preservation_roots'], 'Preserved evidence changed')
    # Preflight every file and retained byte before the first deletion.
    for e in entries:
        p = safe_path(root, e['path'])
        require(p.is_file(), 'Cleanup file missing: '+e['path'])
        require(eligible(root, e['path'], tracked, keep) == e['reason'] and e['reason'] is not None,
                'Protected/ineligible cleanup file: '+e['path'])
        require(p.stat().st_size == e['bytes'] and sha(p) == e['sha256'], 'Cleanup file changed: '+e['path'])
    retained = retained_paths(root, tracked, paths)
    require(fingerprint(root, retained) == plan['retained_fingerprint'], 'Retained bytes changed since dry-run')
    active = idle_check(root)
    require(not active, 'Active build/test/capture tasks; no cleanup: '+json.dumps(active))
    for e in entries:
        p = safe_path(root, e['path'])
        require(sha(p) == e['sha256'], 'Cleanup file changed during apply: '+e['path'])
        p.unlink()  # File-only removal of a resolved, checked path; never a tree.
    after = fingerprint(root, retained)
    require(after == plan['retained_fingerprint'], 'Retained bytes changed during cleanup')
    return dict(result='PASS', head=plan['head'], applied_utc=datetime.now(timezone.utc).isoformat(),
                active_tasks=active, deleted_files=len(paths), deleted_bytes=plan['delete_bytes'],
                retained_before=plan['retained_fingerprint'], retained_after=after,
                physical_status='NOT_RUN')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--plan', required=True, help='Exact JSON inventory; created by default, read with --apply')
    parser.add_argument('--apply', action='store_true', help='Delete only the unchanged eligible files in the saved plan')
    parser.add_argument('--result', help='New apply receipt JSON (required with --apply)')
    args = parser.parse_args()
    plan_path = safe_path(ROOT, args.plan)
    if args.apply:
        require(args.result, '--apply requires a new --result path')
        result_path = safe_path(ROOT, args.result)
        require(not result_path.exists(), 'Never overwrite a cleanup receipt')
        plan_hash = sha(plan_path)
        record = apply_plan(ROOT, json.loads(plan_path.read_text()))
        record.update(plan=args.plan, plan_sha256=plan_hash)
        with result_path.open('x', encoding='utf-8', newline='\n') as f:
            f.write(json.dumps(record, indent=2)+'\n')
        print(json.dumps(record, indent=2))
    else:
        require(not args.result, '--result is only for --apply')
        require(not plan_path.exists(), 'Never overwrite a dry-run inventory')
        record = make_plan(ROOT)
        with plan_path.open('x', encoding='utf-8', newline='\n') as f:
            f.write(json.dumps(record, indent=2)+'\n')
        print('DRY_RUN_ONLY files='+str(record['delete_files'])+' bytes='+str(record['delete_bytes']))


if __name__ == '__main__':
    main()
