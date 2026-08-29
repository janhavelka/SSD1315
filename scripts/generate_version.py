#!/usr/bin/env python3
"""Synchronize Version.h from library.json and expose build metadata via macros.

Default behavior:
- when run by PlatformIO as an extra script: sync generated headers if needed and
  inject build metadata defines into the compile environment
- when run standalone: sync generated headers if needed

Standalone commands:
  sync
      Regenerate generated headers only if source metadata changed.
  check
      Exit with code 1 when generated headers are out of date.
  bump patch|minor|major
      Update library.json, then regenerate generated headers.
  set X.Y.Z
      Set an explicit semantic version, then regenerate generated headers.
"""

from __future__ import annotations

import json
import os
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, List, Optional, Tuple

ENV = None
try:
    Import("env")  # type: ignore[name-defined]  # PlatformIO / SCons built-in
    ENV = env  # type: ignore[name-defined]
except Exception:
    ENV = None

SEMVER_RE = re.compile(r"^(\d+)\.(\d+)\.(\d+)$")
CPP_IDENTIFIER_RE = re.compile(r"^[A-Za-z_]\w*$")
def _find_project_root() -> Path:
    if ENV is not None:
        return Path(ENV["PROJECT_DIR"]).resolve()
    return Path(__file__).resolve().parent.parent


def _read_text(path: Path) -> str:
    with open(path, "r", encoding="utf-8") as handle:
        return handle.read()


def _write_text(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(content)


def _load_library_json(path: Path) -> Dict[str, object]:
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def _write_library_json(path: Path, data: Dict[str, object]) -> None:
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        json.dump(data, handle, indent=2)
        handle.write("\n")


def _parse_semver(version: str) -> Tuple[int, int, int]:
    match = SEMVER_RE.match(version)
    if not match:
        raise ValueError(f"Invalid semantic version: {version}")
    return tuple(int(part) for part in match.groups())


def _bump_semver(version: str, part: str) -> str:
    major, minor, patch = _parse_semver(version)
    if part == "major":
        return f"{major + 1}.0.0"
    if part == "minor":
        return f"{major}.{minor + 1}.0"
    if part == "patch":
        return f"{major}.{minor}.{patch + 1}"
    raise ValueError(f"Unknown bump target: {part}")


def _resolve_namespace_dir(project_root: Path) -> Path:
    include_root = project_root / "include"
    if not include_root.exists():
        raise FileNotFoundError(f"Missing include directory: {include_root}")

    candidates = [path for path in include_root.iterdir() if path.is_dir()]
    version_candidates = [path for path in candidates if (path / "Version.h").exists()]

    if len(version_candidates) == 1:
        return version_candidates[0]
    if len(candidates) == 1:
        return candidates[0]
    if len(version_candidates) > 1:
        raise RuntimeError(
            f"Multiple Version.h candidates under {include_root}: "
            + ", ".join(str(path) for path in version_candidates)
        )
    raise RuntimeError(
        f"Unable to resolve namespace directory under {include_root}; found: "
        + ", ".join(path.name for path in candidates)
    )


def _macro_prefix(namespace: str) -> str:
    return "".join(ch if ch.isalnum() else "_" for ch in namespace).upper()


def _is_cpp_identifier(value: str) -> bool:
    return bool(CPP_IDENTIFIER_RE.match(value))


def _resolve_namespace(library_data: Dict[str, object], namespace_dir: Path) -> Tuple[str, Optional[str]]:
    headers = library_data.get("headers")
    if isinstance(headers, list):
        for header in headers:
            if not isinstance(header, str):
                continue
            header_name = Path(header).stem
            if _is_cpp_identifier(header_name):
                alias = namespace_dir.name if namespace_dir.name != header_name and _is_cpp_identifier(namespace_dir.name) else None
                return header_name, alias

    namespace = namespace_dir.name
    return namespace, None


def _get_git_info(project_root: Path) -> Tuple[str, str]:
    try:
        commit_result = subprocess.run(
            ["git", "rev-parse", "--short", "HEAD"],
            cwd=project_root,
            capture_output=True,
            text=True,
            timeout=2,
            check=False,
        )
        commit = commit_result.stdout.strip() if commit_result.returncode == 0 else "unknown"

        status_result = subprocess.run(
            ["git", "status", "--porcelain", "--untracked-files=no"],
            cwd=project_root,
            capture_output=True,
            text=True,
            timeout=2,
            check=False,
        )
        git_status = "dirty" if status_result.stdout.strip() else "clean"
        return commit or "unknown", git_status
    except Exception:
        return "unknown", "unknown"


def _cpp_string_literal(value: str) -> str:
    escaped = value.replace("\\", "\\\\").replace('"', '\\"')
    return f'\\"{escaped}\\"'


def _build_timestamp_fields() -> Tuple[str, str, str]:
    source_date_epoch = os.environ.get("SOURCE_DATE_EPOCH", "").strip()
    if source_date_epoch:
        try:
            epoch = int(source_date_epoch)
            if epoch < 0:
                raise ValueError
            now = datetime.fromtimestamp(epoch, tz=timezone.utc)
            build_date = now.strftime("%Y-%m-%d")
            build_time = now.strftime("%H:%M:%S")
            return build_date, build_time, f"{build_date} {build_time} UTC"
        except (OverflowError, OSError, ValueError):
            return "unknown", "unknown", "unknown"
    return "unknown", "unknown", "unknown"


def _append_build_metadata_defines(namespace: str, project_root: Path) -> None:
    if ENV is None:
        return

    build_date, build_time, build_timestamp = _build_timestamp_fields()
    git_commit, git_status = _get_git_info(project_root)
    prefix = _macro_prefix(namespace)

    ENV.Append(
        CPPDEFINES=[
            (f"{prefix}_BUILD_DATE", _cpp_string_literal(build_date)),
            (f"{prefix}_BUILD_TIME", _cpp_string_literal(build_time)),
            (f"{prefix}_BUILD_TIMESTAMP", _cpp_string_literal(build_timestamp)),
            (f"{prefix}_GIT_COMMIT", _cpp_string_literal(git_commit)),
            (f"{prefix}_GIT_STATUS", _cpp_string_literal(git_status)),
        ]
    )


def _render_version_header(namespace: str, version: str, namespace_alias: Optional[str] = None) -> str:
    major, minor, patch = _parse_semver(version)
    version_code = major * 10000 + minor * 100 + patch
    prefix = _macro_prefix(namespace)
    alias_block = ""
    if namespace_alias and namespace_alias != namespace:
        alias_block = f"\nnamespace {namespace_alias} = {namespace};\n"

    return f'''/**
 * @file Version.h
 * @brief Version information for {namespace}.
 *
 * This file is AUTO-GENERATED by scripts/generate_version.py from library.json.
 * DO NOT EDIT MANUALLY. Update version metadata via library.json or
 * scripts/generate_version.py bump/set commands.
 *
 * The SSD1315_* macros below are compile-time override hooks for build
 * metadata. The supported public API is the namespace static constexpr values.
 */

#pragma once

#include <stdint.h>

#ifndef {prefix}_VERSION_STRING
/// @brief Compile-time semantic-version override used by the namespace API.
#define {prefix}_VERSION_STRING "{version}"
#endif

#ifndef {prefix}_BUILD_DATE
/// @brief Compile-time build-date override; defaults to "unknown".
#define {prefix}_BUILD_DATE "unknown"
#endif

#ifndef {prefix}_BUILD_TIME
/// @brief Compile-time build-time override; defaults to "unknown".
#define {prefix}_BUILD_TIME "unknown"
#endif

#ifndef {prefix}_BUILD_TIMESTAMP
/// @brief Compile-time UTC build-timestamp override; defaults to "unknown".
#define {prefix}_BUILD_TIMESTAMP "unknown"
#endif

#ifndef {prefix}_GIT_COMMIT
/// @brief Compile-time source-revision override; defaults to "unknown".
#define {prefix}_GIT_COMMIT "unknown"
#endif

#ifndef {prefix}_GIT_STATUS
/// @brief Compile-time source-tree status override; defaults to "unknown".
#define {prefix}_GIT_STATUS "unknown"
#endif

#ifndef {prefix}_VERSION_FULL
/// @brief Compile-time full-version override composed from the metadata macros.
#define {prefix}_VERSION_FULL {prefix}_VERSION_STRING " (" {prefix}_GIT_COMMIT ", " {prefix}_BUILD_TIMESTAMP ", " {prefix}_GIT_STATUS ")"
#endif

namespace {namespace} {{

/// @brief Major version (breaking changes).
static constexpr uint16_t VERSION_MAJOR = {major};

/// @brief Minor version (backward-compatible features).
static constexpr uint16_t VERSION_MINOR = {minor};

/// @brief Patch version (backward-compatible fixes).
static constexpr uint16_t VERSION_PATCH = {patch};

/// @brief Full semantic version string.
static constexpr const char* VERSION = {prefix}_VERSION_STRING;

/// @brief Encoded version for numeric comparison: MAJOR*10000 + MINOR*100 + PATCH.
static constexpr uint32_t VERSION_CODE = {version_code};

/// @brief Backward-compatible alias used by older repositories.
static constexpr int VERSION_INT = {version_code};

/// @brief Build date string.
static constexpr const char* BUILD_DATE = {prefix}_BUILD_DATE;

/// @brief Build time string.
static constexpr const char* BUILD_TIME = {prefix}_BUILD_TIME;

/// @brief Build timestamp string.
static constexpr const char* BUILD_TIMESTAMP = {prefix}_BUILD_TIMESTAMP;

/// @brief Git commit string.
static constexpr const char* GIT_COMMIT = {prefix}_GIT_COMMIT;

/// @brief Git working tree status string.
static constexpr const char* GIT_STATUS = {prefix}_GIT_STATUS;

/// @brief Full version string with build metadata.
static constexpr const char* VERSION_FULL = {prefix}_VERSION_FULL;

}}  // namespace {namespace}
{alias_block}'''


def _expected_outputs(project_root: Path) -> Dict[Path, str]:
    library_json = project_root / "library.json"
    library_data = _load_library_json(library_json)
    version = str(library_data.get("version", "0.0.0"))
    namespace_dir = _resolve_namespace_dir(project_root)
    namespace, namespace_alias = _resolve_namespace(library_data, namespace_dir)

    return {
        namespace_dir / "Version.h": _render_version_header(namespace, version, namespace_alias),
    }


def _sync_version_metadata(project_root: Path, version: str,
                           check_only: bool, quiet: bool) -> bool:
    targets = (
        (project_root / "idf_component.yml",
         re.compile(r'(?m)^version:\s*["\']?[^"\'\r\n]+["\']?\s*$'),
         f'version: {version}'),
        (project_root / "Doxyfile",
         re.compile(r'(?m)^PROJECT_NUMBER\s*=\s*"[^"]+"\s*$'),
         f'PROJECT_NUMBER         = "{version}"'),
    )
    clean = True
    for path, pattern, replacement in targets:
        current = _read_text(path)
        match = pattern.search(current)
        if match is None:
            raise RuntimeError(f"Missing version field in {path}")
        if match.group(0) == replacement:
            if not quiet:
                print(f"Up to date: {path}")
            continue
        clean = False
        if check_only:
            print(f"Out of date: {path}")
            continue
        updated, count = pattern.subn(replacement, current, count=1)
        if count != 1:
            raise RuntimeError(f"Could not update version field in {path}")
        _write_text(path, updated)
        print(f"Updated: {path}")
    return clean


def _sync_outputs(project_root: Path, check_only: bool, quiet: bool) -> bool:
    outputs = _expected_outputs(project_root)
    version = str(_load_library_json(project_root / "library.json").get("version", "0.0.0"))
    clean = True

    for path, expected in outputs.items():
        current = _read_text(path) if path.exists() else None
        if current == expected:
            if not quiet:
                print(f"Up to date: {path}")
            continue

        clean = False
        if check_only:
            print(f"Out of date: {path}")
            continue

        _write_text(path, expected)
        print(f"Updated: {path}")

    metadata_clean = _sync_version_metadata(project_root, version, check_only, quiet)
    return clean and metadata_clean


def _set_version(project_root: Path, new_version: str) -> str:
    _parse_semver(new_version)
    library_json = project_root / "library.json"
    data = _load_library_json(library_json)
    data["version"] = new_version
    _write_library_json(library_json, data)
    return new_version


def _usage() -> str:
    return (
        "Usage:\n"
        "  scripts/generate_version.py sync\n"
        "  scripts/generate_version.py check\n"
        "  scripts/generate_version.py bump patch|minor|major\n"
        "  scripts/generate_version.py set X.Y.Z"
    )


def main(args: List[str]) -> int:
    project_root = _find_project_root()
    namespace = _resolve_namespace_dir(project_root).name
    _append_build_metadata_defines(namespace, project_root)

    if not args:
        _sync_outputs(project_root, check_only=False, quiet=True)
        return 0

    command = args[0]
    if command == "sync":
        _sync_outputs(project_root, check_only=False, quiet=False)
        return 0
    if command == "check":
        return 0 if _sync_outputs(project_root, check_only=True, quiet=False) else 1
    if command == "bump":
        if len(args) != 2:
            print(_usage(), file=sys.stderr)
            return 2
        current_version = str(_load_library_json(project_root / "library.json").get("version", "0.0.0"))
        new_version = _bump_semver(current_version, args[1])
        _set_version(project_root, new_version)
        _sync_outputs(project_root, check_only=False, quiet=False)
        print(f"Version bumped: {current_version} -> {new_version}")
        return 0
    if command == "set":
        if len(args) != 2:
            print(_usage(), file=sys.stderr)
            return 2
        new_version = _set_version(project_root, args[1])
        _sync_outputs(project_root, check_only=False, quiet=False)
        print(f"Version set: {new_version}")
        return 0

    print(_usage(), file=sys.stderr)
    return 2


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

result = main([])
if result != 0:
    raise SystemExit(result)
