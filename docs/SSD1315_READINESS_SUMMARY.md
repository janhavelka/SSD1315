# SSD1315 Readiness Summary

Status: version 4.0.1 remains the tagged maintenance release. The pioarduino
55.03.311 migration and exact ESP32-S3 N16R8 serial HIL are merged on `main` at
`418f71e` but remain in the Unreleased release candidate. Representative
hardware qualification is not complete. Do not describe it as field-ready or
hardware-qualified.

The new COM21 serial evidence covers pioarduino 55.03.311 on an exact N16R8
build/runtime configuration, including functional, retention-cleanup,
benchmark, extended-command, and one-hour soak runs. It remains partial: exact
panel model and controller, supply, pull-ups, reset wiring, visual behavior,
safe faults, logic-analyzer captures, and a production cooperative-owner
fixture were not recorded. Earlier COM21 and COM29 reports remain historical.

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
- `docs/reports/hil-validation-COM21-20260731.md`: current pioarduino 55.03.311
  N16R8 migration audit and partial serial HIL evidence.
- `docs/reports/hil-validation-COM21-20260722.md`: dated current-v4 serial HIL
  evidence on the previous Arduino stack.
- `docs/reports/hil-validation-COM29-20260623.md`: dated historical pre-v4
  serial HIL evidence.

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

The v4.0.0 tag's GitHub Actions
[run 74](https://github.com/janhavelka/SSD1315/actions/runs/29911523207) on
2026-07-22 passed all six jobs: 118 native tests, contract/version/HIL-tool
guards, package construction/content checks, strict Doxygen, Arduino
PlatformIO ESP32-S2/S3 builds, and native ESP-IDF v5.3.5 ESP32-S2/S3 builds.
That historical workflow used PlatformIO 6.1.19 and the immutable pioarduino
54.03.20 archive.

For 4.0.1, local checks passed the 118-test native suite, 35 HIL-parser tests,
timing/CLI/ESP-IDF/version guards, warning-free Doxygen, Arduino ESP32-S2/S3
builds, and generated-package content validation. The annotated release tag is
created only after the exact main commit passes its GitHub Actions workflow.

The 2026-07-22 previous-stack COM21 report passed smoke, functional,
retention, benchmark, the 77-command extended Arduino plan, a measured
96,500-operation hour, and post-soak cleanup. It found and verified the fix for
incomplete full-buffer page iteration in the diagnostic owner. Those remain
real historical serial/device results, not visual, electrical, reset,
physical-fault, or production-owner qualification.

For the merged migration, local checks passed 123/123 native tests, 38/38
HIL-parser tests, timing/CLI/ESP-IDF/version guards, warning-free Doxygen,
current pioarduino 55.03.311 ESP32-S2 and exact N16R8 builds, and the previous
54.03.20 N16R8 compatibility build. The stack resolves Arduino-ESP32 3.3.11
and ESP-IDF libraries 5.5.5. The exact merge commit then passed all nine jobs in
[GitHub Actions run 30688008949](https://github.com/janhavelka/SSD1315/actions/runs/30688008949):
native tests, package/Doxygen validation, current S2/S3 Arduino builds, the
previous-stack S3 build, and native ESP-IDF S2/S3 builds on v5.3.5 and v5.5.5.

For the current Unreleased documentation and cleanup pass on 2026-08-01, local
checks passed 124/124 native tests, 38/38 HIL-parser tests, all HIL dry-run
modes, timing/CLI/ESP-IDF/version guards, warning-free Doxygen,
package-content validation, core cppcheck warning/portability analysis,
current ESP32-S2/S3 Arduino builds, and the previous-stack ESP32-S3
compatibility build. This pass has not run new device HIL or its own GitHub
Actions workflow; those remain release gates for the eventual release commit.

Current COM21 serial HIL passed exact MCU/core/IDF/flash/PSRAM identity, smoke,
the combined functional/retention-cleanup plan, benchmark, all 77 extended
Arduino commands before and after soak, and a measured one-hour soak. The hour
completed 194 batches / 97,000 mixed operations in 3,612.265 measured seconds,
with 389 healthy telemetry samples, zero driver failures, resets, retries, or
serial interruptions, stable heap, and clean final state. The first rejected
smoke exposed the redundant Wire clock-change status defect; the corrected
fallible bus creation then passed both current hardware and previous-stack
build validation. These remain serial/device results, not visual, electrical,
reset, physical-fault, or production-owner qualification.

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
mixed-device shared-bus traffic, and soak duration. The maintained hardware
ledger is the authority; use `Not run` and `unknown` rather than inference.

TunnelMonitor integration remains deferred. Its authoritative dependency policy
still says so, and its 2500 ms display-operation deadline conflicts with the
1250 ms protected result cutoff. Current display writes also use retry-capable
generic transfer/recovery behavior. Those firmware contracts, exact dependency
pinning, production builds, and hardware validation must be resolved before the
direct renderer is replaced.

## Release Status

Version 4.0.1 is the tagged maintenance release. The pioarduino 55.03.311
migration is merged at `418f71e`, has local/COM21 evidence, and passed its exact
post-merge GitHub Actions run, but it has not been cut as a new release. Missing
representative visual, fault/recovery, reset, production-owner, and multi-target
evidence remains an explicit product-qualification gap rather than an
unrecorded software-release claim. Any subsequent release-preparation commit
requires its own green CI run.
