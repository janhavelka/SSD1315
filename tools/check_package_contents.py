#!/usr/bin/env python3
"""Validate generated PlatformIO package contents for SSD1315 releases."""

from __future__ import annotations

import pathlib
import sys
import tarfile
import argparse
import json

ROOT = pathlib.Path(__file__).resolve().parents[1]

REQUIRED_SUFFIXES = {
    "library.json",
    "include/SSD1315.h",
    "include/ssd1315/CommandTable.h",
    "include/ssd1315/Config.h",
    "include/ssd1315/SSD1315.h",
    "include/ssd1315/Status.h",
    "include/ssd1315/Version.h",
    "src/SSD1315.cpp",
    "docs/README.md",
    "docs/SSD1315_datasheet.pdf",
    "docs/Wisevision_X096-2864KSWPG01-H30_module_spec.pdf",
    "docs/extracted-md/00_document_inventory.md",
    "docs/extracted-md/01_chip_overview.md",
    "docs/extracted-md/02_pinout_and_signals.md",
    "docs/extracted-md/03_electrical_and_timing.md",
    "docs/extracted-md/04_protocol_commands_and_transactions.md",
    "docs/extracted-md/05_register_map.md",
    "docs/extracted-md/06_modes_interrupts_status_and_faults.md",
    "docs/extracted-md/07_initialization_reset_and_operational_notes.md",
    "docs/extracted-md/08_variant_differences_and_open_questions.md",
    "docs/pdf-extracted-md/SSD1315_datasheet.md",
    "docs/pdf-extracted-md/Wisevision_X096-2864KSWPG01-H30_module_spec.md",
    "CMakeLists.txt",
    "idf_component.yml",
    "examples/espidf_basic/CMakeLists.txt",
    "examples/espidf_basic/components/SSD1315/CMakeLists.txt",
    "examples/espidf_basic/main/CMakeLists.txt",
    "examples/espidf_basic/main/main.cpp",
}

FORBIDDEN_PARTS = {
    ".git",
    ".pio",
    "__pycache__",
    "docs/doxygen",
    "hil_logs",
}


def fail(message: str) -> None:
    print(f"Package contents FAILED: {message}")
    raise SystemExit(1)


def load_expected_archive() -> pathlib.Path:
    library_json = ROOT / "library.json"
    with library_json.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    version = str(data.get("version", "")).strip()
    if not version:
        fail("library.json does not contain a version")
    archive = ROOT / f"SSD1315-{version}.tar.gz"
    if not archive.exists():
        fail(f"expected archive not found: {archive.name}; run 'python -m platformio pkg pack'")
    return archive


def normalize(name: str) -> str:
    return name.replace("\\", "/").lstrip("./")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", help="Explicit package archive to validate")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    archive = pathlib.Path(args.archive).resolve() if args.archive else load_expected_archive()
    if not archive.exists():
        fail(f"archive not found: {archive}")
    with tarfile.open(archive, "r:gz") as tar:
        members = {normalize(member.name) for member in tar.getmembers()}

    missing = []
    for suffix in sorted(REQUIRED_SUFFIXES):
        if not any(name.endswith(suffix) for name in members):
            missing.append(suffix)
    if missing:
        fail("missing required files: " + ", ".join(missing))

    forbidden_hits = []
    for name in members:
        parts = set(name.split("/"))
        if parts & FORBIDDEN_PARTS or any(part in name for part in FORBIDDEN_PARTS):
            forbidden_hits.append(name)
    if forbidden_hits:
        fail("forbidden build/internal paths in archive: " + ", ".join(sorted(forbidden_hits)[:8]))

    print(f"Package contents PASSED ({archive.name})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
