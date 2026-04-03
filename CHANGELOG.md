# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.1.1] - 2026-04-03

### Added
- `inProgress()` convenience method on `Status` struct.

### Changed
- `I2cScanner.h`: standardized `#if defined(ESP32)` to `#if defined(ARDUINO_ARCH_ESP32)` for portability.
- `I2cTransport.h`: standardized all ESP32 preprocessor guards to `ARDUINO_ARCH_ESP32`.
- `Log.h`: added `LOGV` runtime-verbose macro, added `#ifndef LOG_SERIAL` guard for override support.
- README documentation now reflects the current defaults, non-blocking page-buffer behavior, and shipped reference files.
- Public-header and metadata documentation now describe both the legacy shim include and the canonical `ssd1315/SSD1315.h` path.

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
- Production-ready examples
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

[Unreleased]: https://github.com/janhavelka/SSD1315/compare/v1.1.1...HEAD
[1.1.1]: https://github.com/janhavelka/SSD1315/compare/v1.1.0...v1.1.1
[1.1.0]: https://github.com/janhavelka/SSD1315/compare/v1.0.2...v1.1.0
[1.0.2]: https://github.com/janhavelka/SSD1315/compare/v1.0.1...v1.0.2
[1.0.1]: https://github.com/janhavelka/SSD1315/compare/v1.0.0...v1.0.1
[1.0.0]: https://github.com/janhavelka/SSD1315/compare/v0.1.0...v1.0.0
[0.1.0]: https://github.com/janhavelka/SSD1315/releases/tag/v0.1.0
