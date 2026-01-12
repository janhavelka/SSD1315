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
