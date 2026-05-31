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

## Validation Commands

Use the Arduino bring-up CLI or native ESP-IDF CLI as appropriate:

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
contrast 0
contrast 127
contrast 255
flipx 1
flipx 0
flipy 1
flipy 0
scrollh right 0 7
scrollv left 0 7 1
recover
stress 100
stress_mix 100
monitor
```

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
- Hardware reset, bus recovery, and shared-bus locking are application policy.
- If a panel-control command fails and `controlStateDirty()` is true, run
  `recover()` and redraw/flush before judging visual behavior.
- Avoid long high-contrast static images during soak tests unless the product
  intentionally requires them.
