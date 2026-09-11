#!/usr/bin/env python3
"""Create the CLEAN delivery ZIP with stable ordering and metadata."""

from __future__ import annotations

import argparse
from pathlib import Path
import zipfile


FIXED_ZIP_TIME = (2026, 1, 1, 0, 0, 0)
REQUIRED_OUTPUT_FILENAME = "SD700-Servo-Press-Controller-CLEAN(12).zip"
EXCLUDED_ROOT_DIRECTORIES = {"$tmp", "build", "Output", "tmp"}
EXCLUDED_DIRECTORY_NAMES = {"__pycache__", "obj", "Objects"}
ALWAYS_EXCLUDED_FILE_SUFFIXES = {
    ".axf",
    ".crf",
    ".d",
    ".dep",
    ".exe",
    ".lnp",
    ".lst",
    ".map",
    ".o",
    ".obj",
    ".tmp",
    ".zip",
}
RELEASE_ARTIFACT_SUFFIXES = {".elf", ".hex"}


def is_excluded(relative_path: Path) -> bool:
    parts = relative_path.parts
    if not parts:
        return True
    if parts[0] in EXCLUDED_ROOT_DIRECTORIES:
        return True
    if any(part in EXCLUDED_DIRECTORY_NAMES for part in parts[:-1]):
        return True
    if len(parts) >= 2 and parts[0] == "EIDE" and parts[1] == "build":
        return True
    suffix = relative_path.suffix.lower()
    if suffix in ALWAYS_EXCLUDED_FILE_SUFFIXES:
        return True
    if (suffix in RELEASE_ARTIFACT_SUFFIXES) and (
        parts[0] != "ReleaseArtifacts"
    ):
        return True
    return False


def collect_files(root: Path) -> list[Path]:
    files: list[Path] = []
    for candidate in root.rglob("*"):
        relative_path = candidate.relative_to(root)
        if candidate.is_symlink():
            raise RuntimeError(f"Refusing symlink in delivery: {relative_path}")
        if candidate.is_file() and not is_excluded(relative_path):
            files.append(relative_path)
    return sorted(files, key=lambda path: path.as_posix())


def archive_root_for_output(output: Path) -> str:
    if output.name != REQUIRED_OUTPUT_FILENAME:
        raise RuntimeError(
            f"Output filename must be exactly {REQUIRED_OUTPUT_FILENAME}"
        )
    archive_root = output.stem
    if f"{archive_root}.zip" != REQUIRED_OUTPUT_FILENAME:
        raise RuntimeError("Archive root does not match output filename")
    return archive_root


def create_zip(root: Path, output: Path) -> int:
    files = collect_files(root)
    archive_root = archive_root_for_output(output)
    if output.is_relative_to(root):
        raise RuntimeError("Output ZIP must be outside the workspace")
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists():
        output.unlink()

    with zipfile.ZipFile(
        output,
        mode="w",
        compression=zipfile.ZIP_DEFLATED,
        compresslevel=9,
        strict_timestamps=True,
    ) as archive:
        for relative_path in files:
            archive_name = (
                Path(archive_root) / relative_path
            ).as_posix()
            info = zipfile.ZipInfo(archive_name, FIXED_ZIP_TIME)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.create_system = 3
            info.external_attr = 0o100644 << 16
            with (root / relative_path).open("rb") as source:
                archive.writestr(
                    info,
                    source.read(),
                    compress_type=zipfile.ZIP_DEFLATED,
                    compresslevel=9,
                )
    return len(files)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("workspace", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    root = args.workspace.resolve(strict=True)
    output = args.output.resolve()
    file_count = create_zip(root, output)
    print(f"DETERMINISTIC_ZIP_FILE_COUNT={file_count}")
    print(f"DETERMINISTIC_ZIP={output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
