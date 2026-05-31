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
    HilCommand("cfg"),
    HilCommand("selftest", timeout_scale=2.0,
               note="Selftest is serial/software evidence; it does not prove visual correctness."),
    HilCommand("pattern checker", visual_check=True, risky_visual=True),
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
    HilCommand("cfg"),
)

RETENTION_COMMANDS: Tuple[HilCommand, ...] = (
    HilCommand("version"),
    HilCommand("cfg"),
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
    HilCommand("pattern checker", visual_check=True, risky_visual=True),
    HilCommand("clear", visual_check=True),
    HilCommand("display off", visual_check=True,
               note="End retention isolation with display off unless product policy says otherwise."),
    HilCommand("cfg"),
)


def soak_commands(ops: int) -> Tuple[HilCommand, ...]:
    count = max(1, int(ops))
    return (
        HilCommand("version"),
        HilCommand("cfg"),
        HilCommand("contrast 127"),
        HilCommand("clear", visual_check=True),
        HilCommand(f"stress_mix {count}", visual_check=True,
                   timeout_scale=max(4.0, min(60.0, count / 25.0)),
                   note="Bounded alternating stress; avoid long static full-on images."),
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


def git_value(*args: str) -> str:
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
    return result.stdout.strip() or "unknown"


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
    match = re.search(r"\b(?:stress|stress_mix)\s+(\d+)\b", command)
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

    if command.command.startswith("stress"):
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
        if re.search(rf"\bMonitor:\s*{expected}\b", clean_response, re.IGNORECASE):
            return "PASS", f"monitor {expected.lower()} acknowledged", parsed

    if command.visual_check and any(pattern.search(clean_response) for pattern in PASS_HINTS):
        return "SERIAL_PASS_OPERATOR_REQUIRED", "serial OK; visual command requires operator evidence", parsed

    if any(pattern.search(clean_response) for pattern in PASS_HINTS):
        return "PASS", "serial response contained an expected success token", parsed

    if command.visual_check:
        return "SERIAL_REVIEW_OPERATOR_REQUIRED", "visual command serial output needs review", parsed

    return "REVIEW_REQUIRED", "serial response did not contain a deterministic pass token", parsed


def read_until_ready(ser, timeout_s: float, idle_gap_s: float) -> Tuple[str, str]:
    deadline = time.monotonic() + timeout_s
    last_data_at = time.monotonic()
    data_seen = False
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
            if PROMPT_RE.search(joined):
                return joined, "prompt"
            continue
        if data_seen and (time.monotonic() - last_data_at) >= idle_gap_s:
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
    status = git_value("status", "--short")
    worktree = "unknown" if status == "unknown" else ("clean" if not status else "dirty")
    return {
        "tool": "run_ssd1315_hil.py",
        "tool_version": SCRIPT_VERSION,
        "started": datetime.now().isoformat(timespec="seconds"),
        "mode": args.mode,
        "port": args.port,
        "baud": args.baud,
        "base_timeout_s": args.timeout,
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
    }


def strict_metadata_missing(args: argparse.Namespace) -> List[str]:
    missing = []
    for attr in ("operator", "board", "panel", "supply_voltage", "pullups", "reset_wired", "bus_speed"):
        if getattr(args, attr) in (None, "", "unknown"):
            missing.append(attr.replace("_", "-"))
    return missing


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

    with serial.Serial(args.port, args.baud, timeout=0.1, write_timeout=2) as ser, transcript_path.open(
        "w", encoding="utf-8", newline="\n"
    ) as transcript:
        transcript.write("# SSD1315 HIL serial transcript\n")
        transcript.write(f"# mode={args.mode} port={args.port} baud={args.baud} timeout={args.timeout}\n")
        transcript.write(f"# started={metadata['started']}\n\n")
        time.sleep(args.startup_wait)
        initial, _ = read_until_ready(ser, min(args.timeout, 3.0), args.idle_gap)
        if initial:
            transcript.write("## Initial serial output\n")
            transcript.write(initial)
            transcript.write("\n")

        for item in commands:
            if item.risky_visual:
                print(f"Warning: `{item.command}` may show static/high-contrast OLED content briefly.")
            per_command_timeout = max(0.5, args.timeout * item.timeout_scale)
            transcript.write(f"\n>>> {item.command}\n")
            transcript.flush()
            ser.write((item.command + "\n").encode("utf-8"))
            ser.flush()
            start = time.monotonic()
            response, wait_reason = read_until_ready(ser, per_command_timeout, args.idle_gap)
            elapsed = time.monotonic() - start
            transcript.write(response)
            if response and not response.endswith("\n"):
                transcript.write("\n")
            transcript.flush()

            serial_result, note, parsed = classify_serial(item, response, expectations)
            if wait_reason == "timeout" and serial_result == "PASS":
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
            results.append(CommandResult(
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
            ))

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
        out.write(f"| I2C address | `{final_cfg.get('i2c_address', initial_cfg.get('i2c_address', 'OPERATOR_REQUIRED'))}` |\n")
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
        "failure_analysis.md", "command_plan.json",
    ):
        print(f"{artifact}: {log_dir / artifact}")
    print("\nCommand sequence:")
    for item in commands:
        marker = " [operator visual check]" if item.visual_check else ""
        risky = " [OLED-risky]" if item.risky_visual else ""
        print(f"  {item.command}{marker}{risky}")


def parse_args(argv: Optional[List[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("smoke", "functional", "retention", "soak", "all"),
                        default="functional", help="HIL command plan to run")
    parser.add_argument("--port", help="Serial port for validation firmware, for example COM5 or /dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD, help=f"Serial baud rate (default: {DEFAULT_BAUD})")
    parser.add_argument("--out", default=str(DEFAULT_OUT_ROOT), help="Root directory for timestamped HIL logs")
    parser.add_argument("--timeout", "--command-timeout", dest="timeout", type=float, default=DEFAULT_TIMEOUT_S,
                        help=f"Base per-command timeout in seconds (default: {DEFAULT_TIMEOUT_S})")
    parser.add_argument("--startup-wait", type=float, default=1.0,
                        help="Seconds to wait after opening serial before sending commands")
    parser.add_argument("--idle-gap", type=float, default=0.35,
                        help="Treat command output as complete after this much serial silence")
    parser.add_argument("--dry-run", action="store_true",
                        help="Print command sequence and output paths without opening serial")
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
    parser.add_argument("--strict", action="store_true")
    parser.add_argument("--json", action="store_true", help="Accepted for compatibility; JSON is always written")
    parser.add_argument("--csv", action="store_true", help="Accepted for compatibility; CSV is always written")
    parser.add_argument("--update-matrix-fragment", action="store_true",
                        help="Accepted for compatibility; matrix fragment is always written")
    parser.add_argument("--no-risky-visuals", action="store_true",
                        help="Skip fill/contrast255/static checker style visual commands")
    return parser.parse_args(argv)


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


if __name__ == "__main__":
    raise SystemExit(main())
