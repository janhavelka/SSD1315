# SSD1315 Hardware Validation Matrix

Status: no physical SSD1315 hardware validation has been run for this follow-up.

Do not claim field-grade or SSD1306-compatible behavior until representative
hardware has passed this matrix and the exact results are recorded here.

## Required Test Matrix

| Field | Result |
|-------|--------|
| Panel module model | Not run |
| Controller marking, if visible | Not run |
| Resolution | Not run |
| 7-bit I2C address (`0x3C` or `0x3D`) | Not run |
| Supply voltage | Not run |
| Pull-up values | Not run |
| Reset pin connected/not connected | Not run |
| Bus speed | Not run |
| MCU board | Not run |
| Framework: Arduino or ESP-IDF | Not run |
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
monitor
contrast 127
clear
```

Expected operator observations:

- `scan` should show the expected 7-bit address, commonly `0x3C` or `0x3D`.
- `probe` should report ACK/presence only. It does not prove SSD1315 identity.
- `pattern checker`, `clear`, and `fill` require visual confirmation on the
  panel. Do not leave `fill` or `contrast 255` active longer than needed.
- `invert`, `contrast`, `flipx`, and `flipy` should visibly change the panel
  without changing framebuffer contents.
- `scrollh` and `scrollv` should move displayed content. `scroll stop` should
  stop motion; redraw/flush after scroll before judging framebuffer alignment.
- `monitor` is a bounded diagnostic status surface. Arduino `monitor` reports
  the current monitor state, `monitor <ms>` enables periodic output, and
  `monitor 0` disables it. ESP-IDF `monitor` toggles periodic output and
  `monitor 0` disables it while stdin remains active.
- End the smoke sequence with `contrast 127` and `clear` so the panel is not
  left on a high-contrast static image.

Manual or future matrix rows that are not executable CLI commands:

- Missing-display behavior.
- Unplug/replug behavior.
- Reset-pin behavior.
- Long soak result.
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
- Visual validation requires the operator to observe the display and record
  pass/fail evidence in the matrix.
- Hardware reset, bus recovery, and shared-bus locking are application policy.
- If a panel-control command fails and `controlStateDirty()` is true, run
  `recover()` and redraw/flush before judging visual behavior.
- Avoid long high-contrast static images during soak tests unless the product
  intentionally requires them.
