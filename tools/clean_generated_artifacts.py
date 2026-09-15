"""Clean ignored output artifacts only. Default: saved dry-run; --apply deletes.

Tracked files, nonignored files, physical/uncertain captures and explicit test
inputs survive. Audits live outside the checkout. No source/firmware rewrites.
"""
import argparse
from collections import defaultdict
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import subprocess
import tempfile
import uuid


def require(ok, message):
    if not ok:
        raise ValueError(message)


def git(root, *args, data=None):
    r = subprocess.run(['git', '-C', str(root), *args], input=data, capture_output=True)
    require(r.returncode in ((0, 1) if args[0] == 'check-ignore' else (0,)), r.stderr.decode(errors='replace'))
    return r.stdout


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def safe_output(root, relative):
    require(isinstance(relative, str) and '\\' not in relative and ':' not in relative, 'Noncanonical path')
    parts = PurePosixPath(relative).parts
    require(parts and parts[0] == 'output' and '/'.join(parts) == relative and
            all(p not in ('.', '..') and p.rstrip(' .') == p for p in parts), 'Not a canonical output path')
    for i, part in enumerate(parts):
        if part.lower() == '.git':
            require(i >= 2 and parts[i-1] == 'clone_verification', 'Only nested clone_verification/.git is removable')
    at = root.resolve()
    for part in parts:
        at /= part
        require(not at.is_symlink() and not at.is_junction(), 'Symlink/junction rejected: '+relative)
    require(at.resolve().is_relative_to(root.resolve()/'output'), 'Resolved path escapes output')
    return at


def inventory(root):
    files = {}; directories = []; protected_trees = []
    base = safe_output(root, 'output')
    if not base.exists():
        return files, directories, protected_trees
    for at, dirs, names in os.walk(base, followlinks=False):
        for name in list(dirs):
            p = Path(at, name); rel = p.relative_to(root).as_posix()
            if p.is_symlink() or p.is_junction() or (name.lower() == '.git' and p.parent.name != 'clone_verification'):
                protected_trees.append(rel); dirs.remove(name)
            else:
                directories.append(rel)
        for name in names:
            p = Path(at, name); rel = p.relative_to(root).as_posix()
            if p.is_symlink() or p.is_junction() or name.lower() == '.git':
                protected_trees.append(rel)
            else:
                safe_output(root, rel)
                files[rel] = dict(bytes=p.stat().st_size, sha256=sha(p))
    return files, sorted(directories), sorted(protected_trees)


def capture_audit(root, files):
    """Ambiguous captures survive even inside a directory called dev/test/final."""
    families = set(); keep_dirs = set(); audit = []
    suffixes = ('.metadata.json', '.meta.json', '.report.txt', '.csv')
    for relative in files:
        for suffix in suffixes:
            if relative.lower().endswith(suffix):
                families.add(relative[:-len(suffix)]); break
        if PurePosixPath(relative).name.lower() in ('metadata.json', 'report.txt'):
            keep_dirs.add(PurePosixPath(relative).parent.as_posix())
    for stem in sorted(families):
        meta = root/(stem+'.metadata.json'); synthetic = False
        try:
            d = json.loads(meta.read_text(encoding='utf-8-sig'))
            # A filename or NOT_RUN tag alone is never proof of synthetic data.
            synthetic = (PurePosixPath(stem).name == 'SYNTHETIC' and
                         d.get('firmware_sha256') in ('SYNTHETIC_NOT_DEVICE', 'SYNTHETIC_NO_HARDWARE') and
                         str(d.get('initial_gap', '')).startswith('SYNTHETIC') and
                         str(d.get('current_limit_setting', '')).startswith('SYNTHETIC') and
                         not any(re.search(r'\bCOM\d+\b', str(d.get(k, '')), re.I)
                                 for k in ('port', 'serial_port', 'bus')))
        except (OSError, ValueError, AttributeError):
            pass
        if not synthetic:
            keep_dirs.add(PurePosixPath(stem).parent.as_posix())
        audit.append(dict(stem=stem, classification='SYNTHETIC' if synthetic else 'PHYSICAL_OR_UNCERTAIN_KEEP'))
    return sorted(keep_dirs), audit


def explicit_inputs(root, tracked, files):
    """Conservatively retain existing literal output-file references in tests/tools.

    Computed external fixture paths can additionally be named with --keep.
    Firmware dependencies in the current verifier are all Git tracked.
    """
    refs = set()
    for relative in tracked:
        if not relative.startswith(('tools/', 'Tests/')) or Path(relative).suffix not in ('.py', '.ps1', '.c', '.h'):
            continue
        source = (root/relative).read_text(encoding='utf-8-sig', errors='replace')
        for match in re.findall(r'output[/\\][A-Za-z0-9_./\\-]+', source):
            path = match.replace('\\', '/')
            if path in files:
                refs.add(path)
    return sorted(refs)


def snapshot(root, keep=()):
    root = root.resolve()
    require(Path(git(root, 'rev-parse', '--show-toplevel').decode().strip()).resolve() == root, 'Root must be the Git checkout')
    tracked = sorted(filter(None, git(root, 'ls-files', '-z').decode().split('\0')))
    files, dirs, protected_trees = inventory(root)
    candidates = list(files)+[p+'/' for p in dirs]
    ignored = set(filter(None, git(root, 'check-ignore', '--no-index', '--stdin', '-z',
                                  data=('\0'.join(candidates)+'\0').encode()).decode().split('\0'))) if candidates else set()
    captures, capture_rows = capture_audit(root, files)
    references = explicit_inputs(root, tracked, files)
    keep = sorted(set(keep))
    for p in keep:
        safe_output(root, p)
    protected = captures+keep
    tracked_lower = {p.lower() for p in tracked}
    entries = []; retained = []
    for relative, info in sorted(files.items()):
        parts = PurePosixPath(relative).parts
        if relative.lower() in tracked_lower:
            reason = 'GIT_TRACKED'
        elif relative not in ignored:
            reason = 'NOT_IGNORED'
        elif relative in references:
            reason = 'EXPLICIT_TEST_TOOL_INPUT'
        elif any(p.lower() in ('field', 'captures', 'physical', 'physical_captures') for p in parts[1:-1]):
            reason = 'FIELD_DIRECTORY'
        elif any(relative == p or relative.startswith(p+'/') for p in protected):
            reason = 'PHYSICAL_UNCERTAIN_CAPTURE_OR_EXPLICIT_KEEP'
        else:
            reason = None
        row = dict(path=relative, **info)
        (retained if reason else entries).append(dict(**row, reason=reason or 'IGNORED_GENERATED_OUTPUT'))
    groups = defaultdict(lambda: dict(files=0, bytes=0))
    for e in entries:
        group = '/'.join(e['path'].split('/')[:2]); groups[group]['files'] += 1; groups[group]['bytes'] += e['bytes']
    protected_hashes = {}
    for p in tracked:
        file = root/p
        require(file.is_file(), 'Tracked file missing: '+p)
        protected_hashes[p] = sha(file)
    return dict(version=1, root=str(root), head=git(root, 'rev-parse', 'HEAD').decode().strip(),
                git_status=git(root, 'status', '--porcelain').decode(), keep=keep,
                before_files=len(files), before_bytes=sum(f['bytes'] for f in files.values()),
                removed_files=len(entries), removed_bytes=sum(e['bytes'] for e in entries),
                expected_after_files=len(retained), expected_after_bytes=sum(e['bytes'] for e in retained),
                directory_groups=dict(groups), capture_audit=capture_rows, explicit_inputs=references,
                skipped_link_or_foreign_git_trees=protected_trees, entries=entries, retained=retained,
                directories=sorted(p for p in dirs if p+'/' in ignored and
                                   not any(p == k or p.startswith(k+'/') for k in protected) and
                                   not any(x.lower() in ('field', 'captures', 'physical', 'physical_captures')
                                           for x in PurePosixPath(p).parts[1:])), tracked_sha256=protected_hashes)


def active_tasks(root):
    # Reuse the established read-only Windows process check; no serial I/O.
    import importlib.util
    spec = importlib.util.spec_from_file_location('sd700_cleanup_idle', root/'tools/cleanup_generated.py')
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    return module.active_tasks(root)


def apply_plan(root, plan, idle_check=active_tasks):
    require(plan.get('version') == 1, 'Unsupported inventory')
    # Recompute the entire classification, ignore policy, source dependencies,
    # file hashes, capture evidence, HEAD and status before the first deletion.
    current = snapshot(root, plan['keep'])
    require(current == plan, 'Inventory changed since dry-run; no files deleted')
    active = idle_check(root)
    require(not active, 'Active build/test/capture tasks; no cleanup: '+json.dumps(active))
    for entry in plan['entries']:
        p = safe_output(root, entry['path'])
        require(sha(p) == entry['sha256'], 'Artifact changed during apply: '+entry['path'])
        try:
            p.unlink()
        except PermissionError:
            # Disposable nested Git packs can have the read-only file bit set.
            require('.git' in p.parts and not p.stat().st_mode & stat.S_IWRITE, 'Unexpected deletion permission failure')
            os.chmod(p, stat.S_IREAD | stat.S_IWRITE); p.unlink()
    removed_dirs = []
    for relative in sorted(plan['directories'], key=lambda p: (-p.count('/'), p)):
        p = safe_output(root, relative)
        if p.is_dir() and not any(p.iterdir()):
            p.rmdir(); removed_dirs.append(relative)  # Empty, resolved directory only; no recursive delete.
    for relative, expected in plan['tracked_sha256'].items():
        require(sha(root/relative) == expected, 'Tracked bytes changed: '+relative)
    after, _, _ = inventory(root)
    require(after == {e['path']:dict(bytes=e['bytes'], sha256=e['sha256']) for e in plan['retained']}, 'Retained output differs')
    require(git(root, 'status', '--porcelain').decode() == plan['git_status'], 'Git status changed during cleanup')
    return dict(result='PASS', head=plan['head'], active_tasks=active, git_status=plan['git_status'],
                before_size=plan['before_bytes'], after_size=sum(e['bytes'] for e in after.values()),
                removed_files=plan['removed_files'], removed_bytes=plan['removed_bytes'],
                removed_directories=removed_dirs, tracked_bytes_unchanged=True,
                physical_status='NOT_RUN', completed_utc=datetime.now(timezone.utc).isoformat())


def write_new(path, value):
    with path.open('x', encoding='utf-8', newline='\n') as f:
        f.write(json.dumps(value, indent=2)+'\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--apply', action='store_true', help='Apply the saved/recomputed exact inventory')
    parser.add_argument('--plan', type=Path, help='Existing plan with --apply; new plan destination otherwise')
    parser.add_argument('--keep', action='append', default=[], help='Additional output file/directory fixture to preserve')
    args = parser.parse_args(); root = args.root.resolve()
    plan_path = args.plan.resolve() if args.plan else Path(tempfile.mkdtemp(prefix='SD700_generated_cleanup_'))/'dry-run.json'
    require(not plan_path.is_relative_to(root), 'Store cleanup audits outside the checkout/output')
    if args.apply and plan_path.exists():
        require(not args.keep, '--keep belongs in the saved dry-run plan')
        plan = json.loads(plan_path.read_text())
    else:
        plan = snapshot(root, args.keep); write_new(plan_path, plan)
    print(json.dumps(dict(mode='APPLY' if args.apply else 'DRY_RUN', plan=str(plan_path),
                         before_size=plan['before_bytes'], removed_files=plan['removed_files'],
                         removed_bytes=plan['removed_bytes'], expected_after_size=plan['expected_after_bytes'],
                         capture_groups=len(plan['capture_audit']),
                         uncertain_capture_groups=sum(r['classification'] != 'SYNTHETIC' for r in plan['capture_audit']),
                         directory_groups=plan['directory_groups']), indent=2), flush=True)
    if args.apply:
        receipt = plan_path.with_name(plan_path.stem+'.applied-'+uuid.uuid4().hex[:8]+'.json')
        result = apply_plan(root, plan)
        result.update(plan=str(plan_path), plan_sha256=sha(plan_path)); write_new(receipt, result)
        print(json.dumps(dict(result='PASS', receipt=str(receipt), after_size=result['after_size'],
                             removed_files=result['removed_files'], removed_bytes=result['removed_bytes'],
                             git_status=result['git_status'], physical_status='NOT_RUN'), indent=2))


if __name__ == '__main__':
    main()
