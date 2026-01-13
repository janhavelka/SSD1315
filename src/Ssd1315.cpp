/**
 * @file Ssd1315.cpp
 * @brief SSD1315 OLED display driver implementation.
 */

#include "ssd1315/Ssd1315.h"

#include <new>       // std::nothrow
#include <string.h>  // memset

#if defined(ARDUINO)
#include <Arduino.h>  // millis(), delay() for waitFlush
#endif

namespace ssd1315 {

// ============================================================================
// Built-in 5x7 font (ASCII 32-126)
// ============================================================================

// Font data: 5 bytes per character, each byte is a column, LSB = top
// Character cell is 6x8 (5x7 glyph + 1 column spacing + 1 row spacing)
static const uint8_t FONT_5X7[] = {
    0x00, 0x00, 0x00, 0x00, 0x00,  // 32 ' '
    0x00, 0x00, 0x5F, 0x00, 0x00,  // 33 '!'
    0x00, 0x07, 0x00, 0x07, 0x00,  // 34 '"'
    0x14, 0x7F, 0x14, 0x7F, 0x14,  // 35 '#'
    0x24, 0x2A, 0x7F, 0x2A, 0x12,  // 36 '$'
    0x23, 0x13, 0x08, 0x64, 0x62,  // 37 '%'
    0x36, 0x49, 0x55, 0x22, 0x50,  // 38 '&'
    0x00, 0x05, 0x03, 0x00, 0x00,  // 39 '''
    0x00, 0x1C, 0x22, 0x41, 0x00,  // 40 '('
    0x00, 0x41, 0x22, 0x1C, 0x00,  // 41 ')'
    0x08, 0x2A, 0x1C, 0x2A, 0x08,  // 42 '*'
    0x08, 0x08, 0x3E, 0x08, 0x08,  // 43 '+'
    0x00, 0x50, 0x30, 0x00, 0x00,  // 44 ','
    0x08, 0x08, 0x08, 0x08, 0x08,  // 45 '-'
    0x00, 0x60, 0x60, 0x00, 0x00,  // 46 '.'
    0x20, 0x10, 0x08, 0x04, 0x02,  // 47 '/'
    0x3E, 0x51, 0x49, 0x45, 0x3E,  // 48 '0'
    0x00, 0x42, 0x7F, 0x40, 0x00,  // 49 '1'
    0x42, 0x61, 0x51, 0x49, 0x46,  // 50 '2'
    0x21, 0x41, 0x45, 0x4B, 0x31,  // 51 '3'
    0x18, 0x14, 0x12, 0x7F, 0x10,  // 52 '4'
    0x27, 0x45, 0x45, 0x45, 0x39,  // 53 '5'
    0x3C, 0x4A, 0x49, 0x49, 0x30,  // 54 '6'
    0x01, 0x71, 0x09, 0x05, 0x03,  // 55 '7'
    0x36, 0x49, 0x49, 0x49, 0x36,  // 56 '8'
    0x06, 0x49, 0x49, 0x29, 0x1E,  // 57 '9'
    0x00, 0x36, 0x36, 0x00, 0x00,  // 58 ':'
    0x00, 0x56, 0x36, 0x00, 0x00,  // 59 ';'
    0x00, 0x08, 0x14, 0x22, 0x41,  // 60 '<'
    0x14, 0x14, 0x14, 0x14, 0x14,  // 61 '='
    0x41, 0x22, 0x14, 0x08, 0x00,  // 62 '>'
    0x02, 0x01, 0x51, 0x09, 0x06,  // 63 '?'
    0x32, 0x49, 0x79, 0x41, 0x3E,  // 64 '@'
    0x7E, 0x11, 0x11, 0x11, 0x7E,  // 65 'A'
    0x7F, 0x49, 0x49, 0x49, 0x36,  // 66 'B'
    0x3E, 0x41, 0x41, 0x41, 0x22,  // 67 'C'
    0x7F, 0x41, 0x41, 0x22, 0x1C,  // 68 'D'
    0x7F, 0x49, 0x49, 0x49, 0x41,  // 69 'E'
    0x7F, 0x09, 0x09, 0x01, 0x01,  // 70 'F'
    0x3E, 0x41, 0x41, 0x51, 0x32,  // 71 'G'
    0x7F, 0x08, 0x08, 0x08, 0x7F,  // 72 'H'
    0x00, 0x41, 0x7F, 0x41, 0x00,  // 73 'I'
    0x20, 0x40, 0x41, 0x3F, 0x01,  // 74 'J'
    0x7F, 0x08, 0x14, 0x22, 0x41,  // 75 'K'
    0x7F, 0x40, 0x40, 0x40, 0x40,  // 76 'L'
    0x7F, 0x02, 0x04, 0x02, 0x7F,  // 77 'M'
    0x7F, 0x04, 0x08, 0x10, 0x7F,  // 78 'N'
    0x3E, 0x41, 0x41, 0x41, 0x3E,  // 79 'O'
    0x7F, 0x09, 0x09, 0x09, 0x06,  // 80 'P'
    0x3E, 0x41, 0x51, 0x21, 0x5E,  // 81 'Q'
    0x7F, 0x09, 0x19, 0x29, 0x46,  // 82 'R'
    0x46, 0x49, 0x49, 0x49, 0x31,  // 83 'S'
    0x01, 0x01, 0x7F, 0x01, 0x01,  // 84 'T'
    0x3F, 0x40, 0x40, 0x40, 0x3F,  // 85 'U'
    0x1F, 0x20, 0x40, 0x20, 0x1F,  // 86 'V'
    0x7F, 0x20, 0x18, 0x20, 0x7F,  // 87 'W'
    0x63, 0x14, 0x08, 0x14, 0x63,  // 88 'X'
    0x03, 0x04, 0x78, 0x04, 0x03,  // 89 'Y'
    0x61, 0x51, 0x49, 0x45, 0x43,  // 90 'Z'
    0x00, 0x00, 0x7F, 0x41, 0x41,  // 91 '['
    0x02, 0x04, 0x08, 0x10, 0x20,  // 92 '\'
    0x41, 0x41, 0x7F, 0x00, 0x00,  // 93 ']'
    0x04, 0x02, 0x01, 0x02, 0x04,  // 94 '^'
    0x40, 0x40, 0x40, 0x40, 0x40,  // 95 '_'
    0x00, 0x01, 0x02, 0x04, 0x00,  // 96 '`'
    0x20, 0x54, 0x54, 0x54, 0x78,  // 97 'a'
    0x7F, 0x48, 0x44, 0x44, 0x38,  // 98 'b'
    0x38, 0x44, 0x44, 0x44, 0x20,  // 99 'c'
    0x38, 0x44, 0x44, 0x48, 0x7F,  // 100 'd'
    0x38, 0x54, 0x54, 0x54, 0x18,  // 101 'e'
    0x08, 0x7E, 0x09, 0x01, 0x02,  // 102 'f'
    0x08, 0x14, 0x54, 0x54, 0x3C,  // 103 'g'
    0x7F, 0x08, 0x04, 0x04, 0x78,  // 104 'h'
    0x00, 0x44, 0x7D, 0x40, 0x00,  // 105 'i'
    0x20, 0x40, 0x44, 0x3D, 0x00,  // 106 'j'
    0x00, 0x7F, 0x10, 0x28, 0x44,  // 107 'k'
    0x00, 0x41, 0x7F, 0x40, 0x00,  // 108 'l'
    0x7C, 0x04, 0x18, 0x04, 0x78,  // 109 'm'
    0x7C, 0x08, 0x04, 0x04, 0x78,  // 110 'n'
    0x38, 0x44, 0x44, 0x44, 0x38,  // 111 'o'
    0x7C, 0x14, 0x14, 0x14, 0x08,  // 112 'p'
    0x08, 0x14, 0x14, 0x18, 0x7C,  // 113 'q'
    0x7C, 0x08, 0x04, 0x04, 0x08,  // 114 'r'
    0x48, 0x54, 0x54, 0x54, 0x20,  // 115 's'
    0x04, 0x3F, 0x44, 0x40, 0x20,  // 116 't'
    0x3C, 0x40, 0x40, 0x20, 0x7C,  // 117 'u'
    0x1C, 0x20, 0x40, 0x20, 0x1C,  // 118 'v'
    0x3C, 0x40, 0x30, 0x40, 0x3C,  // 119 'w'
    0x44, 0x28, 0x10, 0x28, 0x44,  // 120 'x'
    0x0C, 0x50, 0x50, 0x50, 0x3C,  // 121 'y'
    0x44, 0x64, 0x54, 0x4C, 0x44,  // 122 'z'
    0x00, 0x08, 0x36, 0x41, 0x00,  // 123 '{'
    0x00, 0x00, 0x7F, 0x00, 0x00,  // 124 '|'
    0x00, 0x41, 0x36, 0x08, 0x00,  // 125 '}'
    0x08, 0x08, 0x2A, 0x1C, 0x08,  // 126 '~'
};

static constexpr uint8_t FONT_FIRST_CHAR = 32;
static constexpr uint8_t FONT_CHAR_COUNT = 95;  // 32-126
static constexpr uint8_t FONT_WIDTH = 5;
static constexpr uint8_t FONT_HEIGHT = 7;
static constexpr uint8_t CHAR_WIDTH = 6;   // Font width + 1px spacing
static constexpr uint8_t CHAR_HEIGHT = 8;  // Font height + 1px spacing

// ============================================================================
// Constructor / Destructor
// ============================================================================

Ssd1315::Ssd1315() {
  memset(_dirtyMinCol, 0xFF, sizeof(_dirtyMinCol));
  memset(_dirtyMaxCol, 0x00, sizeof(_dirtyMaxCol));
}

Ssd1315::~Ssd1315() {
  end();
}

// ============================================================================
// Lifecycle
// ============================================================================

Status Ssd1315::begin(const Config& config) {
  // Clean up previous state if any
  if (_initialized) {
    end();
  }

  // Validate configuration
  if (config.i2cWrite == nullptr) {
    return Error(Err::INVALID_CONFIG, "i2cWrite callback is null");
  }
  if (config.width == 0 || config.width > MAX_WIDTH) {
    return Error(Err::INVALID_DIMENSIONS, "width out of range [1..128]");
  }
  if (config.height == 0 || config.height > MAX_HEIGHT || (config.height % 8) != 0) {
    return Error(Err::INVALID_DIMENSIONS, "height must be 8..64, multiple of 8");
  }

  _totalPages = config.height / 8;

  if (config.pageBufferPages == 0 || config.pageBufferPages > _totalPages) {
    return Error(Err::INVALID_PAGE_COUNT, "pageBufferPages out of range");
  }
  if (config.i2cTimeoutMs == 0) {
    return Error(Err::INVALID_CONFIG, "i2cTimeoutMs must be > 0");
  }

  _config = config;

  // Allocate or assign buffer
  size_t bufSize = static_cast<size_t>(_config.width) * _config.pageBufferPages;
  if (_config.externalBuffer != nullptr) {
    _buffer = _config.externalBuffer;
    _ownsBuffer = false;
  } else {
    _buffer = new (std::nothrow) uint8_t[bufSize];
    if (_buffer == nullptr) {
      return Error(Err::INTERNAL_ERROR, "buffer allocation failed");
    }
    _ownsBuffer = true;
  }

  // Clear buffer
  memset(_buffer, 0, bufSize);

  // Reset state
  _sleeping = true;
  _allPixelsOn = false;
  _flushState = FlushState::IDLE;
  _lastError = Ok();
  _dirtyPages = 0;
  memset(_dirtyMinCol, 0xFF, sizeof(_dirtyMinCol));
  memset(_dirtyMaxCol, 0x00, sizeof(_dirtyMaxCol));
  _currentBufferPage = 0;
  _inPageIteration = false;

  // Copy feature config
  _autoSleepMs = _config.inactivitySleepMs;
  _pageCycleMs = _config.pageCycleMs;
  _userPageCount = 1;
  _activeUserPage = 0;

  // Send initialization sequence
  Status st = initDisplay();
  if (!st.ok()) {
    if (_ownsBuffer) {
      delete[] _buffer;
      _buffer = nullptr;
    }
    return st;
  }

  // Clear GDDRAM to remove any stale content from previous power cycle
  // This is blocking but only happens once during initialization
  st = clearGddram();
  if (!st.ok()) {
    if (_ownsBuffer) {
      delete[] _buffer;
      _buffer = nullptr;
    }
    return st;
  }

  _initialized = true;

  return Ok();
}

void Ssd1315::tick(uint32_t nowMs) {
  if (!_initialized) {
    return;
  }

  // Handle power-on timing
  tickPowerOn(nowMs);

  // Drive flush state machine
  tickFlush(nowMs);

  // Handle auto-sleep
  tickAutoSleep(nowMs);

  // Handle page cycling
  tickPageCycle(nowMs);
}

void Ssd1315::end() {
  if (!_initialized) {
    return;
  }

  // Turn off display
  sendCommand(cmd::DISPLAY_OFF);

  // Free buffer if we own it
  if (_ownsBuffer && _buffer != nullptr) {
    delete[] _buffer;
  }
  _buffer = nullptr;
  _ownsBuffer = false;
  _initialized = false;
  _powerState = PowerState::OFF;
}

// ============================================================================
// Display initialization
// ============================================================================

Status Ssd1315::initDisplay() {
  // Initialization sequence based on SSD1315 datasheet recommendations
  // Keep display off during setup
  Status st;

  st = sendCommand(cmd::DISPLAY_OFF);
  if (!st.ok()) return st;

  // Set memory addressing mode to horizontal
  st = sendCommand2(cmd::SET_MEMORY_MODE, cmd::ADDR_MODE_HORIZONTAL);
  if (!st.ok()) return st;

  // Set display start line to 0
  st = sendCommand(cmd::SET_START_LINE | (_config.startLine & 0x3F));
  if (!st.ok()) return st;

  // Set segment remap (flip X)
  st = sendCommand(_config.flipX ? cmd::SEG_REMAP_ON : cmd::SEG_REMAP_OFF);
  if (!st.ok()) return st;

  // Set multiplex ratio (height - 1)
  st = sendCommand2(cmd::SET_MULTIPLEX, _config.height - 1);
  if (!st.ok()) return st;

  // Set COM scan direction (flip Y)
  st = sendCommand(_config.flipY ? cmd::COM_SCAN_DEC : cmd::COM_SCAN_INC);
  if (!st.ok()) return st;

  // Set display offset
  st = sendCommand2(cmd::SET_DISPLAY_OFFSET, _config.displayOffset);
  if (!st.ok()) return st;

  // Set COM pins configuration
  st = sendCommand2(cmd::SET_COM_PINS, static_cast<uint8_t>(_config.comPins));
  if (!st.ok()) return st;

  // Set clock divide ratio and oscillator frequency
  uint8_t clockDiv = ((_config.oscFrequency & 0x0F) << 4) | ((_config.clockDivide - 1) & 0x0F);
  st = sendCommand2(cmd::SET_CLOCK_DIV, clockDiv);
  if (!st.ok()) return st;

  // Set pre-charge period
  uint8_t precharge = ((_config.prechargePhase2 & 0x0F) << 4) | (_config.prechargePhase1 & 0x0F);
  st = sendCommand2(cmd::SET_PRECHARGE, precharge);
  if (!st.ok()) return st;

  // Set VCOMH level
  st = sendCommand2(cmd::SET_VCOMH, static_cast<uint8_t>(_config.vcomh));
  if (!st.ok()) return st;

  // Set contrast
  st = sendCommand2(cmd::SET_CONTRAST, _config.contrast);
  if (!st.ok()) return st;

  // SSD1315-specific: Set IREF selection
  st = sendCommand2(cmd::SET_IREF, static_cast<uint8_t>(_config.iref));
  if (!st.ok()) return st;

  // Enable charge pump
  st = sendCommand2(cmd::SET_CHARGE_PUMP, static_cast<uint8_t>(_config.chargePumpVoltage));
  if (!st.ok()) return st;

  // Resume to RAM content (not all-on)
  st = sendCommand(cmd::DISPLAY_RAM);
  if (!st.ok()) return st;

  // Set normal/inverse display
  st = sendCommand(_config.invert ? cmd::INVERT_DISPLAY : cmd::NORMAL_DISPLAY);
  if (!st.ok()) return st;

  // Deactivate scroll
  st = sendCommand(cmd::SCROLL_DEACTIVATE);
  if (!st.ok()) return st;

  // Turn on display - but we'll wait for power-on timing in tick()
  st = sendCommand(cmd::DISPLAY_ON);
  if (!st.ok()) return st;

  // Start power-on timing guard
  _powerState = PowerState::INIT_DELAY;
  _powerOnMs = 0;  // Will be set on first tick
  _sleeping = false;

  return Ok();
}

Status Ssd1315::clearGddram() {
  // Clear all GDDRAM by writing zeros to entire display RAM.
  // This is a BLOCKING operation used during initialization.
  // For 128x64: 8 pages × 128 columns = 1024 bytes.

  Status st;

  // Set addressing window to full screen (horizontal addressing mode)
  st = sendCommand3(cmd::SET_COL_ADDR, 0, _config.width - 1);
  if (!st.ok()) return st;
  st = sendCommand3(cmd::SET_PAGE_ADDR, 0, _totalPages - 1);
  if (!st.ok()) return st;

  // Send zeros in chunks (ESP32 Wire buffer is 128 bytes max)
  // Use stack buffer to avoid heap allocation
  constexpr size_t CHUNK_SIZE = 32;  // Small chunks for reliability
  uint8_t buf[CHUNK_SIZE + 1];
  buf[0] = cmd::CTRL_DATA;
  memset(buf + 1, 0x00, CHUNK_SIZE);

  size_t totalBytes = static_cast<size_t>(_config.width) * _totalPages;
  size_t sent = 0;

  while (sent < totalBytes) {
    size_t chunk = (totalBytes - sent) > CHUNK_SIZE ? CHUNK_SIZE : (totalBytes - sent);
    st = _config.i2cWrite(_config.i2cAddress, buf, chunk + 1,
                          _config.i2cTimeoutMs, _config.i2cUser);
    if (!st.ok()) return st;
    sent += chunk;
  }

  return Ok();
}

// ============================================================================
// Raw command access
// ============================================================================

Status Ssd1315::sendCommand(uint8_t cmd) {
  uint8_t buf[2] = {cmd::CTRL_COMMAND, cmd};
  return _config.i2cWrite(_config.i2cAddress, buf, 2, _config.i2cTimeoutMs, _config.i2cUser);
}

Status Ssd1315::sendCommand2(uint8_t cmd, uint8_t arg) {
  uint8_t buf[3] = {cmd::CTRL_COMMAND, cmd, arg};
  return _config.i2cWrite(_config.i2cAddress, buf, 3, _config.i2cTimeoutMs, _config.i2cUser);
}

Status Ssd1315::sendCommand3(uint8_t cmd, uint8_t arg1, uint8_t arg2) {
  uint8_t buf[4] = {cmd::CTRL_COMMAND, cmd, arg1, arg2};
  return _config.i2cWrite(_config.i2cAddress, buf, 4, _config.i2cTimeoutMs, _config.i2cUser);
}

Status Ssd1315::sendCommandList(const uint8_t* cmds, size_t len) {
  if (len == 0) return Ok();

  // Send commands with control byte prefix
  // We need a buffer for control byte + commands
  // Use small stack buffer and send in chunks if needed
  constexpr size_t CHUNK_SIZE = 32;
  uint8_t buf[CHUNK_SIZE + 1];
  buf[0] = cmd::CTRL_COMMAND;

  size_t sent = 0;
  while (sent < len) {
    size_t chunk = (len - sent) > CHUNK_SIZE ? CHUNK_SIZE : (len - sent);
    memcpy(buf + 1, cmds + sent, chunk);
    Status st = _config.i2cWrite(_config.i2cAddress, buf, chunk + 1,
                                  _config.i2cTimeoutMs, _config.i2cUser);
    if (!st.ok()) return st;
    sent += chunk;
  }
  return Ok();
}

Status Ssd1315::sendData(const uint8_t* data, size_t len) {
  if (len == 0) return Ok();

  // Send data with control byte prefix
  // ESP32 Wire buffer is 128 bytes, so chunk + control byte must fit
  constexpr size_t CHUNK_SIZE = 64;  // Conservative to fit in Wire buffer
  uint8_t buf[CHUNK_SIZE + 1];
  buf[0] = cmd::CTRL_DATA;

  size_t sent = 0;
  while (sent < len) {
    size_t chunk = (len - sent) > CHUNK_SIZE ? CHUNK_SIZE : (len - sent);
    memcpy(buf + 1, data + sent, chunk);
    Status st = _config.i2cWrite(_config.i2cAddress, buf, chunk + 1,
                                  _config.i2cTimeoutMs, _config.i2cUser);
    if (!st.ok()) return st;
    sent += chunk;
  }
  return Ok();
}

// ============================================================================
// Display control
// ============================================================================

Status Ssd1315::setContrast(uint8_t contrast) {
  if (!_initialized) return Error(Err::NOT_INITIALIZED, "not initialized");
  return sendCommand2(cmd::SET_CONTRAST, contrast);
}

Status Ssd1315::setInvert(bool invert) {
  if (!_initialized) return Error(Err::NOT_INITIALIZED, "not initialized");
  return sendCommand(invert ? cmd::INVERT_DISPLAY : cmd::NORMAL_DISPLAY);
}

Status Ssd1315::setFlipX(bool flip) {
  if (!_initialized) return Error(Err::NOT_INITIALIZED, "not initialized");
  return sendCommand(flip ? cmd::SEG_REMAP_ON : cmd::SEG_REMAP_OFF);
}

Status Ssd1315::setFlipY(bool flip) {
  if (!_initialized) return Error(Err::NOT_INITIALIZED, "not initialized");
  return sendCommand(flip ? cmd::COM_SCAN_DEC : cmd::COM_SCAN_INC);
}

Status Ssd1315::setSleep(bool sleep) {
  if (!_initialized) return Error(Err::NOT_INITIALIZED, "not initialized");

  Status st = sendCommand(sleep ? cmd::DISPLAY_OFF : cmd::DISPLAY_ON);
  if (st.ok()) {
    _sleeping = sleep;
    if (!sleep) {
      // Waking up - start power-on timing guard
      _powerState = PowerState::INIT_DELAY;
      _powerOnMs = 0;  // Will be set on next tick
    }
  }
  return st;
}

Status Ssd1315::setAllPixelsOn(bool allOn) {
  if (!_initialized) return Error(Err::NOT_INITIALIZED, "not initialized");
  Status st = sendCommand(allOn ? cmd::DISPLAY_ALL_ON : cmd::DISPLAY_RAM);
  if (st.ok()) {
    _allPixelsOn = allOn;
  }
  return st;
}

// ============================================================================
// Auto-sleep and activity
// ============================================================================

void Ssd1315::setAutoSleep(uint32_t inactivityMs) {
  _autoSleepMs = inactivityMs;
}

void Ssd1315::touch() {
  if (!_initialized) return;
  _lastActivityMs = 0;  // Will be updated on next tick call
  wakeIfSleeping();
}

void Ssd1315::resetActivityTimer(uint32_t nowMs) {
  _lastActivityMs = nowMs;
}

void Ssd1315::wakeIfSleeping() {
  if (_sleeping && _initialized) {
    setSleep(false);
  }
}

// ============================================================================
// Page cycling
// ============================================================================

void Ssd1315::setUserPageCount(uint8_t count) {
  _userPageCount = (count == 0) ? 1 : count;
  if (_activeUserPage >= _userPageCount) {
    _activeUserPage = 0;
  }
}

void Ssd1315::setActiveUserPage(uint8_t index) {
  _activeUserPage = (index < _userPageCount) ? index : 0;
}

void Ssd1315::setPageCycleInterval(uint32_t intervalMs) {
  _pageCycleMs = intervalMs;
  _lastPageCycleMs = 0;  // Reset timer
}

// ============================================================================
// Tick helpers
// ============================================================================

void Ssd1315::tickPowerOn(uint32_t nowMs) {
  if (_powerState == PowerState::INIT_DELAY) {
    if (_powerOnMs == 0) {
      _powerOnMs = nowMs;
    }
    uint32_t elapsed = nowMs - _powerOnMs;
    if (elapsed >= _config.displayOnDelayMs) {
      _powerState = PowerState::READY;
    }
  }
}

void Ssd1315::tickAutoSleep(uint32_t nowMs) {
  if (_autoSleepMs == 0 || _sleeping) return;

  if (_lastActivityMs == 0) {
    _lastActivityMs = nowMs;
    return;
  }

  uint32_t inactive = nowMs - _lastActivityMs;
  if (inactive >= _autoSleepMs) {
    setSleep(true);
  }
}

void Ssd1315::tickPageCycle(uint32_t nowMs) {
  if (_pageCycleMs == 0 || _userPageCount <= 1) return;

  if (_lastPageCycleMs == 0) {
    _lastPageCycleMs = nowMs;
    return;
  }

  uint32_t elapsed = nowMs - _lastPageCycleMs;
  if (elapsed >= _pageCycleMs) {
    _lastPageCycleMs = nowMs;
    _activeUserPage = (_activeUserPage + 1) % _userPageCount;
  }
}

void Ssd1315::tickFlush(uint32_t nowMs) {
  if (_flushState == FlushState::IDLE || _flushState == FlushState::DONE ||
      _flushState == FlushState::ERROR) {
    return;
  }

  // Initialize flush start time on first tick
  if (_flushStartMs == 0) {
    _flushStartMs = nowMs;
  }

  // Check timeout
  if (_config.flushTimeoutMs > 0) {
    uint32_t elapsed = nowMs - _flushStartMs;
    if (elapsed > _config.flushTimeoutMs) {
      _lastError = Error(Err::TIMEOUT, "flush timeout");
      _flushState = FlushState::ERROR;
      return;
    }
  }

  // Don't flush if panel not ready
  if (_powerState != PowerState::READY) {
    return;
  }

  Status st;

  // State machine
  switch (_flushState) {
    case FlushState::SET_ADDR:
      // Set address window for current page
      st = setAddressWindow(_flushMinCol, _flushMaxCol, _flushPage, _flushPage);
      if (!st.ok()) {
        _lastError = st;
        _flushState = FlushState::ERROR;
        return;
      }
      _flushCol = _flushMinCol;
      _flushState = FlushState::SEND_DATA;
      break;

    case FlushState::SEND_DATA: {
      // Calculate how many bytes we can send this tick
      uint16_t budget = _config.byteBudgetPerTick;
      if (budget == 0) {
        budget = 0xFFFF;  // Unlimited
      }

      // In page buffer mode, we need to map display page to buffer page
      uint8_t bufferPage = _flushPage % _config.pageBufferPages;

      // Send data in chunks
      while (_flushCol <= _flushMaxCol && budget > 0) {
        size_t remaining = _flushMaxCol - _flushCol + 1;
        size_t toSend = (remaining > budget) ? budget : remaining;

        // Prepare data chunk with control byte
        // Buffer layout: buffer[col + bufferPage * width]
        size_t bufOffset = _flushCol + static_cast<size_t>(bufferPage) * _config.width;

        st = sendData(_buffer + bufOffset, toSend);
        if (!st.ok()) {
          _lastError = st;
          _flushState = FlushState::ERROR;
          return;
        }

        _flushCol += toSend;
        budget -= toSend;
      }

      // Check if page complete
      if (_flushCol > _flushMaxCol) {
        // Clear dirty flag for this page
        _dirtyPages &= ~(1 << _flushPage);
        _dirtyMinCol[_flushPage] = 0xFF;
        _dirtyMaxCol[_flushPage] = 0x00;

        // Find next dirty page
        _flushPage++;
        bool found = false;
        while (_flushPage <= _flushEndPage) {
          if (_dirtyPages & (1 << _flushPage)) {
            _flushMinCol = _dirtyMinCol[_flushPage];
            _flushMaxCol = _dirtyMaxCol[_flushPage];
            if (_flushMaxCol >= _flushMinCol) {
              _flushState = FlushState::SET_ADDR;
              found = true;
              break;
            }
          }
          _flushPage++;
        }

        if (!found) {
          _flushState = FlushState::DONE;
        }
      }
      break;
    }

    default:
      break;
  }
}

// ============================================================================
// Flush control
// ============================================================================

Status Ssd1315::requestFlush() {
  if (!_initialized) {
    return Error(Err::NOT_INITIALIZED, "not initialized");
  }
  if (_flushState == FlushState::SET_ADDR || _flushState == FlushState::SEND_DATA) {
    return Error(Err::BUSY, "flush in progress");
  }

  // Find first dirty page
  if (_dirtyPages == 0) {
    _flushState = FlushState::DONE;
    return Ok();
  }

  _flushStartMs = 0;  // Will be set on first tick

  for (uint8_t p = 0; p < _totalPages; p++) {
    if (_dirtyPages & (1 << p)) {
      _flushPage = p;
      _flushMinCol = _dirtyMinCol[p];
      _flushMaxCol = _dirtyMaxCol[p];
      break;
    }
  }

  // Find last dirty page
  for (uint8_t p = _totalPages; p > 0; p--) {
    if (_dirtyPages & (1 << (p - 1))) {
      _flushEndPage = p - 1;
      break;
    }
  }

  _flushState = FlushState::SET_ADDR;
  _flushStartMs = 0;  // Set on next tick

  return Ok();
}

Status Ssd1315::requestFlushRect(int16_t x, int16_t y, int16_t w, int16_t h) {
  if (!_initialized) {
    return Error(Err::NOT_INITIALIZED, "not initialized");
  }
  if (_flushState == FlushState::SET_ADDR || _flushState == FlushState::SEND_DATA) {
    return Error(Err::BUSY, "flush in progress");
  }

  // Clip to display bounds
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > _config.width) w = _config.width - x;
  if (y + h > _config.height) h = _config.height - y;
  if (w <= 0 || h <= 0) {
    _flushState = FlushState::DONE;
    return Ok();
  }

  // Mark pages as dirty
  uint8_t startPage = y / 8;
  uint8_t endPage = (y + h - 1) / 8;
  uint8_t startCol = x;
  uint8_t endCol = x + w - 1;

  for (uint8_t p = startPage; p <= endPage; p++) {
    markDirty(p, startCol, endCol);
  }

  return requestFlush();
}

bool Ssd1315::isFlushing() const {
  return _flushState == FlushState::SET_ADDR || _flushState == FlushState::SEND_DATA;
}

Status Ssd1315::waitFlush(uint32_t nowMs, uint32_t timeoutMs) {
  if (!_initialized) {
    return Error(Err::NOT_INITIALIZED, "not initialized");
  }

  if (timeoutMs == 0) {
    timeoutMs = _config.flushTimeoutMs;
  }
  if (timeoutMs == 0) {
    timeoutMs = 5000;  // Default 5s
  }

  // Use actual current time for start reference
#if defined(ARDUINO)
  uint32_t start = millis();
#else
  uint32_t start = nowMs;
#endif

  // Wait for power-on delay AND flush to complete
  while (isFlushing() || _powerState != PowerState::READY) {
#if defined(ARDUINO)
    uint32_t currentMs = millis();
#else
    uint32_t currentMs = nowMs++;
#endif

    tick(currentMs);
    
    uint32_t elapsed = currentMs - start;
    if (elapsed > timeoutMs) {
      return Error(Err::TIMEOUT, "waitFlush timeout");
    }
    
    // Small delay to prevent tight spinning and feed watchdog
#if defined(ARDUINO)
    delay(1);
#endif
  }

  return _lastError.ok() ? Ok() : _lastError;
}

Status Ssd1315::setAddressWindow(uint8_t colStart, uint8_t colEnd,
                                  uint8_t pageStart, uint8_t pageEnd) {
  Status st;

  // Set column address
  st = sendCommand3(cmd::SET_COL_ADDR, colStart, colEnd);
  if (!st.ok()) return st;

  // Set page address
  st = sendCommand3(cmd::SET_PAGE_ADDR, pageStart, pageEnd);
  if (!st.ok()) return st;

  return Ok();
}

// ============================================================================
// Buffer helpers
// ============================================================================

size_t Ssd1315::bufferIndex(int16_t x, int16_t y) const {
  // In page buffer mode, y is relative to current buffer page
  uint8_t page = (y / 8) % _config.pageBufferPages;
  return x + static_cast<size_t>(page) * _config.width;
}

uint8_t Ssd1315::bufferBit(int16_t y) const {
  return 1 << (y & 7);
}

bool Ssd1315::isInBuffer(int16_t x, int16_t y) const {
  if (x < 0 || x >= _config.width || y < 0 || y >= _config.height) {
    return false;
  }
  // In page buffer mode, check if y is in current buffer range
  if (isPageBufferMode()) {
    uint8_t page = y / 8;
    uint8_t bufferStartPage = _currentBufferPage * _config.pageBufferPages;
    uint8_t bufferEndPage = bufferStartPage + _config.pageBufferPages - 1;
    return page >= bufferStartPage && page <= bufferEndPage;
  }
  return true;
}

size_t Ssd1315::getBufferSize() const {
  if (!_initialized) return 0;
  return static_cast<size_t>(_config.width) * _config.pageBufferPages;
}

void Ssd1315::markDirty(uint8_t page, uint8_t minCol, uint8_t maxCol) {
  if (page >= _totalPages) return;
  if (maxCol >= _config.width) maxCol = _config.width - 1;
  if (minCol > maxCol) return;

  _dirtyPages |= (1 << page);
  if (minCol < _dirtyMinCol[page]) _dirtyMinCol[page] = minCol;
  if (maxCol > _dirtyMaxCol[page]) _dirtyMaxCol[page] = maxCol;
}

void Ssd1315::markAllDirty() {
  for (uint8_t p = 0; p < _totalPages; p++) {
    _dirtyPages |= (1 << p);
    _dirtyMinCol[p] = 0;
    _dirtyMaxCol[p] = _config.width - 1;
  }
}

void Ssd1315::clearDirty() {
  _dirtyPages = 0;
  memset(_dirtyMinCol, 0xFF, sizeof(_dirtyMinCol));
  memset(_dirtyMaxCol, 0x00, sizeof(_dirtyMaxCol));
}

bool Ssd1315::isDirty() const {
  return _dirtyPages != 0;
}

// ============================================================================
// Page buffer mode
// ============================================================================

bool Ssd1315::isPageBufferMode() const {
  return _initialized && _config.pageBufferPages < _totalPages;
}

void Ssd1315::firstPage() {
  if (!_initialized) return;

  memset(_buffer, 0, getBufferSize());
  _currentBufferPage = 0;
  _inPageIteration = true;
  clearDirty();

  resetActivityTimer(0);
  wakeIfSleeping();
}

bool Ssd1315::nextPage() {
  if (!_initialized || !_inPageIteration) return false;

  // Mark current buffer pages as dirty and flush
  for (uint8_t p = 0; p < _config.pageBufferPages; p++) {
    uint8_t displayPage = _currentBufferPage * _config.pageBufferPages + p;
    if (displayPage < _totalPages) {
      markDirty(displayPage, 0, _config.width - 1);
    }
  }

  // Blocking flush of current page(s)
  requestFlush();
  
  // Use waitFlush with proper timeout
  Status st = waitFlush(millis(), _config.flushTimeoutMs);
  if (!st.ok()) {
    // Flush failed, but keep iteration active for retry
    return true;  // Allow caller to retry
  }

  // Move to next page set
  _currentBufferPage++;
  uint8_t nextDisplayPage = _currentBufferPage * _config.pageBufferPages;

  if (nextDisplayPage >= _totalPages) {
    // Iteration complete
    _inPageIteration = false;
    _currentBufferPage = 0;
    return false;
  }

  // Clear buffer for next page
  memset(_buffer, 0, getBufferSize());
  clearDirty();

  return true;
}

int16_t Ssd1315::pageBufferYOffset() const {
  if (!_initialized) return 0;
  return _currentBufferPage * _config.pageBufferPages * 8;
}

// ============================================================================
// Drawing primitives
// ============================================================================

void Ssd1315::clear() {
  if (!_initialized || _buffer == nullptr) return;

  memset(_buffer, 0, getBufferSize());
  markAllDirty();
  resetActivityTimer(0);
  wakeIfSleeping();
}

void Ssd1315::fill() {
  if (!_initialized || _buffer == nullptr) return;

  memset(_buffer, 0xFF, getBufferSize());
  markAllDirty();
  resetActivityTimer(0);
  wakeIfSleeping();
}

void Ssd1315::setPixel(int16_t x, int16_t y, bool on) {
  if (!isInBuffer(x, y)) return;

  // Adjust y for page buffer mode
  int16_t bufY = y;
  if (isPageBufferMode()) {
    int16_t offset = pageBufferYOffset();
    bufY = y - offset;
    if (bufY < 0 || bufY >= _config.pageBufferPages * 8) return;
  }

  size_t idx = bufferIndex(x, bufY);
  uint8_t bit = bufferBit(bufY);

  if (on) {
    _buffer[idx] |= bit;
  } else {
    _buffer[idx] &= ~bit;
  }

  markDirty(y / 8, x, x);
  resetActivityTimer(0);
  wakeIfSleeping();
}

bool Ssd1315::getPixel(int16_t x, int16_t y) const {
  if (!isInBuffer(x, y)) return false;

  int16_t bufY = y;
  if (isPageBufferMode()) {
    int16_t offset = pageBufferYOffset();
    bufY = y - offset;
    if (bufY < 0 || bufY >= _config.pageBufferPages * 8) return false;
  }

  size_t idx = bufferIndex(x, bufY);
  uint8_t bit = bufferBit(bufY);
  return (_buffer[idx] & bit) != 0;
}

void Ssd1315::drawHLine(int16_t x, int16_t y, int16_t w, bool on) {
  if (w <= 0) return;
  for (int16_t i = 0; i < w; i++) {
    setPixel(x + i, y, on);
  }
}

void Ssd1315::drawVLine(int16_t x, int16_t y, int16_t h, bool on) {
  if (h <= 0) return;
  for (int16_t i = 0; i < h; i++) {
    setPixel(x, y + i, on);
  }
}

void Ssd1315::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, bool on) {
  if (w <= 0 || h <= 0) return;
  drawHLine(x, y, w, on);
  drawHLine(x, y + h - 1, w, on);
  drawVLine(x, y, h, on);
  drawVLine(x + w - 1, y, h, on);
}

void Ssd1315::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, bool on) {
  if (w <= 0 || h <= 0) return;
  for (int16_t j = 0; j < h; j++) {
    drawHLine(x, y + j, w, on);
  }
}

void Ssd1315::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, bool on) {
  // Bresenham's line algorithm
  int16_t dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
  int16_t dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
  int16_t sx = (x0 < x1) ? 1 : -1;
  int16_t sy = (y0 < y1) ? 1 : -1;
  int16_t err = dx - dy;

  while (true) {
    setPixel(x0, y0, on);
    if (x0 == x1 && y0 == y1) break;
    int16_t e2 = 2 * err;
    if (e2 > -dy) {
      err -= dy;
      x0 += sx;
    }
    if (e2 < dx) {
      err += dx;
      y0 += sy;
    }
  }
}

void Ssd1315::drawCircle(int16_t cx, int16_t cy, int16_t r, bool on) {
  // Midpoint circle algorithm
  int16_t x = r;
  int16_t y = 0;
  int16_t err = 0;

  while (x >= y) {
    setPixel(cx + x, cy + y, on);
    setPixel(cx + y, cy + x, on);
    setPixel(cx - y, cy + x, on);
    setPixel(cx - x, cy + y, on);
    setPixel(cx - x, cy - y, on);
    setPixel(cx - y, cy - x, on);
    setPixel(cx + y, cy - x, on);
    setPixel(cx + x, cy - y, on);

    y++;
    err += 1 + 2 * y;
    if (2 * (err - x) + 1 > 0) {
      x--;
      err += 1 - 2 * x;
    }
  }
}

void Ssd1315::fillCircle(int16_t cx, int16_t cy, int16_t r, bool on) {
  drawVLine(cx, cy - r, 2 * r + 1, on);

  int16_t x = r;
  int16_t y = 0;
  int16_t err = 0;

  while (x >= y) {
    drawVLine(cx + x, cy - y, 2 * y + 1, on);
    drawVLine(cx + y, cy - x, 2 * x + 1, on);
    drawVLine(cx - y, cy - x, 2 * x + 1, on);
    drawVLine(cx - x, cy - y, 2 * y + 1, on);

    y++;
    err += 1 + 2 * y;
    if (2 * (err - x) + 1 > 0) {
      x--;
      err += 1 - 2 * x;
    }
  }
}

void Ssd1315::drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap,
                          int16_t w, int16_t h, bool on) {
  if (bitmap == nullptr || w <= 0 || h <= 0) return;

  int16_t byteWidth = (w + 7) / 8;

  for (int16_t j = 0; j < h; j++) {
    for (int16_t i = 0; i < w; i++) {
      // Get bit from bitmap (MSB first)
      int16_t byteIdx = j * byteWidth + i / 8;
      uint8_t bitMask = 0x80 >> (i & 7);
      if (bitmap[byteIdx] & bitMask) {
        setPixel(x + i, y + j, on);
      }
    }
  }
}

// ============================================================================
// Text rendering
// ============================================================================

void Ssd1315::drawChar(int16_t x, int16_t y, char c, bool on) {
  uint8_t ch = static_cast<uint8_t>(c);

  // Map to font index
  if (ch < FONT_FIRST_CHAR || ch >= FONT_FIRST_CHAR + FONT_CHAR_COUNT) {
    // Draw replacement character (filled box)
    fillRect(x, y, FONT_WIDTH, FONT_HEIGHT, on);
    return;
  }

  uint8_t idx = ch - FONT_FIRST_CHAR;
  const uint8_t* glyph = &FONT_5X7[idx * FONT_WIDTH];

  // Draw glyph columns
  for (uint8_t col = 0; col < FONT_WIDTH; col++) {
    uint8_t colData = glyph[col];
    for (uint8_t row = 0; row < FONT_HEIGHT; row++) {
      if (colData & (1 << row)) {
        setPixel(x + col, y + row, on);
      }
    }
  }
}

int16_t Ssd1315::drawText(int16_t x, int16_t y, const char* str, bool on) {
  if (str == nullptr) return x;

  int16_t cursorX = x;
  while (*str) {
    char c = *str++;

    if (c == '\n') {
      cursorX = x;
      y += CHAR_HEIGHT;
      continue;
    }

    if (c == '\r') {
      cursorX = x;
      continue;
    }

    drawChar(cursorX, y, c, on);
    cursorX += CHAR_WIDTH;
  }

  return cursorX;
}

int16_t Ssd1315::getTextWidth(const char* str) {
  if (str == nullptr) return 0;

  int16_t maxWidth = 0;
  int16_t curWidth = 0;

  while (*str) {
    char c = *str++;
    if (c == '\n') {
      if (curWidth > maxWidth) maxWidth = curWidth;
      curWidth = 0;
    } else if (c != '\r') {
      curWidth += CHAR_WIDTH;
    }
  }

  return (curWidth > maxWidth) ? curWidth : maxWidth;
}

// ============================================================================
// Test patterns
// ============================================================================

void Ssd1315::fillCheckerboard(uint8_t size) {
  if (!_initialized || size == 0) return;

  for (int16_t y = 0; y < _config.height; y++) {
    for (int16_t x = 0; x < _config.width; x++) {
      bool on = ((x / size) + (y / size)) & 1;
      setPixel(x, y, on);
    }
  }
}

void Ssd1315::fillVerticalStripes(uint8_t width) {
  if (!_initialized || width == 0) return;

  for (int16_t y = 0; y < _config.height; y++) {
    for (int16_t x = 0; x < _config.width; x++) {
      bool on = (x / width) & 1;
      setPixel(x, y, on);
    }
  }
}

void Ssd1315::fillHorizontalStripes(uint8_t height) {
  if (!_initialized || height == 0) return;

  for (int16_t y = 0; y < _config.height; y++) {
    for (int16_t x = 0; x < _config.width; x++) {
      bool on = (y / height) & 1;
      setPixel(x, y, on);
    }
  }
}

// ============================================================================
// Hardware scrolling
// ============================================================================

Status Ssd1315::startHorizontalScroll(bool left, uint8_t startPage, uint8_t endPage,
                                       ScrollSpeed speed) {
  if (!_initialized) return Error(Err::NOT_INITIALIZED, "not initialized");

  // Validate
  if (startPage > endPage || endPage >= _totalPages) {
    return Error(Err::INVALID_CONFIG, "invalid page range");
  }

  // Deactivate first
  Status st = sendCommand(cmd::SCROLL_DEACTIVATE);
  if (!st.ok()) return st;

  // Setup scroll: cmd, dummy, startPage, speed, endPage, dummy, dummy
  uint8_t cmds[7] = {
      left ? cmd::SCROLL_LEFT : cmd::SCROLL_RIGHT,
      0x00,
      startPage,
      static_cast<uint8_t>(speed),
      endPage,
      0x00,
      0xFF};

  st = sendCommandList(cmds, sizeof(cmds));
  if (!st.ok()) return st;

  // Activate
  return sendCommand(cmd::SCROLL_ACTIVATE);
}

Status Ssd1315::startVerticalScroll(bool left, uint8_t startPage, uint8_t endPage,
                                     ScrollSpeed speed, uint8_t verticalOffset) {
  if (!_initialized) return Error(Err::NOT_INITIALIZED, "not initialized");

  if (startPage > endPage || endPage >= _totalPages) {
    return Error(Err::INVALID_CONFIG, "invalid page range");
  }

  Status st = sendCommand(cmd::SCROLL_DEACTIVATE);
  if (!st.ok()) return st;

  // Setup: cmd, dummy, startPage, speed, endPage, verticalOffset
  uint8_t cmds[6] = {
      left ? cmd::SCROLL_VERT_LEFT : cmd::SCROLL_VERT_RIGHT,
      0x00,
      startPage,
      static_cast<uint8_t>(speed),
      endPage,
      verticalOffset};

  st = sendCommandList(cmds, sizeof(cmds));
  if (!st.ok()) return st;

  return sendCommand(cmd::SCROLL_ACTIVATE);
}

Status Ssd1315::stopScroll() {
  if (!_initialized) return Error(Err::NOT_INITIALIZED, "not initialized");
  return sendCommand(cmd::SCROLL_DEACTIVATE);
}

Status Ssd1315::setVerticalScrollArea(uint8_t topFixedRows, uint8_t scrollRows) {
  if (!_initialized) return Error(Err::NOT_INITIALIZED, "not initialized");
  return sendCommand3(cmd::SET_VERT_SCROLL_AREA, topFixedRows, scrollRows);
}

// ============================================================================
// Advanced display features
// ============================================================================

Status Ssd1315::setFadeMode(FadeMode mode, uint8_t interval) {
  if (!_initialized) return Error(Err::NOT_INITIALIZED, "not initialized");
  uint8_t arg = static_cast<uint8_t>(mode) | (interval & 0x0F);
  return sendCommand2(cmd::SET_FADE_BLINK, arg);
}

Status Ssd1315::setZoom(bool enable) {
  if (!_initialized) return Error(Err::NOT_INITIALIZED, "not initialized");
  return sendCommand2(cmd::SET_ZOOM, enable ? 0x01 : 0x00);
}

}  // namespace ssd1315
