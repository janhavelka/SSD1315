# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Nothing yet

### Changed
- Nothing yet

### Fixed
- Nothing yet

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
- Complete SSD1315 command set exposed via `Commands.h`
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

[Unreleased]: https://github.com/janhavelka/SSD1315/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/janhavelka/SSD1315/releases/tag/v0.1.0
