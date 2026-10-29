# SSD1315 Hardware Validation Ledger

This ledger preserves dated hardware evidence: partial serial HIL on an
ESP32-S3 N16R8 at COM21 on 2026-07-31, plus earlier COM21 and COM29 runs.
Source revisions, configuration, artifacts, measured outcomes and coverage
limits identify what each run establishes; the records do not qualify later
code automatically.

The executable procedure and per-target evidence form live in
`docs/SSD1315_HIL_RUNBOOK.md`. This ledger records completed observations.

## Recorded Evidence

### COM21, ESP32-S3 N16R8, 2026-07-31

Accepted as partial serial HIL for the configured 128x64 device at address
`0x3C`. Functional/benchmark/extended commands, the measured one-hour soak and
post-soak serial reattachment passed for the source and setup below. Operator
visual rows remained operator-required.

| Field | Recorded value |
| --- | --- |
| Repository branch / base | `main`, `d29fefc624a76db56354c72b6b8e85c7225279e1` |
| Firmware source state | Base `d29fefc` plus the dirty pioarduino migration diff recorded in each artifact; no clean-commit claim |
| Library package version reported by firmware | `4.0.1` |
| PlatformIO / platform | Core 6.1.19; pioarduino 55.03.311 |
| Arduino / IDF libraries | Arduino-ESP32 3.3.11; ESP-IDF libraries `5.5.5+sha.b774170ff46` |
| Build tools | GCC 14.2.0+20260121; esptool 5.3.0; GDB 17.1.0+20260402 |
| Environment / board | `esp32s3dev`; `esp32-s3-devkitc1-n16r8` |
| Board memory configuration | 16 MB flash / 8 MB octal PSRAM, `qio_opi`, `default_16MB.csv` |
| Runtime identity | ESP32-S3 revision 2; 16,777,216 flash bytes; PSRAM ready, 8,388,608 bytes |
| Upload detection / verification | ESP32-S3 QFN56 rev 0.2, embedded 8 MB PSRAM, 16 MB flash, USB Serial/JTAG; flash readback verified |
| Serial target | `COM21`, 115200 baud; DTR/RTS deasserted before open |
| I2C configuration | SDA GPIO8, SCL GPIO9, configured 400 kHz; no logic-analyzer speed measurement |
| Display configuration | 128x64, `0x3C`, `example-default-128x64-internal-charge-pump` |
| Panel/controller identity | Unknown; ACK and successful writes do not identify the controller |
| Supply / pull-ups / IREF / reset wiring | Unknown / unknown / unknown / unknown |
| Operation mode | Arduino blocking compatibility diagnostic CLI |
| Operator evidence | Serial-only; no visual pass/fail, photo or video captured |

The shared-bus scan returned ACKs at `0x3C`, `0x41`, `0x50` and `0x51`.
Display commands addressed only `0x3C`; those ACKs establish no device identity.

Raw artifacts are local under ignored
`hil_logs/20260731-pioarduino-55.03.311/`. Accepted runs contain exact
argv/metadata, transcripts, JSON/CSV results, parsed identity/configuration,
health deltas, statistics and operator/validation fragments.

| Run | Artifact directory | Recorded result / coverage |
| --- | --- | --- |
| Rejected initial smoke | `ssd1315_20260731_201217` | FAIL 1/1: no command response after the checked redundant `Wire.setClock()` path halted setup |
| Corrected smoke | `ssd1315_20260731_202111` | PASS 7/7; exact firmware/MCU/memory identity, four-device scan, ACK-only probe, self-test 20/20, clean config |
| Combined all plan | `ssd1315_20260731_202128` | Serial PASS: 32 PASS, 41 operator-required, 0 fail; functional and retention cleanup, one 500-operation soak cycle, clean final config |
| Benchmark | `ssd1315_20260731_202235` | Serial PASS: 5 PASS, 3 operator-required, 0 fail; stress 1000/1000, mixed 100/100, flush 100/100, burst 1000/1000 |
| Arduino extended | `ssd1315_20260731_202257` | Serial PASS: 52 PASS, 25 operator-required, 0 fail; all 77 commands and clean final config |
| One-hour soak | `ssd1315_20260731_202349` | PASS / COMPLETE: 194 batches, 97,000 operations, measured 3,612.265 soak seconds, clean final config |
| Post-soak smoke / reattach | `ssd1315_20260731_212434` | PASS 7/7 on a new serial open |
| Post-soak Arduino extended | `ssd1315_20260731_212442` | Serial PASS: 52 PASS, 25 operator-required, 0 fail; all 77 commands again, clean final config |

The rejected runner capture contains the missing response. A separate esptool
reset/read diagnostic established that setup had stopped at `I2C init failed`
after checking a redundant `Wire.setClock()` return; that boot log is not
content of the rejected runner directory.

The duration run took 3,616.359 host seconds and accepted 1,168 command rows:
586 serial PASS and 582 serial-pass/operator-required, with no failure or
review-required row. Its 389 telemetry samples showed strictly increasing
uptime and loop heartbeat, constant reset reason 11, constant 8,724,752-byte
free heap and 8,719,440-byte minimum free heap, zero driver failures, zero
serial interruptions/retries and no restart. Final state was initialized,
awake, online, not flushing or scrolling, with clean framebuffer/control state.

Coverage limits for this run:

- No operator visual evidence establishes clear/fill/pattern/orientation/scroll
  appearance or absence of OLED retention. Panel/controller identity, supply,
  pull-ups, IREF and reset wiring remain unknown.
- Missing-display, unplug/replug, injected NACK/timeout/bus faults, power-cycle,
  thermal and logic-analyzer tests were not run; fixture wiring and safe fault
  authority were unspecified. Configured bus speed was not electrically measured.
- Arbitrary raw controller-byte combinations were excluded. Coverage consists
  of the diagnostic commands and plans listed above.
- Native ESP-IDF hardware and the cooperative `attach/start/poll/result` owner,
  one-attempt scheduling, cancellation and result lifetime were not qualified
  by this blocking diagnostic. They need a dedicated owner fixture.
- The default profile sends SSD1315-specific `SET_IREF`; these results do not
  establish SSD1306 compatibility or field-grade hardware qualification.

### COM21, ESP32-S3, 2026-07-22

The previous-stack evidence used diagnostic firmware from revisions
`5c84e3496d9f0274689940636bf4efc7935100f8` and
`074463ed4baddf56d031e58e178430ee023d35ed` on an ESP32-S3 product board
using Arduino/PlatformIO `esp32s3dev`, SDA GPIO8, SCL GPIO9,
400 kHz, address `0x3C`, 128x64 geometry, and the configured profile
`example-default-128x64-internal-charge-pump`.

Smoke, functional, retention, benchmark, 77-command extended, a measured
96,500-operation hour, and post-soak cleanup passed as serial evidence. The
scan also observed ACKs at
`0x41`, `0x50`, and `0x51`, which provides limited shared-bus coexistence
evidence. The extended suite found a full-buffer page-iteration bug in the
Arduino diagnostic owner; the refactored complete-flush path then passed.

COM21 does not establish the exact panel/controller, supply, pull-ups, IREF
wiring, visual behavior, reset-pin behavior, safe physical faults, or logic-
analyzer timing. `probe()` and the scan prove ACK only. The Arduino CLI uses
blocking compatibility calls and does not qualify v4 cooperative owner/result
lifetimes.

### COM29, ESP32-S2, 2026-06-23

The historical evidence records Arduino/PlatformIO `esp32s2dev`, address
`0x3C`, 128x64 geometry, GPIO8/GPIO9 at 400 kHz, serial functional/benchmark/
retention coverage, and an eight-hour serial soak with 755,500 mixed
operations and zero serial failure rows.

COM29 predates the v4 ownership model and has the same missing visual,
electrical, reset, physical-fault, and controller-identity evidence. It is
historical device/serial evidence, not v4 qualification.

## Coverage Ledger

| Area | COM21 2026-07-31 / 55.03.311 | COM29 2026-06-23 code | Remaining evidence |
| --- | --- | --- | --- |
| MCU / framework | ESP32-S3 N16R8, Arduino 3.3.11 / IDF libraries 5.5.5 | ESP32-S2, Arduino/PlatformIO | Native ESP-IDF hardware run |
| Geometry / address | 128x64 / `0x3C` configured and ACKed | 128x64 / `0x3C` configured and ACKed | Exact module/controller authority |
| Serial smoke and self-test | PASS; self-test 20/20 | PASS | Visual observation |
| Draw/control/scroll | PASS serial | PASS serial | Photos/video and operator results |
| Full and partial flush | PASS serial | PASS serial | Logic-analyzer transaction proof |
| Software recover | PASS serial | PASS serial | Safe injected transport/reset faults |
| Long soak | 3,612.265 s / 97,000 operations PASS | Eight hours / 755,500 operations PASS | Representative multi-unit/thermal soak |
| Shared-bus presence | `0x3C`, `0x41`, `0x50`, `0x51` ACKed | `0x3C`, `0x51` ACKed | Production owner scheduling and fault isolation |
| Cooperative v4 owner API | Host fault tests only | Not applicable | Dedicated `attach/start/poll/result` hardware fixture |
| One-attempt callback/deadline/cancel | Host fault tests only | Not applicable | Hardware owner/result trace |
| Page-buffer-off presentation | Host tests; CLI page iteration serial PASS | Not applicable | Visual owner-fixture evidence |
| Reset pin | Unknown | Unknown | Board-owned reset timing test |
| Missing/unplugged display | Not run | Not run | Safe absence/reconnect run |
| Electrical profile | Unknown | Unknown | Supply, pull-ups, pump, IREF, COM/remap data |
| Controller identity | Unknown; ACK only | Unknown; ACK only | Marking or authoritative BOM/module record |

## Remaining Qualification Gates

1. Identify the exact panel/module and controller; record supply, logic levels,
   pull-ups, reset, IREF, charge-pump mode, COM pins, remap/orientation, and
   analog defaults.
2. Record operator visual results for clear/fill/patterns, controls, scroll,
   full/partial update, page-buffer presentation, sleep/wake, and retention.
3. Use a safe fixture to test missing display, unplug/replug, NACK, timeout/bus
   errors, reset sequencing, recovery, and dirty-data retention. Distinguish
   address/data NACK only when the transport reports that distinction; the
   native IDF adapter preserves ambiguous transmit failures as bus errors.
4. Capture logic-analyzer evidence for transaction boundaries, control bytes,
   chunking, one-attempt callback behavior, deadlines, and cancellation.
5. Run a production-style shared-bus owner fixture using the cooperative v4 API
   and representative ESP32-S2/S3 Arduino and ESP-IDF builds.
6. Extend soak coverage across representative panels/boards and record visual,
   thermal, reset, and mixed-device behavior.

## Interpretation Rules

- `probe()` is diagnostic-only. ACK never proves SSD1315 identity.
- The default profile sends SSD1315-specific commands including `SET_IREF`;
  these runs do not establish SSD1306 compatibility.
- Serial success proves only that the firmware and CLI observed successful
  transactions/status. It does not prove pixels, voltage, current, temperature,
  reset, or absence behavior.
- `recover()`/`reset` in the diagnostic is software resynchronization. Hardware
  `RES#`, bus recovery, locking, and scheduling remain application-owned.
- Failed control sequences require full resync before trusting modeled panel
  state, and failed flushes must retain dirty data for retry.
- Local raw artifacts stay under ignored `hil_logs/`; this ledger preserves
  the dated committed evidence and its source/artifact references.
