# SSD1315 HIL Runbook

Status: pre-HIL procedure. No physical SSD1315 hardware validation is claimed by
this document.

Use this runbook to produce repeatable serial logs, visual evidence, and matrix
results for `docs/SSD1315_HARDWARE_VALIDATION.md`.

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
6. Run `python tools/run_ssd1315_hil.py --dry-run`.
7. Run the real HIL sequence with the target serial port.
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
python tools/run_ssd1315_hil.py --dry-run
```

Run the HIL smoke sequence and capture logs:

```bash
python tools/run_ssd1315_hil.py --port <serial-port> --baud 115200 --out hil_logs
```

The runner creates a timestamped directory such as
`hil_logs/ssd1315_YYYYMMDD_HHMMSS/` containing:

- `serial_transcript.txt`: raw serial transcript.
- `summary.md`: command summary, host branch/commit metadata, and operator
  checklist.

The runner never flashes firmware and never overwrites an existing log
directory. If `pyserial` is missing, install it with:

```bash
python -m pip install pyserial
```

The runner waits for a CLI prompt, serial-idle interval, or timeout. This is
intentional because the ESP-IDF CLI prints a `>` prompt while the Arduino CLI
logs command responses without a prompt.

## 4. HIL Command Sequence

The runner and matrix use this executable sequence:

```text
version
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
| `scan` | Expected address visible |  | N/A | N/A |  |  |  |
| `probe` | ACK/presence status only |  | N/A | N/A |  |  |  |
| `cfg` | Address/profile/dirty/flush state prints |  | N/A | N/A |  |  |  |
| `selftest` | Serial checks complete without failure |  | Operator observes no unexpected artifact |  |  |  |  |
| `pattern checker` | Command reports OK or no error |  | Checkerboard visible and aligned |  |  |  |  |
| `clear` | Command reports OK or no error |  | Panel fully blank |  |  |  |  |
| `fill` | Command reports OK or no error |  | Panel fully lit briefly |  |  |  |  |
| `invert 1` / `invert 0` | Commands report OK |  | Inversion toggles correctly |  |  |  |  |
| `contrast 1/127/255` | Commands report OK |  | Brightness visibly changes |  |  |  |  |
| `flipx 1/0` | Commands report OK |  | Horizontal orientation changes/restores |  |  |  |  |
| `flipy 1/0` | Commands report OK |  | Vertical orientation changes/restores |  |  |  |  |
| `scrollh right 0 7` | Command reports OK |  | Content scrolls right over pages 0..7 |  |  |  |  |
| `scrollv left 0 7 1` | Command reports OK |  | Content scrolls left with vertical offset |  |  |  |  |
| `scroll stop` | Command reports OK |  | Motion stops |  |  |  |  |
| `recover` | Command reports OK or precise error |  | Display usable after redraw/flush |  |  |  |  |
| `stress 100` | Bounded stress completes |  | No stuck all-on static image |  |  |  |  |
| `stress_mix 100` | Bounded mixed stress completes |  | No tearing or stale-page artifacts |  |  |  |  |
| `monitor 1000` / `monitor 0` | Monitor enables at a bounded interval, then disables |  | N/A | N/A |  |  |  |
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

## 8. Evidence Capture

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
