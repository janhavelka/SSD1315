# SSD1315 Production-Grade Follow-Up Report

Date: 2026-05-31
Branch: `hardening/ssd1315-industry-readiness`
Commit summary: no commit created in this working tree; changes are staged as local edits for review.

## 1. Audit Findings From Actual Code

- Core was already framework-neutral and transport-injected.
- `probe()` was already ACK-only and mapped only address NACK to `DEVICE_NOT_FOUND`.
- The init path was fixed SSD1315, not profile-based, and always sent SSD1315 `SET_IREF`.
- `DISPLAY_ON` was sent before the blocking GDDRAM clear.
- `begin()` and `recover()` were bounded blocking lifecycle calls, but public wording and latency tables were incomplete.
- Reset GPIO ownership was documentation-only; no core reset callbacks existed.
- Flush dirty-page retention existed, but tests did not prove golden init, command/data control bytes, retry payload, or chunking.
- Panel-control dirty state was missing for mid-sequence control failures.
- ESP-IDF CI existed, but the manifest required IDF `>=6.0.1` while CI used `release-v5.3`.
- Local `idf.py` was unavailable.
- Hardware validation had not been run and no matrix existed.

## 2. Implemented

- Added `ControllerProfile::SSD1315` and `Config::controllerProfile`; unsupported profiles are rejected.
- Kept the repository SSD1315-only and removed strong SSD1306 claims from public metadata.
- Moved final `DISPLAY_ON` until after SSD1315 init and optional GDDRAM clear.
- Added `Config::clearOnBegin` and `Config::clearOnRecover`; defaults preserve existing full-clear behavior.
- Added `controlStateDirty()` / `controlStateError()` and `SettingsSnapshot` fields.
- Marked panel control state dirty on failed init/recover/control-command paths; clear only after successful full control resync.
- Documented `end()` as a best-effort destructor-safe shutdown path and added tests that DISPLAY_OFF failures are retained in diagnostics.
- Hardened native ESP-IDF example with example-owned I2C mutex and nonblocking stdin.
- Aligned `idf_component.yml` with CI at IDF `>=5.3.0`.
- Consolidated duplicated native tests through a single implementation file.
- Added transaction-logging fake-transport tests for init sequence, control/data bytes, clear chunks, probe mapping, dirty control-state, flush retry, full-frame flush chunking, and out-of-bounds drawing guards.
- Added `docs/SSD1315_HARDWARE_VALIDATION.md`.
- Updated README/Doxygen/AGENTS with SSD1315 contracts, latency table, reset ownership, ACK-only probe, OLED retention caution, and subagent roles.

## 3. Intentionally Not Implemented

- No SSD1306-compatible profile was added. The current command profile sends SSD1315 `SET_IREF`; SSD1306 support needs a guarded profile plus hardware validation.
- No core reset callbacks were added. Reset GPIO ownership remains platform/application policy.
- No nonblocking init state machine was added. The new flags let production users skip the blocking GDDRAM clear while preserving the existing API model.
- No hardware validation results were claimed.

## 4. Public API / Behavior Changes

- New enum: `ControllerProfile::SSD1315`.
- New config fields: `controllerProfile`, `clearOnBegin`, `clearOnRecover`.
- New diagnostics: `controlStateDirty()`, `controlStateError()`, and matching `SettingsSnapshot` fields.
- `begin()` and `recover()` still default to init-and-clear behavior. Setting `clearOnBegin` or `clearOnRecover` false skips the full GDDRAM clear and marks framebuffer data dirty for redraw/flush.
- Init now keeps the panel off until after the clear/resync policy completes, avoiding display-on-before-clear boot artifacts.

Migration note: existing configurations continue to compile with default SSD1315 behavior. Code that assumes SSD1306 compatibility must treat this release as SSD1315-only until a controller profile is added.

## 5. Controller Compatibility Policy

Policy: SSD1315 only.

`probe()` is not identity. It sends NOP and checks ACK. SSD1306-like panels may respond at `0x3C`/`0x3D`, but ACK does not prove controller type and compatibility is not guaranteed.

## 6. Reset Ownership Contract

The core driver does not own `RES#`, GPIO APIs, bus recovery, pins, locks, or clock policy. `recover()` is software-only: probe, reinitialize SSD1315 controls, optionally clear GDDRAM, then mark framebuffer data dirty for redraw.

## 7. Lifecycle Blocking / Latency

Default 128x64 SSD1315:

| Path | I2C writes | Payload bytes | Timeout upper bound | Approx bus time @100 kHz | Approx bus time @400 kHz |
|------|-----------:|--------------:|---------------------|--------------------------:|--------------------------:|
| `begin()` / `recover()` with clear | 53 | 1112 | about `53 * i2cTimeoutMs` if every write consumes its timeout | about 105 ms | about 26 ms |
| `begin()` / `recover()` with clear disabled | 19 | 48 | about `19 * i2cTimeoutMs` if every write consumes its timeout | about 6 ms | about 1.5 ms |
| Full-frame flush, default budget | 32 | 1104 | `flushTimeoutMs` across ticks plus per-write timeouts | about 102 ms total bus occupancy | about 26 ms total bus occupancy |

## 8. Panel-Control Dirty State

`controlStateDirty` is set after transport failures in init/recover/control-command paths that can leave physical controller state uncertain. It is cleared only after successful `begin()` or `recover()` resync. Recovery recipe: call `recover()`, then redraw or `requestFlush()`.

## 9. Flush / Tick Budget Behavior

Flush remains tick-driven and budgeted by `byteBudgetPerTick`. Failed flushes preserve dirty state for unsent or partially sent pages. Retry sends the framebuffer bytes again. Full-frame default 128x64 flush is covered by native tests as 32 writes: 8 pages times 2 address writes plus 2 data chunks.

## 10. ESP-IDF Build / CI Status

- Native ESP-IDF example remains `app_main` + `driver/i2c_master.h`.
- Example transport now demonstrates shared-bus mutex locking.
- CLI stdin is configured nonblocking so `tick()` continues while idle.
- CI contains an ESP-IDF matrix for `esp32s3` and `esp32s2` using `release-v5.3`.
- Manifest now requires `idf: ">=5.3.0"`.
- Local pure `idf.py` build was not run because `idf.py` is not installed in this shell.

## 11. Commands Run

Passed locally:

- `python tools/check_core_timing_guard.py`
- `python tools/check_cli_contract.py`
- `python tools/check_idf_example_contract.py`
- `python scripts/generate_version.py check`
- `python -m platformio test -e native` (44 tests)
- `python -m platformio run -e esp32s3dev`
- `python -m platformio run -e esp32s2dev`
- `python -m platformio pkg pack`

Not run locally:

- `idf.py -C examples/espidf_basic set-target esp32s3 build`
- `idf.py -C examples/espidf_basic build`
- `idf.py -C examples/espidf_basic fullclean`
- `idf.py -C examples/espidf_basic set-target esp32s2 build`
- `idf.py -C examples/espidf_basic build`

Reason: `idf.py --version` fails because `idf.py` is not on PATH.

Formatting note: `clang-format` is not installed in this shell.

## 12. Hardware Validation Status

Not run. `docs/SSD1315_HARDWARE_VALIDATION.md` now provides the required matrix and command checklist. No field-grade claim should be made until that matrix is filled with representative hardware results.

## 13. Remaining Risks

- Pure ESP-IDF correctness still depends on CI or a local machine with ESP-IDF installed.
- SSD1306 compatibility remains unimplemented and unvalidated.
- Reset-pin behavior remains application-owned and must be validated on production hardware.
- Display analog defaults may differ from specific module vendor examples; production hardware validation must confirm contrast, COM mapping, VCOMH, precharge, IREF, and charge pump choices.
- ESP-IDF transport maps IDF transfer-level invalid-response/not-found errors to `I2C_BUS_ERROR` because the adapter cannot prove whether a NACK was address or data phase in that path.

## 14. Operator Verdict

Mergeable as a production-grade SSD1315 software contract follow-up after CI confirms the pure ESP-IDF jobs. Release-candidate status remains blocked on representative hardware validation.
