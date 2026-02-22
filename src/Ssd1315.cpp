/**
 * @file Ssd1315.cpp
 * @brief SSD1315 OLED display driver implementation.
 */

#include "ssd1315/Ssd1315.h"

#include <new>       // std::nothrow
#include <string.h>  // memset
#include <Arduino.h>  // millis()

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

namespace {

static constexpr uint8_t OUT_LEFT = 0x01;
static constexpr uint8_t OUT_RIGHT = 0x02;
static constexpr uint8_t OUT_TOP = 0x04;
static constexpr uint8_t OUT_BOTTOM = 0x08;

uint8_t outCode(int32_t x, int32_t y,
                int32_t xMin, int32_t yMin,
                int32_t xMax, int32_t yMax) {
  uint8_t code = 0;
  if (x < xMin) {
    code |= OUT_LEFT;
  } else if (x > xMax) {
    code |= OUT_RIGHT;
  }

  if (y < yMin) {
    code |= OUT_TOP;
  } else if (y > yMax) {
    code |= OUT_BOTTOM;
  }

  return code;
}

}  // namespace

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
// Health tracking helpers
// ============================================================================

Status Ssd1315::_updateHealth(const Status& st) {
  // Determine success: OK or IN_PROGRESS are both considered success
  bool isSuccess = st.ok() || st.code == Err::IN_PROGRESS;

  // Health COUNTERS are always updated (regardless of _initialized)
  if (isSuccess) {
    _lastOkMs = millis();
    _consecutiveFailures = 0;
    _totalSuccess++;
  } else {
    _lastError = st;
    _lastErrorMs = millis();
    // Saturate at 255 to prevent uint8_t wrap-around. Without saturation,
    // 256 consecutive failures would wrap the counter back to 0, causing the
    // READY→DEGRADED transition (which triggers at _consecutiveFailures == 1)
    // to fire again on the 257th failure instead of staying OFFLINE.
    // Successes still reset the counter to 0 normally — recovery is unaffected.
    if (_consecutiveFailures < 255u) {
      _consecutiveFailures++;
    }
    _totalFailures++;
  }

  // DriverState transitions are ONLY allowed when _initialized == true
  // This preserves the invariant: _initialized == false ⇒ _driverState == UNINIT
  if (_initialized) {
    if (isSuccess) {
      // Any success → READY (from UNINIT, DEGRADED, or OFFLINE)
      if (_driverState != DriverState::READY) {
        _driverState = DriverState::READY;
      }
    } else {
      // Failure handling: UNINIT/READY → DEGRADED on first failure
      // Note: UNINIT can occur during init when _initialized is already true
      if (_consecutiveFailures == 1 &&
          (_driverState == DriverState::READY || _driverState == DriverState::UNINIT)) {
        _driverState = DriverState::DEGRADED;
      }
      // DEGRADED → OFFLINE when threshold reached
      if (_consecutiveFailures >= _config.offlineThreshold) {
        _driverState = DriverState::OFFLINE;
      }
    }
  }
  return st;
}

Status Ssd1315::_i2cWriteRaw(const uint8_t* data, size_t len) {
  if (!_config.i2cWrite) {
    return Error(Err::INVALID_CONFIG, "I2C write callback null");
  }
  if (len > 0 && data == nullptr) {
    return Error(Err::INVALID_CONFIG, "I2C data pointer null");
  }
  return _config.i2cWrite(_config.i2cAddress, data, len,
                          _config.i2cTimeoutMs, _config.i2cUser);
}

Status Ssd1315::_i2cWriteTracked(const uint8_t* data, size_t len) {
  Status st = _i2cWriteRaw(data, len);
  return _updateHealth(st);
}

Status Ssd1315::_applyConfig() {
  // Apply stored configuration to device.
  // Used by both begin() and recover().
  // Uses tracked I2C wrappers for health tracking.

  Status st;

  // Step 1: Run init sequence (uses sendCommand* which will be tracked)
  st = initDisplay();
  if (!st.ok()) return st;

  // Step 2: Clear GDDRAM
  st = clearGddram();
  if (!st.ok()) return st;

  return Ok();
}

// ============================================================================
// Probe and recovery
// ============================================================================

Status Ssd1315::probe() {
  // SSD1315 has no WHOAMI register. We send NOP (0xE3) and check ACK.
  // NOP command is safe and has no side effects.
  if (!_config.i2cWrite) {
    return Error(Err::INVALID_CONFIG, "I2C write callback null");
  }

  uint8_t buf[2] = {cmd::CTRL_COMMAND, cmd::NOP};
  Status st = _i2cWriteRaw(buf, 2);  // No health tracking!

  if (!st.ok() && (st.code == Err::I2C_NACK_ADDR || st.code == Err::I2C_NACK_DATA ||
                   st.code == Err::I2C_TIMEOUT || st.code == Err::TIMEOUT)) {
    return Error(Err::DEVICE_NOT_FOUND, st.detail, "Device not responding");
  }

  // Note: probe() does NOT call _updateHealth() - diagnostic only
  return st;
}

Status Ssd1315::recover() {
  // Can't recover if never initialized
  if (!_initialized) {
    return Error(Err::NOT_INITIALIZED, "begin() not called");
  }

  // Step 1: Probe device (probe itself is diagnostic-only)
  Status st = probe();
  if (!st.ok()) {
    // Explicitly track probe failure as a real failure
    _updateHealth(st);
    return st;
  }

  // Step 2: Re-apply configuration (includes init sequence)
  // _applyConfig uses tracked wrappers internally via sendCommand*
  st = _applyConfig();
  // Note: _updateHealth already called by sendCommand* internals

  if (st.ok()) {
    // Step 3: Mark framebuffer dirty for resync
    markAllDirty();
  }

  return st;
}

// ============================================================================
// Lifecycle
// ============================================================================

Status Ssd1315::begin(const Config& config) {
  // Clean up previous state if any
  if (_initialized) {
    end();
  }

  // Reset stale pointers/state from any prior failed initialization attempt.
  _buffer = nullptr;
  _ownsBuffer = false;
  _totalPages = 0;
  _flushState = FlushState::IDLE;
  _powerState = PowerState::OFF;
  _dirtyPages = 0;
  _flushStartMs = 0;
  _flushStarted = false;
  _flushError = Ok();
  memset(_dirtyMinCol, 0xFF, sizeof(_dirtyMinCol));
  memset(_dirtyMaxCol, 0x00, sizeof(_dirtyMaxCol));

  // ========== Validate configuration BEFORE copying ==========
  // All validation uses the input 'config' parameter, not _config.
  // This ensures _config is only set after validation passes.

  if (config.i2cWrite == nullptr) {
    return Error(Err::INVALID_CONFIG, "i2cWrite callback is null");
  }
  if (config.width == 0 || config.width > MAX_WIDTH) {
    return Error(Err::INVALID_DIMENSIONS, "width out of range [1..128]");
  }
  if (config.height == 0 || config.height > MAX_HEIGHT || (config.height % 8) != 0) {
    return Error(Err::INVALID_DIMENSIONS, "height must be 8..64, multiple of 8");
  }
  if (config.i2cAddress < 0x03 || config.i2cAddress > 0x77) {
    return Error(Err::INVALID_CONFIG, "i2cAddress must be 7-bit (0x03..0x77)");
  }
  if (config.clockDivide == 0 || config.clockDivide > 16) {
    return Error(Err::INVALID_CONFIG, "clockDivide must be 1..16");
  }
  if (config.oscFrequency > 15) {
    return Error(Err::INVALID_CONFIG, "oscFrequency must be 0..15");
  }
  if (config.prechargePhase1 == 0 || config.prechargePhase1 > 15 ||
      config.prechargePhase2 == 0 || config.prechargePhase2 > 15) {
    return Error(Err::INVALID_CONFIG, "prechargePhase1/2 must be 1..15");
  }
  if (config.displayOffset > 63) {
    return Error(Err::INVALID_CONFIG, "displayOffset must be 0..63");
  }
  if (config.startLine > 63) {
    return Error(Err::INVALID_CONFIG, "startLine must be 0..63");
  }
  if (config.i2cTimeoutMs == 0) {
    return Error(Err::INVALID_CONFIG, "i2cTimeoutMs must be > 0");
  }

  const uint8_t totalPages = config.height / 8;
  if (config.pageBufferPages == 0 || config.pageBufferPages > totalPages) {
    return Error(Err::INVALID_PAGE_COUNT, "pageBufferPages out of range");
  }

  // ========== Validation passed — now copy config and initialize state ==========

  _config = config;
  _totalPages = totalPages;
  _driverState = DriverState::UNINIT;
  _initialized = false;

  // Clamp threshold (modify our copy, not the input)
  if (_config.offlineThreshold < 1) {
    _config.offlineThreshold = 1;
  }

  // Reset health tracking
  _lastOkMs = 0;
  _lastErrorMs = 0;
  _lastError = Ok();
  _consecutiveFailures = 0;
  _totalFailures = 0;
  _totalSuccess = 0;
  _flushError = Ok();

  // Probe device (uses _config.i2cWrite which is now set)
  Status st = probe();
  if (!st.ok()) {
    // Probe failed before init; track failure but stay UNINIT
    // Note: counters update, but _driverState stays UNINIT (not yet initialized)
    _updateHealth(st);
    return st;
  }

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
  _dirtyPages = 0;
  memset(_dirtyMinCol, 0xFF, sizeof(_dirtyMinCol));
  memset(_dirtyMaxCol, 0x00, sizeof(_dirtyMaxCol));
  _currentBufferPage = 0;
  _inPageIteration = false;
  _lastWakeAttemptMs = 0;

  // Copy feature config
  _autoSleepMs = _config.inactivitySleepMs;
  _pageCycleMs = _config.pageCycleMs;
  _userPageCount = 1;
  _activeUserPage = 0;

  // Set _initialized = true BEFORE _applyConfig() so that _updateHealth()
  // can perform state transitions during init.
  // Keep _driverState = UNINIT; _updateHealth() will transition it:
  //   - First I2C success → READY
  //   - First I2C failure → DEGRADED (or OFFLINE if threshold is 1)
  _initialized = true;
  // Note: _driverState remains UNINIT here; _updateHealth() handles transitions

  // Apply config (includes init sequence)
  // _applyConfig uses tracked wrappers, so health is updated per I2C op.
  // First success: UNINIT → READY. First failure: UNINIT → DEGRADED.
  // Subsequent failures: DEGRADED → OFFLINE (based on threshold).
  st = _applyConfig();
  if (!st.ok()) {
    // Init failed - rollback to UNINIT
    // Note: _driverState may be DEGRADED or OFFLINE at this point
    if (_ownsBuffer) {
      delete[] _buffer;
    }
    _buffer = nullptr;
    _ownsBuffer = false;
    _initialized = false;
    _driverState = DriverState::UNINIT;
    _powerState = PowerState::OFF;
    return st;
  }

  // Success: _driverState should already be READY from _updateHealth() calls.
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

  // Turn off display (will track via sendCommand which uses _i2cWriteTracked)
  sendCommand(cmd::DISPLAY_OFF);

  // Free buffer if we own it
  if (_ownsBuffer && _buffer != nullptr) {
    delete[] _buffer;
  }
  _buffer = nullptr;
  _ownsBuffer = false;
  _initialized = false;
  _driverState = DriverState::UNINIT;
  _powerState = PowerState::OFF;
  _lastWakeAttemptMs = 0;

  // Note: Health counters and timestamps are NOT reset.
  // They remain available for post-mortem diagnostics.
  // Counters will be reset on next begin() call.
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
    // Use tracked write for clearGddram during init
    st = _i2cWriteTracked(buf, chunk + 1);
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
  return _i2cWriteTracked(buf, 2);
}

Status Ssd1315::sendCommand2(uint8_t cmd, uint8_t arg) {
  uint8_t buf[3] = {cmd::CTRL_COMMAND, cmd, arg};
  return _i2cWriteTracked(buf, 3);
}

Status Ssd1315::sendCommand3(uint8_t cmd, uint8_t arg1, uint8_t arg2) {
  uint8_t buf[4] = {cmd::CTRL_COMMAND, cmd, arg1, arg2};
  return _i2cWriteTracked(buf, 4);
}

Status Ssd1315::sendCommandList(const uint8_t* cmds, size_t len) {
  if (len == 0) return Ok();
  if (cmds == nullptr) {
    return Error(Err::INVALID_CONFIG, "command list pointer null");
  }

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
    Status st = _i2cWriteTracked(buf, chunk + 1);
    if (!st.ok()) return st;
    sent += chunk;
  }
  return Ok();
}

Status Ssd1315::sendData(const uint8_t* data, size_t len) {
  if (len == 0) return Ok();
  if (data == nullptr) {
    return Error(Err::INVALID_CONFIG, "data pointer null");
  }

  // Send data with control byte prefix
  // ESP32 Wire buffer is 128 bytes, so chunk + control byte must fit
  // NOTE: Uses _i2cWriteRaw() because flush tracks health once at DONE/ERROR,
  // not per chunk. See tickFlush() for flush health tracking.
  constexpr size_t CHUNK_SIZE = 64;  // Conservative to fit in Wire buffer
  uint8_t buf[CHUNK_SIZE + 1];
  buf[0] = cmd::CTRL_DATA;

  size_t sent = 0;
  while (sent < len) {
    size_t chunk = (len - sent) > CHUNK_SIZE ? CHUNK_SIZE : (len - sent);
    memcpy(buf + 1, data + sent, chunk);
    Status st = _i2cWriteRaw(buf, chunk + 1);
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
  Status st = sendCommand2(cmd::SET_CONTRAST, contrast);
  if (st.ok()) {
    _config.contrast = contrast;
  }
  return st;
}

Status Ssd1315::setInvert(bool invert) {
  if (!_initialized) return Error(Err::NOT_INITIALIZED, "not initialized");
  Status st = sendCommand(invert ? cmd::INVERT_DISPLAY : cmd::NORMAL_DISPLAY);
  if (st.ok()) {
    _config.invert = invert;
  }
  return st;
}

Status Ssd1315::setFlipX(bool flip) {
  if (!_initialized) return Error(Err::NOT_INITIALIZED, "not initialized");
  Status st = sendCommand(flip ? cmd::SEG_REMAP_ON : cmd::SEG_REMAP_OFF);
  if (st.ok()) {
    _config.flipX = flip;
  }
  return st;
}

Status Ssd1315::setFlipY(bool flip) {
  if (!_initialized) return Error(Err::NOT_INITIALIZED, "not initialized");
  Status st = sendCommand(flip ? cmd::COM_SCAN_DEC : cmd::COM_SCAN_INC);
  if (st.ok()) {
    _config.flipY = flip;
  }
  return st;
}

Status Ssd1315::setSleep(bool sleep) {
  if (!_initialized) return Error(Err::NOT_INITIALIZED, "not initialized");

  Status st = sendCommand(sleep ? cmd::DISPLAY_OFF : cmd::DISPLAY_ON);
  if (st.ok()) {
    _sleeping = sleep;
    _lastWakeAttemptMs = 0;
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
  _config.inactivitySleepMs = inactivityMs;
}

void Ssd1315::touch() {
  if (!_initialized) return;
  // Delegate to resetActivityTimer() to ensure the "not-started" sentinel (0)
  // is never written to _lastActivityMs, even at boot when millis() == 0.
  resetActivityTimer(millis());
  wakeIfSleeping();
}

void Ssd1315::resetActivityTimer(uint32_t nowMs) {
  // 0 is used as the "not-started" sentinel in tickAutoSleep; avoid writing it.
  // If nowMs is genuinely 0 (boot edge case), use 1 to keep the sentinel distinct.
  _lastActivityMs = (nowMs != 0) ? nowMs : 1u;
}

void Ssd1315::wakeIfSleeping() {
  if (_sleeping && _initialized) {
    const uint32_t nowMs = millis();
    constexpr uint32_t WAKE_RETRY_BACKOFF_MS = 10;

    if (_lastWakeAttemptMs != 0 &&
        (nowMs - _lastWakeAttemptMs) < WAKE_RETRY_BACKOFF_MS) {
      return;
    }

    _lastWakeAttemptMs = nowMs;
    Status st = setSleep(false);
    if (st.ok()) {
      _lastWakeAttemptMs = 0;
    }
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
  _config.pageCycleMs = intervalMs;
  _lastPageCycleMs = 0;  // Reset timer
}

// ============================================================================
// Tick helpers
// ============================================================================

void Ssd1315::tickPowerOn(uint32_t nowMs) {
  if (_powerState == PowerState::INIT_DELAY) {
    if (_powerOnMs == 0) {
      // Avoid 0 (the "not started" sentinel) if millis() is genuinely 0.
      _powerOnMs = (nowMs != 0) ? nowMs : 1u;
    }
    // Unsigned subtraction handles millis() rollover correctly.
    uint32_t elapsed = nowMs - _powerOnMs;
    if (elapsed >= _config.displayOnDelayMs) {
      _powerState = PowerState::READY;
    }
  }
}

void Ssd1315::tickAutoSleep(uint32_t nowMs) {
  if (_autoSleepMs == 0 || _sleeping) return;

  if (_lastActivityMs == 0) {
    // First observation: record current time to start the inactivity window.
    // Avoid writing 0 (the sentinel): if millis() is genuinely 0, use 1.
    _lastActivityMs = (nowMs != 0) ? nowMs : 1u;
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
    // Avoid writing 0 (the sentinel): if millis() is genuinely 0, use 1.
    _lastPageCycleMs = (nowMs != 0) ? nowMs : 1u;
    return;
  }

  uint32_t elapsed = nowMs - _lastPageCycleMs;
  if (elapsed >= _pageCycleMs) {
    _lastPageCycleMs = nowMs;
    _activeUserPage = (_activeUserPage + 1) % _userPageCount;
  }
}

void Ssd1315::tickFlush(uint32_t nowMs) {
  // Handle completed flush states - track health once at completion
  if (_flushState == FlushState::DONE) {
    // Flush completed successfully - track ONCE
    _updateHealth(Ok());
    _flushState = FlushState::IDLE;
    return;
  }

  if (_flushState == FlushState::ERROR) {
    // Flush failed - track ONCE with accumulated error
    Status flushFailure =
        _flushError.ok() ? Error(Err::INTERNAL_ERROR, "flush failed") : _flushError;
    _updateHealth(flushFailure);
    _flushError = flushFailure;
    _flushState = FlushState::IDLE;
    return;
  }

  if (_flushState == FlushState::IDLE) {
    return;
  }

  // Don't flush if panel not ready
  if (_powerState != PowerState::READY) {
    return;
  }

  // Initialize flush start time on first tick when power state is READY.
  // A boolean flag avoids any sentinel value ambiguity (including millis()==0
  // at boot or millis() rolling over to UINT32_MAX after ~49 days).
  if (!_flushStarted) {
    _flushStarted = true;
    // Avoid writing 0 so it stays safe to test _lastActivityMs == 0 elsewhere.
    _flushStartMs = (nowMs != 0) ? nowMs : 1u;
    _flushError = Ok();  // Reset accumulated error
  }

  // Check timeout
  if (_config.flushTimeoutMs > 0) {
    uint32_t elapsed = nowMs - _flushStartMs;
    if (elapsed > _config.flushTimeoutMs) {
      _flushError = Error(Err::TIMEOUT, "flush timeout");
      // Write _lastError immediately for real-time diagnostics.
      // _updateHealth() will set it again at ERROR state completion.
      _lastError = _flushError;
      _flushState = FlushState::ERROR;
      return;
    }
  }

  Status st;

  // State machine
  switch (_flushState) {
    case FlushState::SET_ADDR:
      // Set address window for current page
      // Note: setAddressWindow uses sendCommand3 which is tracked,
      // but we don't want per-command tracking during flush.
      // Use raw writes for address setting too.
      {
        uint8_t colBuf[4] = {cmd::CTRL_COMMAND, cmd::SET_COL_ADDR,
                             _flushMinCol,
                             _flushMaxCol};
        st = _i2cWriteRaw(colBuf, 4);
        if (!st.ok()) {
          _flushError = st;
          _lastError = st;  // Immediate diagnostics; _updateHealth() sets at completion
          _flushState = FlushState::ERROR;
          return;
        }

        uint8_t pageBuf[4] = {cmd::CTRL_COMMAND, cmd::SET_PAGE_ADDR, _flushPage, _flushPage};
        st = _i2cWriteRaw(pageBuf, 4);
        if (!st.ok()) {
          _flushError = st;
          _lastError = st;  // Immediate diagnostics; _updateHealth() sets at completion
          _flushState = FlushState::ERROR;
          return;
        }
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

      // Send data in chunks using _i2cWriteRaw() - NO per-chunk health tracking
      while (_flushCol <= _flushMaxCol && budget > 0) {
        size_t remaining = _flushMaxCol - _flushCol + 1;
        size_t toSend = (remaining > budget) ? budget : remaining;

        // Prepare data chunk with control byte
        // Buffer layout: buffer[col + bufferPage * width]
        size_t bufOffset = _flushCol + static_cast<size_t>(bufferPage) * _config.width;

        st = sendData(_buffer + bufOffset, toSend);
        if (!st.ok()) {
          _flushError = st;  // Accumulate error
          _lastError = st;   // Immediate diagnostics; _updateHealth() sets at completion
          _flushState = FlushState::ERROR;
          return;
        }

        _flushCol += toSend;
        budget -= toSend;
      }

      // Check if page complete
      if (_flushCol > _flushMaxCol) {
        // Clear dirty flag for this page
        _dirtyPages &= static_cast<uint8_t>(~(1u << _flushPage));
        _dirtyMinCol[_flushPage] = 0xFF;
        _dirtyMaxCol[_flushPage] = 0x00;

        // Find next dirty page
        _flushPage++;
        bool found = false;
        while (_flushPage <= _flushEndPage) {
          if (_dirtyPages & static_cast<uint8_t>(1u << _flushPage)) {
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

  // If caller requests a new flush before tick() consumes terminal state,
  // account it now to keep health tracking exact.
  if (_flushState == FlushState::DONE) {
    _updateHealth(Ok());
    _flushState = FlushState::IDLE;
  } else if (_flushState == FlushState::ERROR) {
    Status flushFailure =
        _flushError.ok() ? Error(Err::INTERNAL_ERROR, "flush failed") : _flushError;
    _updateHealth(flushFailure);
    _flushError = flushFailure;
    _flushState = FlushState::IDLE;
  }

  if (_flushState == FlushState::SET_ADDR || _flushState == FlushState::SEND_DATA) {
    return Error(Err::BUSY, "flush in progress");
  }

  // Find first dirty page
  if (_dirtyPages == 0) {
    _flushState = FlushState::IDLE;
    return Ok();
  }

  bool foundFirst = false;
  for (uint8_t p = 0; p < _totalPages; p++) {
    if (!(_dirtyPages & static_cast<uint8_t>(1u << p))) {
      continue;
    }

    if (_dirtyMinCol[p] >= _config.width || _dirtyMaxCol[p] < _dirtyMinCol[p]) {
      // Sanitize inconsistent dirty metadata instead of issuing invalid windows.
      _dirtyPages &= static_cast<uint8_t>(~(1u << p));
      _dirtyMinCol[p] = 0xFF;
      _dirtyMaxCol[p] = 0x00;
      continue;
    }

    _flushPage = p;
    _flushMinCol = _dirtyMinCol[p];
    _flushMaxCol = _dirtyMaxCol[p];
    foundFirst = true;
    break;
  }

  if (!foundFirst) {
    _flushState = FlushState::IDLE;
    return Ok();
  }

  _flushEndPage = _flushPage;

  // Find last dirty page
  for (int16_t p = static_cast<int16_t>(_totalPages) - 1;
       p >= static_cast<int16_t>(_flushPage); p--) {
    if ((_dirtyPages & static_cast<uint8_t>(1u << p)) &&
        _dirtyMinCol[p] < _config.width &&
        _dirtyMaxCol[p] >= _dirtyMinCol[p]) {
      _flushEndPage = static_cast<uint8_t>(p);
      break;
    }
  }

  _flushState = FlushState::SET_ADDR;
  _flushStarted = false;  // Timer will be latched on first tick after panel is READY

  return Ok();
}

Status Ssd1315::requestFlushRect(int16_t x, int16_t y, int16_t w, int16_t h) {
  if (!_initialized) {
    return Error(Err::NOT_INITIALIZED, "not initialized");
  }

  if (_flushState == FlushState::DONE) {
    _updateHealth(Ok());
    _flushState = FlushState::IDLE;
  } else if (_flushState == FlushState::ERROR) {
    Status flushFailure =
        _flushError.ok() ? Error(Err::INTERNAL_ERROR, "flush failed") : _flushError;
    _updateHealth(flushFailure);
    _flushError = flushFailure;
    _flushState = FlushState::IDLE;
  }

  if (_flushState == FlushState::SET_ADDR || _flushState == FlushState::SEND_DATA) {
    return Error(Err::BUSY, "flush in progress");
  }

  if (w <= 0 || h <= 0) {
    _flushState = FlushState::IDLE;
    return Ok();
  }

  // Clip using widened math to avoid signed int16 overflow/underflow.
  int32_t x0 = x;
  int32_t y0 = y;
  int32_t x1 = static_cast<int32_t>(x) + static_cast<int32_t>(w) - 1;
  int32_t y1 = static_cast<int32_t>(y) + static_cast<int32_t>(h) - 1;

  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 >= _config.width) x1 = _config.width - 1;
  if (y1 >= _config.height) y1 = _config.height - 1;

  if (x0 > x1 || y0 > y1) {
    _flushState = FlushState::IDLE;
    return Ok();
  }

  // Mark pages as dirty
  uint8_t startPage = static_cast<uint8_t>(y0 / 8);
  uint8_t endPage = static_cast<uint8_t>(y1 / 8);
  uint8_t startCol = static_cast<uint8_t>(x0);
  uint8_t endCol = static_cast<uint8_t>(x1);

  for (uint8_t p = startPage; p <= endPage; p++) {
    markDirty(p, startCol, endCol);
  }

  return requestFlush();
}

bool Ssd1315::isFlushing() const {
  return _flushState == FlushState::SET_ADDR || _flushState == FlushState::SEND_DATA;
}

// ============================================================================
// Blocking single-page flush (used internally during init / test sequences)
// ============================================================================

Status Ssd1315::flushPageBlocking(uint8_t page) {
  if (!_initialized || _buffer == nullptr) {
    return Error(Err::NOT_INITIALIZED, "not initialized");
  }
  if (page >= _totalPages) {
    return Error(Err::INVALID_CONFIG, "page out of range");
  }

  // Set address window for this page only (all columns)
  Status st = setAddressWindow(0, static_cast<uint8_t>(_config.width - 1), page, page);
  if (!st.ok()) return st;

  // Determine buffer offset for this page.
  // In page-buffer mode the physical page maps into the circular buffer;
  // in full-buffer mode each page is stored linearly.
  uint8_t bufPage = isPageBufferMode()
                        ? (page % _config.pageBufferPages)
                        : page;

  const uint8_t* src = _buffer + static_cast<size_t>(bufPage) * _config.width;
  size_t remaining = _config.width;
  size_t offset = 0;

  // Send in chunks bounded by ESP32 Wire buffer (128 bytes max including
  // the control byte → leave room for 1-byte CTRL_DATA prefix per chunk).
  constexpr size_t CHUNK_SIZE = 32;
  uint8_t buf[CHUNK_SIZE + 1];
  buf[0] = cmd::CTRL_DATA;

  while (remaining > 0) {
    size_t chunk = (remaining > CHUNK_SIZE) ? CHUNK_SIZE : remaining;
    memcpy(buf + 1, src + offset, chunk);
    st = _i2cWriteTracked(buf, chunk + 1);
    if (!st.ok()) return st;
    offset += chunk;
    remaining -= chunk;
  }

  return Ok();
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

  // Use caller-provided timestamp when available; otherwise sample now.
  uint32_t start = nowMs;
  if (start == 0) {
    start = millis();
  }

  // Wait for power-on delay AND flush to complete.
  // Uses unsigned subtraction which is safe across millis() rollover.
  while (isFlushing() || (!_sleeping && _powerState != PowerState::READY)) {
    uint32_t currentMs = millis();
    tick(currentMs);

    // Unsigned subtraction handles millis() 32-bit rollover correctly.
    uint32_t elapsed = currentMs - start;
    if (elapsed >= timeoutMs) {
      if (isFlushing()) {
        // Abort active flush and account failure exactly once.
        _flushError = Error(Err::TIMEOUT, "waitFlush timeout");
        _lastError = _flushError;
        _flushState = FlushState::ERROR;
        _updateHealth(_flushError);
        _flushState = FlushState::IDLE;
      }
      return Error(Err::TIMEOUT, "waitFlush timeout");
    }

    // yield() gives the FreeRTOS scheduler a chance to run other tasks and
    // feeds the WDT. It maps to vTaskDelay(1) on ESP32 Arduino, which is
    // distinct from delay() — it yields once rather than blocking for a set
    // duration. This prevents watchdog resets in tight loops without adding
    // fixed latency. Safe to call from loop() context.
    yield();
  }

  // Flush may have reached terminal state in the final tick() above.
  if (_flushState == FlushState::DONE) {
    _updateHealth(Ok());
    _flushState = FlushState::IDLE;
  } else if (_flushState == FlushState::ERROR) {
    Status flushFailure =
        _flushError.ok() ? Error(Err::INTERNAL_ERROR, "flush failed") : _flushError;
    _updateHealth(flushFailure);
    _flushError = flushFailure;
    _flushState = FlushState::IDLE;
    return flushFailure;
  }

  return Ok();
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
  if (!_initialized || _buffer == nullptr) {
    return false;
  }
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

/**
 * @brief Mark a page (column range) in the frame buffer as dirty.
 *
 * In page buffer mode, this function only affects pages that fall within the
 * currently loaded page-buffer window. If @p page is outside the current
 * window, the call is intentionally ignored and no dirty state is recorded.
 * The caller is responsible for invoking markDirty again when that page is
 * present in the active buffer.
 */
void Ssd1315::markDirty(uint8_t page, uint8_t minCol, uint8_t maxCol) {
  if (page >= _totalPages) return;
  if (isPageBufferMode()) {
    uint8_t bufferStartPage = _currentBufferPage * _config.pageBufferPages;
    uint8_t bufferEndPage = bufferStartPage + _config.pageBufferPages - 1;
    if (page < bufferStartPage || page > bufferEndPage) {
      return;
    }
  }
  if (maxCol >= _config.width) maxCol = _config.width - 1;
  if (minCol > maxCol) return;

  _dirtyPages |= static_cast<uint8_t>(1u << page);
  if (minCol < _dirtyMinCol[page]) _dirtyMinCol[page] = minCol;
  if (maxCol > _dirtyMaxCol[page]) _dirtyMaxCol[page] = maxCol;
}

void Ssd1315::markAllDirty() {
  if (_totalPages == 0) {
    return;
  }

  uint8_t startPage = 0;
  uint8_t endPage = _totalPages - 1;

  if (isPageBufferMode()) {
    startPage = _currentBufferPage * _config.pageBufferPages;
    uint16_t endExclusive = static_cast<uint16_t>(startPage) + _config.pageBufferPages;
    if (endExclusive > _totalPages) {
      endExclusive = _totalPages;
    }
    if (startPage >= endExclusive) {
      return;
    }
    endPage = static_cast<uint8_t>(endExclusive - 1);
  }

  for (uint8_t p = startPage; p <= endPage; p++) {
    _dirtyPages |= static_cast<uint8_t>(1u << p);
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

  resetActivityTimer(millis());
  wakeIfSleeping();
}

bool Ssd1315::nextPage() {
  if (!_initialized || !_inPageIteration) return false;

  // If flush is still in progress, return true (more work to do)
  // Caller should call tick() and try again
  if (isFlushing()) {
    return true;
  }

  // Check if previous flush failed
  if (_flushState == FlushState::ERROR) {
    Status flushFailure =
        _flushError.ok() ? Error(Err::INTERNAL_ERROR, "flush failed") : _flushError;
    _updateHealth(flushFailure);
    _flushError = flushFailure;
    _flushState = FlushState::IDLE;
    _inPageIteration = false;
    return false;  // Caller should check lastError()
  }

  // If this is NOT the first call (i.e., we've already flushed at least once),
  // advance to next page set
  if (_flushState == FlushState::DONE) {
    _updateHealth(Ok());
    _currentBufferPage++;
    uint8_t nextDisplayPage = _currentBufferPage * _config.pageBufferPages;

    if (nextDisplayPage >= _totalPages) {
      // Iteration complete
      _inPageIteration = false;
      _currentBufferPage = 0;
      _flushState = FlushState::IDLE;
      return false;
    }

    // Clear buffer for next page and reset flush state
    memset(_buffer, 0, getBufferSize());
    clearDirty();
    _flushState = FlushState::IDLE;
    return true;  // More pages - caller should draw and call nextPage() again
  }

  // Mark current buffer pages as dirty and request flush (non-blocking)
  for (uint8_t p = 0; p < _config.pageBufferPages; p++) {
    uint8_t displayPage = _currentBufferPage * _config.pageBufferPages + p;
    if (displayPage < _totalPages) {
      markDirty(displayPage, 0, _config.width - 1);
    }
  }

  // Start async flush - actual transfer happens in tick()
  Status st = requestFlush();
  if (!st.ok() && st.code != Err::BUSY) {
    _lastError = st;  // Immediate diagnostics for page iteration errors
    _inPageIteration = false;
    return false;
  }

  return true;  // Flush started - caller should call tick() and nextPage() again
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
  resetActivityTimer(millis());
  wakeIfSleeping();
}

void Ssd1315::fill() {
  if (!_initialized || _buffer == nullptr) return;

  memset(_buffer, 0xFF, getBufferSize());
  markAllDirty();
  resetActivityTimer(millis());
  wakeIfSleeping();
}

void Ssd1315::setPixel(int16_t x, int16_t y, bool on) {
  if (!_initialized || _buffer == nullptr) return;
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
  resetActivityTimer(millis());
  wakeIfSleeping();
}

bool Ssd1315::getPixel(int16_t x, int16_t y) const {
  if (!_initialized || _buffer == nullptr) return false;
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
  if (!_initialized || _buffer == nullptr || w <= 0) return;
  if (y < 0 || y >= _config.height) return;

  int32_t x0 = x;
  int32_t x1 = static_cast<int32_t>(x) + static_cast<int32_t>(w) - 1;

  if (x1 < 0 || x0 >= _config.width) return;
  if (x0 < 0) x0 = 0;
  if (x1 >= _config.width) x1 = _config.width - 1;

  // Write directly to the buffer for efficiency, then mark dirty once.
  int16_t bufY = static_cast<int16_t>(y);
  if (isPageBufferMode()) {
    bufY = static_cast<int16_t>(y - pageBufferYOffset());
    if (bufY < 0 || bufY >= _config.pageBufferPages * 8) return;
  }
  uint8_t bit = bufferBit(bufY);
  for (int32_t xi = x0; xi <= x1; xi++) {
    size_t idx = bufferIndex(static_cast<int16_t>(xi), bufY);
    if (on) {
      _buffer[idx] |= bit;
    } else {
      _buffer[idx] &= ~bit;
    }
  }
  markDirty(static_cast<uint8_t>(y / 8),
            static_cast<uint8_t>(x0),
            static_cast<uint8_t>(x1));
  resetActivityTimer(millis());
  wakeIfSleeping();
}

void Ssd1315::drawVLine(int16_t x, int16_t y, int16_t h, bool on) {
  if (!_initialized || _buffer == nullptr || h <= 0) return;
  if (x < 0 || x >= _config.width) return;

  int32_t y0 = y;
  int32_t y1 = static_cast<int32_t>(y) + static_cast<int32_t>(h) - 1;

  if (y1 < 0 || y0 >= _config.height) return;
  if (y0 < 0) y0 = 0;
  if (y1 >= _config.height) y1 = _config.height - 1;

  // Write directly to the buffer for efficiency, then mark dirty once per page.
  for (int32_t yi = y0; yi <= y1; yi++) {
    int16_t bufY = static_cast<int16_t>(yi);
    if (isPageBufferMode()) {
      bufY = static_cast<int16_t>(yi - pageBufferYOffset());
      if (bufY < 0 || bufY >= _config.pageBufferPages * 8) continue;
    }
    size_t idx = bufferIndex(x, bufY);
    uint8_t bit = bufferBit(bufY);
    if (on) {
      _buffer[idx] |= bit;
    } else {
      _buffer[idx] &= ~bit;
    }
  }
  // Mark all touched pages dirty in one pass.
  uint8_t startPage = static_cast<uint8_t>(y0 / 8);
  uint8_t endPage   = static_cast<uint8_t>(y1 / 8);
  for (uint8_t p = startPage; p <= endPage; p++) {
    markDirty(p, static_cast<uint8_t>(x), static_cast<uint8_t>(x));
  }
  resetActivityTimer(millis());
  wakeIfSleeping();
}

void Ssd1315::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, bool on) {
  if (!_initialized || _buffer == nullptr || w <= 0 || h <= 0) return;

  int32_t x0 = x;
  int32_t y0 = y;
  int32_t x1 = static_cast<int32_t>(x) + static_cast<int32_t>(w) - 1;
  int32_t y1 = static_cast<int32_t>(y) + static_cast<int32_t>(h) - 1;

  if (x1 < 0 || y1 < 0 || x0 >= _config.width || y0 >= _config.height) return;

  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 >= _config.width) x1 = _config.width - 1;
  if (y1 >= _config.height) y1 = _config.height - 1;

  int16_t clippedW = static_cast<int16_t>(x1 - x0 + 1);
  int16_t clippedH = static_cast<int16_t>(y1 - y0 + 1);
  int16_t xStart = static_cast<int16_t>(x0);
  int16_t yStart = static_cast<int16_t>(y0);
  int16_t xEnd = static_cast<int16_t>(x1);
  int16_t yEnd = static_cast<int16_t>(y1);

  drawHLine(xStart, yStart, clippedW, on);
  if (yEnd != yStart) {
    drawHLine(xStart, yEnd, clippedW, on);
  }
  drawVLine(xStart, yStart, clippedH, on);
  if (xEnd != xStart) {
    drawVLine(xEnd, yStart, clippedH, on);
  }
}

void Ssd1315::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, bool on) {
  if (!_initialized || _buffer == nullptr || w <= 0 || h <= 0) return;

  int32_t x0 = x;
  int32_t y0 = y;
  int32_t x1 = static_cast<int32_t>(x) + static_cast<int32_t>(w) - 1;
  int32_t y1 = static_cast<int32_t>(y) + static_cast<int32_t>(h) - 1;

  if (x1 < 0 || y1 < 0 || x0 >= _config.width || y0 >= _config.height) return;

  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 >= _config.width) x1 = _config.width - 1;
  if (y1 >= _config.height) y1 = _config.height - 1;

  // Write directly to the buffer for all rows, then mark dirty once.
  for (int32_t yi = y0; yi <= y1; yi++) {
    int16_t bufY = static_cast<int16_t>(yi);
    if (isPageBufferMode()) {
      bufY = static_cast<int16_t>(yi - pageBufferYOffset());
      if (bufY < 0 || bufY >= _config.pageBufferPages * 8) continue;
    }
    uint8_t bit = bufferBit(bufY);
    for (int32_t xi = x0; xi <= x1; xi++) {
      size_t idx = bufferIndex(static_cast<int16_t>(xi), bufY);
      if (on) {
        _buffer[idx] |= bit;
      } else {
        _buffer[idx] &= ~bit;
      }
    }
  }
  // Mark affected pages dirty in one pass.
  uint8_t startPage = static_cast<uint8_t>(y0 / 8);
  uint8_t endPage   = static_cast<uint8_t>(y1 / 8);
  for (uint8_t p = startPage; p <= endPage; p++) {
    markDirty(p, static_cast<uint8_t>(x0), static_cast<uint8_t>(x1));
  }
  resetActivityTimer(millis());
  wakeIfSleeping();
}

void Ssd1315::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, bool on) {
  if (!_initialized || _buffer == nullptr) return;

  int32_t x0c = x0;
  int32_t y0c = y0;
  int32_t x1c = x1;
  int32_t y1c = y1;

  const int32_t xMin = 0;
  const int32_t yMin = 0;
  const int32_t xMax = _config.width - 1;
  const int32_t yMax = _config.height - 1;

  // Cohen-Sutherland clipping keeps Bresenham bounded to display geometry.
  uint8_t code0 = outCode(x0c, y0c, xMin, yMin, xMax, yMax);
  uint8_t code1 = outCode(x1c, y1c, xMin, yMin, xMax, yMax);

  // Guard: each iteration clips at least one endpoint; max 4 clip boundaries,
  // so ≤ 4 iterations are sufficient. Extra iterations indicate a degenerate
  // case from integer rounding — bail out safely.
  uint8_t clipIter = 0;
  while (true) {
    if ((code0 | code1) == 0) {
      break;  // Fully inside
    }
    if ((code0 & code1) || clipIter >= 4) {
      return;  // Fully outside, or degenerate clipping — abort
    }
    clipIter++;

    uint8_t codeOut = (code0 != 0) ? code0 : code1;
    int32_t xNew = 0;
    int32_t yNew = 0;

    if (codeOut & OUT_TOP) {
      if (y1c == y0c) return;
      xNew = x0c + (x1c - x0c) * (yMin - y0c) / (y1c - y0c);
      yNew = yMin;
    } else if (codeOut & OUT_BOTTOM) {
      if (y1c == y0c) return;
      xNew = x0c + (x1c - x0c) * (yMax - y0c) / (y1c - y0c);
      yNew = yMax;
    } else if (codeOut & OUT_RIGHT) {
      if (x1c == x0c) return;
      yNew = y0c + (y1c - y0c) * (xMax - x0c) / (x1c - x0c);
      xNew = xMax;
    } else {  // OUT_LEFT
      if (x1c == x0c) return;
      yNew = y0c + (y1c - y0c) * (xMin - x0c) / (x1c - x0c);
      xNew = xMin;
    }

    if (codeOut == code0) {
      x0c = xNew;
      y0c = yNew;
      code0 = outCode(x0c, y0c, xMin, yMin, xMax, yMax);
    } else {
      x1c = xNew;
      y1c = yNew;
      code1 = outCode(x1c, y1c, xMin, yMin, xMax, yMax);
    }
  }

  // Bresenham's line algorithm
  int32_t dx = (x1c > x0c) ? (x1c - x0c) : (x0c - x1c);
  int32_t dy = (y1c > y0c) ? (y1c - y0c) : (y0c - y1c);
  int32_t sx = (x0c < x1c) ? 1 : -1;
  int32_t sy = (y0c < y1c) ? 1 : -1;
  int32_t err = dx - dy;

  while (true) {
    setPixel(static_cast<int16_t>(x0c), static_cast<int16_t>(y0c), on);
    if (x0c == x1c && y0c == y1c) break;
    int32_t e2 = 2 * err;
    if (e2 > -dy) {
      err -= dy;
      x0c += sx;
    }
    if (e2 < dx) {
      err += dx;
      y0c += sy;
    }
  }
}

void Ssd1315::drawCircle(int16_t cx, int16_t cy, int16_t r, bool on) {
  if (!_initialized || _buffer == nullptr || r < 0) return;
  if (r == 0) {
    setPixel(cx, cy, on);
    return;
  }

  // Keep work bounded even for pathological input radii.
  const int16_t maxRadius = static_cast<int16_t>(_config.width + _config.height);
  if (r > maxRadius) {
    r = maxRadius;
  }

  if (cx + r < 0 || cy + r < 0 ||
      cx - r >= _config.width || cy - r >= _config.height) {
    return;
  }

  // Midpoint circle algorithm
  // Use int32_t for err/x/y to avoid int16_t overflow for large radii.
  int32_t x = r;
  int32_t y = 0;
  int32_t err = 0;

  while (x >= y) {
    setPixel(static_cast<int16_t>(cx + x), static_cast<int16_t>(cy + y), on);
    setPixel(static_cast<int16_t>(cx + y), static_cast<int16_t>(cy + x), on);
    setPixel(static_cast<int16_t>(cx - y), static_cast<int16_t>(cy + x), on);
    setPixel(static_cast<int16_t>(cx - x), static_cast<int16_t>(cy + y), on);
    setPixel(static_cast<int16_t>(cx - x), static_cast<int16_t>(cy - y), on);
    setPixel(static_cast<int16_t>(cx - y), static_cast<int16_t>(cy - x), on);
    setPixel(static_cast<int16_t>(cx + y), static_cast<int16_t>(cy - x), on);
    setPixel(static_cast<int16_t>(cx + x), static_cast<int16_t>(cy - y), on);

    y++;
    err += 1 + 2 * y;
    if (2 * (err - x) + 1 > 0) {
      x--;
      err += 1 - 2 * x;
    }
  }
}

void Ssd1315::fillCircle(int16_t cx, int16_t cy, int16_t r, bool on) {
  if (!_initialized || _buffer == nullptr || r < 0) return;
  if (r == 0) {
    setPixel(cx, cy, on);
    return;
  }

  const int16_t maxRadius = static_cast<int16_t>(_config.width + _config.height);
  if (r > maxRadius) {
    r = maxRadius;
  }

  if (cx + r < 0 || cy + r < 0 ||
      cx - r >= _config.width || cy - r >= _config.height) {
    return;
  }

  drawVLine(cx, cy - r, 2 * r + 1, on);

  // Use int32_t for err/x/y to avoid int16_t overflow for large radii.
  int32_t x = r;
  int32_t y = 0;
  int32_t err = 0;

  while (x >= y) {
    drawVLine(static_cast<int16_t>(cx + x), static_cast<int16_t>(cy - y), static_cast<int16_t>(2 * y + 1), on);
    drawVLine(static_cast<int16_t>(cx + y), static_cast<int16_t>(cy - x), static_cast<int16_t>(2 * x + 1), on);
    drawVLine(static_cast<int16_t>(cx - y), static_cast<int16_t>(cy - x), static_cast<int16_t>(2 * x + 1), on);
    drawVLine(static_cast<int16_t>(cx - x), static_cast<int16_t>(cy - y), static_cast<int16_t>(2 * y + 1), on);

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
  if (!_initialized || _buffer == nullptr ||
      bitmap == nullptr || w <= 0 || h <= 0) return;

  int32_t x0 = x;
  int32_t y0 = y;
  int32_t x1 = static_cast<int32_t>(x) + static_cast<int32_t>(w) - 1;
  int32_t y1 = static_cast<int32_t>(y) + static_cast<int32_t>(h) - 1;

  if (x1 < 0 || y1 < 0 || x0 >= _config.width || y0 >= _config.height) {
    return;
  }

  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 >= _config.width) x1 = _config.width - 1;
  if (y1 >= _config.height) y1 = _config.height - 1;

  const int32_t byteWidth = (static_cast<int32_t>(w) + 7) / 8;

  for (int32_t j = y0; j <= y1; j++) {
    int32_t srcY = j - y;
    const uint8_t* bmpRow = bitmap + static_cast<size_t>(srcY) * static_cast<size_t>(byteWidth);

    int16_t bufY = static_cast<int16_t>(j);
    if (isPageBufferMode()) {
      bufY = static_cast<int16_t>(j - pageBufferYOffset());
      if (bufY < 0 || bufY >= _config.pageBufferPages * 8) continue;
    }
    uint8_t bit = bufferBit(bufY);

    // Hoist the constant page-base offset out of the inner column loop to
    // avoid a division + multiplication per pixel.
    uint8_t page = static_cast<uint8_t>((bufY / 8) % _config.pageBufferPages);
    size_t rowBase = static_cast<size_t>(page) * _config.width;

    for (int32_t i = x0; i <= x1; i++) {
      int32_t srcX = i - x;
      // Get bit from bitmap (MSB first)
      uint8_t bitMask = static_cast<uint8_t>(0x80u >> (srcX & 7));
      if (bmpRow[srcX / 8] & bitMask) {
        size_t idx = rowBase + static_cast<size_t>(i);
        if (on) { _buffer[idx] |= bit; } else { _buffer[idx] &= ~bit; }
      }
    }
  }

  // Mark dirty once for the entire bounding box.
  uint8_t startPage = static_cast<uint8_t>(y0 / 8);
  uint8_t endPage   = static_cast<uint8_t>(y1 / 8);
  for (uint8_t p = startPage; p <= endPage; p++) {
    markDirty(p, static_cast<uint8_t>(x0), static_cast<uint8_t>(x1));
  }
  resetActivityTimer(millis());
  wakeIfSleeping();
}

// ============================================================================
// Text rendering
// ============================================================================

void Ssd1315::drawChar(int16_t x, int16_t y, char c, bool on) {
  if (!_initialized || _buffer == nullptr) return;

  uint8_t ch = static_cast<uint8_t>(c);

  // Map to font index
  if (ch < FONT_FIRST_CHAR || ch >= FONT_FIRST_CHAR + FONT_CHAR_COUNT) {
    // Draw replacement character (filled box)
    fillRect(x, y, FONT_WIDTH, FONT_HEIGHT, on);
    return;
  }

  uint8_t idx = ch - FONT_FIRST_CHAR;
  const uint8_t* glyph = &FONT_5X7[static_cast<size_t>(idx) * FONT_WIDTH];

  // Write glyph columns directly for efficiency; markDirty once per character.
  for (uint8_t col = 0; col < FONT_WIDTH; col++) {
    uint8_t colData = glyph[col];
    int16_t px = x + col;
    if (px < 0 || px >= _config.width) continue;

    for (uint8_t row = 0; row < FONT_HEIGHT; row++) {
      int16_t py = y + row;
      if (py < 0 || py >= _config.height) continue;

      int16_t bufY = py;
      if (isPageBufferMode()) {
        bufY = static_cast<int16_t>(py - pageBufferYOffset());
        if (bufY < 0 || bufY >= _config.pageBufferPages * 8) continue;
      }

      size_t bufIdx = bufferIndex(px, bufY);
      uint8_t bit   = bufferBit(bufY);
      if (colData & static_cast<uint8_t>(1u << row)) {
        if (on) { _buffer[bufIdx] |= bit; } else { _buffer[bufIdx] &= ~bit; }
      }
    }
  }

  // Mark the entire character cell dirty in one call.
  int32_t x0 = x; if (x0 < 0) x0 = 0;
  int32_t x1 = static_cast<int32_t>(x) + FONT_WIDTH - 1;
  if (x1 >= _config.width) x1 = _config.width - 1;
  int32_t y0 = y; if (y0 < 0) y0 = 0;
  int32_t y1 = static_cast<int32_t>(y) + FONT_HEIGHT - 1;
  if (y1 >= _config.height) y1 = _config.height - 1;
  if (x0 <= x1 && y0 <= y1) {
    uint8_t startPage = static_cast<uint8_t>(y0 / 8);
    uint8_t endPage   = static_cast<uint8_t>(y1 / 8);
    for (uint8_t p = startPage; p <= endPage; p++) {
      markDirty(p, static_cast<uint8_t>(x0), static_cast<uint8_t>(x1));
    }
  }
}

int16_t Ssd1315::drawText(int16_t x, int16_t y, const char* str, bool on) {
  if (str == nullptr) return x;
  if (!_initialized || _buffer == nullptr) return x;

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

  resetActivityTimer(millis());
  wakeIfSleeping();
  return cursorX;
}

int16_t Ssd1315::getTextWidth(const char* str) {
  if (str == nullptr) return 0;

  // Use int32_t accumulators to avoid int16_t overflow on very long strings.
  int32_t maxWidth = 0;
  int32_t curWidth = 0;

  while (*str) {
    char c = *str++;
    if (c == '\n') {
      if (curWidth > maxWidth) maxWidth = curWidth;
      curWidth = 0;
    } else if (c != '\r') {
      curWidth += CHAR_WIDTH;
    }
  }

  int32_t result = (curWidth > maxWidth) ? curWidth : maxWidth;
  // Clamp to int16_t range (saturating)
  if (result > 32767) result = 32767;
  return static_cast<int16_t>(result);
}

// ============================================================================
// Test patterns
// ============================================================================

void Ssd1315::fillCheckerboard(uint8_t size) {
  if (!_initialized || _buffer == nullptr || size == 0) return;

  for (int16_t y = 0; y < _config.height; y++) {
    int16_t bufY = y;
    if (isPageBufferMode()) {
      bufY = static_cast<int16_t>(y - pageBufferYOffset());
      if (bufY < 0 || bufY >= _config.pageBufferPages * 8) continue;
    }
    uint8_t bit = bufferBit(bufY);
    for (int16_t x = 0; x < _config.width; x++) {
      bool on = (static_cast<uint16_t>(x / size) + static_cast<uint16_t>(y / size)) & 1u;
      size_t idx = bufferIndex(x, bufY);
      if (on) {
        _buffer[idx] |= bit;
      } else {
        _buffer[idx] &= ~bit;
      }
    }
  }
  markAllDirty();
  resetActivityTimer(millis());
  wakeIfSleeping();
}

void Ssd1315::fillVerticalStripes(uint8_t width) {
  if (!_initialized || _buffer == nullptr || width == 0) return;

  for (int16_t y = 0; y < _config.height; y++) {
    int16_t bufY = y;
    if (isPageBufferMode()) {
      bufY = static_cast<int16_t>(y - pageBufferYOffset());
      if (bufY < 0 || bufY >= _config.pageBufferPages * 8) continue;
    }
    uint8_t bit = bufferBit(bufY);
    for (int16_t x = 0; x < _config.width; x++) {
      bool on = (static_cast<uint16_t>(x / width)) & 1u;
      size_t idx = bufferIndex(x, bufY);
      if (on) {
        _buffer[idx] |= bit;
      } else {
        _buffer[idx] &= ~bit;
      }
    }
  }
  markAllDirty();
  resetActivityTimer(millis());
  wakeIfSleeping();
}

void Ssd1315::fillHorizontalStripes(uint8_t height) {
  if (!_initialized || _buffer == nullptr || height == 0) return;

  for (int16_t y = 0; y < _config.height; y++) {
    int16_t bufY = y;
    if (isPageBufferMode()) {
      bufY = static_cast<int16_t>(y - pageBufferYOffset());
      if (bufY < 0 || bufY >= _config.pageBufferPages * 8) continue;
    }
    uint8_t bit = bufferBit(bufY);
    bool on = (static_cast<uint16_t>(y / height)) & 1u;
    for (int16_t x = 0; x < _config.width; x++) {
      size_t idx = bufferIndex(x, bufY);
      if (on) {
        _buffer[idx] |= bit;
      } else {
        _buffer[idx] &= ~bit;
      }
    }
  }
  markAllDirty();
  resetActivityTimer(millis());
  wakeIfSleeping();
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
