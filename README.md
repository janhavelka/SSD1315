# SSD1315 I2C OLED Display Driver

[![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32-orange)](https://platformio.org)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

Production-grade, non-blocking I2C driver library for SSD1315/SSD1306 OLED displays on ESP32 (Arduino framework, PlatformIO).

## Features

- **Non-blocking operation** - tick()-based cooperative architecture
- **Partial updates** - dirty tracking with column-level granularity  
- **Page buffer mode** - u8g2-style iteration for low RAM usage (128 bytes vs 1KB)
- **Hardware scroll** - horizontal, vertical, and diagonal scrolling
- **Full command access** - all SSD1315 commands exposed
- **Zero runtime allocation** - all buffers allocated in begin()
- **Robust error handling** - Status return type on all fallible operations
- **Transport abstraction** - no Wire dependency; inject your own I2C callback

## Quick Start

```cpp
#include <Arduino.h>
#include <Wire.h>
#include "ssd1315/Ssd1315.h"

// I2C transport callback
ssd1315::Status myI2cWrite(uint8_t addr, const uint8_t* data, size_t len,
                            uint32_t timeoutMs, void* user) {
  Wire.beginTransmission(addr);
  Wire.write(data, len);
  return Wire.endTransmission() == 0 
    ? ssd1315::Ok() 
    : ssd1315::Error(ssd1315::Err::I2C_BUS_ERROR, "I2C error");
}

ssd1315::Ssd1315 display;

void setup() {
  Wire.begin(8, 9);  // SDA, SCL
  Wire.setClock(400000);

  ssd1315::Config cfg;
  cfg.width = 128;
  cfg.height = 64;
  cfg.i2cAddress = 0x3C;
  cfg.i2cWrite = myI2cWrite;
  cfg.i2cUser = &Wire;
  cfg.pageBufferPages = 8;  // Full buffer mode

  ssd1315::Status st = display.begin(cfg);
  if (!st.ok()) {
    Serial.printf("Error: %s\n", st.msg);
    return;
  }

  display.clear();
  display.drawText(0, 0, "Hello World!");
  display.requestFlush();
}

void loop() {
  display.tick(millis());  // Drive state machine
}
```

## Configuration Reference

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `width` | uint8_t | 128 | Display width in pixels (1-128) |
| `height` | uint8_t | 64 | Display height in pixels (8, 16, 32, 64) |
| `i2cAddress` | uint8_t | 0x3C | 7-bit I2C address (0x03..0x77, typically 0x3C or 0x3D) |
| `i2cWrite` | function | nullptr | **Required.** I2C write callback |
| `i2cUser` | void* | nullptr | User context for callback |
| `pageBufferPages` | uint8_t | 8 | Pages in RAM buffer (1 to height/8) |
| `byteBudgetPerTick` | uint16_t | 128 | Max bytes per tick() (0=unlimited) |
| `i2cTimeoutMs` | uint32_t | 50 | I2C transaction timeout |
| `flushTimeoutMs` | uint32_t | 2000 | Total flush timeout (0=none) |
| `displayOnDelayMs` | uint32_t | 100 | Power-on timing guard |
| `inactivitySleepMs` | uint32_t | 0 | Auto-sleep timeout (0=disabled) |
| `pageCycleMs` | uint32_t | 0 | Page cycling interval (0=disabled) |
| `flipX` | bool | false | Flip horizontally (segment remap) |
| `flipY` | bool | false | Flip vertically (COM scan) |
| `invert` | bool | false | Invert display colors |
| `contrast` | uint8_t | 0x7F | Initial contrast (0-255) |
| `comPins` | enum | ALTERNATIVE_NO_REMAP | COM pin configuration |
| `chargePumpVoltage` | enum | V7_5 | Charge pump voltage |
| `iref` | enum | INTERNAL_19UA | IREF selection (SSD1315) |
| `externalBuffer` | uint8_t* | nullptr | External framebuffer (optional) |

## Memory Modes

### Full Buffer Mode

Set `pageBufferPages` equal to `height/8` (e.g., 8 for 128x64).

- RAM usage: width × height / 8 bytes (1024 bytes for 128x64)
- Draw anywhere in the buffer
- Dirty tracking enables efficient partial updates
- Best for dynamic content with random access

```cpp
cfg.pageBufferPages = 8;  // Full buffer for 128x64
```

### Page Buffer Mode

Set `pageBufferPages` to 1 or 2 for minimal RAM usage.

- RAM usage: width × pageBufferPages bytes (128-256 bytes)
- Must use firstPage()/nextPage() iteration
- nextPage() blocks until the page flush completes (bounded by flushTimeoutMs)
- Renders entire screen each frame, but only page buffer in RAM
- Best for static or slowly-changing content
- clear()/fill() affect only the current buffer window; use firstPage()/nextPage() to cover the full display

```cpp
cfg.pageBufferPages = 1;  // Minimal RAM

// In loop:
display.firstPage();
do {
  // Draw all content - library clips to current page
  display.drawText(0, 0, "Title");
  display.fillCircle(64, 32, 10);
} while (display.nextPage());
```

## Timing Model

### Byte Budget

The `byteBudgetPerTick` setting controls how much I2C data is sent per `tick()` call:

| Setting | Behavior | Use Case |
|---------|----------|----------|
| 128 | ~2-3ms per tick at 400kHz | General use |
| 256 | ~5ms per tick | Faster updates |
| 64 | ~1.5ms per tick | Very responsive loop |
| 0 | Flush full page per tick | Blocking scenarios |

For latency-sensitive systems, keep byteBudgetPerTick small and prefer requestFlush() + tick()
over waitFlush() or nextPage().

### Power-On Timing

The SSD1315 requires ~100ms after display ON before the panel is fully active. The driver enforces this non-blocking via `displayOnDelayMs`. During this period, flush operations are deferred.

### Auto-Sleep

Configure automatic display sleep after inactivity:

```cpp
display.setAutoSleep(30000);  // Sleep after 30 seconds
display.touch();              // Reset timer on user activity
```

## Partial Updates

The driver tracks dirty regions at page and column granularity:

```cpp
// Draw in a specific area
display.fillRect(10, 20, 30, 15);

// Flush only the dirty region
display.requestFlush();  // Automatically flushes only dirty pages

// Or explicitly flush a rectangle
display.requestFlushRect(10, 20, 30, 15);
```

Dirty tracking:
- Per-page dirty flags (bitmask)
- Per-page min/max dirty column range
- Horizontal addressing mode for efficient rectangle updates

## Hardware Scrolling

Hardware scroll moves pixels on the display without CPU involvement:

```cpp
// Horizontal scroll
display.startHorizontalScroll(false, 0, 7, ssd1315::ScrollSpeed::FRAMES_5);

// Vertical + horizontal scroll
display.startVerticalScroll(true, 0, 7, ssd1315::ScrollSpeed::FRAMES_4, 1);

// Stop scrolling (corrupts GDDRAM - redraw needed)
display.stopScroll();
```

**Warning:** Hardware scrolling corrupts the framebuffer. After stopping scroll, you must redraw and flush the display.

## Command Passthrough

All SSD1315 commands are accessible:

```cpp
// Single command
display.sendCommand(ssd1315::cmd::DISPLAY_ALL_ON);

// Command with argument
display.sendCommand2(ssd1315::cmd::SET_CONTRAST, 0xFF);

// Command list
uint8_t cmds[] = {0xA6, 0xAF};
display.sendCommandList(cmds, sizeof(cmds));
```

See [Commands.h](include/ssd1315/Commands.h) for all command definitions.

## Error Handling

All fallible operations return `Status`:

```cpp
ssd1315::Status st = display.begin(cfg);
if (!st.ok()) {
  Serial.printf("Error: %s (code=%d, detail=%d)\n", 
                st.msg, (int)st.code, st.detail);
}
```

Error codes:
- `OK` - Success
- `INVALID_CONFIG` - Bad configuration parameter
- `NOT_INITIALIZED` - begin() not called
- `BUSY` - Flush in progress
- `I2C_NACK_ADDR` - Device not responding
- `I2C_NACK_DATA` - Data transmission failed
- `I2C_TIMEOUT` - I2C timeout
- `TIMEOUT` - Operation timeout

## Examples

| Example | Description |
|---------|-------------|
| [00_basic_text_or_pixels](examples/00_basic_text_or_pixels/) | Full buffer mode, drawing, partial flush |
| [01_page_buffer_mode](examples/01_page_buffer_mode/) | Page buffer iteration for low RAM |
| [02_scroll_and_invert](examples/02_scroll_and_invert/) | Hardware scroll, effects, patterns |

## API Reference

### Lifecycle

```cpp
Status begin(const Config& config);  // Initialize
void tick(uint32_t nowMs);           // Cooperative update
void end();                          // Cleanup
```

### Drawing

```cpp
void clear();
void fill();
void setPixel(int16_t x, int16_t y, bool on = true);
void drawHLine(int16_t x, int16_t y, int16_t w, bool on = true);
void drawVLine(int16_t x, int16_t y, int16_t h, bool on = true);
void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, bool on = true);
void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, bool on = true);
void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, bool on = true);
void drawCircle(int16_t cx, int16_t cy, int16_t r, bool on = true);
void fillCircle(int16_t cx, int16_t cy, int16_t r, bool on = true);
void drawBitmap(int16_t x, int16_t y, const uint8_t* bmp, int16_t w, int16_t h, bool on = true);
void drawChar(int16_t x, int16_t y, char c, bool on = true);
int16_t drawText(int16_t x, int16_t y, const char* str, bool on = true);
```

### Display Control

```cpp
Status setContrast(uint8_t contrast);
Status setInvert(bool invert);
Status setFlipX(bool flip);
Status setFlipY(bool flip);
Status setSleep(bool sleep);
Status setAllPixelsOn(bool allOn);
```

### Flush Control

```cpp
Status requestFlush();
Status requestFlushRect(int16_t x, int16_t y, int16_t w, int16_t h);
bool isFlushing() const;
Status lastError() const;
Status waitFlush(uint32_t nowMs, uint32_t timeoutMs = 0);
```

### Page Buffer Mode

```cpp
bool isPageBufferMode() const;
void firstPage();
bool nextPage();
uint8_t currentPageIndex() const;
int16_t pageBufferYOffset() const;
```

## Threading Model

**Single-threaded only.** Call all methods from the same task (typically `loop()`). The driver is not thread-safe and should not be called from ISRs.

## Resource Ownership

- **I2C bus**: Application owns the bus; library uses callback only
- **Framebuffer**: Library allocates in `begin()` (or uses external buffer)
- **Pins**: Application configures; library has no pin knowledge

## Building

```bash
# Build default example
pio run

# Build specific environment
pio run -e basic_esp32s3
pio run -e pagebuf_esp32s2
pio run -e scroll_esp32s3

# Upload
pio run -t upload -e basic_esp32s3
```

## Hardware Compatibility

Tested displays:
- SSD1315 128x64 (Wisevision modules)
- SSD1306 128x64 (generic)
- SSD1306 128x32

Should work with any SSD1306/SSD1315 compatible display.

## License

MIT License. See [LICENSE](LICENSE) for details.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.
