#!/usr/bin/env python3
"""Validate generated PlatformIO package contents for SSD1315 releases."""

from __future__ import annotations

import pathlib
import sys
import tarfile

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
    "CMakeLists.txt",
    "idf_component.yml",
    "examples/espidf_basic/CMakeLists.txt",
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


def latest_package() -> pathlib.Path:
    candidates = sorted(
        ROOT.glob("SSD1315-*.tar.gz"),
        key=lambda path: path.stat().st_mtime,
        reverse=True,
    )
    if not candidates:
        fail("no SSD1315-*.tar.gz archive found; run 'python -m platformio pkg pack' first")
    return candidates[0]


def normalize(name: str) -> str:
    return name.replace("\\", "/").lstrip("./")


def main() -> int:
    archive = latest_package()
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
