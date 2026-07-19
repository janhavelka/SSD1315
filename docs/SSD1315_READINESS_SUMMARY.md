# SSD1315 Readiness Summary

Status: the pending v4 implementation hardens the software ownership contract,
but release-candidate validation and representative hardware qualification are
not complete. Do not describe it as field-ready or hardware-qualified.

Committed COM29 serial evidence remains useful historical evidence for an
ESP32-S2 Arduino/PlatformIO target. It does not qualify v4: exact panel model,
supply, pull-ups, reset wiring, visual behavior, safe faults, logic-analyzer
captures, and a representative S2/S3 hardware matrix were not recorded.

## Active Documentation Set

- `README.md`: public usage, API, build, validation, and release-gate notes.
- `CHANGELOG.md`: release-facing public changes.
- `docs/TUNNELMONITOR_NODE_SUITABILITY_AUDIT.md`: v4 disposition and remaining
  external integration gates.
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
  Direct and legacy I2C paths remain zero-I2C/BUSY until that result is consumed,
  preserving the result's hardware provenance.
- `pollOperation()` allows at most eight transactions; a normal shared-bus owner
  uses one, and a deadline-bearing operation is limited to one attempt per poll.
  There are no core retries, bus recovery, locking, logging, or hidden tasks.
- `I2cWriteFn` returns a terminal `TransportResult` for exactly one physical
  attempt. `Config::maxWriteBytes` includes the control byte and is validated in
  `[4..129]`.
- A 128x64 initialize-off operation is 17 transactions. Full resync is 42
  transactions with capacity 129 and payload budget 128, or 50 transactions at
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

The final working-tree native rerun after the review fixes passed 103 of 103
tests. This is host evidence, not a published release or hardware claim.
The remaining software gates are:

- host/native tests, core/CLI/IDF contract guards, version generation, and
  package-content checks;
- Arduino PlatformIO ESP32-S2 and ESP32-S3 builds;
- native ESP-IDF ESP32-S2 and ESP32-S3 builds when `idf.py` is available;
- Doxygen review with warnings treated as release failures; and
- inspection of the generated package and immutable version metadata.

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

## Release Gate

Version 3.0.0 remains the latest tagged release. This branch carries planned
4.0.0 metadata but is not published. Do not tag or publish until final source,
tests, documentation, package contents, CI, and available HIL evidence have
been reviewed together.
