# SSD1315 Chunk 04 Hardware Validation Report

Date: 2026-05-31

## Branch and Starting State

- Branch: `hardening/ssd1315-industry-readiness`
- Starting commit: `77a7e04 test: align SSD1315 init and panel contracts with datasheet`
- Starting worktree: clean
- Chunk scope: execute physical SSD1315 hardware validation and fill the
  hardware matrix.

## Outcome

Hardware validation is blocked in this environment. No representative SSD1315
panel setup was confirmed, no operator visual observations were available, and
no SSD1315 validation firmware was flashed. The hardware matrix remains
unfilled and must not be treated as passed.

Two Espressif USB serial ports were visible, but neither was a confirmed
SSD1315 validation target:

| Port | Observation |
| --- | --- |
| `COM5` | Espressif USB serial device, no response to `version` at 115200 baud. |
| `COM18` | Espressif USB serial device running unrelated `tunnelmonitor 0.3.0` firmware; not overwritten. |

The `COM18` firmware reported `lib_ssd1315: 1.2.0 [header]`, but that is not a
hardware validation run of this repository's SSD1315 CLI and does not prove
panel behavior.

## Hardware Setup

| Field | Result |
| --- | --- |
| Panel module model | unknown |
| Visible controller marking | unknown |
| Resolution | unknown |
| 7-bit I2C address | unknown |
| Supply voltage | unknown |
| Pull-up values | unknown |
| Reset pin connected/not connected | unknown |
| Bus speed | unknown |
| MCU board | unknown |
| Framework | unknown |
| Firmware/library version under test | Not flashed |
| Git commit hash | `77a7e04` |
| Internal charge pump or external panel VCC | unknown |

## Preflight Checks Run

| Command | Result |
| --- | --- |
| `git status --short` | Clean at chunk start |
| `git branch --show-current` | `hardening/ssd1315-industry-readiness` |
| `git log --oneline -8` | Latest commit `77a7e04` |
| `python tools/check_core_timing_guard.py` | Pass |
| `python tools/check_cli_contract.py` | Pass before report update and pass after adding `version` guard |
| `python tools/check_idf_example_contract.py` | Pass |
| `python scripts/generate_version.py check` | Pass |
| `python -m platformio test -e native` | Pass, 54 tests |
| `python -m platformio run -e esp32s3dev` | Pass |
| `python -m platformio run -e esp32s2dev` | First run failed while compiling Arduino framework object `esp32-hal-spi.c.o` with no compiler diagnostic; immediate rerun passed |
| `idf.py --version` | Not available: PowerShell reported `idf.py` was not recognized |

Pure ESP-IDF local builds were not run because `idf.py` is not on `PATH`.

## Serial Probing

The local serial inventory showed Bluetooth serial ports plus two Espressif USB
serial devices:

- `COM5`: `USB VID:PID=303A:0002`
- `COM18`: `USB VID:PID=303A:1001`

A non-flashing serial probe sent `version` at 115200 baud:

- `COM5`: no response.
- `COM18`: responded with unrelated `tunnelmonitor 0.3.0` firmware metadata.

No upload was attempted because the target board, panel wiring, and permission
to overwrite the existing firmware were not confirmed.

## Commands Run On SSD1315 Hardware

None. The required smoke sequence was not run on representative SSD1315
hardware.

## Visual Observations

Not run. Without operator observation or a camera view of the panel, serial
logs alone cannot prove checkerboard output, clear/fill behavior, contrast
luminance, orientation, scroll motion, partial update correctness, or visual
artifacts.

## Fault Tests

Not run. Missing-display, unplug/replug, reset-pin, and forced-failure tests
require a confirmed hardware setup and safe operator-controlled power/reset
procedure.

## Soak Result

Not run. No bounded OLED-safe soak was executed.

## Bugs Found

- Documentation mismatch: Prompt 04 requires `version` in the hardware smoke
  sequence, but `docs/SSD1315_HARDWARE_VALIDATION.md` did not list it. The
  document and CLI contract guard were updated to include `version`.

No core driver bug was found in this chunk.

## Remaining Hardware Gaps

- Confirm the target MCU board and COM port.
- Confirm the panel module, visible marking, supply, pullups, bus speed, reset
  wiring, and internal-charge-pump/external-VCC profile.
- Flash the current SSD1315 validation CLI only after the target is confirmed.
- Capture full serial logs and operator visual observations for every visual
  command.
- Run safe missing-display, unplug/replug, reset-pin, and soak tests only with
  a controlled hardware procedure.

## Verdict

Blocked for hardware validation. This chunk records preflight and environment
findings only; it does not establish release or field readiness.
