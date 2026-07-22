# SSD1315 Readiness Summary

Status: version 4.0.0 completes the software ownership hardening and software
release gates. Representative hardware qualification is not complete. Do not
describe it as field-ready or hardware-qualified.

Committed COM29 serial evidence remains useful historical evidence for an
ESP32-S2 Arduino/PlatformIO target. It does not qualify v4: exact panel model,
supply, pull-ups, reset wiring, visual behavior, safe faults, logic-analyzer
captures, and a representative S2/S3 hardware matrix were not recorded.

## Active Documentation Set

- `README.md`: public usage, API, build, validation, and release-gate notes.
- `docs/DOCUMENTATION.md`: maintained document map and evidence policy.
- `CHANGELOG.md`: release-facing public changes.
- `docs/TUNNELMONITOR_INTEGRATION_GATES.md`: remaining external
  integration gates.
- `docs/SSD1315_DATASHEET_ALIGNMENT.md`: controller and panel-profile facts.
- `docs/SSD1315_I2C_Command_Reference.md`: command-level reference notes.
- `docs/IDF_PORT.md`: native ESP-IDF example and component boundaries.
- `docs/SSD1315_HIL_RUNBOOK.md`: repeatable hardware procedure.
- `docs/SSD1315_HIL_TARGET_TEMPLATE.md`: per-target evidence form.
- `docs/SSD1315_HARDWARE_VALIDATION.md`: committed hardware ledger.

## V4 Software Contract

- The core remains framework-neutral, non-owning, non-thread-safe, and not
  ISR-safe. Applications own bus creation, pins, reset, locking, recovery,
  scheduling, timeout policy, and device health policy.
- `attach()` validates/binds configuration and may allocate one framebuffer;
  it performs zero I2C. `detach()`, `end()`, and destruction perform zero I2C.
- A single fixed cooperative operation state machine covers initialize, flush,
  sleep, wake, resync, shutdown, and three-phase horizontal/vertical scroll
  setup. Admission and cancellation are zero-I2C.
- Each operation has nonzero request identity, optional absolute deadline,
  visible progress/effect/power state, and one consume-once terminal result.
  Effect/power are inferred from terminal writes and configured timing, never
  controller readback or optical/electrical verification.
  Direct and legacy I2C paths remain zero-I2C/BUSY until that result is consumed,
  preserving the result's hardware provenance.
- `pollOperation()` allows at most eight transactions; a normal shared-bus owner
  uses one, and a deadline-bearing operation is limited to one attempt per poll.
  There are no core retries, bus recovery, locking, logging, or hidden tasks.
- `I2cWriteFn` returns one terminal `TransportResult` and permits at most one
  physical bus transaction per invocation. `Config::maxWriteBytes` includes
  the control byte and is validated in
  `[4..129]`.
- A 128x64 initialize-off operation is 17 callbacks. Full resync is 42
  callbacks with capacity 129 and payload budget 128, or 50 callbacks at
  default capacity 65. The display-on interval is zero-I2C.
- Drawing, bounded text, dirty marking, and activity helpers are memory-only.
  Successful raw passthrough invalidates modeled panel state.
- `OFFLINE` is diagnostic-only. Automatic sleep/page-cycle settings are
  deprecated compatibility storage and `tick()` never admits those policies.
- Page-buffer mode initializes off and does not support full-buffer resync. The
  owner flushes all page windows while off and then explicitly wakes.
- `begin()` and `recover()` are bounded blocking compatibility facades over the
  same cooperative state machine; they are not the shared-bus owner interface.
- SSD1315 has no NVM programming, calibration storage, endurance-limited write,
  commissioning, or readback procedure; rare/one-time maintenance operations
  are therefore not applicable to this write-only driver.
- The profile remains explicitly SSD1315-only and sends `SET_IREF`. ACK-only
  `probe()` can establish address response, not controller identity.

## Validation Status

GitHub Actions [run 74](https://github.com/janhavelka/SSD1315/actions/runs/29911523207)
on 2026-07-22 passed all six jobs: 118 native tests, contract/version/HIL-tool
guards, package construction/content checks, strict Doxygen, Arduino
PlatformIO ESP32-S2/S3 builds, and native ESP-IDF v5.3.5 ESP32-S2/S3 builds.
The Arduino environments use PlatformIO 6.1.19 and the immutable pioarduino
54.03.20 archive.

Local release checks also passed the same 118-test suite, 21 HIL-parser tests,
guards, package/Doxygen validation, and cache-independent Arduino S2/S3 builds.
No physical HIL was run for 4.0.0. These results establish software release
evidence, not field-grade hardware qualification.

Use `pio test -e native` for the host suite; the native environment is a test
target rather than an application build.

The Arduino and ESP-IDF examples are bring-up diagnostics. They show platform
transport glue and exercise commands, but they do not establish a production
shared-bus ownership, scheduling, cancellation, or health policy.

## Hardware And Integration Gates

The exact target module/controller, power arrangement, IREF mode, orientation,
and reset wiring must be named before selecting a product profile. Preserve the
honest SSD1306 disclaimer and ACK-only probe wording.

Representative HIL must record exact module, MCU, framework, bus speed, reset
wiring, command coverage, visual results, absence/reconnect, safe fault cases,
mixed-device shared-bus traffic, and soak duration. The maintained matrix is the
authority; use `Not run` and `unknown` rather than inference.

TunnelMonitor integration remains deferred. Its authoritative dependency policy
still says so, and its 2500 ms display-operation deadline conflicts with the
1250 ms protected result cutoff. Current display writes also use retry-capable
generic transfer/recovery behavior. Those firmware contracts, exact dependency
pinning, production builds, and hardware validation must be resolved before the
direct renderer is replaced.

## Release Status

Version 4.0.0 is the current software release. Its source, version metadata,
tests, documentation, package contents, CI, and available historical HIL
evidence were reviewed together. The missing representative visual,
fault/recovery, reset, and hardware-matrix evidence remains an explicit product
qualification gap rather than an unrecorded software-release claim.
