#!/usr/bin/env python3
"""Run the SSD1315 serial HIL smoke sequence and capture repeatable logs.

This tool does not flash firmware. Build and upload the selected Arduino or
ESP-IDF validation firmware first, then run this script against its serial CLI.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
import time
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Iterable, List, Optional, Tuple


DEFAULT_BAUD = 115200
DEFAULT_TIMEOUT_S = 8.0
DEFAULT_OUT_ROOT = Path("hil_logs")
PROMPT_RE = re.compile(r"(^|\r?\n)> $")


@dataclass(frozen=True)
class HilCommand:
    command: str
    visual_check: bool = False
    timeout_scale: float = 1.0
    note: str = ""


COMMANDS: Tuple[HilCommand, ...] = (
    HilCommand("version"),
    HilCommand("scan"),
    HilCommand("probe"),
    HilCommand("cfg"),
    HilCommand("selftest", visual_check=True, timeout_scale=2.0,
               note="Selftest is serial/software evidence; visual effects still need operator review."),
    HilCommand("pattern checker", visual_check=True),
    HilCommand("clear", visual_check=True),
    HilCommand("fill", visual_check=True, note="Do not leave full-on OLED content static."),
    HilCommand("invert 1", visual_check=True),
    HilCommand("invert 0", visual_check=True),
    HilCommand("contrast 1", visual_check=True),
    HilCommand("contrast 127", visual_check=True),
    HilCommand("contrast 255", visual_check=True, note="Restore contrast after observing."),
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


FAIL_PATTERNS = (
    re.compile(r"\bFAIL(?:ED)?\b", re.IGNORECASE),
    re.compile(r"\bERROR\b", re.IGNORECASE),
    re.compile(r"\b(?:Status:\s*)?I2C_TIMEOUT\b", re.IGNORECASE),
    re.compile(r"\bStatus:\s*TIMEOUT\b", re.IGNORECASE),
    re.compile(r"\bINVALID_", re.IGNORECASE),
    re.compile(r"\bSTATE_ERROR\b", re.IGNORECASE),
    re.compile(r"\bDEVICE_NOT_FOUND\b", re.IGNORECASE),
    re.compile(r"\bI2C_NACK", re.IGNORECASE),
    re.compile(r"\bI2C_BUS_ERROR\b", re.IGNORECASE),
)

PASS_HINTS = (
    re.compile(r"\bStatus:\s*OK\b", re.IGNORECASE),
    re.compile(r"\bbegin\(\).*OK\b", re.IGNORECASE),
    re.compile(r"\bPASS\b", re.IGNORECASE),
    re.compile(r"\bVersion\b", re.IGNORECASE),
    re.compile(r"\bConfig\b", re.IGNORECASE),
    re.compile(r"\bMonitor:\s*(ON|OFF)\b", re.IGNORECASE),
)


@dataclass
class CommandResult:
    command: str
    serial_result: str
    operator_result: str
    wait_reason: str
    elapsed_s: float
    note: str


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


def make_log_dir(out_root: Path, timestamp: Optional[str] = None) -> Path:
    stamp = timestamp or datetime.now().strftime("%Y%m%d_%H%M%S")
    log_dir = out_root / f"ssd1315_{stamp}"
    if log_dir.exists():
        raise FileExistsError(f"Refusing to overwrite existing HIL log directory: {log_dir}")
    return log_dir


def classify_serial(command: HilCommand, response: str) -> Tuple[str, str]:
    if any(pattern.search(response) for pattern in FAIL_PATTERNS):
        return "FAIL", "failure token found in serial response"
    if command.visual_check:
        return "SERIAL_OK_OR_REVIEW", "visual command requires operator result"
    if any(pattern.search(response) for pattern in PASS_HINTS):
        return "PASS", "serial response contained an expected success token"
    if response.strip():
        return "REVIEW_REQUIRED", "serial response did not contain a deterministic pass token"
    return "FAIL", "no serial response captured"


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


def run_commands(args: argparse.Namespace, commands: Iterable[HilCommand]) -> Tuple[Path, List[CommandResult]]:
    serial = _load_serial_module()
    if not args.port:
        raise SystemExit("--port is required unless --dry-run is used")

    log_dir = make_log_dir(Path(args.out))
    log_dir.mkdir(parents=True, exist_ok=False)
    transcript_path = log_dir / "serial_transcript.txt"

    results: List[CommandResult] = []

    with serial.Serial(args.port, args.baud, timeout=0.1, write_timeout=2) as ser, transcript_path.open(
        "w", encoding="utf-8", newline="\n"
    ) as transcript:
        transcript.write(f"# SSD1315 HIL serial transcript\n")
        transcript.write(f"# port={args.port} baud={args.baud} timeout={args.timeout}\n")
        transcript.write(f"# started={datetime.now().isoformat(timespec='seconds')}\n\n")
        time.sleep(args.startup_wait)
        initial, reason = read_until_ready(ser, min(args.timeout, 3.0), args.idle_gap)
        if initial:
            transcript.write("## Initial serial output\n")
            transcript.write(initial)
            transcript.write("\n")

        for item in commands:
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

            serial_result, note = classify_serial(item, response)
            operator_result = "OPERATOR_CHECK_REQUIRED" if item.visual_check else "N/A"
            if wait_reason == "timeout" and serial_result == "PASS":
                serial_result = "REVIEW_REQUIRED"
                note = "success token found, but command wait timed out"
            if item.note:
                note = f"{note}; {item.note}"
            results.append(CommandResult(item.command, serial_result, operator_result,
                                         wait_reason, elapsed, note))

    write_summary(log_dir, args, results)
    return log_dir, results


def write_summary(log_dir: Path, args: argparse.Namespace, results: List[CommandResult]) -> None:
    summary_path = log_dir / "summary.md"
    with summary_path.open("w", encoding="utf-8", newline="\n") as summary:
        summary.write("# SSD1315 HIL Command Summary\n\n")
        summary.write(f"- Port: `{args.port}`\n")
        summary.write(f"- Baud: `{args.baud}`\n")
        summary.write(f"- Base timeout: `{args.timeout}` seconds\n")
        summary.write(f"- Transcript: `serial_transcript.txt`\n\n")
        summary.write("## Host Repository Metadata\n\n")
        summary.write(f"- Branch: `{git_value('branch', '--show-current')}`\n")
        summary.write(f"- Commit: `{git_value('rev-parse', 'HEAD')}`\n")
        status = git_value("status", "--short")
        worktree = "unknown" if status == "unknown" else ("clean" if not status else "dirty")
        summary.write(f"- Worktree: `{worktree}`\n\n")
        summary.write("| Command | Serial Result | Operator Result | Wait | Seconds | Notes |\n")
        summary.write("| --- | --- | --- | --- | ---: | --- |\n")
        for result in results:
            summary.write(
                f"| `{result.command}` | {result.serial_result} | {result.operator_result} | "
                f"{result.wait_reason} | {result.elapsed_s:.2f} | {result.note} |\n"
            )
        summary.write("\n## Operator Visual Checklist\n\n")
        for item in COMMANDS:
            if item.visual_check:
                summary.write(f"- [ ] `{item.command}` observed and recorded in hardware matrix.\n")
        summary.write("\n## Evidence To Attach\n\n")
        summary.write("- Serial transcript from this directory.\n")
        summary.write("- Photos or video for checkerboard, clear, fill, contrast, flip, and scroll.\n")
        summary.write("- If clear/ghosting is visible, run the runbook isolation sequence with display off/on photos.\n")
        summary.write("- Notes for missing-display, reset, unplug/replug, and soak tests if run.\n")


def print_dry_run(args: argparse.Namespace) -> None:
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    log_dir = Path(args.out) / f"ssd1315_{stamp}"
    print("SSD1315 HIL dry run")
    print(f"Port: {args.port or '<required for serial run>'}")
    print(f"Baud: {args.baud}")
    print(f"Base timeout: {args.timeout} seconds")
    print(f"Output directory: {log_dir}")
    print(f"Transcript: {log_dir / 'serial_transcript.txt'}")
    print(f"Summary: {log_dir / 'summary.md'}")
    print("\nCommand sequence:")
    for item in COMMANDS:
        marker = " [operator visual check]" if item.visual_check else ""
        print(f"  {item.command}{marker}")


def parse_args(argv: Optional[List[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Serial port for validation firmware, for example COM5 or /dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD, help=f"Serial baud rate (default: {DEFAULT_BAUD})")
    parser.add_argument("--out", default=str(DEFAULT_OUT_ROOT), help="Root directory for timestamped HIL logs")
    parser.add_argument("--timeout", type=float, default=DEFAULT_TIMEOUT_S,
                        help=f"Base per-command timeout in seconds (default: {DEFAULT_TIMEOUT_S})")
    parser.add_argument("--startup-wait", type=float, default=1.0,
                        help="Seconds to wait after opening serial before sending commands")
    parser.add_argument("--idle-gap", type=float, default=0.35,
                        help="Treat command output as complete after this much serial silence")
    parser.add_argument("--dry-run", action="store_true",
                        help="Print command sequence and output paths without opening serial")
    return parser.parse_args(argv)


def main(argv: Optional[List[str]] = None) -> int:
    args = parse_args(argv)
    if args.dry_run:
        print_dry_run(args)
        return 0

    try:
        log_dir, results = run_commands(args, COMMANDS)
    except FileExistsError as exc:
        print(str(exc), file=sys.stderr)
        return 2

    print(f"HIL logs written to: {log_dir}")
    print("Operator visual checks:")
    for item in COMMANDS:
        if item.visual_check:
            print(f"  - {item.command}")
    if any(result.serial_result == "FAIL" for result in results):
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
