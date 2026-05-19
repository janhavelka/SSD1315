#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]


def fail(msg: str) -> None:
    print(f"CLI contract FAILED: {msg}")
    raise SystemExit(1)


def read(path: pathlib.Path, label: str) -> str:
    if not path.exists():
        fail(f"missing {label}: {path.as_posix()}")
    return path.read_text(encoding="utf-8", errors="replace")


def main() -> int:
    arduino_cli = read(ROOT / "examples" / "01_basic_bringup_cli" / "main.cpp", "Arduino CLI")
    idf_main = read(ROOT / "examples" / "espidf_basic" / "main" / "main.cpp", "native IDF CLI")

    for cmd in ("help", "scan", "probe", "recover", "drv", "read", "stress", "cfg"):
        if re.search(rf"\b{re.escape(cmd)}\b", arduino_cli) is None:
            fail(f"Arduino CLI missing mandatory command '{cmd}'")
        if f'"{cmd}"' not in idf_main:
            fail(f"IDF CLI missing mandatory command '{cmd}'")

    if (ROOT / "examples" / "common" / "IdfArduinoCompat.h").exists():
        fail("stale ESP-IDF Arduino compatibility shim remains")

    print("CLI contract PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
