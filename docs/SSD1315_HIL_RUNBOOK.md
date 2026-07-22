# SSD1315 HIL Runbook

Status: HIL procedure and evidence template. This document does not claim a
new hardware pass by itself.

Use this runbook to produce repeatable serial logs, visual evidence, and matrix
results for `docs/SSD1315_HARDWARE_VALIDATION.md`.

The shipped Arduino and ESP-IDF CLIs are bring-up diagnostics. A production
shared-bus qualification must additionally exercise the application's sole bus
owner, operation identity/deadline/cancellation path, and single-attempt
transport adapter.

Document ownership:

- `docs/SSD1315_HIL_RUNBOOK.md`: procedure.
- `docs/SSD1315_HIL_TARGET_TEMPLATE.md`: per-target setup and commands.
- `docs/SSD1315_HARDWARE_VALIDATION.md`: final committed result matrix.

## Operator Flow

1. Confirm the branch, commit hash, and clean worktree.
2. Copy `docs/SSD1315_HIL_TARGET_TEMPLATE.md` and fill the pre-run target
   fields.
3. Build both supported targets.
4. Flash only the firmware target that matches the connected board.
5. Optionally open a serial monitor and run `version`, `help`, and `cfg`.
6. Run `python tools/run_ssd1315_hil.py --dry-run --mode functional`.
7. Run the real HIL sequence with the target serial port. Use
   `--interactive-visual` when the operator can enter observations during the
   run.
8. Capture photos or video at the operator-check commands.
9. Fill `docs/SSD1315_HARDWARE_VALIDATION.md` from the transcript, summary,
   visual evidence, and fault/soak notes.
10. Commit the completed matrix. Store large media outside the repository unless
    the project policy says to commit it.

## 1. Preflight Record

Fill the pre-run fields before flashing or running commands. Use `unknown`
rather than guessing. Fill generated log, photo, and logic-analyzer paths after
the run.

For per-target setup details, copy
`docs/SSD1315_HIL_TARGET_TEMPLATE.md` and fill in the target-specific commands,
wiring, and evidence paths before running the sequence.

| Field | Value |
| --- | --- |
| Operator |  |
| Date/time |  |
| Branch |  |
| Commit hash |  |
| Firmware framework | Arduino PlatformIO / ESP-IDF |
| Firmware build target |  |
| Serial port |  |
| Baud rate | 115200 |
| MCU board |  |
| Panel module model |  |
| Controller marking, if visible |  |
| Resolution |  |
| 7-bit I2C address | `0x3C` / `0x3D` / unknown |
| Supply voltage |  |
| Pull-up values |  |
| Reset pin connected | yes / no / unknown |
| Bus speed |  |
| Configured `maxWriteBytes` |  |
| Operation mode | cooperative owner API / blocking compatibility CLI |
| Charge-pump / VCC mode | internal / external / unknown |
| IREF mode | internal / external resistor / unknown |
| Configured panel profile |  |
| Log directory |  |
| Photo/video directory |  |
| Logic analyzer capture path |  |

Record the exact commit with:

```bash
git branch --show-current
git rev-parse HEAD
git status --short
```

The worktree should be clean before creating hardware evidence. If it is dirty,
record the diff or stop and rebuild from a clean commit.

## 2. Build And Flash

### Arduino PlatformIO

Build both supported Arduino targets before selecting the board to flash:

```bash
python -m platformio run -e esp32s3dev
python -m platformio run -e esp32s2dev
```

Upload the matching target to the connected board. Do not guess the port:

```bash
python -m platformio run -e esp32s3dev --target upload --upload-port <serial-port>
python -m platformio run -e esp32s2dev --target upload --upload-port <serial-port>
```

Manual monitor command:

```bash
python -m platformio device monitor --port <serial-port> --baud 115200
```

### ESP-IDF

Build both supported ESP-IDF targets when `idf.py` is available:

```bash
idf.py -C examples/espidf_basic set-target esp32s3
idf.py -C examples/espidf_basic build
idf.py -C examples/espidf_basic fullclean
idf.py -C examples/espidf_basic set-target esp32s2
idf.py -C examples/espidf_basic build
```

Flash and monitor only the target that matches the connected board:

```bash
idf.py -C examples/espidf_basic -p <serial-port> flash monitor
```

## 3. Serial Runner

Preview the command sequence without opening a serial port:

```bash
python tools/run_ssd1315_hil.py --dry-run --mode functional
```

Run a quick serial smoke test and capture logs:

```bash
python tools/run_ssd1315_hil.py --mode smoke --port <serial-port> --baud 115200 --out hil_logs --expect-address 0x3C --serial-only
```

Run the full functional sequence with operator visual prompts:

```bash
python tools/run_ssd1315_hil.py --mode functional --port <serial-port> --baud 115200 --out hil_logs --interactive-visual
```

Additional modes:

```bash
python tools/run_ssd1315_hil.py --mode retention --port <serial-port> --baud 115200 --out hil_logs --interactive-visual
python tools/run_ssd1315_hil.py --mode soak --port <serial-port> --baud 115200 --out hil_logs --soak-ops 1000
python tools/run_ssd1315_hil.py --mode soak --port <serial-port> --baud 115200 --out hil_logs --soak-ops 500 --soak-duration-hours 1 --serial-only
python tools/run_ssd1315_hil.py --mode all --port <serial-port> --baud 115200 --out hil_logs --interactive-visual --soak-ops 1000
python tools/run_ssd1315_hil.py --mode arduino-extended --port <serial-port> --baud 115200 --out hil_logs --serial-only
```

Mode meanings:

- `smoke`: version, telemetry, scan/probe, cfg, selftest, final cfg.
- `functional`: the executable command sequence below.
- `retention`: clear/display-off/display-on prompts to distinguish live GDDRAM
  state from OLED image retention or panel aging.
- `soak`: bounded mixed stress using alternating content. Duration-based soak
  finishes the current cycle, including final `clear` and clean `cfg`, before
  exiting after the requested deadline. Its PASS verdict also requires measured
  soak elapsed time to meet the target and monotonic uptime/loop-heartbeat with
  no reset-reason transition in sampled telemetry.
- `all`: smoke, functional, retention, and soak in one logged run.
- `arduino-extended`: Arduino-only safe diagnostics, compatibility policy,
  display controls, graphics primitives, partial flush, page iteration, and
  software reset. It excludes raw controller `cmd*` passthrough and is not an
  ESP-IDF parity plan.

The runner creates a timestamped directory such as
`hil_logs/ssd1315_YYYYMMDD_HHMMSS/` containing:

- `serial_transcript.txt`: raw serial transcript.
- `summary.md`: command summary, host branch/commit metadata, and verdicts.
- `results.json`: machine-readable command results and parsed fields, including
  uptime, loop heartbeat, reset reason, free heap, and minimum free heap when
  the target firmware supports the `telemetry` command.
- `results.csv`: spreadsheet-friendly command table.
- `metadata.json`: host, operator, target, and command-line metadata.
- `operator_visual_checklist.md`: visual checkpoints and operator notes.
- `hardware_matrix_fragment.md`: rows that can be pasted into the validation
  matrix.
- `parsed_cfg_initial.json`, `parsed_cfg_final.json`, `health_delta.json`
  (initial/final telemetry, deltas, and trend findings),
  `failure_analysis.md`, and `command_plan.json`.

The runner never flashes firmware and never overwrites an existing log
directory. If `pyserial` is missing, install it with:

```bash
python -m pip install pyserial
```

The runner waits for a CLI prompt, explicit known terminator, serial-idle
interval, or timeout. Unknown-command/error responses are terminal failures.
Expected scan addresses are accepted only from scanner grid rows, not the
human-readable common-address footer. This is
intentional because the ESP-IDF CLI prints a `>` prompt while the Arduino CLI
logs command responses without a prompt.

Serial PASS is not visual PASS. The runner can automatically classify command
responses, stress counters, and clean `cfg` state, but display appearance rows
remain `OPERATOR_REQUIRED` unless interactive visual answers are recorded.
Field-ready evidence remains `NO` until serial, visual, fault/recovery, reset
where applicable, and soak evidence are complete.

## 4. HIL Command Sequence

The runner and matrix use this executable sequence:

```text
version
telemetry
scan
probe
cfg
selftest
pattern checker
clear
fill
invert 1
invert 0
contrast 1
contrast 127
contrast 255
flipx 1
flipx 0
flipy 1
flipy 0
scrollh right 0 7
scrollv left 0 7 1
scroll stop
recover
stress 100
stress_mix 100
monitor 1000
monitor 0
telemetry
contrast 127
clear
cfg
```

Do not use `contrast 0`; the driver rejects it by contract. `probe` is ACK-only
and does not identify SSD1315 silicon. `recover` is software-only and does not
toggle `RES#`.

`selftest` provides serial/software evidence only. It may alter the display
state, but its PASS output does not prove visual correctness.

## 5. Per-Command Result Table

Copy this table into the hardware matrix or run notes for each HIL run.

| Command | Expected serial result | Observed serial result | Expected visual result | Observed visual result | Pass/Fail | Evidence ID | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `version` | Build/version/config context prints |  | N/A | N/A |  |  |  |
| `telemetry` | Uptime, loop heartbeat, reset reason, free heap, and minimum free heap print |  | N/A | N/A |  |  |  |
| `scan` | Expected address visible |  | N/A | N/A |  |  |  |
| `probe` | ACK/presence status only |  | N/A | N/A |  |  |  |
| `cfg` | Address/profile/dirty/flush state prints |  | N/A | N/A |  |  |  |
| `selftest` | Serial checks complete without failure |  | N/A; software/serial evidence only | N/A |  |  |  |
| `pattern checker` | Command reports OK or no error |  | Checkerboard visible and aligned |  |  |  |  |
| `clear` | Command reports OK or no error |  | Panel fully blank |  |  |  |  |
| `fill` | Command reports OK or no error |  | Panel fully lit briefly |  |  |  |  |
| `invert 1` | Command reports OK |  | Inversion enabled |  |  |  |  |
| `invert 0` | Command reports OK |  | Inversion disabled |  |  |  |  |
| `contrast 1` | Command reports OK |  | Panel visibly dims |  |  |  |  |
| `contrast 127` | Command reports OK |  | Mid-level brightness restored |  |  |  |  |
| `contrast 255` | Command reports OK |  | Panel visibly brightens briefly |  |  |  |  |
| `flipx 1` | Command reports OK |  | Horizontal orientation changes |  |  |  |  |
| `flipx 0` | Command reports OK |  | Horizontal orientation restores |  |  |  |  |
| `flipy 1` | Command reports OK |  | Vertical orientation changes |  |  |  |  |
| `flipy 0` | Command reports OK |  | Vertical orientation restores |  |  |  |  |
| `scrollh right 0 7` | Command reports OK |  | Content scrolls right over pages 0..7 |  |  |  |  |
| `scrollv left 0 7 1` | Command reports OK |  | Content scrolls left with vertical offset |  |  |  |  |
| `scroll stop` | Command reports OK |  | Motion stops |  |  |  |  |
| `recover` | Command reports OK or precise error |  | Display usable after redraw/flush |  |  |  |  |
| `stress 100` | Bounded stress completes |  | No stuck all-on static image |  |  |  |  |
| `stress_mix 100` | Bounded mixed stress completes |  | No tearing or stale-page artifacts |  |  |  |  |
| `monitor 1000` | Monitor enables at bounded interval |  | N/A | N/A |  |  |  |
| `monitor 0` | Monitor disables |  | N/A | N/A |  |  |  |
| post-monitor `telemetry` | Uptime, loop heartbeat, reset reason, free heap, and minimum free heap print |  | N/A | N/A |  |  |  |
| final `contrast 127` | Command reports OK |  | Mid-level brightness restored |  |  |  |  |
| final `clear` | Command reports OK |  | Panel fully blank |  |  |  |  |
| final `cfg` | No dirty/error state unless explained |  | N/A | N/A |  |  |  |

## 6. Visual Acceptance Criteria

- Checkerboard is visible, aligned to the active columns/pages, and not shifted.
- Clear is fully blank.
- Fill is fully lit but not left static longer than needed.
- Invert toggles the visible pixels without modifying framebuffer content.
- Contrast visibly changes at `1`, `127`, and `255`; `0` is not sent.
- `flipx` and `flipy` change orientation as expected and restore at `0`.
- Scroll direction and bounds match command names.
- After `scroll stop`, redraw/flush before judging framebuffer alignment.
- `recover` returns the display to a usable state after redraw/flush.
- There is no obvious torn page addressing, column offset, mirrored surprise, or
  partially stale data.

## 7. Fault And Recovery Criteria

Run these only when safe for the board and panel.

- Missing display: power down or disconnect only if safe, then run `probe` and
  `selftest`. Expected result is a precise failure status, not a false pass or
  hang.
- Unplug/replug: avoid hotplug unless the fixture is designed for it. Record
  whether `recover` restores the display after replug and redraw/flush.
- Reset pin: run only if `RES#` is wired to application-owned fixture code.
  Record pulse timing, then run `recover` or `begin`, redraw, and flush.
- Forced failure: if transport fault injection is available, record whether
  `controlStateDirty()` is exposed by `cfg` after the failure and cleared only
  by successful recover/resync.
- Soak: use moving or alternating content. Prefer:

```text
stress 1000
stress_mix 500
contrast 127
clear
cfg
```

Record duration, I2C errors, visual artifacts, reset/reconnect events, final
dirty/control state, and whether high-contrast static images were avoided.

## 8. Clear/Ghosting Isolation

Use this sequence if `clear` appears to leave stale content, `fill` followed by
`clear` does not look blank, or a demo shows old content underneath. Capture
photos or video before and after `display off`.

Preferred runner form:

```bash
python tools/run_ssd1315_hil.py --mode retention --port <serial-port> --baud 115200 --out hil_logs --interactive-visual
```

```text
version
cfg
recover
scroll stop
invert 0
allon 0
clear
fill
clear
pattern checker
clear
demo 1
clear
cfg
clear
contrast 1
clear
contrast 127
clear
fill
clear
display off
display on
recover
clear
cfg
```

Interpretation:

- If the serial output reports successful clears and the panel still shows old
  content while `display off` is active, treat that as strong evidence of OLED
  image retention, burn-in, optical residue, or panel damage.
- If old content disappears while `display off` is active but returns after
  `display on` before any new draw/flush, power-cycle safely and record whether
  the artifact persists before drawing.
- If a second panel clears correctly with the same firmware, the original panel
  is suspect.
- If multiple panels show the same stale live pixels after the same commands,
  stop HIL and file a software/addressing bug with the transcript and photos.

Do not use this isolation sequence as proof of hardware validation by itself.
It is a diagnostic step for separating panel artifacts from GDDRAM or flush
behavior.

## 9. Evidence Capture

Required:

- `serial_transcript.txt`.
- `summary.md`.
- Photo or video of checkerboard, clear, fill, contrast levels, flip states, and
  scroll behavior.
- Completed per-command result table.
- Completed hardware matrix.

Recommended:

- Logic analyzer capture showing 7-bit address, I2C control bytes `0x00` and
  `0x40`, page/column addressing, and data payload shape.
- Photo of wiring and reset-pin connection.
- Notes for any skipped fault tests and why they were unsafe or unavailable.
