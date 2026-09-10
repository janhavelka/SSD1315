# SSD1315 Hardware Validation Ledger

This ledger records the hardware evidence that exists for the SSD1315 driver:
partial serial HIL for the pioarduino 55.03.311 ESP32-S3 N16R8 candidate on
COM21 (`docs/reports/hil-validation-COM21-20260731.md`), plus consolidated
earlier COM21 and COM29 serial runs whose superseded reports remain only in
Git history.

The executable procedure, per-command result table, and per-target evidence
form live in `docs/SSD1315_HIL_RUNBOOK.md`. This file records durable outcomes
only; it intentionally does not duplicate empty command tables or command
recipes from the runbook.

## Recorded Evidence

### COM21, ESP32-S3 N16R8, 2026-07-31

The current report is `docs/reports/hil-validation-COM21-20260731.md`; its run
context table records the exact base commit, dirty-worktree state, toolchain
versions, board definition, and runtime identity.

Exact identity, smoke, combined functional/retention cleanup, benchmark, all
77 Arduino extended commands before and after soak, and a measured 97,000-
operation hour passed serial validation. The duration run captured 389 healthy
telemetry samples, no driver failure, reset, retry, interruption, or heap
change, and a clean final state. The scan also ACKed `0x3C`, `0x41`, `0x50`,
and `0x51`.

The first rejected smoke run exposed an example-adapter initialization defect;
the report's adapter-defect section records the cause and the corrected
`Wire.begin()` path that then passed on both stacks.

The panel/controller, supply, pull-ups, IREF wiring, reset wiring, visual
behavior, physical faults, and logic-analyzer timing remain unknown/not run.
Configured 400 kHz was not electrically measured. ACK proves address response
only. The dirty-worktree evidence is not a clean release/CI claim.

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

| Area | COM21 current 55.03.311 checkout | COM29 historical code | Remaining evidence |
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
3. Use a safe fixture to test missing display, unplug/replug, address/data NACK,
   timeout/bus errors, reset sequencing, recovery, and dirty-data retention.
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
- Local raw artifacts stay under ignored `hil_logs/`; the dated reports are the
  committed evidence record.
