# SSD1315 Hardware Validation Matrix

Status: partial serial HIL command evidence exists, but complete hardware
validation is still open.

Do not claim field-grade or SSD1306-compatible behavior until representative
hardware has passed this matrix and the exact results are recorded here.

Use `docs/SSD1315_HIL_RUNBOOK.md` for the executable preflight, build/flash,
serial logging, per-command result table, and evidence capture procedure.
Copy `docs/SSD1315_HIL_TARGET_TEMPLATE.md` for target-specific board, panel,
command, and evidence fields.
`tools/run_ssd1315_hil.py` can generate a `hardware_matrix_fragment.md` with
serial results and operator-required placeholders.

Document ownership:

- Runbook: the operator procedure.
- Target template: per-board and per-panel setup before the run.
- This matrix: the committed result record after real hardware is tested.

Use `unknown` rather than guessing. Leave untested rows as `Not run`.

## Recorded Serial HIL Evidence

Local COM16 and COM17 serial HIL runs were reported during earlier hardening,
but their raw artifact logs are not committed. They remain historical partial
bring-up evidence only.

The maintained serial evidence is the COM29 report:
`docs/reports/hil-validation-COM29-20260623.md`. It records an ESP32-S2 target
using PlatformIO `esp32s2dev`, Arduino framework, address `0x3C`, 128x64
geometry, SDA GPIO8, SCL GPIO9, 400 kHz, and panel profile
`example-default-128x64-internal-charge-pump`. The report includes serial
functional, benchmark, retention, an 8-hour serial soak, and post-soak serial
cleanup evidence. The 8-hour soak ran 10575 commands, including 1511
`stress_mix 500` blocks for 755500 mixed operations, with 0 serial FAIL rows.

COM29 still lacks committed operator visual pass/fail evidence, photos/video,
safe physical fault injection, reset-pin validation, logic-analyzer capture,
known panel module model, supply voltage, and pull-up values. Treat it as
partial serial/device evidence, not field validation.

Closeout serial-only HIL was rerun on 2026-06-23 after the `3.0.0` follow-up
changes were built and uploaded to COM29. The upload wrote and verified flash,
then hit the known post-upload reset/COM re-enumeration error; the firmware
subsequently responded on COM29. Local artifacts:
`hil_logs\ssd1315_20260623_113231` (smoke),
`hil_logs\ssd1315_20260623_113248` (functional),
`hil_logs\ssd1315_20260623_113316` (retention), and
`hil_logs\ssd1315_20260623_113336` (short soak, `--soak-ops 100`). These runs
passed serial classification against the dirty local worktree and do not add
visual, reset, fault-injection, or logic-analyzer evidence.

## Required Test Matrix

| Field | Result |
|-------|--------|
| Operator | Not recorded for COM29 serial-only run |
| Date/time | Partial serial: 2026-06-23 Europe/Prague report; soak ran 2026-06-22T21:00:29+02:00 to 2026-06-23T05:00:37+02:00 |
| Branch | Partial serial: `main` |
| Commit hash | Partial serial report commit: `59759a80ebb474401ff3e09e17cfe42186ce3a97`; closeout HIL host commit `7949afcef0bb64560d511612cdbb3a0a86911b7b` with dirty worktree |
| Worktree state | Partial serial: dirty for closeout HIL because follow-up changes were uncommitted |
| Firmware framework | Partial serial: Arduino framework |
| Firmware build target | Partial serial: PlatformIO `esp32s2dev` |
| Serial port | Partial serial: `COM29` |
| Baud rate | Partial serial: 115200 |
| HIL log directory | Partial serial: report paths under `hil_logs\ssd1315_20260622_*`; closeout local paths `hil_logs\ssd1315_20260623_113231`, `113248`, `113316`, `113336`; raw `hil_logs` artifacts are local/untracked unless separately archived |
| Serial HIL command sequence | Partial serial PASS: COM29 smoke/functional/benchmark/retention/8-hour soak/post-soak functional cleanup from report; closeout smoke/functional/retention/short-soak serial-only rerun passed |
| Photo/video evidence path | Not run |
| Logic analyzer capture path | Not run |
| Panel module model | Unknown / not recorded |
| Configured driver panel profile | Partial serial: `example-default-128x64-internal-charge-pump` |
| Controller marking, if visible | Not run |
| Resolution | Partial serial: 128x64 |
| 7-bit I2C address (`0x3C` or `0x3D`) | Partial serial: `0x3C` ACKed; `probe()` proves ACK only |
| Supply voltage | Unknown / not recorded |
| Pull-up values | Unknown / not recorded |
| Reset pin connected/not connected | Unknown / no reset-pin test run |
| Bus speed | Partial serial: 400 kHz |
| MCU board | Partial serial: ESP32-S2; upload detected ESP32-S2FH4 rev v1.0 / ESP32-S2-Saola-1 firmware wording |
| Charge pump mode and voltage | Not run |
| IREF mode: external resistor or internal current | Not run |
| COM pins / segment remap / COM scan direction | Not run |
| Init analog defaults: contrast, clock, precharge, VCOMH | Not run |
| Init result | Partial serial: firmware boot/config/probe/selftest passed on COM29 |
| Full-frame flush | Partial serial: exercised by clear/fill/stress paths; no visual or logic-analyzer proof |
| Partial update | Partial serial: exercised by stress/stress_mix paths; no visual or logic-analyzer proof |
| Clear/fill/checkerboard | Partial serial only; visual evidence not run |
| Invert/contrast/orientation | Partial serial only; visual evidence not run |
| Scroll, if supported by product UI | Partial serial only; visual motion evidence not run |
| Recover after forced failure | Partial serial software recover command passed; no physical/reset fault injection |
| Missing-display behavior | Not run |
| Unplug/replug behavior | Not run |
| Reset-pin behavior | Not run |
| Long soak result | Partial serial: COM29 8-hour serial soak passed, 755500 mixed ops, 0 serial failures; closeout rerun included short `--soak-ops 100` serial soak only; visual/fault/reset evidence not run |
| Notes/screenshots/logic analyzer captures | COM29 report committed; screenshots/video/logic-analyzer captures not run |

## Executable CLI Smoke Commands

Use the Arduino bring-up CLI or native ESP-IDF CLI as appropriate. These
commands are intentionally limited to command surfaces implemented by both
examples. They do not replace operator visual inspection.

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

Expected operator observations:

- `scan` should show the expected 7-bit address, commonly `0x3C` or `0x3D`.
- `probe` should report ACK/presence only. It does not prove SSD1315 identity.
- `telemetry` should report nonzero free heap, minimum free heap, uptime, loop
  heartbeat, and reset reason. Treat zero heap or missing fields as a failure
  or firmware/version mismatch.
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
- The HIL runner may report `SERIAL_PASS_OPERATOR_REQUIRED` for visual commands.
  Treat that as serial evidence only until photos/video or operator notes are
  attached.

## Per-Command Result Record

Fill this table from `serial_transcript.txt`, `summary.md`, and operator visual
evidence. Mark visual rows as pass only after observing the panel.

| Command | Serial result | Visual result | Pass/Fail | Evidence path or ID | Notes |
| --- | --- | --- | --- | --- | --- |
| `version` | Not run | N/A | Not run |  |  |
| `telemetry` | Not run | N/A | Not run |  |  |
| `scan` | Not run | N/A | Not run |  |  |
| `probe` | Not run | N/A | Not run |  |  |
| `cfg` | Not run | N/A | Not run |  |  |
| `selftest` | Not run | N/A; software/serial evidence only | Not run |  |  |
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
| post-monitor `telemetry` | Not run | N/A | Not run |  |  |
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

The runner can create these evidence files automatically:

- `serial_transcript.txt`
- `summary.md`
- `results.json`
- `results.csv`
- `metadata.json`
- `operator_visual_checklist.md`
- `hardware_matrix_fragment.md`
- parsed cfg snapshots and health/failure summaries

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
- If `clear` appears to leave a ghost image, run the clear/ghosting isolation
  sequence in `docs/SSD1315_HIL_RUNBOOK.md` before accepting or rejecting the
  panel. Record whether the artifact remains after `display off`, after a safe
  power cycle, and on a second panel if one is available.
- A visible ghost with the display off or before any post-boot draw is hardware
  evidence, not proof of live GDDRAM corruption. A repeated stale live-pixel
  pattern on multiple panels is a software/addressing fault until proven
  otherwise.
