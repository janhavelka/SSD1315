#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import sys
from typing import Dict

ROOT = pathlib.Path(__file__).resolve().parents[1]
SCAN_DIRS = ("src", "include")
VALID_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hpp"}

FORBIDDEN_PATTERNS = {
    "millis": re.compile(r"\bmillis\s*\("),
    "micros": re.compile(r"\bmicros\s*\("),
    "delay": re.compile(r"\bdelay\s*\("),
    "delayMicroseconds": re.compile(r"\bdelayMicroseconds\s*\("),
    "yield": re.compile(r"\byield\s*\("),
    "ESP_LOG": re.compile(r"\bESP_LOG[A-Z_]*\b"),
    "Serial": re.compile(r"\bSerial\b"),
    "String": re.compile(r"\bString\b"),
    "esp_api": re.compile(r"\besp_[A-Za-z0-9_]*\s*\("),
}

FORBIDDEN_INCLUDES = {
    "Arduino.h": re.compile(r'^\s*#\s*include\s*[<"]Arduino\.h[>"]', re.MULTILINE),
    "Wire.h": re.compile(r'^\s*#\s*include\s*[<"]Wire\.h[>"]', re.MULTILINE),
    "freertos": re.compile(r'^\s*#\s*include\s*[<"]freertos/', re.MULTILINE),
    "driver": re.compile(r'^\s*#\s*include\s*[<"]driver/', re.MULTILINE),
}
BLOCK_COMMENT_RE = re.compile(r"/\*.*?\*/", re.DOTALL)
LINE_COMMENT_RE = re.compile(r"//[^\n]*")
STRING_RE = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')

ALLOWED_PATTERN_COUNTS: Dict[str, Dict[str, int]] = {}
ALLOWED_INCLUDE_COUNTS: Dict[str, Dict[str, int]] = {}


def strip_comments(text: str) -> str:
    text = BLOCK_COMMENT_RE.sub("", text)
    return LINE_COMMENT_RE.sub("", text)


def strip_non_code(text: str) -> str:
    return STRING_RE.sub('""', strip_comments(text))


def collect_sources() -> list[pathlib.Path]:
    files: list[pathlib.Path] = []
    for dirname in SCAN_DIRS:
        root = ROOT / dirname
        if not root.exists():
            continue
        for path in root.rglob("*"):
            if path.is_file() and path.suffix.lower() in VALID_SUFFIXES:
                files.append(path)
    return files


def main() -> int:
    observed_patterns: Dict[str, Dict[str, int]] = {}
    observed_includes: Dict[str, Dict[str, int]] = {}

    for path in collect_sources():
        rel = path.relative_to(ROOT).as_posix()
        raw = path.read_text(encoding="utf-8", errors="replace")
        commentless = strip_comments(raw)
        code = strip_non_code(raw)

        pattern_counts: Dict[str, int] = {}
        for pattern_name, pattern in FORBIDDEN_PATTERNS.items():
            count = len(pattern.findall(code))
            if count > 0:
                pattern_counts[pattern_name] = count
        if pattern_counts:
            observed_patterns[rel] = pattern_counts

        include_counts: Dict[str, int] = {}
        for include_name, pattern in FORBIDDEN_INCLUDES.items():
            count = len(pattern.findall(commentless))
            if count > 0:
                include_counts[include_name] = count
        if include_counts:
            observed_includes[rel] = include_counts

    errors: list[str] = []

    for rel, counts in observed_patterns.items():
        if rel not in ALLOWED_PATTERN_COUNTS:
            errors.append(f"forbidden core patterns in unexpected file: {rel} -> {counts}")
            continue
        expected = ALLOWED_PATTERN_COUNTS[rel]
        for pattern_name, count in counts.items():
            exp = expected.get(pattern_name, 0)
            if count != exp:
                errors.append(
                    f"core pattern count mismatch in {rel}: {pattern_name} observed={count}, expected={exp}"
                )

    for rel, expected in ALLOWED_PATTERN_COUNTS.items():
        observed = observed_patterns.get(rel, {})
        for pattern_name, exp in expected.items():
            obs = observed.get(pattern_name, 0)
            if obs != exp:
                errors.append(
                    f"core pattern count mismatch in {rel}: {pattern_name} observed={obs}, expected={exp}"
                )
        unexpected_patterns = set(observed.keys()) - set(expected.keys())
        if unexpected_patterns:
            errors.append(f"unexpected core pattern types in {rel}: {sorted(unexpected_patterns)}")

    for rel, counts in observed_includes.items():
        expected = ALLOWED_INCLUDE_COUNTS.get(rel, {})
        for include_name, count in counts.items():
            exp = expected.get(include_name, 0)
            if count != exp:
                errors.append(
                    f"framework include count mismatch in {rel}: {include_name} observed={count}, expected={exp}"
                )

    for rel, expected in ALLOWED_INCLUDE_COUNTS.items():
        observed = observed_includes.get(rel, {})
        for include_name, exp in expected.items():
            obs = observed.get(include_name, 0)
            if obs != exp:
                errors.append(
                    f"framework include count mismatch in {rel}: {include_name} observed={obs}, expected={exp}"
                )

    if errors:
        print("Core timing guard FAILED:")
        for err in errors:
            print(f"- {err}")
        return 1

    print("Core timing guard PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
