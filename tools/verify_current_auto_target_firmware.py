"""Verify the current field pair's hashes and exact ELF configuration; no hardware I/O."""
import hashlib
from pathlib import Path
import struct

# Reuse only the ELF reader. The historical FieldReady1 packager and its
# original hash/configuration assertions remain unchanged and are not run here.
from package_auto_target import elf_symbols


ROOT = Path(__file__).resolve().parent.parent
STEM = "SD700_AutoTarget_ConvergenceMeasure1_RealBench_Release"


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
