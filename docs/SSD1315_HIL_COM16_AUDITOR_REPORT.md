# SSD1315 HIL Attempt Report - COM16

Date: 2026-05-31

## Verdict

Blocked. This was not a valid SSD1315 hardware-in-the-loop validation run.

The serial port `COM16` responded, and the I2C scan showed an OLED-like device
at base address `0x3C`, but the firmware running on the board was a BME280
validation CLI, not the SSD1315 validation firmware. Most SSD1315 HIL commands
therefore returned `Unknown command`.

Do not use this run as SSD1315 hardware validation evidence.

## Repository Context

- Repository branch: `hardening/ssd1315-industry-readiness`
- Repository commit: `c644c16a290ce8ebfc784cea76896c9870579d99`
- Worktree before run: clean
- HIL command:

```bash
python tools/run_ssd1315_hil.py --port COM16 --baud 115200 --out hil_logs
```

- HIL log directory:

```text
hil_logs/ssd1315_20260531_174413/
```

## Requested Target

- Serial port: `COM16`
- Requested OLED/base I2C address: `0x3C`
- Baud rate: `115200`

## Evidence From Serial Transcript

The `version` command identified the firmware as BME280:

```text
BME280 library version: 1.5.0
BME280 library full: 1.5.0 (047cbcb, 2026-05-31 17:10:01, clean)
```

The I2C scan did show an OLED-like address:

```text
30: -- -- -- -- -- -- -- -- -- -- -- -- 3C -- -- --
50: -- 51 -- -- -- -- -- -- -- -- -- -- -- -- -- --
70: -- -- -- -- -- -- 76 --
Scan complete. Found 3 device(s).
Common addresses: 0x3C/0x3D=OLED, 0x48-0x4B=ADS1115, 0x51=RV3032, 0x76/0x77=BME280
```

The `cfg` output confirmed the active firmware target was a BME280 at `0x76`,
not the SSD1315 display at `0x3C`:

```text
=== Chip Settings ===
ctrl_hum: 0x01
ctrl_meas: 0x24
config: 0x40
```

SSD1315 display commands were not recognized:

```text
Unknown command: pattern checker
Unknown command: clear
Unknown command: fill
Unknown command: invert 1
Unknown command: contrast 127
Unknown command: scrollh right 0 7
Unknown command: scroll stop
```

The BME280 `probe`, `selftest`, and stress output are not SSD1315 validation
evidence. They only show that the BME280 firmware and device on address `0x76`
were responding.

## Runner Summary Interpretation

The HIL runner exited with a nonzero status. That is expected for this invalid
run because the serial endpoint did not implement the SSD1315 command surface.

Some rows in `summary.md` are also misleading for this run because the BME280
CLI prints text such as `fail=0`; the generic runner flags the token `fail` as
a possible failure. The authoritative finding is simpler: the wrong firmware was
running on `COM16`.

## Hardware Validation Status

- SSD1315 command execution: not run.
- SSD1315 visual validation: not run.
- Clear/fill/ghosting validation: not run.
- Display-off retention test: not run.
- Fault/recovery validation: not run.
- Soak validation: not run.

The OLED address `0x3C` was visible on the bus, but no SSD1315 driver operations
were executed against it.

## Required Next Action

Flash the SSD1315 validation firmware to the board connected to `COM16`, or use
the serial port for the board that already has SSD1315 firmware.

Before rerunning HIL, confirm:

1. `version` prints SSD1315 library/version information, not BME280.
2. `cfg` reports active SSD1315 address `0x3C`, 128x64 geometry, and the
   SSD1315 panel profile.
3. `help` lists commands such as `pattern checker`, `clear`, `fill`,
   `contrast`, `flipx`, `flipy`, `scrollh`, `scrollv`, and `display off/on`.

Then rerun:

```bash
python tools/run_ssd1315_hil.py --port COM16 --baud 115200 --out hil_logs
```

If clear/ghosting is still observed after the correct firmware is running, run
the clear/ghosting isolation sequence in `docs/SSD1315_HIL_RUNBOOK.md` and
capture display-off and power-cycle evidence.
