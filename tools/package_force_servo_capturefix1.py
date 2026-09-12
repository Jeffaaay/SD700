"""Create one new CaptureFix1 delivery from committed main; preserve earlier deliveries."""
import json
from pathlib import Path
import subprocess
import zipfile

from firmware_image import require
from verify_force_servo_firmware import sha, verify
from verify_force_servo_capturefix1_package import MANIFEST, digest, verify_entries, verify_path

ROOT = Path(__file__).resolve().parent.parent
BASELINE = '29d0a9091b8333c690bcb41f5474dffc3909ef61'


def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT).decode().strip()


def main():
    require(not git('status', '--porcelain'), 'Commit reviewable work before packaging')
    require(git('branch', '--show-current') == 'main', 'Package main only')
    firmware = verify()
    proof = json.loads((ROOT/'Docs/ForceServo1_CaptureFix1/verification.json').read_text())
    before = json.loads((ROOT/'output/ForceServo1_CaptureFix1/before.json').read_text())
    require(before['head'] == BASELINE, 'Unexpected recorded starting point')
    for name, item in before['evidence'].items():
        path = ROOT/name
        require(sha(path) == item['sha256'] and path.stat().st_mtime_ns == item['mtime_ns'],
                'Historical evidence changed: '+name)
    for name, value in proof['production_sources_unchanged'].items():
        require(sha(ROOT/name) == value == before['tracked'][name], 'Production source changed: '+name)
    for name, value in proof['sources_at_verification'].items():
        require(sha(ROOT/name) == value, 'Source changed after verification: '+name)
    paths = set(git('ls-files', '-z').split('\0'))-{''}
    # Old verification describes its historical source, not these repaired tools.
    # Check that source at the reviewed commit and all original log hashes.
    old = json.loads((ROOT/'Docs/ForceServo1/verification.json').read_text())
    for name, value in old['sources_at_verification'].items():
        data = subprocess.check_output(['git', 'show', BASELINE+':'+name], cwd=ROOT)
        require(digest(data) == value, 'Historical verified source mismatch: '+name)
    for record in old['baseline']+old['final']:
        require(sha(ROOT/record['log']) == record['sha256'], 'Historical log changed: '+record['log'])
        paths.add(record['log'])
    for name in ('O0', 'O2', 'Os'):
        paths.add('output/ForceServo1/host/'+name+'.log')
    for file in (ROOT/'output/ForceServo1_CaptureFix1').rglob('*'):
        if file.is_file() and file.suffix.lower() not in ('.exe', '.pyc', '.zip'):
            paths.add(file.relative_to(ROOT).as_posix())
    commit = git('rev-parse', 'HEAD')
    remote = git('ls-remote', 'origin', 'refs/heads/main').split()[0]
    require(remote == commit, 'Remote main must match packaged source commit')
    entries = {name:(ROOT/name).read_bytes() for name in sorted(paths)}
    meta = dict(candidate='ForceServo1 CaptureFix1', source_commit=commit, remote_main=remote,
                baseline=BASELINE, firmware=firmware, physical_output='LOCKED', physical_test='NOT_RUN',
                hardware_stop_validation='NOT_VALIDATED', static_250='NOT_VALIDATED', rotating_load='NOT_VALIDATED',
                arm_rebuild='NOT_RUN_TOOLS_ONLY_UNCHANGED_FIRMWARE',
                preserved_historical_evidence=len(before['evidence']))
    entries['SOURCE_OF_TRUTH.json'] = (json.dumps(meta, indent=2)+'\n').encode()
    entries[MANIFEST] = ''.join(digest(data)+' *'+name+'\n'
                                for name, data in sorted(entries.items())).encode()
    verify_entries(entries)
    target = ROOT/'output/SD700_ForceServo1_CaptureFix1_SourceOfTruth.zip'
    require(not target.exists() and not target.with_suffix('.zip.sha256').exists(),
            'Never overwrite delivered ZIP or hash sidecar')
    with zipfile.ZipFile(target, 'x', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, data in sorted(entries.items()):
            archive.writestr('SD700/'+name, data)
    result = verify_path(target)
    value = sha(target)
    with target.with_suffix('.zip.sha256').open('x', encoding='ascii', newline='\n') as sidecar:
        sidecar.write(value+' *'+target.name+'\n')
    result.update(zip=str(target), zip_sha256=value)
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
