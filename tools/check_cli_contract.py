#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]

HIL_COMMAND_SEQUENCE = [
    "version",
    "scan",
    "probe",
    "cfg",
    "selftest",
    "pattern checker",
    "clear",
    "fill",
    "invert 1",
    "invert 0",
    "contrast 1",
    "contrast 127",
    "contrast 255",
    "flipx 1",
    "flipx 0",
    "flipy 1",
    "flipy 0",
    "scrollh right 0 7",
    "scrollv left 0 7 1",
    "scroll stop",
    "recover",
    "stress 100",
    "stress_mix 100",
    "monitor 1000",
    "monitor 0",
    "contrast 127",
    "clear",
    "cfg",
]

UNIQUE_VALIDATION_COMMANDS = list(dict.fromkeys(HIL_COMMAND_SEQUENCE))

ARDUINO_HELP_TOKENS = [
    "probe\", \"ACK-only address check",
    "reset\", \"Software reinitialize; does not toggle RES#",
    "contrast [1-255]",
    "invert [0|1]",
    "flipx [0|1]",
    "flipy [0|1]",
    "scrollh <left|right> <startPage> <endPage> [speed]",
    "scrollv <left|right> <startPage> <endPage> <offset> [speed]",
    "scroll stop / scrollstop",
    "pattern <checker|vstripes|hstripes> [size]",
    "monitor [ms]",
]

IDF_HELP_TOKENS = [
    "probe is ACK-only; not SSD1315 identity",
    "recover/reset are software-only; they do not toggle RES#",
    "monitor [0|1|ms]",
    "contrast <1..255>",
    "invert <0|1>",
    "flipx <0|1>",
    "flipy <0|1>",
    "scrollh <left|right> <startPage> <endPage> [speed]",
    "scrollv <left|right> <start> <end> <offset> [speed] scroll stop",
    "pattern <checker|vstripes|hstripes>",
]

ARDUINO_SOURCE_TOKENS = [
    "parseScrollSpeedArg(int raw",
    "runSelfTest",
    "fillCheckerboard",
    "display.setInvert",
    "display.setContrast",
    "validationContrastFromIndex",
    "display.setFlipX",
    "display.setFlipY",
    "display.startHorizontalScroll",
    "display.startVerticalScroll",
    "display.stopScroll",
    "Usage: scrollh <left|right> <startPage> <endPage> [speed 0..7]",
    "Usage: scrollv <left|right> <startPage> <endPage> <offset 0..63> [speed 0..7]",
    "scrollh direction must be left or right",
    "scrollv direction must be left or right",
    "runContrastStress",
    "runStressMix",
    "healthMonitor.begin",
]

IDF_SOURCE_TOKENS = [
    "display.setContrast",
    "display.setInvert",
    "display.setFlipX",
    "display.setFlipY",
    "display.startHorizontalScroll",
    "display.startVerticalScroll",
    "display.stopScroll",
    "display.fillCheckerboard",
    "runStress(count, strcmp(cmd, \"stress_mix\") == 0)",
    "monitorNextMs",
]


def fail(msg: str) -> None:
    print(f"CLI contract FAILED: {msg}")
    raise SystemExit(1)


def read(path: pathlib.Path, label: str) -> str:
    if not path.exists():
        fail(f"missing {label}: {path.as_posix()}")
    return path.read_text(encoding="utf-8", errors="replace")


def require_exact_command_block(text: str, label: str) -> None:
    expected = "\n".join(HIL_COMMAND_SEQUENCE)
    normalized = text.replace("\r\n", "\n")
    if expected not in normalized:
        fail(f"{label} missing exact ordered HIL command sequence")


def require_runner_sequence(text: str) -> None:
    cursor = 0
    for command in HIL_COMMAND_SEQUENCE:
        needle = f'"{command}"'
        next_pos = text.find(needle, cursor)
        if next_pos < 0:
            fail(f"HIL runner missing ordered command '{command}'")
        cursor = next_pos + len(needle)


def main() -> int:
    arduino_cli = read(ROOT / "examples" / "01_basic_bringup_cli" / "main.cpp", "Arduino CLI")
    idf_main = read(ROOT / "examples" / "espidf_basic" / "main" / "main.cpp", "native IDF CLI")
    readme = read(ROOT / "README.md", "README")
    hardware_doc = read(ROOT / "docs" / "SSD1315_HARDWARE_VALIDATION.md", "hardware validation doc")
    hil_runbook = read(ROOT / "docs" / "SSD1315_HIL_RUNBOOK.md", "HIL runbook")
    hil_runner = read(ROOT / "tools" / "run_ssd1315_hil.py", "HIL runner")

    for cmd in ("help", "version", "scan", "probe", "recover", "drv", "read", "stress", "cfg",
                "selftest", "clear", "fill", "invert", "contrast", "flipx", "flipy",
                "scrollh", "scrollv", "monitor", "stress_mix"):
        if re.search(rf"\b{re.escape(cmd)}\b", arduino_cli) is None:
            fail(f"Arduino CLI missing mandatory command '{cmd}'")
        if f'"{cmd}"' not in idf_main:
            fail(f"IDF CLI missing mandatory command '{cmd}'")

    for token in ARDUINO_HELP_TOKENS:
        if token not in arduino_cli:
            fail(f"Arduino CLI help missing syntax token: {token}")

    for token in IDF_HELP_TOKENS:
        if token not in idf_main:
            fail(f"IDF CLI help missing syntax token: {token}")

    for token in ARDUINO_SOURCE_TOKENS:
        if token not in arduino_cli:
            fail(f"Arduino CLI implementation missing token: {token}")

    for token in IDF_SOURCE_TOKENS:
        if token not in idf_main:
            fail(f"IDF CLI implementation missing token: {token}")

    for command in UNIQUE_VALIDATION_COMMANDS:
        if re.search(rf"^{re.escape(command)}$", readme, re.MULTILINE) is None:
            fail(f"README missing executable command '{command}'")
        if re.search(rf"^{re.escape(command)}$", hardware_doc, re.MULTILINE) is None:
            fail(f"hardware validation doc missing executable command '{command}'")
        if re.search(rf"^{re.escape(command)}$", hil_runbook, re.MULTILINE) is None:
            fail(f"HIL runbook missing executable command '{command}'")
        if f'"{command}"' not in hil_runner:
            fail(f"HIL runner missing executable command '{command}'")

    for label, text in (
        ("README", readme),
        ("hardware validation doc", hardware_doc),
        ("HIL runbook", hil_runbook),
    ):
        require_exact_command_block(text, label)
    require_runner_sequence(hil_runner)

    if re.search(r"^contrast 0$", hardware_doc, re.MULTILINE):
        fail("hardware validation doc must not use contrast 0 in the smoke sequence")

    for doc_label, doc_text in (("hardware validation doc", hardware_doc), ("HIL runbook", hil_runbook)):
        if re.search(r"^monitor$", doc_text, re.MULTILINE):
            fail(f"{doc_label} must use bounded monitor 1000/monitor 0 sequence")
        if re.search(r"^monitor 1$", doc_text, re.MULTILINE):
            fail(f"{doc_label} must not use 1 ms Arduino monitor interval")

    if re.search(r"setContrast[\s\S]{0,120}(%\s*256|&\s*0xFF)", arduino_cli) or \
       re.search(r"(%\s*256|&\s*0xFF)[\s\S]{0,120}setContrast", arduino_cli):
        fail("Arduino validation stress must not send contrast 0")

    if re.search(r"if \(monitorMode\)\s*\{[^}]*continue;", idf_main, re.DOTALL):
        fail("IDF monitor mode must continue polling stdin so it has a clean stop path")

    for token in ("cfg.nowMs", "cfg.cooperativeYield", "cfg.i2cWriteRead"):
        if token not in arduino_cli:
            fail(f"Arduino CLI missing transport/timing config token '{token}'")

    if (ROOT / "examples" / "common" / "IdfArduinoCompat.h").exists():
        fail("stale ESP-IDF Arduino compatibility shim remains")

    print("CLI contract PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
