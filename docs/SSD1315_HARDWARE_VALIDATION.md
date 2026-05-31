# SSD1315 Hardware Validation Matrix

Status: no physical SSD1315 hardware validation has been run for this follow-up.

Do not claim field-grade or SSD1306-compatible behavior until representative
hardware has passed this matrix and the exact results are recorded here.

Use `docs/SSD1315_HIL_RUNBOOK.md` for the executable preflight, build/flash,
serial logging, per-command result table, and evidence capture procedure.
Copy `docs/SSD1315_HIL_TARGET_TEMPLATE.md` for target-specific board, panel,
command, and evidence fields.

Document ownership:

- Runbook: the operator procedure.
- Target template: per-board and per-panel setup before the run.
- This matrix: the committed result record after real hardware is tested.

Use `unknown` rather than guessing. Leave untested rows as `Not run`.

## Required Test Matrix

| Field | Result |
|-------|--------|
| Operator | Not run |
| Date/time | Not run |
| Branch | Not run |
| Commit hash | Not run |
| Worktree state | Not run |
| Firmware framework | Not run |
| Firmware build target | Not run |
| Serial port | Not run |
| Baud rate | Not run |
| HIL log directory | Not run |
| Photo/video evidence path | Not run |
| Logic analyzer capture path | Not run |
| Panel module model | Not run |
| Configured driver panel profile | Not run |
| Controller marking, if visible | Not run |
| Resolution | Not run |
| 7-bit I2C address (`0x3C` or `0x3D`) | Not run |
| Supply voltage | Not run |
| Pull-up values | Not run |
| Reset pin connected/not connected | Not run |
| Bus speed | Not run |
| MCU board | Not run |
| Charge pump mode and voltage | Not run |
| IREF mode: external resistor or internal current | Not run |
| COM pins / segment remap / COM scan direction | Not run |
| Init analog defaults: contrast, clock, precharge, VCOMH | Not run |
| Init result | Not run |
| Full-frame flush | Not run |
| Partial update | Not run |
| Clear/fill/checkerboard | Not run |
| Invert/contrast/orientation | Not run |
| Scroll, if supported by product UI | Not run |
| Recover after forced failure | Not run |
| Missing-display behavior | Not run |
| Unplug/replug behavior | Not run |
| Reset-pin behavior | Not run |
| Long soak result | Not run |
| Notes/screenshots/logic analyzer captures | Not run |

## Executable CLI Smoke Commands

Use the Arduino bring-up CLI or native ESP-IDF CLI as appropriate. These
commands are intentionally limited to command surfaces implemented by both
examples. They do not replace operator visual inspection.

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

Expected operator observations:

- `scan` should show the expected 7-bit address, commonly `0x3C` or `0x3D`.
- `probe` should report ACK/presence only. It does not prove SSD1315 identity.
- `pattern checker`, `clear`, and `fill` require visual confirmation on the
  panel. Do not leave `fill` or `contrast 255` active longer than needed.
- `invert`, `contrast`, `flipx`, and `flipy` should visibly change the panel
  without changing framebuffer contents.
- `scrollh` and `scrollv` should move displayed content. While scroll is
  active, framebuffer flush is blocked by the driver. `scroll stop` should stop
  motion and mark the framebuffer dirty; redraw/flush after scroll before
  judging framebuffer alignment.
- `monitor` is a bounded diagnostic status surface. For repeatable HIL logs,
  prefer `monitor 1000` followed by `monitor 0`. This keeps Arduino periodic
  output bounded at a sane interval and then disables periodic output.
- End the smoke sequence with `contrast 127` and `clear` so the panel is not
  left on a high-contrast static image.
- `selftest` is serial/software evidence only. It may change display state, but
  its PASS output does not prove visual correctness.

## Per-Command Result Record

Fill this table from `serial_transcript.txt`, `summary.md`, and operator visual
evidence. Mark visual rows as pass only after observing the panel.

| Command | Serial result | Visual result | Pass/Fail | Evidence path or ID | Notes |
| --- | --- | --- | --- | --- | --- |
| `version` | Not run | N/A | Not run |  |  |
| `scan` | Not run | N/A | Not run |  |  |
| `probe` | Not run | N/A | Not run |  |  |
| `cfg` | Not run | N/A | Not run |  |  |
| `selftest` | Not run | Operator check | Not run |  |  |
| `pattern checker` | Not run | Not run | Not run |  |  |
| `clear` | Not run | Not run | Not run |  |  |
| `fill` | Not run | Not run | Not run |  |  |
| `invert 1` | Not run | Not run | Not run |  |  |
| `invert 0` | Not run | Not run | Not run |  |  |
| `contrast 1` | Not run | Not run | Not run |  |  |
| `contrast 127` | Not run | Not run | Not run |  |  |
| `contrast 255` | Not run | Not run | Not run |  |  |
| `flipx 1` | Not run | Not run | Not run |  |  |
| `flipx 0` | Not run | Not run | Not run |  |  |
| `flipy 1` | Not run | Not run | Not run |  |  |
| `flipy 0` | Not run | Not run | Not run |  |  |
| `scrollh right 0 7` | Not run | Not run | Not run |  |  |
| `scrollv left 0 7 1` | Not run | Not run | Not run |  |  |
| `scroll stop` | Not run | Not run | Not run |  |  |
| `recover` | Not run | Not run | Not run |  |  |
| `stress 100` | Not run | Operator check | Not run |  |  |
| `stress_mix 100` | Not run | Operator check | Not run |  |  |
| `monitor 1000` | Not run | N/A | Not run |  |  |
| `monitor 0` | Not run | N/A | Not run |  |  |
| final `contrast 127` | Not run | Not run | Not run |  |  |
| final `clear` | Not run | Not run | Not run |  |  |
| final `cfg` | Not run | N/A | Not run |  |  |

Manual, fault, or soak rows that are not completed by the smoke script alone:

- Missing-display behavior.
- Unplug/replug behavior.
- Reset-pin behavior.
- Long soak result. Run and record bounded commands such as `stress 1000`,
  `stress_mix 500`, `contrast 127`, `clear`, and `cfg`.
- Screenshot and logic-analyzer capture references.

For ESP-IDF builds, verify both targets before hardware runs:

```bash
idf.py -C examples/espidf_basic set-target esp32s3
idf.py -C examples/espidf_basic build
idf.py -C examples/espidf_basic fullclean
idf.py -C examples/espidf_basic set-target esp32s2
idf.py -C examples/espidf_basic build
```

## Notes

- `probe` is ACK-only. It does not prove the controller is SSD1315.
- The CLI `recover`/`reset` path is software-only. Hardware `RES#` pulse
  timing and power sequencing must be validated manually by board firmware or
  fixture code that owns the reset GPIO.
- Visual validation requires the operator to observe the display and record
  pass/fail evidence in the matrix.
- Hardware reset, bus recovery, and shared-bus locking are application policy.
- If a panel-control command fails and `controlStateDirty()` is true, run
  `recover()` and redraw/flush before judging visual behavior.
- Avoid long high-contrast static images during soak tests unless the product
  intentionally requires them.
