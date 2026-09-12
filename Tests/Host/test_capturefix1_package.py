"""Offline package rejection tests with synthetic metadata and original locked bytes."""
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools'))
from verify_force_servo_capturefix1_package import MANIFEST, digest, manifest_entries, verify_entries
from verify_force_servo_firmware import STEM


def seal(entries):
    entries[MANIFEST] = ''.join(digest(data)+' *'+name+'\n' for name, data in sorted(entries.items())
                                if name != MANIFEST).encode()
    return entries


class PackageTests(unittest.TestCase):
    def fixture(self):
        base = '29d0a9091b8333c690bcb41f5474dffc3909ef61'
        entries = {name:(ROOT/name).read_bytes() for name in
                   ['Firmware/ForceServo1.SHA256SUMS.txt']+
                   [f'output/ForceServo1/firmware/{STEM}.{ext}' for ext in ('hex', 'elf')]}
        meta = dict(candidate='ForceServo1 CaptureFix1', baseline=base, source_commit=base, remote_main=base,
                    physical_test='NOT_RUN', physical_output='LOCKED', hardware_stop_validation='NOT_VALIDATED',
                    static_250='NOT_VALIDATED', rotating_load='NOT_VALIDATED')
        # Deliberately synthetic check records; these unit fixtures are never delivered.
        checks = []
        for name in ('capture_length_and_observe', 'force_capture_selftest', 'pressure_capture_selftest',
                     'auto_capture_selftest', 'force_host', 'data_schema', 'verifier_rejections',
                     'offline_firmware', 'historical_auto_firmware'):
            log = f'SYNTHETIC/{name}.log'; entries[log] = b'SYNTHETIC_TEST_ONLY\n'
            checks.append(dict(name=name, exit_code=0, result='PASS', log=log, sha256=digest(entries[log])))
        entries['SOURCE_OF_TRUTH.json'] = json.dumps(meta).encode()
        entries['Docs/ForceServo1_CaptureFix1/verification.json'] = json.dumps(dict(
            baseline_commit=base, sources_at_verification={}, checks=checks)).encode()
        entries['SHA256SUMS.txt'] = ''.join(digest(data)+' *'+name+'\n' for name, data in entries.items()).encode()
        return seal(entries)

    def test_valid_addressed_firmware_in_package(self):
        result = verify_entries(self.fixture())
        self.assertEqual(result['package'], 'PASS')
        self.assertEqual(result['load_bytes_compared'], 30512)

    def test_modified_missing_and_extra_entry(self):
        for kind in ('modified', 'missing', 'extra'):
            with self.subTest(kind=kind):
                entries = self.fixture(); name = 'SYNTHETIC/force_host.log'
                if kind == 'modified': entries[name] += b'changed'
                elif kind == 'missing': del entries[name]
                else: entries['unexpected.py'] = b'extra'
                with self.assertRaises(ValueError): verify_entries(entries)

    def test_resealed_package_cannot_skip_source_check(self):
        entries = self.fixture(); entries['SYNTHETIC/force_host.log'] += b'changed'
        with self.assertRaisesRegex(ValueError, 'Source hash'): verify_entries(seal(entries))

    def test_unsafe_or_duplicate_manifest_paths(self):
        for name in ('../escape', '/absolute', 'C:/outside', 'a\\b', 'a/./b', 'a//b'):
            with self.assertRaises(ValueError): manifest_entries(('0'*64+' *'+name+'\n').encode())
        with self.assertRaises(ValueError): manifest_entries((('0'*64+' *a\n')*2).encode())

    def test_missing_check_rejected_after_resealing(self):
        entries = self.fixture()
        name = 'Docs/ForceServo1_CaptureFix1/verification.json'
        proof = json.loads(entries[name]); proof['checks'] = proof['checks'][1:]
        entries[name] = json.dumps(proof).encode()
        entries['SHA256SUMS.txt'] = (digest(entries[name])+' *'+name+'\n').encode()
        with self.assertRaisesRegex(ValueError, 'Required software check missing'):
            verify_entries(seal(entries))


if __name__ == '__main__':
    unittest.main(verbosity=2)
