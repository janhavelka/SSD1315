# SSD1315 pioarduino 55.03.311 HIL Validation - COM21 - 2026-07-31

Status: accepted as partial serial HIL for the configured 128x64 device at
address `0x3C`. The current pioarduino stack, exact ESP32-S3 N16R8 memory
configuration, functional/benchmark/extended plans, measured one-hour soak,
and post-soak serial reattachment passed. This report does not claim
controller identity, visual correctness, electrical qualification, physical
fault injection, reset-pin coverage, or production-owner qualification.

## Run Context

| Field | Value |
| --- | --- |
| Repository branch / base | `main`, `d29fefc624a76db56354c72b6b8e85c7225279e1` |
| Sync state before changes | `origin/main` was the default/newest remote branch and the checkout was already fast-forward current |
| Firmware source state | `d29fefc` plus the dirty pioarduino migration diff recorded in each artifact; no clean-commit claim |
| Library package version | `4.0.1` |
| PlatformIO | Core 6.1.19, pioarduino platform 55.3.311 |
| Arduino / IDF libraries | Arduino-ESP32 3.3.11; ESP-IDF libraries `5.5.5+sha.b774170ff46` |
| Build tools | GCC 14.2.0+20260121, esptool 5.3.0, GDB 17.1.0+20260402 |
| PlatformIO environment / board | `esp32s3dev`; `esp32-s3-devkitc1-n16r8` |
| Runtime identity | ESP32-S3 revision 2; 16,777,216 flash bytes; PSRAM ready, 8,388,608 bytes |
| Upload detection | ESP32-S3 QFN56 rev 0.2, embedded 8 MB PSRAM, 16 MB flash, USB Serial/JTAG |
| Serial target | `COM21`, 115200 baud; DTR/RTS deasserted before open |
| I2C configuration | SDA GPIO8, SCL GPIO9, configured 400 kHz; no logic-analyzer speed measurement |
| Display configuration | 128x64, `0x3C`, `example-default-128x64-internal-charge-pump` |
| Panel/controller identity | Unknown; ACK and successful writes do not identify the controller |
| Supply / pull-ups / reset | Unknown / unknown / unknown |
| Operation mode | Arduino blocking compatibility diagnostic CLI |
| Operator evidence | Serial-only; visual rows were skipped rather than promoted to pass |

The shared-bus scan returned ACKs at `0x3C`, `0x41`, `0x50`, and `0x51`.
Only `0x3C` was addressed by the display diagnostic. No device identity is
inferred from those ACKs.

## Platform Impact Audit

The immutable platform pin follows the sibling EE871 project's version-pin
pattern, but its generic 4 MB/QSPI memory overrides were not copied because
this target is explicitly an N16R8 board.

| Surface | Previous qualification stack | Current stack / decision |
| --- | --- | --- |
| pioarduino | 54.3.20 | 55.3.311 |
| Arduino-ESP32 | 3.2.0 | 3.3.11 |
| Resolved ESP-IDF libraries | `5.4.0+sha.2f7dcd862a` | `5.5.5+sha.b774170ff46` |
| esptool | 4.8.9 | 5.3.0; upload and readback passed |
| Xtensa toolchain | 14.2.0+20241119 | 14.2.0+20260121; S2/S3 builds passed |
| Xtensa GDB | 14.2.0+20240403 | 17.1.0+20260402 |
| S3 board metadata | Generic DevKitC-1 plus manual 4 MB/default partition and PSRAM flags | Exact N16R8 definition: 16 MB flash, 8 MB octal PSRAM, `qio_opi`, `default_16MB.csv` |
| Wire writes | 128-byte buffer and write/end status mapping | Compatible; write-side API and status codes used by the adapter are unchanged |
| Wire reads | Not used by this driver | New request-length clamp has no effect on this write-only transport |
| HWCDC | Older implementation | Upstream write/race fixes are relevant to the CLI; all response-boundary and reattachment runs passed |
| Native ESP-IDF | CI v5.3.5 only | CI matrix now covers v5.3.5 and v5.5.5 for S2/S3; `idf.py` was unavailable locally |
| Root IDF manifest | Stable hand formatting | pioarduino 55.03.311 normalizes YAML and creates `.orig`; semantic version/dependency checks and an exact backup ignore keep repeated builds deterministic |

No SSD1315 core, init profile, command sequence, transaction capacity, or
controller-compatibility change was required. The core remains framework-
neutral; platform-specific changes are confined to examples, build/CI, tests,
and HIL tooling.

## Hardware-Found Adapter Defect

The first hardened firmware checked both `Wire.begin()` and a redundant
`Wire.setClock()` call. Its first smoke run timed out because setup had stopped
at `I2C init failed`. A separate esptool reset/read diagnostic captured that
boot log; it is an observed session diagnostic, not content in the rejected
runner directory. The Arduino-ESP32 NG I2C HAL used by both pinned stacks can
apply a clock change but retain a failure return when no cached device handle
exists.

The example adapter now performs one fallible
`Wire.begin(sda, scl, frequency)` call and then sets the transaction timeout.
Host tests verify pins/frequency/timeout and begin failure. Current-stack HIL
and the previous 54.03.20 compatibility build then passed. This is an example-
adapter correction; the transport-injected SSD1315 core did not own or
reconfigure the bus.

## Build And Host Validation

| Check | Result |
| --- | --- |
| Current ESP32-S3 N16R8 Arduino build/upload | PASS; RAM 25,712 bytes, flash 400,380 bytes; flash readback verified |
| Current ESP32-S2 Arduino build | PASS; RAM 52,308 bytes, flash 407,105 bytes |
| Previous-stack N16R8 compatibility build | PASS; Arduino 3.2.0; RAM 23,988 bytes, flash 438,722 bytes |
| Native host suite | PASS, 123/123 |
| HIL parser/runner suite | PASS, 38/38 |
| Timing, CLI, IDF, version, Python compile guards | PASS |
| Exact HIL dry-run plans | PASS for smoke, functional, retention, benchmark, Arduino extended, soak, and all |
| Doxygen 1.13.2 warning-as-error build | PASS |
| Native local ESP-IDF compile | Not run: `idf.py` and Docker were unavailable; CI matrix was expanded but not executed remotely for this dirty checkout |

## HIL Evidence

Raw directories are local under ignored
`hil_logs/20260731-pioarduino-55.03.311/`. Each accepted run contains exact
argv/metadata, transcript, JSON/CSV results, parsed identity/configuration,
health deltas, run statistics, and operator/validation fragments.

| Run | Artifact directory | Result / coverage |
| --- | --- | --- |
| Rejected initial smoke | `ssd1315_20260731_201217` | FAIL 1/1: no command response after the checked redundant `setClock()` path halted setup; retained as defect evidence |
| Corrected smoke | `ssd1315_20260731_202111` | PASS 7/7; exact firmware/MCU/memory identity, four-device scan, ACK-only probe, self-test 20/20, clean config |
| Combined all plan | `ssd1315_20260731_202128` | Serial PASS: 32 PASS, 41 operator-required, 0 fail; functional, retention-cleanup, one 500-operation soak cycle, clean final config |
| Benchmark | `ssd1315_20260731_202235` | Serial PASS: 5 PASS, 3 operator-required, 0 fail; stress 1000/1000, mixed 100/100, flush 100/100, burst 1000/1000 |
| Arduino extended | `ssd1315_20260731_202257` | Serial PASS: 52 PASS, 25 operator-required, 0 fail; all 77 commands and clean final config |
| One-hour soak | `ssd1315_20260731_202349` | PASS / COMPLETE: 194 batches, 97,000 operations, measured 3,612.265 soak seconds, clean final config |
| Post-soak smoke / reattach | `ssd1315_20260731_212434` | PASS 7/7 on a new serial open |
| Post-soak Arduino extended | `ssd1315_20260731_212442` | Serial PASS: 52 PASS, 25 operator-required, 0 fail; all 77 commands again, clean final config |

The duration run took 3,616.359 host seconds and accepted 1,168 command rows:
586 serial PASS and 582 serial-pass/operator-required, with no failure or
review-required row. Its 389 telemetry samples had strictly increasing uptime
and loop heartbeat, constant reset reason 11, constant 8,724,752-byte free
heap and 8,719,440-byte minimum free heap, zero driver failures, zero serial
interruptions/retries, and no restart. Final state was initialized, awake,
online, not flushing, not scrolling, with clean framebuffer/control state.

## Validation Limits

- No operator visual pass/fail, photo, or video was captured. Serial success
  does not prove clear/fill/pattern/orientation/scroll appearance or absence of
  OLED retention.
- No panel/controller marking, supply voltage, pull-up value, IREF wiring, or
  reset GPIO was established.
- No unplug/replug, missing-display, injected NACK/timeout/bus fault,
  power-cycle, thermal, or logic-analyzer test was attempted because fixture
  wiring and safe fault authority were not specified. Host fake-transport
  tests cover failure retention and state contracts, not physical behavior.
- Arbitrary raw controller-byte combinations were intentionally excluded.
  They can be unsafe or invalidate modeled panel state; host tests cover raw-
  command invalidation and resync.
- The diagnostic uses blocking compatibility calls. Hardware qualification of
  the cooperative `attach/start/poll/result` owner, one-attempt scheduling,
  cancellation, and result lifetime needs a dedicated production fixture.
- `probe()` and scan results prove address ACK only. The default profile sends
  SSD1315-specific `SET_IREF`; this is not SSD1306 compatibility evidence.
- This is strong serial/device evidence for the exact stack and board, but not
  a field-grade hardware qualification claim.
