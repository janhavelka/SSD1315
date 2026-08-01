# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed

- Detect pioarduino's nested ESP-IDF Xtensa toolchain layout during PlatformIO
  builds. Affected Windows installations placed the compiler below
  `toolchain-xtensa-esp-elf/xtensa-esp-elf/bin` while PlatformIO searched only
  the package-level `bin`, causing `xtensa-esp32s2/3-elf-g++` not-found errors.
- Prevent blocking `begin()`/`recover()` compatibility calls from mistaking
  confirmed operation progress for a stalled injected clock. Minimum payload
  budgets can now complete the documented 1,058-callback 128x64 resync while a
  clock remains constant; a genuinely stalled display-on interval still fails
  deterministically.
- Configure Arduino `Wire` frequency in the fallible `begin(sda, scl,
  frequency)` call. The Arduino-ESP32 NG I2C HAL used by both pinned stacks
  can apply a later `setClock()` change but report failure when no device
  handles exist; the redundant call made reliable initialization checks
  impossible and stopped the first hardened HIL firmware before display init.
- Accept the stable unquoted/reordered `idf_component.yml` serialization
  produced by pioarduino 55.03.311 and ignore its root-manifest backup so
  repeated Arduino builds do not leave misleading metadata changes.

### Changed

- Removed unused example-only wrapper headers, styling/recovery helpers,
  ESP-IDF bookkeeping, and cached scroll state. Removed tracked historical
  package archives and ignored future generated `SSD1315-*.tar.gz` artifacts;
  release archives remain recoverable from Git history/releases.
- Pinned Arduino builds to immutable pioarduino `55.03.311` (Arduino-ESP32
  3.3.11, ESP-IDF libraries 5.5.5) and selected the exact
  `esp32-s3-devkitc1-n16r8` definition for 16 MB flash and 8 MB octal PSRAM.
  Added `compat_pioarduino_54_s3` as a build-only previous-stack check.
- Added exact MCU, Arduino core, ESP-IDF, flash, and PSRAM identity to the
  diagnostic CLI and HIL expectations, including parser regression coverage.
- Expanded CI to build the current and previous Arduino stacks and native
  ESP-IDF examples with both v5.3.5 and v5.5.5.
- Added host tests for the Arduino Wire adapter's initialization, transaction
  capacity, partial-write handling, and exact error mapping.

### Documentation

- Corrected page-buffer Doxygen examples so a completed page is advanced only
  after its cooperative flush result is consumed. Clarified the 17-callback/
  20-command initialization sequence, multi-transaction callback bounds,
  counted health diagnostics, deprecated compatibility APIs, application-owned
  pins, and the fade interval encoding.
- Re-audited the retained controller and Wisevision PDFs. Split external-VCC
  and internal-charge-pump sequencing, documented the `0x8D,0x10` shutdown
  erratum and the `12.5 µA` OCR loss, and clarified I2C continuation semantics.
  Raw PDF transcripts remain preserved for search and AI-assisted review.
- Refreshed release-candidate/CI status and native ESP-IDF version, target, and
  example-default documentation.

### Validation

- For the current release-preparation pass, local checks passed 124/124 native
  tests, 38/38 HIL-parser tests, all HIL dry-run modes, timing/CLI/ESP-IDF/
  version guards, warning-free Doxygen, package-content validation, core
  cppcheck warnings/portability analysis, current ESP32-S2/S3 Arduino builds,
  and the previous-stack ESP32-S3 compatibility build. No new device HIL was
  performed for this pass.
- Recorded the green nine-job post-merge CI run for `418f71e`, including
  package/Doxygen checks, native tests, both Arduino stacks, and ESP32-S2/S3
  native ESP-IDF builds on v5.3.5 and v5.5.5.
- Added the dated pioarduino 55.03.311 COM21 report. Exact ESP32-S3 N16R8
  identity, smoke, combined functional/retention cleanup, benchmark, the
  77-command Arduino plan before and after soak, and a measured 97,000-
  operation hour passed serial validation with zero driver failures, resets,
  serial interruptions, or heap change. Visual, electrical, reset-pin,
  physical-fault, and production-owner qualification remain open.

## [4.0.1] - 2026-07-22

### Fixed

- Made Arduino diagnostic command matching exact, so aliases such as `ver` no
  longer swallow `verbose`, and `flush`/`fill` no longer swallow
  `flushrect`/`fillrect`; the advertised `?` help alias now works.
- Added deterministic ESP-IDF self-test counters and made the HIL runner fail
  fast on unknown-command/wrong-firmware responses.
- Made expected scan-address checks parse only scanner grid rows instead of
  accepting address examples from the footer.
- Aligned the executable retention plan with the documented ghosting-isolation
  sequence. Duration soaks now record and enforce measured duration, compare
  telemetry uptime/heartbeat/reset trends, and preserve exact argv and
  expectations in evidence metadata.
- Added an Arduino-only extended HIL plan for safe diagnostics, lifecycle
  compatibility controls, graphics primitives, partial flush, page iteration,
  software reset, and explicit regression coverage of command-name collisions.
- Fixed the Arduino `pageiter` diagnostic so full-buffer iteration completes a
  full flush before reporting success; the old one-tick path could leave dirty
  or unsynchronized GDDRAM and make a subsequent wake fail safely.
- Hardened duration soaks against bounded host USB-serial interruptions. Only
  `version`, `telemetry`, and `cfg` may be retried on the existing serial handle
  after missing or truncated output under explicit `--soak-read-retries`; the original
  deadline continues, port reopen/reset, mutating/display commands, host
  exceptions, and explicit device failures are never hidden, and every retry
  remains in the evidence artifacts. Telemetry validation now requires uptime
  and loop heartbeat to increase between samples instead of accepting a
  stalled counter.
- Split timed-soak execution into a one-time identity/config prologue, bounded
  repeated stress/telemetry/clear body, and final config cleanup so verbose
  diagnostics do not dominate or prematurely fail the endurance workload.
- Added a shared Arduino/ESP-IDF `soakstep` diagnostic that reuses the mixed-
  stress owner but emits one complete count/health record per batch. The runner
  requires that full record, rejects unhealthy driver deltas, and exits nonzero
  when requested duration or final cleanup evidence is incomplete. Firmware and
  host tooling reject missing, out-of-range, negative, or non-finite soak
  bounds before starting unbounded work.
- Flushed each Arduino diagnostic response at the command boundary and reduced
  the machine-readable soak result to a newline-terminated record shorter than
  one 64-byte USB CDC packet. The runner now stops after any unrecovered
  timeout, including one with partial output, instead of risking
  cross-transaction result attribution.
- Added CI dry-run coverage for the benchmark and Arduino extended HIL plans,
  and made release-package validation require the current COM21 report.

### Documentation

- Corrected `waitFlush()` Doxygen: it can invoke only the injected cooperative
  yield hook; there is no hidden platform yield.
- Added the COM21 ESP32-S3 v4 serial-HIL report, including a measured
  96,500-operation hour and post-soak 77-command pass; simplified the
  maintained validation ledger; and refreshed the TunnelMonitor Prompt 45L
  integration gates without promoting ACK-only or serial evidence to hardware
  identity or field qualification.

## [4.0.0] - 2026-07-22

This is a breaking transport and lifecycle release.

### Added

- Added passive `attach()`/`detach()` binding and one fixed cooperative
  initialize, flush, sleep, wake, resync, shutdown, and hardware-scroll setup
  state machine.
- Added `OperationOptions`, `OperationProgress`, consume-once `OperationResult`,
  nonzero request identity, absolute deadlines, zero-I2C cancellation, effect
  certainty, and command-confirmed modeled panel power state.
- Added `Config::maxWriteBytes` with validation in `[4..129]`, allowing a
  129-byte control-plus-full-page write when the application transport supports
  it.
- Added bounded `drawTextN()`/`getTextWidthN()`, `markDirtyRect()`, framebuffer
  sizing, write-capacity, operation, effect, and power-state helpers.
- Added owner-safe cooperative horizontal/vertical scroll setup with three
  explicit phases and retained the old bounded calls as advanced compatibility.

### Changed

- **Breaking:** `I2cWriteFn` now returns terminal `TransportResult` instead of
  general `Status`. Each callback permits at most one physical transaction and
  must not retry, recover, back off, or replay an ambiguous write. Bus ownership
  and any required serialization remain application policy.
- **Breaking:** shared-bus owners should use
  `attach()`/`start...()`/`pollOperation()`/`takeOperationResult()`.
  `begin()` and `recover()` remain bounded blocking compatibility facades over
  the same state machine.
- `pollOperation()` permits at most eight callback invocations per call; the normal
  owner contract is one; deadline-bearing operations execute at most one per
  poll. A 128x64 initialize-off sequence is 17 callbacks.
  Full resync is 42 callbacks at capacity 129/payload budget 128 and 50 at
  the default capacity 65, followed by a zero-I2C display-on interval.
- `detach()`, `end()`, and destruction now perform zero I2C. Applications must
  explicitly complete `startShutdown()` before release when hardware shutdown
  is required.
- Drawing and activity bookkeeping are memory-only. Core `tick()` no longer
  admits automatic sleep or page-cycle policy.
- `OFFLINE` is diagnostic-only and no longer gates operation admission or owns
  recovery policy.
- Successful raw command passthrough invalidates cached panel-control, power,
  and GDDRAM certainty; full resync restores command-confirmed modeled state.
- Initialization now explicitly restores the full-height vertical-scroll area,
  fade-off, zoom-off, and scroll deactivation while retaining 17 callbacks.
- Wake rejects dirty or incompletely populated GDDRAM and never hides a flush.
  The invariant is rechecked immediately before display-on, and page-buffer
  mutation during transfer is retried before advancing its window.
- Rebinding rejects active work, unconsumed results, and active legacy flushes;
  `detach()` remains the explicit local-state discard path.
- `firstPage()` now returns `Status` and refuses to erase active flush/retry
  state. Failed page windows remain selected for retry, while clean completed
  flush state is normalized. Legacy blocking scroll calls use the cooperative
  operation state model.
- Corrected precharge documentation: codes `1..15` encode `2..30` DCLKs.
- Transport health changes only after callback-backed work, never after a
  zero-I2C deadline, empty flush, or local display-on invariant rejection;
  cooperative flush/resync publishes health exactly once at terminal result.
- Page-buffer mode initializes off, does not support full-buffer resync, and
  requires owner-driven page iteration/flush followed by explicit wake. Every
  fresh window is fully dirty until written, even when drawing is partial.
- Opaque command lists are never split at command/argument boundaries; capacity
  failures are zero-I2C. Owned-buffer rebinding rejects every overlapping range.
- Successful shutdown now leaves every power profile locally uninitialized;
  caller invalidation cancels active work while preserving its terminal result.
- Direct commands and legacy flush paths now remain BUSY/zero-I2C while a
  cooperative terminal result awaits consume-once retrieval.
- `nextPage()` and `clearDirtyIfIdle()` also preserve active/unconsumed
  cooperative ownership instead of advancing or discarding dirty state.
- Ambiguous raw-command failures invalidate modeled control, power, and GDDRAM;
  address NACK retains the definite-no-effect model. Direct wake refuses dirty
  control state.

### Deprecated

- `clearOnRecover`, auto-sleep, and page-cycle configuration/accessors remain
  compatibility storage only. Application policy should schedule explicit
  operations.
- Blocking `begin()` and `recover()` remain available for compatibility but are
  not the production shared-bus-owner interface.

### Removed

- Removed the unused `I2cWriteReadFn`/`Config::i2cWriteRead` hook from the
  write-only SSD1315 transport contract.

### Fixed

- Pinned the Arduino PlatformIO environments to the immutable pioarduino
  `54.03.20` release archive so clean CI runners resolve the same platform as
  developer machines instead of looking for a nonexistent registry version.

### Documentation

- Removed completed task prompts and the superseded 91-test internal stress
  report after preserving current contracts and validation status in maintained
  documentation and the 118-test suite.
- Replaced the completed TunnelMonitor suitability-audit diary with a concise,
  package-excluded integration-gate document containing only current target
  facts, blockers, and required adoption validation.
- Tightened Doxygen into a warning-as-error public-API completeness gate,
  documented previously uncovered enum aliases, version hooks, operation
  results, framebuffer overloads, and settings snapshot fields, and excluded
  internal historical prompts from generated API pages.
- Completed the README public API index, clarified the legacy `nextPage()`
  error contract, refreshed contribution/security guidance, and made the
  README-linked COM29 evidence report an explicit release-package member.
- Renamed the supporting documentation index to `docs/DOCUMENTATION.md` so it
  has an unambiguous generated Doxygen page instead of colliding with the root
  README main page.
- Recorded dispositions and native-test traceability for TunnelMonitor
  suitability findings H-02 through H-11. The final working-tree native rerun
  passed 118 of 118 tests; H-01 remains blocked on exact module and
  electrical-profile evidence.
- Kept TunnelMonitor integration deferred pending its 2500 ms operation versus
  1250 ms result-lifetime conflict, removal of retry-capable OLED writes,
  immutable dependency selection, exact builds, and representative HIL.
- Labeled Arduino and ESP-IDF examples as bring-up diagnostics rather than
  production shared-bus templates and documented `pio test -e native` as the
  host/native validation command.

## [3.0.0] - 2026-06-29

### SemVer
- Version metadata is set to `3.0.0` because latched
  offline operations now return `DRIVER_OFFLINE` instead of `BUSY`, which can
  affect callers that branch on exact status codes.

### Added
- Added `Config::externalBufferSizeBytes` and native regressions so caller-owned
  framebuffers are size-checked before any I2C transaction.
- Added `Err::BUFFER_TOO_SMALL` and `Err::DRIVER_OFFLINE` for distinguishable
  caller-buffer and latched-offline diagnostics.
- Added checked `drawBitmap(..., bitmapSizeBytes, ...)`, `clearDirtyIfIdle()`,
  and `clearLastError()` while keeping legacy compatibility helpers.
- Added Arduino and native ESP-IDF HIL `telemetry` output for uptime, loop
  heartbeat, reset reason, free heap, and minimum free heap.

### Changed
- `pollFlush(nowMs, 0, 0)` is now a no-I2C query for active flushes, and flush
  timeout expiration uses the exact `elapsed >= flushTimeoutMs` boundary.
- Generated version metadata now defaults to deterministic `"unknown"` build
  timestamp strings unless `SOURCE_DATE_EPOCH` or compile-time overrides are
  supplied.
- HIL verdicts now treat review-required serial rows and skipped visual checks
  conservatively, and generated matrix fragments include strict metadata fields.
- Maintained hardware-validation docs now fold in COM29 serial functional,
  retention, benchmark, 8-hour soak, and post-soak cleanup evidence without
  claiming visual, reset, fault, or logic-analyzer coverage.
- HIL duration soak now finishes the current command cycle and final cleanup
  before exiting after the requested duration.
- HIL contract checks, README, runbook, and hardware-validation matrix now use
  the telemetry-bearing functional command sequence.

### Fixed
- Hardened the example CLI parser against null output buffers, zero buffer
  sizes, malformed integer tokens, and narrowing overflow.
- Expanded the core guard against framework leakage in `include/` and `src/`.
- Made package validation check the `library.json` versioned archive by default
  instead of the newest archive by modification time.

## [2.1.0] - 2026-06-23

### Added
- Added `pollFlush(nowMs, maxInstructions, byteBudget)` and
  `getFlushStatus()` for explicit OLED instruction and data-budget ownership.
- Added native regressions for split address-window polling, independent byte
  and instruction budgets, flush timeout across display-on delay, and
  external-buffer ownership.
- Added native stress regressions for zero-instruction flush polling, changing
  instruction/byte budget matrices, page-address failure retry, and hostile
  drawing/flush-rect inputs with external-buffer guard checks.
- Added release package validation for the source PDFs, full extracted PDF text,
  and compact `docs/extracted-md/00` through `08` chip notes.
- Added dated HIL and internal stress audit reports under `docs/reports/`.

### Changed
- Framebuffer flushing now treats column-address, page-address, and data
  transfers as separate bounded instructions while preserving dirty-page retry
  behavior.
- Arduino CLI bring-up now demonstrates a caller-owned static framebuffer
  through `Config::externalBuffer`.
- Documentation map now lists each compact chip documentation note explicitly as
  source evidence.
- Public chip-behavior docs now preserve ACK-less module caveats, electrical and
  reset limits, raw-command warnings, continuation control bytes, and
  content-scroll delay ownership.

## [2.0.0] - 2026-06-01

This is the next real release after `1.2.0`.

### Added
- ESP-IDF component metadata and a native `examples/espidf_basic` application
  using `app_main`, fixed-buffer CLI input, display-specific command handlers,
  and `driver/i2c_master.h`.
- ESP-IDF port implementation notes and contract checks.
- Example-local ESP-IDF I2C/timing/yield transport glue under
  `examples/common/`.
- `ControllerProfile::SSD1315`, lifecycle clear policy flags
  (`clearOnBegin`, `clearOnRecover`), and panel-control dirty diagnostics
  (`controlStateDirty()`, `controlStateError()`).
- `PanelProfile` and `applyPanelProfile()` for documented 128x64 SSD1315
  panel/electrical presets, including generic internal-charge-pump and
  Wisevision internal-DC/DC or external-VCC profiles.
- Golden native tests for SSD1315 init bytes, command/data control bytes,
  clear chunking, probe mapping, flush retry, and panel-control dirty state.
- Golden native tests for panel profiles, SSD1315 address/contrast validation,
  corrected scroll command sequences, scroll-active flush blocking, and
  charge-pump shutdown behavior.
- `docs/SSD1315_HIL_RUNBOOK.md`, `docs/SSD1315_HIL_TARGET_TEMPLATE.md`,
  `docs/SSD1315_HARDWARE_VALIDATION.md`, and `docs/SSD1315_READINESS_SUMMARY.md`
  for repeatable HIL preparation, release gates, and final hardware result
  capture.
- `tools/run_ssd1315_hil.py` serial runner for the shared Arduino/ESP-IDF HIL
  smoke sequence. Visual commands are reported as operator checks, not automatic
  hardware passes.
- HIL runner device-test modes (`smoke`, `functional`, `retention`, `soak`,
  `all`) with JSON, CSV, Markdown, metadata, cfg snapshots, health delta,
  failure analysis, operator checklist, and hardware-matrix fragment artifacts.
- `tools/test_hil_runner_parser.py` for parser regression coverage.
- `tools/check_package_contents.py` for package artifact validation.

### Changed
- Promoted the SSD1315 industrial-hardening release line to a major version
  because public API contracts, lifecycle behavior, diagnostics, and validation
  tooling changed materially.
- Aligned PlatformIO, ESP-IDF component, generated version header, Doxygen, and
  changelog metadata on `2.0.0`.
- Public timing/yield documentation now requires framework adapters to inject
  timing and scheduler hooks; the core no longer calls platform runtime APIs.
- README memory guidance now shows caller-supplied framebuffer ownership for
  deterministic production use, clarifies that internal allocation is a
  convenience mode, and removes stale `byteBudgetPerTick == 0` guidance.
- README now states that `espidf_basic` has a separate native fixed-buffer CLI
  rather than reusing the Arduino CLI source, and calls out remaining IDF
  example parity gaps.
- PlatformIO metadata now declares ESP-IDF framework compatibility.
- The ESP-IDF example now exposes native display controls, graphics commands,
  diagnostics, stress tools, and self-test flow without Arduino compatibility
  shims.
- Arduino and native ESP-IDF bus scans now use the same table-style procedure
  and common-address hints as the other I2C example libraries.
- README and metadata now describe the repository as SSD1315-first and no
  longer claim generic SSD1306 compatibility.
- `begin()` and `recover()` keep the panel off until init and the configured
  GDDRAM clear policy complete; production users can disable lifecycle clears
  and resync through normal dirty flushing.
- Configured SSD1315 I2C addresses are now limited to `0x3C` and `0x3D`, and
  `probe()` remains ACK-only rather than an identity check.
- Contrast value `0` is now rejected; public docs and CLI validation use the
  SSD1315 command-table range `1..255`.
- SSD1315 scroll-speed enum labels and scroll command byte sequences now match
  the SSD1315 datasheet while preserving old raw-value aliases.
- Framebuffer flushes are blocked while hardware scroll is active; stopping
  scroll marks framebuffer data dirty for redraw/flush.
- Native ESP-IDF example transport now demonstrates mutex-serialized bus access
  and configures stdin nonblocking so display ticks continue while the CLI is
  idle.
- Public package wording was softened from production-grade to hardened until
  representative hardware/fault/soak validation is recorded.
- Operator documentation was consolidated: stale chunk reports and intermediate
  readiness reports were removed from the active docs set.
- `docs/README.md` now defines the maintained documentation set, source
  evidence policy, and validation-claim policy. One-off COM16/COM17 auditor
  reports and superseded exploration/ghosting reports were removed after their
  persistent conclusions were folded into the readiness summary and hardware
  matrix.
- ESP-IDF `cfg` output now exposes panel/profile, analog/timing, dirty/control,
  scroll, sleep/all-on, and health evidence fields similar to the Arduino CLI.
- Hardware scroll is now explicitly contracted to 128-column SSD1315 configs;
  non-128-wide configs can still draw/flush with configured-width address
  windows, but scroll setup returns `UNSUPPORTED`.
- Tightened Doxygen settings so documentation generation is visible in local and
  CI validation without relying on stale generated output.
- Clarified release and hardware-validation wording in the maintained readiness
  summary and hardware validation ledger.
- Replaced the hard-coded package-inspection tarball name in README validation
  instructions with a version-neutral placeholder.

### Fixed
- Arduino bring-up config now injects `nowMs`, `cooperativeYield`, and the
  optional write-read hook, preventing `waitFlush()` timeouts caused by a
  missing example clock source.
- `waitFlush()` no longer underflows elapsed-time math when called with a
  nonzero timestamp and no configured clock hook.
- Failed panel-control I2C operations now expose a dirty/resync diagnostic
  instead of relying only on transport health state.
- Dirty-page tracking now preserves pages for retry when framebuffer content is
  changed during an active flush, preventing older partial GDDRAM chunks from
  being treated as synchronized with newer clear/fill contents.
- Arduino and native ESP-IDF demo commands now establish a known display
  baseline and stop if the baseline clear/flush fails.
- HIL runner result parsing no longer treats successful counters such as
  `fail=0` or `FAIL:0` as command failures.
- HIL runner result parsing no longer treats harmless text such as
  `Last error: never` as a failure.
- `end()` now sends best-effort `DISPLAY_OFF` and internal charge-pump disable
  through a raw shutdown path even when the normal operation state is `OFFLINE`.
- `tick(0)` no longer bypasses `displayOnDelayMs`, and flush timeout tracking
  no longer wraps when a flush starts at timestamp zero.
- Vertical scroll offset validation now respects the active vertical scroll
  area configured by `setVerticalScrollArea()`.
- Page-buffer clear/fill semantics and display-off/recover/clear sequences now
  have direct native regression coverage.
- Arduino validation stress commands no longer intentionally send invalid
  contrast `0` values.
- Supporting docs were consolidated around `docs/README.md`,
  `SSD1315_READINESS_SUMMARY.md`, `SSD1315_HARDWARE_VALIDATION.md`, and
  `SSD1315_HIL_RUNBOOK.md`. Historical one-off reports were removed from the
  active docs set, and the remaining docs now state that serial HIL evidence is
  not the same as complete visual/fault/soak validation.

### Removed
- Removed stale one-off COM17 and industrial gap-closure reports from the active
  documentation set after folding their durable conclusions into the maintained
  readiness summary and hardware validation ledger.

## [1.2.0] - 2026-05-14

### Added
- `SettingsSnapshot` struct for reading cached configuration and runtime state without I2C.
- `getSettings(SettingsSnapshot&)` method to populate a settings snapshot.
- `Status::is(Err)` method for type-safe error code comparison.
- `Status::Ok()` and `Status::Error()` static factory methods on the `Status` struct.
- `Err::I2C_BUS` compatibility alias for `Err::I2C_BUS_ERROR`.
- `I2cWriteReadFn` callback type and `Config::i2cWriteRead` field for uniform upper-layer wiring (SSD1315 remains write-only internally).
- Native coverage proving latched `OFFLINE` blocks normal I2C operations without touching the bus while `recover()` remains the explicit recovery path.

### Changed
- Doxyfile project metadata now matches `library.json` and references the
  maintained docs tree instead of removed template files.
- Reference documentation now uses human-readable vendor PDF names and separates compact display notes from full PDF extractions under `docs/extracted-md/` and `docs/pdf-extracted-md/`.
- Explicit recovery bypass internals now use the shared `ScopedOfflineI2cAllowance` / `_reassertOfflineLatch()` procedure so failed recovery attempts that begin from `OFFLINE` keep the latch asserted.
- Public raw command helpers now require successful `begin()` and return `NOT_INITIALIZED` before any I2C if the driver is not active.
- Scroll, fade, vertical scroll area, and panel tuning enums are validated before command transmission.
- Health behavior is now standardized on latched `OFFLINE`: normal public operations return `BUSY` with `Driver is offline; call recover()` and do not touch I2C until `recover()` succeeds. The previous auto-recovery-on-any-success semantics were removed.

### Fixed
- ESP32-S2 example uploads now reset into the application after flashing, and
  USB CDC logging waits for the monitor before printing the bring-up CLI banner.
- Charge pump disable mode now uses `ChargePumpVoltage::OFF` instead of
  `DISABLED`, avoiding Arduino-ESP32's global `DISABLED` macro in ESP32 builds.
- Local I2C buffer/configuration errors are rejected before transport and no longer affect health counters.
- Health success/failure counters now saturate at `UINT32_MAX` instead of wrapping.
- `waitFlush()` now has a finite stalled-clock guard when an injected time source stops advancing.
- Flush error handling keeps dirty flags intact and records the failed flush exactly once.

## [1.1.3] - 2026-04-05

### Changed
- README quick-start code now matches the current `Status` helpers and `Err` enumeration.
- README configuration, error-code, and API reference sections now reflect the current public timing hooks, panel-tuning fields, diagnostics, and page-cycling helpers.

## [1.1.2] - 2026-04-03

### Added
- `Version.h` is exposed from the supported public headers so version constants are available from both `SSD1315.h` and `ssd1315/SSD1315.h`.

### Changed
- README documentation now reflects the current defaults, non-blocking page-buffer behavior, and shipped reference files.
- Public-header and metadata documentation now describe both the legacy shim include and the canonical `ssd1315/SSD1315.h` path.

### Fixed
- README wording for defaults and the `probe()` / `nextPage()` behavior now matches the implementation.

## [1.1.1] - 2026-04-03

### Added
- `inProgress()` convenience method on `Status` struct.

### Changed
- `I2cScanner.h`: standardized `#if defined(ESP32)` to `#if defined(ARDUINO_ARCH_ESP32)` for portability.
- `I2cTransport.h`: standardized all ESP32 preprocessor guards to `ARDUINO_ARCH_ESP32`.
- `Log.h`: added `LOGV` runtime-verbose macro, added `#ifndef LOG_SERIAL` guard for override support.

## [1.1.0] - 2026-03-01

### Changed
- Synchronized `docs/IDF_PORT.md` and public header declarations with current implementation state.
- Updated version-generation workflow script behavior and release metadata consistency.
- Cleaned and clarified public API Doxygen wording in `include/ssd1315/SSD1315.h` (tick timing docs, recovery state notes, and buffer format wording).
- Fixed malformed API doc examples and punctuation in page-buffer/flush documentation to match current non-blocking behavior.

## [1.0.2] - 2026-02-28

### Added
- Unified `examples/01_basic_bringup_cli` with shared `examples/common/*` framework components
- Cross-library CLI/timing contract checks and unification documentation
- Top-level compatibility include `include/SSD1315.h` and canonical header/source naming (`SSD1315.h`, `SSD1315.cpp`)

### Changed
- Example command/help appearance aligned with the common I2C CLI style while preserving deep diagnostics and colored key status output
- Stress and feature verification commands consolidated into the single canonical bringup example

### Removed
- Deprecated standalone example set (`examples/00_*`, `examples/01_page_buffer_mode`, `examples/02_*`) in favor of one comprehensive bringup CLI

## [1.0.1] - 2026-02-22

### Fixed
- **Critical:** `_flushCol` (`uint8_t`) wrapped to 0 when display width = 128, causing `_flushCol > _flushMaxCol` to never fire and flush to stall — widened to `uint16_t`
- **Critical:** `(1 << page)` used signed integer shift UB throughout dirty-page bitmask operations — changed to `static_cast<uint8_t>(1u << page)` everywhere
- `resetActivityTimer(0)` / `touch()` wrote the `tickAutoSleep` sentinel value `0` to `_lastActivityMs`, silently resetting the inactivity timer on every draw call — all write sites now go through `resetActivityTimer()` which guards against writing `0`
- `drawLine` Cohen-Sutherland clipping loop had no iteration bound — added `clipIter` guard (max 4 iterations, one per clip boundary); degenerate clamped endpoints now abort safely
- `drawCircle` / `fillCircle` midpoint algorithm used `int16_t` for `err`, `x`, `y` — overflows for radius > ~100 px; promoted to `int32_t`
- `drawBitmap` `byteWidth` calculated as `int16_t` — overflowed for very wide bitmaps; changed to `int32_t`
- `getTextWidth` accumulator was `int16_t` — overflowed on long strings; changed to `int32_t` with saturating clamp
- `_flushStartMs` used `~0u` (UINT32_MAX) as sentinel — replaced with explicit `bool _flushStarted` flag; immune to `millis()` rollover edge case
- `tickPowerOn` used `_powerOnMs == 0` as sentinel — `millis()` can return `0` at boot; guard now writes `1` instead of `0` in that edge case
- `_consecutiveFailures` (`uint8_t`) wrapped at 256 back to `0`, falsely triggering READY?DEGRADED transition again on the 257th failure — increment is now saturating at 255
- `waitFlush` tight loop had no cooperative yield — added `yield()` to feed the FreeRTOS watchdog without using the forbidden `delay()`
- `flushPageBlocking` was declared as a private method but never implemented — added full implementation (sets address window, sends page in 32-byte chunks)
- `_flushMinCol` / `_flushMaxCol` unnecessarily widened to `uint16_t` — reverted to `uint8_t`; hardware column addresses are bounded 0–127

### Changed
- `drawHLine`, `drawVLine`, `fillRect`, `drawChar`, `drawBitmap`, and all three test patterns rewritten to write directly to the framebuffer and call `wakeIfSleeping()` / `markDirty()` once per call instead of once per pixel — eliminates O(n) overhead of `millis()` and `markDirty()` per pixel
- `drawBitmap` inner column loop hoists the constant page-row base offset (`page × width`) outside the loop, removing a division and multiplication per pixel
- `drawText` now calls `resetActivityTimer()` and `wakeIfSleeping()` once after drawing the full string instead of per character

## [1.0.0] - 2026-01-20

### Added
- First stable release
- Complete API documentation
- Complete bring-up examples for the release feature set
- Health and stress test example (02)

### Changed
- Promoted from pre-release to stable

## [0.2.0] - 2026-01-13

### Added
- Enhanced examples with comprehensive interactive demos
- Page buffer mode example (01) with improved rendering logic
- Example consolidation: Example 00 now includes shapes, scrolling, effects, patterns
- Status command for real-time FPS and frame monitoring
- Auto-demo state machine with multiple visual demonstrations
- Reset command for display initialization recovery

### Changed
- Example 00: Expanded from basic drawing to full interactive demo
- Example 01: Improved page buffer drawing with conditional rendering
- Example 02: Consolidated into Example 00 (removed duplicate file)
- Library now Arduino-only (removed non-Arduino compilation paths)
- Updated `nextPage()` to use proper `waitFlush()` instead of manual blocking
- Format specifiers: uint32_t consistently uses %u (not %lu on ESP32)

### Fixed
- **Critical:** Fixed `nextPage()` blocking flush hanging watchdog (was calling `tickFlush(0)` without delays)
- **Critical:** Fixed demo state timing underflow (recapture time before state duration check)
- Fixed format warnings in examples (uint32_t format specifiers)
- Fixed page buffer drawing overlapping text (conditional rendering by Y offset)
- Removed vertical scroll from example 00 (confusing diagonal behavior due to hardware limits)
- Improved error handling in `nextPage()` (exit iteration on flush failure)

### Deprecated
- Example 02 functionality moved to Example 00 (consolidated)

## [0.1.0] - 2026-01-12

### Added
- Initial SSD1315/SSD1306 I2C OLED display driver
- Non-blocking tick()-based architecture
- Full buffer mode (1024 bytes for 128x64)
- Page buffer mode (128+ bytes, u8g2-style iteration)
- Dirty tracking with page and column granularity
- Partial flush support (`requestFlush()`, `requestFlushRect()`)
- Configurable byte budget per tick for deterministic timing
- Complete SSD1315 command set exposed via `CommandTable.h`
- Strongly-typed command wrappers:
  - `setContrast()`, `setInvert()`, `setFlipX()`, `setFlipY()`
  - `setSleep()`, `setAllPixelsOn()`
- Hardware scrolling support:
  - `startHorizontalScroll()`, `startVerticalScroll()`
  - `stopScroll()`, `setVerticalScrollArea()`
- Drawing primitives:
  - `setPixel()`, `drawHLine()`, `drawVLine()`
  - `drawRect()`, `fillRect()`, `drawLine()`
  - `drawCircle()`, `fillCircle()`, `drawBitmap()`
- Built-in 5x7 ASCII font with `drawChar()`, `drawText()`
- Test patterns: `fillCheckerboard()`, `fillVerticalStripes()`, `fillHorizontalStripes()`
- Auto-sleep feature with `setAutoSleep()` and `touch()`
- Page cycling for multi-screen UIs
- Power-on timing guard (non-blocking)
- Transport abstraction via callback (no Wire dependency)
- External buffer support for DMA or memory-constrained systems
- Comprehensive Status/Err error handling
- SSD1315-specific IREF selection support
- Examples:
  - `00_basic_text_or_pixels` - Full buffer mode demo
  - `01_page_buffer_mode` - Page iteration demo
  - `02_scroll_and_invert` - Hardware effects demo
- Full Doxygen documentation for public API
- ESP32-S2 and ESP32-S3 support

[Unreleased]: https://github.com/janhavelka/SSD1315/compare/v4.0.1...HEAD
[4.0.1]: https://github.com/janhavelka/SSD1315/compare/v4.0.0...v4.0.1
[4.0.0]: https://github.com/janhavelka/SSD1315/compare/v3.0.0...v4.0.0
[3.0.0]: https://github.com/janhavelka/SSD1315/compare/v2.1.0...v3.0.0
[2.1.0]: https://github.com/janhavelka/SSD1315/compare/v2.0.0...v2.1.0
[2.0.0]: https://github.com/janhavelka/SSD1315/compare/v1.2.0...v2.0.0
[1.2.0]: https://github.com/janhavelka/SSD1315/compare/v1.1.3...v1.2.0
[1.1.3]: https://github.com/janhavelka/SSD1315/compare/v1.1.2...v1.1.3
[1.1.2]: https://github.com/janhavelka/SSD1315/compare/v1.1.1...v1.1.2
[1.1.1]: https://github.com/janhavelka/SSD1315/compare/v1.1.0...v1.1.1
[1.1.0]: https://github.com/janhavelka/SSD1315/compare/v1.0.2...v1.1.0
[1.0.2]: https://github.com/janhavelka/SSD1315/compare/v1.0.1...v1.0.2
[1.0.1]: https://github.com/janhavelka/SSD1315/compare/v1.0.0...v1.0.1
[1.0.0]: https://github.com/janhavelka/SSD1315/compare/v0.1.0...v1.0.0
[0.1.0]: https://github.com/janhavelka/SSD1315/releases/tag/v0.1.0
