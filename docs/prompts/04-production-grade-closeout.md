# AI Coder Prompt: SSD1315 Production-Grade Closeout

You are working inside the SSD1315 repository. Do a focused production-grade
closeout pass. The current library is strong on software contracts, CI, native
tests, and serial HIL tooling, but the repository still has gaps before it can
honestly claim production-grade or field-ready status.

Goal:
Close the remaining factual gaps with small, concrete changes. Prefer simple,
functional, robust, readable code. Reuse existing driver APIs, native fake-bus
tests, HIL runner artifacts, examples/common helpers, and docs structure. Do
not add broad frameworks, speculative abstractions, or platform ownership to
the core driver.

Do not mutate or recreate the already-pushed `v2.1.0` tag. Treat this as
follow-up work for the next release. If public API or documented behavior
changes, update `CHANGELOG.md` and version metadata according to SemVer.

## Required First Steps

1. Read `AGENTS.md` first and follow it as binding.
2. Check `git status -sb`; preserve dirty user changes.
3. Inspect:
   - `README.md`
   - `CHANGELOG.md`
   - `include/ssd1315/*.h`
   - `src/SSD1315.cpp`
   - `test/native/test_basic.cpp`
   - `.github/workflows/ci.yml`
   - `tools/run_ssd1315_hil.py`
   - `tools/test_hil_runner_parser.py`
   - `tools/check_*.py`
   - `docs/SSD1315_READINESS_SUMMARY.md`
   - `docs/SSD1315_HARDWARE_VALIDATION.md`
   - `docs/SSD1315_HIL_RUNBOOK.md`
   - `docs/SSD1315_HIL_TARGET_TEMPLATE.md`
   - `docs/reports/*.md`
4. Use subagents only for read-only audit splits. Do final judgment, edits, and
   verification yourself.

Suggested subagents:
- `docs-hw-agent`: audit production claims, HIL evidence, hardware matrix,
  README/readiness wording, and release notes.
- `core-contracts-agent`: audit public Status/API contracts, caller buffers,
  dirty/error clearing, offline state, and flush timeout semantics.
- `tests-fault-agent`: audit host fake-transport tests, HIL runner parser
  coverage, and missing fault/reset evidence hooks.
- `integration-review-agent`: review final diff for framework leakage,
  unsupported claims, accidental broad refactors, and missing tests.

## Current Factual Baseline

These are known from the current repository state and prior release work:

- CI was reported as passing for the `v2.1.0` release commit. Verify and record
  the exact workflow run URL, commit SHA, and workflow names before documenting
  this as evidence.
- Native tests pass locally: `python -m platformio test -e native` reports
  `91 test cases: 91 succeeded`.
- CI config includes PlatformIO ESP32-S2/ESP32-S3 builds, native tests, package
  validation, Doxygen generation, and ESP-IDF example builds through
  `espressif/esp-idf-ci-action@v1`.
- A COM29 ESP32-S2 Arduino/PlatformIO serial HIL report exists with an 8-hour
  soak and 0 serial failures: `docs/reports/hil-validation-COM29-20260623.md`.
- The committed source-of-truth hardware matrix still says most rows are
  `Not run`: `docs/SSD1315_HARDWARE_VALIDATION.md`.
- Visual checks, S2/S3 hardware checks, and CI pass may have been confirmed by
  the operator, but they are not fully recorded in the maintained hardware
  matrix. Do not invent missing target details or media paths.
- Sanitizer execution was attempted but not completed because the local MinGW
  install lacked ASan/UBSan runtime libraries:
  `docs/reports/internal-stress-audit-20260623.md`.

## Production-Grade Definition For This Pass

The repository may call the library "production-grade software-contract ready"
only if all of these are true and documented:

1. Core driver remains framework-neutral and bus-non-owning.
2. Public fallible APIs return `Status` or are explicitly documented as
   best-effort/void legacy APIs.
3. Steady-state flush work is bounded and tested across instruction and byte
   budgets.
4. Caller-owned memory has a verifiable size contract.
5. Offline, busy, timeout, invalid config, buffer too small, and I2C transport
   failures are distinguishable where the API can observe them.
6. Dirty framebuffer retry behavior is preserved after failed flushes.
7. Multi-command panel-control failures mark control state dirty until a full
   resync.
8. CI and local release validation commands are documented with exact results.
9. Hardware claims name exact board, MCU, framework, panel/module, address,
   bus speed, reset wiring, command coverage, visual coverage, fault coverage,
   and soak duration.

Do not claim "field-ready", "field-grade", "complete hardware validation", or
"production hardware validated" unless the maintained hardware matrix has exact
representative evidence for visual, reset, fault/recovery, and soak coverage.

## Required Code/API Closeout

Keep changes narrow. Add tests before or with each behavior change.

### 1. Caller-Owned Buffer Size Contract

Current gap:
`Config::externalBuffer` has no length field, but `begin()` writes
`width * pageBufferPages` bytes into it.

Required design:
- Add `size_t externalBufferSizeBytes = 0;` to `SSD1315::Config`.
- Define required size as:
  - `requiredBufferSizeBytes = width * pageBufferPages`
  - for full-buffer 128x64 default, this is `128 * 8 = 1024` bytes.
- If `externalBuffer != nullptr`, require
  `externalBufferSizeBytes >= requiredBufferSizeBytes`.
- Reject undersized or zero-sized external buffers before any I2C transaction.
- Add a precise error:
  - Preferred enum: append `BUFFER_TOO_SMALL` to `Err` without shifting existing
    enum numeric values.
  - If adding an enum is too disruptive, use `INVALID_CONFIG` with static
    message `"external buffer too small"`, but document the choice.
- Update all examples that provide external buffers:
  - `examples/01_basic_bringup_cli/main.cpp`
  - `examples/espidf_basic/main/main.cpp` if it uses caller storage
  - any shared config helper under `examples/common/`
- Tests:
  - exact-size external buffer succeeds
  - undersized external buffer returns `BUFFER_TOO_SMALL` or `INVALID_CONFIG`
  - undersized external buffer performs zero I2C writes
  - page-buffer mode size is `width * pageBufferPages`, not full height

### 2. Distinguish Latched Offline From Busy

Current gap:
`_offlineStatus()` reports `BUSY`, which conflates active work with a latched
driver fault.

Required design:
- Add a specific status code:
  - Preferred enum: append `DRIVER_OFFLINE` to `Err`.
  - Message: `"Driver is offline; call recover()"`.
- Keep `BUSY` for transient active work only, such as active flush or command
  conflict.
- Update `diag::errToString()` and ESP-IDF/Arduino example error formatting.
- Update tests that currently expect `BUSY` for offline.
- Add tests for:
  - normal public operation while offline returns `DRIVER_OFFLINE`
  - offline operation performs zero I2C writes
  - `recover()` may still attempt I2C through its explicit recovery path

SemVer note:
Changing a public return code may be a breaking behavior change. If the project
treats it as breaking, bump the next release to `3.0.0`. If maintained as a
compatible diagnostic refinement, explain that in `CHANGELOG.md`.

### 3. Clarify `PANEL_NOT_READY`

Current gap:
`Err::PANEL_NOT_READY` is public but normal display-on delay uses
`IN_PROGRESS`.

Required design:
- Keep normal asynchronous delay behavior as `IN_PROGRESS`.
- Mark `PANEL_NOT_READY` as reserved/legacy in Doxygen and README unless you
  deliberately choose to emit it from a new explicit query API.
- Do not change `tick()` or `pollFlush()` semantics just to use this enum.
- Add or update one test/doc assertion so the intended behavior is clear.

### 4. Exact Flush Timeout Boundary

Current gap:
`pollFlush()` times out when `elapsed > flushTimeoutMs`, while `waitFlush()`
uses `>=`.

Required design:
- Use `elapsed >= flushTimeoutMs` for flush deadline expiration.
- Add a native test at exactly `start + flushTimeoutMs`.
- Confirm dirty flags remain intact after timeout.
- Confirm health accounting increments once.

### 5. `pollFlush(maxInstructions=0)` Query Contract

Current gap:
`pollFlush(now, 0, 0)` should be a no-I2C query but currently rejects
`byteBudget == 0` after active flush checks.

Required design:
- If `maxInstructions == 0`, do not validate `byteBudget`.
- Active flush: return `IN_PROGRESS`, no I2C, no phase/page/column advance.
- Idle flush: return `OK`, no I2C.
- Add tests:
  - active `pollFlush(now, 0, 0)` no-I2C/no-advance
  - active `pollFlush(now, 0, 16)` remains covered
  - active `pollFlush(now, 1, 0)` still returns `INVALID_CONFIG`

### 6. Dirty And Error Clearing Semantics

Current gap:
`clearDirty()` can erase retry state during/after failed flushes. `clearError()`
only clears `_lastError`, not flush or control-state diagnostics.

Required design:
- Preserve backward compatibility if possible.
- Preferred 2.x-compatible design:
  - Keep existing `void clearDirty()` but document it as a force/manual direct
    buffer ownership operation that can discard retry state.
  - Add `Status clearDirtyIfIdle()`:
    - returns `BUSY` if `isFlushing()`
    - returns `STATE_ERROR` if flush phase is `ERROR` and dirty pages remain
    - otherwise clears dirty tracking and returns `OK`
  - Add `void clearLastError()` and keep `clearError()` as a deprecated or
    compatibility alias.
  - Document that neither clears `controlStateDirty`; only successful
    `begin()` or `recover()` clears control-state dirty.
- If preparing a major release, you may instead change `clearDirty()` to return
  `Status`, but do not do this silently.
- Tests:
  - active flush cannot be cleared by the safe API
  - failed flush dirty retry state is not cleared by the safe API
  - forced legacy clear remains documented and tested if kept
  - `clearLastError()` does not clear `controlStateDirty`

### 7. Checked Bitmap Input Size

Current gap:
`drawBitmap()` has no source-size contract and can read past caller data if
`w/h` do not match the pointed buffer.

Required design:
- Add checked overload:
  ```cpp
  Status drawBitmap(int16_t x, int16_t y,
                    const uint8_t* bitmap,
                    int16_t w, int16_t h,
                    size_t bitmapSizeBytes,
                    bool on = true);
  ```
- Required bytes:
  - `requiredBitmapBytes = ((w + 7) / 8) * h`
  - reject `bitmap == nullptr` when required bytes > 0
  - reject `w <= 0` or `h <= 0` with `OK` no-op or `INVALID_CONFIG`; choose
    one policy and document it. Preferred: no-op `OK` for nonpositive geometry.
  - reject undersized data with `BUFFER_TOO_SMALL` or `INVALID_CONFIG`.
- Keep the existing void overload only as a legacy unchecked helper, or have it
  call the checked implementation with a documented trusted size when possible.
- Tests:
  - exact-size bitmap draws and marks dirty
  - undersized bitmap returns error and does not touch dirty state
  - clipped offscreen bitmap does not require reading irrelevant rows beyond
    the validated source bounds

### 8. Generated Version Header And Determinism

Current gap:
Generated `Version.h` uses macros and `__DATE__`/`__TIME__`, which is at odds
with the repository's macro and deterministic-build posture.

Required design:
- Either:
  - document generated override macros as an explicit exception in `AGENTS.md`
    and Doxygen, with constexpr values as the supported public API; or
  - change `scripts/generate_version.py` to emit generated `static constexpr`
    strings and only use macros for optional compile-time overrides.
- Prefer deterministic builds:
  - Support `SOURCE_DATE_EPOCH` or a generated fixed timestamp.
  - If no deterministic timestamp is available, use `"unknown"` rather than
    `__DATE__`/`__TIME__` for release packages.
- Tests/checks:
  - `python scripts/generate_version.py check`
  - native version test still passes
  - package contains the regenerated header

### 9. Strengthen Guard And Package Tools

Required `tools/check_core_timing_guard.py` improvements:
- Keep existing checks.
- Add forbidden core patterns for `include/` and `src/`:
  - `#include <Arduino.h>`
  - `#include "Arduino.h"`
  - `#include <Wire.h>`
  - `#include "Wire.h"`
  - `#include <freertos/`
  - `#include "freertos/`
  - `#include <driver/`
  - `#include "driver/`
  - `ESP_LOG`
  - `Serial`
  - `String`
  - `delay(`
  - direct `esp_` APIs outside comments if feasible without false positives
- Make false positives explicit and tested.

Required `tools/check_package_contents.py` improvements:
- Load `library.json`.
- Require archive name `SSD1315-{version}.tar.gz` by default.
- Optionally accept `--archive <path>` for explicit package checks.
- Do not validate the newest tarball by mtime when the expected version archive
  exists.
- Add parser/self tests or a small unit test if the tool has test structure.

### 10. Harden Example CLI Parser Helpers

Current gap:
`examples/common/CommandHandler.h` has unchecked output arguments and permissive
integer parsing.

Required design:
- Validate output pointers and buffer sizes before writing.
- Replace `atoi` with `strtol` or equivalent bounded parsing.
- Reject partial tokens like `123abc`.
- Reject out-of-range values before narrowing to `int`.
- Preserve simple fixed-buffer behavior.
- Update CLI contract checks if needed.

## Required Documentation And Evidence Closeout

### 1. Update Maintained Hardware Matrix

Update `docs/SSD1315_HARDWARE_VALIDATION.md` so it is the source of truth.

Required:
- Fold in COM29 serial HIL and 8-hour soak facts from
  `docs/reports/hil-validation-COM29-20260623.md`.
- Preserve incomplete rows honestly:
  - visual evidence incomplete unless exact operator/media evidence is supplied
  - reset-pin evidence incomplete unless reset wiring and reset behavior are
    supplied
  - physical fault evidence incomplete unless tested safely
  - logic analyzer incomplete unless capture path is supplied
  - ESP-IDF hardware HIL incomplete unless run on hardware
- If the operator supplies S2/S3 visual confirmation, record:
  - operator name or identifier
  - exact date/time
  - exact branch and commit
  - framework and firmware target
  - board model
  - panel/module model
  - resolution
  - I2C address
  - SDA/SCL pins
  - bus speed
  - reset wiring
  - command coverage
  - visual result for each visual command
  - media path or explicit "operator-only, no media captured"
- Do not mark a row `PASS` from chat memory alone unless the exact required
  fields are present. Use `Operator-reported, evidence path unknown` if that is
  all that exists.

### 2. Update README And Readiness Summary

Required:
- Replace stale COM16/COM17-only wording with COM29 serial HIL and current
  CI/tag facts.
- Keep the boundary between:
  - host/native tests
  - CI builds
  - serial HIL
  - visual HIL
  - physical fault/reset validation
- Qualify the README feature wording currently implying all SSD1315 commands
  are exposed. Preferred wording:
  - "SSD1315 command constants and supported write-command helpers are exposed
     for the driver-supported command surface."
- If S2/S3 visual hardware checks are documented, state exactly what passed and
  what did not run.

### 3. CI Evidence

Required if documenting CI pass:
- Record:
  - GitHub workflow run URL
  - commit SHA
  - branch/tag
  - jobs that passed
  - date/time
- CI pass alone does not prove hardware behavior.

### 4. HIL Runner Evidence Quality

Evaluate whether the runner needs small improvements:
- `--strict` currently requires operator/board/panel/supply/pullups/reset/bus
  metadata. Keep or extend this.
- Consider adding `--require-visual-pass`:
  - exits nonzero if any visual command is `UNKNOWN`, `SKIPPED_SERIAL_ONLY`,
    `OPERATOR_REQUIRED`, or `FAIL`
  - only valid with `--interactive-visual`
- Consider adding `--field-ready-check`:
  - always conservative
  - requires strict metadata, visual pass, retention run, soak run, no serial
    failures, and no review rows
  - still reports physical fault/reset/logic analyzer as missing unless
    supplied through explicit metadata fields
- Keep `field_ready_claim_allowed` false unless the matrix evidence is truly
  complete.

### 5. Hardware Fault And Reset Evidence

Do not add unsafe hotplug or power-fault automation without fixture details.

If reset wiring is available, add example-only reset support, not core support:
- Add `pins::OLED_RESET = -1` in `examples/common/BoardConfig.h`.
- Add optional timings:
  - `OLED_RESET_ASSERT_MS = 10`
  - `OLED_RESET_RELEASE_DELAY_MS = 100`
- Add CLI command name: `hwreset`
  - only compiled/executed if `OLED_RESET >= 0`
  - toggles application-owned reset GPIO active-low
  - reinitializes or calls `recover()` after release
  - reports exact `Status`
  - documents that core driver does not own reset
- Add HIL matrix row and runbook command only if implemented.

If reset wiring is not available, keep the matrix row incomplete and record
`Reset pin not connected` or `unknown`.

## Acceptance Criteria

Code/API:
- External buffers are size-checked before I2C.
- Offline status is distinguishable from transient busy, or a documented
  compatibility decision explains why not.
- Flush timeout boundary is exact and tested.
- `pollFlush(now, 0, 0)` is a no-I2C query.
- Dirty/error clearing semantics are documented and tested.
- Checked bitmap drawing cannot read past caller data.
- Core guard catches framework leakage.
- Package checker validates the versioned release archive.
- Example parsers reject malformed integers and invalid output buffers.

Docs/evidence:
- `SSD1315_HARDWARE_VALIDATION.md` is no longer stale relative to COM29.
- README/readiness wording does not overclaim.
- CI pass is documented only with exact workflow evidence.
- S2/S3 visual/hardware claims name exact target facts.
- Missing physical fault/reset/logic-analyzer/sanitizer evidence remains
  explicitly marked incomplete.

Verification:
Run the smallest relevant checks after each code change, then the full release
set before final response:

```powershell
python tools\check_core_timing_guard.py
python tools\check_cli_contract.py
python tools\check_idf_example_contract.py
python scripts\generate_version.py check
python -m py_compile tools\run_ssd1315_hil.py tools\check_cli_contract.py tools\check_idf_example_contract.py tools\check_package_contents.py
python tools\test_hil_runner_parser.py
python tools\run_ssd1315_hil.py --dry-run --mode smoke
python tools\run_ssd1315_hil.py --dry-run --mode functional
python tools\run_ssd1315_hil.py --dry-run --mode retention
python tools\run_ssd1315_hil.py --dry-run --mode soak --soak-ops 10
python tools\run_ssd1315_hil.py --dry-run --mode all --soak-ops 10
python -m platformio test -e native
python -m platformio run -e esp32s3dev
python -m platformio run -e esp32s2dev
python -m platformio pkg pack
python tools\check_package_contents.py
git diff --check
```

If `idf.py` is available locally, also run:

```powershell
idf.py -C examples/espidf_basic set-target esp32s3
idf.py -C examples/espidf_basic build
idf.py -C examples/espidf_basic fullclean
idf.py -C examples/espidf_basic set-target esp32s2
idf.py -C examples/espidf_basic build
```

If hardware is connected and the operator is ready, run exact HIL commands with
strict metadata. Example for Arduino ESP32-S2/S3:

```powershell
python tools\run_ssd1315_hil.py --mode functional --port <PORT> --baud 115200 --out hil_logs --expect-address 0x3C --expect-width 128 --expect-height 64 --expect-controller SSD1315 --expect-panel-profile example-default-128x64-internal-charge-pump --interactive-visual --strict --operator <NAME> --board <BOARD> --panel <PANEL> --supply-voltage <VOLTS> --pullups <OHMS> --reset-wired <yes|no|unknown> --bus-speed 400000
python tools\run_ssd1315_hil.py --mode retention --port <PORT> --baud 115200 --out hil_logs --interactive-visual --strict --operator <NAME> --board <BOARD> --panel <PANEL> --supply-voltage <VOLTS> --pullups <OHMS> --reset-wired <yes|no|unknown> --bus-speed 400000
python tools\run_ssd1315_hil.py --mode soak --port <PORT> --baud 115200 --out hil_logs --expect-address 0x3C --expect-width 128 --expect-height 64 --expect-controller SSD1315 --expect-panel-profile example-default-128x64-internal-charge-pump --soak-ops 500 --soak-duration-hours 8 --strict --operator <NAME> --board <BOARD> --panel <PANEL> --supply-voltage <VOLTS> --pullups <OHMS> --reset-wired <yes|no|unknown> --bus-speed 400000
```

## Final Response Requirements

Report:
- Files changed.
- Findings fixed, ordered by severity.
- Tests and builds run, with pass/fail results.
- HIL commands run, exact target metadata, and evidence paths.
- What still prevents field-ready claims, if anything.
- Whether the next release should be patch, minor, or major, with the reason.

Keep the response concise and factual. Do not claim hardware or production
evidence that is not recorded in the repository.
