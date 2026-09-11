#!/usr/bin/env python3
"""Verify the exact CLEAN(12) deterministic delivery contract."""

from __future__ import annotations

import argparse
from collections import Counter
import hashlib
from pathlib import Path, PurePosixPath
import re
import zipfile

from create_deterministic_zip import (
    RELEASE_ARTIFACT_SUFFIXES,
    REQUIRED_OUTPUT_FILENAME,
    is_excluded,
)


REQUIRED_FILES = {
    "README.md",
    "Docs/MODBUS_REGISTER_MAP.md",
    "Docs/Architecture/00_SYSTEM_ARCHITECTURE.md",
    "Docs/Architecture/04_HOST_TEST_PLAN.md",
    "Application/direct_pulse_config.h",
    "Application/machine.c",
    "Application/machine.h",
    "Application/motion_build_policy.h",
    "Application/runtime.c",
    "Application/runtime.h",
    "Board/Motor/motor_diagnostics.h",
    "Board/Motor/motor_executor.h",
    "Board/Motor/motor_executor_locked.c",
    "Board/Motor/motor_executor_real.c",
    "Board/Motor/motor_hw_real.c",
    "Board/Motor/motor_hw_real.h",
    "Board/Motor/motor_real_config.h",
    "Board/Motor/motor_stop_timer.h",
    "Board/Motor/motor_stop_timer_tim5.c",
    "Protocol/Modbus/modbus_register_map.h",
    "Transport/Modbus/modbus_rtu_server.c",
    "Transport/Modbus/modbus_semantic_map.c",
    "User/stm32f4xx_it.c",
    "User/stm32f4xx_it.h",
    "Tests/Host/check_direct_command_ownership.ps1",
    "Tests/Host/check_physical_output_lock.ps1",
    "Tests/Host/fake_motor_executor.c",
    "Tests/Host/fake_motor_executor.h",
    "Tests/Host/fake_motor_hw.c",
    "Tests/Host/fake_motor_hw.h",
    "Tests/Host/fake_motor_hw_real.c",
    "Tests/Host/fake_motor_hw_real.h",
    "Tests/Host/fake_motor_stop_timer.c",
    "Tests/Host/fake_motor_stop_timer.h",
    "Tests/Host/fake_stm32_hal.c",
    "Tests/Host/fake_stm32_hal.h",
    "Tests/Host/Shim/stm32f4xx.h",
    "Tests/Host/Shim/stm32f4xx_hal.h",
    "Tests/Host/test_benchmark0.c",
    "Tests/Host/test_direct_pulse_commands.c",
    "Tests/Host/test_direct_pulse_full_chain.c",
    "Tests/Host/test_machine_real_integration.c",
    "Tests/Host/test_modbus_crc16.c",
    "Tests/Host/test_modbus_direct_commands.c",
    "Tests/Host/test_modbus_rtu.c",
    "Tests/Host/test_modbus_uart_ingress_concurrency.c",
    "Tests/Host/test_motion_build_policy_compile.c",
    "Tests/Host/test_motor_executor.c",
    "Tests/Host/test_motor_executor_real.c",
    "Tests/Host/test_motor_executor_real_compile_check.c",
    "Tests/Host/test_motor_hw_real_diagnostics.c",
    "Tests/Host/test_motor_real_config_compile.c",
    "Tests/Host/test_motor_stop_timer_tim5.c",
    "Tests/Host/test_pressure_receiver.c",
    "Tests/Host/test_pressure_sensor_protocol.c",
    "Tests/Host/test_runtime_integration.c",
    "tools/build_gcc.ps1",
    "tools/create_deterministic_zip.py",
    "tools/run_host_tests.ps1",
    "tools/verify_delivery_zip.py",
}


def validate_manifest(
    archive: zipfile.ZipFile,
    root: str,
    file_names: set[str],
    errors: list[str],
) -> bool:
    initial_error_count = len(errors)
    manifest_relative = "ReleaseArtifacts/SHA256SUMS.txt"
    manifest_name = f"{root}/{manifest_relative}"
    if manifest_name not in file_names:
        errors.append(f"missing {manifest_relative}")
        return False

    release_prefix = f"{root}/ReleaseArtifacts/"
    release_files = {
        name for name in file_names if name.startswith(release_prefix)
    }
    artifact_files = {
        name
        for name in release_files
        if PurePosixPath(name).suffix.lower()
        in RELEASE_ARTIFACT_SUFFIXES
    }
    if len(artifact_files) != 16:
        errors.append(
            f"ReleaseArtifacts ELF/HEX count is {len(artifact_files)}, not 16"
        )
    expected_release_files = artifact_files | {manifest_name}
    if release_files != expected_release_files:
        extras = sorted(release_files - expected_release_files)
        errors.append(f"unexpected ReleaseArtifacts files: {extras}")

    try:
        lines = archive.read(manifest_name).decode("ascii").splitlines()
    except (KeyError, UnicodeDecodeError) as exc:
        errors.append(f"invalid SHA256SUMS.txt: {exc}")
        return False

    manifest_paths: set[str] = set()
    for line_number, line in enumerate(lines, start=1):
        match = re.fullmatch(r"([0-9a-f]{64})  (.+)", line)
        if match is None:
            errors.append(f"invalid manifest line {line_number}: {line!r}")
            continue
        expected_hash, artifact_relative = match.groups()
        artifact_path = PurePosixPath(artifact_relative)
        if (
            artifact_path.is_absolute()
            or ".." in artifact_path.parts
            or "\\" in artifact_relative
        ):
            errors.append(
                f"unsafe manifest path on line {line_number}: "
                f"{artifact_relative}"
            )
            continue
        if artifact_relative in manifest_paths:
            errors.append(f"duplicate manifest path: {artifact_relative}")
            continue
        manifest_paths.add(artifact_relative)
        archive_name = f"{release_prefix}{artifact_relative}"
        if archive_name not in artifact_files:
            errors.append(f"manifest entry is missing: {artifact_relative}")
            continue
        actual_hash = hashlib.sha256(archive.read(archive_name)).hexdigest()
        if actual_hash != expected_hash:
            errors.append(f"manifest hash mismatch: {artifact_relative}")

    actual_relative_artifacts = {
        name.removeprefix(release_prefix) for name in artifact_files
    }
    if manifest_paths != actual_relative_artifacts:
        missing = sorted(actual_relative_artifacts - manifest_paths)
        extra = sorted(manifest_paths - actual_relative_artifacts)
        errors.append(
            f"manifest coverage mismatch: missing={missing}, extra={extra}"
        )
    return len(errors) == initial_error_count


def verify_delivery(zip_path: Path) -> tuple[list[str], int, int, bool]:
    errors: list[str] = []
    forbidden_path_count = 0
    elf_hex_outside_release = 0
    manifest_valid = False

    if zip_path.name != REQUIRED_OUTPUT_FILENAME:
        errors.append(
            f"filename must be exactly {REQUIRED_OUTPUT_FILENAME}"
        )
    expected_root = Path(REQUIRED_OUTPUT_FILENAME).stem

    try:
        archive = zipfile.ZipFile(zip_path, "r")
    except (FileNotFoundError, zipfile.BadZipFile) as exc:
        return [f"cannot open ZIP: {exc}"], 0, 0, False

    with archive:
        infos = archive.infolist()
        names = [info.filename for info in infos]
        duplicate_names = sorted(
            name for name, count in Counter(names).items() if count > 1
        )
        if duplicate_names:
            errors.append(f"duplicate archive paths: {duplicate_names}")

        roots: set[str] = set()
        file_names: set[str] = set()
        relative_file_names: set[str] = set()
        for info in infos:
            name = info.filename
            path = PurePosixPath(name)
            if (
                path.is_absolute()
                or ".." in path.parts
                or "\\" in name
                or not path.parts
            ):
                errors.append(f"unsafe archive path: {name}")
                continue
            roots.add(path.parts[0])
            if path.parts[0] != expected_root:
                continue
            if info.is_dir():
                continue
            relative = PurePosixPath(*path.parts[1:])
            relative_text = relative.as_posix()
            file_names.add(name)
            relative_file_names.add(relative_text)
            if is_excluded(Path(*relative.parts)):
                forbidden_path_count += 1
            if (
                relative.suffix.lower() in RELEASE_ARTIFACT_SUFFIXES
                and not relative_text.startswith("ReleaseArtifacts/")
            ):
                elf_hex_outside_release += 1

        if roots != {expected_root}:
            errors.append(
                f"top-level roots are {sorted(roots)}, expected {expected_root}"
            )
        missing_required = sorted(REQUIRED_FILES - relative_file_names)
        if missing_required:
            errors.append(f"missing required files: {missing_required}")
        if forbidden_path_count != 0:
            errors.append(
                f"forbidden path count is {forbidden_path_count}"
            )
        if elf_hex_outside_release != 0:
            errors.append(
                "ELF/HEX files outside ReleaseArtifacts: "
                f"{elf_hex_outside_release}"
            )
        bad_crc_entry = archive.testzip()
        if bad_crc_entry is not None:
            errors.append(f"ZIP CRC failure: {bad_crc_entry}")
        manifest_valid = validate_manifest(
            archive, expected_root, file_names, errors
        )

    return errors, forbidden_path_count, elf_hex_outside_release, manifest_valid


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("zip_file", type=Path)
    args = parser.parse_args()

    errors, forbidden_count, outside_count, manifest_valid = (
        verify_delivery(args.zip_file.resolve())
    )
    print(f"FORBIDDEN_PATH_COUNT={forbidden_count}")
    print(f"ELF_HEX_OUTSIDE_RELEASEARTIFACTS={outside_count}")
    print(
        "RELEASE_MANIFEST=" + ("PASS" if manifest_valid else "FAIL")
    )
    if errors:
        print("DELIVERY_ZIP_VALIDATION=FAIL")
        for error in errors:
            print(f"DELIVERY_ZIP_ERROR={error}")
        return 1
    print("DELIVERY_ZIP_VALIDATION=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
