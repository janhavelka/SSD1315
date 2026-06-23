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
import re
import subprocess
import sys
import time
from dataclasses import asdict, dataclass, field
from datetime import datetime
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Tuple


SCRIPT_VERSION = "2.0"
DEFAULT_BAUD = 115200
DEFAULT_TIMEOUT_S = 8.0
DEFAULT_OUT_ROOT = Path("hil_logs")
PROMPT_RE = re.compile(r"(^|\r?\n)> $")
ANSI_RE = re.compile(r"\x1B\[[0-?]*[ -/]*[@-~]")


@dataclass(frozen=True)
class HilCommand:
    command: str
    visual_check: bool = False
    timeout_scale: float = 1.0
    note: str = ""
    risky_visual: bool = False
    require_clean_cfg: bool = True


@dataclass
class Expectations:
    address: Optional[int] = None
    width: Optional[int] = None
    height: Optional[int] = None
    controller: Optional[str] = None
    panel_profile: Optional[str] = None
    commit: Optional[str] = None
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
    HilCommand("flipx 1", visual_check=True),
    HilCommand("flipx 0", visual_check=True),
    HilCommand("flipy 1", visual_check=True),
    HilCommand("flipy 0", visual_check=True),
    HilCommand("scrollh right 0 7", visual_check=True),
    HilCommand("scrollv left 0 7 1", visual_check=True),
    HilCommand("scroll stop", visual_check=True),
    HilCommand("recover", visual_check=True, timeout_scale=2.0,
               note="Recover is software-only; redraw/flush behavior requires observation."),
    HilCommand("stress 100", visual_check=True, timeout_scale=3.0),
    HilCommand("stress_mix 100", visual_check=True, timeout_scale=4.0),
    HilCommand("monitor 1000", timeout_scale=0.75),
    HilCommand("monitor 0", timeout_scale=0.75),
    HilCommand("contrast 127", visual_check=True),
    HilCommand("clear", visual_check=True),
    HilCommand("cfg"),
)

SMOKE_COMMANDS: Tuple[HilCommand, ...] = (
    HilCommand("version"),
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
    HilCommand("contrast 127"),
    HilCommand("clear", visual_check=True),
    HilCommand("cfg"),
    HilCommand("display off", visual_check=True,
               note="Operator records whether ghosting remains while display is off."),
    HilCommand("display on", visual_check=True),
    HilCommand("clear", visual_check=True),
    HilCommand("pattern checker", visual_check=True, risky_visual=True, timeout_scale=2.0),
    HilCommand("clear", visual_check=True),
    HilCommand("display off", visual_check=True,
               note="End retention isolation with display off unless product policy says otherwise."),
    HilCommand("cfg"),
)


def soak_commands(ops: int) -> Tuple[HilCommand, ...]:
    count = max(1, int(ops))
    return (
        HilCommand("version"),
        HilCommand("cfg", require_clean_cfg=False),
        HilCommand("contrast 127"),
        HilCommand("clear", visual_check=True),
        HilCommand(f"stress_mix {count}", visual_check=True,
                   timeout_scale=max(4.0, min(60.0, count / 25.0)),
                   note="Bounded alternating stress; avoid long static full-on images."),
        HilCommand("clear", visual_check=True),
        HilCommand("cfg"),
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
        "initialized": r"initialized=(yes|no|true|false)",
        "dirty": r"dirty=(yes|no|true|false|0x[0-9a-fA-F]+)",
        "flushing": r"(?:flush|flushing)=(yes|no|true|false)",
        "control_dirty": r"controlDirty=(yes|no|true|false)",
        "scroll_active": r"scrollActive=(yes|no|true|false)",
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
    return data


def parse_counters(text: str) -> Dict[str, int]:
    clean = strip_ansi(text)
    counters: Dict[str, int] = {}
    patterns = {
        "pass": r"\bpass\s*[:=]\s*(\d+)",
        "fail": r"\bfail\s*[:=]\s*(\d+)",
        "skip": r"\bskip\s*[:=]\s*(\d+)",
        "successes": r"\bSuccesses:\s*(\d+)",
        "failures": r"\bFailures:\s*(\d+)",
        "operations": r"\b(?:Operations|Ops|Total ops):\s*(\d+)",
    }
    for key, regex in patterns.items():
        match = re.search(regex, clean, re.IGNORECASE)
        if match:
            counters[key] = int(match.group(1))
    return counters


def parse_command_count(command: str) -> Optional[int]:
    match = re.search(r"\b(?:stress|stress_mix|flushstress|burst)\s+(\d+)\b", command)
    return int(match.group(1)) if match else None


def has_failure_token(clean_response: str) -> bool:
    return any(pattern.search(clean_response) for pattern in FAIL_TOKEN_PATTERNS)


def check_expectations(parsed: Dict[str, object], expectations: Expectations) -> Optional[str]:
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
    if expectations.commit:
        observed = str(parsed.get("firmware_commit", ""))
        if not observed.startswith(expectations.commit):
            return f"expected commit prefix {expectations.commit}, observed {observed or 'unknown'}"
    return None


def classify_serial(command: HilCommand, response: str,
                    expectations: Optional[Expectations] = None) -> Tuple[str, str, Dict[str, object]]:
    expectations = expectations or Expectations()
    clean_response = strip_ansi(response)
    parsed: Dict[str, object] = {}
    if command.command == "version":
      parsed.update(parse_version_info(clean_response))
    if command.command == "cfg":
      parsed.update(parse_cfg(clean_response))
    parsed.update({f"counter_{k}": v for k, v in parse_counters(clean_response).items()})

    if not clean_response.strip():
        return "FAIL", "no serial response captured", parsed

    if has_failure_token(clean_response):
        return "FAIL", "failure token found in serial response", parsed

    if command.command == "version":
        mismatch = check_expectations(parsed, expectations)
        if mismatch:
            return "FAIL", mismatch, parsed
        if "controller_profile" in parsed or "library_version" in parsed:
            return "PASS", "firmware identity parsed", parsed

    if command.command == "scan":
        if expectations.address is not None:
            needle = f"{expectations.address:02X}"
            if needle not in clean_response.upper():
                return "FAIL", f"expected address 0x{expectations.address:02X} not found in scan", parsed
        return "PASS", "scan completed; expected address present when configured", parsed

    if command.command == "cfg":
        mismatch = check_expectations(parsed, expectations)
        if mismatch:
            return "FAIL", mismatch, parsed
        if command.require_clean_cfg:
            bad_cfg = []
            if str(parsed.get("control_dirty", "no")).lower() in ("yes", "true"):
                bad_cfg.append("controlDirty")
            if str(parsed.get("flushing", "no")).lower() in ("yes", "true"):
                bad_cfg.append("flushing")
            if str(parsed.get("scroll_active", "no")).lower() in ("yes", "true"):
                bad_cfg.append("scrollActive")
            dirty = parsed.get("dirty")
            if isinstance(dirty, str) and dirty.lower() in ("yes", "true"):
                bad_cfg.append("dirty")
            if isinstance(dirty, int) and dirty != 0:
                bad_cfg.append("dirty")
            if bad_cfg:
                return "FAIL", "cfg reports non-clean final state: " + ", ".join(bad_cfg), parsed
        elif parsed:
            return "PASS", "cfg parsed; clean-state check not required for this intermediate cfg", parsed
        return "PASS", "cfg parsed and clean-state checks passed", parsed

    if command.command == "probe":
        if re.search(r"\bStatus:\s*OK\b|\bProbe result:\s*OK\b", clean_response, re.IGNORECASE):
            return "PASS", "probe reported OK", parsed
        return "REVIEW_REQUIRED", "probe output needs human review", parsed

    if command.command == "selftest":
        counters = parse_counters(clean_response)
        if counters.get("fail", 0) == 0 and counters.get("pass", 0) > 0:
            return ("SERIAL_PASS_OPERATOR_REQUIRED" if command.visual_check else "PASS",
                    "selftest counters have fail=0", parsed)
        return "REVIEW_REQUIRED", "selftest counters not fully parsed", parsed

    if command.command.startswith(("stress", "flushstress", "burst")):
        expected = parse_command_count(command.command)
        counters = parse_counters(clean_response)
        failures = counters.get("failures", counters.get("fail", 0))
        successes = counters.get("successes", counters.get("operations"))
        if failures == 0 and expected is not None and successes == expected:
            return ("SERIAL_PASS_OPERATOR_REQUIRED" if command.visual_check else "PASS",
                    f"stress counters match N={expected}", parsed)
        if failures == 0 and successes is not None:
            return ("SERIAL_PASS_OPERATOR_REQUIRED" if command.visual_check else "PASS",
                    "stress counters show zero failures", parsed)
        return "REVIEW_REQUIRED", "stress counters not fully parsed", parsed

    if command.command.startswith("monitor"):
        expected = "OFF" if command.command.endswith(" 0") else "ON"
        if re.search(rf"\b(?:Health monitor|Monitor):\s*{expected}\b", clean_response, re.IGNORECASE):
            return "PASS", f"monitor {expected.lower()} acknowledged", parsed

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
    name = command.command
    if name.startswith(("stress", "flushstress", "burst")):
        return bool(
            re.search(r"\bResults:", clean)
            and re.search(r"\bSuccesses:\s*\d+", clean, re.IGNORECASE)
            and re.search(r"\bFailures:\s*\d+", clean, re.IGNORECASE)
        )
    if name == "selftest":
        return bool(re.search(r"\bSelftest result:\s*pass=\d+\s+fail=\d+", clean, re.IGNORECASE))
    if name == "scan":
        return "Scan complete" in clean
    if name == "probe":
        return bool(re.search(r"\bProbe result:\s*", clean, re.IGNORECASE))
    if name == "cfg":
        return "Config:" in clean and "initialized=" in clean and "controlDirty=" in clean
    if name == "version":
        return "SSD1315 library version:" in clean and "Geometry:" in clean
    if name.startswith("monitor"):
        return bool(re.search(r"\b(?:Health monitor|Monitor):\s*(ON|OFF)\b", clean, re.IGNORECASE))
    if name == "recover":
        return bool(re.search(r"\bRecover result:\s*", clean, re.IGNORECASE))
    return bool(re.search(r"\bOK\b|\bStatus:\s*OK\b", clean, re.IGNORECASE))


def read_until_ready(ser, timeout_s: float, idle_gap_s: float,
                     command: Optional[HilCommand] = None) -> Tuple[str, str]:
    deadline = time.monotonic() + timeout_s
    last_data_at = time.monotonic()
    data_seen = False
    completion_seen = command is None
    chunks: List[str] = []

    while time.monotonic() < deadline:
        pending = getattr(ser, "in_waiting", 0)
        data = ser.read(pending or 1)
        if data:
            text = decode(data)
            chunks.append(text)
            data_seen = True
            last_data_at = time.monotonic()
            joined = "".join(chunks)
            if command is not None and response_has_completion(command, joined):
                completion_seen = True
            if PROMPT_RE.search(joined):
                return joined, "prompt"
            continue
        if data_seen and completion_seen and (time.monotonic() - last_data_at) >= idle_gap_s:
            return "".join(chunks), "serial-idle"

    return "".join(chunks), "timeout"


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
        "started": datetime.now().isoformat(timespec="seconds"),
        "mode": args.mode,
        "port": args.port,
        "baud": args.baud,
        "base_timeout_s": args.timeout,
        "startup_wait_s": args.startup_wait,
        "idle_gap_s": args.idle_gap,
        "soak_duration_s": args.soak_duration_s,
        "soak_ops": args.soak_ops,
        "reconnect_attempts": args.reconnect_attempts,
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
        try:
            return serial.Serial(args.port, args.baud, timeout=0.1, write_timeout=2)
        except Exception as exc:  # pyserial raises SerialException/OSError variants.
            last_exc = exc
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
    if result.serial_result == "FAIL":
        return True
    if result.wait_reason == "timeout" and not result.clean_excerpt.strip():
        return True
    return False


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

    with open_serial(serial, args) as ser, transcript_path.open("w", encoding="utf-8", newline="\n") as transcript:
        transcript.write("# SSD1315 HIL serial transcript\n")
        transcript.write(f"# mode={args.mode} port={args.port} baud={args.baud} timeout={args.timeout}\n")
        transcript.write(f"# started={metadata['started']}\n\n")
        time.sleep(args.startup_wait)
        initial, _ = read_until_ready(ser, min(args.timeout, 3.0), args.idle_gap)
        if initial:
            transcript.write("## Initial serial output\n")
            transcript.write(initial)
            transcript.write("\n")

        duration_deadline = None
        if args.mode == "soak" and args.soak_duration_s > 0:
            duration_deadline = time.monotonic() + args.soak_duration_s

        stop_requested = False
        cycle = 0
        while not stop_requested:
            cycle += 1
            if duration_deadline is not None:
                transcript.write(f"\n## Soak cycle {cycle}\n")
                if args.verbose:
                    print(f"Starting soak cycle {cycle}")

            for item in commands:
                if duration_deadline is not None and time.monotonic() >= duration_deadline:
                    stop_requested = True
                    break
                if item.risky_visual:
                    print(f"Warning: `{item.command}` may show static/high-contrast OLED content briefly.")
                per_command_timeout = max(0.5, args.timeout * item.timeout_scale)
                transcript.write(f"\n>>> {item.command}\n")
                transcript.flush()
                ser.write((item.command + "\n").encode("utf-8"))
                ser.flush()
                start = time.monotonic()
                response, wait_reason = read_until_ready(ser, per_command_timeout, args.idle_gap, item)
                elapsed = time.monotonic() - start
                transcript.write(response)
                if response and not response.endswith("\n"):
                    transcript.write("\n")
                transcript.flush()
                if args.verbose:
                    print(response, end="" if response.endswith("\n") else "\n")

                serial_result, note, parsed = classify_serial(item, response, expectations)
                if wait_reason == "timeout" and serial_result in ("PASS", "SERIAL_PASS_OPERATOR_REQUIRED"):
                    serial_result = "REVIEW_REQUIRED"
                    note = "success token found, but command wait timed out"

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
                    note = f"soak cycle {cycle}; {note}"
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
                    stop_requested = True
                    break

            if duration_deadline is None:
                break
            if args.soak_max_cycles > 0 and cycle >= args.soak_max_cycles:
                stop_requested = True

    metadata["completed"] = datetime.now().isoformat(timespec="seconds")
    metadata["elapsed_s"] = time.monotonic() - run_started
    metadata["command_count"] = len(results)
    metadata["result_counts"] = result_counts(results)
    metadata["latency_stats"] = latency_stats(results)

    write_artifacts(log_dir, args, commands, results, metadata)
    return log_dir, results, metadata


def verdicts_for(mode: str, results: List[CommandResult]) -> Dict[str, object]:
    serial_fail = any(result.serial_result == "FAIL" for result in results)
    serial_review = any("REVIEW" in result.serial_result for result in results)
    visual_results = [r for r in results if r.operator_result not in ("N/A",)]
    visual_complete = bool(visual_results) and all(
        r.operator_result not in ("OPERATOR_REQUIRED", "SKIPPED_SERIAL_ONLY", "UNKNOWN") for r in visual_results
    )
    visual_fail = any(r.operator_result == "FAIL" for r in visual_results)
    retention_run = mode in ("retention", "all")
    soak_run = mode in ("soak", "all")
    return {
        "serial_device_pass": not serial_fail,
        "serial_review_required": serial_review,
        "visual_complete": visual_complete,
        "visual_pass": visual_complete and not visual_fail,
        "retention_isolation_complete": retention_run and visual_complete,
        "soak_complete": soak_run and not serial_fail,
        "field_ready_claim_allowed": False,
    }


def first_last_cfg(results: List[CommandResult]) -> Tuple[Dict[str, object], Dict[str, object]]:
    cfgs = [result.parsed for result in results if result.command == "cfg" and result.parsed]
    return (cfgs[0] if cfgs else {}, cfgs[-1] if cfgs else {})


def write_artifacts(log_dir: Path, args: argparse.Namespace, commands: Sequence[HilCommand],
                    results: List[CommandResult], metadata: Dict[str, object]) -> None:
    verdicts = verdicts_for(args.mode, results)
    initial_cfg, final_cfg = first_last_cfg(results)

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
    (log_dir / "health_delta.json").write_text(
        json.dumps({"initial_cfg": initial_cfg, "final_cfg": final_cfg}, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    (log_dir / "run_stats.json").write_text(
        json.dumps({
            "elapsed_s": metadata.get("elapsed_s", 0.0),
            "command_count": len(results),
            "counts": result_counts(results),
            "latency": latency_stats(results),
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
        summary.write("## Run Stats\n\n")
        summary.write(f"- Elapsed seconds: `{metadata.get('elapsed_s', 0.0):.2f}`\n")
        summary.write(f"- Command count: `{len(results)}`\n")
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

    with (log_dir / "hardware_matrix_fragment.md").open("w", encoding="utf-8", newline="\n") as out:
        out.write("# SSD1315 Hardware Matrix Fragment\n\n")
        out.write("| Field | Result |\n| --- | --- |\n")
        out.write(f"| Branch | `{metadata['host_branch']}` |\n")
        out.write(f"| Commit hash | `{metadata['host_commit']}` |\n")
        out.write(f"| Worktree state | `{metadata['host_worktree']}` |\n")
        out.write(f"| Serial port | `{metadata['port']}` |\n")
        out.write(f"| Baud rate | `{metadata['baud']}` |\n")
        out.write(f"| MCU board | `{metadata.get('board') or 'unknown'}` |\n")
        out.write(f"| Panel module model | `{metadata.get('panel') or 'unknown'}` |\n")
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
        "parsed_cfg_initial.json", "parsed_cfg_final.json", "health_delta.json",
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
    parser.add_argument("--mode", choices=("smoke", "functional", "retention", "soak", "all", "benchmark"),
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
    parser.add_argument("--reconnect-delay-s", type=float, default=1.0,
                        help="Delay between bounded serial open retries")
    parser.add_argument("--verbose", action="store_true",
                        help="Echo command responses to stdout while still writing transcripts")
    parser.add_argument("--continue-on-fail", action="store_true",
                        help="Continue after FAIL results; default stops on FAIL or no-response timeout")
    parser.add_argument("--strict", action="store_true")
    parser.add_argument("--json", action="store_true", help="Accepted for compatibility; JSON is always written")
    parser.add_argument("--csv", action="store_true", help="Accepted for compatibility; CSV is always written")
    parser.add_argument("--update-matrix-fragment", action="store_true",
                        help="Accepted for compatibility; matrix fragment is always written")
    parser.add_argument("--no-risky-visuals", action="store_true",
                        help="Skip fill/contrast255/static checker style visual commands")
    args = parser.parse_args(argv)
    if args.soak_duration_hours > 0:
        args.soak_duration_s = args.soak_duration_hours * 3600.0
    return args


def expectations_from_args(args: argparse.Namespace) -> Expectations:
    return Expectations(
        address=parse_hex_or_any(args.expect_address),
        width=args.expect_width,
        height=args.expect_height,
        controller=parse_text_or_any(args.expect_controller),
        panel_profile=parse_text_or_any(args.expect_panel_profile),
        commit=parse_text_or_any(args.expect_commit),
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
        log_dir, results, _ = run_commands(args, commands, expectations)
    except FileExistsError as exc:
        print(str(exc), file=sys.stderr)
        return 2

    verdicts = verdicts_for(args.mode, results)
    print(f"HIL logs written to: {log_dir}")
    print("Device tester verdicts:")
    print(f"  Serial device test: {'PASS' if verdicts['serial_device_pass'] else 'FAIL'}")
    print(f"  Visual operator checks: {'COMPLETE' if verdicts['visual_complete'] else 'INCOMPLETE'}")
    print(f"  Retention isolation: {'COMPLETE' if verdicts['retention_isolation_complete'] else ('NOT_RUN' if args.mode not in ('retention', 'all') else 'INCOMPLETE')}")
    print(f"  Soak: {'COMPLETE' if verdicts['soak_complete'] else ('NOT_RUN' if args.mode not in ('soak', 'all') else 'INCOMPLETE')}")
    print("  Field-ready evidence: NO")
    return 1 if not verdicts["serial_device_pass"] else 0


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
