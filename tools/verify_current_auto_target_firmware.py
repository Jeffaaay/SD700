"""Verify the current field pair's hashes and exact ELF configuration; no hardware I/O."""
import hashlib
from pathlib import Path
import struct

# Keep the historical ELF reader here; firmware verification has no delivery-tool dependency.


ROOT = Path(__file__).resolve().parent.parent
STEM = "SD700_AutoTarget_ConvergenceMeasure1_RealBench_Release"


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


def verify():
    manifest = {}
    for line in (ROOT / "Firmware/SHA256SUMS.txt").read_text().splitlines():
        digest, name = line.split(" *", 1)
        assert name not in manifest, "Duplicate firmware manifest entry"
        manifest[name] = digest
    expected_paths = {f"../output/AutoTarget/firmware/{STEM}.{ext}" for ext in ("elf", "hex")}
    assert set(manifest) == expected_paths, "Current firmware pair required"
    for name, digest in manifest.items():
        path = ROOT / "Firmware" / name
        actual = hashlib.sha256(path.read_bytes()).hexdigest().upper()
        assert actual == digest, f"Firmware hash mismatch: {name}"
        print(f"{path.suffix[1:].upper()}_SHA256={actual}")

    symbols = elf_symbols(ROOT / "output/AutoTarget/firmware" / f"{STEM}.elf")
    pi = symbols["g_sd700_auto_target_force_pi_config"]
    assert struct.unpack("<7f", pi) == (8.0, 0.0, -800.0, 1200.0, 0.0, 0.0, 1.0)
    raw = symbols["g_sd700_auto_target_machine_config"]
    assert len(raw) == 76, "MachineConfig layout changed"
    assert struct.unpack_from("<17I", raw) == (
        275, 20, 5, 10, 200, 50, 250, 10000, 20, 50, 10000, 10, 40,
        5000, 10, 40, 30000,
    ), "Unexpected machine configuration (including convergence timeout)"
    assert raw[68:70] == b"\x01\x01", "Release correction/raw overpressure must remain enabled"
    assert struct.unpack_from("<I", raw, 72)[0] == 325, "Raw overpressure limit changed"
    assert symbols["approach_diagnostics_ram"]["size_bytes"] == 208
    assert symbols["approach_diagnostics_ram"]["section_type"] == 8, "Diagnostics must remain in RAM/BSS"
    print("CURRENT_AUTO_TARGET_FIRMWARE=PASS; ELF_CONVERGENCE_TIMEOUT_MS=30000")
    print("physical test NOT RUN")


if __name__ == "__main__":
    verify()
