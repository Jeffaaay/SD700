"""Create and verify the single source-of-truth ZIP; no flashing or serial I/O."""
from pathlib import Path
import hashlib
import json
import re
import shutil
import struct
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / 'output/AutoTarget'
FW = OUT / 'firmware'
STEM = 'SD700_AutoTarget_PressBoost1_RealBench_Release'
ARCHIVE = ROOT / 'output/SD700_AutoTarget_PressBoost1_FieldReady1_ReviewBundle.zip'
BASE = OUT / 'field_ready1_baseline'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def read_text(path):
    data = path.read_bytes()
    return data.decode('utf-16' if data[:2] in (b'\xff\xfe', b'\xfe\xff') else 'utf-8-sig')


def write_json(path, data):
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')


def elf_symbols(path):
    data = path.read_bytes()
    assert data[:6] == b'\x7fELF\x01\x01', 'Expected ELF32 little endian'
    assert struct.unpack_from('<H', data, 18)[0] == 40, 'Expected ARM target'
    shoff = struct.unpack_from('<I', data, 32)[0]
    entsize, count = struct.unpack_from('<HH', data, 46)
    sections = [struct.unpack_from('<10I', data, shoff + i*entsize) for i in range(count)]
    found = {}
    for s in sections:
        if s[1] != 2:
            continue
        strings = sections[s[6]]
        text = data[strings[4]:strings[4] + strings[5]]
        for offset in range(s[4], s[4] + s[5], s[9]):
            name, value, size, info, other, section = struct.unpack_from('<IIIBBH', data, offset)
            name = text[name:text.index(b'\0', name)].decode('ascii')
            if name in ('g_sd700_auto_target_machine_config', 'g_sd700_auto_target_force_pi_config'):
                source = sections[section]
                at = source[4] + value - source[3]
                found[name] = data[at:at+size]
            if name == 'g_sd700_approach_diagnostics':
                found['approach_diagnostics_ram'] = {'address': hex(value), 'size_bytes': size,
                                                    'section_type': sections[section][1]}
    return found


def verify_inputs():
    # This package must retain the exact physical-test candidate; no ARM tool
    # invocation or rebuild is needed for host-only preflight/report changes.
    host = read_text(OUT / 'field_ready1_host_suite.log')
    races = read_text(OUT / 'field_ready1_completion_races.log')
    auto = read_text(OUT / 'field_ready1_auto_tests.log')
    capture = read_text(OUT / 'field_ready1_auto_capture.log')
    assert host.count('HOST_TEST_RESULT=') == 33 and 'HOST_TEST_SUITE=PASS' in host
    assert races.count('COMPLETION_RACES=') == 6 and 'COMPLETION_RACE_SUITE=PASS' in races
    assert auto.count('AUTO_TARGET_HOST=') == 2 and 'AUTO_TARGET_TEST_SUITE=PASS' in auto
    assert auto.count('AUTO_TARGET_POLICY=') == 11
    assert auto.count('CONTACT_DEADLINE_TIMELY_EVIDENCE_FAILURES=0') == 2
    assert auto.count('PRESS_SEGMENTS_TWO_NORMAL_LOW_RESPONSES_CAP_RESET_NEAR_HOLD=PASS') == 2
    assert 'AUTO_TARGET_CAPTURE_SELF_TEST=PASS' in capture
    assert 'COARSE_APPROACH_CAPTURE_SELF_TEST=PASS' in capture
    assert 'AUTO_FIELD_REPORT_SELF_TEST=PASS' in capture
    assert capture.count('LENGTH_AWARE_CASE=') == 13
    assert capture.count('AUTO_PREFLIGHT_CASE=') == 20
    for case in ('TENTH_ATTEMPT_ONE_TARGET_WRITE_ONE_START',
                 'BASELINE_EXHAUSTED_ZERO_START', 'TARGET_READBACK_EXHAUSTED_ZERO_START',
                 'LOST_START_ECHO_NO_RETRY_STOP',
                 'SAFETY_FAULT_BOTH_PHASES_ATTEMPTS_1_TO_10',
                 'TRANSPORT_CRC_BOTH_PHASES_ATTEMPTS_1_TO_10',
                 'TRANSPORT_TIMEOUT_BOTH_PHASES_ATTEMPTS_1_TO_10'):
        assert 'AUTO_PREFLIGHT_CASE=' + case + ' PASS' in capture
    assert 'CAPTURE_PRESSURE_RESPONSE_SELF_TEST=PASS' in read_text(OUT / 'field_ready1_pressure_capture.log')
    tests = json.loads(read_text(OUT / 'field_ready1_test_results.json'))
    assert len(tests['tests']) == 6
    for result in tests['tests']:
        assert result['exit_code'] == 0 and result['pass_marker_present']
        assert sha(ROOT / result['script']) == result['script_sha256'], 'Script changed after final test'

    allowed = {'tools/capture_auto_target_static.ps1', 'tools/package_auto_target.py'}
    before = json.loads(read_text(BASE / 'source_before.json'))
    changed, protected = [], []
    for item in before:
        rel = item['Path'].replace('\\', '/')
        current = sha(ROOT / rel)
        if current != item['Sha256'].upper():
            assert rel in allowed, 'Unexpected source change: ' + rel
            changed.append(rel)
        else:
            protected.append({'path': rel, 'sha256': current})
    roots = ('Application', 'Board', 'Drivers', 'Transport', 'Protocol', 'User', 'Projects', 'EIDE')
    recorded = {x['Path'] for x in before if x['Path'].split('/')[0] in roots
                and Path(x['Path']).suffix.lower() in ('.c', '.h', '.s')}
    current = {p.relative_to(ROOT).as_posix() for root in roots for p in (ROOT / root).rglob('*')
               if p.is_file() and p.suffix.lower() in ('.c', '.h', '.s')}
    assert current == recorded, 'Production source file set changed'

    def ps_function(source, name):
        return re.search(r'^function ' + name + r' \{.*?(?=^function |^if \(\$SelfTest|\Z)',
                         source, re.M | re.S).group(0).strip()
    old = read_text(BASE / 'tools/capture_auto_target_static.ps1').replace('\r\n', '\n')
    new = read_text(ROOT / 'tools/capture_auto_target_static.ps1').replace('\r\n', '\n')
    names = ('Read-AutoWords','Join-AutoWords','Read-AutoStatus','Set-AutoTarget',
             'Invoke-AutoCapture','Get-AutoMetrics','Test-AutoTargetFraming')
    for name in names:
        assert ps_function(old,name) == ps_function(new,name), 'Protected capture function changed: ' + name
    helper = ps_function(old,'Read-AutoPreflightStatus')
    helper = helper.replace('Three complete','Ten complete').replace('$attempt -le 3','$attempt -le 10')
    helper = helper.replace('$attempt -lt 3','$attempt -lt 10').replace('after 3 complete','after 10 complete')
    assert helper == ps_function(new,'Read-AutoPreflightStatus'), 'Only attempt bound may change in preflight'
    write_json(OUT / 'field_ready1_source_audit.json', {
        'baseline': 'CURRENT PressBoost1 before FieldReady1', 'recorded_files': len(before),
        'existing_files_changed': changed, 'protected_files_unchanged': protected,
        'production_source_file_set_unchanged': True,
        'protected_capture_function_bodies': names, 'preflight_only_attempt_bound_changed': True,
        'mcu_source_changes': [], 'arm_build': 'NOT_RUN'})

    firmware_before = json.loads(read_text(BASE / 'firmware_before.json'))
    for entry in firmware_before:
        p = ROOT / entry['Path']
        assert sha(p) == entry['Sha256'], 'Firmware bytes changed'
        assert p.stat().st_mtime_ns == entry['MtimeNs'], 'Firmware was rewritten/rebuilt'
    elf, image = FW / f'{STEM}.elf', FW / f'{STEM}.hex'
    assert sha(image) == '26CB9D8AB146A63DCF21F420CCA4AAB256004FF60AD2B08317BCF4FDA7B5AEB2'
    assert sha(elf) == 'DAF479B97E4BD0E286962EBF16D21FFA235C3450D4E46CA5B6CF6D3DB9725256'
    symbols = elf_symbols(elf)
    assert struct.unpack('<7f', symbols['g_sd700_auto_target_force_pi_config']) == (8.0,0.0,-800.0,1200.0,0.0,0.0,1.0)
    raw = symbols['g_sd700_auto_target_machine_config']
    assert struct.unpack_from('<17I', raw) == (275,20,5,10,200,50,250,10000,20,50,10000,10,40,5000,10,40,8000)
    assert raw[68:70] == b'\x01\x01' and struct.unpack_from('<I', raw, 72)[0] == 325
    assert symbols['approach_diagnostics_ram']['size_bytes'] == 176
    write_json(OUT / 'field_ready1_firmware_identity.json', {
        'firmware': STEM, 'hex_sha256': sha(image), 'elf_sha256': sha(elf),
        'bytes_and_timestamps_unchanged': True, 'arm_rebuild': 'NOT_RUN',
        'unchanged_ram_symbol': symbols['approach_diagnostics_ram'],
        'physical_trial': 'NOT_RUN; success requires real field observations',
        'scope': 'static fixed workpiece; Target250 SENSOR CONTROL UNITS; one START; no ApproachOnly'})
    expected = ''.join(f'{sha(p)} *../output/AutoTarget/firmware/{p.name}\n' for p in (elf,image))
    assert read_text(ROOT / 'Firmware/SHA256SUMS.txt').replace('\r\n','\n') == expected


def selected(path):
    rel = path.relative_to(ROOT)
    if 'build' in [x.lower() for x in rel.parts] or '__pycache__' in rel.parts:
        return False
    if path.suffix.lower() in ('.hex','.elf'):
        return path.parent == FW and path.stem == STEM
    if path.suffix.lower() in ('.zip','.exe','.o','.d','.i','.map','.pyc','.axf','.bin'):
        return False
    if path.name.endswith('.zip.sha256') or rel.as_posix() in (
            'output/AutoTarget/package_verification.json', 'output/AutoTarget/verification.log'):
        return False
    return True


def main():
    verify_inputs()
    # Active C/header timestamps must precede the Release ELF. No source mutation
    # is performed here; source hashes bind the complete reviewed archive.
    elf_time = (FW / f'{STEM}.elf').stat().st_mtime
    for dirname in ('Application','Board','Drivers','Transport','Protocol','User'):
        for p in (ROOT / dirname).rglob('*'):
            if p.is_file() and p.suffix.lower() in ('.c','.h','.s'):
                assert p.stat().st_mtime <= elf_time, f'Active source newer than release: {p}'

    files = sorted((p for p in ROOT.rglob('*') if p.is_file() and selected(p)),
                   key=lambda p: p.relative_to(ROOT).as_posix())
    sums = ROOT / 'SHA256SUMS.txt'
    files = [p for p in files if p != sums]
    sums.write_text(''.join(f'{sha(p)} *{p.relative_to(ROOT).as_posix()}\n' for p in files), encoding='utf-8')
    files.append(sums)
    with zipfile.ZipFile(ARCHIVE, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for p in files:
            z.write(p, 'SD700/' + p.relative_to(ROOT).as_posix())
    with zipfile.ZipFile(ARCHIVE) as z:
        assert z.testzip() is None
        names = z.namelist()
        assert len(names) == len(set(names))
        assert len([n for n in names if n.endswith('.hex')]) == 1
        assert len([n for n in names if n.endswith('.elf')]) == 1
        entries = z.read('SD700/SHA256SUMS.txt').decode('utf-8').splitlines()
        for line in entries:
            expected, rel = line.split(' *', 1)
            actual = hashlib.sha256(z.read('SD700/' + rel)).hexdigest().upper()
            assert actual == expected, f'ZIP hash mismatch: {rel}'
        assert len(entries) + 1 == len(names), 'Manifest must cover every file except itself'
    checksum = sha(ARCHIVE)
    report = {'archive': ARCHIVE.name, 'sha256': checksum,
        'files': len(files), 'manifest_files_verified': len(entries),
        'hex_count': 1, 'elf_count': 1, 'zip_crc': 'PASS', 'manifest_hashes': 'PASS',
        'physical_test': 'NOT_RUN'}
    write_json(OUT / 'package_verification.json', report)
    ARCHIVE.with_suffix('.zip.sha256').write_text(f'{checksum} *{ARCHIVE.name}\n', encoding='ascii')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
