# SSD1315 IDF-Merged Industry-Readiness Audit

Date: 2026-05-29
Repository: `C:\Users\HonzovoSpectre\Documents\Projects\SSD1315`
Branch: `audit/ssd1315-idf-merged-industry-readiness`
Audit mode: report-only / no implementation
IDF merge classification: `QUALIFYING_IDF_MERGED`

## Executive Summary

The ESP-IDF port is merged into `main`, and the core is mostly framework-neutral with injected write-only I2C transport, dirty-page flushing, tick-budgeted transfers, and useful display diagnostics. The IDF example is native IDF and does not use Arduino compatibility shims.

The readiness gaps are centered on display-specific production behavior: pure ESP-IDF builds are not proven, `begin()` performs a long blocking initialization/clear sequence, the init sequence turns the display on before clearing GDDRAM, IDF example timeout handling is misleading, and hardware validation is absent. The repo is not architecture-blocked, but it needs a targeted hardening pass before production claims.

## IDF Merge Evidence

- Default branch evidence: `origin/HEAD -> refs/heads/main`.
- Merge commit: `1c7e300b716caaa78b374a568adf050a026bbd8b`.
- Merge message: `Merge pull request #4 from janhavelka:feature/ssd1315-idf-port`.
- Merged branch tip: `19d98b7ab2c194a6f5732c9af347523d6de6d55c`.
- Branch ancestry: `feature/ssd1315-idf-port` is an ancestor of `main`.
- IDF artifacts present on `main`:
  - `CMakeLists.txt`
  - `idf_component.yml`
  - `examples/espidf_basic/main/main.cpp`
  - `examples/espidf_basic/main/CMakeLists.txt`
  - `examples/common/IdfI2cTransport.cpp`
  - `examples/common/IdfI2cTransport.h`
  - `tools/check_idf_example_contract.py`
  - `docs/IDF_PORT.md`
  - `docs/IDF_PORT_IMPLEMENTATION.md`
- Limitation: the native ESP-IDF example was not built with `idf.py` in this audit because `idf.py` is not installed or not on `PATH`.

## Readiness Classification

Engineering-grade with major gaps.

The design is reasonable for a display controller, but the long blocking initialization path, startup display-on-before-clear behavior, missing pure IDF build proof, and no hardware validation keep it below pre-production readiness.

## Scope Reviewed

- `include/ssd1315/`
- `src/`
- `examples/01_basic_bringup_cli/`
- `examples/espidf_basic/`
- `examples/common/`
- `test/`
- `tools/`
- `docs/`
- `README.md`
- `platformio.ini`
- `library.json`
- `CMakeLists.txt`
- `idf_component.yml`
- `.github/workflows/ci.yml`

## Datasheet / Documentation Sources Found

- `docs/SSD1315_datasheet.pdf`
- `docs/SSD1315_I2C_Command_Reference.md`
- `docs/Wisevision_X096-2864KSWPG01-H30_module_spec.pdf`
- `docs/IDF_PORT.md`
- `docs/IDF_PORT_IMPLEMENTATION.md`
- `docs/extracted-md/*.md`

## Scorecard

| Area | Rating | Notes |
| --- | --- | --- |
| IDF merge evidence | Strong | Merge commit, branch ancestry, IDF artifacts, and docs are present. |
| Core framework neutrality | Strong | No framework headers or direct framework APIs found in core. |
| I2C ownership/injection | Good | Write callback is injected; optional write-read callback exists but is unused internally. |
| ESP-IDF component correctness | Medium | Component files exist; no pure IDF build proof. |
| ESP-IDF example correctness | Medium | Native IDF, but init timeout parameter is ignored. |
| Status/error model | Good | Status-returning APIs and health exist; probe identity is limited by lack of ID register. |
| Timing/determinism | Medium | Tick flush is budgeted, but `begin()` and `waitFlush()` can block for many transactions. |
| Device-specific correctness | Medium | SSD1315 docs exist; init order should keep display off until GDDRAM is cleared. |
| Partial hardware state handling | Good | Dirty flags are preserved on flush failure per docs/tests; init partial state needs review. |
| Health/recovery behavior | Good | Health and recovery paths exist. |
| Thread/ISR contract | Good | README and public header document single-threaded behavior; ISR contract could be stronger in Doxygen. |
| Tests/fault injection | Medium | 31 native tests pass; duplicate test files and write-focused fault injection remain. |
| ESP-IDF build coverage | Weak | Pure IDF build missing locally and in CI. |
| Arduino ESP32-S2/S3 readiness | Good | `esp32s3dev` and `esp32s2dev` PlatformIO builds passed. |
| Documentation honesty | Good | README labels IDF example as bring-up and notes no display hardware validation. |
| Hardware validation | Unknown | No hardware commands were run in this audit. |

## What Is Strong

- Core uses injected I2C write callback and does not include Arduino/ESP-IDF headers.
- Copy construction/assignment are explicitly deleted (`include/ssd1315/SSD1315.h:115`).
- The driver supports external framebuffer injection for deterministic memory ownership (`README.md:131`, `README.md:155`).
- Dirty-page and dirty-column tracking are documented and implemented (`README.md:230`, `include/ssd1315/SSD1315.h:997`).
- Flush failure is intended to preserve dirty state and store the last error (`docs/SSD1315_I2C_Command_Reference.md:246`).
- The IDF CLI keeps ticking before stdin polling (`examples/espidf_basic/main/main.cpp:375`, `examples/espidf_basic/main/main.cpp:376`, `examples/espidf_basic/main/main.cpp:382`).
- README explicitly states the IDF example is native bring-up and not complete visual hardware validation (`README.md:398`, `README.md:401`).

## High-Severity Findings

### H1. Pure ESP-IDF build is not validated locally or in CI

Severity: High

Evidence:
- `CMakeLists.txt`, `idf_component.yml`, and `examples/espidf_basic/` exist on `main`.
- CI runs PlatformIO workflows but no `idf.py build` job was found.
- Local command `idf.py --version` failed: `The term 'idf.py' is not recognized as the name of a cmdlet, function, script file, or operable program.`

Impact:
- Native ESP-IDF consumers can receive code that passes PlatformIO but fails in IDF CMake, component requirements, or IDF target builds.

Recommended remediation:
- Add CI jobs:
  - `idf.py -C examples/espidf_basic set-target esp32s3 build`
  - `idf.py -C examples/espidf_basic set-target esp32s2 build`

Suggested tests:
- Pure IDF S2/S3 build matrix.
- Contract check for native IDF-only example dependencies.

### H2. `begin()` can monopolize the bus with a long blocking init and clear sequence

Severity: High

Evidence:
- `begin()` validates, allocates/selects framebuffer, then applies config (`src/SSD1315.cpp:551`, `src/SSD1315.cpp:640`, `src/SSD1315.cpp:680`).
- `_applyConfig()` runs `initDisplay()` then `clearGddram()` (`src/SSD1315.cpp:477`, `src/SSD1315.cpp:482`).
- `initDisplay()` is about 18 command transactions; `clearGddram()` is blocking and for 128x64 writes 1024 bytes (`src/SSD1315.cpp:740`, `src/SSD1315.cpp:826`, `src/SSD1315.cpp:828`).
- Timing audit estimates typical `begin()` as 1 probe + 18 init writes + 34 clear writes = about 53 I2C writes.

Impact:
- On a shared I2C bus, startup can block other devices for many transactions. This is acceptable only if documented as a blocking init phase or split into a tick-driven init state machine.

Recommended remediation:
- Document `begin()` as blocking with transaction counts and bus occupancy.
- Consider optional nonblocking init/clear if production use requires the display to coexist with sensors during boot.
- Ensure examples label this as bring-up behavior, not a shared-bus production manager.

Suggested tests:
- Native transaction-count tests for `begin()` at supported geometries.
- Hardware timing measurement at 100 kHz and 400 kHz.

## Medium-Severity Findings

### M1. Init sequence turns display on before clearing GDDRAM

Severity: Medium

Evidence:
- `_applyConfig()` calls `initDisplay()` before `clearGddram()` (`src/SSD1315.cpp:477`, `src/SSD1315.cpp:482`).
- `initDisplay()` sends `DISPLAY_ON` (`src/SSD1315.cpp:814`, `src/SSD1315.cpp:815`).
- `clearGddram()` runs only afterward (`src/SSD1315.cpp:826`).

Impact:
- The panel can briefly show stale/random RAM during startup before GDDRAM is cleared. On production devices this looks like a boot artifact and can be unacceptable for user-facing displays.

Recommended remediation:
- Keep display off through init, clear GDDRAM, then send `DISPLAY_ON`.
- Add a test that asserts init command order.

Suggested tests:
- Fake-bus command-order test: `DISPLAY_OFF/init`, clear writes, then `DISPLAY_ON`.
- Hardware startup visual check on the target module.

### M2. IDF example `initWire()` accepts timeout but ignores it

Severity: Medium

Evidence:
- `examples/common/IdfI2cTransport.cpp:62` accepts `timeoutMs`.
- `examples/common/IdfI2cTransport.cpp:63` explicitly casts it unused.
- Transaction callbacks later use per-call timeout values, so the naming is misleading rather than completely broken.

Impact:
- Example users may think the init-time timeout configures bus timeout policy when it does not. That is poor diagnostic honesty for production examples.

Recommended remediation:
- Rename the parameter, remove it, or document that only per-transaction `Config::i2cTimeoutMs` controls transfer timeouts.

Suggested tests:
- IDF example contract check for ignored timeout parameters.

### M3. Dynamic framebuffer allocation remains a bring-up convenience

Severity: Medium

Evidence:
- `begin()` allocates a framebuffer if `externalBuffer == nullptr` (`src/SSD1315.cpp:640`, `src/SSD1315.cpp:644`).
- External buffer injection is supported and documented (`README.md:131`, `README.md:155`).

Impact:
- Heap allocation at initialization can fail or fragment constrained systems. The status path can report failure, but production firmware should own memory deterministically.

Recommended remediation:
- Keep allocation support, but classify it clearly as simple/bring-up mode.
- Make production examples use `externalBuffer`.

Suggested tests:
- Native test for allocation failure if injectable allocator support is added.
- Example contract that production template uses external buffer.

## Low-Severity Findings

### L1. Address validation is broader than the documented SSD1315 module addresses

Severity: Low

Evidence:
- Local docs define SSD1315 module I2C addresses as 0x3C/0x3D from SA0.
- The driver accepts any non-reserved 7-bit address according to device-specific audit.
- SSD1315 has no ID register, so an ACKing wrong device can be probed as present.

Impact:
- Sending OLED commands to the wrong ACKing device is possible if the address is misconfigured.

Recommended remediation:
- Default to allowing only 0x3C/0x3D unless the user opts into advanced/custom address mode.
- Document that `probe()` is ACK-only.

Suggested tests:
- Invalid address tests for strict/default mode.

### L2. Native tests are duplicated

Severity: Low

Evidence:
- Both `test/test_basic.cpp` and `test/native/test_basic.cpp` exist and are near-duplicates according to tests/CI audit.
- PlatformIO collected one native suite and reported 31 cases.

Impact:
- Maintenance risk: future changes can update one copy and not the other.

Recommended remediation:
- Consolidate duplicate tests or document why both paths exist.

Suggested tests:
- CI guard that only the intended native test path is collected.

### L3. Move operations are not explicitly deleted

Severity: Low

Evidence:
- Copy construction/assignment are deleted (`include/ssd1315/SSD1315.h:115`).
- Move operations are not explicitly declared.

Impact:
- They are likely suppressed by the user-declared destructor/deleted copy operations, but explicit deletion would make the contract obvious.

Recommended remediation:
- Explicitly delete move constructor and move assignment.

Suggested tests:
- Compile-time `static_assert` checks for non-copyable and non-movable behavior.

## Device-Specific Correctness Checklist

| Item | Status | Notes |
| --- | --- | --- |
| SSD1315-specific init | PARTIAL | Init exists, but display-on-before-clear should be fixed. |
| Module geometry 128x64 | PASS/PARTIAL | Supported; hardware validation missing. |
| I2C address/control-byte format | PASS | Command/data control bytes are used. |
| Reset/power sequence | PARTIAL | No core GPIO ownership, but startup command order needs hardening. |
| Charge pump/external VCC | PARTIAL | Config exists; hardware validation missing. |
| Framebuffer ownership | PARTIAL | External buffer supported; default heap allocation remains. |
| Allocation failure reporting | PASS | Allocation can return error; production docs should emphasize external buffer. |
| Full-frame/page flush transaction count | PASS/PARTIAL | Known and bounded, but should be tested/documented per geometry. |
| Chunking/timeouts/shared bus | PARTIAL | Chunking exists; init can still monopolize bus. |
| Dirty-page semantics | PASS | Dirty tracking and failure preservation are present. |
| Mid-flush NACK behavior | PASS/PARTIAL | Fake write failures exist; hardware validation missing. |
| Display modes/scroll | PARTIAL | APIs exist; hardware validation missing. |
| Reset GPIO ownership | NOT APPLICABLE | Core should not own GPIO; adapter/example layer owns board reset if any. |
| Hardware validation | UNKNOWN | No hardware commands were run. |

## API Latency / Blocking Model

| API | I2C transactions | Other waits | Worst-case bound | Notes |
| --- | ---: | --- | --- | --- |
| `begin()` | About 53 writes for 128x64 typical init/clear | Allocation if no external buffer | Per write bounded by `i2cTimeoutMs`; total can be large | Blocking. |
| `tick(nowMs)` | 0 or several writes depending on flush state/budget | None | Budgeted by `byteBudgetPerTick` and per-write timeout | Data send may chunk at 64 bytes. |
| `requestFlush()` | 0 | None | O(pages) local state | Actual transfer in `tick()`. |
| Full-frame flush | About 32 writes at 128x64 with default budget | Display-on delay may defer | Bounded by `flushTimeoutMs` across ticks | Dirty pages only. |
| `waitFlush()` | Repeated `tick()` until done | Optional yield; display-on delay | `timeoutMs` or `flushTimeoutMs`, with stall guard | Explicit blocking helper. |
| Drawing APIs | Usually 0 | None | CPU only | Some paths may wake display with `DISPLAY_ON`. |
| Command setters | Usually 1 command write | None | `i2cTimeoutMs` | Contrast/invert/scroll/etc. |
| `clearGddram()` | Address writes + data chunks | None | Per write `i2cTimeoutMs`; blocking | Used in init. |
| `probe()` | 1 NOP write | None | `i2cTimeoutMs` | ACK-only; no ID register. |
| `recover()` | Re-applies init/config | Display init/clear work | Similar to `begin()` | Request flush afterward if RAM redraw is needed. |

## Partial-State / Cache Consistency Assessment

Flush partial state is handled better than init partial state. The docs say NACK/timeout should stop flush, keep dirty flags, and store `lastError`, and fake write-failure tests exist. During `begin()`/`recover()`, the display can receive a partial init sequence or partial GDDRAM clear before failure. The driver returns `Status`, but production docs should state what on-panel state may remain and whether the caller should power-cycle, recover, or retry clear.

## Tests and Build Coverage

Run locally:
- `git status --short`: clean before report edits.
- `python --version`: `Python 3.13.12`.
- `python -m platformio --version`: `PlatformIO Core, version 6.1.19`.
- `python tools/check_core_timing_guard.py`: PASS, `Core timing guard PASSED`.
- `python tools/check_cli_contract.py`: PASS, `CLI contract PASSED`.
- `python tools/check_idf_example_contract.py`: PASS, `IDF example contract PASSED`.
- `python scripts/generate_version.py check`: PASS, `include\ssd1315\Version.h` up to date.
- `python -m platformio test -e native`: PASS, `31 test cases: 31 succeeded`.
- `python -m platformio run -e esp32s3dev`: PASS, `SUCCESS`.
- `python -m platformio run -e esp32s2dev`: PASS, `SUCCESS`.
- `python -m platformio pkg pack`: PASS, wrote `SSD1315-1.2.0.tar.gz`; tarball removed after audit.
- `idf.py --version`: FAIL, command not found.

Present in CI:
- PlatformIO S2/S3 builds.
- Native tests.
- Core timing guard.
- CLI contract.
- `pio pkg pack`.

Not run:
- `idf.py -C examples/espidf_basic set-target esp32s3 build`: not run because `idf.py` is unavailable.
- `idf.py -C examples/espidf_basic set-target esp32s2 build`: not run because `idf.py` is unavailable.
- Hardware validation: not run.

Missing:
- Pure IDF build in CI.
- Direct CI invocation of `tools/check_idf_example_contract.py`.
- Init command-order test.
- Hardware display validation matrix.
- Consolidation or explanation of duplicate native tests.

## ESP-IDF Port Assessment

- Pure ESP-IDF component: Present.
- Pure ESP-IDF example: Present at `examples/espidf_basic`.
- Native IDF APIs, not Arduino: Yes, uses `app_main`, `driver/i2c_master.h`, fixed C buffer CLI, and native transport.
- External bus ownership: Yes. Example transport owns the IDF bus/device setup.
- Error mapping: Mostly good. IDF transport maps `esp_err_t` to `Status`.
- Timeout propagation: Per-transaction timeout is used, but `initWire(timeoutMs)` ignores its timeout parameter.
- Locking: Not a production shared-bus manager. External serialization is required.
- Task/tick behavior: Acceptable diagnostic pattern; `display.tick()` runs before stdin polling.
- CI IDF build: Missing.

## Documentation Assessment

Missing or incomplete documentation contracts:
- `begin()` transaction count and shared-bus impact by geometry and I2C speed.
- Startup GDDRAM clear/display-on behavior.
- Explicit production recommendation to use `externalBuffer`.
- Probe limitation: ACK-only because SSD1315 has no ID register.
- Pure IDF build status and ESP-IDF version.
- Hardware validation matrix with actual panel/module results.

## Hardware Validation Needed

| Scenario | Target | Expected evidence |
| --- | --- | --- |
| Cold boot visual behavior | Wisevision 128x64 module | No stale/random flash before clear. |
| Init at 100/400 kHz | ESP32-S2/S3 | Transaction timing and no timeouts. |
| Full-frame flush | 128x64 | Measured bus time and visual correctness. |
| Dirty-rect flush | Single page/partial columns | Only expected pixels update. |
| Mid-flush NACK | Fault jig or fake | Dirty flags preserved; retry succeeds. |
| External buffer | Static buffer | No heap allocation path used. |
| Sleep/wake/display-on delay | Hardware | `displayOnDelayMs` behavior verified. |
| Scroll modes | Hardware | Framebuffer corruption warning validated or mitigated. |

## Recommended Implementation Plan

### P0 - Must fix before production claim

- Add pure ESP-IDF S2/S3 CI builds.
- Fix init ordering so GDDRAM clear occurs before `DISPLAY_ON`, or document why the current order is required.
- Document `begin()` as blocking with transaction counts and shared-bus impact.
- Add hardware validation matrix with actual panel results.

### P1 - Should fix before release/merge

- Make IDF `initWire()` timeout behavior honest.
- Add init command-order and transaction-count tests.
- Consolidate duplicate native tests.
- Explicitly delete move operations.

### P2 - Nice hardening / later

- Add optional nonblocking init/clear state machine if shared-bus startup latency matters.
- Add strict address mode for 0x3C/0x3D with opt-in custom address support.
- Add production template using external framebuffer and external bus lock.

## Proposed Branch for Future Hardening

`hardening/ssd1315-industry-readiness`

## Final Verdict

Not ready for an industry-grade claim as-is. The architecture is workable, but pure IDF build proof, startup sequencing, blocking init documentation, and hardware display validation are still required.
