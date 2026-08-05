# SSD1315 Readiness Summary

Status: version 4.0.2 is prepared as a software maintenance release. The exact
release commit must pass GitHub Actions before its annotated tag is created.
Representative hardware qualification remains incomplete; do not describe the
library as field-ready, hardware-qualified, or SSD1306-compatible.

## Software Contract

- Core and public headers are framework-neutral. Applications own the I2C bus,
  pins, reset, locking, recovery, scheduling, timeout, and health policy.
- The shared-bus path is `attach()` plus explicit start/poll/result operations.
  Admission and cancellation are zero-I2C, and a normal owner poll permits one
  synchronous, timeout-bounded transport attempt.
- The core and callback do not retry, recover, back off, or replay ambiguous
  writes. Driver instances are not thread-safe and public APIs are not ISR-safe.
- Failed flushes retain dirty framebuffer data. Failed multi-command controls
  and arbitrary raw passthrough make modeled panel state uncertain until a full
  resync succeeds.
- `begin()` and `recover()` remain bounded blocking compatibility facades.
  Deprecated auto-sleep/page-cycle fields remain source-compatible storage;
  application policy owns their scheduling.
- The active profile is SSD1315-specific and sends `SET_IREF`. `probe()` proves
  only address ACK, never controller identity.

README and public Doxygen contain the full API, timing, ownership, memory, and
error contracts. `docs/SSD1315_HARDWARE_VALIDATION.md` is the authority for
hardware evidence and remaining qualification gaps.

## Validation Snapshot

The pre-release-preparation `main` commit `5186b45` passed all nine jobs in
[GitHub Actions run 30795162504](https://github.com/janhavelka/SSD1315/actions/runs/30795162504):
native tests, package/Doxygen validation, current and previous Arduino stacks,
and ESP32-S2/S3 native ESP-IDF builds on v5.3.5 and v5.5.5.

The release-preparation working tree passed 125/125 native tests, 38/38 HIL
parser tests, static timing/CLI/ESP-IDF/version guards, all-mode HIL dry-run,
warning-as-error Doxygen, all three Arduino builds, and package validation on
2026-08-05. Native ESP-IDF was not installed locally; the release commit still
requires its own GitHub matrix run before tagging.

The retained COM21 report records partial serial HIL on an ESP32-S3 N16R8 with
pioarduino 55.03.311, Arduino-ESP32 3.3.11, ESP-IDF libraries 5.5.5, configured
400 kHz I2C, and address `0x3C`. Smoke, functional/retention cleanup, benchmark,
77-command extended plans before and after soak, and a measured 97,000-operation
hour passed serial validation. This is not visual, electrical, reset, physical-
fault, or production-owner qualification.

## Open Hardware Gates

Before a stronger product claim, record the exact module/controller, supply,
pull-ups, IREF and charge-pump profile, reset wiring, bus timing, visual results,
absence/reconnect behavior, safe injected faults, cooperative shared-bus owner
behavior, and representative multi-unit/thermal soak duration. Use `unknown`
and `Not run` instead of inference.

## Release Gate

Follow `docs/RELEASING.md`. The release tag must be new, annotated, and point to
the exact commit whose `CI` run completed successfully. The tag-triggered CI run
is an additional verification, not a substitute for checking the main commit
before tagging.
