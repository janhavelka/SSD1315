#!/usr/bin/env python3
"""Validate generated PlatformIO package contents for SSD1315 releases."""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys
import tarfile

ROOT = pathlib.Path(__file__).resolve().parents[1]

REQUIRED_SUFFIXES = {
    "library.json",
    "CHANGELOG.md",
    "Doxyfile",
    "include/SSD1315.h",
    "include/ssd1315/CommandTable.h",
    "include/ssd1315/Config.h",
    "include/ssd1315/SSD1315.h",
    "include/ssd1315/Status.h",
    "include/ssd1315/Version.h",
    "src/SSD1315.cpp",
    "docs/DOCUMENTATION.md",
    "docs/reports/hil-validation-COM21-20260731.md",
    "docs/reports/hil-validation-COM21-20260722.md",
    "docs/reports/hil-validation-COM29-20260623.md",
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

FORBIDDEN_SUFFIXES = {
    "docs/TUNNELMONITOR_INTEGRATION_GATES.md",
}


def fail(message: str) -> None:
    print(f"Package contents FAILED: {message}")
    raise SystemExit(1)


def load_expected_archive() -> pathlib.Path:
    version = load_source_version()
    archive = ROOT / f"SSD1315-{version}.tar.gz"
    if not archive.exists():
        fail(f"expected archive not found: {archive.name}; run 'python -m platformio pkg pack'")
    return archive


def load_source_version() -> str:
    library_json = ROOT / "library.json"
    with library_json.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    version = str(data.get("version", "")).strip()
    if not version:
        fail("library.json does not contain a version")
    return version


def normalize(name: str) -> str:
    return name.replace("\\", "/").lstrip("./")


def require_pattern(text: str, pattern: str, label: str) -> None:
    if re.search(pattern, text, re.MULTILINE) is None:
        fail(f"{label} does not match expected version metadata")


def validate_version_texts(version: str, files: dict[str, str], label_prefix: str) -> None:
    try:
        major, minor, patch = (int(part) for part in version.split("."))
        expected_code_int = major * 10000 + minor * 100 + patch
    except (IndexError, ValueError):
        fail(f"library.json version is not MAJOR.MINOR.PATCH: {version}")

    library_data = json.loads(files["library.json"])
    if str(library_data.get("version", "")).strip() != version:
        fail(f"{label_prefix}library.json version does not match {version}")

    require_pattern(files["idf_component.yml"],
                    rf'^version:\s*["\']?{re.escape(version)}["\']?\s*$',
                    f"{label_prefix}idf_component.yml")
    require_pattern(files["Doxyfile"], rf'^PROJECT_NUMBER\s*=\s*"{re.escape(version)}"\s*$',
                    f"{label_prefix}Doxyfile")
    require_pattern(files["include/ssd1315/Version.h"],
                    rf'#define\s+SSD1315_VERSION_STRING\s+"{re.escape(version)}"',
                    f"{label_prefix}Version.h version string")
    require_pattern(files["include/ssd1315/Version.h"],
                    rf'static\s+constexpr\s+uint32_t\s+VERSION_CODE\s*=\s*{expected_code_int}\s*;',
                    f"{label_prefix}Version.h version code")


def validate_release_evidence(files: dict[str, str], label_prefix: str) -> None:
    for suffix in (
        "docs/reports/hil-validation-COM21-20260731.md",
        "docs/reports/hil-validation-COM21-20260722.md",
    ):
        report = files[suffix]
        if "SOAK_PENDING_REPLACE" in report or "IN_PROGRESS_DO_NOT_RELEASE" in report:
            fail(f"{label_prefix}{suffix} still contains a release blocker")
    current = files["docs/reports/hil-validation-COM21-20260731.md"]
    for token in ("55.03.311", "97,000 operations", "3,612.265"):
        if token not in current:
            fail(f"{label_prefix}current COM21 report missing evidence token '{token}'")


def read_source_files() -> dict[str, str]:
    paths = {
        "library.json": ROOT / "library.json",
        "idf_component.yml": ROOT / "idf_component.yml",
        "Doxyfile": ROOT / "Doxyfile",
        "include/ssd1315/Version.h": ROOT / "include" / "ssd1315" / "Version.h",
        "docs/reports/hil-validation-COM21-20260731.md":
            ROOT / "docs" / "reports" / "hil-validation-COM21-20260731.md",
        "docs/reports/hil-validation-COM21-20260722.md":
            ROOT / "docs" / "reports" / "hil-validation-COM21-20260722.md",
    }
    return {name: path.read_text(encoding="utf-8", errors="replace")
            for name, path in paths.items()}


def find_member_name(members: set[str], suffix: str) -> str:
    matches = sorted(name for name in members if name.endswith(suffix))
    if not matches:
        fail(f"missing required file: {suffix}")
    if len(matches) > 1:
        fail(f"multiple archive members match {suffix}: {', '.join(matches[:4])}")
    return matches[0]


def read_archive_texts(tar: tarfile.TarFile, members: set[str]) -> dict[str, str]:
    files = {}
    for suffix in ("library.json", "idf_component.yml", "Doxyfile",
                   "include/ssd1315/Version.h",
                   "docs/reports/hil-validation-COM21-20260731.md",
                   "docs/reports/hil-validation-COM21-20260722.md"):
        member_name = find_member_name(members, suffix)
        extracted = tar.extractfile(member_name)
        if extracted is None:
            fail(f"archive member is not a regular file: {member_name}")
        files[suffix] = extracted.read().decode("utf-8", errors="replace")
    return files


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", help="Explicit package archive to validate")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    version = load_source_version()
    source_files = read_source_files()
    validate_version_texts(version, source_files, "source ")
    validate_release_evidence(source_files, "source ")
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
            if (parts & FORBIDDEN_PARTS or
                    any(part in name for part in FORBIDDEN_PARTS) or
                    any(name.endswith(suffix) for suffix in FORBIDDEN_SUFFIXES)):
                forbidden_hits.append(name)
        if forbidden_hits:
            fail("forbidden build/internal paths in archive: " + ", ".join(sorted(forbidden_hits)[:8]))

        if archive.name != f"SSD1315-{version}.tar.gz":
            fail(f"archive name {archive.name} does not match library.json version {version}")
        archive_files = read_archive_texts(tar, members)
        validate_version_texts(version, archive_files, "archive ")
        validate_release_evidence(archive_files, "archive ")

    print(f"Package contents PASSED ({archive.name})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
