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
#include "SSD1315.h"

// I2C transport callback
static SSD1315::Status mapWireError(uint8_t result, const char* msg) {
  switch (result) {
    case 0: return SSD1315::Ok();
    case 1: return SSD1315::Error(SSD1315::Err::BUFFER_OVERFLOW, static_cast<int32_t>(result), "Write too long");
    case 2: return SSD1315::Error(SSD1315::Err::I2C_NACK_ADDR, static_cast<int32_t>(result), msg);
    case 3: return SSD1315::Error(SSD1315::Err::I2C_NACK_DATA, static_cast<int32_t>(result), msg);
    case 4: return SSD1315::Error(SSD1315::Err::I2C_BUS_ERROR, static_cast<int32_t>(result), msg);
    case 5: return SSD1315::Error(SSD1315::Err::I2C_TIMEOUT, static_cast<int32_t>(result), msg);
    default: return SSD1315::Error(SSD1315::Err::I2C_BUS_ERROR, static_cast<int32_t>(result), msg);
  }
}

SSD1315::Status myI2cWrite(uint8_t addr, const uint8_t* data, size_t len,
                            uint32_t timeoutMs, void* user) {
  (void)timeoutMs;
  TwoWire* wire = static_cast<TwoWire*>(user);
  if (wire == nullptr) {
    return SSD1315::Error(SSD1315::Err::INVALID_CONFIG, "Wire instance is null");
  }
  if (data == nullptr || len == 0) {
    return SSD1315::Error(SSD1315::Err::INTERNAL_ERROR, "Invalid write buffer");
  }
  wire->beginTransmission(addr);
  size_t written = wire->write(data, len);
  if (written != len) {
    return SSD1315::Error(SSD1315::Err::BUFFER_OVERFLOW, "Write incomplete",
                          static_cast<int32_t>(written));
  }
  uint8_t result = wire->endTransmission(true);
  return mapWireError(result, "I2C error");
}

SSD1315::SSD1315 display;

void setup() {
  Wire.begin(8, 9);  // SDA, SCL
  Wire.setClock(400000);
  Wire.setTimeOut(25);

  SSD1315::Config cfg;
  cfg.width = 128;
  cfg.height = 64;
  cfg.i2cAddress = 0x3C;
  cfg.i2cWrite = myI2cWrite;
  cfg.i2cUser = &Wire;
  cfg.i2cTimeoutMs = 25;
  cfg.pageBufferPages = 8;  // Full buffer mode

  SSD1315::Status st = display.begin(cfg);
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
| `nowMs` | function | `nullptr` | Optional monotonic clock source (`millis()` fallback when null) |
| `cooperativeYield` | function | `nullptr` | Optional yield hook for bounded wait helpers |
| `timeUser` | void* | `nullptr` | User context for `nowMs` / `cooperativeYield` |
| `pageBufferPages` | uint8_t | 8 | Pages in RAM buffer (1 to height/8) |
| `byteBudgetPerTick` | uint16_t | 128 | Max bytes per tick() (0=flush one full page per tick) |
| `i2cTimeoutMs` | uint32_t | 25 | I2C transaction timeout |
| `flushTimeoutMs` | uint32_t | 1000 | Total flush timeout (0=none) |
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
| `vcomh` | enum | V_077_VCC | VCOMH deselect level |
| `clockDivide` | uint8_t | 1 | Display clock divide ratio |
| `oscFrequency` | uint8_t | 8 | Oscillator frequency trim |
| `prechargePhase1` | uint8_t | 2 | Pre-charge phase 1 DCLK count |
| `prechargePhase2` | uint8_t | 2 | Pre-charge phase 2 DCLK count |
| `displayOffset` | uint8_t | 0 | Vertical display offset (`0xD3`) |
| `startLine` | uint8_t | 0 | Display start line (`0x40..0x7F`) |
| `offlineThreshold` | uint8_t | 3 | Consecutive failures before `OFFLINE` |
| `externalBuffer` | uint8_t* | nullptr | External framebuffer (optional) |

## Memory Modes

### Full Buffer Mode

Set `pageBufferPages` equal to `height/8` (e.g., 8 for 128x64).

- RAM usage: width x height / 8 bytes (1024 bytes for 128x64)
- Draw anywhere in the buffer
- Dirty tracking enables efficient partial updates
- Best for dynamic content with random access

```cpp
cfg.pageBufferPages = 8;  // Full buffer for 128x64
```

### Page Buffer Mode

Set `pageBufferPages` to 1 or 2 for minimal RAM usage.

- RAM usage: width x pageBufferPages bytes (128-256 bytes)
- Must use firstPage()/nextPage() iteration
- nextPage() is non-blocking; tick() advances the flush job
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
display.startHorizontalScroll(false, 0, 7, SSD1315::ScrollSpeed::FRAMES_5);

// Vertical + horizontal scroll
display.startVerticalScroll(true, 0, 7, SSD1315::ScrollSpeed::FRAMES_4, 1);

// Stop scrolling (corrupts GDDRAM - redraw needed)
display.stopScroll();
```

**Warning:** Hardware scrolling corrupts the framebuffer. After stopping scroll, you must redraw and flush the display.

## Command Passthrough

All SSD1315 commands are accessible:

```cpp
// Single command
display.sendCommand(SSD1315::cmd::DISPLAY_ALL_ON);

// Command with argument
display.sendCommand2(SSD1315::cmd::SET_CONTRAST, 0xFF);

// Command list
uint8_t cmds[] = {0xA6, 0xAF};
display.sendCommandList(cmds, sizeof(cmds));
```

See [CommandTable.h](include/ssd1315/CommandTable.h) for all command definitions.

## Error Handling

All fallible operations return `Status`:

```cpp
SSD1315::Status st = display.begin(cfg);
if (!st.ok()) {
  Serial.printf("Error: %s (code=%d, detail=%d)\n", 
                st.msg, (int)st.code, st.detail);
}
```

Error codes:
- `OK` - Success
- `INVALID_CONFIG` - Bad configuration parameter
- `INVALID_DIMENSIONS` - Unsupported width/height combination
- `INVALID_PAGE_COUNT` - `pageBufferPages` is outside the valid range
- `NOT_INITIALIZED` - begin() not called
- `STATE_ERROR` - Operation not valid in the current state
- `BUSY` - Flush in progress
- `PANEL_NOT_READY` - Post-power-on settling delay is still active
- `I2C_NACK_ADDR` - Device not responding
- `I2C_NACK_DATA` - Data transmission failed
- `I2C_TIMEOUT` - I2C timeout
- `I2C_BUS_ERROR` - Arbitration/stuck-bus/other bus-level failure
- `TIMEOUT` - Operation timeout
- `BUFFER_OVERFLOW` - Buffer or transfer size exceeded supported bounds
- `UNSUPPORTED` - Requested operation is not supported in this mode/backend
- `INTERNAL_ERROR` - Internal invariant failure or impossible callback contract violation
- `DEVICE_NOT_FOUND` - Device not present (from `probe()` after `begin()`)
- `IN_PROGRESS` - Async operation in progress (not an error)

## Health Tracking

The driver tracks device health to detect communication failures and enable recovery.

### DriverState

```cpp
enum class DriverState : uint8_t {
  UNINIT,    // Not initialized
  READY,     // Last I2C transaction succeeded
  DEGRADED,  // 1 to (N-1) consecutive failures
  OFFLINE    // N+ consecutive failures (threshold reached)
};
```

State transitions occur automatically based on I2C results:
- Any success -> `READY`
- First failure -> `DEGRADED`
- Failures >= `offlineThreshold` -> `OFFLINE`
- `end()` -> `UNINIT`

### Health API

```cpp
// Device presence check (diagnostic only, requires begin()/configured transport)
Status probe();

// Re-initialize after failure
Status recover();

// State queries
DriverState state() const;
bool isOnline() const;  // true if READY or DEGRADED

// Diagnostics
uint32_t lastOkMs() const;         // Timestamp of last success
uint32_t lastErrorMs() const;      // Timestamp of last error
Status lastError() const;          // Most recent error
uint8_t consecutiveFailures() const;
uint32_t totalFailures() const;
uint32_t totalSuccess() const;
```

### Configuration

```cpp
cfg.offlineThreshold = 3;  // Failures before OFFLINE (default: 3, min: 1)
```

### Recovery Pattern

```cpp
if (display.state() == SSD1315::DriverState::OFFLINE) {
  Status st = display.recover();
  if (st.ok()) {
    display.requestFlush();  // Resync display
  }
}
```

### Notes

- `probe()` is diagnostic-only: does not affect health counters or state
- `recover()` requires prior `begin()` (returns `NOT_INITIALIZED` otherwise)
- Health counters persist across `end()` for post-mortem analysis; reset on next `begin()`

## Examples

| Example | Description |
|---------|-------------|
| [01_basic_bringup_cli](examples/01_basic_bringup_cli/) | Unified bringup CLI with diagnostics, stress tools, and full feature commands |

The unified `01_basic_bringup_cli` example includes:
- common bringup commands (`help`, `scan`, `probe`, `recover`, `drv`, `read`, `cfg/settings`, `verbose`, `stress`)
- feature controls (`contrast`, `invert`, `flipx`, `flipy`, `sleep`, `allon`, `zoom`, `fade`, scroll commands)
- graphics commands (`text`, `pattern`, `line`, `rect`, `fillrect`, `circle`, `fillcircle`, `flush`, `flushrect`)
- validation helpers (`stress_mix`, `selftest`/`featuretest`, `flushstress`, `burst`, `monitor`)

### Example Helpers (`examples/common/`)

Not part of the library. These simulate project-level glue and keep examples self-contained:

| File | Purpose |
|------|---------|
| `BoardConfig.h` | Pin definitions and Wire init for supported boards |
| `BuildConfig.h` | Compile-time `LOG_LEVEL` configuration |
| `Log.h` | Serial logging macros (`LOGE`/`LOGW`/`LOGI`/`LOGD`/`LOGT`/`LOGV`) |
| `I2cTransport.h` | Wire-based I2C write transport adapter |
| `I2cScanner.h` | I2C bus scanner with table output and bus recovery |
| `BusDiag.h` | Bus diagnostics wrapper (scan + probe) |
| `CliShell.h` | Serial command-line shell with line editing |
| `CommandHandler.h` | Command parsing helpers (`readLine`, `match`, `parseInt`) |
| `HealthView.h` | Compact health status display |
| `HealthDiag.h` | Verbose health diagnostics with color, snapshots, and `HealthMonitor` |
| `TransportAdapter.h` | Transport function pointer adapter |

## API Reference

### Lifecycle

```cpp
Status begin(const Config& config);  // Initialize
void tick(uint32_t nowMs);           // Cooperative update
void end();                          // Cleanup
bool isInitialized() const;
const Config& getConfig() const;
Status probe();                      // Raw presence check, no health tracking
Status recover();                    // Re-probe and reinitialize cached config
DriverState state() const;
bool isOnline() const;
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
Status setBrightness(uint8_t brightness);  // Alias for setContrast()
Status setInvert(bool invert);
Status setFlipX(bool flip);
Status setFlipY(bool flip);
Status setSleep(bool sleep);
Status setAllPixelsOn(bool allOn);
```

### Diagnostics and Runtime Helpers

```cpp
void setAutoSleep(uint32_t inactivityMs);
void touch();
void setUserPageCount(uint8_t count);
void setActiveUserPage(uint8_t page);
uint8_t getActiveUserPage() const;
uint8_t getUserPageCount() const;
void setPageCycleInterval(uint32_t intervalMs);
Status requestFlushRect(int16_t x, int16_t y, int16_t w, int16_t h);
```

Notes:

- `probe()` is diagnostic-only and does not affect health counters.
- `recover()` rebuilds panel state from the cached config; request a flush afterward if you need to redraw GDDRAM from RAM.

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
pio run -e esp32s3dev
pio run -e esp32s2dev
pio run -e native

# Upload
pio run -t upload -e esp32s3dev
```

## Validation

```bash
pio test -e native
python tools/check_cli_contract.py
python tools/check_core_timing_guard.py
pio run -e esp32s3dev
pio run -e esp32s2dev
```

## Hardware Compatibility

Tested displays:
- SSD1315 128x64 (Wisevision modules)
- SSD1306 128x64 (generic)
- SSD1306 128x32

Should work with any SSD1306/SSD1315 compatible display.

## Documentation

- `CHANGELOG.md` - full release history
- `docs/IDF_PORT.md` - ESP-IDF portability guidance
- `docs/SSD1315_I2C_Command_Reference.md` - command reference notes
- `docs/SSD1315.pdf` - device reference material
- `docs/C18723026.pdf` - display module reference sheet

## License

MIT License. See [LICENSE](LICENSE) for details.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.
