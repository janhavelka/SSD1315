/**
 * @file SSD1315.cpp
 * @brief SSD1315 OLED display driver implementation.
 */

#include "ssd1315/SSD1315.h"

#include <new>       // std::nothrow
#include <string.h>  // memset

namespace SSD1315 {

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

bool byteRangesOverlap(const uint8_t* first, size_t firstSize,
                       const uint8_t* second, size_t secondSize) {
  if (first == nullptr || second == nullptr || firstSize == 0 ||
      secondSize == 0) {
    return false;
  }

  const uintptr_t firstAddress = reinterpret_cast<uintptr_t>(first);
  const uintptr_t secondAddress = reinterpret_cast<uintptr_t>(second);
  if (firstAddress <= secondAddress) {
    return (secondAddress - firstAddress) < firstSize;
  }
  return (firstAddress - secondAddress) < secondSize;
}

bool isValidComPinsConfig(ComPinsConfig value) {
  switch (value) {
    case ComPinsConfig::SEQUENTIAL_NO_REMAP:
    case ComPinsConfig::ALTERNATIVE_NO_REMAP:
    case ComPinsConfig::SEQUENTIAL_REMAP:
    case ComPinsConfig::ALTERNATIVE_REMAP:
      return true;
  }
  return false;
}

bool isSupportedControllerProfile(ControllerProfile value) {
  switch (value) {
    case ControllerProfile::SSD1315:
      return true;
  }
  return false;
}

bool isValidChargePumpVoltage(ChargePumpVoltage value) {
  switch (value) {
    case ChargePumpVoltage::OFF:
    case ChargePumpVoltage::V7_5:
    case ChargePumpVoltage::V8_5:
    case ChargePumpVoltage::V9_0:
      return true;
  }
  return false;
}

bool isValidIrefSelection(IrefSelection value) {
  switch (value) {
    case IrefSelection::IREF_EXTERNAL:
    case IrefSelection::INTERNAL_19UA:
    case IrefSelection::INTERNAL_30UA:
      return true;
  }
  return false;
}

bool isValidVcomhLevel(VcomhLevel value) {
  switch (value) {
    case VcomhLevel::V_065_VCC:
    case VcomhLevel::V_071_VCC:
    case VcomhLevel::V_077_VCC:
    case VcomhLevel::V_083_VCC:
      return true;
  }
  return false;
}

bool isValidSsd1315Address(uint8_t address) {
  return address == 0x3C || address == 0x3D;
}

bool isAlternativeComPinsConfig(ComPinsConfig value) {
  return (static_cast<uint8_t>(value) & 0x10u) != 0;
}

bool isValidScrollSpeed(ScrollSpeed speed) {
  return static_cast<uint8_t>(speed) <=
         static_cast<uint8_t>(ScrollSpeed::FRAMES_2);
}

bool isValidFadeMode(FadeMode mode) {
  switch (mode) {
    case FadeMode::OFF:
    case FadeMode::FADE_OUT:
    case FadeMode::BLINK:
      return true;
  }
  return false;
}

bool isControlStateUncertainError(const Status& st) {
  switch (st.code) {
    case Err::I2C_NACK_ADDR:
    case Err::I2C_NACK_DATA:
    case Err::I2C_TIMEOUT:
    case Err::I2C_BUS_ERROR:
    case Err::TIMEOUT:
    case Err::DEVICE_NOT_FOUND:
    case Err::INTERNAL_ERROR:
      return true;
    default:
      return false;
  }
}

uint32_t saturatedAdd(uint32_t a, uint32_t b) {
  const uint32_t room = UINT32_MAX - a;
  return (b > room) ? UINT32_MAX : (a + b);
}

uint32_t saturatedMul(uint32_t value, uint32_t factor) {
  if (factor != 0 && value > UINT32_MAX / factor) {
    return UINT32_MAX;
  }
  return value * factor;
}

uint32_t waitFlushStallGuardIterations(uint32_t timeoutMs, uint32_t i2cTimeoutMs) {
  static constexpr uint32_t MAX_STALLED_ITERATIONS = 8192;
  uint32_t budget = saturatedAdd(timeoutMs, i2cTimeoutMs);
  budget = saturatedMul(budget, 4u);
  budget = saturatedAdd(budget, 16u);
  if (budget < 32u) return 32u;
  return budget > MAX_STALLED_ITERATIONS ? MAX_STALLED_ITERATIONS : budget;
}

static constexpr size_t FLUSH_DATA_CHUNK_BYTES = 128;
static constexpr size_t COMMAND_LIST_MAX_BYTES = 32;
static constexpr size_t TEXT_MAX_CHARS = 512;
static constexpr uint8_t INIT_STEP_COUNT = 17;
static constexpr uint8_t MAX_POLL_TRANSACTIONS = 8;

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

const char* toString(OperationKind kind) {
  switch (kind) {
    case OperationKind::NONE: return "none";
    case OperationKind::INITIALIZE: return "initialize";
    case OperationKind::FLUSH: return "flush";
    case OperationKind::SLEEP: return "sleep";
    case OperationKind::WAKE: return "wake";
    case OperationKind::RESYNC: return "resync";
    case OperationKind::SHUTDOWN: return "shutdown";
    case OperationKind::HORIZONTAL_SCROLL: return "horizontal-scroll";
    case OperationKind::VERTICAL_SCROLL: return "vertical-scroll";
  }
  return "invalid";
}

const char* toString(OperationPhase phase) {
  switch (phase) {
    case OperationPhase::IDLE: return "idle";
    case OperationPhase::INITIALIZE_COMMAND: return "initialize-command";
    case OperationPhase::SET_COL_ADDR: return "set-column-address";
    case OperationPhase::SET_PAGE_ADDR: return "set-page-address";
    case OperationPhase::SEND_DATA: return "send-data";
    case OperationPhase::DISPLAY_OFF: return "display-off";
    case OperationPhase::DISPLAY_ON: return "display-on";
    case OperationPhase::DISPLAY_ON_DELAY: return "display-on-delay";
    case OperationPhase::CHARGE_PUMP_OFF: return "charge-pump-off";
    case OperationPhase::SCROLL_DEACTIVATE: return "scroll-deactivate";
    case OperationPhase::SCROLL_CONFIGURE: return "scroll-configure";
    case OperationPhase::SCROLL_ACTIVATE: return "scroll-activate";
    case OperationPhase::COMPLETE: return "complete";
  }
  return "invalid";
}

const char* toString(OperationState state) {
  switch (state) {
    case OperationState::IDLE: return "idle";
    case OperationState::ACTIVE: return "active";
    case OperationState::SUCCEEDED: return "succeeded";
    case OperationState::FAILED: return "failed";
    case OperationState::CANCELLED: return "cancelled";
    case OperationState::TIMED_OUT: return "timed-out";
  }
  return "invalid";
}

const char* toString(EffectState effect) {
  switch (effect) {
    case EffectState::NONE: return "none";
    case EffectState::CONFIRMED: return "confirmed";
    case EffectState::PARTIAL: return "partial";
    case EffectState::INDETERMINATE: return "indeterminate";
  }
  return "invalid";
}

const char* toString(PanelPowerState state) {
  switch (state) {
    case PanelPowerState::UNKNOWN: return "unknown";
    case PanelPowerState::OFF: return "off";
    case PanelPowerState::STARTING: return "starting";
    case PanelPowerState::ON: return "on";
  }
  return "invalid";
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

SSD1315::SSD1315() {
  memset(_dirtyMinCol, 0xFF, sizeof(_dirtyMinCol));
  memset(_dirtyMaxCol, 0x00, sizeof(_dirtyMaxCol));
}

SSD1315::~SSD1315() noexcept {
  detach();
}

// ============================================================================
// Health tracking helpers
// ============================================================================

Status SSD1315::_updateHealth(const Status& st) {
  if (!_attached || st.inProgress() || _operationActive()) {
    return st;
  }

  bool isSuccess = st.ok();

  if (isSuccess) {
    _lastOkMs = _nowMs();
    _consecutiveFailures = 0;
    if (_totalSuccess < UINT32_MAX) {
      _totalSuccess++;
    }
  } else {
    _lastError = st;
    _lastErrorMs = _nowMs();
    // Saturate at 255 to prevent uint8_t wrap-around. Without saturation,
    // 256 consecutive failures would wrap the counter back to 0, causing the
    // READY→DEGRADED transition (which triggers at _consecutiveFailures == 1)
    // to fire again on the 257th failure instead of staying OFFLINE.
    // Successes still reset the counter to 0 normally — recovery is unaffected.
    if (_consecutiveFailures < 255u) {
      _consecutiveFailures++;
    }
    if (_totalFailures < UINT32_MAX) {
      _totalFailures++;
    }
  }

  // Health transitions are meaningful only after initialization completed.
  if (_initialized) {
    if (isSuccess) {
      // Success returns the driver to READY when I2C is explicitly allowed.
      _driverState = DriverState::READY;
    } else {
      // Failure handling: READY -> DEGRADED on first failure.
      if (_consecutiveFailures == 1 &&
          _driverState == DriverState::READY) {
        _driverState = DriverState::DEGRADED;
      }
      // DEGRADED -> OFFLINE when threshold reached.
      if (_consecutiveFailures >= _config.offlineThreshold) {
        _driverState = DriverState::OFFLINE;
      }
    }
  }
  return st;
}

Status SSD1315::getSettings(SettingsSnapshot& out) const {
  out.attached = _attached;
  out.initialized = _initialized;
  out.state = _driverState;
  out.controllerProfile = _config.controllerProfile;
  out.i2cAddress = _config.i2cAddress;
  out.i2cTimeoutMs = _config.i2cTimeoutMs;
  out.offlineThreshold = _config.offlineThreshold;
  out.hasNowMsHook = (_config.nowMs != nullptr);
  out.hasCooperativeYieldHook = (_config.cooperativeYield != nullptr);
  out.maxWriteBytes = _config.maxWriteBytes;

  out.width = _config.width;
  out.height = _config.height;
  out.pageBufferPages = _config.pageBufferPages;
  out.totalPages = _totalPages;
  out.pageBufferMode = isPageBufferMode();
  out.sleeping = _sleeping;
  out.allPixelsOn = _allPixelsOn;
  out.panelPowerState = _panelPowerState;
  out.userPageCount = _userPageCount;
  out.activeUserPage = _activeUserPage;
  out.currentPageIndex = _currentBufferPage;
  out.pageIterationActive = _inPageIteration;
  out.byteBudgetPerTick = _config.byteBudgetPerTick;
  out.flushTimeoutMs = _config.flushTimeoutMs;
  out.displayOnDelayMs = _config.displayOnDelayMs;
  out.inactivitySleepMs = _config.inactivitySleepMs;
  out.pageCycleMs = _config.pageCycleMs;
  out.flipX = _config.flipX;
  out.flipY = _config.flipY;
  out.invert = _config.invert;
  out.contrast = _config.contrast;
  out.comPins = static_cast<uint8_t>(_config.comPins);
  out.chargePumpVoltage = static_cast<uint8_t>(_config.chargePumpVoltage);
  out.iref = static_cast<uint8_t>(_config.iref);
  out.vcomh = static_cast<uint8_t>(_config.vcomh);
  out.clockDivide = _config.clockDivide;
  out.oscFrequency = _config.oscFrequency;
  out.prechargePhase1 = _config.prechargePhase1;
  out.prechargePhase2 = _config.prechargePhase2;
  out.scrollActive = _scrollActive;
  out.clearOnBegin = _config.clearOnBegin;
  out.clearOnRecover = _config.clearOnRecover;
  out.hasExternalBuffer = (_config.externalBuffer != nullptr);
  out.ownsBuffer = _ownsBuffer;
  out.bufferSize = getBufferSize();
  out.dirtyPages = _dirtyPages;
  out.gddramSynchronized = _gddramSynchronized && _dirtyPages == 0;
  out.flushing = isFlushing();
  out.controlStateDirty = _controlStateDirty;
  out.controlStateError = _controlStateError;
  out.lastOkMs = _lastOkMs;
  out.lastErrorMs = _lastErrorMs;
  out.consecutiveFailures = _consecutiveFailures;
  out.totalFailures = _totalFailures;
  out.totalSuccess = _totalSuccess;
  out.lastError = _lastError;
  return Ok();
}

uint32_t SSD1315::_nowMs() const {
  if (_config.nowMs != nullptr) {
    return _config.nowMs(_config.timeUser);
  }
  return 0U;
}

void SSD1315::_cooperativeYield() const {
  if (_config.cooperativeYield != nullptr) {
    _config.cooperativeYield(_config.timeUser);
  }
}

Status SSD1315::_i2cWriteRaw(const uint8_t* data, size_t len) {
  if (!_config.i2cWrite) {
    return Error(Err::INVALID_CONFIG, "I2C write callback null");
  }
  if (data == nullptr || len == 0) {
    return Error(Err::INVALID_CONFIG, "I2C write buffer invalid");
  }
  if (len > _config.maxWriteBytes) {
    return Error(Err::BUFFER_OVERFLOW, "I2C write exceeds configured capacity");
  }
  const TransportResult result = _config.i2cWrite(
      _config.i2cAddress, data, len, _transportTimeoutMs, _config.i2cUser);
  return _mapTransportResult(result);
}

Status SSD1315::_checkDirectI2cAdmission() const {
  if (_operationActive() || _operationResultReady || isFlushing()) {
    return Error(Err::BUSY,
                 "cooperative operation active or result pending, or flush active");
  }
  return Ok();
}

Status SSD1315::_i2cWriteTracked(const uint8_t* data, size_t len) {
  Status st = _i2cWriteRaw(data, len);
  return _updateHealth(st);
}

Status SSD1315::_mapTransportResult(const TransportResult& result) {
  switch (result.code) {
    case TransportCode::OK:
      return Ok();
    case TransportCode::NACK_ADDRESS:
      return Error(Err::I2C_NACK_ADDR, result.detail, toString(result.code));
    case TransportCode::NACK_DATA:
      return Error(Err::I2C_NACK_DATA, result.detail, toString(result.code));
    case TransportCode::TIMEOUT:
      return Error(Err::I2C_TIMEOUT, result.detail, toString(result.code));
    case TransportCode::BUS_ERROR:
      return Error(Err::I2C_BUS_ERROR, result.detail, toString(result.code));
  }
  return Error(Err::I2C_BUS_ERROR, result.detail, "invalid transport code");
}

void SSD1315::_resetRuntimeState() {
  if (_ownsBuffer && _buffer != nullptr) {
    delete[] _buffer;
  }

  _config = Config{};
  _attached = false;
  _initialized = false;
  _sleeping = true;
  _allPixelsOn = false;
  _scrollActive = false;

  _buffer = nullptr;
  _ownsBuffer = false;
  _totalPages = 0;

  _dirtyPages = 0;
  memset(_dirtyMinCol, 0xFF, sizeof(_dirtyMinCol));
  memset(_dirtyMaxCol, 0x00, sizeof(_dirtyMaxCol));
  memset(_dirtyGeneration, 0x00, sizeof(_dirtyGeneration));
  _gddramSynchronized = false;

  _flushState = FlushState::IDLE;
  _flushPage = 0;
  _flushCol = 0;
  _flushEndPage = 0;
  _flushMinCol = 0;
  _flushMaxCol = 0;
  _flushPageGeneration = 0;
  _flushStartMs = 0;
  _flushStarted = false;
  _flushAccounted = false;
  _lastError = Ok();
  _flushBytesCompleted = 0;
  _flushDataChunkCount = 0;
  _flushTransactionCount = 0;

  _operationOptions = OperationOptions{};
  _operation = OperationProgress{};
  _operationResultReady = false;
  _lastOperationRequestId = 0;
  _initStep = 0;
  _operationDelayStartMs = 0;
  _operationDelayStarted = false;
  _operationScrollLeft = false;
  _operationScrollStartPage = 0;
  _operationScrollEndPage = 0;
  _operationScrollSpeed = ScrollSpeed::FRAMES_5;
  _operationScrollVerticalOffset = 0;
  _compatRequestId = 0x80000000u;
  _panelPowerState = PanelPowerState::UNKNOWN;
  _transportTimeoutMs = _config.i2cTimeoutMs;

  _powerState = PowerState::OFF;
  _powerOnMs = 0;
  _powerOnDelayStarted = false;
  _verticalScrollRows = MAX_HEIGHT;

  _userPageCount = 1;
  _activeUserPage = 0;

  _currentBufferPage = 0;
  _inPageIteration = false;

  _driverState = DriverState::UNINIT;
  _lastOkMs = 0;
  _lastErrorMs = 0;
  _consecutiveFailures = 0;
  _totalFailures = 0;
  _totalSuccess = 0;
  _flushError = Ok();
  _controlStateDirty = false;
  _controlStateError = Ok();
}

void SSD1315::_markControlStateDirty(const Status& st) {
  if (st.ok() || st.inProgress() || !isControlStateUncertainError(st)) {
    return;
  }
  _controlStateDirty = true;
  _controlStateError = st;
}

void SSD1315::_markRawCommandFailure(const Status& st) {
  if (st.code == Err::I2C_NACK_ADDR) {
    return;  // Definite address absence means the raw command had no effect.
  }
  _markControlStateDirty(st);
  if (_effectForFailure(st, false) == EffectState::INDETERMINATE) {
    _panelPowerState = PanelPowerState::UNKNOWN;
    _gddramSynchronized = false;
  }
}

void SSD1315::_clearControlStateDirty() {
  _controlStateDirty = false;
  _controlStateError = Ok();
}

Status SSD1315::_validateConfig(const Config& config,
                                size_t& requiredBufferSize) const {
  requiredBufferSize = 0;
  if (config.i2cWrite == nullptr) {
    return Error(Err::INVALID_CONFIG, "i2cWrite callback is null");
  }
  if (!isSupportedControllerProfile(config.controllerProfile)) {
    return Error(Err::INVALID_CONFIG, "unsupported controller profile");
  }
  if (config.width == 0 || config.width > MAX_WIDTH) {
    return Error(Err::INVALID_DIMENSIONS, "width out of range [1..128]");
  }
  if (config.height < 16 || config.height > MAX_HEIGHT ||
      (config.height % 8) != 0) {
    return Error(Err::INVALID_DIMENSIONS,
                 "height must be 16..64, multiple of 8");
  }
  if (!isValidSsd1315Address(config.i2cAddress)) {
    return Error(Err::INVALID_CONFIG,
                 "i2cAddress must be SSD1315 7-bit 0x3C or 0x3D");
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
  if (config.contrast < cmd::CONTRAST_MIN) {
    return Error(Err::INVALID_CONFIG, "contrast must be 1..255");
  }
  if (config.displayOffset > 63 || config.startLine > 63) {
    return Error(Err::INVALID_CONFIG,
                 "displayOffset/startLine must be 0..63");
  }
  if (config.startLine >= config.height) {
    return Error(Err::INVALID_CONFIG,
                 "startLine must be less than panel height");
  }
  if (config.i2cTimeoutMs == 0) {
    return Error(Err::INVALID_CONFIG, "i2cTimeoutMs must be > 0");
  }
  if (config.byteBudgetPerTick == 0) {
    return Error(Err::INVALID_CONFIG, "byteBudgetPerTick must be > 0");
  }
  if (config.maxWriteBytes < 4 ||
      config.maxWriteBytes > (MAX_WIDTH + 1u)) {
    return Error(Err::INVALID_CONFIG, "maxWriteBytes must be 4..129");
  }
  if (!isValidComPinsConfig(config.comPins) ||
      !isValidChargePumpVoltage(config.chargePumpVoltage) ||
      !isValidIrefSelection(config.iref) ||
      !isValidVcomhLevel(config.vcomh)) {
    return Error(Err::INVALID_CONFIG, "invalid panel configuration enum");
  }

  const uint8_t totalPageCount = static_cast<uint8_t>(config.height / 8u);
  if (config.pageBufferPages == 0 ||
      config.pageBufferPages > totalPageCount) {
    return Error(Err::INVALID_PAGE_COUNT, "pageBufferPages out of range");
  }
  requiredBufferSize = requiredFramebufferBytes(config.width,
                                                 config.pageBufferPages);
  if (config.externalBuffer != nullptr &&
      config.externalBufferSizeBytes < requiredBufferSize) {
    return Error(Err::BUFFER_TOO_SMALL, "external buffer too small");
  }
  if (config.externalBuffer == nullptr &&
      config.externalBufferSizeBytes != 0) {
    return Error(Err::INVALID_CONFIG,
                 "external buffer size set without external buffer");
  }
  return Ok();
}

Status SSD1315::validateConfig(const Config& config) const {
  size_t requiredBufferSize = 0;
  Status st = _validateConfig(config, requiredBufferSize);
  if (!st.ok()) {
    return st;
  }
  if (_ownsBuffer &&
      byteRangesOverlap(config.externalBuffer, requiredBufferSize, _buffer,
                        getBufferSize())) {
    return Error(Err::INVALID_CONFIG,
                 "externalBuffer aliases driver-owned storage");
  }
  return Ok();
}

Status SSD1315::attach(const Config& config) {
  if (_operationActive() || _operationResultReady || isFlushing()) {
    return Error(Err::BUSY,
                 "operation active or result pending, or flush active");
  }
  Config candidate = config;
  size_t requiredBufferSize = 0;
  Status st = _validateConfig(candidate, requiredBufferSize);
  if (!st.ok()) {
    return st;
  }
  if (_ownsBuffer &&
      byteRangesOverlap(candidate.externalBuffer, requiredBufferSize, _buffer,
                        getBufferSize())) {
    return Error(Err::INVALID_CONFIG,
                 "externalBuffer aliases driver-owned storage");
  }

  uint8_t* candidateBuffer = candidate.externalBuffer;
  bool candidateOwnsBuffer = false;
  if (candidateBuffer == nullptr) {
    candidateBuffer = new (std::nothrow) uint8_t[requiredBufferSize];
    if (candidateBuffer == nullptr) {
      return Error(Err::INTERNAL_ERROR, "buffer allocation failed");
    }
    candidateOwnsBuffer = true;
  }
  memset(candidateBuffer, 0, requiredBufferSize);

  _resetRuntimeState();
  _config = candidate;
  if (_config.offlineThreshold == 0) {
    _config.offlineThreshold = 1;
  }
  _buffer = candidateBuffer;
  _ownsBuffer = candidateOwnsBuffer;
  _totalPages = static_cast<uint8_t>(_config.height / 8u);
  _attached = true;
  _initialized = false;
  _sleeping = true;
  _panelPowerState = PanelPowerState::UNKNOWN;
  _powerState = PowerState::OFF;
  _transportTimeoutMs = _config.i2cTimeoutMs;
  markAllDirty();
  return Ok();
}

void SSD1315::detach() noexcept {
  _resetRuntimeState();
}

bool SSD1315::_deadlineReached(uint32_t nowMs, uint32_t deadlineMs) {
  return static_cast<int32_t>(nowMs - deadlineMs) >= 0;
}

EffectState SSD1315::_effectForFailure(const Status& status,
                                       bool hasConfirmedPrefix) {
  if (status.code == Err::I2C_NACK_ADDR) {
    return hasConfirmedPrefix ? EffectState::PARTIAL : EffectState::NONE;
  }
  if (status.code == Err::I2C_NACK_DATA ||
      status.code == Err::I2C_TIMEOUT ||
      status.code == Err::I2C_BUS_ERROR) {
    return EffectState::INDETERMINATE;
  }
  return hasConfirmedPrefix ? EffectState::PARTIAL : EffectState::NONE;
}

Status SSD1315::_startOperation(OperationKind kind,
                                const OperationOptions& options) {
  if (!_attached) {
    return Error(Err::NOT_INITIALIZED, "driver not attached");
  }
  if (kind == OperationKind::RESYNC && isPageBufferMode()) {
    return Error(Err::UNSUPPORTED,
                 "resync requires a full framebuffer");
  }
  if (_operationActive() || _operationResultReady) {
    return Error(Err::BUSY, "operation active or result not consumed");
  }
  if (isFlushing()) {
    return Error(Err::BUSY, "legacy flush active");
  }
  if (options.requestId == 0 ||
      options.requestId == _lastOperationRequestId) {
    return Error(Err::INVALID_CONFIG,
                 "requestId must be nonzero and differ from prior request");
  }
  if (kind != OperationKind::INITIALIZE && kind != OperationKind::RESYNC &&
      kind != OperationKind::SHUTDOWN && !_initialized) {
    return Error(Err::NOT_INITIALIZED, "controller not initialized");
  }
  if ((kind == OperationKind::FLUSH || kind == OperationKind::WAKE ||
       kind == OperationKind::HORIZONTAL_SCROLL ||
       kind == OperationKind::VERTICAL_SCROLL) &&
      _controlStateDirty) {
    return Error(Err::CONTROL_STATE_UNKNOWN,
                 "panel state unknown; resync required");
  }
  if (kind == OperationKind::WAKE &&
      _panelPowerState == PanelPowerState::UNKNOWN) {
    return Error(Err::CONTROL_STATE_UNKNOWN,
                 "panel power unknown; resync required");
  }
  if (kind == OperationKind::WAKE &&
      (isDirty() || !_gddramSynchronized)) {
    return Error(Err::STATE_ERROR,
                 "wake requires synchronized clean GDDRAM");
  }
  if (kind == OperationKind::FLUSH &&
      _panelPowerState != PanelPowerState::ON &&
      _panelPowerState != PanelPowerState::OFF) {
    return Error(Err::PANEL_NOT_READY,
                 "flush requires command-confirmed modeled power");
  }
  if (kind == OperationKind::WAKE &&
      _panelPowerState != PanelPowerState::OFF) {
    return Error(Err::STATE_ERROR, "wake requires modeled panel-off state");
  }

  const uint32_t priorRequestId = _lastOperationRequestId;
  _operationOptions = options;
  _operation = OperationProgress{};
  _operation.requestId = options.requestId;
  _operation.kind = kind;
  _operation.state = OperationState::ACTIVE;
  _operation.effect = EffectState::NONE;
  _operation.power = _panelPowerState;
  _operation.status = Error(Err::IN_PROGRESS, "operation in progress");
  _operationResultReady = false;
  _lastOperationRequestId = options.requestId;
  _operationDelayStarted = false;
  _initStep = 0;

  switch (kind) {
    case OperationKind::INITIALIZE:
    case OperationKind::RESYNC:
      _operation.phase = OperationPhase::INITIALIZE_COMMAND;
      break;
    case OperationKind::FLUSH: {
      Status st = _requestFlushInternal();
      if (!st.ok()) {
        _operation = OperationProgress{};
        _lastOperationRequestId = priorRequestId;
        return st;
      }
      _syncOperationFromFlush();
      break;
    }
    case OperationKind::SLEEP:
    case OperationKind::SHUTDOWN:
      _operation.phase = OperationPhase::DISPLAY_OFF;
      break;
    case OperationKind::HORIZONTAL_SCROLL:
    case OperationKind::VERTICAL_SCROLL:
      _operation.phase = OperationPhase::SCROLL_DEACTIVATE;
      break;
    case OperationKind::WAKE:
      _operation.phase = OperationPhase::DISPLAY_ON;
      break;
    case OperationKind::NONE:
    default:
      _operation = OperationProgress{};
      _lastOperationRequestId = priorRequestId;
      return Error(Err::INVALID_CONFIG, "invalid operation kind");
  }
  return Ok();
}

Status SSD1315::startInitialize(const OperationOptions& options) {
  return _startOperation(OperationKind::INITIALIZE, options);
}

Status SSD1315::startFlush(const OperationOptions& options) {
  return _startOperation(OperationKind::FLUSH, options);
}

Status SSD1315::startSleep(const OperationOptions& options) {
  return _startOperation(OperationKind::SLEEP, options);
}

Status SSD1315::startWake(const OperationOptions& options) {
  return _startOperation(OperationKind::WAKE, options);
}

Status SSD1315::startResync(const OperationOptions& options) {
  return _startOperation(OperationKind::RESYNC, options);
}

Status SSD1315::startShutdown(const OperationOptions& options) {
  return _startOperation(OperationKind::SHUTDOWN, options);
}

Status SSD1315::startHorizontalScrollOperation(
    const OperationOptions& options, bool left, uint8_t startPage,
    uint8_t endPage, ScrollSpeed speed) {
  if (_operationActive() || _operationResultReady || isFlushing()) {
    return Error(Err::BUSY,
                 "operation active or result pending, or flush active");
  }
  if (!_initialized) return Error(Err::NOT_INITIALIZED, "not initialized");
  Status st = _validateHorizontalScroll(left, startPage, endPage, speed);
  if (!st.ok()) return st;
  st = _startOperation(OperationKind::HORIZONTAL_SCROLL, options);
  if (!st.ok()) return st;
  _operationScrollLeft = left;
  _operationScrollStartPage = startPage;
  _operationScrollEndPage = endPage;
  _operationScrollSpeed = speed;
  _operationScrollVerticalOffset = 0;
  return Ok();
}

Status SSD1315::startVerticalScrollOperation(
    const OperationOptions& options, bool left, uint8_t startPage,
    uint8_t endPage, ScrollSpeed speed, uint8_t verticalOffset) {
  if (_operationActive() || _operationResultReady || isFlushing()) {
    return Error(Err::BUSY,
                 "operation active or result pending, or flush active");
  }
  if (!_initialized) return Error(Err::NOT_INITIALIZED, "not initialized");
  Status st = _validateVerticalScroll(left, startPage, endPage, speed,
                                      verticalOffset);
  if (!st.ok()) return st;
  st = _startOperation(OperationKind::VERTICAL_SCROLL, options);
  if (!st.ok()) return st;
  _operationScrollLeft = left;
  _operationScrollStartPage = startPage;
  _operationScrollEndPage = endPage;
  _operationScrollSpeed = speed;
  _operationScrollVerticalOffset = verticalOffset;
  return Ok();
}

Status SSD1315::_sendInitStep(uint8_t step) {
  const uint8_t clockDiv = static_cast<uint8_t>(
      ((_config.oscFrequency & 0x0Fu) << 4) |
      ((_config.clockDivide - 1u) & 0x0Fu));
  const uint8_t precharge = static_cast<uint8_t>(
      ((_config.prechargePhase2 & 0x0Fu) << 4) |
      (_config.prechargePhase1 & 0x0Fu));
  switch (step) {
    case 0: return _sendCommand(cmd::DISPLAY_OFF);
    case 1: return _sendCommand2(cmd::SET_MEMORY_MODE, cmd::ADDR_MODE_HORIZONTAL);
    case 2: return _sendCommand(
        static_cast<uint8_t>(cmd::SET_START_LINE | (_config.startLine & 0x3Fu)));
    case 3: return _sendCommand(_config.flipX ? cmd::SEG_REMAP_ON : cmd::SEG_REMAP_OFF);
    case 4: return _sendCommand2(cmd::SET_MULTIPLEX,
                                 static_cast<uint8_t>(_config.height - 1u));
    case 5: return _sendCommand(_config.flipY ? cmd::COM_SCAN_DEC : cmd::COM_SCAN_INC);
    case 6: return _sendCommand2(cmd::SET_DISPLAY_OFFSET, _config.displayOffset);
    case 7: return _sendCommand2(cmd::SET_COM_PINS,
                                 static_cast<uint8_t>(_config.comPins));
    case 8: return _sendCommand2(cmd::SET_CLOCK_DIV, clockDiv);
    case 9: return _sendCommand2(cmd::SET_PRECHARGE, precharge);
    case 10: return _sendCommand2(cmd::SET_VCOMH,
                                  static_cast<uint8_t>(_config.vcomh));
    case 11: return _sendCommand2(cmd::SET_CONTRAST, _config.contrast);
    case 12: return _sendCommand2(cmd::SET_IREF,
                                  static_cast<uint8_t>(_config.iref));
    case 13: {
      const uint8_t commands[3] = {
          cmd::SET_CHARGE_PUMP,
          static_cast<uint8_t>(_config.chargePumpVoltage),
          cmd::SCROLL_DEACTIVATE};
      return _sendCommandList(commands, sizeof(commands));
    }
    case 14: {
      const uint8_t commands[3] = {
          cmd::DISPLAY_RAM, cmd::SET_FADE_BLINK,
          static_cast<uint8_t>(FadeMode::OFF)};
      return _sendCommandList(commands, sizeof(commands));
    }
    case 15: {
      const uint8_t commands[3] = {
          _config.invert ? cmd::INVERT_DISPLAY : cmd::NORMAL_DISPLAY,
          cmd::SET_ZOOM, 0x00};
      return _sendCommandList(commands, sizeof(commands));
    }
    case 16: return _sendCommand3(cmd::SET_VERT_SCROLL_AREA, 0,
                                  _config.height);
    default: return Error(Err::INTERNAL_ERROR, "invalid initialize step");
  }
}

Status SSD1315::_pollInitializePhase() {
  Status st = _sendInitStep(_initStep);
  _operation.transactionCount++;
  if (!st.ok()) {
    _panelPowerState = PanelPowerState::UNKNOWN;
    _operation.power = _panelPowerState;
    _markControlStateDirty(st);
    return _failOperation(st, _effectForFailure(
                                  st, _operation.transactionCount > 1));
  }
  _operation.effect = EffectState::PARTIAL;
  ++_initStep;
  if (_initStep < INIT_STEP_COUNT) {
    return Error(Err::IN_PROGRESS, "initialize in progress");
  }

  _initialized = true;
  _sleeping = true;
  _allPixelsOn = false;
  _scrollActive = false;
  _verticalScrollRows = _config.height;
  _panelPowerState = PanelPowerState::OFF;
  _powerState = PowerState::OFF;
  _operation.power = _panelPowerState;
  _clearControlStateDirty();
  _gddramSynchronized = false;
  markAllDirty();
  return Ok();
}

void SSD1315::_syncOperationFromFlush() {
  const FlushStatus flush = getFlushStatus();
  _operation.currentPage = flush.currentPage;
  _operation.currentColumn = flush.currentColumn;
  _operation.bytesCompleted = _flushBytesCompleted;
  _operation.dataChunkCount = _flushDataChunkCount;
  if (_flushTransactionCount != 0 && _operationActive()) {
    _operation.effect = EffectState::PARTIAL;
  }
  switch (flush.phase) {
    case FlushPhase::SET_COL_ADDR:
      _operation.phase = OperationPhase::SET_COL_ADDR;
      break;
    case FlushPhase::SET_PAGE_ADDR:
      _operation.phase = OperationPhase::SET_PAGE_ADDR;
      break;
    case FlushPhase::SEND_DATA:
      _operation.phase = OperationPhase::SEND_DATA;
      break;
    default:
      break;
  }
}

Status SSD1315::_pollFlushPhase(uint32_t nowMs, uint8_t maxTransactions,
                                uint16_t byteBudget) {
  Status st = _pollFlushInternal(nowMs, maxTransactions, byteBudget);
  _syncOperationFromFlush();
  return st;
}

void SSD1315::_finishOperation() {
  _operation.phase = OperationPhase::COMPLETE;
  _operation.state = OperationState::SUCCEEDED;
  _operation.effect = EffectState::CONFIRMED;
  _operation.power = _panelPowerState;
  _operation.status = Ok();
  _operationResultReady = true;
  _transportTimeoutMs = _config.i2cTimeoutMs;
  if (_operation.transactionCount != 0) {
    _updateHealth(Ok());
  }
  if (_operation.kind == OperationKind::FLUSH ||
      _operation.kind == OperationKind::RESYNC) {
    _flushAccounted = true;
  }
}

Status SSD1315::_failOperation(const Status& status, EffectState effect,
                               OperationState state, bool publishHealth) {
  const bool hasFlushJob =
      _flushState == FlushState::SET_COL_ADDR ||
      _flushState == FlushState::SET_PAGE_ADDR ||
      _flushState == FlushState::SEND_DATA ||
      _flushState == FlushState::DONE ||
      _flushState == FlushState::ERROR;
  if ((_operation.kind == OperationKind::FLUSH ||
       _operation.kind == OperationKind::RESYNC) && hasFlushJob) {
    _flushError = status;
    _flushState = FlushState::ERROR;
    _flushStarted = false;
    _flushAccounted = true;
  }
  _operation.phase = OperationPhase::COMPLETE;
  _operation.state = state;
  _operation.effect = effect;
  _operation.power = _panelPowerState;
  _operation.status = status;
  _operationResultReady = true;
  _transportTimeoutMs = _config.i2cTimeoutMs;
  if (state != OperationState::CANCELLED) {
    _lastError = status;
    if (publishHealth && _operation.transactionCount != 0) {
      _updateHealth(status);
    }
  }
  return status;
}

Status SSD1315::_terminateOperation(const Status& status,
                                    OperationState state) {
  const bool controlSequence =
      _operation.kind == OperationKind::INITIALIZE ||
      _operation.kind == OperationKind::RESYNC ||
      _operation.kind == OperationKind::WAKE ||
      _operation.kind == OperationKind::SHUTDOWN ||
      _operation.kind == OperationKind::HORIZONTAL_SCROLL ||
      _operation.kind == OperationKind::VERTICAL_SCROLL;
  const bool powerBecomesUnknown =
      _operation.kind == OperationKind::INITIALIZE ||
      _operation.kind == OperationKind::RESYNC ||
      _operation.kind == OperationKind::WAKE;
  if (controlSequence && _operation.transactionCount != 0) {
    _controlStateDirty = true;
    _controlStateError = status;
    if (powerBecomesUnknown) {
      _panelPowerState = PanelPowerState::UNKNOWN;
    }
    if (_operation.kind == OperationKind::INITIALIZE ||
        _operation.kind == OperationKind::RESYNC) {
      _gddramSynchronized = false;
    }
  }
  return _failOperation(
      status,
      _operation.transactionCount == 0 ? EffectState::NONE
                                       : EffectState::PARTIAL,
      state);
}

Status SSD1315::pollOperation(uint32_t nowMs, uint8_t maxTransactions,
                              uint16_t byteBudget) {
  if (!_operationActive()) {
    return _operationResultReady ? _operation.status : Ok();
  }
  if (maxTransactions > MAX_POLL_TRANSACTIONS) {
    return Error(Err::INVALID_CONFIG, "maxTransactions must be 0..8");
  }
  if (_operationOptions.useDeadline && maxTransactions > 1u) {
    maxTransactions = 1u;
  }
  if (_operationOptions.useDeadline &&
      _deadlineReached(nowMs, _operationOptions.deadlineMs)) {
    return _terminateOperation(
        Error(Err::TIMEOUT, "operation deadline expired"),
        OperationState::TIMED_OUT);
  }
  _transportTimeoutMs = _config.i2cTimeoutMs;
  if (_operationOptions.useDeadline) {
    const uint32_t remainingMs = _operationOptions.deadlineMs - nowMs;
    if (remainingMs < _transportTimeoutMs) {
      _transportTimeoutMs = remainingMs;
    }
  }
  if (byteBudget == 0) {
    byteBudget = _config.byteBudgetPerTick;
  }

  uint8_t transactionsLeft = maxTransactions;
  while (_operationActive()) {
    switch (_operation.phase) {
      case OperationPhase::INITIALIZE_COMMAND: {
        if (transactionsLeft == 0) {
          return Error(Err::IN_PROGRESS, "initialize waiting for budget");
        }
        Status st = _pollInitializePhase();
        --transactionsLeft;
        if (!st.ok() && !st.inProgress()) {
          return st;
        }
        if (_initStep < INIT_STEP_COUNT) {
          if (transactionsLeft == 0) {
            return Error(Err::IN_PROGRESS, "initialize in progress");
          }
          continue;
        }
        if (_operation.kind == OperationKind::INITIALIZE) {
          _finishOperation();
          return Ok();
        }
        Status flushStart = _requestFlushInternal();
        if (!flushStart.ok()) {
          return _failOperation(flushStart, EffectState::PARTIAL);
        }
        _syncOperationFromFlush();
        if (transactionsLeft == 0) {
          return Error(Err::IN_PROGRESS, "resync flush pending");
        }
        continue;
      }

      case OperationPhase::SET_COL_ADDR:
      case OperationPhase::SET_PAGE_ADDR:
      case OperationPhase::SEND_DATA: {
        if (transactionsLeft == 0) {
          return Error(Err::IN_PROGRESS, "flush waiting for budget");
        }
        const uint16_t before = _flushTransactionCount;
        Status st = _pollFlushPhase(nowMs, transactionsLeft, byteBudget);
        const uint16_t used = static_cast<uint16_t>(_flushTransactionCount - before);
        _operation.transactionCount = static_cast<uint16_t>(
            _operation.transactionCount + used);
        transactionsLeft = used >= transactionsLeft
                               ? 0
                               : static_cast<uint8_t>(transactionsLeft - used);
        if (st.inProgress()) {
          return st;
        }
        if (!st.ok()) {
          const bool hasConfirmedPrefix =
              used == 0 ? _operation.transactionCount != 0
                        : _operation.transactionCount > 1;
          return _failOperation(
              st, _effectForFailure(st, hasConfirmedPrefix));
        }
        if (_operation.kind == OperationKind::FLUSH) {
          _finishOperation();
          return Ok();
        }
        _operation.phase = OperationPhase::DISPLAY_ON;
        if (transactionsLeft == 0) {
          return Error(Err::IN_PROGRESS, "display-on pending");
        }
        continue;
      }

      case OperationPhase::DISPLAY_OFF: {
        if (transactionsLeft == 0) {
          return Error(Err::IN_PROGRESS, "display-off waiting for budget");
        }
        Status st = _sendCommand(cmd::DISPLAY_OFF);
        _operation.transactionCount++;
        --transactionsLeft;
        if (!st.ok()) {
          _panelPowerState = PanelPowerState::UNKNOWN;
          _markControlStateDirty(st);
          return _failOperation(st, _effectForFailure(
                                        st, _operation.transactionCount > 1));
        }
        _panelPowerState = PanelPowerState::OFF;
        _powerState = PowerState::OFF;
        _operation.power = _panelPowerState;
        _sleeping = true;
        _operation.effect = EffectState::PARTIAL;
        if (_operation.kind == OperationKind::SHUTDOWN &&
            _config.chargePumpVoltage != ChargePumpVoltage::OFF) {
          _operation.phase = OperationPhase::CHARGE_PUMP_OFF;
          if (transactionsLeft == 0) {
            return Error(Err::IN_PROGRESS, "charge-pump shutdown pending");
          }
          continue;
        }
        if (_operation.kind == OperationKind::SHUTDOWN) {
          _initialized = false;
          _driverState = DriverState::UNINIT;
        }
        _finishOperation();
        return Ok();
      }

      case OperationPhase::DISPLAY_ON: {
        if (isDirty() || !_gddramSynchronized) {
          return _failOperation(
              Error(Err::STATE_ERROR,
                    "display-on requires synchronized clean GDDRAM"),
              _operation.transactionCount == 0 ? EffectState::NONE
                                               : EffectState::PARTIAL,
              OperationState::FAILED, false);
        }
        if (transactionsLeft == 0) {
          return Error(Err::IN_PROGRESS, "display-on waiting for budget");
        }
        Status st = _sendCommand(cmd::DISPLAY_ON);
        _operation.transactionCount++;
        if (!st.ok()) {
          _panelPowerState = PanelPowerState::UNKNOWN;
          _markControlStateDirty(st);
          return _failOperation(st, _effectForFailure(
                                        st, _operation.transactionCount > 1));
        }
        _sleeping = false;
        _panelPowerState = PanelPowerState::STARTING;
        _operation.power = _panelPowerState;
        _operation.effect = EffectState::PARTIAL;
        _operation.phase = OperationPhase::DISPLAY_ON_DELAY;
        _operationDelayStartMs = nowMs;
        _operationDelayStarted = true;
        if (_config.displayOnDelayMs == 0) {
          _panelPowerState = PanelPowerState::ON;
          _powerState = PowerState::READY;
          _finishOperation();
          return Ok();
        }
        return Error(Err::IN_PROGRESS, "display-on delay");
      }

      case OperationPhase::DISPLAY_ON_DELAY:
        if (_operationDelayStarted &&
            (nowMs - _operationDelayStartMs) >= _config.displayOnDelayMs) {
          _panelPowerState = PanelPowerState::ON;
          _powerState = PowerState::READY;
          _finishOperation();
          return Ok();
        }
        return Error(Err::IN_PROGRESS, "display-on delay");

      case OperationPhase::CHARGE_PUMP_OFF: {
        if (transactionsLeft == 0) {
          return Error(Err::IN_PROGRESS, "charge-pump shutdown waiting for budget");
        }
        Status st = _sendCommand2(cmd::SET_CHARGE_PUMP,
                                  static_cast<uint8_t>(ChargePumpVoltage::OFF));
        _operation.transactionCount++;
        if (!st.ok()) {
          _markControlStateDirty(st);
          return _failOperation(st, _effectForFailure(st, true));
        }
        _initialized = false;
        _driverState = DriverState::UNINIT;
        _panelPowerState = PanelPowerState::OFF;
        _finishOperation();
        return Ok();
      }

      case OperationPhase::SCROLL_DEACTIVATE: {
        if (transactionsLeft == 0) {
          return Error(Err::IN_PROGRESS, "scroll deactivate waiting for budget");
        }
        Status st = _sendCommand(cmd::SCROLL_DEACTIVATE);
        _operation.transactionCount++;
        --transactionsLeft;
        if (!st.ok()) {
          _markControlStateDirty(st);
          return _failOperation(
              st, _effectForFailure(st, _operation.transactionCount > 1));
        }
        if (_scrollActive) markAllDirty();
        _scrollActive = false;
        _operation.effect = EffectState::PARTIAL;
        _operation.phase = OperationPhase::SCROLL_CONFIGURE;
        if (transactionsLeft == 0) {
          return Error(Err::IN_PROGRESS, "scroll setup pending");
        }
        continue;
      }

      case OperationPhase::SCROLL_CONFIGURE: {
        if (transactionsLeft == 0) {
          return Error(Err::IN_PROGRESS, "scroll setup waiting for budget");
        }
        Status st;
        if (_operation.kind == OperationKind::HORIZONTAL_SCROLL) {
          const uint8_t commands[7] = {
              _operationScrollLeft ? cmd::SCROLL_LEFT : cmd::SCROLL_RIGHT,
              cmd::SCROLL_DUMMY,
              _operationScrollStartPage,
              static_cast<uint8_t>(_operationScrollSpeed),
              _operationScrollEndPage,
              cmd::SCROLL_COL_START,
              cmd::SCROLL_COL_END};
          st = _sendCommandList(commands, sizeof(commands));
        } else if (_operation.kind == OperationKind::VERTICAL_SCROLL) {
          const uint8_t commands[8] = {
              _operationScrollLeft ? cmd::SCROLL_VERT_LEFT
                                   : cmd::SCROLL_VERT_RIGHT,
              cmd::SCROLL_ONE_COL,
              _operationScrollStartPage,
              static_cast<uint8_t>(_operationScrollSpeed),
              _operationScrollEndPage,
              _operationScrollVerticalOffset,
              cmd::SCROLL_COL_START,
              cmd::SCROLL_COL_END};
          st = _sendCommandList(commands, sizeof(commands));
        } else {
          return _failOperation(Error(Err::INTERNAL_ERROR,
                                      "invalid scroll operation kind"),
                                EffectState::PARTIAL);
        }
        _operation.transactionCount++;
        --transactionsLeft;
        if (!st.ok()) {
          _markControlStateDirty(st);
          return _failOperation(st, _effectForFailure(st, true));
        }
        _operation.effect = EffectState::PARTIAL;
        _operation.phase = OperationPhase::SCROLL_ACTIVATE;
        if (transactionsLeft == 0) {
          return Error(Err::IN_PROGRESS, "scroll activation pending");
        }
        continue;
      }

      case OperationPhase::SCROLL_ACTIVATE: {
        if (transactionsLeft == 0) {
          return Error(Err::IN_PROGRESS, "scroll activation waiting for budget");
        }
        Status st = _sendCommand(cmd::SCROLL_ACTIVATE);
        _operation.transactionCount++;
        if (!st.ok()) {
          _markControlStateDirty(st);
          return _failOperation(st, _effectForFailure(st, true));
        }
        _scrollActive = true;
        _finishOperation();
        return Ok();
      }

      case OperationPhase::IDLE:
        if (_operation.kind == OperationKind::FLUSH ||
            _operation.kind == OperationKind::RESYNC) {
          if (_operation.kind == OperationKind::FLUSH) {
            _finishOperation();
            return Ok();
          }
          _operation.phase = OperationPhase::DISPLAY_ON;
          continue;
        }
        return _failOperation(Error(Err::INTERNAL_ERROR,
                                    "active operation has idle phase"),
                              EffectState::PARTIAL);

      case OperationPhase::COMPLETE:
      default:
        return _operation.status;
    }
  }
  return _operation.status;
}

Status SSD1315::cancelOperation() {
  if (!_operationActive()) {
    return Error(Err::STATE_ERROR, "no active operation");
  }
  (void)_terminateOperation(Error(Err::CANCELLED, "operation cancelled"),
                            OperationState::CANCELLED);
  return Ok();
}

Status SSD1315::takeOperationResult(OperationResult& out) {
  if (!_operationResultReady || _operationActive()) {
    return Error(Err::RESULT_NOT_AVAILABLE, "operation result not available");
  }
  out = _operation;
  _operationResultReady = false;
  _operation = OperationProgress{};
  return Ok();
}

void SSD1315::invalidatePanelState() {
  const Status unknown = Error(Err::CONTROL_STATE_UNKNOWN,
                               "panel state invalidated by caller");
  _panelPowerState = PanelPowerState::UNKNOWN;
  if (_operationActive()) {
    (void)cancelOperation();
  }
  _controlStateDirty = true;
  _controlStateError = unknown;
  _gddramSynchronized = false;
}

// ============================================================================
// Probe and recovery
// ============================================================================

Status SSD1315::_probeRaw() {
  // SSD1315 has no WHOAMI register. We send NOP (0xE3) and check ACK.
  // NOP command is safe and has no side effects.
  const uint8_t buf[2] = {cmd::CTRL_COMMAND, cmd::NOP};
  Status st = _i2cWriteRaw(buf, 2);  // No health tracking!

  if (st.code == Err::I2C_NACK_ADDR) {
    return Error(Err::DEVICE_NOT_FOUND, st.detail, "Device not responding");
  }

  // Note: probe() does NOT call _updateHealth() - diagnostic only
  return st;
}

Status SSD1315::probe() {
  if (!_attached) {
    return Error(Err::NOT_INITIALIZED, "driver not attached");
  }
  Status admission = _checkDirectI2cAdmission();
  if (!admission.ok()) return admission;
  return _probeRaw();
}

Status SSD1315::_runBlockingOperation(OperationKind kind) {
  if (kind == OperationKind::RESYNC && isPageBufferMode()) {
    return Error(Err::UNSUPPORTED,
                 "resync requires a full framebuffer");
  }
  if ((kind == OperationKind::RESYNC || kind == OperationKind::WAKE) &&
      _config.displayOnDelayMs != 0 && _config.nowMs == nullptr) {
    return Error(Err::INVALID_CONFIG,
                 "blocking display-on requires nowMs callback");
  }

  Status st = _startOperation(kind, _nextCompatibilityOptions());
  if (!st.ok()) {
    return st;
  }
  return _runBlockingAdmittedOperation();
}

OperationOptions SSD1315::_nextCompatibilityOptions() {
  ++_compatRequestId;
  if (_compatRequestId == 0 || _compatRequestId == _lastOperationRequestId) {
    ++_compatRequestId;
  }
  OperationOptions options;
  options.requestId = _compatRequestId;
  return options;
}

Status SSD1315::_runBlockingAdmittedOperation() {
  Status st = Error(Err::IN_PROGRESS, "operation in progress");

  uint32_t lastNowMs = _nowMs();
  uint32_t stalledIterations = 0;
  const uint32_t maxStalledIterations = waitFlushStallGuardIterations(
      saturatedAdd(_config.displayOnDelayMs, _config.flushTimeoutMs),
      _config.i2cTimeoutMs);
  while (_operationActive()) {
    const OperationPhase phaseBefore = _operation.phase;
    const uint16_t transactionsBefore = _operation.transactionCount;
    const uint32_t bytesBefore = _operation.bytesCompleted;
    const uint8_t pageBefore = _operation.currentPage;
    const uint16_t columnBefore = _operation.currentColumn;
    st = pollOperation(lastNowMs, MAX_POLL_TRANSACTIONS,
                       static_cast<uint16_t>(_config.byteBudgetPerTick));
    if (!st.ok() && !st.inProgress()) {
      break;
    }
    if (!_operationActive()) {
      break;
    }
    _cooperativeYield();
    const uint32_t nextNowMs = _nowMs();
    const bool operationProgressed =
        _operation.phase != phaseBefore ||
        _operation.transactionCount != transactionsBefore ||
        _operation.bytesCompleted != bytesBefore ||
        _operation.currentPage != pageBefore ||
        _operation.currentColumn != columnBefore;
    if (nextNowMs == lastNowMs && !operationProgressed) {
      if (++stalledIterations >= maxStalledIterations) {
        st = _terminateOperation(
            Error(Err::TIMEOUT, "blocking operation clock stalled"),
            OperationState::TIMED_OUT);
        break;
      }
    } else {
      lastNowMs = nextNowMs;
      stalledIterations = 0;
    }
  }

  OperationResult result;
  if (!takeOperationResult(result).ok()) {
    return st.ok() ? Error(Err::INTERNAL_ERROR,
                           "blocking operation lost terminal result")
                   : st;
  }
  return result.status;
}

Status SSD1315::recover() {
  if (!_attached) {
    return Error(Err::NOT_INITIALIZED, "driver not attached");
  }
  return _runBlockingOperation(OperationKind::RESYNC);
}

// ============================================================================
// Lifecycle
// ============================================================================

Status SSD1315::begin(const Config& config) {
  Status attachStatus = attach(config);
  if (!attachStatus.ok()) {
    return attachStatus;
  }
  const OperationKind lifecycleKind = _config.clearOnBegin &&
                                          !isPageBufferMode()
                                          ? OperationKind::RESYNC
                                          : OperationKind::INITIALIZE;
  return _runBlockingOperation(lifecycleKind);
}

void SSD1315::tick(uint32_t nowMs) {
  if (!_attached) {
    return;
  }
  tickPowerOn(nowMs);
  if (_operationActive()) {
    (void)pollOperation(nowMs, 1, _config.byteBudgetPerTick);
    return;
  }
  if (_operationResultReady) {
    return;
  }
  if (_initialized && isFlushing()) {
    (void)pollFlush(nowMs, 1, _config.byteBudgetPerTick);
  }
}

void SSD1315::end() {
  detach();
}

// ============================================================================
// Display initialization
// ============================================================================

// ============================================================================
// Raw command access
// ============================================================================

Status SSD1315::_sendCommand(uint8_t command) {
  const uint8_t buf[2] = {cmd::CTRL_COMMAND, command};
  return _i2cWriteTracked(buf, 2);
}

Status SSD1315::_sendCommand2(uint8_t command, uint8_t arg) {
  const uint8_t buf[3] = {cmd::CTRL_COMMAND, command, arg};
  return _i2cWriteTracked(buf, 3);
}

Status SSD1315::_sendCommand3(uint8_t command, uint8_t arg1, uint8_t arg2) {
  const uint8_t buf[4] = {cmd::CTRL_COMMAND, command, arg1, arg2};
  return _i2cWriteTracked(buf, 4);
}

Status SSD1315::_sendCommandList(const uint8_t* commands, size_t length) {
  if (length == 0) return Ok();
  if (commands == nullptr || length > COMMAND_LIST_MAX_BYTES) {
    return Error(Err::INVALID_CONFIG, "command list buffer invalid");
  }
  if (length + 1u > _config.maxWriteBytes) {
    return Error(Err::BUFFER_OVERFLOW,
                 "command list exceeds transport write capacity");
  }
  uint8_t buffer[COMMAND_LIST_MAX_BYTES + 1];
  buffer[0] = cmd::CTRL_COMMAND;
  memcpy(buffer + 1, commands, length);
  return _i2cWriteTracked(buffer, length + 1u);
}

Status SSD1315::sendCommand(uint8_t cmd) {
  Status admission = _checkDirectI2cAdmission();
  if (!admission.ok()) return admission;
  if (!_initialized) return Error(Err::NOT_INITIALIZED, "not initialized");
  Status st = _sendCommand(cmd);
  if (st.ok()) {
    invalidatePanelState();
  } else {
    _markRawCommandFailure(st);
  }
  return st;
}

Status SSD1315::sendCommand2(uint8_t cmd, uint8_t arg) {
  Status admission = _checkDirectI2cAdmission();
  if (!admission.ok()) return admission;
  if (!_initialized) return Error(Err::NOT_INITIALIZED, "not initialized");
  Status st = _sendCommand2(cmd, arg);
  if (st.ok()) {
    invalidatePanelState();
  } else {
    _markRawCommandFailure(st);
  }
  return st;
}

Status SSD1315::sendCommand3(uint8_t cmd, uint8_t arg1, uint8_t arg2) {
  Status admission = _checkDirectI2cAdmission();
  if (!admission.ok()) return admission;
  if (!_initialized) return Error(Err::NOT_INITIALIZED, "not initialized");
  Status st = _sendCommand3(cmd, arg1, arg2);
  if (st.ok()) {
    invalidatePanelState();
  } else {
    _markRawCommandFailure(st);
  }
  return st;
}

Status SSD1315::sendCommandList(const uint8_t* cmds, size_t len) {
  Status admission = _checkDirectI2cAdmission();
  if (!admission.ok()) return admission;
  if (!_initialized) return Error(Err::NOT_INITIALIZED, "not initialized");
  if (len == 0) return Ok();
  if (cmds == nullptr) {
    return Error(Err::INVALID_CONFIG, "command list buffer invalid");
  }
  if (len > COMMAND_LIST_MAX_BYTES) {
    return Error(Err::BUFFER_OVERFLOW, "command list too long");
  }
  if (len + 1u > _config.maxWriteBytes) {
    return Error(Err::BUFFER_OVERFLOW,
                 "command list exceeds transport write capacity");
  }

  Status st = _sendCommandList(cmds, len);
  if (!st.ok()) {
    _markRawCommandFailure(st);
    return st;
  }
  invalidatePanelState();
  return Ok();
}

// ============================================================================
// Display control
// ============================================================================

Status SSD1315::setContrast(uint8_t contrast) {
  Status admission = _checkDirectI2cAdmission();
  if (!admission.ok()) return admission;
  if (!_initialized) return Error(Err::NOT_INITIALIZED, "not initialized");
  if (contrast < cmd::CONTRAST_MIN) {
    return Error(Err::INVALID_CONFIG, "contrast must be 1..255");
  }
  Status st = _sendCommand2(cmd::SET_CONTRAST, contrast);
  if (st.ok()) {
    _config.contrast = contrast;
  } else {
    _markControlStateDirty(st);
  }
  return st;
}

Status SSD1315::setInvert(bool invert) {
  Status admission = _checkDirectI2cAdmission();
  if (!admission.ok()) return admission;
  if (!_initialized) return Error(Err::NOT_INITIALIZED, "not initialized");
  Status st = _sendCommand(invert ? cmd::INVERT_DISPLAY : cmd::NORMAL_DISPLAY);
  if (st.ok()) {
    _config.invert = invert;
  } else {
    _markControlStateDirty(st);
  }
  return st;
}

Status SSD1315::setFlipX(bool flip) {
  Status admission = _checkDirectI2cAdmission();
  if (!admission.ok()) return admission;
  if (!_initialized) return Error(Err::NOT_INITIALIZED, "not initialized");
  Status st = _sendCommand(flip ? cmd::SEG_REMAP_ON : cmd::SEG_REMAP_OFF);
  if (st.ok()) {
    _config.flipX = flip;
  } else {
    _markControlStateDirty(st);
  }
  return st;
}

Status SSD1315::setFlipY(bool flip) {
  Status admission = _checkDirectI2cAdmission();
  if (!admission.ok()) return admission;
  if (!_initialized) return Error(Err::NOT_INITIALIZED, "not initialized");
  Status st = _sendCommand(flip ? cmd::COM_SCAN_DEC : cmd::COM_SCAN_INC);
  if (st.ok()) {
    _config.flipY = flip;
  } else {
    _markControlStateDirty(st);
  }
  return st;
}

Status SSD1315::setSleep(bool sleep) {
  Status admission = _checkDirectI2cAdmission();
  if (!admission.ok()) return admission;
  if (!_initialized) return Error(Err::NOT_INITIALIZED, "not initialized");
  if (!sleep && _controlStateDirty) {
    return Error(Err::CONTROL_STATE_UNKNOWN,
                 "panel state unknown; resync required");
  }
  if (!sleep && (isDirty() || !_gddramSynchronized)) {
    return Error(Err::STATE_ERROR,
                 "wake requires synchronized clean GDDRAM");
  }

  Status st = _sendCommand(sleep ? cmd::DISPLAY_OFF : cmd::DISPLAY_ON);
  if (st.ok()) {
    _sleeping = sleep;
    if (sleep) {
      _panelPowerState = PanelPowerState::OFF;
      _powerState = PowerState::OFF;
    } else {
      // Waking up - start power-on timing guard
      if (_config.displayOnDelayMs == 0) {
        _powerState = PowerState::READY;
        _powerOnDelayStarted = false;
        _panelPowerState = PanelPowerState::ON;
      } else {
        _powerState = PowerState::INIT_DELAY;
        _powerOnMs = 0;
        _powerOnDelayStarted = false;  // Will be set on next tick
        _panelPowerState = PanelPowerState::STARTING;
      }
    }
  } else {
    _panelPowerState = PanelPowerState::UNKNOWN;
    _markControlStateDirty(st);
  }
  return st;
}

Status SSD1315::setAllPixelsOn(bool allOn) {
  Status admission = _checkDirectI2cAdmission();
  if (!admission.ok()) return admission;
  if (!_initialized) return Error(Err::NOT_INITIALIZED, "not initialized");
  Status st = _sendCommand(allOn ? cmd::DISPLAY_ALL_ON : cmd::DISPLAY_RAM);
  if (st.ok()) {
    _allPixelsOn = allOn;
  } else {
    _markControlStateDirty(st);
  }
  return st;
}

// ============================================================================
// Auto-sleep and activity
// ============================================================================

void SSD1315::setAutoSleep(uint32_t inactivityMs) {
  _config.inactivitySleepMs = inactivityMs;
}

void SSD1315::touch() {
  // Deprecated activity policy is application-owned; retained as a no-op.
}

// ============================================================================
// Page cycling
// ============================================================================

void SSD1315::setUserPageCount(uint8_t count) {
  _userPageCount = (count == 0) ? 1 : count;
  if (_activeUserPage >= _userPageCount) {
    _activeUserPage = 0;
  }
}

void SSD1315::setActiveUserPage(uint8_t index) {
  _activeUserPage = (index < _userPageCount) ? index : 0;
}

void SSD1315::setPageCycleInterval(uint32_t intervalMs) {
  _config.pageCycleMs = intervalMs;
}

// ============================================================================
// Tick helpers
// ============================================================================

void SSD1315::tickPowerOn(uint32_t nowMs) {
  if (_powerState == PowerState::INIT_DELAY) {
    if (_config.displayOnDelayMs == 0) {
      _powerState = PowerState::READY;
      _powerOnDelayStarted = false;
      if (!_sleeping) _panelPowerState = PanelPowerState::ON;
      return;
    }
    if (!_powerOnDelayStarted) {
      _powerOnMs = nowMs;
      _powerOnDelayStarted = true;
      return;
    }
    // Unsigned subtraction handles 32-bit clock rollover correctly.
    uint32_t elapsed = nowMs - _powerOnMs;
    if (elapsed >= _config.displayOnDelayMs) {
      _powerState = PowerState::READY;
      _powerOnDelayStarted = false;
      if (!_sleeping) _panelPowerState = PanelPowerState::ON;
    }
  }
}

Status SSD1315::pollFlush(uint32_t nowMs, uint8_t maxInstructions,
                          uint16_t byteBudget) {
  if (_operationActive() || _operationResultReady) {
    return Error(Err::BUSY,
                 "cooperative operation active or result pending");
  }
  return _pollFlushInternal(nowMs, maxInstructions, byteBudget);
}

Status SSD1315::_pollFlushInternal(uint32_t nowMs, uint8_t maxInstructions,
                                   uint16_t byteBudget) {
  if (!_initialized) {
    return Error(Err::NOT_INITIALIZED, "not initialized");
  }
  tickPowerOn(nowMs);

  if (_inPageIteration &&
      (_flushState == FlushState::DONE || _flushState == FlushState::ERROR)) {
    return (_flushState == FlushState::ERROR)
               ? (_flushError.ok() ? Error(Err::INTERNAL_ERROR, "flush failed") : _flushError)
               : Ok();
  }

  // Handle completed flush states - track health once at completion
  if (_flushState == FlushState::DONE) {
    // Flush completed successfully - track once. A direct pollFlush() caller
    // may already have observed the terminal OK on the completing call.
    if (!_flushAccounted) {
      _updateHealth(Ok());
    }
    _flushAccounted = false;
    _flushState = FlushState::IDLE;
    return Ok();
  }

  if (_flushState == FlushState::ERROR) {
    // Flush failed - track once with accumulated error. A direct pollFlush()
    // caller may already have observed the terminal error on the failing call.
    Status flushFailure =
        _flushError.ok() ? Error(Err::INTERNAL_ERROR, "flush failed") : _flushError;
    if (!_flushAccounted) {
      _updateHealth(flushFailure);
    }
    _flushError = flushFailure;
    _flushAccounted = false;
    _flushState = FlushState::IDLE;
    return flushFailure;
  }

  if (_flushState == FlushState::IDLE) {
    return Ok();
  }

  if (maxInstructions == 0) {
    return Error(Err::IN_PROGRESS, "flush query only");
  }

  if (byteBudget == 0) {
    return Error(Err::INVALID_CONFIG, "byteBudget must be > 0");
  }

  // Initialize flush start time if a job was created by older state or tests.
  // A boolean flag avoids sentinel ambiguity when the injected clock is 0 or
  // rolls over to UINT32_MAX.
  if (!_flushStarted) {
    _flushStarted = true;
    _flushStartMs = nowMs;
    _flushError = Ok();  // Reset accumulated error
    _flushAccounted = false;
  }

  // Check timeout
  if (_config.flushTimeoutMs > 0) {
    uint32_t elapsed = nowMs - _flushStartMs;
    if (elapsed >= _config.flushTimeoutMs) {
      _flushError = Error(Err::TIMEOUT, "flush timeout");
      // Write _lastError immediately for real-time diagnostics.
      _lastError = _flushError;
      _flushState = FlushState::ERROR;
      if (!_inPageIteration && !_operationActive()) {
        _updateHealth(_flushError);
        _flushAccounted = true;
      }
      return _flushError;
    }
  }

  // Owner-scheduled flushes may populate GDDRAM while DISPLAY_OFF is confirmed.
  const bool ownerFlushWhileOff =
      _operationActive() &&
      (_operation.kind == OperationKind::RESYNC ||
       _operation.kind == OperationKind::FLUSH) &&
      _initialized && !_controlStateDirty &&
      _panelPowerState == PanelPowerState::OFF;
  const bool pageIterationFlushWhileOff =
      _inPageIteration && isPageBufferMode() && _initialized &&
      !_controlStateDirty && _panelPowerState == PanelPowerState::OFF;
  if (_powerState != PowerState::READY && !ownerFlushWhileOff &&
      !pageIterationFlushWhileOff) {
    return Error(Err::IN_PROGRESS, "flush waiting for panel");
  }

  Status st;

  uint8_t instructionsLeft = maxInstructions;
  uint16_t dataBudget = byteBudget;

  while (instructionsLeft > 0) {
    switch (_flushState) {
      case FlushState::SET_COL_ADDR: {
        uint8_t colBuf[4] = {cmd::CTRL_COMMAND, cmd::SET_COL_ADDR,
                             _flushMinCol,
                             _flushMaxCol};
        st = _i2cWriteRaw(colBuf, sizeof(colBuf));
        if (_flushTransactionCount < UINT16_MAX) {
          ++_flushTransactionCount;
        }
        if (!st.ok()) {
          _flushError = st;
          _lastError = st;  // Immediate diagnostics.
          _flushState = FlushState::ERROR;
          if (!_inPageIteration && !_operationActive()) {
            _updateHealth(st);
            _flushAccounted = true;
          }
          return st;
        }
        instructionsLeft--;
        _flushState = FlushState::SET_PAGE_ADDR;
        break;
      }

      case FlushState::SET_PAGE_ADDR: {
        uint8_t pageBuf[4] = {cmd::CTRL_COMMAND, cmd::SET_PAGE_ADDR,
                               _flushPage, _flushPage};
        st = _i2cWriteRaw(pageBuf, sizeof(pageBuf));
        if (_flushTransactionCount < UINT16_MAX) {
          ++_flushTransactionCount;
        }
        if (!st.ok()) {
          _flushError = st;
          _lastError = st;  // Immediate diagnostics.
          _flushState = FlushState::ERROR;
          if (!_inPageIteration && !_operationActive()) {
            _updateHealth(st);
            _flushAccounted = true;
          }
          return st;
        }
        instructionsLeft--;
        _flushCol = _flushMinCol;
        _flushState = FlushState::SEND_DATA;
        break;
      }

      case FlushState::SEND_DATA: {
        if (dataBudget == 0) {
          return Error(Err::IN_PROGRESS, "flush data budget exhausted");
        }

        const uint8_t bufferPage = _flushPage % _config.pageBufferPages;
        const size_t remaining = _flushMaxCol - _flushCol + 1;
        size_t toSend = remaining;
        if (toSend > FLUSH_DATA_CHUNK_BYTES) {
          toSend = FLUSH_DATA_CHUNK_BYTES;
        }
        const size_t transportPayload =
            maxDataBytesForWriteCapacity(_config.maxWriteBytes);
        if (toSend > transportPayload) {
          toSend = transportPayload;
        }
        if (toSend > dataBudget) {
          toSend = dataBudget;
        }

        const size_t bufOffset = _flushCol + static_cast<size_t>(bufferPage) * _config.width;
        uint8_t txBuf[FLUSH_DATA_CHUNK_BYTES + 1];
        txBuf[0] = cmd::CTRL_DATA;
        memcpy(txBuf + 1, _buffer + bufOffset, toSend);

        st = _i2cWriteRaw(txBuf, toSend + 1);
        if (_flushTransactionCount < UINT16_MAX) {
          ++_flushTransactionCount;
        }
        if (!st.ok()) {
          _flushError = st;  // Accumulate error.
          _lastError = st;   // Immediate diagnostics.
          _flushState = FlushState::ERROR;
          if (!_inPageIteration && !_operationActive()) {
            _updateHealth(st);
            _flushAccounted = true;
          }
          return st;
        }

        instructionsLeft--;
        dataBudget -= static_cast<uint16_t>(toSend);
        _flushCol += static_cast<uint16_t>(toSend);
        _flushBytesCompleted = saturatedAdd(
            _flushBytesCompleted, static_cast<uint32_t>(toSend));
        if (_flushDataChunkCount < UINT16_MAX) {
          ++_flushDataChunkCount;
        }

        if (_flushCol <= _flushMaxCol) {
          break;
        }

        // Clear the page only if no framebuffer mutator marked it dirty while
        // this page was being transferred. Otherwise leave it dirty so a later
        // requestFlush() retries the current framebuffer.
        if (_dirtyGeneration[_flushPage] == _flushPageGeneration) {
          _dirtyPages &= static_cast<uint8_t>(~(1u << _flushPage));
          _dirtyMinCol[_flushPage] = 0xFF;
          _dirtyMaxCol[_flushPage] = 0x00;
        }

        _flushPage++;
        bool found = false;
        while (_flushPage <= _flushEndPage) {
          if (_dirtyPages & static_cast<uint8_t>(1u << _flushPage)) {
            _flushMinCol = _dirtyMinCol[_flushPage];
            _flushMaxCol = _dirtyMaxCol[_flushPage];
            if (_flushMaxCol >= _flushMinCol) {
              _flushPageGeneration = _dirtyGeneration[_flushPage];
              _flushState = FlushState::SET_COL_ADDR;
              found = true;
              break;
            }
          }
          _flushPage++;
        }

        if (!found) {
          _flushState = FlushState::DONE;
          if (!isPageBufferMode() && _dirtyPages == 0 &&
              (_gddramSynchronized ||
               _flushBytesCompleted == getBufferSize())) {
            _gddramSynchronized = true;
          }
          if (!_inPageIteration && !_operationActive()) {
            _updateHealth(Ok());
            _flushAccounted = true;
          }
          return Ok();
        }
        break;
      }

      default:
        return Ok();
    }
  }

  if (_flushState == FlushState::DONE) {
    return Ok();
  }
  if (_flushState == FlushState::ERROR) {
    return _flushError.ok() ? Error(Err::INTERNAL_ERROR, "flush failed") : _flushError;
  }
  return Error(Err::IN_PROGRESS, "flush in progress");
}

// ============================================================================
// Flush control
// ============================================================================

Status SSD1315::requestFlush() {
  if (_operationActive() || _operationResultReady) {
    return Error(Err::BUSY,
                 "cooperative operation active or result pending");
  }
  return _requestFlushInternal();
}

Status SSD1315::_requestFlushInternal() {
  if (!_initialized) {
    return Error(Err::NOT_INITIALIZED, "not initialized");
  }
  // If caller requests a new flush before tick() consumes terminal state,
  // account it now to keep health tracking exact.
  if (_flushState == FlushState::DONE) {
    if (!_flushAccounted) {
      _updateHealth(Ok());
    }
    _flushAccounted = false;
    _flushState = FlushState::IDLE;
  } else if (_flushState == FlushState::ERROR) {
    Status flushFailure =
        _flushError.ok() ? Error(Err::INTERNAL_ERROR, "flush failed") : _flushError;
    if (!_flushAccounted) {
      _updateHealth(flushFailure);
    }
    _flushError = flushFailure;
    _flushAccounted = false;
    _flushState = FlushState::IDLE;
  }

  if (_flushState == FlushState::SET_COL_ADDR ||
      _flushState == FlushState::SET_PAGE_ADDR ||
      _flushState == FlushState::SEND_DATA) {
    return Error(Err::BUSY, "flush in progress");
  }
  if (_controlStateDirty) {
    return Error(Err::CONTROL_STATE_UNKNOWN,
                 "panel state unknown; resync required");
  }

  _flushBytesCompleted = 0;
  _flushDataChunkCount = 0;
  _flushTransactionCount = 0;

  // Find first dirty page
  if (_dirtyPages == 0) {
    _flushState = FlushState::IDLE;
    return Ok();
  }
  if (_scrollActive) {
    return Error(Err::STATE_ERROR, "scroll active; stop scroll before flushing GDDRAM");
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
    _flushPageGeneration = _dirtyGeneration[p];
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

  _flushState = FlushState::SET_COL_ADDR;
  _flushStarted = false;  // Timer latches on first tick()/pollFlush() caller timestamp.
  _flushAccounted = false;
  _flushError = Ok();

  return Ok();
}

Status SSD1315::requestFlushRect(int16_t x, int16_t y, int16_t w, int16_t h) {
  if (_operationActive() || _operationResultReady) {
    return Error(Err::BUSY,
                 "cooperative operation active or result pending");
  }
  if (!_initialized) {
    return Error(Err::NOT_INITIALIZED, "not initialized");
  }
  if (_flushState == FlushState::DONE) {
    if (!_flushAccounted) {
      _updateHealth(Ok());
    }
    _flushAccounted = false;
    _flushState = FlushState::IDLE;
  } else if (_flushState == FlushState::ERROR) {
    Status flushFailure =
        _flushError.ok() ? Error(Err::INTERNAL_ERROR, "flush failed") : _flushError;
    if (!_flushAccounted) {
      _updateHealth(flushFailure);
    }
    _flushError = flushFailure;
    _flushAccounted = false;
    _flushState = FlushState::IDLE;
  }

  if (_flushState == FlushState::SET_COL_ADDR ||
      _flushState == FlushState::SET_PAGE_ADDR ||
      _flushState == FlushState::SEND_DATA) {
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

bool SSD1315::isFlushing() const {
  return _flushState == FlushState::SET_COL_ADDR ||
         _flushState == FlushState::SET_PAGE_ADDR ||
         _flushState == FlushState::SEND_DATA;
}

FlushStatus SSD1315::getFlushStatus() const {
  FlushStatus out;
  switch (_flushState) {
    case FlushState::SET_COL_ADDR:
      out.phase = FlushPhase::SET_COL_ADDR;
      break;
    case FlushState::SET_PAGE_ADDR:
      out.phase = FlushPhase::SET_PAGE_ADDR;
      break;
    case FlushState::SEND_DATA:
      out.phase = FlushPhase::SEND_DATA;
      break;
    case FlushState::DONE:
      out.phase = FlushPhase::DONE;
      break;
    case FlushState::ERROR:
      out.phase = FlushPhase::ERROR;
      break;
    case FlushState::IDLE:
    default:
      out.phase = FlushPhase::IDLE;
      break;
  }

  out.inProgress = isFlushing();
  out.dirtyPages = _dirtyPages;
  out.currentPage = _flushPage;
  out.endPage = _flushEndPage;
  out.currentColumn = _flushCol;
  out.minColumn = _flushMinCol;
  out.maxColumn = _flushMaxCol;
  out.bytesCompleted = _flushBytesCompleted;
  out.dataChunkCount = _flushDataChunkCount;
  out.transactionCount = _flushTransactionCount;
  out.lastError = (_flushState == FlushState::ERROR) ? _flushError : _lastError;
  return out;
}

Status SSD1315::waitFlush(uint32_t nowMs, uint32_t timeoutMs) {
  if (_operationActive() || _operationResultReady) {
    return Error(Err::BUSY,
                 "cooperative operation active or result pending");
  }
  if (!_initialized) {
    return Error(Err::NOT_INITIALIZED, "not initialized");
  }
  if (timeoutMs == 0) {
    timeoutMs = _config.flushTimeoutMs;
  }
  if (timeoutMs == 0) {
    timeoutMs = 5000;  // Default 5s
  }

  // Use caller-provided timestamp when available; otherwise sample the hook.
  // If no hook is configured, keep the caller timestamp as the wait clock so
  // unsigned elapsed math does not underflow from nowMs -> 0.
  const bool hasTimeHook = (_config.nowMs != nullptr);
  uint32_t start = nowMs;
  if (start == 0) {
    start = _nowMs();
  }
  uint32_t lastObservedMs = start;
  uint32_t stalledIterations = 0;
  const uint32_t maxStalledIterations =
      waitFlushStallGuardIterations(timeoutMs, _config.i2cTimeoutMs);

  // Wait for power-on delay AND flush to complete.
  // Uses unsigned subtraction which is safe across 32-bit clock rollover.
  while (isFlushing() || (!_sleeping && _powerState != PowerState::READY)) {
    uint32_t currentMs = hasTimeHook ? _nowMs() : start;
    tick(currentMs);

    // Unsigned subtraction handles 32-bit clock rollover correctly.
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

    if (currentMs == lastObservedMs) {
      if (++stalledIterations >= maxStalledIterations) {
        if (isFlushing()) {
          _flushError = Error(Err::TIMEOUT, "waitFlush time stalled");
          _lastError = _flushError;
          _flushState = FlushState::ERROR;
          _updateHealth(_flushError);
          _flushState = FlushState::IDLE;
        }
        return Error(Err::TIMEOUT, "waitFlush time stalled");
      }
    } else {
      lastObservedMs = currentMs;
      stalledIterations = 0;
    }

    // The application-injected hook may cooperatively yield between bounded
    // attempts. The core does not assume a framework, scheduler, or delay API.
    _cooperativeYield();
  }

  // Flush may have reached terminal state in the final tick() above.
  if (_flushState == FlushState::DONE) {
    if (!_flushAccounted) {
      _updateHealth(Ok());
    }
    _flushAccounted = false;
    _flushState = FlushState::IDLE;
  } else if (_flushState == FlushState::ERROR) {
    Status flushFailure =
        _flushError.ok() ? Error(Err::INTERNAL_ERROR, "flush failed") : _flushError;
    if (!_flushAccounted) {
      _updateHealth(flushFailure);
    }
    _flushError = flushFailure;
    _flushAccounted = false;
    _flushState = FlushState::IDLE;
    return flushFailure;
  }

  return Ok();
}

// ============================================================================
// Buffer helpers
// ============================================================================

size_t SSD1315::bufferIndex(int16_t x, int16_t y) const {
  // In page buffer mode, y is relative to current buffer page
  uint8_t page = (y / 8) % _config.pageBufferPages;
  return x + static_cast<size_t>(page) * _config.width;
}

uint8_t SSD1315::bufferBit(int16_t y) const {
  return 1 << (y & 7);
}

bool SSD1315::isInBuffer(int16_t x, int16_t y) const {
  if (!_attached || _buffer == nullptr) {
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

size_t SSD1315::getBufferSize() const {
  if (!_attached) return 0;
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
void SSD1315::markDirty(uint8_t page, uint8_t minCol, uint8_t maxCol) {
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
  _dirtyGeneration[page]++;
}

void SSD1315::markAllDirty() {
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
    _dirtyGeneration[p]++;
  }
}

void SSD1315::markDirtyRect(int16_t x, int16_t y, int16_t w, int16_t h) {
  if (!_attached || w <= 0 || h <= 0) {
    return;
  }
  int32_t x0 = x;
  int32_t y0 = y;
  int32_t x1 = static_cast<int32_t>(x) + static_cast<int32_t>(w) - 1;
  int32_t y1 = static_cast<int32_t>(y) + static_cast<int32_t>(h) - 1;
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 >= _config.width) x1 = _config.width - 1;
  if (y1 >= _config.height) y1 = _config.height - 1;
  if (x0 > x1 || y0 > y1) {
    return;
  }
  const uint8_t firstDirtyPage = static_cast<uint8_t>(y0 / 8);
  const uint8_t lastDirtyPage = static_cast<uint8_t>(y1 / 8);
  for (uint8_t page = firstDirtyPage; page <= lastDirtyPage; ++page) {
    markDirty(page, static_cast<uint8_t>(x0), static_cast<uint8_t>(x1));
  }
}

void SSD1315::clearDirty() {
  if (_dirtyPages != 0) {
    _gddramSynchronized = false;
  }
  _dirtyPages = 0;
  memset(_dirtyMinCol, 0xFF, sizeof(_dirtyMinCol));
  memset(_dirtyMaxCol, 0x00, sizeof(_dirtyMaxCol));
}

Status SSD1315::clearDirtyIfIdle() {
  if (_operationActive() || _operationResultReady) {
    return Error(Err::BUSY,
                 "cooperative operation active or result pending");
  }
  if (_flushState == FlushState::SET_COL_ADDR ||
      _flushState == FlushState::SET_PAGE_ADDR ||
      _flushState == FlushState::SEND_DATA) {
    return Error(Err::BUSY, "flush in progress");
  }

  if (_dirtyPages != 0 &&
      (_flushState == FlushState::ERROR || !_flushError.ok())) {
    return Error(Err::STATE_ERROR, "dirty retry state pending");
  }

  clearDirty();
  return Ok();
}

bool SSD1315::isDirty() const {
  return _dirtyPages != 0;
}

// ============================================================================
// Page buffer mode
// ============================================================================

bool SSD1315::isPageBufferMode() const {
  return _attached && _config.pageBufferPages < _totalPages;
}

Status SSD1315::firstPage() {
  if (!_attached) {
    return Error(Err::NOT_INITIALIZED, "driver not attached");
  }
  if (_operationActive() || _operationResultReady || isFlushing()) {
    return Error(Err::BUSY,
                 "operation, result, or flush state owns framebuffer");
  }
  if (_flushState == FlushState::ERROR && _dirtyPages != 0) {
    return Error(Err::STATE_ERROR,
                 "dirty page retry must complete before firstPage");
  }
  if (_flushState == FlushState::DONE || _flushState == FlushState::ERROR) {
    if (!_flushAccounted) {
      _updateHealth(_flushState == FlushState::DONE ? Ok() : _flushError);
    }
    _flushState = FlushState::IDLE;
    _flushStarted = false;
    _flushAccounted = false;
    _flushError = Ok();
  }
  if (_dirtyPages != 0 && !_flushError.ok()) {
    return Error(Err::STATE_ERROR,
                 "dirty page retry must complete before firstPage");
  }

  memset(_buffer, 0, getBufferSize());
  _gddramSynchronized = false;
  _currentBufferPage = 0;
  if (!isPageBufferMode()) {
    _inPageIteration = false;
    markAllDirty();
    return Ok();
  }
  _inPageIteration = true;
  clearDirty();
  markAllDirty();
  return Ok();
}

bool SSD1315::nextPage() {
  if (!_initialized || !_inPageIteration) return false;
  if (_operationActive() || _operationResultReady) {
    return true;  // Consume cooperative ownership before changing windows.
  }
  if (!isPageBufferMode()) {
    _inPageIteration = false;
    return false;
  }

  // If flush is still in progress, return true (more work to do)
  // Caller should call tick() and try again
  if (isFlushing()) {
    return true;
  }

  // Check if previous flush failed
  if (_flushState == FlushState::ERROR) {
    Status flushFailure =
        _flushError.ok() ? Error(Err::INTERNAL_ERROR, "flush failed") : _flushError;
    if (!_flushAccounted) {
      _updateHealth(flushFailure);
    }
    _flushAccounted = false;
    _flushError = flushFailure;
    _flushState = FlushState::IDLE;
    _inPageIteration = true;
    return false;  // Caller should check lastError()
  }

  // If this is NOT the first call (i.e., we've already flushed at least once),
  // advance to next page set
  if (_flushState == FlushState::DONE) {
    if (!_flushAccounted) {
      _updateHealth(Ok());
    }
    _flushAccounted = false;
    if (_dirtyPages != 0) {
      // A framebuffer mutation raced a completed page-window transfer. Keep
      // the same window selected and require a new flush before advancing.
      _flushState = FlushState::IDLE;
      _flushStarted = false;
      _gddramSynchronized = false;
      return true;
    }
    _currentBufferPage++;
    uint8_t nextDisplayPage = _currentBufferPage * _config.pageBufferPages;

    if (nextDisplayPage >= _totalPages) {
      // Iteration complete
      _inPageIteration = false;
      _currentBufferPage = 0;
      _flushState = FlushState::IDLE;
      _gddramSynchronized = true;
      return false;
    }

    // Clear buffer for next page and reset flush state
    memset(_buffer, 0, getBufferSize());
    clearDirty();
    markAllDirty();
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

int16_t SSD1315::pageBufferYOffset() const {
  if (!_attached) return 0;
  return _currentBufferPage * _config.pageBufferPages * 8;
}

// ============================================================================
// Drawing primitives
// ============================================================================

void SSD1315::clear() {
  if (!_attached || _buffer == nullptr) return;

  memset(_buffer, 0, getBufferSize());
  markAllDirty();
}

void SSD1315::fill() {
  if (!_attached || _buffer == nullptr) return;

  memset(_buffer, 0xFF, getBufferSize());
  markAllDirty();
}

void SSD1315::setPixel(int16_t x, int16_t y, bool on) {
  if (!_attached || _buffer == nullptr) return;
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
}

bool SSD1315::getPixel(int16_t x, int16_t y) const {
  if (!_attached || _buffer == nullptr) return false;
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

void SSD1315::drawHLine(int16_t x, int16_t y, int16_t w, bool on) {
  if (!_attached || _buffer == nullptr || w <= 0) return;
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
}

void SSD1315::drawVLine(int16_t x, int16_t y, int16_t h, bool on) {
  if (!_attached || _buffer == nullptr || h <= 0) return;
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
}

void SSD1315::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, bool on) {
  if (!_attached || _buffer == nullptr || w <= 0 || h <= 0) return;

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

void SSD1315::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, bool on) {
  if (!_attached || _buffer == nullptr || w <= 0 || h <= 0) return;

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
}

void SSD1315::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, bool on) {
  if (!_attached || _buffer == nullptr) return;

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

void SSD1315::drawCircle(int16_t cx, int16_t cy, int16_t r, bool on) {
  if (!_attached || _buffer == nullptr || r < 0) return;
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

void SSD1315::fillCircle(int16_t cx, int16_t cy, int16_t r, bool on) {
  if (!_attached || _buffer == nullptr || r < 0) return;
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

Status SSD1315::drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap,
                            int16_t w, int16_t h, size_t bitmapSizeBytes,
                            bool on) {
  if (!_attached || _buffer == nullptr) {
    return Error(Err::NOT_INITIALIZED, "not initialized");
  }
  if (w <= 0 || h <= 0) {
    return Ok();
  }

  const size_t byteWidth = (static_cast<size_t>(w) + 7u) / 8u;
  const size_t requiredBytes = byteWidth * static_cast<size_t>(h);
  if (bitmap == nullptr) {
    return Error(Err::INVALID_CONFIG, "bitmap is null");
  }
  if (bitmapSizeBytes < requiredBytes) {
    return Error(Err::BUFFER_TOO_SMALL, "bitmap buffer too small");
  }

  int32_t x0 = x;
  int32_t y0 = y;
  int32_t x1 = static_cast<int32_t>(x) + static_cast<int32_t>(w) - 1;
  int32_t y1 = static_cast<int32_t>(y) + static_cast<int32_t>(h) - 1;

  if (x1 < 0 || y1 < 0 || x0 >= _config.width || y0 >= _config.height) {
    return Ok();
  }

  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 >= _config.width) x1 = _config.width - 1;
  if (y1 >= _config.height) y1 = _config.height - 1;

  for (int32_t j = y0; j <= y1; j++) {
    int32_t srcY = j - y;
    const uint8_t* bmpRow = bitmap + static_cast<size_t>(srcY) * byteWidth;

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
  return Ok();
}

// ============================================================================
// Text rendering
// ============================================================================

void SSD1315::drawChar(int16_t x, int16_t y, char c, bool on) {
  if (!_attached || _buffer == nullptr) return;

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

void SSD1315::drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap,
                          int16_t w, int16_t h, bool on) {
  if (w <= 0 || h <= 0) {
    (void)drawBitmap(x, y, bitmap, w, h, 0, on);
    return;
  }
  const size_t byteWidth = (static_cast<size_t>(w) + 7u) / 8u;
  const size_t requiredBytes = byteWidth * static_cast<size_t>(h);
  (void)drawBitmap(x, y, bitmap, w, h, requiredBytes, on);
}

int16_t SSD1315::drawText(int16_t x, int16_t y, const char* str, bool on) {
  if (str == nullptr) return x;
  size_t length = 0;
  while (length < TEXT_MAX_CHARS && str[length] != '\0') {
    ++length;
  }
  return drawTextN(x, y, str, length, on);
}

int16_t SSD1315::drawTextN(int16_t x, int16_t y, const char* data,
                           size_t length, bool on) {
  if (data == nullptr || length == 0) return x;
  if (!_attached || _buffer == nullptr) return x;

  const size_t boundedLength =
      length < TEXT_MAX_CHARS ? length : TEXT_MAX_CHARS;
  const int32_t startX = x;
  int32_t cursorX = x;
  int32_t cursorY = y;
  for (size_t index = 0; index < boundedLength; ++index) {
    const char c = data[index];

    if (c == '\n') {
      cursorX = startX;
      cursorY += CHAR_HEIGHT;
      continue;
    }

    if (c == '\r') {
      cursorX = startX;
      continue;
    }

    if (cursorX >= INT16_MIN && cursorX <= INT16_MAX &&
        cursorY >= INT16_MIN && cursorY <= INT16_MAX) {
      drawChar(static_cast<int16_t>(cursorX), static_cast<int16_t>(cursorY), c, on);
    }
    cursorX += CHAR_WIDTH;
  }

  if (cursorX > INT16_MAX) return INT16_MAX;
  if (cursorX < INT16_MIN) return INT16_MIN;
  return static_cast<int16_t>(cursorX);
}

int16_t SSD1315::getTextWidth(const char* str) {
  if (str == nullptr) return 0;
  size_t length = 0;
  while (length < TEXT_MAX_CHARS && str[length] != '\0') {
    ++length;
  }
  return getTextWidthN(str, length);
}

int16_t SSD1315::getTextWidthN(const char* data, size_t length) {
  if (data == nullptr || length == 0) return 0;
  const size_t boundedLength =
      length < TEXT_MAX_CHARS ? length : TEXT_MAX_CHARS;

  // Use int32_t accumulators to avoid int16_t overflow on very long strings.
  int32_t maxWidth = 0;
  int32_t curWidth = 0;

  for (size_t index = 0; index < boundedLength; ++index) {
    const char c = data[index];
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

void SSD1315::fillCheckerboard(uint8_t size) {
  if (!_attached || _buffer == nullptr || size == 0) return;

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
}

void SSD1315::fillVerticalStripes(uint8_t width) {
  if (!_attached || _buffer == nullptr || width == 0) return;

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
}

void SSD1315::fillHorizontalStripes(uint8_t height) {
  if (!_attached || _buffer == nullptr || height == 0) return;

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
}

// ============================================================================
// Hardware scrolling
// ============================================================================

Status SSD1315::_validateHorizontalScroll(bool, uint8_t startPage,
                                          uint8_t endPage,
                                          ScrollSpeed speed) const {
  if (_config.width != MAX_WIDTH) {
    return Error(Err::UNSUPPORTED, "hardware scroll requires 128-column panel");
  }
  if (startPage > endPage || endPage >= _totalPages) {
    return Error(Err::INVALID_CONFIG, "invalid page range");
  }
  if (!isValidScrollSpeed(speed)) {
    return Error(Err::INVALID_CONFIG, "invalid scroll speed");
  }
  if (_config.maxWriteBytes < 8u) {
    return Error(Err::BUFFER_OVERFLOW,
                 "horizontal scroll setup exceeds write capacity");
  }
  return Ok();
}

Status SSD1315::_validateVerticalScroll(bool, uint8_t startPage,
                                        uint8_t endPage, ScrollSpeed speed,
                                        uint8_t verticalOffset) const {
  if (_config.width != MAX_WIDTH) {
    return Error(Err::UNSUPPORTED, "hardware scroll requires 128-column panel");
  }
  if (startPage > endPage || endPage >= _totalPages) {
    return Error(Err::INVALID_CONFIG, "invalid page range");
  }
  if (!isValidScrollSpeed(speed)) {
    return Error(Err::INVALID_CONFIG, "invalid scroll speed");
  }
  if (verticalOffset > 63) {
    return Error(Err::INVALID_CONFIG, "verticalOffset must be 0..63");
  }
  if (_verticalScrollRows == 0 || verticalOffset >= _verticalScrollRows) {
    return Error(Err::INVALID_CONFIG,
                 "verticalOffset must be less than scroll area rows");
  }
  if (_config.maxWriteBytes < 9u) {
    return Error(Err::BUFFER_OVERFLOW,
                 "vertical scroll setup exceeds write capacity");
  }
  return Ok();
}

Status SSD1315::startHorizontalScroll(bool left, uint8_t startPage, uint8_t endPage,
                                       ScrollSpeed speed) {
  Status admission = _checkDirectI2cAdmission();
  if (!admission.ok()) return admission;
  if (!_initialized) return Error(Err::NOT_INITIALIZED, "not initialized");
  Status st = startHorizontalScrollOperation(
      _nextCompatibilityOptions(), left, startPage, endPage, speed);
  return st.ok() ? _runBlockingAdmittedOperation() : st;
}

Status SSD1315::startVerticalScroll(bool left, uint8_t startPage, uint8_t endPage,
                                     ScrollSpeed speed, uint8_t verticalOffset) {
  Status admission = _checkDirectI2cAdmission();
  if (!admission.ok()) return admission;
  if (!_initialized) return Error(Err::NOT_INITIALIZED, "not initialized");
  Status st = startVerticalScrollOperation(
      _nextCompatibilityOptions(), left, startPage, endPage, speed,
      verticalOffset);
  return st.ok() ? _runBlockingAdmittedOperation() : st;
}

Status SSD1315::stopScroll() {
  Status admission = _checkDirectI2cAdmission();
  if (!admission.ok()) return admission;
  if (!_initialized) return Error(Err::NOT_INITIALIZED, "not initialized");
  Status st = _sendCommand(cmd::SCROLL_DEACTIVATE);
  if (!st.ok()) {
    _markControlStateDirty(st);
  } else {
    if (_scrollActive) {
      markAllDirty();
    }
    _scrollActive = false;
  }
  return st;
}

Status SSD1315::setVerticalScrollArea(uint8_t topFixedRows, uint8_t scrollRows) {
  Status admission = _checkDirectI2cAdmission();
  if (!admission.ok()) return admission;
  if (!_initialized) return Error(Err::NOT_INITIALIZED, "not initialized");
  if (scrollRows == 0 || _config.startLine >= scrollRows ||
      static_cast<uint16_t>(topFixedRows) + scrollRows > _config.height) {
    return Error(Err::INVALID_CONFIG, "invalid scroll area");
  }
  Status st = _sendCommand3(cmd::SET_VERT_SCROLL_AREA, topFixedRows, scrollRows);
  if (!st.ok()) {
    _markControlStateDirty(st);
  } else {
    _verticalScrollRows = scrollRows;
  }
  return st;
}

// ============================================================================
// Advanced display features
// ============================================================================

Status SSD1315::setFadeMode(FadeMode mode, uint8_t interval) {
  Status admission = _checkDirectI2cAdmission();
  if (!admission.ok()) return admission;
  if (!_initialized) return Error(Err::NOT_INITIALIZED, "not initialized");
  if (!isValidFadeMode(mode)) {
    return Error(Err::INVALID_CONFIG, "invalid fade mode");
  }
  if (interval > 15) {
    return Error(Err::INVALID_CONFIG, "fade interval must be 0..15");
  }
  uint8_t arg = static_cast<uint8_t>(mode) | (interval & 0x0F);
  Status st = _sendCommand2(cmd::SET_FADE_BLINK, arg);
  if (!st.ok()) {
    _markControlStateDirty(st);
  }
  return st;
}

Status SSD1315::setZoom(bool enable) {
  Status admission = _checkDirectI2cAdmission();
  if (!admission.ok()) return admission;
  if (!_initialized) return Error(Err::NOT_INITIALIZED, "not initialized");
  if (enable && !isAlternativeComPinsConfig(_config.comPins)) {
    return Error(Err::INVALID_CONFIG, "zoom requires alternative COM pins");
  }
  Status st = _sendCommand2(cmd::SET_ZOOM, enable ? 0x01 : 0x00);
  if (!st.ok()) {
    _markControlStateDirty(st);
  }
  return st;
}

}  // namespace SSD1315
