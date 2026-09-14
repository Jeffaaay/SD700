"""Current firmware verifier rejection tests; mutate temporary bytes only."""
import hashlib
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools'))
import verify_force_servo_firmware as verifier
from firmware_image import ElfImage, compare_images, read_hex


def record(kind, address=0, payload=b''):
    data = bytes([len(payload)])+address.to_bytes(2, 'big')+bytes([kind])+payload
    return ':'+(data+bytes([-sum(data) & 255])).hex().upper()


class VerifierTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.elf = (verifier.FW/(verifier.STEM+'.elf')).read_bytes()
        cls.hex = (verifier.FW/(verifier.STEM+'.hex')).read_bytes()
        cls.image = ElfImage(cls.elf)

    def reject_elf(self, data, message):
        with self.assertRaisesRegex(ValueError, message):
            verifier.verify_contents(data, self.hex)

    def test_exact_pair_and_ram_initializers(self):
        image = verifier.verify_contents(self.elf, self.hex)
        self.assertEqual(len(image.load_bytes), 41152)
        data_segment = image.loads[1]
        self.assertEqual(data_segment[2], 0x20000000)
        self.assertIn(data_segment[3], image.load_bytes)
        self.assertNotIn(0x20000000, image.load_bytes)
        self.assertEqual(hashlib.sha256(self.hex).hexdigest().upper(), verifier.PINNED_HASHES['hex'])
        self.assertEqual(hashlib.sha256(self.elf).hexdigest().upper(), verifier.PINNED_HASHES['elf'])

    def test_offline_cli_with_empty_path(self):
        result = subprocess.run([sys.executable, str(ROOT/'tools/verify_force_servo_firmware.py')],
                                env={**os.environ, 'PATH': ''}, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('PURE_PYTHON', result.stdout)

    def test_optimized_python_still_checks(self):
        code = "import sys;sys.path.insert(0,'tools');from firmware_image import ElfImage;ElfImage(b'bad')"
        result = subprocess.run([sys.executable, '-O', '-c', code], cwd=ROOT, capture_output=True)
        self.assertNotEqual(result.returncode, 0)

    def test_hex_checksum_truncation_and_length(self):
        lines = self.hex.decode().splitlines()
        bad = lines.copy(); bad[1] = bad[1][:-2]+'00'
        with self.assertRaisesRegex(ValueError, 'checksum'): read_hex(('\n'.join(bad)+'\n').encode())
        for bad in (b':0100', b':GG', b':', b'\xff', b'', self.hex[:-12]):
            with self.assertRaises(ValueError): read_hex(bad)

    def test_hex_duplicate_and_trailing_data(self):
        lines = self.hex.decode().splitlines()
        with self.assertRaisesRegex(ValueError, 'overlapping'):
            read_hex(('\n'.join(lines[:2]+[lines[1]]+lines[2:])+'\n').encode())
        with self.assertRaisesRegex(ValueError, 'after EOF'): read_hex(self.hex+b':00000001FF\n')

    def test_hex_bad_address_entry_missing_and_extra_bytes(self):
        for kind in ('address', 'entry', 'missing', 'extra'):
            with self.subTest(kind=kind):
                lines = self.hex.decode().splitlines()
                if kind == 'address': lines[0] = record(4, payload=b'\x20\x00')
                elif kind == 'entry': lines[-2] = record(5, payload=(self.image.entry ^ 2).to_bytes(4, 'big'))
                elif kind == 'missing': del lines[1]
                else: lines.insert(1, record(0, 0xF000, b'\x01'))
                with self.assertRaises(ValueError): compare_images(self.image, ('\n'.join(lines)+'\n').encode())

    def test_hex_valid_checksum_changed_byte_rejected(self):
        lines = self.hex.decode().splitlines()
        data = bytearray.fromhex(lines[1][1:]); data[4] ^= 1
        lines[1] = record(data[3], int.from_bytes(data[1:3], 'big'), bytes(data[4:-1]))
        with self.assertRaisesRegex(ValueError, 'addressized'):
            compare_images(self.image, ('\n'.join(lines)+'\n').encode())

    def test_hex_format_independence(self):
        # Same addressed bytes, different record ordering/chunk size. The release hash
        # remains pinned separately; load-image equality must not compare text alone.
        lines = [record(4, payload=b'\x08\x00')]
        addresses = sorted(self.image.load_bytes)
        for address in reversed(addresses):
            lines.append(record(0, address & 65535, bytes([self.image.load_bytes[address]])))
        lines += [record(5, payload=self.image.entry.to_bytes(4, 'big')), record(1)]
        compare_images(self.image, ('\n'.join(lines)+'\n').encode())

    def test_elf_type_class_endianness_machine_abi(self):
        for offset, value in ((4, 2), (5, 2), (6, 0), (16, 1), (18, 3), (37, 0)):
            with self.subTest(offset=offset):
                bad = bytearray(self.elf); bad[offset] = value
                self.reject_elf(bad, 'ELF')

    def test_elf_truncated_tables_and_bad_sizes(self):
        for size in (0, 10, 51, 100, len(self.elf)-1): self.reject_elf(self.elf[:size], 'ELF')
        for offset, value, fmt in ((28, 0xFFFFFFF0, '<I'), (32, 0xFFFFFFF0, '<I'),
                                   (40, 51, '<H'), (42, 0, '<H'), (46, 0, '<H'), (48, 0, '<H')):
            bad = bytearray(self.elf); struct.pack_into(fmt, bad, offset, value)
            self.reject_elf(bad, 'ELF')

    def test_elf_load_bounds_overlap_and_bss(self):
        for offset, value in ((52+4, 0xFFFFFF00), (52+16, 0xFFFFFFFF),
                              (52+20, 1), (52+12, 0x20000000),
                              (52+32+12, 0x08000000), (52+64+20, 0xFFFFFFFF)):
            bad = bytearray(self.elf); struct.pack_into('<I', bad, offset, value)
            self.reject_elf(bad, 'ELF')

    def test_elf_section_and_symbol_boundaries(self):
        shoff = struct.unpack_from('<I', self.elf, 32)[0]
        for offset, value in ((shoff+40+16, 0xFFFFFFF0), (shoff+40+20, 0xFFFFFFFF),
                              (shoff+40+24, 9999)):
            bad = bytearray(self.elf); struct.pack_into('<I', bad, offset, value)
            self.reject_elf(bad, 'ELF')
        symtab = next(s for s in self.image.sections if s[1] == 2)
        bad = bytearray(self.elf); struct.pack_into('<I', bad, symtab[4]+16, 0xFFFFFFF0)
        self.reject_elf(bad, 'string offset')

    def test_wrong_identity_and_protected_config(self):
        # Content checks are exercised without pinning hashes first, so these
        # failures prove contract checks themselves, not just changed file hashes.
        for name, offset, value in (('g_force_servo_contract', 0, 0xF101),
                                    ('g_force_servo_contract', 4, 0x46530104),
                                    ('g_force_servo_contract', 8, 0),
                                    ('g_force_servo_contract', 20, 45000),
                                    ('g_force_servo_contract', 24, 100),
                                    ('g_force_servo_contract', 24, 721),
                                    ('g_force_servo_contract', 28, 101),
                                    ('g_force_servo_contract', 32, 500),
                                    ('g_force_servo_contract', 36, 500),
                                    ('g_force_servo_contract', 40, 126),
                                    ('g_force_servo_contract', 44, 131),
                                    ('g_force_servo_contract', 48, 100),
                                    ('g_force_servo_contract', 48, 721),
                                    ('g_force_servo_contract', 52, 101),
                                    ('g_force_servo_contract', 56, 0),
                                    ('g_force_servo_contract', 60, 0),
                                    ('g_force_servo_contract', 64, 9600),
                                    ('g_force_servo_contract', 68, 2400),
                                    ('g_force_servo_contract', 72, 12),
                                    ('g_force_servo_contract', 76, 0),
                                    ('g_force_servo_default_config', 0, 0x3F800000),
                                    ('g_force_servo_default_config', 4, 0x3F800000),
                                    ('g_force_servo_default_config', 28, 0x42C80000),
                                    ('g_force_servo_default_config', 28, 0x44344000),
                                    ('g_force_servo_default_config', 32, 0x42CA0000),
                                    ('g_force_servo_default_config', 56, 0x42240000),
                                    ('g_force_servo_default_config', 60, 0x41A80000),
                                    ('g_force_servo_default_config', 64, 0x424C0000),
                                    ('g_sd700_auto_target_machine_config', 0, 500),
                                    ('g_sd700_auto_target_machine_config', 72, 500)):
            bad = bytearray(self.elf)
            struct.pack_into('<I', bad, self.image.symbol_offsets[name]+offset, value)
            self.reject_elf(bad, 'configuration|configuration assertion|parameter group')

    def test_profile_mutations_rejected(self):
        # Every field is pinned independently of hash: unlocking/calibration,
        # target/trip, raw bounds, caps, timing, peak and cumulative budget.
        for index in range(33):
            bad=bytearray(self.elf)
            offset=self.image.symbol_offsets['g_force_servo_profile']+index*4
            old=struct.unpack_from('<f',bad,offset)[0]
            struct.pack_into('<f',bad,offset,old+1)
            self.reject_elf(bad,'operating profile')

    def test_all_candidate_fields_pinned_including_enable_and_cooling(self):
        for index in range(3*33):
            bad=bytearray(self.elf)
            offset=self.image.symbol_offsets['g_force_servo_candidates']+index*4
            old=struct.unpack_from('<f',bad,offset)[0]
            struct.pack_into('<f',bad,offset,old+1)
            self.reject_elf(bad,'candidate catalog')

    def test_boost1_is_not_current_candidate(self):
        old=ROOT/'output/StaticForce3000_Boost1/firmware/SD700_ForceServo1_StaticForce3000_Boost1_RealBench_Release.elf'
        self.reject_elf(old.read_bytes(),'identity/configuration')

    def test_authority2_before_reviewfix_is_not_current_candidate(self):
        old=ROOT/'output/StaticForceAuthority2/firmware/SD700_ForceServo1_StaticForceAuthority2_RealBench_Release.elf'
        self.reject_elf(old.read_bytes(),'identity/configuration')

    def test_static3000_1_is_not_current_candidate(self):
        old=ROOT/'output/StaticForce3000_1/firmware/SD700_ForceServo1_StaticForce3000_1_RealBench_Release.elf'
        self.reject_elf(old.read_bytes(),'identity/configuration')

    def test_authority1_is_not_current_candidate(self):
        old=ROOT/'output/Target250Authority1/firmware/SD700_ForceServo1_Target250Authority1_RealBench_Release.elf'
        self.reject_elf(old.read_bytes(),'identity/configuration')

    def test_wrong_commissioning_gate_rejected(self):
        bad = bytearray(self.elf)
        bad[self.image.symbol_offsets['MotorHwReal_OutputArmingAllowed']] = 0  # movs r0,#0
        self.reject_elf(bad, 'not commissioning armed')

    def test_locked_predecessor_is_not_current_candidate(self):
        old = ROOT/'output/ForceServo1/firmware/SD700_ForceServo1_RealBench_Locked_Release.elf'
        self.reject_elf(old.read_bytes(), 'identity/configuration')

    def test_previous_commissioning_is_not_current_candidate(self):
        old = ROOT/'output/CommissioningUnlock1/firmware/SD700_ForceServo1_CommissioningUnlock1_RealBench_Release.elf'
        self.reject_elf(old.read_bytes(), 'identity/configuration')

    def test_mvp1_is_not_current_candidate(self):
        old = ROOT/'output/Target250MVP1/firmware/SD700_ForceServo1_Target250MVP1_RealBench_Release.elf'
        self.reject_elf(old.read_bytes(), 'identity/configuration')

    def test_continuous2_is_not_current_candidate(self):
        old = ROOT/'output/Target250Continuous2/firmware/SD700_ForceServo1_Target250Continuous2_RealBench_Release.elf'
        self.reject_elf(old.read_bytes(), 'identity/configuration')

    def test_true_hash_and_manifest_pins(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d); fw = root/verifier.FW_RELATIVE; fw.mkdir(parents=True)
            (root/'Firmware').mkdir()
            for ext, data in (('hex', self.hex), ('elf', self.elf)):
                (fw/(verifier.STEM+'.'+ext)).write_bytes(data)
            manifest = root/'Firmware/ForceServo1.SHA256SUMS.txt'
            original = (ROOT/'Firmware/ForceServo1.SHA256SUMS.txt').read_bytes()
            manifest.write_bytes(original)
            with patch.object(verifier, 'ROOT', root), patch.object(verifier, 'FW', fw):
                (fw/(verifier.STEM+'.hex')).write_bytes(self.hex+b'\n')
                with self.assertRaisesRegex(ValueError, 'Hash mismatch'): verifier.verify()
                manifest.write_bytes(original.replace(verifier.PINNED_HASHES['hex'].encode(), b'0'*64))
                with self.assertRaisesRegex(ValueError, 'pinned'): verifier.verify()


if __name__ == '__main__': unittest.main(verbosity=2)
