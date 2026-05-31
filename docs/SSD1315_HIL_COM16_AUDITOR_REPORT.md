# SSD1315 HIL Attempt Report - COM16

Date: 2026-05-31

## Verdict

Serial HIL command execution passed after the SSD1315 firmware was re-uploaded.
The final COM16 runner attempt exited with status `0`. The OLED responded at
the base 7-bit address `0x3C`, the firmware identified itself as SSD1315, every
SSD1315 command in the runner returned an `OK`/clean status in the raw
transcript, and final `cfg` reported no dirty, flushing, or control-dirty state.

This is not yet complete field hardware validation because operator visual
checks, photos/video, fault tests, display-off ghosting isolation, and soak
evidence were not captured in this report.

## Repository Context

- Repository branch: `hardening/ssd1315-industry-readiness`
- Firmware commit reported by the board: `a15bea3` / `a15bea3bf4140841759d1a4e7a27f054c8d4271b`
- Host repository worktree before attempt 2: clean
- Host repository worktree during final attempt: dirty only because this report
  and the HIL runner parser fix were being prepared after attempt 2
- Serial port: `COM16`
- Baud rate: `115200`
- Requested OLED/base I2C address: `0x3C`

## Attempt 1 - Blocked Wrong Firmware

Log directory:

```text
hil_logs/ssd1315_20260531_174413/
```

The first COM16 run was blocked because the board was still running a BME280
validation CLI. It did show an OLED-like ACK at `0x3C`, but the active firmware
reported BME280 version/configuration and returned `Unknown command` for
SSD1315 commands such as `clear`, `fill`, `contrast`, and `scrollh`.

That first run is not SSD1315 validation evidence.

## Attempt 2 - SSD1315 Firmware, Parser False Positive

Command:

```bash
python tools/run_ssd1315_hil.py --port COM16 --baud 115200 --out hil_logs
```

Log directory:

```text
hil_logs/ssd1315_20260531_174858/
```

Generated files:

```text
hil_logs/ssd1315_20260531_174858/serial_transcript.txt
hil_logs/ssd1315_20260531_174858/summary.md
```

The HIL runner process exited with status `1`, but the raw transcript showed this
was a runner classification false-positive: it flagged generic counters such as
`FAIL:0`, `fail=0`, and `Failures: 0` in otherwise successful command summaries.

The runner was then fixed to strip ANSI color codes before classification and to
ignore zero-valued fail counters.

## Attempt 3 - Final Clean Runner Result

Command:

```bash
python tools/run_ssd1315_hil.py --port COM16 --baud 115200 --out hil_logs
```

Log directory:

```text
hil_logs/ssd1315_20260531_175152/
```

Generated files:

```text
hil_logs/ssd1315_20260531_175152/serial_transcript.txt
hil_logs/ssd1315_20260531_175152/summary.md
```

Runner result: exit status `0`.

The final summary still marks visual commands as `OPERATOR_CHECK_REQUIRED`,
which is expected. Serial-only commands were `PASS` or `REVIEW_REQUIRED` where
the runner intentionally requires human review of scan/probe text. No command
was classified as `FAIL`.

## Firmware Identity

The `version` command identified the uploaded firmware as SSD1315:

```text
Framework: Arduino
Build target: "Espressif ESP32-S3-DevKitC-1-N8 (8 MB QD, No PSRAM)"
Example firmware build: May 31 2026 17:48:12
SSD1315 library version: 1.2.0
SSD1315 library full: 1.2.0 (a15bea3, 2026-05-31 17:47:51, clean)
SSD1315 library commit: a15bea3 (clean)
Controller profile: SSD1315
Panel profile: example-default-128x64-internal-charge-pump
Active I2C address: 0x3C
Geometry: 128x64 pages=8 pageBufferPages=8
```

## I2C Scan

The bus scan showed `0x3C` plus other devices:

```text
30: -- -- -- -- -- -- -- -- -- -- -- -- 3C -- -- --
50: -- 51 -- -- -- -- -- -- -- -- -- -- -- -- -- --
70: -- -- -- -- -- -- 76 --
Scan complete. Found 3 device(s).
Common addresses: 0x3C/0x3D=OLED, 0x48-0x4B=ADS1115, 0x51=RV3032, 0x76/0x77=BME280
```

`probe` returned `OK` and did not change health counters:

```text
Probe result: OK (code=0: OK)
Health: state=READY online=true controlDirty=false consecFail=0 ok=1 fail=0
```

As documented elsewhere, this is ACK/presence evidence only. It does not prove
controller identity beyond the firmware's configured SSD1315 target.

## Command Results From Raw Transcript

Serial command execution:

| Command | Raw serial result |
| --- | --- |
| `version` | SSD1315 firmware identity printed |
| `scan` | `0x3C` present |
| `probe` | `OK` |
| `cfg` | SSD1315, `128x64`, address `0x3C`, clean state |
| `selftest` | `pass=20 fail=0 skip=0` |
| `pattern checker` | `pattern: OK` |
| `clear` | `clear+flush: OK` |
| `fill` | `fill: OK` |
| `invert 1` | `invert: OK` |
| `invert 0` | `invert: OK` |
| `contrast 1` | `setContrast: OK` |
| `contrast 127` | `setContrast: OK` |
| `contrast 255` | `setContrast: OK` |
| `flipx 1` | `flipx: OK` |
| `flipx 0` | `flipx: OK` |
| `flipy 1` | `flipy: OK` |
| `flipy 0` | `flipy: OK` |
| `scrollh right 0 7` | `scrollh: OK` |
| `scrollv left 0 7 1` | `scrollv: OK` |
| `scroll stop` | `scroll stop: OK` |
| `recover` | `Recover result: OK` |
| `stress 100` | `Successes: 100`, `Failures: 0` |
| `stress_mix 100` | `Successes: 100`, `Failures: 0` |
| `monitor 1000` | monitor enabled and health printed |
| `monitor 0` | monitor disabled |
| final `contrast 127` | `setContrast: OK` |
| final `clear` | `clear+flush: OK` |
| final `cfg` | clean state |

Final `cfg` state:

```text
width=128 height=64 addr=0x3C
controllerProfile=SSD1315 panelProfile=example-default-128x64-internal-charge-pump
clearOnBegin=yes clearOnRecover=yes scrollActive=no
initialized=yes sleeping=no flushing=no pageIterating=no dirty=no controlDirty=no
invert=no flipX=no flipY=no sleep=no allOn=no zoom=no
```

## Operator Visual Status

Not recorded in this report.

The runner marks the following as operator-required visual checks:

- `selftest`
- `pattern checker`
- `clear`
- `fill`
- `invert 1`
- `invert 0`
- `contrast 1`
- `contrast 127`
- `contrast 255`
- `flipx 1`
- `flipx 0`
- `flipy 1`
- `flipy 0`
- `scrollh right 0 7`
- `scrollv left 0 7 1`
- `scroll stop`
- `recover`
- `stress 100`
- `stress_mix 100`
- final `contrast 127`
- final `clear`

These must be filled by the operator with photos/video and observations before
the hardware matrix can be considered complete.

## Clear/Ghosting Status

The serial path for `clear`, `fill`, and final `clear` returned `OK`; final
`cfg` reported no dirty state. This supports the software command path, but it
does not prove the physical OLED was visually clean.

If ghosting, stale content, or burn-in-like artifacts remain visible, run the
clear/ghosting isolation sequence from `docs/SSD1315_HIL_RUNBOOK.md` and record:

- whether the artifact remains after `display off`;
- whether it persists across a safe power cycle before drawing;
- whether a second panel behaves differently;
- photos/video before and after `display off`.

## Hardware Validation Status

- SSD1315 serial command execution: passed on final attempt.
- SSD1315 visual validation: not recorded.
- Clear/fill/ghosting validation: serial path passed; visual state not recorded.
- Display-off retention test: not run.
- Fault/recovery validation: not run.
- Long soak validation: not run.

## Auditor Conclusion

The final COM16 run is valid serial HIL evidence for the SSD1315 command
surface at address `0x3C`. It is not full hardware-release evidence until the
operator completes the visual matrix, ghosting isolation if needed,
fault/recovery checks, and soak evidence.
