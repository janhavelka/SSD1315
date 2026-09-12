#!/usr/bin/env python3
"""Run SSD1315 serial HIL device tests and capture repeatable evidence.

This tool does not flash firmware. Build and upload the selected Arduino or
ESP-IDF validation firmware first, then run this script against its serial CLI.

The runner can automatically classify serial/device evidence. It cannot prove
visual OLED correctness without an operator or camera, so visual commands are
reported separately and never become field-ready evidence by serial output
alone.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import re
import subprocess
import sys
import time
from dataclasses import asdict, dataclass, field
from datetime import datetime
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Tuple


SCRIPT_VERSION = "2.8"
DEFAULT_BAUD = 115200
DEFAULT_TIMEOUT_S = 8.0
DEFAULT_OUT_ROOT = Path("hil_logs")
PROMPT_RE = re.compile(r"(^|\r?\n)> $")
ANSI_RE = re.compile(r"\x1B\[[0-?]*[ -/]*[@-~]")
MAX_RESPONSE_BYTES = 256 * 1024
CRASH_RE = re.compile(r"Guru Meditation|assert failed|abort\(\)|Backtrace:|"
                      r"(?:^|\n)\s*(?:ESP-ROM:|rst:|Rebooting\.\.\.)", re.IGNORECASE)


@dataclass(frozen=True)
class HilCommand:
    command: str
    visual_check: bool = False
    timeout_scale: float = 1.0
    note: str = ""
    risky_visual: bool = False
    require_clean_cfg: bool = True
    success_pattern: str = ""
    completion_pattern: str = ""
    duration_phase: str = "cycle"


@dataclass
class Expectations:
    address: Optional[int] = None
    width: Optional[int] = None
    height: Optional[int] = None
    controller: Optional[str] = None
    panel_profile: Optional[str] = None
    commit: Optional[str] = None
    chip_model: Optional[str] = None
    arduino_core: Optional[str] = None
    esp_idf: Optional[str] = None
    flash_bytes: Optional[int] = None
    psram_bytes: Optional[int] = None
    strict: bool = False


@dataclass
class CommandResult:
    command: str
    serial_result: str
    operator_result: str
    wait_reason: str
    elapsed_s: float
    note: str
    raw_excerpt: str
    clean_excerpt: str
    parsed: Dict[str, object] = field(default_factory=dict)
    operator_notes: str = ""


FUNCTIONAL_COMMANDS: Tuple[HilCommand, ...] = (
    HilCommand("version"),
    HilCommand("telemetry"),
    HilCommand("scan"),
    HilCommand("probe"),
    HilCommand("cfg", require_clean_cfg=False),
    HilCommand("selftest", timeout_scale=2.0,
               note="Selftest is serial/software evidence; it does not prove visual correctness."),
    HilCommand("pattern checker", visual_check=True, risky_visual=True, timeout_scale=2.0),
    HilCommand("clear", visual_check=True),
    HilCommand("fill", visual_check=True, risky_visual=True,
               note="Do not leave full-on OLED content static."),
    HilCommand("invert 1", visual_check=True),
    HilCommand("invert 0", visual_check=True),
    HilCommand("contrast 1", visual_check=True),
    HilCommand("contrast 127", visual_check=True),
    HilCommand("contrast 255", visual_check=True, risky_visual=True,
               note="Restore contrast after observing."),
    HilCommand("contrast 127", visual_check=True),
    HilCommand("clear", visual_check=True),
    HilCommand("rect 4 8 20 12", visual_check=True),
    HilCommand("line 4 8 42 30", visual_check=True,
               note="Asymmetric orientation reference: upper-left rectangle and diagonal tail."),
    HilCommand("flipx 1", visual_check=True,
               note="Wait for rewrite completion; the reference must mirror horizontally."),
    HilCommand("cfg"),
    HilCommand("flipx 0", visual_check=True,
               note="Wait for rewrite completion; the reference must return to its original orientation."),
    HilCommand("cfg"),
    HilCommand("flipy 1", visual_check=True),
    HilCommand("flipy 0", visual_check=True),
    HilCommand("scrollh right 0 7", visual_check=True),
    HilCommand("scrollv left 0 7 1", visual_check=True),
    HilCommand("scroll stop", visual_check=True),
    HilCommand("flush", visual_check=True,
               note="Rewrite GDDRAM after stopping scroll before sleep/wake."),
    HilCommand("display off", visual_check=True),
    HilCommand("display on", visual_check=True,
               note="The rewritten reference must survive sleep/wake without STATE_ERROR."),
    HilCommand("recover", visual_check=True, timeout_scale=2.0,
               note="Recover is software-only; redraw/flush behavior requires observation."),
    HilCommand("stress 100", visual_check=True, timeout_scale=3.0),
    HilCommand("stress_mix 100", visual_check=True, timeout_scale=4.0),
    HilCommand("monitor 1000", timeout_scale=0.75),
    HilCommand("monitor 0", timeout_scale=0.75),
    HilCommand("telemetry"),
    HilCommand("contrast 127", visual_check=True),
    HilCommand("clear", visual_check=True),
    HilCommand("cfg"),
)

SMOKE_COMMANDS: Tuple[HilCommand, ...] = (
    HilCommand("version"),
    HilCommand("telemetry"),
    HilCommand("scan"),
    HilCommand("probe"),
    HilCommand("cfg"),
    HilCommand("selftest", timeout_scale=2.0),
    HilCommand("cfg", require_clean_cfg=False,
               note="Selftest may leave framebuffer data dirty; this intermediate cfg does not require a clean framebuffer."),
)

RETENTION_COMMANDS: Tuple[HilCommand, ...] = (
    HilCommand("version"),
    HilCommand("cfg", require_clean_cfg=False),
    HilCommand("recover", timeout_scale=2.0),
    HilCommand("scroll stop"),
    HilCommand("invert 0"),
    HilCommand("allon 0"),
    HilCommand("clear", visual_check=True),
    HilCommand("fill", visual_check=True, risky_visual=True),
    HilCommand("clear", visual_check=True),
    HilCommand("pattern checker", visual_check=True, risky_visual=True, timeout_scale=2.0),
    HilCommand("clear", visual_check=True),
    HilCommand("demo 1", visual_check=True, timeout_scale=3.0),
    HilCommand("clear", visual_check=True),
    HilCommand("cfg"),
    HilCommand("clear", visual_check=True),
    HilCommand("contrast 1", visual_check=True),
    HilCommand("clear", visual_check=True),
    HilCommand("contrast 127", visual_check=True),
    HilCommand("clear", visual_check=True),
    HilCommand("fill", visual_check=True, risky_visual=True),
    HilCommand("clear", visual_check=True),
    HilCommand("display off", visual_check=True,
               note="Operator records whether ghosting remains while display is off."),
    HilCommand("display on", visual_check=True),
    HilCommand("recover", visual_check=True, timeout_scale=2.0),
    HilCommand("clear", visual_check=True),
    HilCommand("cfg"),
)


ARDUINO_EXTENDED_COMMANDS: Tuple[HilCommand, ...] = (
    HilCommand("?", timeout_scale=3.0, success_pattern=r"SSD1315 CLI Help",
               completion_pattern=r"Safety:"),
    HilCommand("ver", success_pattern=r"SSD1315 library version:"),
    HilCommand("settings", success_pattern=r"\bConfig:"),
    HilCommand("verbose 1", success_pattern=r"Verbose mode:.*ON"),
    HilCommand("verbose 0", success_pattern=r"Verbose mode:.*OFF"),
    HilCommand("drv", success_pattern=r"Driver Health"),
    HilCommand("read", success_pattern=r"\bHealth:.*state="),
    HilCommand("health", success_pattern=r"Driver Health"),
    HilCommand("brief", success_pattern=r"\bHealth:.*state="),
    HilCommand("counters", success_pattern=r"Raw Health Counters:"),
    HilCommand("threshold", success_pattern=r"Offline threshold:"),
    HilCommand("bufsize", success_pattern=r"Buffer size:\s*\d+ bytes"),
    HilCommand("statex", success_pattern=r"initialized=.*controlDirty="),
    HilCommand("buffer 16", success_pattern=r"buffer=.*dump=16"),
    HilCommand("dirty all", success_pattern=r"dirty=(?:yes|true)"),
    HilCommand("dirty clear", success_pattern=r"dirty=(?:no|false)"),
    HilCommand("dirty", success_pattern=r"dirty=(?:no|false)"),
    HilCommand("dirty mark 0 0 127", success_pattern=r"dirty=(?:yes|true)"),
    HilCommand("dirty clear", success_pattern=r"dirty=(?:no|false)"),
    HilCommand("clearerr", success_pattern=r"lastError cleared"),
    HilCommand("pageiter 8", success_pattern=r"pageiter completed, windows=\d+"),
    HilCommand("bright 127", visual_check=True),
    HilCommand("contrast", success_pattern=r"Current contrast:\s*127"),
    HilCommand("bright", success_pattern=r"Current brightness:\s*127"),
    HilCommand("sleep 1", visual_check=True),
    HilCommand("sleep 0", visual_check=True),
    HilCommand("display", success_pattern=r"display=on"),
    HilCommand("sleep", success_pattern=r"sleep=(?:no|false)"),
    HilCommand("allon 1", visual_check=True, risky_visual=True),
    HilCommand("allon 0", visual_check=True),
    HilCommand("allon", success_pattern=r"allon=(?:no|false)"),
    HilCommand("zoom 1", visual_check=True),
    HilCommand("zoom 0", visual_check=True),
    HilCommand("zoom", success_pattern=r"zoom=(?:no|false)"),
    HilCommand("fade fade 1", visual_check=True),
    HilCommand("fade blink 1", visual_check=True),
    HilCommand("fade off 0", visual_check=True),
    HilCommand("fade", success_pattern=r"fade=off interval=0"),
    HilCommand("invert", success_pattern=r"invert=(?:no|false)"),
    HilCommand("flipx", success_pattern=r"flipx=(?:no|false)"),
    HilCommand("flipy", success_pattern=r"flipy=(?:no|false)"),
    HilCommand("scrollstop"),
    HilCommand("scrollarea 0 64"),
    HilCommand("pattern vstripes 4", visual_check=True),
    HilCommand("pattern hstripes 4", visual_check=True),
    HilCommand("line 0 0 127 63", visual_check=True),
    HilCommand("vline 64 0 64", visual_check=True),
    HilCommand("rect 8 8 112 48", visual_check=True),
    HilCommand("fillrect 8 8 32 16", visual_check=True),
    HilCommand("circle 64 32 20", visual_check=True),
    HilCommand("fillcircle 64 32 8", visual_check=True),
    HilCommand("pixel 0 0 1", visual_check=True),
    HilCommand("pixel 0 0", success_pattern=r"pixel\(0,0\)=1"),
    HilCommand("char 0 0 A", visual_check=True),
    HilCommand("bitmap 60 20", visual_check=True),
    HilCommand("textw TunnelMonitor", success_pattern=r"text width:\s*\d+ px"),
    HilCommand("text TunnelMonitor", visual_check=True),
    HilCommand("flush"),
    HilCommand("flushrect 8 8 32 16", visual_check=True),
    HilCommand("reset", success_pattern=r"After begin\(\):"),
    HilCommand("featuretest", success_pattern=r"Selftest result:.*fail=.*0",
               timeout_scale=3.0),
    HilCommand("contrast 127", visual_check=True),
    HilCommand("clear", visual_check=True),
    HilCommand("cfg"),
)


def soak_commands(ops: int) -> Tuple[HilCommand, ...]:
    count = max(1, int(ops))
    return (
        HilCommand("version", duration_phase="prologue"),
        HilCommand("telemetry", duration_phase="prologue"),
        HilCommand("cfg", require_clean_cfg=False, duration_phase="prologue"),
        HilCommand("contrast 127"),
        HilCommand("clear", visual_check=True),
        HilCommand(f"soakstep {count}", visual_check=True,
                   timeout_scale=max(4.0, min(60.0, count / 25.0)),
                   note="Compact bounded mixed-operation soak record."),
        HilCommand("telemetry"),
        HilCommand("clear", visual_check=True),
        HilCommand("telemetry"),
        HilCommand("cfg", duration_phase="epilogue"),
    )


def benchmark_commands(count: int) -> Tuple[HilCommand, ...]:
    bounded_count = max(1, int(count))
    short_count = min(bounded_count, 100)
    return (
        HilCommand("version"),
        HilCommand("cfg", require_clean_cfg=False),
        HilCommand(f"stress {bounded_count}", timeout_scale=max(4.0, min(60.0, bounded_count / 25.0)),
                   note="setContrast benchmark path"),
        HilCommand(f"stress_mix {short_count}", visual_check=True,
                   timeout_scale=max(4.0, min(60.0, short_count / 20.0)),
                   note="mixed draw/flush benchmark path"),
        HilCommand(f"flushstress {short_count}", visual_check=True,
                   timeout_scale=max(4.0, min(60.0, short_count / 10.0)),
                   note="flush benchmark path; Arduino CLI only"),
        HilCommand(f"burst {bounded_count}", timeout_scale=max(4.0, min(60.0, bounded_count / 100.0)),
                   note="command burst benchmark path; Arduino CLI only"),
        HilCommand("clear", visual_check=True),
        HilCommand("cfg"),
    )


def command_plan(mode: str, soak_ops: int, no_risky_visuals: bool = False) -> Tuple[HilCommand, ...]:
    if mode == "smoke":
        plan = SMOKE_COMMANDS
    elif mode == "functional":
        plan = FUNCTIONAL_COMMANDS
    elif mode == "retention":
        plan = RETENTION_COMMANDS
    elif mode == "soak":
        plan = soak_commands(soak_ops)
    elif mode == "all":
        plan = SMOKE_COMMANDS + FUNCTIONAL_COMMANDS + RETENTION_COMMANDS + soak_commands(soak_ops)
    elif mode == "benchmark":
        plan = benchmark_commands(soak_ops)
    elif mode == "arduino-extended":
        plan = ARDUINO_EXTENDED_COMMANDS
    else:
        raise ValueError(f"unsupported mode: {mode}")

    if no_risky_visuals:
        return tuple(item for item in plan if not item.risky_visual)
    return plan


FAIL_TOKEN_PATTERNS = (
    re.compile(r"\b(?:fail|fails|failed|failure|failures)\s*[:=]\s*[1-9]\d*\b", re.IGNORECASE),
    re.compile(r"\bFAIL(?:ED)?\b(?!\s*[:=]\s*0\b)", re.IGNORECASE),
    re.compile(r"\bI2C_TIMEOUT\b", re.IGNORECASE),
    re.compile(r"\bStatus:\s*TIMEOUT\b", re.IGNORECASE),
    re.compile(r"\bDEVICE_NOT_FOUND\b", re.IGNORECASE),
    re.compile(r"\bSTATE_ERROR\b", re.IGNORECASE),
    re.compile(r"\bBUS_ERROR\b", re.IGNORECASE),
    re.compile(r"\bI2C_BUS_ERROR\b", re.IGNORECASE),
    re.compile(r"\bI2C_NACK(?:_ADDR|_DATA)?\b", re.IGNORECASE),
    re.compile(r"\bINVALID_", re.IGNORECASE),
    re.compile(r"\bunknown command\b", re.IGNORECASE),
    re.compile(r"(?:^|\n)\s*\[(?:ERROR|ERR)\]", re.IGNORECASE),
)

PASS_HINTS = (
    re.compile(r"\bStatus:\s*OK\b", re.IGNORECASE),
    re.compile(r"\bOK\b", re.IGNORECASE),
    re.compile(r"\bPASS\b", re.IGNORECASE),
    re.compile(r"\bbegin\(\).*OK\b", re.IGNORECASE),
    re.compile(r"\bVersion\b", re.IGNORECASE),
    re.compile(r"\bConfig\b", re.IGNORECASE),
    re.compile(r"\bMonitor:\s*(ON|OFF)\b", re.IGNORECASE),
)


def _load_serial_module():
    try:
        import serial  # type: ignore
    except ImportError:
        print(
            "pyserial is required for serial HIL runs. Install it with: "
            "python -m pip install pyserial",
            file=sys.stderr,
        )
        raise SystemExit(2)
    return serial


def strip_ansi(text: str) -> str:
    return ANSI_RE.sub("", text)


def decode(data: bytes) -> str:
    return data.decode("utf-8", errors="replace")


def git_value(*args: str, allow_empty: bool = False) -> str:
    try:
        result = subprocess.run(
            ["git", *args],
            cwd=Path(__file__).resolve().parents[1],
            check=True,
            capture_output=True,
            text=True,
        )
    except (OSError, subprocess.CalledProcessError):
        return "unknown"
    value = result.stdout.strip()
    if value or allow_empty:
        return value
    return "unknown"


def make_log_dir(out_root: Path, timestamp: Optional[str] = None) -> Path:
    stamp = timestamp or datetime.now().strftime("%Y%m%d_%H%M%S")
    log_dir = out_root / f"ssd1315_{stamp}"
    if log_dir.exists():
        raise FileExistsError(f"Refusing to overwrite existing HIL log directory: {log_dir}")
    return log_dir


def parse_hex_or_any(text: Optional[str]) -> Optional[int]:
    if text is None or text.lower() == "any":
        return None
    return int(text, 0)


def parse_text_or_any(text: Optional[str]) -> Optional[str]:
    if text is None or text.lower() == "any":
        return None
    return text


def parse_version_info(text: str) -> Dict[str, object]:
    clean = strip_ansi(text)
    data: Dict[str, object] = {}
    match = re.search(r"SSD1315 library version:\s*([^\r\n]+)", clean, re.IGNORECASE)
    if match:
        data["library_version"] = match.group(1).strip()
    match = re.search(r"SSD1315 library commit:\s*([0-9a-fA-F]+|unknown)(?:\s*\(([^)]+)\))?", clean)
    if match:
        data["firmware_commit"] = match.group(1)
        if match.group(2):
            data["firmware_git_status"] = match.group(2)
    match = re.search(r"Framework:\s*([^\r\n]+)", clean, re.IGNORECASE)
    if match:
        data["framework"] = match.group(1).strip()
    match = re.search(r"MCU:\s*([^\s]+)\s+revision=(\d+)", clean, re.IGNORECASE)
    if match:
        data["chip_model"] = match.group(1).strip()
        data["chip_revision"] = int(match.group(2))
    match = re.search(r"Arduino-ESP32:\s*([^\r\n]+)", clean, re.IGNORECASE)
    if match:
        data["arduino_core_version"] = match.group(1).strip()
    match = re.search(r"ESP-IDF:\s*([^\r\n]+)", clean, re.IGNORECASE)
    if match:
        data["esp_idf_version"] = match.group(1).strip()
    match = re.search(r"Flash bytes:\s*(\d+)", clean, re.IGNORECASE)
    if match:
        data["flash_bytes"] = int(match.group(1))
    match = re.search(r"PSRAM:\s*(ready|not-ready)\s+bytes=(\d+)", clean, re.IGNORECASE)
    if match:
        data["psram_ready"] = match.group(1).lower() == "ready"
        data["psram_bytes"] = int(match.group(2))
    match = re.search(r"Controller profile:\s*([^\r\n]+)", clean, re.IGNORECASE)
    if match:
        data["controller_profile"] = match.group(1).strip()
    match = re.search(r"Panel profile:\s*([^\r\n]+)", clean, re.IGNORECASE)
    if match:
        data["panel_profile"] = match.group(1).strip()
    match = re.search(r"Active I2C address:\s*0x([0-9a-fA-F]+)", clean, re.IGNORECASE)
    if match:
        data["i2c_address"] = int(match.group(1), 16)
    match = re.search(r"Geometry:\s*(\d+)x(\d+).*?pageBufferPages=(\d+)", clean, re.IGNORECASE)
    if match:
        data["width"] = int(match.group(1))
        data["height"] = int(match.group(2))
        data["page_buffer_pages"] = int(match.group(3))
    return data


CFG_COMMANDS = ("cfg", "settings")
CFG_STATE_FIELDS = {
    "initialized": "initialized",
    "dirty": "dirty",
    "flushing": "(?:flush|flushing)",
    "control_dirty": "controlDirty",
    "scroll_active": "scrollActive",
}


def parse_cfg(text: str) -> Dict[str, object]:
    clean = strip_ansi(text)
    data: Dict[str, object] = {}
    match = re.search(r"(?:width=|geometry=)(\d+)x(\d+)", clean)
    if match:
        data["width"] = int(match.group(1))
        data["height"] = int(match.group(2))
    else:
        match = re.search(r"\bwidth=(\d+)\s+height=(\d+)", clean)
        if match:
            data["width"] = int(match.group(1))
            data["height"] = int(match.group(2))
    match = re.search(r"(?:addr=|Active I2C address:\s*)0x([0-9a-fA-F]+)", clean)
    if match:
        data["i2c_address"] = int(match.group(1), 16)
    for key, regex in {
        "controller_profile": r"controllerProfile=([^\s]+)",
        "panel_profile": r"panelProfile=([^\s]+)",
        "state": r"state=([A-Z]+)",
        "online": r"online=(yes|no|true|false)",
        "sleep": r"(?:sleep|sleeping)=(yes|no|true|false)",
        "all_on": r"allOn=(yes|no|true|false)",
        "contrast": r"contrast=(\d+)",
        "com_pins": r"comPins=(0x[0-9a-fA-F]+)",
        "charge_pump": r"chargePump=(0x[0-9a-fA-F]+)",
        "iref": r"iref=(0x[0-9a-fA-F]+)",
        "vcomh": r"vcomh=(0x[0-9a-fA-F]+)",
    }.items():
        match = re.search(regex, clean)
        if match:
            value = match.group(1)
            if value.startswith("0x"):
                data[key] = int(value, 16)
            elif value.isdigit():
                data[key] = int(value)
            else:
                data[key] = value
    for key, field in CFG_STATE_FIELDS.items():
        # Each required state must occur once as a complete, recognized token.
        # Missing, malformed or repeated evidence must never default to clean.
        values = re.findall(rf"(?<!\S){field}=([^\s]*)", clean, re.IGNORECASE)
        if len(values) != 1:
            continue
        value = values[0].lower()
        if value in ("yes", "no", "true", "false"):
            data[key] = value
        elif key == "dirty" and re.fullmatch(r"0x[0-9a-f]{1,2}", value):
            data[key] = int(value, 16)
    return data


def parse_telemetry(text: str) -> Dict[str, object]:
    clean = strip_ansi(text)
    data: Dict[str, object] = {}
    patterns = {
        "uptime_ms": "uptimeMs", "loop_heartbeat": "loopHeartbeat",
        "last_loop_ms": "lastLoopMs", "free_heap": "freeHeap",
        "min_free_heap": "minFreeHeap", "reset_reason_code": "resetReason",
        "total_success": "totalSuccess", "total_failures": "totalFailures",
    }
    for key, field in patterns.items():
        values = re.findall(rf"(?<!\S){field}=([^\s]*)", clean)
        if len(values) == 1 and re.fullmatch(r"\d+", values[0]):
            data[key] = int(values[0])
    for key, field, pattern in (("driver_state", "driverState", "UNINIT|READY|DEGRADED|OFFLINE"),
                                ("online", "online", "true|false"),
                                ("dirty", "dirty", "true|false"),
                                ("control_dirty", "controlDirty", "true|false")):
        values = re.findall(rf"(?<!\S){field}=([^\s]*)", clean)
        if len(values) == 1 and re.fullmatch(pattern, values[0]):
            data[key] = values[0]
    match = one_record(clean, "resetReason=", r"resetReason=\d+\s+\(([^)]+)\)")
    if match:
        data["reset_reason"] = match.group(1)
    return data


def response_lines(text: str) -> List[str]:
    """Normalize only the Arduino logger prefix; keep logical records intact."""
    return [re.sub(r"^\s*\[[A-Z]+\]\s*", "", re.sub(r"^\s*>[ \t]*", "", line)).strip()
            for line in strip_ansi(text).splitlines()]


def one_record(text: str, prefix: str, pattern: str):
    lines = [line for line in response_lines(text) if line.startswith(prefix)]
    return re.fullmatch(pattern, lines[0]) if len(lines) == 1 else None


def version_complete(text: str) -> bool:
    return bool(one_record(text, "SSD1315 library version:", r"SSD1315 library version:\s+\S+")
                and one_record(text, "Geometry:", r"Geometry:\s+\d+x\d+\s+pages=\d+\s+pageBufferPages=\d+"))


TELEMETRY_FIELDS = ("uptime_ms", "loop_heartbeat", "last_loop_ms", "free_heap",
                    "min_free_heap", "reset_reason_code", "reset_reason",
                    "driver_state", "online", "dirty", "control_dirty",
                    "total_success", "total_failures")


def parse_health(text: str) -> Dict[str, object]:
    """Require one complete current Arduino or IDF health snapshot."""
    state = r"(UNINIT|READY|DEGRADED|OFFLINE)"
    boolean = r"(true|false|yes|no)"
    headers = ("Health:", "Display:", "Raw Health Counters:", "=== Driver Health")
    if sum(line.startswith(headers) for line in response_lines(text)) != 1:
        return {}
    compact = one_record(text, "Health:", r"Health: state=" + state +
                         r" online=" + boolean + r" controlDirty=" + boolean +
                         r" consecFail=(\d+) ok=(\d+) fail=(\d+)")
    if compact:
        keys = ("driver_state", "online", "control_dirty", "consecutive", "total_success", "total_failures")
        return dict(zip(keys, compact.groups()))
    compact = one_record(text, "Display:", r"Display: state=" + state + r" online=" + boolean +
                         r" init=" + boolean + r" sleep=" + boolean + r" flush=" + boolean +
                         r" dirty=(0x[0-9A-Fa-f]{2}) controlDirty=" + boolean +
                         r" ok=(\d+) fail=(\d+) consec=(\d+)")
    if compact:
        keys = ("driver_state", "online", "initialized", "sleeping", "flushing", "dirty",
                "control_dirty", "total_success", "total_failures", "consecutive")
        return dict(zip(keys, compact.groups()))
    if one_record(text, "Raw Health Counters:", r"Raw Health Counters:"):
        patterns = (
            ("driver_state", "state()", r"state\(\)\s*=\s*\d+ \(" + state + r"\)"),
            ("online", "isOnline()", r"isOnline\(\)\s*=\s*" + boolean),
            ("consecutive", "consecutiveFailures()", r"consecutiveFailures\(\)\s*=\s*(\d+)"),
            ("total_success", "totalSuccess()", r"totalSuccess\(\)\s*=\s*(\d+)"),
            ("total_failures", "totalFailures()", r"totalFailures\(\)\s*=\s*(\d+)"),
            ("last_ok", "lastOkMs()", r"lastOkMs\(\)\s*=\s*(\d+)"),
            ("last_fail", "lastErrorMs()", r"lastErrorMs\(\)\s*=\s*(\d+)"),
            ("last_code", "lastError().code", r"lastError\(\)\.code\s*=\s*\d+ \(([A-Z][A-Z0-9_]*)\)"),
            ("last_detail", "lastError().detail", r"lastError\(\)\.detail\s*=\s*(-?\d+)"),
            ("last_message", "lastError().msg", r'lastError\(\)\.msg\s*=\s*"(.*)"'),
        )
    elif one_record(text, "=== Driver Health", r"=== Driver Health ==="):
        for title in ("=== Timestamps ===", "=== Last Error ==="):
            if not one_record(text, title, re.escape(title)):
                return {}
        patterns = (
            ("driver_state", "State:", "State: " + state),
            ("online", "Online:", "Online: " + boolean),
            ("control_dirty", "Control state dirty:", "Control state dirty: " + boolean),
            ("consecutive", "Consecutive failures:", r"Consecutive failures: (\d+)"),
            ("total_success", "Total success:", r"Total success: (\d+)"),
            ("total_failures", "Total failures:", r"Total failures: (\d+)"),
            ("success_rate", "Success rate:", r"Success rate: (\d+\.\d+)%"),
            ("last_ok", "Last success:", r"Last success: (never|\d+ ms ago \(at \d+ ms\))"),
            ("last_fail", "Last failure:", r"Last failure: (never|\d+ ms ago \(at \d+ ms\))"),
        )
        if one_record(text, "Last error:", r"Last error: none"):
            if any(line.startswith(("Code:", "Detail:", "Message:")) for line in response_lines(text)):
                return {}
        else:
            patterns += (("last_code", "Code:", r"Code: ([A-Z][A-Z0-9_]*)"),
                         ("last_detail", "Detail:", r"Detail: (-?\d+)"),
                         ("last_message", "Message:", r"Message: (.*)"))
    else:
        return {}
    result = {}
    for key, prefix, pattern in patterns:
        match = one_record(text, prefix, pattern)
        if match is None:
            return {}
        result[key] = match.group(1)
    if result.get("control_dirty") in ("true", "yes"):
        if not one_record(text, "Control state error:", r"Control state error: [A-Z][A-Z0-9_]* detail=-?\d+ msg=.*"):
            return {}
    return result


def parse_scan_addresses(text: str) -> List[int]:
    """Return ACKed addresses from scanner grid rows, excluding footer text."""
    addresses: List[int] = []
    clean = strip_ansi(text)
    for match in re.finditer(r"^\s*[0-7][0-9A-Fa-f]:\s*(.*)$", clean, re.MULTILINE):
        for token in re.findall(r"(?<![0-9A-Fa-f])[0-9A-Fa-f]{2}(?![0-9A-Fa-f])", match.group(1)):
            address = int(token, 16)
            if 0x08 <= address <= 0x77:
                addresses.append(address)
    return addresses


def parse_counters(text: str) -> Dict[str, int]:
    clean = strip_ansi(text)
    counters: Dict[str, int] = {}
    patterns = {
        "pass": r"\bpass\s*[:=]\s*([^\s]*)",
        "fail": r"\bfail\s*[:=]\s*([^\s]*)",
        "skip": r"\bskip\s*[:=]\s*([^\s]*)",
        "successes": r"\bSuccesses:\s*([^\s]*)",
        "failures": r"\bFailures:\s*([^\s]*)",
        "operations": r"\b(?:Operations|Ops|Total ops):\s*([^\s]*)",
    }
    for key, regex in patterns.items():
        values = re.findall(regex, clean, re.IGNORECASE)
        if len(values) == 1 and re.fullmatch(r"[0-9]+", values[0]):
            counters[key] = int(values[0])
    return counters


def parse_compact_soak(text: str) -> Dict[str, object]:
    """Parse one complete sub-64-byte soak transaction record."""
    clean = strip_ansi(text)
    match = one_record(clean, "SOAK", r"SOAK n=(\d+) o=(\d+) f=(\d+) do=(\d+) df=(\d+) "
                       r"s=([A-Z_]+) c=(\d+)")
    if match is None:
        return {}
    return {
        "counter_operations": int(match.group(1)),
        "counter_successes": int(match.group(2)),
        "counter_failures": int(match.group(3)),
        "driver_success_delta": int(match.group(4)),
        "driver_failure_delta": int(match.group(5)),
        "driver_state": match.group(6),
        "consecutive_failures": int(match.group(7)),
    }


def parse_command_count(command: str) -> Optional[int]:
    match = re.search(r"\b(?:stress|stress_mix|soakstep|flushstress|burst)\s+(\d+)\b", command)
    return int(match.group(1)) if match else None


def workload_counters(command: str, text: str) -> Dict[str, int]:
    """Do not combine fields from separate or partly received summaries."""
    if command in ("selftest", "demo") or command.startswith("demo "):
        title = "Selftest result:" if command == "selftest" else "Demo summary:"
        match = one_record(text, title, re.escape(title) + r"\s+pass=(\d+)\s+fail=(\d+)(?:\s+skip=(\d+))?")
        if not match:
            return {}
        result = {"pass": int(match.group(1)), "fail": int(match.group(2))}
        if match.group(3) is not None:
            result["skip"] = int(match.group(3))
        return result
    lines = response_lines(text)
    candidates = [index for index, line in enumerate(lines) if line.startswith("Results:")]
    if len(candidates) != 1 or lines[candidates[0]] != "Results:":
        return {}
    return parse_counters("\n".join(lines[candidates[0] + 1:]))


def has_failure_token(clean_response: str) -> bool:
    return bool(CRASH_RE.search(clean_response)) or any(pattern.search(clean_response) for pattern in FAIL_TOKEN_PATTERNS)


def check_expectations(parsed: Dict[str, object], expectations: Expectations,
                       check_commit: bool = True, observed_only: bool = False) -> Optional[str]:
    # Retry admission must distinguish absent, still-arriving identity fields
    # from an identity mismatch that was already captured.
    if observed_only:
        fields = {
            "controller": "controller_profile", "address": "i2c_address",
            "width": "width", "height": "height", "panel_profile": "panel_profile",
            "commit": "firmware_commit", "chip_model": "chip_model",
            "arduino_core": "arduino_core_version", "esp_idf": "esp_idf_version",
            "flash_bytes": "flash_bytes", "psram_bytes": "psram_bytes",
        }
        expectations = Expectations(**{
            name: getattr(expectations, name) for name, key in fields.items() if key in parsed
        })
    if expectations.controller and parsed.get("controller_profile") != expectations.controller:
        return f"expected controller {expectations.controller}, observed {parsed.get('controller_profile')}"
    if expectations.address is not None and parsed.get("i2c_address") != expectations.address:
        return f"expected address 0x{expectations.address:02X}, observed {parsed.get('i2c_address')}"
    if expectations.width is not None and parsed.get("width") != expectations.width:
        return f"expected width {expectations.width}, observed {parsed.get('width')}"
    if expectations.height is not None and parsed.get("height") != expectations.height:
        return f"expected height {expectations.height}, observed {parsed.get('height')}"
    if expectations.panel_profile and parsed.get("panel_profile") != expectations.panel_profile:
        return f"expected panel profile {expectations.panel_profile}, observed {parsed.get('panel_profile')}"
    if expectations.commit and check_commit:
        observed = str(parsed.get("firmware_commit", ""))
        if not observed.startswith(expectations.commit):
            return f"expected commit prefix {expectations.commit}, observed {observed or 'unknown'}"
    if check_commit:
        if expectations.chip_model:
            observed = str(parsed.get("chip_model", ""))
            if observed.lower() != expectations.chip_model.lower():
                return f"expected chip model {expectations.chip_model}, observed {observed or 'unknown'}"
        for expected, key, label in (
            (expectations.arduino_core, "arduino_core_version", "Arduino-ESP32"),
            (expectations.esp_idf, "esp_idf_version", "ESP-IDF"),
        ):
            if expected:
                observed = str(parsed.get(key, ""))
                if observed.lstrip("vV") != expected.lstrip("vV"):
                    return f"expected {label} {expected}, observed {observed or 'unknown'}"
        for expected, key, label in (
            (expectations.flash_bytes, "flash_bytes", "flash bytes"),
            (expectations.psram_bytes, "psram_bytes", "PSRAM bytes"),
        ):
            if expected is not None and parsed.get(key) != expected:
                return f"expected {label} {expected}, observed {parsed.get(key, 'unknown')}"
    return None


ARDUINO_STATUS_LABELS = {
    "contrast": "setContrast",
    "bright": "setBrightness",
    "scrollstop": "scroll stop",
    "text": "text+flush",
    "clear": "clear+flush",
    "reset": "begin",
    "probe": "Probe result",
    "recover": "Recover result",
}


def parse_status_codes(command: HilCommand, clean_response: str) -> List[str]:
    """Read current-command/IDF status records, excluding historical diagnostics."""
    name = command.command.split()[0]
    labels = "|".join(re.escape(label) for label in
                      ("Status", command.command, name, ARDUINO_STATUS_LABELS.get(name, name)))
    return re.findall(
        rf"(?m)^[ \t]*(?:\[[^\]\r\n]+\][ \t]*)*(?:{labels}):[ \t]*"
        r"([A-Z][A-Z0-9_]*)(?=\s|$)", "\n".join(response_lines(clean_response)),
    )


def status_record_lines(command: HilCommand, text: str) -> List[str]:
    name = command.command.split()[0]
    prefixes = tuple(label + ":" for label in
                     ("Status", command.command, name, ARDUINO_STATUS_LABELS.get(name, name)))
    return [line for line in response_lines(text) if line.startswith(prefixes)]


def operation_health_complete(name: str, text: str) -> bool:
    """Arduino probe/recover include before, after and change records."""
    lines = response_lines(text)
    title = f"=== {name.capitalize()} Operation ==="
    if title not in lines:
        # IDF prints Status only. Arduino-specific fragments cannot establish
        # completion when the operation header was lost.
        arduino_markers = ("===", "Before ", "After ", "Health:", "Changes:",
                           "Probe result:", "Recover result:", "Note:")
        return (any(line.startswith("Status:") for line in lines)
                and not any(line.startswith(arduino_markers) for line in lines))
    markers = (title, f"Before {name}:", f"After {name}:", "Changes:")
    if any(lines.count(marker) != 1 for marker in markers):
        return False
    positions = [lines.index(marker) for marker in markers]
    if positions != sorted(positions):
        return False
    before = parse_health("\n".join(lines[positions[1] + 1:positions[2]]))
    after = parse_health("\n".join(lines[positions[2] + 1:positions[3]]))
    changes = [line for line in lines[positions[3] + 1:] if line and line not in (">", "TM_CLI_RESPONSE_END")]
    pattern = (r"\(no changes\)|State: (?:UNINIT|READY|DEGRADED|OFFLINE) -> (?:UNINIT|READY|DEGRADED|OFFLINE)|"
               r"ConsecFail: \d+ -> \d+|Total(?:OK|Fail): \d+ -> \d+ \(\+\d+\)")
    return bool(before and after and changes and all(re.fullmatch(pattern, line) for line in changes))


def classify_serial(command: HilCommand, response: str,
                    expectations: Optional[Expectations] = None) -> Tuple[str, str, Dict[str, object]]:
    expectations = expectations or Expectations()
    clean_response = strip_ansi(response)
    parsed: Dict[str, object] = {}
    if command.command in ("version", "ver"):
        for prefix in ("=== Version Info", "SSD1315 library version:", "SSD1315 library commit:", "Geometry:",
                       "Framework:", "Controller profile:", "Active I2C address:"):
            if sum(line.startswith(prefix) for line in response_lines(clean_response)) > 1:
                return "FAIL", "repeated firmware identity record", parsed
        parsed.update(parse_version_info(clean_response))
    if command.command in CFG_COMMANDS:
        parsed.update(parse_cfg(clean_response))
    if command.command == "telemetry":
        parsed.update(parse_telemetry(clean_response))
    parsed.update({f"counter_{k}": v for k, v in parse_counters(clean_response).items()})

    if not clean_response.strip():
        return "FAIL", "no serial response captured", parsed

    if (has_failure_token(clean_response) or
            any(code not in ("OK", "IN_PROGRESS")
                for code in parse_status_codes(command, clean_response))):
        return "FAIL", "failure token found in serial response", parsed

    if command.command in ("version", "ver"):
        mismatch = check_expectations(parsed, expectations)
        if mismatch:
            return "FAIL", mismatch, parsed
        if version_complete(clean_response):
            return "PASS", "firmware identity parsed", parsed
        return "REVIEW_REQUIRED", "firmware identity is incomplete or repeated", parsed

    if command.command == "scan":
        addresses = parse_scan_addresses(clean_response)
        parsed["scan_addresses"] = addresses
        if expectations.address is not None:
            if expectations.address not in addresses:
                return "FAIL", f"expected address 0x{expectations.address:02X} not found in scan", parsed
        if "Scan complete" not in clean_response:
            return "REVIEW_REQUIRED", "scan completion marker missing", parsed
        return "PASS", "scan completed; ACK addresses parsed from scanner rows", parsed

    if command.command in CFG_COMMANDS:
        mismatch = check_expectations(parsed, expectations, check_commit=False)
        if mismatch:
            return "FAIL", mismatch, parsed
        missing = [key for key in CFG_STATE_FIELDS if key not in parsed]
        if missing:
            return "FAIL", "cfg state fields missing, invalid or repeated: " + ", ".join(missing), parsed
        if command.require_clean_cfg:
            bad_cfg = []
            if parsed["initialized"] not in ("yes", "true"):
                bad_cfg.append("uninitialized")
            if parsed["control_dirty"] in ("yes", "true"):
                bad_cfg.append("controlDirty")
            if parsed["flushing"] in ("yes", "true"):
                bad_cfg.append("flushing")
            if parsed["scroll_active"] in ("yes", "true"):
                bad_cfg.append("scrollActive")
            dirty = parsed["dirty"]
            if isinstance(dirty, str) and dirty.lower() in ("yes", "true"):
                bad_cfg.append("dirty")
            if isinstance(dirty, int) and dirty != 0:
                bad_cfg.append("dirty")
            if bad_cfg:
                return "FAIL", "cfg reports non-clean final state: " + ", ".join(bad_cfg), parsed
        else:
            return "PASS", "cfg parsed; clean-state check not required for this intermediate cfg", parsed
        return "PASS", "cfg parsed and clean-state checks passed", parsed

    if command.command == "telemetry":
        if parsed.get("free_heap") == 0 or parsed.get("min_free_heap") == 0:
            return "FAIL", "telemetry reports zero heap", parsed
        missing = [key for key in TELEMETRY_FIELDS if key not in parsed]
        if missing or not one_record(clean_response, "Telemetry:", r"Telemetry:"):
            return "REVIEW_REQUIRED", "telemetry fields missing, invalid or repeated: " + ", ".join(missing), parsed
        if parsed["driver_state"] != "READY" or parsed["online"] != "true" or parsed["control_dirty"] != "false":
            return "FAIL", "telemetry reports unhealthy driver state", parsed
        return "PASS", "telemetry parsed", parsed

    if command.command in ("drv", "health", "read", "brief", "counters"):
        snapshot = parse_health(clean_response)
        parsed.update(snapshot)
        if not snapshot:
            return "REVIEW_REQUIRED", "health snapshot incomplete, invalid or repeated", parsed
        if (snapshot["driver_state"] != "READY" or snapshot["online"] not in ("true", "yes")
                or int(snapshot["consecutive"]) != 0 or snapshot.get("control_dirty") in ("true", "yes")):
            return "FAIL", "health snapshot reports unhealthy driver state", parsed
        return "PASS", "complete health snapshot parsed", parsed

    if command.command == "probe":
        if len(status_record_lines(command, clean_response)) != 1 or not operation_health_complete("probe", clean_response):
            return "REVIEW_REQUIRED", "probe status record missing or repeated", parsed
        if re.search(r"\bStatus:\s*OK\b|\bProbe result:\s*OK\b", clean_response, re.IGNORECASE):
            return "PASS", "probe reported OK", parsed
        return "REVIEW_REQUIRED", "probe output needs human review", parsed

    if command.command == "selftest" or command.command.split()[0] == "demo":
        counters = workload_counters(command.command, clean_response)
        parsed.update({f"counter_{key}": value for key, value in counters.items()})
        if counters.get("fail") == 0 and counters.get("pass", 0) > 0:
            return ("SERIAL_PASS_OPERATOR_REQUIRED" if command.visual_check else "PASS",
                    "selftest/demo counters have fail=0", parsed)
        return "REVIEW_REQUIRED", "selftest/demo counters not fully parsed", parsed

    if command.command.startswith(("stress", "soakstep", "flushstress", "burst")):
        expected = parse_command_count(command.command)
        counters = workload_counters(command.command, clean_response)
        failures = counters.get("failures")
        successes = counters.get("successes")
        if command.command.startswith("soakstep"):
            compact = parse_compact_soak(clean_response)
            if not compact:
                return "REVIEW_REQUIRED", "compact soak health record is incomplete", parsed
            parsed.update(compact)
            operations = compact["counter_operations"]
            successes = compact["counter_successes"]
            failures = compact["counter_failures"]
            if (expected is None or operations != expected or
                    successes is None or failures is None or
                    successes + failures != operations):
                return "FAIL", "compact soak operation counters do not reconcile", parsed
            driver_ok = compact["driver_success_delta"]
            driver_fail = compact["driver_failure_delta"]
            driver_state = compact["driver_state"]
            consecutive_fail = compact["consecutive_failures"]
            if (driver_fail != 0 or consecutive_fail != 0 or
                    driver_state != "READY" or driver_ok <= 0):
                return "FAIL", "compact soak driver health is not clean", parsed
        if failures == 0 and expected is not None and successes == expected:
            return ("SERIAL_PASS_OPERATOR_REQUIRED" if command.visual_check else "PASS",
                    f"stress counters match N={expected}", parsed)
        if failures == 0 and expected is not None and successes is not None:
            return "FAIL", f"stress counters do not match requested N={expected}", parsed
        if failures == 0 and expected is None and successes is not None:
            return ("SERIAL_PASS_OPERATOR_REQUIRED" if command.visual_check else "PASS",
                    "stress counters show zero failures", parsed)
        return "REVIEW_REQUIRED", "stress counters not fully parsed", parsed

    if command.command.startswith("monitor"):
        expected = "OFF" if command.command.endswith(" 0") else "ON"
        if re.search(rf"\b(?:Health monitor|Monitor):\s*{expected}\b", clean_response, re.IGNORECASE):
            return "PASS", f"monitor {expected.lower()} acknowledged", parsed

    if command.command == "recover" and (len(status_record_lines(command, clean_response)) != 1
                                         or not operation_health_complete("recover", clean_response)):
        return "REVIEW_REQUIRED", "recover status record missing or repeated", parsed

    if command.success_pattern and re.search(command.success_pattern, clean_response,
                                             re.IGNORECASE | re.MULTILINE):
        return ("SERIAL_PASS_OPERATOR_REQUIRED" if command.visual_check else "PASS",
                "command-specific success marker found", parsed)

    if command.visual_check and any(pattern.search(clean_response) for pattern in PASS_HINTS):
        return "SERIAL_PASS_OPERATOR_REQUIRED", "serial OK; visual command requires operator evidence", parsed

    if any(pattern.search(clean_response) for pattern in PASS_HINTS):
        return "PASS", "serial response contained an expected success token", parsed

    if command.visual_check:
        return "SERIAL_REVIEW_OPERATOR_REQUIRED", "visual command serial output needs review", parsed

    return "REVIEW_REQUIRED", "serial response did not contain a deterministic pass token", parsed


def response_has_completion(command: HilCommand, response: str) -> bool:
    clean = strip_ansi(response)
    if not clean.strip():
        return False
    if CRASH_RE.search(clean):
        return True
    if re.search(r"\bunknown command\b", clean, re.IGNORECASE):
        return True
    if "TM_CLI_RESPONSE_END" in clean:
        return True
    # The last token may still be arriving from serial (e.g. dirty=0x0[1]).
    # Only delimited fields/status codes establish a completion boundary.
    complete_tokens = re.sub(r"\S+\Z", "", clean)
    status_codes = parse_status_codes(command, complete_tokens)
    if any(code not in ("OK", "IN_PROGRESS") for code in status_codes):
        return True
    if command.command in CFG_COMMANDS:
        parsed = parse_cfg(complete_tokens)
        return "Config:" in clean and all(key in parsed for key in CFG_STATE_FIELDS)
    if command.command in ("drv", "health", "read", "brief", "counters"):
        return bool(parse_health(complete_tokens))
    if command.command == "telemetry":
        parsed = parse_telemetry(complete_tokens)
        return bool(one_record(complete_tokens, "Telemetry:", r"Telemetry:")
                    and all(key in parsed for key in TELEMETRY_FIELDS))
    if command.command in ("version", "ver"):
        return version_complete(complete_tokens)
    completion_pattern = command.completion_pattern or command.success_pattern
    if completion_pattern and re.search(completion_pattern, clean,
                                        re.IGNORECASE | re.MULTILINE):
        return True
    name = command.command
    if name.startswith("soakstep"):
        return bool(parse_compact_soak(complete_tokens))
    if name.startswith(("stress", "soakstep", "flushstress", "burst")):
        counters = workload_counters(name, complete_tokens)
        return bool(
            re.search(r"\bResults:", clean)
            and "successes" in counters and "failures" in counters
        )
    if name == "selftest" or name.split()[0] == "demo":
        counters = workload_counters(name, complete_tokens)
        return "pass" in counters and "fail" in counters
    if name == "scan":
        return "Scan complete" in clean
    if name == "probe":
        return (len(status_record_lines(command, complete_tokens)) == 1 and status_codes == ["OK"]
                and operation_health_complete(name, complete_tokens))
    if name.startswith("monitor"):
        return bool(re.search(r"\b(?:Health monitor|Monitor):\s*(ON|OFF)\b", clean, re.IGNORECASE))
    if name == "recover":
        return (len(status_record_lines(command, complete_tokens)) == 1 and status_codes == ["OK"]
                and operation_health_complete(name, complete_tokens))
    return bool(re.search(r"\bOK\b|\bStatus:\s*OK\b", complete_tokens, re.IGNORECASE))


def read_until_ready(ser, timeout_s: float, idle_gap_s: float,
                     command: Optional[HilCommand] = None, *,
                     write_log=None, require_startup: bool = False) -> Tuple[str, str]:
    deadline = time.monotonic() + timeout_s
    last_data_at = time.monotonic()
    data_seen = False
    completion_seen = command is None and not require_startup
    chunks: List[str] = []
    received = 0

    while time.monotonic() < deadline:
        try:
            pending = getattr(ser, "in_waiting", 0)
            data = ser.read(min(pending or 1, 4096))
        except (OSError, RuntimeError) as exc:
            return "".join(chunks), f"serial-error: {type(exc).__name__}: {exc}"
        if data:
            text = decode(data)
            chunks.append(text)
            received += len(data)
            if write_log is not None:
                write_log(text)
            if received > MAX_RESPONSE_BYTES:
                return "".join(chunks), "capture-limit"
            data_seen = True
            last_data_at = time.monotonic()
            joined = "".join(chunks)
            if command is not None and response_has_completion(command, joined):
                completion_seen = True
            if require_startup and ("Ready for commands!" in joined or "Type 'help' for commands." in joined):
                completion_seen = True
            continue
        if data_seen and completion_seen and (time.monotonic() - last_data_at) >= idle_gap_s:
            joined = "".join(chunks)
            return joined, "prompt" if PROMPT_RE.search(joined) else "serial-idle"

    return "".join(chunks), "timeout"


def startup_error(text: str, expectations: Expectations) -> Optional[str]:
    """Check the original supported example startup, before any CLI command."""
    clean = strip_ansi(text)
    arduino = "=== SSD1315 01_basic_bringup_cli ==="
    idf = "=== SSD1315 native ESP-IDF bringup ==="
    headers = [header for header in (arduino, idf) if header in clean]
    if len(headers) != 1 or clean.count(headers[0]) != 1:
        return "missing or repeated original startup banner"
    application = clean.split(headers[0], 1)[1]
    if clean.count("ESP-ROM:") > 1 or clean.count("rst:") > 1 or has_failure_token(application):
        return "startup failure, crash or reset observed"
    if re.search(r"Guru Meditation|assert failed|abort\(\)|Backtrace:|\[E\]", clean):
        return "startup error/crash observed"
    if headers[0] == arduino:
        if not one_record(application, "begin() returned:", r"begin\(\) returned: OK \(code=0\)"):
            return "successful original begin result missing or repeated"
        if response_lines(application)[-1:] != ["Ready for commands!"]:
            return "original Arduino startup tail is incomplete"
        snapshot = parse_health(application)
        if (not snapshot or snapshot["driver_state"] != "READY"
                or snapshot["online"] not in ("yes", "true")
                or snapshot.get("control_dirty") not in ("no", "false")
                or int(snapshot["consecutive"]) != 0 or int(snapshot["total_failures"]) != 0):
            return "original startup health is incomplete or unhealthy"
        dimensions = one_record(application, "Dimensions:", r"Dimensions:\s+(\d+)x(\d+)")
        address = one_record(application, "I2C Address:", r"I2C Address:\s+0x([0-9A-Fa-f]{2})")
        if not dimensions or not address:
            return "original display configuration is incomplete"
        if ((expectations.width is not None and int(dimensions.group(1)) != expectations.width)
                or (expectations.height is not None and int(dimensions.group(2)) != expectations.height)
                or (expectations.address is not None and int(address.group(1), 16) != expectations.address)):
            return "original display configuration differs from expectations"
    else:
        # Native IDF startup prints begin, four panel commands and two flushes.
        statuses = parse_status_codes(HilCommand("startup"), application)
        if statuses != ["OK"] * 7:
            return "native IDF startup operations are incomplete or failed"
        if not re.search(r"Type 'help' for commands\.\s*\n> \s*\Z", application):
            return "original IDF startup tail is incomplete"
    return None


def prompt_operator(command: HilCommand) -> Tuple[str, str]:
    print(f"Visual check required for `{command.command}`.")
    print("Enter p=pass, f=fail, s=skip, u=unknown. Add optional notes after a space.")
    try:
        answer = input("visual> ").strip()
    except EOFError:
        return "UNKNOWN", "no operator input"
    if not answer:
        return "UNKNOWN", ""
    code, _, notes = answer.partition(" ")
    code = code.lower()
    if code == "p":
        return "PASS", notes.strip()
    if code == "f":
        return "FAIL", notes.strip()
    if code == "s":
        return "SKIP", notes.strip()
    return "UNKNOWN", notes.strip()


def build_metadata(args: argparse.Namespace, log_dir: Path) -> Dict[str, object]:
    status = git_value("status", "--short", allow_empty=True)
    worktree = "unknown" if status == "unknown" else ("clean" if not status else "dirty")
    return {
        "tool": "run_ssd1315_hil.py",
        "tool_version": SCRIPT_VERSION,
        "argv": list(sys.argv),
        "started": datetime.now().isoformat(timespec="seconds"),
        "mode": args.mode,
        "port": args.port,
        "baud": args.baud,
        "base_timeout_s": args.timeout,
        "startup_wait_s": args.startup_wait,
        "require_startup": args.require_startup,
        "idle_gap_s": args.idle_gap,
        "soak_duration_s": args.soak_duration_s,
        "soak_ops": args.soak_ops,
        "reconnect_attempts": args.reconnect_attempts,
        "soak_read_retries": args.soak_read_retries,
        "soak_read_retry_delay_s": args.soak_read_retry_delay_s,
        "expected_address": args.expect_address,
        "expected_width": args.expect_width,
        "expected_height": args.expect_height,
        "expected_controller": args.expect_controller,
        "expected_panel_profile": args.expect_panel_profile,
        "expected_commit": args.expect_commit,
        "expected_chip_model": args.expect_chip_model,
        "expected_arduino_core": args.expect_arduino_core,
        "expected_esp_idf": args.expect_esp_idf,
        "expected_flash_bytes": args.expect_flash_bytes,
        "expected_psram_bytes": args.expect_psram_bytes,
        "log_dir": str(log_dir),
        "operator": args.operator,
        "board": args.board,
        "panel": args.panel,
        "supply_voltage": args.supply_voltage,
        "pullups": args.pullups,
        "reset_wired": args.reset_wired,
        "bus_speed": args.bus_speed,
        "host_branch": git_value("branch", "--show-current"),
        "host_commit": git_value("rev-parse", "HEAD"),
        "host_worktree": worktree,
        "host_status_short": status,
    }


def strict_metadata_missing(args: argparse.Namespace) -> List[str]:
    missing = []
    for attr in ("operator", "board", "panel", "supply_voltage", "pullups", "reset_wired", "bus_speed"):
        if getattr(args, attr) in (None, "", "unknown"):
            missing.append(attr.replace("_", "-"))
    return missing


def open_serial(serial, args: argparse.Namespace):
    attempts = max(1, int(args.reconnect_attempts) + 1)
    last_exc: Optional[BaseException] = None
    for attempt in range(attempts):
        ser = None
        try:
            ser = serial.Serial(port=None, baudrate=args.baud, timeout=0.1,
                                write_timeout=2)
            ser.dtr = False
            ser.rts = False
            ser.port = args.port
            ser.open()
            return ser
        except Exception as exc:  # pyserial raises SerialException/OSError variants.
            last_exc = exc
            if ser is not None:
                try:
                    ser.close()
                except Exception:
                    pass
            if attempt + 1 >= attempts:
                break
            time.sleep(max(0.0, args.reconnect_delay_s))
    if last_exc is not None:
        raise last_exc
    raise RuntimeError("serial open failed without an exception")


def result_counts(results: List[CommandResult]) -> Dict[str, Dict[str, int]]:
    serial_counts: Dict[str, int] = {}
    operator_counts: Dict[str, int] = {}
    for result in results:
        serial_counts[result.serial_result] = serial_counts.get(result.serial_result, 0) + 1
        operator_counts[result.operator_result] = operator_counts.get(result.operator_result, 0) + 1
    return {"serial": serial_counts, "operator": operator_counts}


def latency_stats(results: List[CommandResult]) -> Dict[str, float]:
    if not results:
        return {"min_s": 0.0, "mean_s": 0.0, "max_s": 0.0, "sum_s": 0.0}
    elapsed = [result.elapsed_s for result in results]
    return {
        "min_s": min(elapsed),
        "mean_s": sum(elapsed) / len(elapsed),
        "max_s": max(elapsed),
        "sum_s": sum(elapsed),
    }


def should_stop_after_result(args: argparse.Namespace, result: CommandResult) -> bool:
    if args.continue_on_fail:
        return False
    if result.serial_result == "FAIL" or "REVIEW" in result.serial_result:
        return True
    # A timed-out response has no trustworthy transaction boundary.  Even if
    # a partial echo or payload was received, sending the next command could
    # misattribute a late completion record to that next transaction.  Safe
    # read-only retries are exhausted before a CommandResult reaches here.
    if result.wait_reason == "timeout":
        return True
    return False


def soak_retry_safe_read(command: HilCommand) -> bool:
    """Return true only for commands that are safe to repeat after lost output."""
    return command.command in ("version", "telemetry", "cfg")


def duration_soak_batch(
    commands: Sequence[HilCommand], cycle: int, cleanup: bool
) -> Tuple[HilCommand, ...]:
    """Select one-time prologue, repeated body, or final cleanup commands."""
    if cleanup:
        return tuple(item for item in commands if item.duration_phase == "epilogue")
    body = tuple(item for item in commands if item.duration_phase == "cycle")
    if cycle == 1:
        prologue = tuple(
            item for item in commands if item.duration_phase == "prologue"
        )
        return prologue + body
    return body


def duration_soak_cleanup_reason(
    cycle: int, deadline_reached: bool, max_cycles: int
) -> str:
    """Choose a final-cleanup reason only after at least one complete cycle."""
    if cycle <= 0:
        return ""
    if deadline_reached:
        return "duration_deadline_after_cycle_cleanup"
    if max_cycles > 0 and cycle >= max_cycles:
        return "soak_max_cycles_before_duration_final_cleanup"
    return ""


def should_retry_read_after_interruption(
    command: HilCommand,
    duration_soak: bool,
    retries_used: int,
    retry_limit: int,
    wait_reason: str,
    response: str,
    expectations: Optional[Expectations] = None,
) -> bool:
    """Retry incomplete duration-soak reads only without observed failure evidence."""
    if not (
        duration_soak and
        soak_retry_safe_read(command) and
        retries_used < max(0, retry_limit) and
        wait_reason == "timeout" and
        not has_failure_token(strip_ansi(response))
    ):
        return False
    # A partial snapshot can already prove failure. Do not replace that
    # evidence with a later successful read, even when its tail timed out.
    clean = re.sub(r"\S+\Z", "", strip_ansi(response))
    if any(code not in ("OK", "IN_PROGRESS") for code in parse_status_codes(command, clean)):
        return False
    if command.command == "telemetry":
        fields = {
            "freeHeap": r"0*[1-9]\d*", "minFreeHeap": r"0*[1-9]\d*",
            "driverState": "READY", "online": "true", "controlDirty": "false",
        }
    elif command.command == "cfg":
        fields = {
            field: (r"yes|true" if key == "initialized" else
                    r"no|false|0x0{1,2}" if key == "dirty" else r"no|false")
            for key, field in CFG_STATE_FIELDS.items()
        } if command.require_clean_cfg else {
            field: r"yes|true|no|false|0x[0-9a-f]{1,2}" if key == "dirty" else r"yes|true|no|false"
            for key, field in CFG_STATE_FIELDS.items()
        }
    else:
        fields = {}
    for field, allowed in fields.items():
        values = re.findall(rf"(?<!\S){field}=([^\s]*)", clean, re.IGNORECASE)
        if values and (len(values) != 1 or not re.fullmatch(allowed, values[0], re.IGNORECASE)):
            return False
    if command.command == "version" and clean.strip() and classify_serial(command, clean)[0] == "FAIL":
        return False
    parsed = parse_cfg(clean) if command.command == "cfg" else parse_version_info(clean)
    if check_expectations(parsed, expectations or Expectations(),
                          check_commit=command.command == "version", observed_only=True):
        return False
    return True


def run_commands(args: argparse.Namespace, commands: Sequence[HilCommand],
                 expectations: Expectations) -> Tuple[Path, List[CommandResult], Dict[str, object]]:
    serial = _load_serial_module()
    if not args.port:
        raise SystemExit("--port is required unless --dry-run is used")

    missing = strict_metadata_missing(args)
    if args.strict and missing:
        raise SystemExit("--strict requires metadata: " + ", ".join(missing))

    log_dir = make_log_dir(Path(args.out))
    log_dir.mkdir(parents=True, exist_ok=False)
    metadata = build_metadata(args, log_dir)
    transcript_path = log_dir / "serial_transcript.txt"
    results: List[CommandResult] = []
    run_started = time.monotonic()
    read_retry_count = 0
    serial_interruptions: List[Dict[str, object]] = []
    transcript = transcript_path.open("w", encoding="utf-8", newline="\n")
    try:
        ser = open_serial(serial, args)
    except Exception:
        transcript.close()
        raise

    try:
        transcript.write("# SSD1315 HIL serial transcript\n")
        transcript.write(f"# mode={args.mode} port={args.port} baud={args.baud} timeout={args.timeout}\n")
        transcript.write(f"# started={metadata['started']}\n\n")
        def live_log(text: str) -> None:
            transcript.write(text)
            transcript.flush()
            if args.verbose:
                print(text, end="", flush=True)
        time.sleep(args.startup_wait)
        transcript.write("## Initial serial output\n"); transcript.flush()
        initial, initial_wait = read_until_ready(
            ser, args.timeout if args.require_startup else min(args.timeout, 3.0), args.idle_gap,
            write_log=live_log, require_startup=args.require_startup)
        initial_error = ""
        if initial_wait.startswith("serial-error") or initial_wait == "capture-limit":
            initial_error = initial_wait
        elif args.require_startup or "=== SSD1315 " in strip_ansi(initial):
            initial_error = startup_error(initial, expectations) or ""
            if initial_wait == "timeout":
                initial_error = initial_error or "original startup capture timed out"
        elif CRASH_RE.search(strip_ansi(initial)):
            initial_error = "unqualified reset/crash in initial capture"
        metadata["startup_validated"] = not initial_error and (args.require_startup or "=== SSD1315 " in initial)
        if initial_error:
            results.append(CommandResult("startup", "FAIL", "N/A", initial_wait, 0.0,
                                         initial_error, initial[-2000:], strip_ansi(initial)[-2000:]))
            live_log("\n[HOST STARTUP FAILURE] " + initial_error + "\n")

        duration_deadline = None
        duration_started_at = None
        if args.mode == "soak" and args.soak_duration_s > 0:
            duration_started_at = time.monotonic()
            duration_deadline = duration_started_at + args.soak_duration_s

        stop_requested = bool(initial_error)
        stop_reason = "startup_failure" if initial_error else "single_cycle_complete"
        cycle = 0
        while not stop_requested:
            cleanup_reason = duration_soak_cleanup_reason(
                cycle,
                duration_deadline is not None and time.monotonic() >= duration_deadline,
                int(args.soak_max_cycles) if duration_deadline is not None else 0,
            )
            duration_cleanup = bool(cleanup_reason)
            if duration_cleanup:
                stop_reason = cleanup_reason
                active_commands = duration_soak_batch(commands, cycle, True)
                transcript.write("\n## Soak final cleanup\n")
                if args.verbose:
                    print("Starting soak final cleanup")
            else:
                cycle += 1
                active_commands = commands
                if duration_deadline is not None:
                    active_commands = duration_soak_batch(commands, cycle, False)
                    transcript.write(f"\n## Soak cycle {cycle}\n")
                    if args.verbose:
                        print(f"Starting soak cycle {cycle}")

            for item in active_commands:
                if item.risky_visual:
                    print(f"Warning: `{item.command}` may show static/high-contrast OLED content briefly.")
                per_command_timeout = max(0.5, args.timeout * item.timeout_scale)
                command_started = time.monotonic()
                recovered_interruptions = 0
                while True:
                    transcript.write(f"\n# command_at={datetime.now().astimezone().isoformat()}\n>>> {item.command}\n")
                    transcript.flush()
                    try:
                        payload = (item.command + "\n").encode("utf-8")
                        if ser.write(payload) != len(payload):
                            raise RuntimeError("short serial command write")
                        response, wait_reason = read_until_ready(
                            ser, per_command_timeout, args.idle_gap, item, write_log=live_log
                        )
                        serial_error = wait_reason if wait_reason.startswith("serial-error") else ""
                    except Exception as exc:  # SerialException/OSError variants.
                        response = ""
                        wait_reason = "serial-error"
                        serial_error = f"{type(exc).__name__}: {exc}"

                    if response and not response.endswith("\n"):
                        transcript.write("\n")
                    if serial_error:
                        transcript.write(f"[HOST SERIAL ERROR] {serial_error}\n")
                    transcript.flush()
                    serial_result, note, parsed = classify_serial(
                        item, response, expectations
                    )
                    if serial_error:
                        serial_result = "FAIL"
                        note = "host serial I/O exception"
                    elif wait_reason == "capture-limit":
                        serial_result = "FAIL"
                        note = "bounded serial capture size exceeded"
                    elif (wait_reason == "timeout" and
                          serial_result in ("PASS", "SERIAL_PASS_OPERATOR_REQUIRED")):
                        serial_result = "REVIEW_REQUIRED"
                        note = "success token found, but command wait timed out"

                    recoverable_interruption = should_retry_read_after_interruption(
                        item,
                        duration_deadline is not None,
                        read_retry_count,
                        int(args.soak_read_retries),
                        wait_reason,
                        response,
                        expectations,
                    )
                    if not recoverable_interruption:
                        break

                    read_retry_count += 1
                    recovered_interruptions += 1
                    serial_interruptions.append({
                        "cycle": cycle,
                        "command": item.command,
                        "wait_reason": wait_reason,
                        "serial_result": serial_result,
                        "elapsed_s": time.monotonic() - command_started,
                        "clean_excerpt": strip_ansi(response)[-1000:],
                    })
                    transcript.write(
                        f"[HOST READ RETRY] read-only retry {read_retry_count}/"
                        f"{args.soak_read_retries} after {wait_reason}\n"
                    )
                    transcript.flush()
                    try:
                        ser.reset_input_buffer()
                    except Exception as exc:
                        response = ""
                        wait_reason = "serial-error"
                        serial_result = "FAIL"
                        parsed = {}
                        note = f"host serial input reset failed: {type(exc).__name__}: {exc}"
                        transcript.write(f"[HOST READ RETRY FAILED] {note}\n")
                        transcript.flush()
                        break
                    time.sleep(max(0.0, args.soak_read_retry_delay_s))

                elapsed = time.monotonic() - command_started
                if recovered_interruptions > 0 and serial_result in (
                    "PASS", "SERIAL_PASS_OPERATOR_REQUIRED"
                ):
                    note = (
                        f"recovered {recovered_interruptions} read-only serial "
                        f"interruption(s); {note}"
                    )

                operator_result = "N/A"
                operator_notes = ""
                if item.visual_check:
                    if args.serial_only:
                        operator_result = "SKIPPED_SERIAL_ONLY"
                    elif args.interactive_visual:
                        operator_result, operator_notes = prompt_operator(item)
                    else:
                        operator_result = "OPERATOR_REQUIRED"

                if item.note:
                    note = f"{note}; {item.note}"
                if duration_deadline is not None:
                    phase = (
                        "soak final cleanup" if duration_cleanup
                        else f"soak cycle {cycle}"
                    )
                    note = f"{phase}; {note}"
                result = CommandResult(
                    command=item.command,
                    serial_result=serial_result,
                    operator_result=operator_result,
                    wait_reason=wait_reason,
                    elapsed_s=elapsed,
                    note=note,
                    raw_excerpt=response[-2000:],
                    clean_excerpt=strip_ansi(response)[-2000:],
                    parsed=parsed,
                    operator_notes=operator_notes,
                )
                results.append(result)
                if should_stop_after_result(args, result):
                    stop_reason = "command_failure_or_timeout"
                    stop_requested = True
                    break

            if duration_deadline is None:
                break
            if duration_cleanup:
                break
    finally:
        transcript.close()
        try:
            ser.close()
        except Exception:
            pass

    metadata["completed"] = datetime.now().isoformat(timespec="seconds")
    metadata["elapsed_s"] = time.monotonic() - run_started
    metadata["command_count"] = len(results)
    metadata["result_counts"] = result_counts(results)
    metadata["latency_stats"] = latency_stats(results)
    metadata["stop_reason"] = stop_reason
    metadata["serial_read_retry_count"] = read_retry_count
    metadata["serial_interruptions"] = serial_interruptions
    if duration_deadline is not None:
        metadata["soak_cycles_started"] = cycle
        metadata["soak_elapsed_s"] = (
            time.monotonic() - duration_started_at
            if duration_started_at is not None else 0.0
        )

    write_artifacts(log_dir, args, commands, results, metadata)
    return log_dir, results, metadata


def telemetry_health(results: List[CommandResult]) -> Dict[str, object]:
    samples = [result.parsed for result in results
               if result.command == "telemetry" and result.parsed]
    problems: List[str] = []
    for index in range(1, len(samples)):
        previous = samples[index - 1]
        current = samples[index]
        for key, label in (("uptime_ms", "uptime"),
                           ("loop_heartbeat", "loop heartbeat")):
            before = previous.get(key)
            after = current.get(key)
            if isinstance(before, int) and isinstance(after, int) and after <= before:
                problems.append(
                    f"{label} did not increase between telemetry samples "
                    f"{index} and {index + 1}"
                )
        before_reset = previous.get("reset_reason_code")
        after_reset = current.get("reset_reason_code")
        if (isinstance(before_reset, int) and isinstance(after_reset, int) and
                after_reset != before_reset):
            problems.append(
                f"reset reason changed between telemetry samples {index} and {index + 1}"
            )

    initial = samples[0] if samples else {}
    final = samples[-1] if samples else {}
    deltas: Dict[str, int] = {}
    for key in ("uptime_ms", "loop_heartbeat", "free_heap", "min_free_heap"):
        before = initial.get(key)
        after = final.get(key)
        if isinstance(before, int) and isinstance(after, int):
            deltas[key] = after - before
    return {
        "sample_count": len(samples),
        "initial": initial,
        "final": final,
        "deltas": deltas,
        "problems": problems,
        "pass": not problems,
    }


def verdicts_for(mode: str, results: List[CommandResult],
                 metadata: Optional[Dict[str, object]] = None) -> Dict[str, object]:
    metadata = metadata or {}
    serial_fail = any(result.serial_result == "FAIL" for result in results)
    serial_review = any("REVIEW" in result.serial_result for result in results)
    health = telemetry_health(results)
    visual_results = [r for r in results if r.operator_result not in ("N/A",)]
    visual_complete = bool(visual_results) and all(
        r.operator_result in ("PASS", "FAIL") for r in visual_results
    )
    visual_fail = any(r.operator_result == "FAIL" for r in visual_results)
    retention_run = mode in ("retention", "all")
    soak_run = mode in ("soak", "all")
    soak_final_cleanup_complete = (
        soak_run and bool(results) and results[-1].command == "cfg" and
        results[-1].serial_result == "PASS"
    )
    duration_target_s = float(metadata.get("soak_duration_s", 0.0) or 0.0)
    duration_elapsed_s = float(metadata.get("soak_elapsed_s", 0.0) or 0.0)
    soak_duration_met = duration_target_s <= 0.0 or duration_elapsed_s >= duration_target_s
    return {
        "serial_device_pass": not serial_fail and not serial_review and health["pass"],
        "serial_review_required": serial_review,
        "telemetry_health_pass": health["pass"],
        "visual_complete": visual_complete,
        "visual_pass": visual_complete and not visual_fail,
        "retention_isolation_complete": retention_run and visual_complete,
        "soak_final_cleanup_complete": soak_final_cleanup_complete,
        "soak_duration_met": soak_run and soak_duration_met,
        "soak_complete": (soak_run and not serial_fail and not serial_review and
                          health["pass"] and soak_duration_met and
                          soak_final_cleanup_complete),
        "field_ready_claim_allowed": False,
    }


def run_exit_pass(mode: str, verdicts: Dict[str, object]) -> bool:
    """Require serial integrity and, for soak plans, complete soak evidence."""
    if not bool(verdicts.get("serial_device_pass")):
        return False
    if mode in ("soak", "all"):
        return bool(verdicts.get("soak_complete"))
    return True


def first_last_cfg(results: List[CommandResult]) -> Tuple[Dict[str, object], Dict[str, object]]:
    cfgs = [result.parsed for result in results if result.command == "cfg" and result.parsed]
    return (cfgs[0] if cfgs else {}, cfgs[-1] if cfgs else {})


def first_version(results: List[CommandResult]) -> Dict[str, object]:
    for result in results:
        if result.command in ("version", "ver") and result.parsed:
            return result.parsed
    return {}


def write_artifacts(log_dir: Path, args: argparse.Namespace, commands: Sequence[HilCommand],
                    results: List[CommandResult], metadata: Dict[str, object]) -> None:
    verdicts = verdicts_for(args.mode, results, metadata)
    initial_cfg, final_cfg = first_last_cfg(results)
    health = telemetry_health(results)

    (log_dir / "metadata.json").write_text(
        json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    (log_dir / "command_plan.json").write_text(
        json.dumps([asdict(command) for command in commands], indent=2) + "\n", encoding="utf-8"
    )
    (log_dir / "results.json").write_text(
        json.dumps({
            "metadata": metadata,
            "verdicts": verdicts,
            "results": [asdict(result) for result in results],
        }, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    (log_dir / "parsed_cfg_initial.json").write_text(
        json.dumps(initial_cfg, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    (log_dir / "parsed_cfg_final.json").write_text(
        json.dumps(final_cfg, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    (log_dir / "parsed_version.json").write_text(
        json.dumps(first_version(results), indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    (log_dir / "health_delta.json").write_text(
        json.dumps({
            "initial_cfg": initial_cfg,
            "final_cfg": final_cfg,
            "telemetry": health,
        }, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    (log_dir / "run_stats.json").write_text(
        json.dumps({
            "elapsed_s": metadata.get("elapsed_s", 0.0),
            "soak_elapsed_s": metadata.get("soak_elapsed_s", 0.0),
            "soak_duration_s": metadata.get("soak_duration_s", 0.0),
            "command_count": len(results),
            "counts": result_counts(results),
            "latency": latency_stats(results),
            "serial_read_retry_count": metadata.get("serial_read_retry_count", 0),
            "serial_interruptions": metadata.get("serial_interruptions", []),
        }, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    write_results_csv(log_dir / "results.csv", results)
    write_summary(log_dir, args, results, metadata, verdicts)
    write_visual_checklist(log_dir, results)
    write_matrix_fragment(log_dir, metadata, results, initial_cfg, final_cfg)
    write_failure_analysis(log_dir, results, verdicts)


def write_results_csv(path: Path, results: List[CommandResult]) -> None:
    with path.open("w", encoding="utf-8", newline="") as out:
        writer = csv.writer(out)
        writer.writerow(["command", "serial_result", "operator_result", "wait", "seconds", "note", "operator_notes"])
        for result in results:
            writer.writerow([
                result.command,
                result.serial_result,
                result.operator_result,
                result.wait_reason,
                f"{result.elapsed_s:.2f}",
                result.note,
                result.operator_notes,
            ])


def write_summary(log_dir: Path, args: argparse.Namespace, results: List[CommandResult],
                  metadata: Dict[str, object], verdicts: Dict[str, object]) -> None:
    identity = first_version(results)
    with (log_dir / "summary.md").open("w", encoding="utf-8", newline="\n") as summary:
        summary.write("# SSD1315 HIL Device Test Summary\n\n")
        summary.write(f"- Tool version: `{SCRIPT_VERSION}`\n")
        summary.write(f"- Mode: `{args.mode}`\n")
        summary.write(f"- Port: `{args.port}`\n")
        summary.write(f"- Baud: `{args.baud}`\n")
        summary.write(f"- Transcript: `serial_transcript.txt`\n")
        summary.write(f"- Host branch: `{metadata['host_branch']}`\n")
        summary.write(f"- Host commit: `{metadata['host_commit']}`\n")
        summary.write(f"- Worktree: `{metadata['host_worktree']}`\n\n")
        summary.write("## Firmware Identity\n\n")
        for label, key in (
            ("Framework", "framework"),
            ("Chip model", "chip_model"),
            ("Chip revision", "chip_revision"),
            ("Arduino-ESP32", "arduino_core_version"),
            ("ESP-IDF", "esp_idf_version"),
            ("Flash bytes", "flash_bytes"),
            ("PSRAM ready", "psram_ready"),
            ("PSRAM bytes", "psram_bytes"),
            ("SSD1315 version", "library_version"),
            ("Firmware commit", "firmware_commit"),
        ):
            summary.write(f"- {label}: `{identity.get(key, 'unknown')}`\n")
        summary.write("\n")
        summary.write("## Run Stats\n\n")
        summary.write(f"- Elapsed seconds: `{metadata.get('elapsed_s', 0.0):.2f}`\n")
        summary.write(f"- Command count: `{len(results)}`\n")
        if metadata.get("stop_reason"):
            summary.write(f"- Stop reason: `{metadata['stop_reason']}`\n")
        if metadata.get("soak_cycles_started") is not None:
            summary.write(f"- Soak cycles started: `{metadata['soak_cycles_started']}`\n")
        summary.write(
            f"- Read-only serial retry attempts: "
            f"`{metadata.get('serial_read_retry_count', 0)}`\n"
        )
        summary.write(f"- Serial counts: `{result_counts(results)['serial']}`\n")
        latency = latency_stats(results)
        summary.write(
            f"- Command latency seconds: min=`{latency['min_s']:.2f}` "
            f"mean=`{latency['mean_s']:.2f}` max=`{latency['max_s']:.2f}`\n\n"
        )
        summary.write("## Verdicts\n\n")
        for key, value in verdicts.items():
            summary.write(f"- {key}: `{value}`\n")
        summary.write("\nField-ready evidence is always `false` unless serial, visual, retention, fault, and soak evidence are complete.\n\n")
        summary.write("## Commands\n\n")
        summary.write("| Command | Serial Result | Operator Result | Wait | Seconds | Notes |\n")
        summary.write("| --- | --- | --- | --- | ---: | --- |\n")
        for result in results:
            summary.write(
                f"| `{result.command}` | {result.serial_result} | {result.operator_result} | "
                f"{result.wait_reason} | {result.elapsed_s:.2f} | {result.note} {result.operator_notes} |\n"
            )
        summary.write("\n## OLED Retention Warning\n\n")
        summary.write("OLED panels can retain static content or age unevenly. Do not treat a serial PASS as a visual PASS. ")
        summary.write("If ghosting appears, record clear/display-off/power-cycle observations and photos/video.\n")


def write_visual_checklist(log_dir: Path, results: List[CommandResult]) -> None:
    with (log_dir / "operator_visual_checklist.md").open("w", encoding="utf-8", newline="\n") as out:
        out.write("# Operator Visual Checklist\n\n")
        for result in results:
            if result.operator_result != "N/A":
                out.write(f"- [ ] `{result.command}`: result={result.operator_result}; notes={result.operator_notes}\n")
        out.write("\nAttach photos/video for checkerboard, clear, fill, contrast, flip, and scroll checks.\n")


def write_matrix_fragment(log_dir: Path, metadata: Dict[str, object], results: List[CommandResult],
                          initial_cfg: Dict[str, object], final_cfg: Dict[str, object]) -> None:
    def format_address(value: object) -> object:
        return f"0x{value:02X}" if isinstance(value, int) else value

    identity = first_version(results)
    with (log_dir / "hardware_matrix_fragment.md").open("w", encoding="utf-8", newline="\n") as out:
        out.write("# SSD1315 Hardware Matrix Fragment\n\n")
        out.write("| Field | Result |\n| --- | --- |\n")
        out.write(f"| Branch | `{metadata['host_branch']}` |\n")
        out.write(f"| Commit hash | `{metadata['host_commit']}` |\n")
        out.write(f"| Worktree state | `{metadata['host_worktree']}` |\n")
        out.write(f"| Worktree status detail | `{metadata.get('host_status_short') or 'clean'}` |\n")
        out.write(f"| HIL log directory | `{metadata.get('log_dir', 'unknown')}` |\n")
        out.write(f"| Operator | `{metadata.get('operator') or 'unknown'}` |\n")
        out.write(f"| Serial port | `{metadata['port']}` |\n")
        out.write(f"| Baud rate | `{metadata['baud']}` |\n")
        out.write(f"| MCU board | `{metadata.get('board') or 'unknown'}` |\n")
        out.write(f"| Runtime chip | `{identity.get('chip_model', 'unknown')} revision {identity.get('chip_revision', 'unknown')}` |\n")
        out.write(f"| Arduino-ESP32 | `{identity.get('arduino_core_version', 'N/A')}` |\n")
        out.write(f"| ESP-IDF | `{identity.get('esp_idf_version', 'unknown')}` |\n")
        out.write(f"| Runtime flash bytes | `{identity.get('flash_bytes', 'unknown')}` |\n")
        out.write(f"| Runtime PSRAM | `ready={identity.get('psram_ready', 'unknown')} bytes={identity.get('psram_bytes', 'unknown')}` |\n")
        out.write(f"| Panel module model | `{metadata.get('panel') or 'unknown'}` |\n")
        out.write(f"| Supply voltage | `{metadata.get('supply_voltage') or 'unknown'}` |\n")
        out.write(f"| Pull-up values | `{metadata.get('pullups') or 'unknown'}` |\n")
        out.write(f"| Reset pin connected/not connected | `{metadata.get('reset_wired') or 'unknown'}` |\n")
        out.write(f"| Bus speed | `{metadata.get('bus_speed') or 'unknown'}` |\n")
        out.write(f"| Soak duration target | `{metadata.get('soak_duration_s', 0)}` seconds |\n")
        out.write(f"| I2C address | `{format_address(final_cfg.get('i2c_address', initial_cfg.get('i2c_address', 'OPERATOR_REQUIRED')))}` |\n")
        out.write(f"| Geometry | `{final_cfg.get('width', initial_cfg.get('width', 'unknown'))}x{final_cfg.get('height', initial_cfg.get('height', 'unknown'))}` |\n")
        out.write(f"| Panel profile | `{final_cfg.get('panel_profile', initial_cfg.get('panel_profile', 'unknown'))}` |\n")
        out.write("\n## Per-command serial results\n\n")
        out.write("| Command | Serial result | Visual result |\n| --- | --- | --- |\n")
        for result in results:
            visual = result.operator_result if result.operator_result != "N/A" else "N/A"
            out.write(f"| `{result.command}` | {result.serial_result} | {visual} |\n")


def write_failure_analysis(log_dir: Path, results: List[CommandResult], verdicts: Dict[str, object]) -> None:
    with (log_dir / "failure_analysis.md").open("w", encoding="utf-8", newline="\n") as out:
        out.write("# Failure Analysis\n\n")
        problem_results = [r for r in results if r.serial_result == "FAIL" or "REVIEW" in r.serial_result]
        if not problem_results:
            out.write("No serial failures or review-required command results were detected.\n")
        for result in problem_results:
            out.write(f"- `{result.command}`: {result.serial_result}; {result.note}\n")
        out.write("\n## Verdicts\n\n")
        for key, value in verdicts.items():
            out.write(f"- {key}: `{value}`\n")


def print_dry_run(args: argparse.Namespace, commands: Sequence[HilCommand]) -> None:
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    log_dir = Path(args.out) / f"ssd1315_{stamp}"
    print("SSD1315 HIL dry run")
    print(f"Tool version: {SCRIPT_VERSION}")
    print(f"Mode: {args.mode}")
    print(f"Port: {args.port or '<required for serial run>'}")
    print(f"Baud: {args.baud}")
    print(f"Base timeout: {args.timeout} seconds")
    print(f"Output directory: {log_dir}")
    for artifact in (
        "serial_transcript.txt", "summary.md", "results.json", "results.csv",
        "metadata.json", "operator_visual_checklist.md", "hardware_matrix_fragment.md",
        "parsed_version.json", "parsed_cfg_initial.json", "parsed_cfg_final.json", "health_delta.json",
        "failure_analysis.md", "command_plan.json", "run_stats.json",
    ):
        print(f"{artifact}: {log_dir / artifact}")
    print("\nCommand sequence:")
    for item in commands:
        marker = " [operator visual check]" if item.visual_check else ""
        risky = " [OLED-risky]" if item.risky_visual else ""
        print(f"  {item.command}{marker}{risky}")


def parse_args(argv: Optional[List[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("smoke", "functional", "retention", "soak", "all", "benchmark", "arduino-extended"),
                        default="functional", help="HIL command plan to run")
    parser.add_argument("--port", help="Serial port for validation firmware, for example COM5 or /dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD, help=f"Serial baud rate (default: {DEFAULT_BAUD})")
    parser.add_argument("--out", default=str(DEFAULT_OUT_ROOT), help="Root directory for timestamped HIL logs")
    parser.add_argument("--timeout", "--timeout-s", "--command-timeout", "--command-timeout-s",
                        dest="timeout", type=float, default=DEFAULT_TIMEOUT_S,
                        help=f"Base per-command timeout in seconds (default: {DEFAULT_TIMEOUT_S})")
    parser.add_argument("--startup-wait", "--boot-settle-s", "--reset-settle-s",
                        dest="startup_wait", type=float, default=1.0,
                        help="Seconds to wait after opening serial before sending commands")
    parser.add_argument("--require-startup", action="store_true",
                        help="Require and validate the original Arduino/IDF startup before any command")
    parser.add_argument("--idle-gap", "--idle-timeout-s", dest="idle_gap", type=float, default=0.35,
                        help="Treat command output as complete after this much serial silence")
    parser.add_argument("--dry-run", action="store_true",
                        help="Print command sequence and output paths without opening serial")
    parser.add_argument("--parser-self-test", action="store_true",
                        help="Run bounded parser/classifier self-tests and exit")
    parser.add_argument("--expect-address", default=None, help="Expected 7-bit OLED address, e.g. 0x3C, or any")
    parser.add_argument("--expect-width", type=int, default=None)
    parser.add_argument("--expect-height", type=int, default=None)
    parser.add_argument("--expect-controller", default="SSD1315")
    parser.add_argument("--expect-panel-profile", default=None)
    parser.add_argument("--expect-commit", default=None)
    parser.add_argument("--expect-chip-model", default=None)
    parser.add_argument("--expect-arduino-core", default=None)
    parser.add_argument("--expect-esp-idf", default=None)
    parser.add_argument("--expect-flash-bytes", type=int, default=None)
    parser.add_argument("--expect-psram-bytes", type=int, default=None)
    parser.add_argument("--operator", default="")
    parser.add_argument("--board", default="")
    parser.add_argument("--panel", default="")
    parser.add_argument("--supply-voltage", default="")
    parser.add_argument("--pullups", default="")
    parser.add_argument("--reset-wired", choices=("yes", "no", "unknown"), default="unknown")
    parser.add_argument("--bus-speed", default="")
    parser.add_argument("--interactive-visual", action="store_true")
    parser.add_argument("--serial-only", action="store_true")
    parser.add_argument("--soak-ops", type=int, default=1000)
    parser.add_argument("--soak-duration-s", type=float, default=0.0,
                        help="For --mode soak, repeat the soak command cycle until this bounded deadline")
    parser.add_argument("--soak-duration-hours", type=float, default=0.0,
                        help="For --mode soak, duration deadline in hours; overrides --soak-duration-s")
    parser.add_argument("--soak-max-cycles", type=int, default=0,
                        help="Optional additional cap for duration soak cycles; 0 means deadline only")
    parser.add_argument("--reconnect-attempts", type=int, default=0,
                        help="Bounded serial open retry count before the run starts")
    parser.add_argument("--soak-read-retries", type=int, default=0,
                        help="Bounded duration-soak same-handle retries for missing/truncated read-only output")
    parser.add_argument("--soak-read-retry-delay-s", type=float, default=1.0,
                        help="Delay between bounded duration-soak read retries")
    parser.add_argument("--reconnect-delay-s", type=float, default=1.0,
                        help="Delay between bounded serial open retries")
    parser.add_argument("--verbose", action="store_true",
                        help="Echo command responses to stdout while still writing transcripts")
    parser.add_argument("--continue-on-fail", action="store_true",
                        help="Continue after failed/incomplete results; default stops before the next transaction")
    parser.add_argument("--strict", action="store_true")
    parser.add_argument("--no-risky-visuals", action="store_true",
                        help="Skip fill/contrast255/static checker style visual commands")
    args = parser.parse_args(argv)
    finite_values = (
        args.timeout,
        args.startup_wait,
        args.idle_gap,
        args.soak_duration_s,
        args.soak_duration_hours,
        args.reconnect_delay_s,
        args.soak_read_retry_delay_s,
    )
    if not all(math.isfinite(value) for value in finite_values):
        parser.error("timeouts, durations, and delays must be finite")
    if args.baud <= 0:
        parser.error("--baud must be positive")
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    if args.startup_wait < 0 or args.idle_gap < 0:
        parser.error("serial wait intervals must be nonnegative")
    if args.soak_duration_s < 0 or args.soak_duration_hours < 0:
        parser.error("soak duration must be nonnegative")
    if args.soak_max_cycles < 0:
        parser.error("--soak-max-cycles must be nonnegative")
    if args.reconnect_attempts < 0 or args.soak_read_retries < 0:
        parser.error("retry counts must be nonnegative")
    if args.reconnect_delay_s < 0 or args.soak_read_retry_delay_s < 0:
        parser.error("retry delays must be nonnegative")
    if args.expect_flash_bytes is not None and args.expect_flash_bytes <= 0:
        parser.error("--expect-flash-bytes must be positive")
    if args.expect_psram_bytes is not None and args.expect_psram_bytes < 0:
        parser.error("--expect-psram-bytes must be nonnegative")
    if args.soak_duration_hours > 0:
        args.soak_duration_s = args.soak_duration_hours * 3600.0
        if not math.isfinite(args.soak_duration_s):
            parser.error("soak duration is too large")
    if args.soak_ops < 1 or args.soak_ops > 10000:
        parser.error("--soak-ops must be in 1..10000")
    return args


def expectations_from_args(args: argparse.Namespace) -> Expectations:
    return Expectations(
        address=parse_hex_or_any(args.expect_address),
        width=args.expect_width,
        height=args.expect_height,
        controller=parse_text_or_any(args.expect_controller),
        panel_profile=parse_text_or_any(args.expect_panel_profile),
        commit=parse_text_or_any(args.expect_commit),
        chip_model=parse_text_or_any(args.expect_chip_model),
        arduino_core=parse_text_or_any(args.expect_arduino_core),
        esp_idf=parse_text_or_any(args.expect_esp_idf),
        flash_bytes=args.expect_flash_bytes,
        psram_bytes=args.expect_psram_bytes,
        strict=args.strict,
    )


def main(argv: Optional[List[str]] = None) -> int:
    args = parse_args(argv)
    if args.parser_self_test:
        return parser_self_test()
    commands = command_plan(args.mode, args.soak_ops, args.no_risky_visuals)
    expectations = expectations_from_args(args)

    if args.dry_run:
        print_dry_run(args, commands)
        return 0

    try:
        log_dir, results, metadata = run_commands(args, commands, expectations)
    except FileExistsError as exc:
        print(str(exc), file=sys.stderr)
        return 2

    verdicts = verdicts_for(args.mode, results, metadata)
    print(f"HIL logs written to: {log_dir}")
    print("Device tester verdicts:")
    print(f"  Serial device test: {'PASS' if verdicts['serial_device_pass'] else 'FAIL'}")
    print(f"  Visual operator checks: {'COMPLETE' if verdicts['visual_complete'] else 'INCOMPLETE'}")
    print(f"  Retention isolation: {'COMPLETE' if verdicts['retention_isolation_complete'] else ('NOT_RUN' if args.mode not in ('retention', 'all') else 'INCOMPLETE')}")
    print(f"  Soak: {'COMPLETE' if verdicts['soak_complete'] else ('NOT_RUN' if args.mode not in ('soak', 'all') else 'INCOMPLETE')}")
    print("  Field-ready evidence: NO")
    return 0 if run_exit_pass(args.mode, verdicts) else 1


def parser_self_test() -> int:
    checks = (
        (HilCommand("version"),
         "SSD1315 library version: 2.0.0\nController profile: SSD1315\nActive I2C address: 0x3C\nGeometry: 128x64 pages=8 pageBufferPages=8",
         "PASS"),
        (HilCommand("cfg"),
         "Config:\ncontrollerProfile=SSD1315 panelProfile=x addr=0x3C geometry=128x64\ninitialized=yes dirty=no flushing=no controlDirty=no scrollActive=no",
         "PASS"),
        (HilCommand("stress_mix 10"),
         "Results:\n  Total ops: 10\n  Successes: 10\n  Failures: 0\n",
         "PASS"),
        (HilCommand("probe"),
         "Status: I2C_TIMEOUT\n",
         "FAIL"),
    )
    for command, text, expected in checks:
        observed, reason, _ = classify_serial(command, text, Expectations(address=0x3C, width=128, height=64))
        if observed != expected:
            print(
                f"parser self-test failed for `{command.command}`: expected {expected}, "
                f"observed {observed}: {reason}",
                file=sys.stderr,
            )
            return 1
    if git_value("status", "--short", allow_empty=True) == "unknown":
        print("parser self-test failed: git status helper cannot distinguish clean from unknown", file=sys.stderr)
        return 1
    print("parser self-test passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
