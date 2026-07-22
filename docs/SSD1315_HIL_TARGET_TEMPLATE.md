# SSD1315 HIL Target Template

Status: operator template only. Filling this template does not by itself
constitute hardware validation.

Copy this template for each physical target before running HIL. Use `unknown`
for fields that are not known.

## Target Identity

| Field | Value |
| --- | --- |
| Operator |  |
| Date/time |  |
| Branch | Record from `git branch --show-current` |
| Commit hash |  |
| Worktree state | clean / dirty / unknown |
| MCU board |  |
| PlatformIO environment | `esp32s3dev` / `esp32s2dev` / other: |
| Framework | Arduino PlatformIO / ESP-IDF |
| Serial port |  |
| Baud rate | `115200` |
| Panel module |  |
| Controller marking, if visible |  |
| Resolution |  |
| SSD1315 7-bit address | `0x3C` / `0x3D` / unknown |
| Supply voltage |  |
| Pull-up values |  |
| I2C bus speed |  |
| Configured `maxWriteBytes` |  |
| Operation mode | cooperative owner API / blocking compatibility CLI |
| Reset pin wiring | connected / not connected / unknown |
| Charge-pump / VCC mode | internal charge pump / external VCC / unknown |
| IREF mode | internal / external resistor / unknown |
| Configured panel profile |  |

## Exact Commands For This Target

Build command:

```bash
python -m platformio run -e <platformio-env>
```

Upload command:

```bash
python -m platformio run -e <platformio-env> --target upload --upload-port <serial-port>
```

HIL runner command:

```bash
python tools/run_ssd1315_hil.py --mode functional --port <serial-port> --baud 115200 --out hil_logs --expect-address <0x3C-or-0x3D> --expect-width 128 --expect-height 64 --expect-controller SSD1315 --interactive-visual
```

Optional smoke-only serial command:

```bash
python tools/run_ssd1315_hil.py --mode smoke --port <serial-port> --baud 115200 --out hil_logs --expect-address <0x3C-or-0x3D> --serial-only
```

Retention isolation command:

```bash
python tools/run_ssd1315_hil.py --mode retention --port <serial-port> --baud 115200 --out hil_logs --interactive-visual
```

One-hour unattended serial soak, after recording the exact target metadata:

```bash
python tools/run_ssd1315_hil.py --mode soak --port <serial-port> --baud 115200 --out hil_logs --expect-address <0x3C-or-0x3D> --expect-width 128 --expect-height 64 --expect-controller SSD1315 --expect-panel-profile <configured-profile> --expect-commit <firmware-sha> --operator <operator> --board <exact-board> --panel <exact-panel> --supply-voltage <measured-voltage> --pullups <values> --reset-wired <yes-or-no> --bus-speed <speed> --strict --serial-only --soak-ops 500 --soak-duration-hours 1
```

Do not invent strict metadata. If controller marking, panel identity, electrical
values, or reset wiring are unknown, record `unknown`, omit `--strict`, and
treat the run as partial serial evidence.

ESP-IDF build command, if this target uses ESP-IDF:

```bash
idf.py -C examples/espidf_basic set-target <esp32s2-or-esp32s3>
idf.py -C examples/espidf_basic build
```

ESP-IDF flash/monitor command, if this target uses ESP-IDF:

```bash
idf.py -C examples/espidf_basic -p <serial-port> flash monitor
```

## Photo / Video Checklist

- [ ] Wiring overview, including power, SDA, SCL, and reset if connected.
- [ ] `pattern checker`: checkerboard visible and aligned.
- [ ] `clear`: panel fully blank.
- [ ] `fill`: panel fully lit briefly, not left static.
- [ ] `invert 1` and `invert 0`: inversion toggles and restores.
- [ ] `contrast 1`, `contrast 127`, `contrast 255`: brightness changes.
- [ ] `flipx 1` and `flipx 0`: horizontal orientation changes and restores.
- [ ] `flipy 1` and `flipy 0`: vertical orientation changes and restores.
- [ ] `scrollh right 0 7`: content scrolls right.
- [ ] `scrollv left 0 7 1`: content scrolls left with vertical offset.
- [ ] `scroll stop`: motion stops.
- [ ] `recover`: display usable after redraw/flush.

## Logic Analyzer Checklist

Use when available. Leave unchecked if not captured.

- [ ] 7-bit address is `0x3C` or `0x3D`.
- [ ] Command control byte `0x00` appears on command streams.
- [ ] Data control byte `0x40` appears on GDDRAM data streams.
- [ ] Page and column address commands match expected 128x64 layout.
- [ ] Full-frame or representative partial flush payload is visible.
- [ ] Scroll setup command sequence is captured.
- [ ] NACK/missing-display behavior is captured, if safely tested.

## Evidence Paths

| Evidence | Path / ID |
| --- | --- |
| HIL log directory |  |
| `serial_transcript.txt` |  |
| `summary.md` |  |
| `results.json` |  |
| `results.csv` |  |
| `operator_visual_checklist.md` |  |
| `hardware_matrix_fragment.md` |  |
| Photos/video |  |
| Logic analyzer capture |  |
| Operator notes |  |
