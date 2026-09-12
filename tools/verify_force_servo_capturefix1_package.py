"""Offline CaptureFix1 package integrity + locked firmware verification (stdlib only)."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import zipfile

from firmware_image import require
from verify_force_servo_firmware import PINNED_HASHES, STEM, verify_contents

MANIFEST = 'ForceServo1_CaptureFix1_PACKAGE_SHA256SUMS.txt'


def digest(data):
    return hashlib.sha256(data).hexdigest().upper()


def manifest_entries(data):
    result = {}
    for line in data.decode('utf-8').splitlines():
        match = re.fullmatch(r'([0-9A-F]{64}) \*(.+)', line)
        require(match is not None, 'Malformed package manifest')
        value, name = match.groups()
        path = PurePosixPath(name)
        require(not path.is_absolute() and str(path) == name and
                all(part not in ('.', '..') for part in path.parts) and
                '\\' not in name and ':' not in name and name not in result,
                'Unsafe or duplicate manifest path: '+name)
        result[name] = value
    require(result, 'Empty package manifest')
    return result


def verify_entries(entries):
    require(MANIFEST in entries, 'Package manifest missing')
    manifest = manifest_entries(entries[MANIFEST])
    require(set(entries) == set(manifest) | {MANIFEST}, 'Unexpected or missing package entries')
    for name, expected in manifest.items():
        require(digest(entries[name]) == expected, 'Package hash mismatch: '+name)
    source = manifest_entries(entries['SHA256SUMS.txt'])
    for name, expected in source.items():
        require(name in entries and digest(entries[name]) == expected, 'Source hash mismatch: '+name)
    meta = json.loads(entries['SOURCE_OF_TRUTH.json'])
    require(meta['candidate'] == 'ForceServo1 CaptureFix1' and
            re.fullmatch('[0-9a-f]{40}', meta['source_commit']) and
            meta['remote_main'] == meta['source_commit'] and
            meta['baseline'] == '29d0a9091b8333c690bcb41f5474dffc3909ef61',
            'Package source identity mismatch')
    require(meta['physical_test'] == 'NOT_RUN' and meta['physical_output'] == 'LOCKED' and
            all(meta[key] == 'NOT_VALIDATED' for key in
                ('hardware_stop_validation', 'static_250', 'rotating_load')),
            'Unexpected hardware validation claim')
    proof = json.loads(entries['Docs/ForceServo1_CaptureFix1/verification.json'])
    require(proof['baseline_commit'] == meta['baseline'], 'Verification baseline mismatch')
    required_checks = {'capture_length_and_observe', 'force_capture_selftest', 'pressure_capture_selftest',
                       'auto_capture_selftest', 'force_host', 'data_schema', 'verifier_rejections',
                       'offline_firmware', 'historical_auto_firmware'}
    require(required_checks <= {r['name'] for r in proof['checks']}, 'Required software check missing')
    for name, expected in proof['sources_at_verification'].items():
        require(name in entries and digest(entries[name]) == expected, 'Verified source mismatch: '+name)
    for record in proof['checks']:
        require(record['exit_code'] == 0 and record['result'] == 'PASS' and
                digest(entries[record['log']]) == record['sha256'], 'Invalid check record: '+record['name'])
    expected = {f'../output/ForceServo1/firmware/{STEM}.{ext}':value for ext,value in PINNED_HASHES.items()}
    firmware_lines = entries['Firmware/ForceServo1.SHA256SUMS.txt'].decode().splitlines()
    require(len(firmware_lines) == len(expected), 'Firmware manifest must contain exactly the pinned pair')
    firmware_manifest = dict(line.split(' *', 1)[::-1] for line in firmware_lines)
    require(firmware_manifest == expected, 'Pinned firmware manifest mismatch')
    firmware = {}
    for ext, value in PINNED_HASHES.items():
        firmware[ext] = entries[f'output/ForceServo1/firmware/{STEM}.{ext}']
        require(digest(firmware[ext]) == value, 'Pinned firmware hash mismatch: '+ext)
    image = verify_contents(firmware['elf'], firmware['hex'])
    return dict(package='PASS', candidate=meta['candidate'], source_commit=meta['source_commit'],
                verified_entries=len(entries), load_bytes_compared=len(image.load_bytes),
                physical_test='NOT_RUN', hardware_stop_validation='NOT_VALIDATED',
                static_250='NOT_VALIDATED', rotating_load='NOT_VALIDATED')


def verify_path(path):
    if path.is_dir():
        names = manifest_entries((path/MANIFEST).read_bytes())
        entries = {}
        for name in list(names)+[MANIFEST]:
            file = path/name
            require(file.resolve().is_relative_to(path.resolve()) and not file.is_symlink(),
                    'Package path escapes extraction directory: '+name)
            entries[name] = file.read_bytes()
    else:
        with zipfile.ZipFile(path) as archive:
            require(archive.testzip() is None, 'Corrupt ZIP entry')
            names = archive.namelist()
            require(len(set(names)) == len(names) and all(n.startswith('SD700/') for n in names),
                    'Duplicate or unexpected ZIP path')
            entries = {n[len('SD700/'):]: archive.read(n) for n in names}
    return verify_entries(entries)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('path', type=Path, help='Delivered ZIP or extracted SD700 directory')
    print(json.dumps(verify_path(parser.parse_args().path), indent=2))
