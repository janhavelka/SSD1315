# SSD1315 Industry Readiness Audit

Branch: `hardening/ssd1315-industry-readiness`

## Executive Summary

The SSD1315 driver is structurally stronger than a typical hobby display library: the core is framework-neutral, I2C is injected, framebuffer flushing is budgeted through `tick()`, and the ESP-IDF example is native IDF code. It is not yet fully industry-grade because ESP-IDF builds are not covered in CI/local validation, lifecycle calls contain bounded but significant blocking I2C work, and some SSD1315-versus-SSD1306/reset/partial-control-state contracts are under-specified.

Readiness classification: suitable as a hardened diagnostic/display library candidate, but merge should require build coverage and clearer blocking/reset/controller claims.

## Baseline Checks

- `python tools/check_core_timing_guard.py`: pass
- `python tools/check_cli_contract.py`: pass
- `python tools/check_idf_example_contract.py`: pass
- `python scripts/generate_version.py check`: pass
- `python -m platformio test -e native`: pass, 31 test cases
- ESP-IDF build: not yet run in this pass.
- Hardware validation: not run in this audit.

## Scorecard

| Area | Status | Notes |
| --- | --- | --- |
| Core framework neutrality | Mostly ready | Core/public headers do not include Arduino or ESP-IDF framework headers. |
| I2C ownership | Mostly ready | Core receives transport callbacks; examples own platform bus glue. |
| Timing contracts | Partial | `tick()` is budgeted, but `begin()`/`recover()` run init plus full GDDRAM clear. |
| Status/error precision | Partial | Probe maps timeout/data NACK to `DEVICE_NOT_FOUND`. |
| Health/recovery | Mostly ready | Offline latch and recovery are tested. |
| Partial hardware state | Partial | Flush dirty state is conservative; multi-command panel controls lack a dirty diagnostic. |
| Device correctness | Partial | SSD1315-specific `SET_IREF` conflicts with broad SSD1306 compatibility claims. |
| Tests | Partial | Native tests cover many paths; IDF adapter/build and golden init sequence tests are missing. |
| Docs/examples | Partial | IDF example is native, but README overstates non-blocking behavior and compatibility. |

## Strengths

- Core implementation is framework-neutral.
- `Status` uses static messages and preserves detail codes.
- Flush state keeps dirty pages intact on failure.
- Thread and ISR limits are already documented.
- Native IDF example uses `app_main`, `driver/i2c_master.h`, fixed C buffers, and IDF timing/task APIs.

## High Findings

1. CI does not build the pure ESP-IDF example even though `espidf` is advertised.
2. README and Doxygen describe the driver as non-blocking, but `begin()` and `recover()` run a blocking init sequence and GDDRAM clear.
3. SSD1306 compatibility is not proven locally; init always sends SSD1315-specific `SET_IREF`.

## Medium Findings

- Hardware reset pin ownership is under-specified; `recover()` is software init only.
- IDF adapter error mapping is coarser than the core status model and does not demonstrate app-level locking.
- Multi-command hardware operations such as scroll setup can fail mid-sequence without a panel dirty/resync diagnostic.
- `begin()` Doxygen says it calls `end()` first, but implementation resets runtime state directly.
- Move operations are not explicitly deleted even though copies are.

## Recommended Remediation Plan

- Add CI coverage for IDF contract checking and `idf.py` builds for ESP32-S2/S3.
- Preserve timeout/bus/generic probe errors; map only address NACK to `DEVICE_NOT_FOUND`.
- Explicitly delete move construction/assignment and add native static assertions.
- Document `begin()`/`recover()` as bounded blocking and software-reset-only.
- Qualify SSD1306 compatibility until a separate controller profile or hardware validation exists.

## Exit Criteria For Industry Grade

- Native tests, PlatformIO Arduino builds, and pure ESP-IDF builds pass in CI.
- Public API docs list transaction counts and latency for blocking operations.
- Controller compatibility and reset-pin responsibilities are documented and tested.
- Hardware validation records init, flush, recovery, missing-display, and reset behavior on representative panels.
