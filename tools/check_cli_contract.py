#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import sys

from run_ssd1315_hil import FUNCTIONAL_COMMANDS, RETENTION_COMMANDS

ROOT = pathlib.Path(__file__).resolve().parents[1]

HIL_COMMAND_SEQUENCE = [command.command for command in FUNCTIONAL_COMMANDS]

UNIQUE_VALIDATION_COMMANDS = list(dict.fromkeys(HIL_COMMAND_SEQUENCE))
RETENTION_COMMAND_SEQUENCE = [command.command for command in RETENTION_COMMANDS]

ARDUINO_HELP_TOKENS = [
    "probe\", \"ACK-only address check",
    "telemetry\", \"Heap/reset/uptime/loop heartbeat",
    "reset\", \"Software reinitialize; does not toggle RES#",
    "contrast [1-255]",
    "invert [0|1]",
    "flipx [0|1]",
    "flipy [0|1]",
    "display <off|on>",
    "allon [0|1]",
    "scrollh <left|right> <startPage> <endPage> [speed]",
    "scrollv <left|right> <startPage> <endPage> <offset> [speed]",
    "scroll stop / scrollstop",
    "pattern <checker|vstripes|hstripes> [size]",
    "monitor [ms]",
    "soakstep <N>",
]

IDF_HELP_TOKENS = [
    "probe is ACK-only; not SSD1315 identity",
    "telemetry prints heap/reset/uptime/loop heartbeat",
    "recover/reset are software-only; they do not toggle RES#",
    "monitor [0|1|ms]",
    "soakstep <n>",
    "contrast <1..255>",
    "invert <0|1>",
    "flipx <0|1>",
    "flipy <0|1>",
    "display <off|on>",
    "allon <0|1>",
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
    "ESP.getCoreVersion()",
    "ESP.getSdkVersion()",
    "ESP.getFlashChipSize()",
    "ESP.getPsramSize()",
    "printTelemetry",
    "validationContrastFromIndex",
    "display.setFlipX",
    "display.setFlipY",
    "Usage: display <off|on>",
    "Usage: allon [0|1|off|on]",
    "display.startHorizontalScroll",
    "display.startVerticalScroll",
    "display.stopScroll",
    "Usage: scrollh <left|right> <startPage> <endPage> [speed 0..7]",
    "Usage: scrollv <left|right> <startPage> <endPage> <offset 0..63> [speed 0..7]",
    "scrollh direction must be left or right",
    "scrollv direction must be left or right",
    "runContrastStress",
    "runStressMix",
    "runStressMix(value, true)",
    "SOAK n=",
    "healthMonitor.begin",
]

IDF_SOURCE_TOKENS = [
    "display.setContrast",
    "esp_get_idf_version()",
    "printTelemetry",
    "display.setInvert",
    "display.setFlipX",
    "display.setFlipY",
    "strcmp(cmd, \"display\")",
    "strcmp(cmd, \"allon\")",
    "display.startHorizontalScroll",
    "display.startVerticalScroll",
    "display.stopScroll",
    "display.fillCheckerboard",
    "runStress(count, mixed, compact)",
    "strcmp(cmd, \"soakstep\") == 0",
    "SOAK n=",
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
    hil_runbook = read(ROOT / "docs" / "SSD1315_HIL_RUNBOOK.md", "HIL runbook")
    hil_runner = read(ROOT / "tools" / "run_ssd1315_hil.py", "HIL runner")
    command_handler = read(ROOT / "examples" / "common" / "CommandHandler.h", "Arduino command parser")
    platformio = read(ROOT / "platformio.ini", "PlatformIO configuration")

    for cmd in ("help", "version", "telemetry", "scan", "probe", "recover", "drv", "read", "stress", "cfg",
                "selftest", "clear", "fill", "invert", "contrast", "flipx", "flipy",
                "display", "allon", "scrollh", "scrollv", "monitor", "stress_mix",
                "soakstep"):
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
        if re.search(rf"^{re.escape(command)}$", hil_runbook, re.MULTILINE) is None:
            fail(f"HIL runbook missing executable command '{command}'")
        if f'"{command}"' not in hil_runner:
            fail(f"HIL runner missing executable command '{command}'")

    for command in RETENTION_COMMAND_SEQUENCE:
        if command not in hil_runbook:
            fail(f"HIL runbook missing retention command '{command}'")
        if f'"{command}"' not in hil_runner:
            fail(f"HIL runner missing retention command '{command}'")

    retention_block = "\n".join(RETENTION_COMMAND_SEQUENCE)
    if retention_block not in hil_runbook.replace("\r\n", "\n"):
        fail("HIL runbook retention sequence differs from runner")

    if "return strcasecmp(cmd, keyword) == 0;" not in command_handler:
        fail("Arduino command parser must use an exact case-insensitive match")
    if 'cmd::match(cmdBuf, "help") || cmd::match(cmdBuf, "?")' not in arduino_cli:
        fail("Arduino CLI must dispatch its advertised '?' help alias")
    if "Selftest result: pass=%lu fail=%lu" not in idf_main:
        fail("IDF selftest must emit deterministic pass/fail counters")
    for token in ("runPageIterationDiagnostic", "if (!display.isPageBufferMode())",
                  "return flushBlocking();"):
        if token not in arduino_cli:
            fail(f"Arduino page-iteration diagnostic missing safe full-buffer path: {token}")

    require_exact_command_block(hil_runbook, "HIL runbook")
    if "docs/SSD1315_HIL_RUNBOOK.md" not in readme:
        fail("README missing HIL runbook link")
    require_runner_sequence(hil_runner)

    for doc_label, doc_text in (("HIL runbook", hil_runbook),):
        if re.search(r"^contrast 0$", doc_text, re.MULTILINE):
            fail(f"{doc_label} must not use contrast 0 in the smoke sequence")
        if re.search(r"^monitor$", doc_text, re.MULTILINE):
            fail(f"{doc_label} must use bounded monitor 1000/monitor 0 sequence")
        if re.search(r"^monitor 1$", doc_text, re.MULTILINE):
            fail(f"{doc_label} must not use 1 ms Arduino monitor interval")

    if re.search(r"setContrast[\s\S]{0,120}(%\s*256|&\s*0xFF)", arduino_cli) or \
       re.search(r"(%\s*256|&\s*0xFF)[\s\S]{0,120}setContrast", arduino_cli):
        fail("Arduino validation stress must not send contrast 0")

    # Positive structural check on cliLoop(). The previous negative regex could
    # never match the real code (the monitor test is a compound condition), so it
    # passed for reasons unrelated to the property it claims to enforce.
    cli_loop = re.search(r"(?ms)^void cliLoop\(\)\s*\{(.*?)^\}", idf_main)
    if cli_loop is None:
        fail("IDF example must define cliLoop()")
    loop_body = cli_loop.group(1)
    cursor = -1
    for token in ("display.tick(now);", "if (monitorMode", "getchar()"):
        found = loop_body.find(token, cursor + 1)
        if found < 0:
            fail(f"IDF cliLoop must run '{token}' after the preceding contract step")
        cursor = found
    monitor_branch = re.search(r"(?s)if \(monitorMode.*?\n    \}", loop_body)
    if monitor_branch is None:
        fail("IDF cliLoop must contain a bounded monitorMode branch")
    elif re.search(r"\b(continue|return|break)\b", monitor_branch.group(0)):
        fail("IDF monitor mode must not skip the stdin read; it needs a clean stop path")

    for token in ("cfg.nowMs", "cfg.cooperativeYield", "cfg.maxWriteBytes"):
        if token not in arduino_cli:
            fail(f"Arduino CLI missing transport/timing config token '{token}'")

    if (ROOT / "examples" / "common" / "IdfArduinoCompat.h").exists():
        fail("stale ESP-IDF Arduino compatibility shim remains")

    for token in (
        "releases/download/55.03.311/platform-espressif32.zip",
        "releases/download/54.03.20/platform-espressif32.zip",
        "board = esp32-s3-devkitc1-n16r8",
        "[env:compat_pioarduino_54_s3]",
    ):
        if token not in platformio:
            fail(f"PlatformIO configuration missing migration contract: {token}")
    s3_section = re.search(
        r"(?ms)^\[env:esp32s3dev\]\s*$\n(.*?)(?=^\[|\Z)", platformio
    )
    if s3_section is None:
        fail("PlatformIO configuration missing esp32s3dev section")
    for stale in ("board_build.flash_size", "board_upload.flash_size",
                  "board_build.partitions", "-mfix-esp32-psram-cache-issue"):
        if stale in s3_section.group(1):
            fail(f"exact N16R8 environment must not override board metadata: {stale}")

    print("CLI contract PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
