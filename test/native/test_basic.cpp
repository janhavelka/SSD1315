/// @file test_basic.cpp
/// @brief Native contract tests for SSD1315 lifecycle and health behavior.

#include <unity.h>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <type_traits>

#include "Arduino.h"
#include "Wire.h"

SerialClass Serial;
TwoWire Wire;
uint32_t gMillis = 0;
uint32_t gMicros = 0;
uint32_t gMillisStep = 0;
uint32_t gMicrosStep = 0;

#include "SSD1315.h"

namespace {

struct FakeBus {
  struct Transaction {
    uint8_t addr = 0;
    uint32_t timeoutMs = 0;
    size_t len = 0;
    bool truncated = false;
    uint8_t data[132] = {};
  };

  static constexpr size_t MAX_TRANSACTIONS = 260;
  static constexpr size_t MAX_TRANSACTION_BYTES = 132;

  uint32_t nowMs = 100;
  uint32_t writeCalls = 0;
  uint32_t yieldCalls = 0;
  bool advanceNowOnYield = true;
  int failWriteRemaining = 0;
  uint32_t failOnWriteCall = 0;
  bool sawChargePumpDisabled = false;
  SSD1315::TransportResult failResult = SSD1315::TransportResult::NackAddress(-1);
  Transaction transactions[MAX_TRANSACTIONS] = {};
  size_t transactionCount = 0;
  bool transactionOverflow = false;

  void clearTransactions() {
    transactionCount = 0;
    transactionOverflow = false;
    for (size_t i = 0; i < MAX_TRANSACTIONS; ++i) {
      transactions[i] = Transaction{};
    }
  }
};

SSD1315::TransportResult fakeI2cWrite(uint8_t addr, const uint8_t* data, size_t len,
                                      uint32_t timeoutMs, void* user) {
  FakeBus* bus = static_cast<FakeBus*>(user);
  bus->writeCalls++;
  if (data == nullptr || len == 0) {
    return SSD1315::TransportResult::BusError(-100);
  }
  if (bus->transactionCount < FakeBus::MAX_TRANSACTIONS) {
    FakeBus::Transaction& tx = bus->transactions[bus->transactionCount++];
    tx.addr = addr;
    tx.timeoutMs = timeoutMs;
    tx.len = len;
    const size_t copyLen = len > FakeBus::MAX_TRANSACTION_BYTES
                               ? FakeBus::MAX_TRANSACTION_BYTES
                               : len;
    memcpy(tx.data, data, copyLen);
    tx.truncated = len > FakeBus::MAX_TRANSACTION_BYTES;
  } else {
    bus->transactionOverflow = true;
  }
  if (len >= 3 && data[0] == SSD1315::cmd::CTRL_COMMAND &&
      data[1] == SSD1315::cmd::SET_CHARGE_PUMP && data[2] == 0x10) {
    bus->sawChargePumpDisabled = true;
  }
  if (bus->failOnWriteCall != 0 && bus->writeCalls == bus->failOnWriteCall) {
    bus->failOnWriteCall = 0;
    return bus->failResult;
  }
  if (bus->failWriteRemaining > 0) {
    bus->failWriteRemaining--;
    return bus->failResult;
  }
  return SSD1315::TransportResult::Ok();
}

uint32_t fakeNowMs(void* user) {
  return static_cast<FakeBus*>(user)->nowMs;
}

void fakeYield(void* user) {
  FakeBus* bus = static_cast<FakeBus*>(user);
  bus->yieldCalls++;
  if (bus->advanceNowOnYield) {
    bus->nowMs++;
  }
}

SSD1315::Config makeConfig(FakeBus& bus) {
  SSD1315::Config cfg;
  cfg.i2cWrite = fakeI2cWrite;
  cfg.i2cUser = &bus;
  cfg.nowMs = fakeNowMs;
  cfg.cooperativeYield = fakeYield;
  cfg.timeUser = &bus;
  cfg.i2cTimeoutMs = 10;
  cfg.offlineThreshold = 3;
  cfg.byteBudgetPerTick = 64;
  cfg.displayOnDelayMs = 0;
  return cfg;
}

void assertTransactionBytes(const FakeBus::Transaction& tx, const uint8_t* expected,
                            size_t expectedLen) {
  TEST_ASSERT_FALSE(tx.truncated);
  TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(expectedLen),
                           static_cast<uint32_t>(tx.len));
  for (size_t i = 0; i < expectedLen; ++i) {
    TEST_ASSERT_EQUAL_HEX8(expected[i], tx.data[i]);
  }
}

bool transactionHasCommand(const FakeBus::Transaction& tx, uint8_t command) {
  return tx.len >= 2 && tx.data[0] == SSD1315::cmd::CTRL_COMMAND &&
         tx.data[1] == command;
}

bool logContainsCommand(const FakeBus& bus, uint8_t command) {
  for (size_t i = 0; i < bus.transactionCount; ++i) {
    if (transactionHasCommand(bus.transactions[i], command)) {
      return true;
    }
  }
  return false;
}

size_t requireCommandIndex(const FakeBus& bus, uint8_t command, size_t start = 0) {
  for (size_t i = start; i < bus.transactionCount; ++i) {
    if (transactionHasCommand(bus.transactions[i], command)) {
      return i;
    }
  }
  TEST_FAIL_MESSAGE("expected command was not found in transaction log");
  return 0;
}

uint32_t countCommand(const FakeBus& bus, uint8_t command) {
  uint32_t count = 0;
  for (size_t i = 0; i < bus.transactionCount; ++i) {
    if (transactionHasCommand(bus.transactions[i], command)) {
      count++;
    }
  }
  return count;
}

bool logContainsCommandArg(const FakeBus& bus, uint8_t command, uint8_t arg) {
  for (size_t i = 0; i < bus.transactionCount; ++i) {
    const FakeBus::Transaction& tx = bus.transactions[i];
    if (tx.len >= 3 && tx.data[0] == SSD1315::cmd::CTRL_COMMAND &&
        tx.data[1] == command && tx.data[2] == arg) {
      return true;
    }
  }
  return false;
}

uint32_t countDataPayloadBytes(const FakeBus& bus, uint8_t value,
                               bool requireValue = true) {
  uint32_t bytes = 0;
  for (size_t i = 0; i < bus.transactionCount; ++i) {
    const FakeBus::Transaction& tx = bus.transactions[i];
    if (tx.len == 0 || tx.data[0] != SSD1315::cmd::CTRL_DATA) {
      continue;
    }
    for (size_t b = 1; b < tx.len; ++b) {
      if (requireValue) {
        TEST_ASSERT_EQUAL_HEX8(value, tx.data[b]);
      }
      bytes++;
    }
  }
  return bytes;
}

uint32_t countDataTransactions(const FakeBus& bus) {
  uint32_t count = 0;
  for (size_t i = 0; i < bus.transactionCount; ++i) {
    const FakeBus::Transaction& tx = bus.transactions[i];
    if (tx.len > 0 && tx.data[0] == SSD1315::cmd::CTRL_DATA) {
      TEST_ASSERT_FALSE(tx.truncated);
      count++;
    }
  }
  return count;
}

void drainFlush(SSD1315::SSD1315& display, FakeBus& bus, uint8_t maxTicks = 80) {
  for (uint8_t i = 0; i < maxTicks && display.isFlushing(); ++i) {
    display.tick(bus.nowMs);
  }
  TEST_ASSERT_FALSE(display.isFlushing());
}

bool loadTextFile(const char* relativePath, std::string& out) {
  std::ifstream in(relativePath, std::ios::in | std::ios::binary);
  if (!in.good()) {
    return false;
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  out = ss.str();
  return true;
}

void assertControlStateDirty(SSD1315::SSD1315& display, SSD1315::Err expected) {
  SSD1315::SettingsSnapshot snap;
  TEST_ASSERT_TRUE(display.getSettings(snap).ok());
  TEST_ASSERT_TRUE(snap.controlStateDirty);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected),
                          static_cast<uint8_t>(snap.controlStateError.code));
}

void assertControlStateClean(SSD1315::SSD1315& display) {
  SSD1315::SettingsSnapshot snap;
  TEST_ASSERT_TRUE(display.getSettings(snap).ok());
  TEST_ASSERT_FALSE(snap.controlStateDirty);
  TEST_ASSERT_TRUE(snap.controlStateError.ok());
}

SSD1315::OperationOptions operationOptions(uint32_t requestId) {
  SSD1315::OperationOptions options;
  options.requestId = requestId;
  return options;
}

void pollOperationUntilTerminal(SSD1315::SSD1315& display, FakeBus& bus,
                                uint8_t maxTransactions = 1,
                                uint16_t byteBudget = 128,
                                uint32_t maxPolls = 4096) {
  for (uint32_t poll = 0; poll < maxPolls; ++poll) {
    const uint32_t writesBefore = bus.writeCalls;
    const SSD1315::Status st = display.pollOperation(
        bus.nowMs, maxTransactions, byteBudget);
    TEST_ASSERT_TRUE(bus.writeCalls - writesBefore <= maxTransactions);
    if (display.getOperationProgress().state != SSD1315::OperationState::ACTIVE) {
      TEST_ASSERT_FALSE(st.inProgress());
      return;
    }
    TEST_ASSERT_TRUE(st.inProgress());
    bus.nowMs++;
  }
  TEST_FAIL_MESSAGE("cooperative operation did not terminate within poll bound");
}

void takeSuccessfulOperation(SSD1315::SSD1315& display,
                             SSD1315::OperationResult& result) {
  TEST_ASSERT_TRUE(display.takeOperationResult(result).ok());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(SSD1315::OperationState::SUCCEEDED),
      static_cast<uint8_t>(result.state));
  TEST_ASSERT_TRUE(result.status.ok());
}

void attachAndInitialize(SSD1315::SSD1315& display, FakeBus& bus,
                         const SSD1315::Config& config,
                         uint32_t requestId = 1) {
  const uint32_t writesBefore = bus.writeCalls;
  TEST_ASSERT_TRUE(display.attach(config).ok());
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);
  TEST_ASSERT_TRUE(display.startInitialize(operationOptions(requestId)).ok());
  pollOperationUntilTerminal(display, bus);
  SSD1315::OperationResult result;
  takeSuccessfulOperation(display, result);
  TEST_ASSERT_EQUAL_UINT32(requestId, result.requestId);
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_status_ok() {
  const auto st = SSD1315::Ok();
  TEST_ASSERT_EQUAL(SSD1315::Err::OK, st.code);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(st.is(SSD1315::Err::OK));
  TEST_ASSERT_TRUE(static_cast<bool>(st));
}

void test_status_helpers() {
  const SSD1315::Status st = SSD1315::Status::Error(SSD1315::Err::I2C_BUS, "bus");
  TEST_ASSERT_FALSE(st.ok());
  TEST_ASSERT_TRUE(st.is(SSD1315::Err::I2C_BUS_ERROR));
  TEST_ASSERT_TRUE(st.is(SSD1315::Err::I2C_BUS));
  TEST_ASSERT_FALSE(st.inProgress());
  TEST_ASSERT_FALSE(static_cast<bool>(st));

  const SSD1315::Status withDetail =
      SSD1315::Status::Error(SSD1315::Err::I2C_TIMEOUT, "timeout", -7);
  TEST_ASSERT_EQUAL_INT32(-7, withDetail.detail);
}

void test_config_defaults() {
  SSD1315::Config cfg;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::ControllerProfile::SSD1315),
                          static_cast<uint8_t>(cfg.controllerProfile));
  TEST_ASSERT_EQUAL_UINT8(128, cfg.width);
  TEST_ASSERT_EQUAL_UINT8(64, cfg.height);
  TEST_ASSERT_EQUAL_UINT8(0x3C, cfg.i2cAddress);
  TEST_ASSERT_EQUAL_UINT16(65u, cfg.maxWriteBytes);
  TEST_ASSERT_EQUAL_UINT8(8, cfg.pageBufferPages);
  TEST_ASSERT_EQUAL_UINT16(128, cfg.byteBudgetPerTick);
  TEST_ASSERT_TRUE(cfg.clearOnBegin);
  TEST_ASSERT_TRUE(cfg.clearOnRecover);
  TEST_ASSERT_EQUAL_UINT8(3, cfg.offlineThreshold);
}

void test_panel_profiles_apply_module_specific_defaults() {
  SSD1315::Config cfg;

  TEST_ASSERT_TRUE(SSD1315::applyPanelProfile(
      cfg, SSD1315::PanelProfile::WISEVISION_X096_2864KSWPG01_H30_INTERNAL_DC_DC).ok());
  TEST_ASSERT_EQUAL_UINT8(128, cfg.width);
  TEST_ASSERT_EQUAL_UINT8(64, cfg.height);
  TEST_ASSERT_TRUE(cfg.flipX);
  TEST_ASSERT_TRUE(cfg.flipY);
  TEST_ASSERT_EQUAL_UINT8(0xB0, cfg.contrast);
  TEST_ASSERT_EQUAL_UINT8(1, cfg.clockDivide);
  TEST_ASSERT_EQUAL_UINT8(9, cfg.oscFrequency);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::VcomhLevel::V_083_VCC),
                          static_cast<uint8_t>(cfg.vcomh));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::ChargePumpVoltage::V7_5),
                          static_cast<uint8_t>(cfg.chargePumpVoltage));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::IrefSelection::IREF_EXTERNAL),
                          static_cast<uint8_t>(cfg.iref));

  TEST_ASSERT_TRUE(SSD1315::applyPanelProfile(
      cfg, SSD1315::PanelProfile::WISEVISION_X096_2864KSWPG01_H30_EXTERNAL_VCC).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::ChargePumpVoltage::OFF),
                          static_cast<uint8_t>(cfg.chargePumpVoltage));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::IrefSelection::IREF_EXTERNAL),
                          static_cast<uint8_t>(cfg.iref));

  const uint8_t beforeWidth = cfg.width;
  const uint8_t beforeHeight = cfg.height;
  const bool beforeFlipX = cfg.flipX;
  const bool beforeFlipY = cfg.flipY;
  const uint8_t beforeContrast = cfg.contrast;
  const SSD1315::ChargePumpVoltage beforePump = cfg.chargePumpVoltage;
  const SSD1315::IrefSelection beforeIref = cfg.iref;
  const SSD1315::Status invalid = SSD1315::applyPanelProfile(
      cfg, static_cast<SSD1315::PanelProfile>(0xFF));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(invalid.code));
  TEST_ASSERT_EQUAL_UINT8(beforeWidth, cfg.width);
  TEST_ASSERT_EQUAL_UINT8(beforeHeight, cfg.height);
  TEST_ASSERT_EQUAL(beforeFlipX, cfg.flipX);
  TEST_ASSERT_EQUAL(beforeFlipY, cfg.flipY);
  TEST_ASSERT_EQUAL_UINT8(beforeContrast, cfg.contrast);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(beforePump),
                          static_cast<uint8_t>(cfg.chargePumpVoltage));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(beforeIref),
                          static_cast<uint8_t>(cfg.iref));
}

void test_canonical_api_symbols_exist() {
  SSD1315::SSD1315 display;
  (void)display;
  static_assert(!std::is_copy_constructible<SSD1315::SSD1315>::value,
                "SSD1315 must not be copy constructible");
  static_assert(!std::is_copy_assignable<SSD1315::SSD1315>::value,
                "SSD1315 must not be copy assignable");
  static_assert(!std::is_move_constructible<SSD1315::SSD1315>::value,
                "SSD1315 must not be move constructible");
  static_assert(!std::is_move_assignable<SSD1315::SSD1315>::value,
                "SSD1315 must not be move assignable");
}

void test_begin_requires_i2c_write_callback() {
  SSD1315::SSD1315 display;
  SSD1315::Config cfg;
  const SSD1315::Status st = display.begin(cfg);
  TEST_ASSERT_FALSE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::DriverState::UNINIT),
                          static_cast<uint8_t>(display.state()));
}

void test_begin_rejects_invalid_config_enums() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.comPins = static_cast<SSD1315::ComPinsConfig>(0xFF);

  SSD1315::SSD1315 display;
  const SSD1315::Status st = display.begin(cfg);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(display.isInitialized());
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
}

void test_begin_rejects_datasheet_invalid_multiplex_height() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.height = 8;
  cfg.pageBufferPages = 1;

  SSD1315::SSD1315 display;
  const SSD1315::Status st = display.begin(cfg);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::INVALID_DIMENSIONS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(display.isInitialized());
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
}

void test_begin_rejects_non_ssd1315_i2c_address_and_contrast_zero() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.i2cAddress = 0x3E;

  SSD1315::SSD1315 display;
  SSD1315::Status st = display.begin(cfg);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(display.isInitialized());
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);

  cfg = makeConfig(bus);
  cfg.i2cAddress = 0x3D;
  cfg.contrast = 0;
  st = display.begin(cfg);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(display.isInitialized());
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
}

void test_begin_accepts_charge_pump_disabled_reset_value() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.chargePumpVoltage = SSD1315::ChargePumpVoltage::OFF;
  cfg.iref = SSD1315::IrefSelection::IREF_EXTERNAL;

  SSD1315::SSD1315 display;
  const SSD1315::Status st = display.begin(cfg);

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(display.isInitialized());
  TEST_ASSERT_TRUE(bus.sawChargePumpDisabled);
  TEST_ASSERT_TRUE(logContainsCommandArg(
      bus, SSD1315::cmd::SET_CHARGE_PUMP,
      static_cast<uint8_t>(SSD1315::ChargePumpVoltage::OFF)));
  TEST_ASSERT_FALSE(logContainsCommandArg(
      bus, SSD1315::cmd::SET_CHARGE_PUMP,
      static_cast<uint8_t>(SSD1315::ChargePumpVoltage::V7_5)));
  TEST_ASSERT_TRUE(logContainsCommandArg(
      bus, SSD1315::cmd::SET_IREF,
      static_cast<uint8_t>(SSD1315::IrefSelection::IREF_EXTERNAL)));
  const size_t pump = requireCommandIndex(bus, SSD1315::cmd::SET_CHARGE_PUMP);
  const size_t displayOn = requireCommandIndex(bus, SSD1315::cmd::DISPLAY_ON);
  TEST_ASSERT_TRUE(pump < displayOn);
}

void test_wisevision_panel_profiles_drive_expected_init_values() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  TEST_ASSERT_TRUE(SSD1315::applyPanelProfile(
      cfg, SSD1315::PanelProfile::WISEVISION_X096_2864KSWPG01_H30_INTERNAL_DC_DC).ok());

  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  TEST_ASSERT_TRUE(logContainsCommandArg(bus, SSD1315::cmd::SET_CONTRAST, 0xB0));
  TEST_ASSERT_TRUE(logContainsCommandArg(bus, SSD1315::cmd::SET_CLOCK_DIV, 0x90));
  TEST_ASSERT_TRUE(logContainsCommandArg(bus, SSD1315::cmd::SET_PRECHARGE, 0x22));
  TEST_ASSERT_TRUE(logContainsCommandArg(bus, SSD1315::cmd::SET_VCOMH, 0x30));
  TEST_ASSERT_TRUE(logContainsCommandArg(bus, SSD1315::cmd::SET_COM_PINS, 0x12));
  TEST_ASSERT_TRUE(logContainsCommandArg(
      bus, SSD1315::cmd::SET_IREF,
      static_cast<uint8_t>(SSD1315::IrefSelection::IREF_EXTERNAL)));
  TEST_ASSERT_TRUE(logContainsCommandArg(
      bus, SSD1315::cmd::SET_CHARGE_PUMP,
      static_cast<uint8_t>(SSD1315::ChargePumpVoltage::V7_5)));
  TEST_ASSERT_TRUE(logContainsCommand(bus, SSD1315::cmd::SEG_REMAP_ON));
  TEST_ASSERT_TRUE(logContainsCommand(bus, SSD1315::cmd::COM_SCAN_DEC));

  bus = FakeBus{};
  cfg = makeConfig(bus);
  TEST_ASSERT_TRUE(SSD1315::applyPanelProfile(
      cfg, SSD1315::PanelProfile::WISEVISION_X096_2864KSWPG01_H30_EXTERNAL_VCC).ok());
  SSD1315::SSD1315 externalDisplay;
  TEST_ASSERT_TRUE(externalDisplay.begin(cfg).ok());
  TEST_ASSERT_TRUE(logContainsCommandArg(
      bus, SSD1315::cmd::SET_CHARGE_PUMP,
      static_cast<uint8_t>(SSD1315::ChargePumpVoltage::OFF)));
  TEST_ASSERT_FALSE(logContainsCommandArg(
      bus, SSD1315::cmd::SET_CHARGE_PUMP,
      static_cast<uint8_t>(SSD1315::ChargePumpVoltage::V7_5)));
}

void test_begin_uses_ssd1315_golden_init_sequence() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  const SSD1315::Status st = display.begin(makeConfig(bus));

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FALSE(bus.transactionOverflow);

  const uint8_t displayOff[] = {SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::DISPLAY_OFF};
  assertTransactionBytes(bus.transactions[0], displayOff, sizeof(displayOff));

  const uint8_t init[][4] = {
      {SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::DISPLAY_OFF, 0, 0},
      {SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::SET_MEMORY_MODE, SSD1315::cmd::ADDR_MODE_HORIZONTAL, 0},
      {SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::SET_START_LINE, 0, 0},
      {SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::SEG_REMAP_OFF, 0, 0},
      {SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::SET_MULTIPLEX, 63, 0},
      {SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::COM_SCAN_INC, 0, 0},
      {SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::SET_DISPLAY_OFFSET, 0, 0},
      {SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::SET_COM_PINS, 0x12, 0},
      {SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::SET_CLOCK_DIV, 0x80, 0},
      {SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::SET_PRECHARGE, 0x22, 0},
      {SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::SET_VCOMH, 0x20, 0},
      {SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::SET_CONTRAST, 0x7F, 0},
      {SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::SET_IREF, 0x10, 0},
      {SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::SET_CHARGE_PUMP, 0x14, 0},
      {SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::DISPLAY_RAM, 0, 0},
      {SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::NORMAL_DISPLAY, 0, 0},
      {SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::SCROLL_DEACTIVATE, 0, 0},
  };
  const size_t initLens[] = {2, 3, 2, 2, 3, 2, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2};
  for (size_t i = 0; i < sizeof(initLens) / sizeof(initLens[0]); ++i) {
    assertTransactionBytes(bus.transactions[i], init[i], initLens[i]);
  }

  const size_t memoryMode = requireCommandIndex(bus, SSD1315::cmd::SET_MEMORY_MODE);
  const size_t iref = requireCommandIndex(bus, SSD1315::cmd::SET_IREF);
  const size_t pump = requireCommandIndex(bus, SSD1315::cmd::SET_CHARGE_PUMP);
  const size_t colAddrIndex = requireCommandIndex(bus, SSD1315::cmd::SET_COL_ADDR);
  const size_t pageAddrIndex = requireCommandIndex(bus, SSD1315::cmd::SET_PAGE_ADDR);
  const size_t displayOnIndex = requireCommandIndex(bus, SSD1315::cmd::DISPLAY_ON);

  TEST_ASSERT_EQUAL_UINT32(1u, countCommand(bus, SSD1315::cmd::DISPLAY_ON));
  TEST_ASSERT_TRUE(memoryMode < colAddrIndex);
  TEST_ASSERT_TRUE(colAddrIndex < pageAddrIndex);
  TEST_ASSERT_TRUE(pageAddrIndex < displayOnIndex);
  TEST_ASSERT_TRUE(iref < displayOnIndex);
  TEST_ASSERT_TRUE(pump < displayOnIndex);

  const uint8_t colAddr[] = {
      SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::SET_COL_ADDR, 0, 127};
  const uint8_t pageAddr[] = {
      SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::SET_PAGE_ADDR, 0, 0};
  assertTransactionBytes(bus.transactions[colAddrIndex], colAddr, sizeof(colAddr));
  assertTransactionBytes(bus.transactions[pageAddrIndex], pageAddr, sizeof(pageAddr));

  size_t firstDataIndex = bus.transactionCount;
  size_t lastDataIndex = 0;
  for (size_t i = 0; i < bus.transactionCount; ++i) {
    const FakeBus::Transaction& tx = bus.transactions[i];
    if (tx.len > 0 && tx.data[0] == SSD1315::cmd::CTRL_DATA) {
      if (firstDataIndex == bus.transactionCount) firstDataIndex = i;
      lastDataIndex = i;
    }
  }
  TEST_ASSERT_TRUE(firstDataIndex != bus.transactionCount);
  TEST_ASSERT_TRUE(pageAddrIndex < firstDataIndex);
  TEST_ASSERT_TRUE(lastDataIndex < displayOnIndex);
  TEST_ASSERT_EQUAL_UINT32(1024u, countDataPayloadBytes(bus, 0x00));

  const uint8_t displayOn[] = {SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::DISPLAY_ON};
  assertTransactionBytes(bus.transactions[displayOnIndex], displayOn, sizeof(displayOn));
}

void test_clear_on_begin_can_skip_blocking_gddram_clear() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.clearOnBegin = false;
  cfg.displayOnDelayMs = 0;
  SSD1315::SSD1315 display;

  const SSD1315::Status st = display.begin(cfg);

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(display.isDirty());
  TEST_ASSERT_FALSE(logContainsCommand(bus, SSD1315::cmd::SET_COL_ADDR));
  TEST_ASSERT_EQUAL_UINT32(0u, countDataPayloadBytes(bus, 0x00, false));
  TEST_ASSERT_EQUAL_HEX8(SSD1315::cmd::SCROLL_DEACTIVATE,
                          bus.transactions[bus.transactionCount - 1].data[1]);
  TEST_ASSERT_EQUAL_UINT32(0u, countCommand(bus, SSD1315::cmd::DISPLAY_ON));
}

void test_failed_begin_during_clear_never_sends_display_on() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  SSD1315::SSD1315 display;
  bus.failOnWriteCall = 20u;  // 17 init + 2 address-window writes, then first data
  bus.failResult = SSD1315::TransportResult::Timeout(-21);

  const SSD1315::Status st = display.begin(cfg);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_TRUE(display.isInitialized());
  TEST_ASSERT_FALSE(logContainsCommand(bus, SSD1315::cmd::DISPLAY_ON));

  SSD1315::SettingsSnapshot snap;
  TEST_ASSERT_TRUE(display.getSettings(snap).ok());
  TEST_ASSERT_FALSE(snap.controlStateDirty);
  TEST_ASSERT_TRUE(snap.controlStateError.ok());
  TEST_ASSERT_TRUE(display.isDirty());
}

void test_raw_commands_require_begin() {
  SSD1315::SSD1315 display;
  const SSD1315::Status st = display.sendCommand(SSD1315::cmd::NOP);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::NOT_INITIALIZED),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(0u, display.totalFailures());
  TEST_ASSERT_EQUAL_UINT32(0u, display.totalSuccess());
}

void test_begin_success_sets_ready_without_health_counts() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  const SSD1315::Status st = display.begin(makeConfig(bus));
  TEST_ASSERT_TRUE_MESSAGE(st.ok(), st.msg);
  TEST_ASSERT_TRUE(display.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::DriverState::READY),
                          static_cast<uint8_t>(display.state()));
  TEST_ASSERT_TRUE(display.isOnline());
  TEST_ASSERT_EQUAL_UINT32(0u, display.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(0u, display.totalFailures());
  TEST_ASSERT_EQUAL_UINT32(0u, display.lastOkMs());
}

void test_invalid_begin_after_success_preserves_binding_without_i2c() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(makeConfig(bus)).ok());
  TEST_ASSERT_TRUE(display.isInitialized());
  TEST_ASSERT_NOT_NULL(display.getBuffer());

  const uint32_t writesBefore = bus.writeCalls;

  SSD1315::Config bad = makeConfig(bus);
  bad.comPins = static_cast<SSD1315::ComPinsConfig>(0xFF);
  const SSD1315::Status st = display.begin(bad);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_TRUE(display.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::DriverState::READY),
                          static_cast<uint8_t>(display.state()));
  TEST_ASSERT_NOT_NULL(display.getBuffer());
  TEST_ASSERT_EQUAL_UINT32(1024u, static_cast<uint32_t>(display.getBufferSize()));
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);

  SSD1315::SettingsSnapshot snap;
  TEST_ASSERT_TRUE(display.getSettings(snap).ok());
  TEST_ASSERT_TRUE(snap.initialized);
  TEST_ASSERT_EQUAL_UINT8(0x3C, snap.i2cAddress);
  TEST_ASSERT_EQUAL_UINT8(3u, snap.offlineThreshold);
  TEST_ASSERT_GREATER_THAN_UINT32(0u, snap.totalSuccess);
  TEST_ASSERT_EQUAL_UINT32(0u, snap.totalFailures);
}

void test_failed_begin_apply_rolls_back_buffer_and_health() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  SSD1315::SSD1315 display;
  bus.failOnWriteCall = 2u;  // probe succeeds, first init command fails

  const SSD1315::Status st = display.begin(cfg);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::I2C_NACK_ADDR),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(display.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::DriverState::UNINIT),
                          static_cast<uint8_t>(display.state()));
  TEST_ASSERT_NULL(display.getBuffer());
  TEST_ASSERT_EQUAL_UINT32(0u, static_cast<uint32_t>(display.getBufferSize()));
  TEST_ASSERT_EQUAL_UINT32(0u, display.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(0u, display.totalFailures());
}

void test_runtime_i2c_after_begin_updates_health() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(makeConfig(bus)).ok());
  const uint32_t successBefore = display.totalSuccess();

  bus.nowMs = 333u;
  const SSD1315::Status st = display.sendCommand(SSD1315::cmd::NOP);

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(successBefore + 1u, display.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(0u, display.totalFailures());
  TEST_ASSERT_EQUAL_UINT32(333u, display.lastOkMs());
}

void test_get_settings_snapshot() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  SSD1315::Config cfg = makeConfig(bus);
  TEST_ASSERT_TRUE(display.begin(cfg).ok());

  SSD1315::SettingsSnapshot snap;
  TEST_ASSERT_TRUE(display.getSettings(snap).ok());
  TEST_ASSERT_TRUE(snap.initialized);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::DriverState::READY),
                          static_cast<uint8_t>(snap.state));
  const SSD1315::SettingsSnapshot byValue = display.getSettings();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(snap.state),
                          static_cast<uint8_t>(byValue.state));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(display.state()),
                          static_cast<uint8_t>(display.driverState()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::ControllerProfile::SSD1315),
                          static_cast<uint8_t>(snap.controllerProfile));
  TEST_ASSERT_EQUAL_UINT8(0x3C, snap.i2cAddress);
  TEST_ASSERT_EQUAL_UINT32(10u, snap.i2cTimeoutMs);
  TEST_ASSERT_EQUAL_UINT8(3u, snap.offlineThreshold);
  TEST_ASSERT_TRUE(snap.hasNowMsHook);
  TEST_ASSERT_EQUAL_UINT16(65u, snap.maxWriteBytes);
  TEST_ASSERT_EQUAL_UINT8(128u, snap.width);
  TEST_ASSERT_EQUAL_UINT8(64u, snap.height);
  TEST_ASSERT_EQUAL_UINT8(8u, snap.pageBufferPages);
  TEST_ASSERT_FALSE(snap.pageBufferMode);
  TEST_ASSERT_EQUAL_UINT8(0u, snap.currentPageIndex);
  TEST_ASSERT_FALSE(snap.pageIterationActive);
  TEST_ASSERT_EQUAL_UINT32(128u * 8u, static_cast<uint32_t>(snap.bufferSize));
  TEST_ASSERT_FALSE(snap.flushing);
  TEST_ASSERT_FALSE(snap.controlStateDirty);
  TEST_ASSERT_TRUE(snap.controlStateError.ok());
  TEST_ASSERT_TRUE(snap.clearOnBegin);
  TEST_ASSERT_TRUE(snap.clearOnRecover);
  TEST_ASSERT_TRUE(snap.lastError.is(SSD1315::Err::OK));
}

void test_external_buffer_begin_uses_caller_storage_without_ownership() {
  FakeBus bus;
  uint8_t framebuffer[128 * 8] = {};
  SSD1315::Config cfg = makeConfig(bus);
  cfg.externalBuffer = framebuffer;
  cfg.externalBufferSizeBytes = sizeof(framebuffer);
  SSD1315::SSD1315 display;

  TEST_ASSERT_TRUE(display.begin(cfg).ok());

  SSD1315::SettingsSnapshot snap;
  TEST_ASSERT_TRUE(display.getSettings(snap).ok());
  TEST_ASSERT_TRUE(snap.hasExternalBuffer);
  TEST_ASSERT_FALSE(snap.ownsBuffer);
  TEST_ASSERT_EQUAL_UINT32(sizeof(framebuffer), static_cast<uint32_t>(snap.bufferSize));

  display.fill();
  for (size_t i = 0; i < sizeof(framebuffer); ++i) {
    TEST_ASSERT_EQUAL_HEX8(0xFF, framebuffer[i]);
  }

  display.end();
  TEST_ASSERT_EQUAL_HEX8(0xFF, framebuffer[0]);
  TEST_ASSERT_EQUAL_HEX8(0xFF, framebuffer[sizeof(framebuffer) - 1]);
}

void test_external_buffer_rejects_undersized_storage_before_i2c() {
  FakeBus bus;
  uint8_t framebuffer[128 * 8] = {};
  SSD1315::Config cfg = makeConfig(bus);
  cfg.externalBuffer = framebuffer;
  cfg.externalBufferSizeBytes = sizeof(framebuffer) - 1;
  SSD1315::SSD1315 display;

  const SSD1315::Status st = display.begin(cfg);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::BUFFER_TOO_SMALL),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_STRING("external buffer too small", st.msg);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
  TEST_ASSERT_FALSE(display.isInitialized());
}

void test_external_buffer_size_contract_uses_page_buffer_pages() {
  FakeBus bus;
  uint8_t onePage[128] = {};
  SSD1315::Config cfg = makeConfig(bus);
  cfg.pageBufferPages = 1;
  cfg.externalBuffer = onePage;
  cfg.externalBufferSizeBytes = sizeof(onePage);
  SSD1315::SSD1315 display;

  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  TEST_ASSERT_TRUE(display.isPageBufferMode());
  TEST_ASSERT_EQUAL_UINT32(sizeof(onePage), static_cast<uint32_t>(display.getBufferSize()));
}

void test_command_list_parameter_error_does_not_touch_bus_or_health() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(makeConfig(bus)).ok());

  const uint32_t writesBefore = bus.writeCalls;
  const uint32_t successBefore = display.totalSuccess();
  const uint32_t failuresBefore = display.totalFailures();

  const SSD1315::Status st = display.sendCommandList(nullptr, 1);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(successBefore, display.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(failuresBefore, display.totalFailures());
}

void test_command_list_length_is_bounded_without_i2c_on_overflow() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(makeConfig(bus)).ok());

  uint8_t maxList[32] = {};
  for (size_t i = 0; i < sizeof(maxList); ++i) {
    maxList[i] = SSD1315::cmd::NOP;
  }

  bus.clearTransactions();
  TEST_ASSERT_TRUE(display.sendCommandList(maxList, sizeof(maxList)).ok());
  TEST_ASSERT_EQUAL_UINT32(1u, static_cast<uint32_t>(bus.transactionCount));
  TEST_ASSERT_EQUAL_UINT32(33u, static_cast<uint32_t>(bus.transactions[0].len));
  TEST_ASSERT_EQUAL_HEX8(SSD1315::cmd::CTRL_COMMAND, bus.transactions[0].data[0]);

  uint8_t tooLong[33] = {};
  const uint32_t writesBefore = bus.writeCalls;
  const uint32_t successBefore = display.totalSuccess();
  const uint32_t failuresBefore = display.totalFailures();

  const SSD1315::Status st = display.sendCommandList(tooLong, sizeof(tooLong));

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::BUFFER_OVERFLOW),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(successBefore, display.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(failuresBefore, display.totalFailures());
}

void test_probe_requires_initialized_and_does_not_touch_stale_transport() {
  FakeBus bus;
  SSD1315::SSD1315 display;

  SSD1315::Status st = display.probe();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::NOT_INITIALIZED),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);

  TEST_ASSERT_TRUE(display.begin(makeConfig(bus)).ok());
  display.end();
  bus.clearTransactions();
  const uint32_t writesBefore = bus.writeCalls;

  st = display.probe();

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::NOT_INITIALIZED),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, static_cast<uint32_t>(bus.transactionCount));
}

void test_probe_failure_does_not_update_health() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(makeConfig(bus)).ok());

  const uint32_t beforeSuccess = display.totalSuccess();
  const uint32_t beforeFailures = display.totalFailures();
  const uint8_t beforeConsecutive = display.consecutiveFailures();
  const SSD1315::DriverState beforeState = display.state();

  bus.failWriteRemaining = 1;
  bus.failResult = SSD1315::TransportResult::NackAddress(-9);
  const SSD1315::Status st = display.probe();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::DEVICE_NOT_FOUND),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(beforeSuccess, display.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(beforeFailures, display.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(beforeConsecutive, display.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(beforeState),
                          static_cast<uint8_t>(display.state()));
}

void test_probe_timeout_preserves_transport_error() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(makeConfig(bus)).ok());

  const uint32_t beforeSuccess = display.totalSuccess();
  const uint32_t beforeFailures = display.totalFailures();

  bus.failWriteRemaining = 1;
  bus.failResult = SSD1315::TransportResult::Timeout(-19);
  const SSD1315::Status st = display.probe();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-19, st.detail);
  TEST_ASSERT_EQUAL_UINT32(beforeSuccess, display.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(beforeFailures, display.totalFailures());
}

void test_probe_sends_ack_only_nop_transaction() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(makeConfig(bus)).ok());
  bus.clearTransactions();

  const SSD1315::Status st = display.probe();

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(1u, static_cast<uint32_t>(bus.transactionCount));
  const uint8_t expected[] = {SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::NOP};
  assertTransactionBytes(bus.transactions[0], expected, sizeof(expected));
}

void test_probe_preserves_non_address_transport_errors() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(makeConfig(bus)).ok());

  bus.failWriteRemaining = 1;
  bus.failResult = SSD1315::TransportResult::NackData(-31);
  SSD1315::Status st = display.probe();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::I2C_NACK_DATA),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-31, st.detail);

  bus.failWriteRemaining = 1;
  bus.failResult = SSD1315::TransportResult::BusError(-32);
  st = display.probe();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::I2C_BUS_ERROR),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-32, st.detail);

  bus.failWriteRemaining = 1;
  bus.failResult = SSD1315::TransportResult::BusError(-33);
  st = display.probe();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::I2C_BUS_ERROR),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-33, st.detail);
}

void test_recover_failure_updates_health() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(makeConfig(bus)).ok());

  bus.failWriteRemaining = 1;
  bus.failResult = SSD1315::TransportResult::NackAddress(-7);
  const SSD1315::Status st = display.recover();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::DEVICE_NOT_FOUND),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(1u, display.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(1u, display.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::DriverState::DEGRADED),
                          static_cast<uint8_t>(display.state()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::DEVICE_NOT_FOUND),
                          static_cast<uint8_t>(display.lastError().code));
}

void test_recover_success_restores_ready() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(makeConfig(bus)).ok());
  const uint32_t beginSuccess = display.totalSuccess();

  bus.failWriteRemaining = 1;
  bus.failResult = SSD1315::TransportResult::NackAddress(-7);
  (void)display.recover();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::DriverState::DEGRADED),
                          static_cast<uint8_t>(display.state()));

  bus.nowMs = 500;
  const SSD1315::Status st = display.recover();
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::DriverState::READY),
                          static_cast<uint8_t>(display.state()));
  TEST_ASSERT_EQUAL_UINT8(0u, display.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT32(1u, display.totalFailures());
  TEST_ASSERT_GREATER_THAN_UINT32(beginSuccess, display.totalSuccess());
}

void test_recover_reaches_offline_when_threshold_is_one() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.offlineThreshold = 1;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());

  bus.failWriteRemaining = 1;
  bus.failResult = SSD1315::TransportResult::NackAddress(-5);
  const SSD1315::Status st = display.recover();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::DEVICE_NOT_FOUND),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::DriverState::OFFLINE),
                          static_cast<uint8_t>(display.state()));
  TEST_ASSERT_FALSE(display.isOnline());
}

void test_offline_latches_send_command_without_i2c() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.offlineThreshold = 1;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());

  bus.failWriteRemaining = 1;
  bus.failResult = SSD1315::TransportResult::NackAddress(-6);
  SSD1315::Status st = display.recover();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::DriverState::OFFLINE),
                          static_cast<uint8_t>(display.state()));

  const uint32_t writesBefore = bus.writeCalls;
  st = display.sendCommand(SSD1315::cmd::NOP);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::DRIVER_OFFLINE),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_STRING("Driver is offline; call recover()", st.msg);
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);
}

void test_failed_recover_from_offline_keeps_latch_after_intermediate_success() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.offlineThreshold = 3;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());

  for (uint8_t i = 0; i < cfg.offlineThreshold; ++i) {
    bus.failWriteRemaining = 1;
    bus.failResult = SSD1315::TransportResult::NackAddress(-10);
    (void)display.recover();
  }
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::DriverState::OFFLINE),
                          static_cast<uint8_t>(display.state()));

  const uint32_t writesBeforeRecover = bus.writeCalls;
  bus.failResult = SSD1315::TransportResult::Timeout(-11);
  bus.failOnWriteCall = bus.writeCalls + 3u;  // probe + one tracked recovery success, then fail
  const SSD1315::Status st = display.recover();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_GREATER_THAN_UINT32(writesBeforeRecover, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::DriverState::OFFLINE),
                          static_cast<uint8_t>(display.state()));
  TEST_ASSERT_TRUE(display.consecutiveFailures() >= cfg.offlineThreshold);

  const uint32_t writesBefore = bus.writeCalls;
  const SSD1315::Status latched = display.sendCommand(SSD1315::cmd::NOP);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::DRIVER_OFFLINE),
                          static_cast<uint8_t>(latched.code));
  TEST_ASSERT_EQUAL_STRING("Driver is offline; call recover()", latched.msg);
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);
}

void test_next_page_does_not_clear_offline_after_completed_flush() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.displayOnDelayMs = 0;
  cfg.offlineThreshold = 1;
  cfg.pageBufferPages = 1;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  display.tick(bus.nowMs);

  display.firstPage();
  TEST_ASSERT_TRUE(display.nextPage());
  display.tick(bus.nowMs);
  display.tick(bus.nowMs);
  display.tick(bus.nowMs);
  display.tick(bus.nowMs);
  TEST_ASSERT_FALSE(display.isFlushing());

  bus.failWriteRemaining = 1;
  bus.failResult = SSD1315::TransportResult::NackAddress(-12);
  (void)display.recover();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::DriverState::OFFLINE),
                          static_cast<uint8_t>(display.state()));

  TEST_ASSERT_FALSE(display.nextPage());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::DriverState::OFFLINE),
                          static_cast<uint8_t>(display.state()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::DRIVER_OFFLINE),
                          static_cast<uint8_t>(display.lastError().code));
  TEST_ASSERT_EQUAL_STRING("Driver is offline; call recover()", display.lastError().msg);
}

void test_page_buffer_tick_preserves_done_for_next_page() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.displayOnDelayMs = 0;
  cfg.pageBufferPages = 1;
  cfg.byteBudgetPerTick = 255;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  display.tick(bus.nowMs);

  display.firstPage();
  display.setPixel(0, 0, true);
  TEST_ASSERT_TRUE(display.nextPage());
  for (uint8_t i = 0; i < 4 && display.isFlushing(); ++i) {
    display.tick(bus.nowMs);
  }
  TEST_ASSERT_FALSE(display.isFlushing());

  display.tick(bus.nowMs);

  SSD1315::SettingsSnapshot snap;
  TEST_ASSERT_TRUE(display.getSettings(snap).ok());
  TEST_ASSERT_TRUE(snap.pageIterationActive);
  TEST_ASSERT_EQUAL_UINT8(0u, snap.currentPageIndex);

  TEST_ASSERT_TRUE(display.nextPage());
  TEST_ASSERT_TRUE(display.getSettings(snap).ok());
  TEST_ASSERT_TRUE(snap.pageIterationActive);
  TEST_ASSERT_EQUAL_UINT8(1u, snap.currentPageIndex);
}

void test_page_buffer_tick_preserves_error_for_next_page_abort() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.displayOnDelayMs = 0;
  cfg.pageBufferPages = 1;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  display.tick(bus.nowMs);

  display.firstPage();
  display.setPixel(0, 0, true);
  TEST_ASSERT_TRUE(display.nextPage());

  bus.failWriteRemaining = 1;
  bus.failResult = SSD1315::TransportResult::Timeout(-33);
  display.tick(bus.nowMs);
  TEST_ASSERT_FALSE(display.isFlushing());

  display.tick(bus.nowMs);

  TEST_ASSERT_FALSE(display.nextPage());
  SSD1315::SettingsSnapshot snap;
  TEST_ASSERT_TRUE(display.getSettings(snap).ok());
  TEST_ASSERT_FALSE(snap.pageIterationActive);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(display.lastError().code));
  TEST_ASSERT_EQUAL_UINT32(1u, display.totalFailures());
}

void test_page_buffer_clear_affects_current_window_only() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.pageBufferPages = 1;
  cfg.displayOnDelayMs = 0;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  display.tick(bus.nowMs);

  display.firstPage();
  display.fill();
  display.clear();

  SSD1315::SettingsSnapshot snap;
  TEST_ASSERT_TRUE(display.getSettings(snap).ok());
  TEST_ASSERT_EQUAL_UINT8(0x01u, snap.dirtyPages);
  const uint8_t* buffer = display.getBuffer();
  TEST_ASSERT_NOT_NULL(buffer);
  for (uint16_t i = 0; i < display.getBufferSize(); ++i) {
    TEST_ASSERT_EQUAL_HEX8(0x00, buffer[i]);
  }
}

void test_page_buffer_fill_affects_current_window_only() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.pageBufferPages = 1;
  cfg.displayOnDelayMs = 0;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  display.tick(bus.nowMs);

  display.firstPage();
  display.fill();

  SSD1315::SettingsSnapshot snap;
  TEST_ASSERT_TRUE(display.getSettings(snap).ok());
  TEST_ASSERT_EQUAL_UINT8(0x01u, snap.dirtyPages);
  const uint8_t* buffer = display.getBuffer();
  TEST_ASSERT_NOT_NULL(buffer);
  for (uint16_t i = 0; i < display.getBufferSize(); ++i) {
    TEST_ASSERT_EQUAL_HEX8(0xFF, buffer[i]);
  }
}

void test_page_buffer_full_iteration_clear_flushes_all_pages() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.pageBufferPages = 1;
  cfg.displayOnDelayMs = 0;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  display.tick(bus.nowMs);
  bus.clearTransactions();

  display.firstPage();
  for (uint8_t page = 0; page < 8; ++page) {
    display.clear();
    TEST_ASSERT_TRUE(display.nextPage());
    drainFlush(display, bus);
    if (page < 7) {
      TEST_ASSERT_TRUE(display.nextPage());
    } else {
      TEST_ASSERT_FALSE(display.nextPage());
    }
  }

  TEST_ASSERT_EQUAL_UINT32(1024u, countDataPayloadBytes(bus, 0x00));
}

void test_page_buffer_full_iteration_fill_flushes_all_pages() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.pageBufferPages = 1;
  cfg.displayOnDelayMs = 0;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  display.tick(bus.nowMs);
  bus.clearTransactions();

  display.firstPage();
  for (uint8_t page = 0; page < 8; ++page) {
    display.fill();
    TEST_ASSERT_TRUE(display.nextPage());
    drainFlush(display, bus);
    if (page < 7) {
      TEST_ASSERT_TRUE(display.nextPage());
    } else {
      TEST_ASSERT_FALSE(display.nextPage());
    }
  }

  TEST_ASSERT_EQUAL_UINT32(1024u, countDataPayloadBytes(bus, 0xFF));
}

void test_page_buffer_docs_contract_is_not_contradicted_by_api_comments() {
  std::string readme;
  std::string header;
  TEST_ASSERT_TRUE(loadTextFile("README.md", readme));
  TEST_ASSERT_TRUE(loadTextFile("include/ssd1315/SSD1315.h", header));
  TEST_ASSERT_NOT_NULL(strstr(readme.c_str(), "clear()/fill() affect only the current buffer window"));
  TEST_ASSERT_NOT_NULL(strstr(header.c_str(), "In page buffer mode, this clears only the current buffer window"));
  TEST_ASSERT_NOT_NULL(strstr(header.c_str(), "In page buffer mode, this fills only the current buffer window"));
}

void test_invalid_scroll_and_fade_params_do_not_send_i2c() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(makeConfig(bus)).ok());
  const uint32_t writesBefore = bus.writeCalls;
  const uint32_t successBefore = display.totalSuccess();

  SSD1315::Status st = display.startHorizontalScroll(
      false, 0, 1, static_cast<SSD1315::ScrollSpeed>(0x80));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));

  st = display.startHorizontalScroll(false, 2, 1, SSD1315::ScrollSpeed::FRAMES_5);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));

  st = display.startHorizontalScroll(false, 0, 8, SSD1315::ScrollSpeed::FRAMES_5);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));

  st = display.startVerticalScroll(false, 0, 1, SSD1315::ScrollSpeed::FRAMES_5, 64);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));

  st = display.startVerticalScroll(false, 2, 1, SSD1315::ScrollSpeed::FRAMES_5, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));

  st = display.startVerticalScroll(false, 0, 8, SSD1315::ScrollSpeed::FRAMES_5, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));

  st = display.setVerticalScrollArea(0, 0);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));

  st = display.setVerticalScrollArea(63, 2);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));

  st = display.setFadeMode(SSD1315::FadeMode::FADE_OUT, 16);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));

  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(successBefore, display.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(0u, display.totalFailures());
  assertControlStateClean(display);
}

void test_display_control_commands_send_expected_bytes() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(makeConfig(bus)).ok());
  bus.clearTransactions();

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(display.setContrast(0).code));
  TEST_ASSERT_EQUAL_UINT32(0u, static_cast<uint32_t>(bus.transactionCount));

  TEST_ASSERT_TRUE(display.setContrast(1).ok());
  const uint8_t contrastMin[] = {
      SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::SET_CONTRAST, 0x01};
  assertTransactionBytes(bus.transactions[0], contrastMin, sizeof(contrastMin));
  SSD1315::SettingsSnapshot snap;
  TEST_ASSERT_TRUE(display.getSettings(snap).ok());
  TEST_ASSERT_EQUAL_UINT8(0x01, snap.contrast);

  TEST_ASSERT_TRUE(display.setContrast(255).ok());
  const uint8_t contrastMax[] = {
      SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::SET_CONTRAST, 0xFF};
  assertTransactionBytes(bus.transactions[1], contrastMax, sizeof(contrastMax));
  TEST_ASSERT_TRUE(display.getSettings(snap).ok());
  TEST_ASSERT_EQUAL_UINT8(0xFF, snap.contrast);

  TEST_ASSERT_TRUE(display.setInvert(true).ok());
  const uint8_t invertOn[] = {
      SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::INVERT_DISPLAY};
  assertTransactionBytes(bus.transactions[2], invertOn, sizeof(invertOn));

  TEST_ASSERT_TRUE(display.setInvert(false).ok());
  const uint8_t invertOff[] = {
      SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::NORMAL_DISPLAY};
  assertTransactionBytes(bus.transactions[3], invertOff, sizeof(invertOff));

  TEST_ASSERT_TRUE(display.setFlipX(true).ok());
  const uint8_t flipX[] = {
      SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::SEG_REMAP_ON};
  assertTransactionBytes(bus.transactions[4], flipX, sizeof(flipX));

  TEST_ASSERT_TRUE(display.setFlipY(true).ok());
  const uint8_t flipY[] = {
      SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::COM_SCAN_DEC};
  assertTransactionBytes(bus.transactions[5], flipY, sizeof(flipY));

  TEST_ASSERT_TRUE(display.setSleep(true).ok());
  const uint8_t displayOff[] = {
      SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::DISPLAY_OFF};
  assertTransactionBytes(bus.transactions[6], displayOff, sizeof(displayOff));
  TEST_ASSERT_TRUE(display.getSettings(snap).ok());
  TEST_ASSERT_TRUE(snap.sleeping);

  TEST_ASSERT_TRUE(display.setSleep(false).ok());
  const uint8_t displayOn[] = {
      SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::DISPLAY_ON};
  assertTransactionBytes(bus.transactions[7], displayOn, sizeof(displayOn));
  TEST_ASSERT_TRUE(display.getSettings(snap).ok());
  TEST_ASSERT_FALSE(snap.sleeping);

  TEST_ASSERT_TRUE(display.setAllPixelsOn(true).ok());
  const uint8_t allOn[] = {
      SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::DISPLAY_ALL_ON};
  assertTransactionBytes(bus.transactions[8], allOn, sizeof(allOn));
  TEST_ASSERT_TRUE(display.getSettings(snap).ok());
  TEST_ASSERT_TRUE(snap.allPixelsOn);

  TEST_ASSERT_TRUE(display.setAllPixelsOn(false).ok());
  const uint8_t displayRam[] = {
      SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::DISPLAY_RAM};
  assertTransactionBytes(bus.transactions[9], displayRam, sizeof(displayRam));
  TEST_ASSERT_TRUE(display.getSettings(snap).ok());
  TEST_ASSERT_FALSE(snap.allPixelsOn);

  TEST_ASSERT_EQUAL_UINT32(10u, static_cast<uint32_t>(bus.transactionCount));
}

void test_sleep_display_off_does_not_disable_charge_pump() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(makeConfig(bus)).ok());
  bus.clearTransactions();

  TEST_ASSERT_TRUE(display.setSleep(true).ok());
  TEST_ASSERT_EQUAL_UINT32(1u, static_cast<uint32_t>(bus.transactionCount));
  TEST_ASSERT_TRUE(transactionHasCommand(bus.transactions[0], SSD1315::cmd::DISPLAY_OFF));
  TEST_ASSERT_FALSE(logContainsCommand(bus, SSD1315::cmd::SET_CHARGE_PUMP));
}

void test_clear_after_display_off_on_flushes_zero_bytes() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.displayOnDelayMs = 0;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  display.tick(bus.nowMs);

  TEST_ASSERT_TRUE(display.setSleep(true).ok());
  TEST_ASSERT_TRUE(display.setSleep(false).ok());
  display.fill();
  TEST_ASSERT_TRUE(display.requestFlush().ok());
  drainFlush(display, bus);
  display.clear();
  bus.clearTransactions();
  TEST_ASSERT_TRUE(display.requestFlush().ok());
  drainFlush(display, bus);

  TEST_ASSERT_EQUAL_UINT32(1024u, countDataPayloadBytes(bus, 0x00));
}

void test_sleep_or_display_off_failure_sets_control_dirty() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(makeConfig(bus)).ok());

  bus.failOnWriteCall = bus.writeCalls + 1u;
  bus.failResult = SSD1315::TransportResult::Timeout(-80);
  TEST_ASSERT_FALSE(display.setSleep(true).ok());
  assertControlStateDirty(display, SSD1315::Err::I2C_TIMEOUT);
}

void test_recover_then_clear_resyncs_control_and_gddram_state() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.displayOnDelayMs = 0;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  display.tick(bus.nowMs);

  bus.failOnWriteCall = bus.writeCalls + 1u;
  bus.failResult = SSD1315::TransportResult::NackData(-81);
  TEST_ASSERT_FALSE(display.setInvert(true).ok());
  assertControlStateDirty(display, SSD1315::Err::I2C_NACK_DATA);

  TEST_ASSERT_TRUE(display.recover().ok());
  assertControlStateClean(display);
  display.tick(bus.nowMs);
  display.clear();
  bus.clearTransactions();
  TEST_ASSERT_TRUE(display.requestFlush().ok());
  drainFlush(display, bus);
  TEST_ASSERT_EQUAL_UINT32(1024u, countDataPayloadBytes(bus, 0x00));
}

void test_display_control_failures_mark_dirty_and_recover_clears() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(makeConfig(bus)).ok());

  bus.failOnWriteCall = bus.writeCalls + 1u;
  bus.failResult = SSD1315::TransportResult::Timeout(-60);
  TEST_ASSERT_FALSE(display.setContrast(0x33).ok());
  assertControlStateDirty(display, SSD1315::Err::I2C_TIMEOUT);
  SSD1315::SettingsSnapshot snap;
  TEST_ASSERT_TRUE(display.getSettings(snap).ok());
  TEST_ASSERT_EQUAL_UINT8(0x7F, snap.contrast);
  bus.clearTransactions();
  TEST_ASSERT_TRUE(display.recover().ok());
  assertControlStateClean(display);

  bus.failOnWriteCall = bus.writeCalls + 1u;
  bus.failResult = SSD1315::TransportResult::NackData(-61);
  TEST_ASSERT_FALSE(display.setInvert(true).ok());
  assertControlStateDirty(display, SSD1315::Err::I2C_NACK_DATA);
  TEST_ASSERT_TRUE(display.getSettings(snap).ok());
  TEST_ASSERT_FALSE(snap.invert);
  bus.clearTransactions();
  TEST_ASSERT_TRUE(display.recover().ok());
  assertControlStateClean(display);

  bus.failOnWriteCall = bus.writeCalls + 1u;
  bus.failResult = SSD1315::TransportResult::BusError(-62);
  TEST_ASSERT_FALSE(display.setFlipX(true).ok());
  assertControlStateDirty(display, SSD1315::Err::I2C_BUS_ERROR);
  TEST_ASSERT_TRUE(display.getSettings(snap).ok());
  TEST_ASSERT_FALSE(snap.flipX);
  bus.clearTransactions();
  TEST_ASSERT_TRUE(display.recover().ok());
  assertControlStateClean(display);

  bus.failOnWriteCall = bus.writeCalls + 1u;
  bus.failResult = SSD1315::TransportResult::Timeout(-63);
  TEST_ASSERT_FALSE(display.setFlipY(true).ok());
  assertControlStateDirty(display, SSD1315::Err::I2C_TIMEOUT);
  TEST_ASSERT_TRUE(display.getSettings(snap).ok());
  TEST_ASSERT_FALSE(snap.flipY);
  bus.clearTransactions();
  TEST_ASSERT_TRUE(display.recover().ok());
  assertControlStateClean(display);
  TEST_ASSERT_FALSE(display.isDirty());
}

void test_scroll_commands_send_expected_byte_sequences() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(makeConfig(bus)).ok());

  bus.clearTransactions();
  TEST_ASSERT_TRUE(display.startHorizontalScroll(
      true, 2, 5, SSD1315::ScrollSpeed::FRAMES_5).ok());
  const uint8_t hDeactivate[] = {
      SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::SCROLL_DEACTIVATE};
  const uint8_t hSetup[] = {
      SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::SCROLL_LEFT, 0x00, 0x02,
      static_cast<uint8_t>(SSD1315::ScrollSpeed::FRAMES_5), 0x05, 0x00, 0x7F};
  const uint8_t hActivate[] = {
      SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::SCROLL_ACTIVATE};
  TEST_ASSERT_EQUAL_UINT32(3u, static_cast<uint32_t>(bus.transactionCount));
  assertTransactionBytes(bus.transactions[0], hDeactivate, sizeof(hDeactivate));
  assertTransactionBytes(bus.transactions[1], hSetup, sizeof(hSetup));
  assertTransactionBytes(bus.transactions[2], hActivate, sizeof(hActivate));

  bus.clearTransactions();
  TEST_ASSERT_TRUE(display.startVerticalScroll(
      false, 0, 7, SSD1315::ScrollSpeed::FRAMES_64, 1).ok());
  const uint8_t vSetup[] = {
      SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::SCROLL_VERT_RIGHT, 0x01, 0x00,
      static_cast<uint8_t>(SSD1315::ScrollSpeed::FRAMES_64), 0x07, 0x01, 0x00, 0x7F};
  TEST_ASSERT_EQUAL_UINT32(3u, static_cast<uint32_t>(bus.transactionCount));
  assertTransactionBytes(bus.transactions[0], hDeactivate, sizeof(hDeactivate));
  assertTransactionBytes(bus.transactions[1], vSetup, sizeof(vSetup));
  assertTransactionBytes(bus.transactions[2], hActivate, sizeof(hActivate));

  bus.clearTransactions();
  TEST_ASSERT_TRUE(display.startHorizontalScroll(
      false, 0, 0, SSD1315::ScrollSpeed::FRAMES_6).ok());
  const uint8_t hRightSetup[] = {
      SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::SCROLL_RIGHT, 0x00, 0x00,
      static_cast<uint8_t>(SSD1315::ScrollSpeed::FRAMES_6), 0x00, 0x00, 0x7F};
  assertTransactionBytes(bus.transactions[1], hRightSetup, sizeof(hRightSetup));

  bus.clearTransactions();
  TEST_ASSERT_TRUE(display.startVerticalScroll(
      true, 7, 7, SSD1315::ScrollSpeed::FRAMES_2, 63).ok());
  const uint8_t vLeftSetup[] = {
      SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::SCROLL_VERT_LEFT, 0x01, 0x07,
      static_cast<uint8_t>(SSD1315::ScrollSpeed::FRAMES_2), 0x07, 0x3F, 0x00, 0x7F};
  assertTransactionBytes(bus.transactions[1], vLeftSetup, sizeof(vLeftSetup));

  bus.clearTransactions();
  TEST_ASSERT_TRUE(display.stopScroll().ok());
  TEST_ASSERT_EQUAL_UINT32(1u, static_cast<uint32_t>(bus.transactionCount));
  assertTransactionBytes(bus.transactions[0], hDeactivate, sizeof(hDeactivate));

  bus.clearTransactions();
  TEST_ASSERT_TRUE(display.setVerticalScrollArea(0, 64).ok());
  const uint8_t area[] = {
      SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::SET_VERT_SCROLL_AREA, 0x00, 0x40};
  TEST_ASSERT_EQUAL_UINT32(1u, static_cast<uint32_t>(bus.transactionCount));
  assertTransactionBytes(bus.transactions[0], area, sizeof(area));

  bus.clearTransactions();
  TEST_ASSERT_TRUE(display.setVerticalScrollArea(63, 1).ok());
  const uint8_t areaBottom[] = {
      SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::SET_VERT_SCROLL_AREA, 0x3F, 0x01};
  assertTransactionBytes(bus.transactions[0], areaBottom, sizeof(areaBottom));
}

void test_vertical_scroll_offset_valid_for_default_area() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(makeConfig(bus)).ok());
  bus.clearTransactions();

  const SSD1315::Status st =
      display.startVerticalScroll(false, 0, 7, SSD1315::ScrollSpeed::FRAMES_5, 63);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(3u, static_cast<uint32_t>(bus.transactionCount));
}

void test_vertical_scroll_offset_rejected_for_small_scroll_area() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(makeConfig(bus)).ok());
  TEST_ASSERT_TRUE(display.setVerticalScrollArea(0, 16).ok());
  bus.clearTransactions();

  const SSD1315::Status st =
      display.startVerticalScroll(false, 0, 7, SSD1315::ScrollSpeed::FRAMES_5, 16);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(0u, static_cast<uint32_t>(bus.transactionCount));
}

void test_vertical_scroll_invalid_params_preserve_existing_scroll_state() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(makeConfig(bus)).ok());
  TEST_ASSERT_TRUE(display.setVerticalScrollArea(0, 8).ok());
  TEST_ASSERT_TRUE(display.startHorizontalScroll(
      false, 0, 7, SSD1315::ScrollSpeed::FRAMES_6).ok());

  bus.clearTransactions();
  const SSD1315::Status st =
      display.startVerticalScroll(true, 0, 7, SSD1315::ScrollSpeed::FRAMES_6, 8);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(0u, static_cast<uint32_t>(bus.transactionCount));

  SSD1315::SettingsSnapshot snap;
  TEST_ASSERT_TRUE(display.getSettings(snap).ok());
  TEST_ASSERT_TRUE(snap.scrollActive);
  display.setPixel(0, 0, true);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::STATE_ERROR),
                          static_cast<uint8_t>(display.requestFlush().code));
}

void test_set_vertical_scroll_area_validation() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(makeConfig(bus)).ok());
  bus.clearTransactions();

  SSD1315::Status st = display.setVerticalScrollArea(0, 0);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  st = display.setVerticalScrollArea(63, 2);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(0u, static_cast<uint32_t>(bus.transactionCount));
}

void test_non_128_width_scroll_rejected_and_flush_uses_configured_width() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.width = 64;
  cfg.displayOnDelayMs = 0;
  cfg.clearOnBegin = false;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  display.tick(bus.nowMs);

  bus.clearTransactions();
  SSD1315::Status st = display.startHorizontalScroll(
      false, 0, 7, SSD1315::ScrollSpeed::FRAMES_6);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::UNSUPPORTED),
                          static_cast<uint8_t>(st.code));
  st = display.startVerticalScroll(false, 0, 7, SSD1315::ScrollSpeed::FRAMES_6, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::UNSUPPORTED),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(0u, static_cast<uint32_t>(bus.transactionCount));

  display.fill();
  TEST_ASSERT_TRUE(display.startFlush(operationOptions(1)).ok());
  pollOperationUntilTerminal(display, bus, 1, 64);
  SSD1315::OperationResult result;
  takeSuccessfulOperation(display, result);
  const size_t colAddr = requireCommandIndex(bus, SSD1315::cmd::SET_COL_ADDR);
  const uint8_t colWindow[] = {
      SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::SET_COL_ADDR, 0x00, 0x3F};
  assertTransactionBytes(bus.transactions[colAddr], colWindow, sizeof(colWindow));
  TEST_ASSERT_EQUAL_UINT32(512u, countDataPayloadBytes(bus, 0xFF));
}

void test_scroll_failures_mark_control_state_dirty() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(makeConfig(bus)).ok());
  SSD1315::Status st;

  bus.failOnWriteCall = bus.writeCalls + 3u;  // deactivate and setup succeed, activate fails
  bus.failResult = SSD1315::TransportResult::Timeout(-64);
  TEST_ASSERT_FALSE(display.startHorizontalScroll(
      false, 0, 1, SSD1315::ScrollSpeed::FRAMES_5).ok());
  assertControlStateDirty(display, SSD1315::Err::I2C_TIMEOUT);
  TEST_ASSERT_TRUE(display.recover().ok());
  assertControlStateClean(display);

  bus.failOnWriteCall = bus.writeCalls + 1u;
  bus.failResult = SSD1315::TransportResult::BusError(-67);
  TEST_ASSERT_FALSE(display.startVerticalScroll(
      true, 0, 1, SSD1315::ScrollSpeed::FRAMES_6, 1).ok());
  assertControlStateDirty(display, SSD1315::Err::I2C_BUS_ERROR);
  TEST_ASSERT_TRUE(display.recover().ok());
  assertControlStateClean(display);

  bus.clearTransactions();
  bus.failOnWriteCall = bus.writeCalls + 2u;  // deactivate succeeds, setup fails
  bus.failResult = SSD1315::TransportResult::Timeout(-68);
  TEST_ASSERT_FALSE(display.startVerticalScroll(
      true, 0, 1, SSD1315::ScrollSpeed::FRAMES_6, 1).ok());
  assertControlStateDirty(display, SSD1315::Err::I2C_TIMEOUT);
  TEST_ASSERT_FALSE(logContainsCommand(bus, SSD1315::cmd::SCROLL_ACTIVATE));
  TEST_ASSERT_TRUE(display.recover().ok());
  assertControlStateClean(display);

  bus.failOnWriteCall = bus.writeCalls + 3u;  // deactivate and setup succeed, activate fails
  bus.failResult = SSD1315::TransportResult::NackData(-69);
  st = display.startVerticalScroll(true, 0, 1, SSD1315::ScrollSpeed::FRAMES_6, 1);
  TEST_ASSERT_EQUAL_INT32(-69, st.detail);
  TEST_ASSERT_FALSE(st.ok());
  assertControlStateDirty(display, SSD1315::Err::I2C_NACK_DATA);
  TEST_ASSERT_TRUE(display.recover().ok());
  assertControlStateClean(display);

  bus.failOnWriteCall = bus.writeCalls + 1u;
  bus.failResult = SSD1315::TransportResult::NackData(-65);
  TEST_ASSERT_FALSE(display.stopScroll().ok());
  assertControlStateDirty(display, SSD1315::Err::I2C_NACK_DATA);
  TEST_ASSERT_TRUE(display.recover().ok());
  assertControlStateClean(display);

  bus.failOnWriteCall = bus.writeCalls + 1u;
  bus.failResult = SSD1315::TransportResult::BusError(-66);
  TEST_ASSERT_FALSE(display.setVerticalScrollArea(0, 64).ok());
  assertControlStateDirty(display, SSD1315::Err::I2C_BUS_ERROR);
}

void test_scroll_active_blocks_flush_until_stopped_and_marks_dirty() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.displayOnDelayMs = 0;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  display.tick(bus.nowMs);
  display.clearDirty();

  TEST_ASSERT_TRUE(display.startHorizontalScroll(
      false, 0, 7, SSD1315::ScrollSpeed::FRAMES_6).ok());
  display.setPixel(0, 0, true);
  bus.clearTransactions();

  SSD1315::Status st = display.requestFlush();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::STATE_ERROR),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_TRUE(display.isDirty());
  TEST_ASSERT_EQUAL_UINT32(0u, static_cast<uint32_t>(bus.transactionCount));

  TEST_ASSERT_TRUE(display.stopScroll().ok());
  TEST_ASSERT_TRUE(display.isDirty());
  bus.clearTransactions();
  TEST_ASSERT_TRUE(display.requestFlush().ok());
}

void test_control_state_dirty_after_scroll_mid_sequence_failure_and_recover_clears() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(makeConfig(bus)).ok());

  SSD1315::SettingsSnapshot snap;
  TEST_ASSERT_TRUE(display.getSettings(snap).ok());
  TEST_ASSERT_FALSE(snap.controlStateDirty);

  bus.failOnWriteCall = bus.writeCalls + 2u;  // deactivate succeeds, setup list fails
  bus.failResult = SSD1315::TransportResult::Timeout(-44);
  const SSD1315::Status st =
      display.startHorizontalScroll(false, 0, 1, SSD1315::ScrollSpeed::FRAMES_5);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_TRUE(display.getSettings(snap).ok());
  TEST_ASSERT_TRUE(snap.controlStateDirty);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(snap.controlStateError.code));
  display.clearLastError();
  TEST_ASSERT_TRUE(display.lastError().ok());
  TEST_ASSERT_TRUE(display.getSettings(snap).ok());
  TEST_ASSERT_TRUE(snap.controlStateDirty);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(snap.controlStateError.code));

  TEST_ASSERT_TRUE(display.recover().ok());
  TEST_ASSERT_TRUE(display.getSettings(snap).ok());
  TEST_ASSERT_FALSE(snap.controlStateDirty);
  TEST_ASSERT_TRUE(snap.controlStateError.ok());
  TEST_ASSERT_FALSE(display.isDirty());
}

void test_control_state_dirty_survives_invalid_begin_until_successful_resync() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(makeConfig(bus)).ok());

  bus.failOnWriteCall = bus.writeCalls + 1u;
  bus.failResult = SSD1315::TransportResult::NackData(-45);
  TEST_ASSERT_FALSE(display.setContrast(0x22).ok());

  SSD1315::SettingsSnapshot snap;
  TEST_ASSERT_TRUE(display.getSettings(snap).ok());
  TEST_ASSERT_TRUE(snap.controlStateDirty);

  SSD1315::Config bad = makeConfig(bus);
  bad.comPins = static_cast<SSD1315::ComPinsConfig>(0xFF);
  const SSD1315::Status invalid = display.begin(bad);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(invalid.code));
  TEST_ASSERT_TRUE(display.isInitialized());

  TEST_ASSERT_TRUE(display.getSettings(snap).ok());
  TEST_ASSERT_TRUE(snap.controlStateDirty);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::I2C_NACK_DATA),
                          static_cast<uint8_t>(snap.controlStateError.code));

  TEST_ASSERT_TRUE(display.begin(makeConfig(bus)).ok());
  TEST_ASSERT_TRUE(display.getSettings(snap).ok());
  TEST_ASSERT_FALSE(snap.controlStateDirty);
}

void test_clear_on_recover_can_skip_blocking_gddram_clear() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.clearOnRecover = false;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  display.clearDirty();
  bus.clearTransactions();

  const SSD1315::Status st = display.recover();

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(display.isDirty());
  TEST_ASSERT_EQUAL_UINT32(19u, static_cast<uint32_t>(bus.transactionCount));
  TEST_ASSERT_FALSE(logContainsCommand(bus, SSD1315::cmd::SET_COL_ADDR));
  TEST_ASSERT_EQUAL_HEX8(SSD1315::cmd::DISPLAY_ON,
                         bus.transactions[bus.transactionCount - 1].data[1]);
}

void test_default_recover_clears_gddram_before_display_on() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(makeConfig(bus)).ok());
  display.clearDirty();
  bus.clearTransactions();

  const SSD1315::Status st = display.recover();

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FALSE(display.isDirty());
  const size_t colAddr = requireCommandIndex(bus, SSD1315::cmd::SET_COL_ADDR);
  const size_t pageAddr = requireCommandIndex(bus, SSD1315::cmd::SET_PAGE_ADDR);
  const size_t displayOn = requireCommandIndex(bus, SSD1315::cmd::DISPLAY_ON);
  TEST_ASSERT_TRUE(colAddr < pageAddr);
  TEST_ASSERT_TRUE(pageAddr < displayOn);
  TEST_ASSERT_EQUAL_UINT32(1024u, countDataPayloadBytes(bus, 0x00));
}

void test_end_is_best_effort_and_records_display_off_failure() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(makeConfig(bus)).ok());

  bus.failOnWriteCall = bus.writeCalls + 1u;
  bus.failResult = SSD1315::TransportResult::Timeout(-46);
  bus.clearTransactions();
  display.end();

  TEST_ASSERT_FALSE(display.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::DriverState::UNINIT),
                          static_cast<uint8_t>(display.state()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(display.lastError().code));
  TEST_ASSERT_TRUE(bus.transactionCount >= 2u);
  TEST_ASSERT_TRUE(transactionHasCommand(bus.transactions[0], SSD1315::cmd::DISPLAY_OFF));
  TEST_ASSERT_TRUE(transactionHasCommand(bus.transactions[1], SSD1315::cmd::SET_CHARGE_PUMP));

  SSD1315::SettingsSnapshot snap;
  TEST_ASSERT_TRUE(display.getSettings(snap).ok());
  TEST_ASSERT_TRUE(snap.controlStateDirty);
}

void test_end_offline_still_attempts_best_effort_shutdown() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.offlineThreshold = 1;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());

  bus.failWriteRemaining = 1;
  bus.failResult = SSD1315::TransportResult::NackAddress(-71);
  TEST_ASSERT_FALSE(display.sendCommand(SSD1315::cmd::NOP).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::DriverState::OFFLINE),
                          static_cast<uint8_t>(display.state()));
  TEST_ASSERT_EQUAL_UINT32(1u, display.totalFailures());

  bus.clearTransactions();
  display.end();

  TEST_ASSERT_TRUE(bus.transactionCount >= 2u);
  TEST_ASSERT_TRUE(transactionHasCommand(bus.transactions[0], SSD1315::cmd::DISPLAY_OFF));
  TEST_ASSERT_TRUE(transactionHasCommand(bus.transactions[1], SSD1315::cmd::SET_CHARGE_PUMP));
  TEST_ASSERT_EQUAL_UINT32(1u, display.totalFailures());
  TEST_ASSERT_EQUAL_UINT32(0u, display.totalSuccess());
  TEST_ASSERT_FALSE(display.isInitialized());
}

void test_end_is_idempotent_and_clears_transient_state() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.displayOnDelayMs = 0;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  display.tick(bus.nowMs);

  display.fill();
  TEST_ASSERT_TRUE(display.requestFlush().ok());
  TEST_ASSERT_TRUE(display.isFlushing());
  TEST_ASSERT_TRUE(display.isDirty());

  display.end();

  SSD1315::SettingsSnapshot snap;
  TEST_ASSERT_TRUE(display.getSettings(snap).ok());
  TEST_ASSERT_FALSE(snap.initialized);
  TEST_ASSERT_FALSE(snap.flushing);
  TEST_ASSERT_FALSE(snap.pageIterationActive);
  TEST_ASSERT_EQUAL_UINT8(0u, snap.dirtyPages);
  TEST_ASSERT_EQUAL_UINT32(0u, static_cast<uint32_t>(snap.bufferSize));
  TEST_ASSERT_TRUE(snap.sleeping);

  const uint32_t writesAfterFirstEnd = bus.writeCalls;
  display.end();
  TEST_ASSERT_EQUAL_UINT32(writesAfterFirstEnd, bus.writeCalls);
}

void test_end_disables_internal_charge_pump_after_display_off() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(makeConfig(bus)).ok());
  bus.clearTransactions();

  display.end();

  TEST_ASSERT_TRUE(bus.transactionCount >= 2u);
  const uint8_t displayOff[] = {SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::DISPLAY_OFF};
  const uint8_t pumpOff[] = {
      SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::SET_CHARGE_PUMP,
      static_cast<uint8_t>(SSD1315::ChargePumpVoltage::OFF)};
  assertTransactionBytes(bus.transactions[0], displayOff, sizeof(displayOff));
  assertTransactionBytes(bus.transactions[1], pumpOff, sizeof(pumpOff));

  bus = FakeBus{};
  SSD1315::Config cfg = makeConfig(bus);
  cfg.chargePumpVoltage = SSD1315::ChargePumpVoltage::OFF;
  SSD1315::SSD1315 externalDisplay;
  TEST_ASSERT_TRUE(externalDisplay.begin(cfg).ok());
  bus.clearTransactions();
  externalDisplay.end();
  TEST_ASSERT_EQUAL_UINT32(1u, static_cast<uint32_t>(bus.transactionCount));
  assertTransactionBytes(bus.transactions[0], displayOff, sizeof(displayOff));
}

void test_zoom_enable_requires_alternative_com_pins_without_i2c() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.comPins = SSD1315::ComPinsConfig::SEQUENTIAL_NO_REMAP;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());

  const uint32_t writesBefore = bus.writeCalls;
  const uint32_t successBefore = display.totalSuccess();
  const SSD1315::Status st = display.setZoom(true);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(successBefore, display.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(0u, display.totalFailures());
}

void test_wait_flush_returns_timeout_when_time_source_stalls() {
  FakeBus bus;
  bus.advanceNowOnYield = false;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.displayOnDelayMs = 100;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());

  const SSD1315::Status st = display.waitFlush(bus.nowMs, 2);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_GREATER_THAN_UINT32(0u, bus.yieldCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, display.totalFailures());
}

void test_wait_flush_without_clock_hook_uses_caller_time() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.nowMs = nullptr;
  cfg.cooperativeYield = nullptr;
  cfg.timeUser = nullptr;
  cfg.displayOnDelayMs = 0;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());

  display.setPixel(0, 0, true);
  TEST_ASSERT_TRUE(display.requestFlush().ok());
  const SSD1315::Status st = display.waitFlush(100, 1000);

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FALSE(display.isFlushing());
  TEST_ASSERT_FALSE(display.isDirty());
  TEST_ASSERT_EQUAL_UINT32(2u, display.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(0u, display.totalFailures());
}

void test_flush_error_preserves_dirty_flags_and_updates_health_once() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.displayOnDelayMs = 0;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  display.tick(bus.nowMs);

  display.setPixel(0, 0, true);
  TEST_ASSERT_TRUE(display.isDirty());
  TEST_ASSERT_TRUE(display.requestFlush().ok());
  TEST_ASSERT_TRUE(display.isFlushing());

  bus.failWriteRemaining = 1;
  bus.failResult = SSD1315::TransportResult::Timeout(-22);
  display.tick(bus.nowMs);
  TEST_ASSERT_TRUE(display.isDirty());
  TEST_ASSERT_FALSE(display.isFlushing());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(display.lastError().code));
  TEST_ASSERT_EQUAL_UINT32(1u, display.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(1u, display.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::DriverState::DEGRADED),
                          static_cast<uint8_t>(display.state()));

  display.tick(bus.nowMs);
  TEST_ASSERT_TRUE(display.isDirty());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(display.lastError().code));
  TEST_ASSERT_EQUAL_UINT32(1u, display.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(1u, display.consecutiveFailures());
}

void test_clear_dirty_if_idle_preserves_active_and_failed_retry_state() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.displayOnDelayMs = 0;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  display.tick(bus.nowMs);

  display.setPixel(0, 0, true);
  TEST_ASSERT_TRUE(display.requestFlush().ok());
  SSD1315::Status st = display.clearDirtyIfIdle();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::BUSY),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_TRUE(display.isDirty());

  bus.failWriteRemaining = 1;
  bus.failResult = SSD1315::TransportResult::Timeout(-58);
  st = display.pollFlush(bus.nowMs, 1, 16);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_TRUE(display.isDirty());

  st = display.clearDirtyIfIdle();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::STATE_ERROR),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_TRUE(display.isDirty());

  display.clearDirty();
  TEST_ASSERT_FALSE(display.isDirty());
  st = display.clearDirtyIfIdle();
  TEST_ASSERT_TRUE_MESSAGE(st.ok(), st.msg);
}

void test_flush_retry_replays_failed_dirty_byte() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.displayOnDelayMs = 0;
  cfg.byteBudgetPerTick = 16;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  display.tick(bus.nowMs);

  display.setPixel(0, 0, true);
  TEST_ASSERT_TRUE(display.requestFlush().ok());
  display.tick(bus.nowMs);  // column address window
  display.tick(bus.nowMs);  // page address window

  bus.failWriteRemaining = 1;
  bus.failResult = SSD1315::TransportResult::Timeout(-55);
  display.tick(bus.nowMs);  // data byte fails
  TEST_ASSERT_TRUE(display.isDirty());
  display.tick(bus.nowMs);  // consume ERROR state into health

  bus.clearTransactions();
  TEST_ASSERT_TRUE(display.requestFlush().ok());
  display.tick(bus.nowMs);  // column address window
  display.tick(bus.nowMs);  // page address window
  display.tick(bus.nowMs);  // retry data byte

  TEST_ASSERT_TRUE(bus.transactionCount >= 3u);
  TEST_ASSERT_EQUAL_HEX8(SSD1315::cmd::CTRL_DATA, bus.transactions[2].data[0]);
  TEST_ASSERT_EQUAL_UINT32(2u, static_cast<uint32_t>(bus.transactions[2].len));
  TEST_ASSERT_EQUAL_HEX8(0x01, bus.transactions[2].data[1]);
  TEST_ASSERT_FALSE(display.isDirty());
}

void test_flush_error_reaches_offline_immediately_when_threshold_is_one() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.displayOnDelayMs = 0;
  cfg.offlineThreshold = 1;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  display.tick(bus.nowMs);

  display.setPixel(0, 0, true);
  TEST_ASSERT_TRUE(display.requestFlush().ok());
  display.tick(bus.nowMs);  // column address window
  display.tick(bus.nowMs);  // page address window

  bus.failWriteRemaining = 1;
  bus.failResult = SSD1315::TransportResult::Timeout(-56);
  display.tick(bus.nowMs);

  TEST_ASSERT_TRUE(display.isDirty());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::DriverState::OFFLINE),
                          static_cast<uint8_t>(display.state()));
  TEST_ASSERT_EQUAL_UINT32(1u, display.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(1u, display.consecutiveFailures());

  display.tick(bus.nowMs);
  TEST_ASSERT_EQUAL_UINT32(1u, display.totalFailures());
}

void test_clear_after_fill_flush_sends_zero_payload() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.displayOnDelayMs = 0;
  cfg.byteBudgetPerTick = 128;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  display.tick(bus.nowMs);

  bus.clearTransactions();
  display.fill();
  TEST_ASSERT_TRUE(display.requestFlush().ok());
  drainFlush(display, bus);
  TEST_ASSERT_FALSE(display.isDirty());
  TEST_ASSERT_EQUAL_UINT32(16u, countDataTransactions(bus));
  TEST_ASSERT_EQUAL_UINT32(1024u, countDataPayloadBytes(bus, 0xFF));

  bus.clearTransactions();
  display.clear();
  TEST_ASSERT_TRUE(display.requestFlush().ok());
  drainFlush(display, bus);
  TEST_ASSERT_FALSE(display.isDirty());
  TEST_ASSERT_EQUAL_UINT32(16u, countDataTransactions(bus));
  TEST_ASSERT_EQUAL_UINT32(1024u, countDataPayloadBytes(bus, 0x00));
}

void test_active_flush_mutation_keeps_dirty_and_retry_sends_current_framebuffer() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.displayOnDelayMs = 0;
  cfg.byteBudgetPerTick = 64;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  display.tick(bus.nowMs);

  bus.clearTransactions();
  display.fill();
  TEST_ASSERT_TRUE(display.requestFlush().ok());
  display.tick(bus.nowMs);  // column address page 0
  display.tick(bus.nowMs);  // page address page 0
  display.tick(bus.nowMs);  // send columns 0..63 as 0xFF
  TEST_ASSERT_EQUAL_UINT32(64u, countDataPayloadBytes(bus, 0xFF));

  display.clear();
  drainFlush(display, bus);
  TEST_ASSERT_TRUE(display.isDirty());

  bus.clearTransactions();
  TEST_ASSERT_TRUE(display.requestFlush().ok());
  drainFlush(display, bus);
  TEST_ASSERT_FALSE(display.isDirty());
  TEST_ASSERT_EQUAL_UINT32(2u, countDataTransactions(bus));
  TEST_ASSERT_EQUAL_UINT32(128u, countDataPayloadBytes(bus, 0x00));
}

void test_failed_partial_flush_retry_uses_current_framebuffer() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.displayOnDelayMs = 0;
  cfg.byteBudgetPerTick = 64;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  display.tick(bus.nowMs);

  display.fill();
  TEST_ASSERT_TRUE(display.requestFlush().ok());
  display.tick(bus.nowMs);  // column address page 0
  display.tick(bus.nowMs);  // page address page 0

  bus.failWriteRemaining = 1;
  bus.failResult = SSD1315::TransportResult::Timeout(-72);
  display.tick(bus.nowMs);  // first data chunk fails
  TEST_ASSERT_TRUE(display.isDirty());

  display.clear();
  bus.clearTransactions();
  TEST_ASSERT_TRUE(display.requestFlush().ok());
  drainFlush(display, bus);

  TEST_ASSERT_FALSE(display.isDirty());
  TEST_ASSERT_EQUAL_UINT32(16u, countDataTransactions(bus));
  TEST_ASSERT_EQUAL_UINT32(1024u, countDataPayloadBytes(bus, 0x00));
}

void test_full_frame_flush_transaction_count_and_chunking() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.displayOnDelayMs = 0;
  cfg.byteBudgetPerTick = 128;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  display.tick(bus.nowMs);

  bus.clearTransactions();
  display.fill();
  TEST_ASSERT_TRUE(display.requestFlush().ok());
  for (uint8_t i = 0; i < 40 && display.isFlushing(); ++i) {
    display.tick(bus.nowMs);
  }

  TEST_ASSERT_FALSE(display.isFlushing());
  TEST_ASSERT_FALSE(display.isDirty());
  TEST_ASSERT_EQUAL_UINT32(32u, static_cast<uint32_t>(bus.transactionCount));

  uint32_t dataTransactions = 0;
  uint8_t expectedPage = 0;
  for (size_t i = 0; i < bus.transactionCount; ++i) {
    if (bus.transactions[i].len > 0 &&
        bus.transactions[i].data[0] == SSD1315::cmd::CTRL_DATA) {
      dataTransactions++;
      TEST_ASSERT_EQUAL_UINT32(65u, static_cast<uint32_t>(bus.transactions[i].len));
      for (size_t b = 1; b < bus.transactions[i].len; ++b) {
        TEST_ASSERT_EQUAL_HEX8(0xFF, bus.transactions[i].data[b]);
      }
    }
  }
  TEST_ASSERT_EQUAL_UINT32(16u, dataTransactions);

  for (size_t i = 0; i < bus.transactionCount; i += 4) {
    const uint8_t colAddr[] = {
        SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::SET_COL_ADDR, 0, 127};
    const uint8_t pageAddr[] = {
        SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::SET_PAGE_ADDR, expectedPage, expectedPage};
    assertTransactionBytes(bus.transactions[i], colAddr, sizeof(colAddr));
    assertTransactionBytes(bus.transactions[i + 1], pageAddr, sizeof(pageAddr));
    TEST_ASSERT_EQUAL_HEX8(SSD1315::cmd::CTRL_DATA, bus.transactions[i + 2].data[0]);
    TEST_ASSERT_EQUAL_HEX8(SSD1315::cmd::CTRL_DATA, bus.transactions[i + 3].data[0]);
    expectedPage++;
  }
  TEST_ASSERT_EQUAL_UINT8(8u, expectedPage);
}

void test_poll_flush_address_window_uses_one_instruction_per_poll() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.displayOnDelayMs = 0;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  display.tick(bus.nowMs);

  display.setPixel(3, 0, true);
  TEST_ASSERT_TRUE(display.requestFlush().ok());
  const uint32_t successBefore = display.totalSuccess();
  SSD1315::FlushStatus snap = display.getFlushStatus();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::FlushPhase::SET_COL_ADDR),
                          static_cast<uint8_t>(snap.phase));

  bus.clearTransactions();
  SSD1315::Status st = display.pollFlush(bus.nowMs, 1, 16);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::IN_PROGRESS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(1u, static_cast<uint32_t>(bus.transactionCount));
  const uint8_t colAddr[] = {
      SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::SET_COL_ADDR, 3, 3};
  assertTransactionBytes(bus.transactions[0], colAddr, sizeof(colAddr));

  snap = display.getFlushStatus();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::FlushPhase::SET_PAGE_ADDR),
                          static_cast<uint8_t>(snap.phase));

  st = display.pollFlush(bus.nowMs, 1, 16);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::IN_PROGRESS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(2u, static_cast<uint32_t>(bus.transactionCount));
  const uint8_t pageAddr[] = {
      SSD1315::cmd::CTRL_COMMAND, SSD1315::cmd::SET_PAGE_ADDR, 0, 0};
  assertTransactionBytes(bus.transactions[1], pageAddr, sizeof(pageAddr));

  snap = display.getFlushStatus();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::FlushPhase::SEND_DATA),
                          static_cast<uint8_t>(snap.phase));

  st = display.pollFlush(bus.nowMs, 1, 16);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(3u, static_cast<uint32_t>(bus.transactionCount));
  TEST_ASSERT_EQUAL_HEX8(SSD1315::cmd::CTRL_DATA, bus.transactions[2].data[0]);
  TEST_ASSERT_EQUAL_UINT32(2u, static_cast<uint32_t>(bus.transactions[2].len));
  TEST_ASSERT_EQUAL_HEX8(0x01, bus.transactions[2].data[1]);
  TEST_ASSERT_FALSE(display.isDirty());
  TEST_ASSERT_EQUAL_UINT32(successBefore + 1u, display.totalSuccess());

  snap = display.getFlushStatus();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::FlushPhase::DONE),
                          static_cast<uint8_t>(snap.phase));
}

void test_poll_flush_byte_budget_limits_data_with_instruction_headroom() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.displayOnDelayMs = 0;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  display.tick(bus.nowMs);

  display.drawHLine(0, 0, 32, true);
  TEST_ASSERT_TRUE(display.requestFlush().ok());
  bus.clearTransactions();

  SSD1315::Status st = display.pollFlush(bus.nowMs, 3, 8);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::IN_PROGRESS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(3u, static_cast<uint32_t>(bus.transactionCount));
  TEST_ASSERT_EQUAL_UINT32(1u, countDataTransactions(bus));
  TEST_ASSERT_EQUAL_UINT32(8u, countDataPayloadBytes(bus, 0x00, false));
  SSD1315::FlushStatus snap = display.getFlushStatus();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::FlushPhase::SEND_DATA),
                          static_cast<uint8_t>(snap.phase));
  TEST_ASSERT_EQUAL_UINT32(8u, static_cast<uint32_t>(snap.currentColumn));

  st = display.pollFlush(bus.nowMs, 5, 8);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::IN_PROGRESS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(4u, static_cast<uint32_t>(bus.transactionCount));
  TEST_ASSERT_EQUAL_UINT32(2u, countDataTransactions(bus));
  TEST_ASSERT_EQUAL_UINT32(16u, countDataPayloadBytes(bus, 0x00, false));
  snap = display.getFlushStatus();
  TEST_ASSERT_EQUAL_UINT32(16u, static_cast<uint32_t>(snap.currentColumn));
}

void test_poll_flush_instruction_budget_limits_data_transactions() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.displayOnDelayMs = 0;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  display.tick(bus.nowMs);

  display.fill();
  display.clearDirty();
  display.markDirty(0, 0, 127);
  TEST_ASSERT_TRUE(display.requestFlush().ok());
  bus.clearTransactions();

  SSD1315::Status st = display.pollFlush(bus.nowMs, 3, 128);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::IN_PROGRESS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(3u, static_cast<uint32_t>(bus.transactionCount));
  TEST_ASSERT_EQUAL_UINT32(1u, countDataTransactions(bus));
  TEST_ASSERT_EQUAL_UINT32(64u, countDataPayloadBytes(bus, 0xFF));
  SSD1315::FlushStatus snap = display.getFlushStatus();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::FlushPhase::SEND_DATA),
                          static_cast<uint8_t>(snap.phase));
  TEST_ASSERT_EQUAL_UINT32(64u, static_cast<uint32_t>(snap.currentColumn));

  st = display.pollFlush(bus.nowMs, 1, 128);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(4u, static_cast<uint32_t>(bus.transactionCount));
  TEST_ASSERT_EQUAL_UINT32(2u, countDataTransactions(bus));
  TEST_ASSERT_EQUAL_UINT32(128u, countDataPayloadBytes(bus, 0xFF));
  TEST_ASSERT_FALSE(display.isDirty());
}

void test_poll_flush_timeout_includes_display_on_delay_gate() {
  FakeBus bus;
  bus.nowMs = 100;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.displayOnDelayMs = 100;
  cfg.flushTimeoutMs = 50;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());

  display.setPixel(0, 0, true);
  TEST_ASSERT_TRUE(display.requestFlush().ok());
  bus.clearTransactions();

  SSD1315::Status st = display.pollFlush(bus.nowMs, 1, 16);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::IN_PROGRESS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(0u, static_cast<uint32_t>(bus.transactionCount));

  bus.nowMs = 150;
  st = display.pollFlush(bus.nowMs, 1, 16);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(0u, static_cast<uint32_t>(bus.transactionCount));
  TEST_ASSERT_FALSE(display.isFlushing());
  TEST_ASSERT_TRUE(display.isDirty());
  TEST_ASSERT_EQUAL_UINT32(1u, display.totalFailures());
  SSD1315::FlushStatus snap = display.getFlushStatus();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::FlushPhase::ERROR),
                          static_cast<uint8_t>(snap.phase));

  st = display.pollFlush(bus.nowMs, 1, 16);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(1u, display.totalFailures());
  TEST_ASSERT_TRUE(display.isDirty());
}

void test_poll_flush_zero_instruction_queries_do_not_touch_i2c_or_advance() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.displayOnDelayMs = 0;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  display.tick(bus.nowMs);

  display.setPixel(9, 8, true);
  TEST_ASSERT_TRUE(display.requestFlush().ok());

  SSD1315::FlushStatus before = display.getFlushStatus();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::FlushPhase::SET_COL_ADDR),
                          static_cast<uint8_t>(before.phase));
  TEST_ASSERT_TRUE(display.isDirty());

  static const uint16_t queryBudgets[] = {0, 16};
  for (uint8_t i = 0; i < 10; ++i) {
    bus.clearTransactions();
    SSD1315::Status st = display.pollFlush(bus.nowMs, 0,
                                           queryBudgets[i % 2u]);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::IN_PROGRESS),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_UINT32(0u, static_cast<uint32_t>(bus.transactionCount));
    TEST_ASSERT_FALSE(bus.transactionOverflow);

    SSD1315::FlushStatus after = display.getFlushStatus();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(before.phase),
                            static_cast<uint8_t>(after.phase));
    TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(before.currentPage),
                             static_cast<uint32_t>(after.currentPage));
    TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(before.currentColumn),
                             static_cast<uint32_t>(after.currentColumn));
    TEST_ASSERT_TRUE(display.isDirty());
  }

  bus.clearTransactions();
  SSD1315::Status st = display.pollFlush(bus.nowMs, 1, 0);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(0u, static_cast<uint32_t>(bus.transactionCount));

  bus.clearTransactions();
  st = display.pollFlush(bus.nowMs, 1, 16);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::IN_PROGRESS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(1u, static_cast<uint32_t>(bus.transactionCount));
  SSD1315::FlushStatus progressed = display.getFlushStatus();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::FlushPhase::SET_PAGE_ADDR),
                          static_cast<uint8_t>(progressed.phase));
}

void test_poll_flush_budget_stress_matrix_preserves_dirty_and_completes() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.displayOnDelayMs = 0;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  display.tick(bus.nowMs);

  display.clear();
  display.clearDirty();
  display.drawHLine(-7, 0, 48, true);
  display.drawVLine(127, -4, 72, true);
  display.drawRect(2, 9, 23, 29, true);
  display.fillRect(61, 17, 19, 23, true);
  display.drawLine(-20, 63, 147, -8, true);
  display.drawCircle(96, 32, 14, true);
  display.fillCircle(15, 53, 9, true);
  TEST_ASSERT_TRUE(display.requestFlush().ok());
  const uint32_t successBefore = display.totalSuccess();

  static const uint8_t instructionBudgets[] = {0, 1, 2, 3, 4, 7, 255};
  static const uint16_t byteBudgets[] = {1, 2, 3, 7, 8, 15, 16, 31,
                                         32, 63, 64, 65, 127, 128, 255};

  bool completed = false;
  for (uint16_t i = 0; i < 900 && !completed; ++i) {
    const uint8_t maxInstructions =
        instructionBudgets[i % (sizeof(instructionBudgets) / sizeof(instructionBudgets[0]))];
    const uint16_t byteBudget =
        byteBudgets[(i / 3) % (sizeof(byteBudgets) / sizeof(byteBudgets[0]))];

    bus.clearTransactions();
    SSD1315::Status st = display.pollFlush(bus.nowMs + i, maxInstructions, byteBudget);
    TEST_ASSERT_TRUE(st.ok() || st.inProgress());
    TEST_ASSERT_FALSE(bus.transactionOverflow);
    TEST_ASSERT_TRUE(bus.transactionCount <= maxInstructions);
    TEST_ASSERT_TRUE(countDataPayloadBytes(bus, 0x00, false) <= byteBudget);
    for (size_t txIndex = 0; txIndex < bus.transactionCount; ++txIndex) {
      const FakeBus::Transaction& tx = bus.transactions[txIndex];
      if (tx.len > 0 && tx.data[0] == SSD1315::cmd::CTRL_DATA) {
        TEST_ASSERT_TRUE(tx.len <= 65u);
      }
    }
    if (st.ok() && !display.isFlushing()) {
      completed = true;
    }
  }

  TEST_ASSERT_TRUE(completed);
  TEST_ASSERT_FALSE(display.isDirty());
  TEST_ASSERT_EQUAL_UINT32(successBefore + 1u, display.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(0u, display.totalFailures());
}

void test_poll_flush_page_address_failure_preserves_dirty_and_retries() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.displayOnDelayMs = 0;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  display.tick(bus.nowMs);

  display.setPixel(4, 16, true);
  TEST_ASSERT_TRUE(display.requestFlush().ok());
  bus.clearTransactions();
  bus.failOnWriteCall = bus.writeCalls + 2u;
  bus.failResult = SSD1315::TransportResult::Timeout(-23);

  SSD1315::Status st = display.pollFlush(bus.nowMs, 3, 64);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-23, st.detail);
  TEST_ASSERT_EQUAL_UINT32(2u, static_cast<uint32_t>(bus.transactionCount));
  TEST_ASSERT_TRUE(display.isDirty());
  TEST_ASSERT_FALSE(display.isFlushing());
  TEST_ASSERT_EQUAL_UINT32(1u, display.totalFailures());
  SSD1315::FlushStatus failed = display.getFlushStatus();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::FlushPhase::ERROR),
                          static_cast<uint8_t>(failed.phase));

  bus.clearTransactions();
  TEST_ASSERT_TRUE(display.requestFlush().ok());
  for (uint8_t i = 0; i < 10 && display.isFlushing(); ++i) {
    st = display.pollFlush(bus.nowMs + i, 1, 1);
    TEST_ASSERT_TRUE(st.ok() || st.inProgress());
  }

  TEST_ASSERT_FALSE(display.isFlushing());
  TEST_ASSERT_FALSE(display.isDirty());
  TEST_ASSERT_EQUAL_UINT32(1u, countDataPayloadBytes(bus, 0x01));
}

void test_checked_draw_bitmap_exact_size_draws_and_marks_dirty() {
  FakeBus bus;
  uint8_t storage[128 * 8] = {};
  SSD1315::Config cfg = makeConfig(bus);
  cfg.externalBuffer = storage;
  cfg.externalBufferSizeBytes = sizeof(storage);
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  display.clearDirty();

  const uint8_t bitmap[8] = {
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80};
  const SSD1315::Status st = display.drawBitmap(0, 0, bitmap, 8, 8,
                                                sizeof(bitmap), true);

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(display.isDirty());
  TEST_ASSERT_EQUAL_HEX8(0xFF, storage[0]);
}

void test_checked_draw_bitmap_rejects_undersized_source_without_dirty() {
  FakeBus bus;
  uint8_t storage[128 * 8] = {};
  SSD1315::Config cfg = makeConfig(bus);
  cfg.externalBuffer = storage;
  cfg.externalBufferSizeBytes = sizeof(storage);
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  display.clearDirty();

  const uint8_t bitmap[7] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  const SSD1315::Status st = display.drawBitmap(0, 0, bitmap, 8, 8,
                                                sizeof(bitmap), true);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::BUFFER_TOO_SMALL),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(display.isDirty());
  TEST_ASSERT_EQUAL_HEX8(0x00, storage[0]);
}

void test_checked_draw_bitmap_clipped_offscreen_reads_validated_bounds_only() {
  FakeBus bus;
  uint8_t storage[128 * 8] = {};
  SSD1315::Config cfg = makeConfig(bus);
  cfg.externalBuffer = storage;
  cfg.externalBufferSizeBytes = sizeof(storage);
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  display.clearDirty();

  const uint8_t bitmap[8] = {
      0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01};
  const SSD1315::Status st = display.drawBitmap(-7, 0, bitmap, 8, 8,
                                                sizeof(bitmap), true);

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(display.isDirty());
  TEST_ASSERT_EQUAL_HEX8(0xFF, storage[0]);
}

void test_out_of_bounds_draws_preserve_external_buffer_guards() {
  FakeBus bus;
  uint8_t storage[1026] = {};
  storage[0] = 0xA5;
  storage[1025] = 0x5A;

  SSD1315::Config cfg = makeConfig(bus);
  cfg.externalBuffer = &storage[1];
  cfg.externalBufferSizeBytes = 1024;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());

  display.setPixel(-1, 0, true);
  display.setPixel(128, 63, true);
  display.drawHLine(-20, -1, 10, true);
  display.drawVLine(130, 0, 20, true);
  display.fillRect(200, 200, 20, 20, true);
  display.drawLine(-200, -200, -100, -100, true);

  TEST_ASSERT_EQUAL_HEX8(0xA5, storage[0]);
  TEST_ASSERT_EQUAL_HEX8(0x5A, storage[1025]);
}

void test_hostile_drawing_and_flush_rect_stress_preserves_external_buffer_guards() {
  FakeBus bus;
  uint8_t storage[1040] = {};
  for (uint8_t i = 0; i < 8; ++i) {
    storage[i] = static_cast<uint8_t>(0xA0u + i);
    storage[1032 + i] = static_cast<uint8_t>(0x50u + i);
  }

  SSD1315::Config cfg = makeConfig(bus);
  cfg.displayOnDelayMs = 0;
  cfg.externalBuffer = &storage[8];
  cfg.externalBufferSizeBytes = 1024;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  display.tick(bus.nowMs);

  uint32_t seed = 0x1315ACEu;
  for (uint16_t i = 0; i < 240; ++i) {
    seed = seed * 1664525u + 1013904223u;
    const int16_t x = static_cast<int16_t>(static_cast<int32_t>((seed >> 16) % 250u) - 60);
    seed = seed * 1664525u + 1013904223u;
    const int16_t y = static_cast<int16_t>(static_cast<int32_t>((seed >> 16) % 180u) - 58);
    seed = seed * 1664525u + 1013904223u;
    const int16_t w = static_cast<int16_t>(static_cast<int32_t>((seed >> 16) % 190u) - 31);
    seed = seed * 1664525u + 1013904223u;
    const int16_t h = static_cast<int16_t>(static_cast<int32_t>((seed >> 16) % 140u) - 27);
    const bool on = (seed & 0x8000u) != 0;

    switch (i % 9u) {
      case 0:
        display.setPixel(x, y, on);
        break;
      case 1:
        display.drawHLine(x, y, w, on);
        break;
      case 2:
        display.drawVLine(x, y, h, on);
        break;
      case 3:
        display.drawRect(x, y, w, h, on);
        break;
      case 4:
        display.fillRect(x, y, w, h, on);
        break;
      case 5:
        display.drawLine(x, y, static_cast<int16_t>(x + w), static_cast<int16_t>(y + h),
                         on);
        break;
      case 6:
        display.drawCircle(x, y, static_cast<int16_t>(w % 38), on);
        break;
      case 7:
        display.fillCircle(x, y, static_cast<int16_t>(h % 32), on);
        break;
      default:
        {
          SSD1315::Status st = display.requestFlushRect(x, y, w, h);
          TEST_ASSERT_TRUE(st.ok() || st.is(SSD1315::Err::BUSY));
          for (uint8_t flushPoll = 0; flushPoll < 20 && display.isFlushing(); ++flushPoll) {
            st = display.pollFlush(bus.nowMs + i + flushPoll, 8, 128);
            TEST_ASSERT_TRUE(st.ok() || st.inProgress());
          }
          TEST_ASSERT_FALSE(display.isFlushing());
        }
        bus.clearTransactions();
        display.clearDirty();
        break;
    }

    if ((i % 17u) == 0u) {
      display.markDirty(static_cast<uint8_t>(i % 16u),
                        static_cast<uint8_t>((i * 11u) & 0xFFu),
                        static_cast<uint8_t>((i * 7u) & 0xFFu));
    }

    for (uint8_t g = 0; g < 8; ++g) {
      TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(0xA0u + g), storage[g]);
      TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(0x50u + g), storage[1032 + g]);
    }
    TEST_ASSERT_FALSE(display.getPixel(-1, y));
    TEST_ASSERT_FALSE(display.getPixel(128, y));
  }
}

void test_long_text_does_not_wrap_back_into_visible_buffer() {
  FakeBus bus;
  uint8_t storage[128 * 8] = {};
  SSD1315::Config cfg = makeConfig(bus);
  cfg.externalBuffer = storage;
  cfg.externalBufferSizeBytes = sizeof(storage);
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  display.clearDirty();

  static char longText[12050];
  memset(longText, 'A', sizeof(longText) - 1);
  longText[sizeof(longText) - 1] = '\0';

  const int16_t endX = display.drawText(129, 0, longText);

  TEST_ASSERT_EQUAL_INT16(129 + 512 * 6, endX);
  TEST_ASSERT_FALSE(display.isDirty());
  for (size_t i = 0; i < sizeof(storage); ++i) {
    TEST_ASSERT_EQUAL_HEX8(0x00, storage[i]);
  }
  TEST_ASSERT_EQUAL_INT16(512 * 6, SSD1315::SSD1315::getTextWidth(longText));
}

void test_draw_char_updates_activity_and_reports_wake_failure() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.displayOnDelayMs = 0;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());
  TEST_ASSERT_TRUE(display.setSleep(true).ok());

  bus.failOnWriteCall = bus.writeCalls + 1u;
  bus.failResult = SSD1315::TransportResult::NackData(-81);

  display.drawChar(0, 0, 'A');

  SSD1315::SettingsSnapshot snap;
  TEST_ASSERT_TRUE(display.getSettings(snap).ok());
  TEST_ASSERT_TRUE(snap.controlStateDirty);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::I2C_NACK_DATA),
                          static_cast<uint8_t>(display.lastError().code));
}

void test_auto_sleep_timer_handles_wraparound() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(makeConfig(bus)).ok());

  display.setAutoSleep(5);
  bus.nowMs = 0xFFFFFFFEu;
  display.touch();
  TEST_ASSERT_FALSE(display.isSleeping());

  // Inactivity elapsed across wrap is exactly 5 ms.
  bus.nowMs = 3u;
  display.tick(bus.nowMs);
  TEST_ASSERT_TRUE(display.isSleeping());
}

void test_tick_zero_does_not_bypass_display_on_delay() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.displayOnDelayMs = 100;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());

  display.setPixel(0, 0, true);
  TEST_ASSERT_TRUE(display.requestFlush().ok());
  bus.clearTransactions();

  display.tick(0);
  TEST_ASSERT_EQUAL_UINT32(0u, static_cast<uint32_t>(bus.transactionCount));
  display.tick(99);
  TEST_ASSERT_EQUAL_UINT32(0u, static_cast<uint32_t>(bus.transactionCount));
  display.tick(100);
  TEST_ASSERT_TRUE(bus.transactionCount > 0u);
}

void test_display_on_delay_releases_after_elapsed_time() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.displayOnDelayMs = 25;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());

  display.fill();
  TEST_ASSERT_TRUE(display.requestFlush().ok());
  bus.clearTransactions();

  display.tick(10);
  TEST_ASSERT_EQUAL_UINT32(0u, static_cast<uint32_t>(bus.transactionCount));
  display.tick(34);
  TEST_ASSERT_EQUAL_UINT32(0u, static_cast<uint32_t>(bus.transactionCount));
  display.tick(35);
  TEST_ASSERT_TRUE(bus.transactionCount > 0u);
}

void test_display_on_delay_zero_is_immediate() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.displayOnDelayMs = 0;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());

  display.fill();
  TEST_ASSERT_TRUE(display.requestFlush().ok());
  bus.clearTransactions();
  display.tick(0);
  TEST_ASSERT_TRUE(bus.transactionCount > 0u);
}

void test_display_on_delay_wraparound_safe() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.displayOnDelayMs = 10;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(cfg).ok());

  display.fill();
  TEST_ASSERT_TRUE(display.requestFlush().ok());
  bus.clearTransactions();

  display.tick(0xFFFFFFFEu);
  TEST_ASSERT_EQUAL_UINT32(0u, static_cast<uint32_t>(bus.transactionCount));
  display.tick(6u);
  TEST_ASSERT_EQUAL_UINT32(0u, static_cast<uint32_t>(bus.transactionCount));
  display.tick(8u);
  TEST_ASSERT_TRUE(bus.transactionCount > 0u);
}

void test_v4_types_are_fixed_trivial_and_noexcept() {
  static_assert(std::is_same<decltype(&fakeI2cWrite),
                             SSD1315::I2cWriteFn>::value,
                "transport callback must return terminal TransportResult");
  static_assert(std::is_trivially_copyable<SSD1315::TransportResult>::value,
                "transport result must be trivially copyable");
  static_assert(std::is_trivially_copyable<SSD1315::OperationOptions>::value,
                "operation options must be trivially copyable");
  static_assert(std::is_trivially_copyable<SSD1315::OperationProgress>::value,
                "operation progress must be trivially copyable");
  static_assert(sizeof(SSD1315::TransportResult) <= 8,
                "transport result must not carry transient message storage");
  static_assert(sizeof(SSD1315::OperationOptions) <= 12,
                "operation options must remain fixed and compact");
  static_assert(sizeof(SSD1315::OperationProgress) <= 64,
                "operation progress must remain fixed and compact");
  static_assert(std::is_nothrow_destructible<SSD1315::SSD1315>::value,
                "driver destruction must be noexcept");
  static_assert(!std::is_copy_constructible<SSD1315::SSD1315>::value,
                "driver must remain non-copyable");
  static_assert(!std::is_move_constructible<SSD1315::SSD1315>::value,
                "driver must remain non-movable");

  TEST_ASSERT_EQUAL_UINT32(1024,
      static_cast<uint32_t>(SSD1315::requiredFramebufferBytes(128, 8)));
  TEST_ASSERT_EQUAL_UINT16(0, SSD1315::maxDataBytesForWriteCapacity(0));
  TEST_ASSERT_EQUAL_UINT16(128, SSD1315::maxDataBytesForWriteCapacity(129));
}

void test_attach_is_zero_i2c_atomic_and_retains_binding_after_init_failure() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  SSD1315::Config cfg = makeConfig(bus);

  TEST_ASSERT_TRUE(display.attach(cfg).ok());
  TEST_ASSERT_EQUAL_UINT32(0, bus.writeCalls);
  uint8_t* const ownedBuffer = display.getBuffer();
  TEST_ASSERT_NOT_NULL(ownedBuffer);

  SSD1315::Config invalid = cfg;
  invalid.maxWriteBytes = 3;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::INVALID_CONFIG),
      static_cast<uint8_t>(display.attach(invalid).code));
  TEST_ASSERT_EQUAL_PTR(ownedBuffer, display.getBuffer());
  TEST_ASSERT_TRUE(display.isAttached());
  TEST_ASSERT_EQUAL_UINT32(0, bus.writeCalls);

  SSD1315::Config aliased = cfg;
  aliased.externalBuffer = ownedBuffer;
  aliased.externalBufferSizeBytes =
      SSD1315::requiredFramebufferBytes(cfg.width, cfg.pageBufferPages);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::INVALID_CONFIG),
      static_cast<uint8_t>(display.attach(aliased).code));
  TEST_ASSERT_EQUAL_PTR(ownedBuffer, display.getBuffer());
  TEST_ASSERT_EQUAL_UINT32(0, bus.writeCalls);

  TEST_ASSERT_TRUE(display.startInitialize(operationOptions(1)).ok());
  bus.failResult = SSD1315::TransportResult::Timeout(-71);
  bus.failOnWriteCall = bus.writeCalls + 1;
  const SSD1315::Status poll = display.pollOperation(bus.nowMs, 1, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(poll.code));
  SSD1315::OperationResult failed;
  TEST_ASSERT_TRUE(display.takeOperationResult(failed).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::OperationState::FAILED),
                          static_cast<uint8_t>(failed.state));
  TEST_ASSERT_EQUAL_INT32(-71, failed.status.detail);
  TEST_ASSERT_TRUE(display.isAttached());
  TEST_ASSERT_FALSE(display.isInitialized());
  TEST_ASSERT_EQUAL_PTR(ownedBuffer, display.getBuffer());

  TEST_ASSERT_TRUE(display.startInitialize(operationOptions(2)).ok());
  pollOperationUntilTerminal(display, bus, 1, 1);
  SSD1315::OperationResult recovered;
  takeSuccessfulOperation(display, recovered);
}

void test_attach_rejects_invalid_write_capacities_before_i2c() {
  const uint16_t invalidCapacities[] = {0, 1, 2, 3, 130, UINT16_MAX};
  for (size_t index = 0;
       index < sizeof(invalidCapacities) / sizeof(invalidCapacities[0]);
       ++index) {
    FakeBus bus;
    SSD1315::SSD1315 display;
    SSD1315::Config cfg = makeConfig(bus);
    cfg.maxWriteBytes = invalidCapacities[index];
    const SSD1315::Status st = display.attach(cfg);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::INVALID_CONFIG),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_UINT32(0, bus.writeCalls);
    TEST_ASSERT_FALSE(display.isAttached());
  }

  FakeBus bus;
  SSD1315::SSD1315 display;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.maxWriteBytes = 4;
  TEST_ASSERT_TRUE(display.attach(cfg).ok());
  TEST_ASSERT_EQUAL_UINT32(0, bus.writeCalls);
}

void test_all_cooperative_operations_respect_one_transaction_and_byte_budget() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.flushTimeoutMs = 0;
  TEST_ASSERT_TRUE(display.attach(cfg).ok());

  SSD1315::OperationResult result;
  TEST_ASSERT_TRUE(display.startInitialize(operationOptions(1)).ok());
  pollOperationUntilTerminal(display, bus, 1, 1);
  takeSuccessfulOperation(display, result);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::PanelPowerState::OFF),
                          static_cast<uint8_t>(result.power));

  TEST_ASSERT_TRUE(display.startResync(operationOptions(2)).ok());
  pollOperationUntilTerminal(display, bus, 1, 1);
  takeSuccessfulOperation(display, result);
  TEST_ASSERT_EQUAL_UINT32(1024, result.bytesCompleted);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::PanelPowerState::ON),
                          static_cast<uint8_t>(result.power));

  display.setPixel(3, 3, true);
  TEST_ASSERT_TRUE(display.startFlush(operationOptions(3)).ok());
  pollOperationUntilTerminal(display, bus, 1, 1);
  takeSuccessfulOperation(display, result);

  TEST_ASSERT_TRUE(display.startSleep(operationOptions(4)).ok());
  pollOperationUntilTerminal(display, bus, 1, 1);
  takeSuccessfulOperation(display, result);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::PanelPowerState::OFF),
                          static_cast<uint8_t>(result.power));

  TEST_ASSERT_TRUE(display.startWake(operationOptions(5)).ok());
  pollOperationUntilTerminal(display, bus, 1, 1);
  takeSuccessfulOperation(display, result);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::PanelPowerState::ON),
                          static_cast<uint8_t>(result.power));

  TEST_ASSERT_TRUE(display.startShutdown(operationOptions(6)).ok());
  pollOperationUntilTerminal(display, bus, 1, 1);
  takeSuccessfulOperation(display, result);
  TEST_ASSERT_FALSE(display.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::PanelPowerState::OFF),
                          static_cast<uint8_t>(result.power));
}

void test_resync_with_129_byte_capacity_sends_eight_full_chunks_before_display_on() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.maxWriteBytes = 129;
  TEST_ASSERT_TRUE(display.attach(cfg).ok());
  TEST_ASSERT_TRUE(display.startResync(operationOptions(1)).ok());
  pollOperationUntilTerminal(display, bus, 1, 128);

  SSD1315::OperationResult result;
  takeSuccessfulOperation(display, result);
  TEST_ASSERT_EQUAL_UINT32(1024, result.bytesCompleted);
  TEST_ASSERT_EQUAL_UINT16(8, result.dataChunkCount);
  TEST_ASSERT_EQUAL_UINT32(8, countDataTransactions(bus));

  const size_t displayOnIndex = requireCommandIndex(bus, SSD1315::cmd::DISPLAY_ON);
  uint32_t payloadBeforeDisplayOn = 0;
  for (size_t index = 0; index < displayOnIndex; ++index) {
    const FakeBus::Transaction& tx = bus.transactions[index];
    if (tx.len > 0 && tx.data[0] == SSD1315::cmd::CTRL_DATA) {
      TEST_ASSERT_EQUAL_UINT32(129, static_cast<uint32_t>(tx.len));
      payloadBeforeDisplayOn += static_cast<uint32_t>(tx.len - 1);
    }
  }
  TEST_ASSERT_EQUAL_UINT32(1024, payloadBeforeDisplayOn);
  TEST_ASSERT_EQUAL_UINT32(1, countCommand(bus, SSD1315::cmd::DISPLAY_ON));
}

void test_ambiguous_flush_timeout_retains_dirty_and_retry_readdresses() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.attach(makeConfig(bus)).ok());
  TEST_ASSERT_TRUE(display.startResync(operationOptions(1)).ok());
  pollOperationUntilTerminal(display, bus, 1, 128);
  SSD1315::OperationResult result;
  takeSuccessfulOperation(display, result);

  bus.clearTransactions();
  display.setPixel(7, 2, true);
  TEST_ASSERT_TRUE(display.startFlush(operationOptions(2)).ok());
  TEST_ASSERT_TRUE(display.pollOperation(bus.nowMs, 1, 128).inProgress());
  TEST_ASSERT_TRUE(display.pollOperation(bus.nowMs, 1, 128).inProgress());
  bus.failResult = SSD1315::TransportResult::Timeout(-404);
  bus.failOnWriteCall = bus.writeCalls + 1;
  const SSD1315::Status failure = display.pollOperation(bus.nowMs, 1, 128);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(failure.code));
  TEST_ASSERT_TRUE(display.takeOperationResult(result).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::EffectState::INDETERMINATE),
                          static_cast<uint8_t>(result.effect));
  TEST_ASSERT_TRUE(display.isDirty());

  bus.clearTransactions();
  TEST_ASSERT_TRUE(display.startFlush(operationOptions(3)).ok());
  pollOperationUntilTerminal(display, bus, 1, 128);
  takeSuccessfulOperation(display, result);
  TEST_ASSERT_TRUE(bus.transactionCount >= 3);
  TEST_ASSERT_TRUE(transactionHasCommand(bus.transactions[0],
                                         SSD1315::cmd::SET_COL_ADDR));
  TEST_ASSERT_TRUE(transactionHasCommand(bus.transactions[1],
                                         SSD1315::cmd::SET_PAGE_ADDR));
  TEST_ASSERT_EQUAL_HEX8(SSD1315::cmd::CTRL_DATA,
                         bus.transactions[2].data[0]);
  TEST_ASSERT_FALSE(display.isDirty());
}

void test_cancellation_is_zero_i2c_during_init_and_each_flush_phase() {
  {
    FakeBus bus;
    SSD1315::SSD1315 display;
    TEST_ASSERT_TRUE(display.attach(makeConfig(bus)).ok());
    TEST_ASSERT_TRUE(display.startInitialize(operationOptions(1)).ok());
    const uint32_t before = bus.writeCalls;
    TEST_ASSERT_TRUE(display.cancelOperation().ok());
    TEST_ASSERT_EQUAL_UINT32(before, bus.writeCalls);
    SSD1315::OperationResult result;
    TEST_ASSERT_TRUE(display.takeOperationResult(result).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::OperationState::CANCELLED),
                            static_cast<uint8_t>(result.state));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::EffectState::NONE),
                            static_cast<uint8_t>(result.effect));
  }

  const SSD1315::OperationPhase phases[] = {
      SSD1315::OperationPhase::SET_COL_ADDR,
      SSD1315::OperationPhase::SET_PAGE_ADDR,
      SSD1315::OperationPhase::SEND_DATA};
  for (size_t target = 0; target < sizeof(phases) / sizeof(phases[0]); ++target) {
    FakeBus bus;
    SSD1315::SSD1315 display;
    TEST_ASSERT_TRUE(display.attach(makeConfig(bus)).ok());
    TEST_ASSERT_TRUE(display.startResync(operationOptions(1)).ok());
    for (uint32_t polls = 0;
         display.getOperationProgress().phase != phases[target] && polls < 64;
         ++polls) {
      const uint32_t beforePoll = bus.writeCalls;
      TEST_ASSERT_TRUE(display.pollOperation(bus.nowMs, 1, 1).inProgress());
      TEST_ASSERT_TRUE(bus.writeCalls - beforePoll <= 1);
    }
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(phases[target]),
        static_cast<uint8_t>(display.getOperationProgress().phase));
    const uint32_t beforeCancel = bus.writeCalls;
    TEST_ASSERT_TRUE(display.cancelOperation().ok());
    TEST_ASSERT_EQUAL_UINT32(beforeCancel, bus.writeCalls);
    SSD1315::OperationResult result;
    TEST_ASSERT_TRUE(display.takeOperationResult(result).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::OperationState::CANCELLED),
                            static_cast<uint8_t>(result.state));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::EffectState::PARTIAL),
                            static_cast<uint8_t>(result.effect));
    TEST_ASSERT_TRUE(display.isDirty());
  }
}

void test_deadlines_are_exact_and_wrap_safe_without_late_i2c() {
  {
    FakeBus bus;
    SSD1315::SSD1315 display;
    TEST_ASSERT_TRUE(display.attach(makeConfig(bus)).ok());
    SSD1315::OperationOptions options = operationOptions(1);
    options.useDeadline = true;
    options.deadlineMs = 100;
    TEST_ASSERT_TRUE(display.startInitialize(options).ok());
    TEST_ASSERT_TRUE(display.pollOperation(99, 1, 1).inProgress());
    const uint32_t beforeDeadline = bus.writeCalls;
    const SSD1315::Status timedOut = display.pollOperation(100, 1, 1);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::TIMEOUT),
                            static_cast<uint8_t>(timedOut.code));
    TEST_ASSERT_EQUAL_UINT32(beforeDeadline, bus.writeCalls);
    SSD1315::OperationResult result;
    TEST_ASSERT_TRUE(display.takeOperationResult(result).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::OperationState::TIMED_OUT),
                            static_cast<uint8_t>(result.state));
  }

  {
    FakeBus bus;
    SSD1315::SSD1315 display;
    TEST_ASSERT_TRUE(display.attach(makeConfig(bus)).ok());
    SSD1315::OperationOptions options = operationOptions(2);
    options.useDeadline = true;
    options.deadlineMs = 1;
    TEST_ASSERT_TRUE(display.startInitialize(options).ok());
    TEST_ASSERT_TRUE(display.pollOperation(UINT32_MAX - 1u, 1, 1).inProgress());
    TEST_ASSERT_TRUE(display.pollOperation(UINT32_MAX, 1, 1).inProgress());
    TEST_ASSERT_TRUE(display.pollOperation(0, 1, 1).inProgress());
    const uint32_t beforeDeadline = bus.writeCalls;
    const SSD1315::Status timedOut = display.pollOperation(1, 1, 1);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::TIMEOUT),
                            static_cast<uint8_t>(timedOut.code));
    TEST_ASSERT_EQUAL_UINT32(beforeDeadline, bus.writeCalls);
  }
}

void test_request_ids_are_busy_until_result_is_consumed_and_take_is_once() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.attach(makeConfig(bus)).ok());
  TEST_ASSERT_TRUE(display.startInitialize(operationOptions(10)).ok());

  SSD1315::OperationResult result;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::RESULT_NOT_AVAILABLE),
      static_cast<uint8_t>(display.takeOperationResult(result).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::BUSY),
      static_cast<uint8_t>(display.startInitialize(operationOptions(11)).code));
  pollOperationUntilTerminal(display, bus, 1, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::BUSY),
      static_cast<uint8_t>(display.startResync(operationOptions(11)).code));
  takeSuccessfulOperation(display, result);
  TEST_ASSERT_EQUAL_UINT32(10, result.requestId);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::RESULT_NOT_AVAILABLE),
      static_cast<uint8_t>(display.takeOperationResult(result).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::INVALID_CONFIG),
      static_cast<uint8_t>(display.startResync(operationOptions(10)).code));
  TEST_ASSERT_TRUE(display.startResync(operationOptions(11)).ok());
  TEST_ASSERT_TRUE(display.cancelOperation().ok());
  TEST_ASSERT_TRUE(display.takeOperationResult(result).ok());
}

void test_raw_success_invalidates_control_and_resync_restores_flush_admission() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.attach(makeConfig(bus)).ok());
  TEST_ASSERT_TRUE(display.startResync(operationOptions(1)).ok());
  pollOperationUntilTerminal(display, bus, 1, 128);
  SSD1315::OperationResult result;
  takeSuccessfulOperation(display, result);

  const uint32_t beforeRaw = bus.writeCalls;
  TEST_ASSERT_TRUE(display.sendCommand(SSD1315::cmd::NOP).ok());
  TEST_ASSERT_EQUAL_UINT32(beforeRaw + 1, bus.writeCalls);
  assertControlStateDirty(display, SSD1315::Err::CONTROL_STATE_UNKNOWN);
  display.setPixel(0, 0, true);
  const uint32_t beforeRejectedFlush = bus.writeCalls;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::CONTROL_STATE_UNKNOWN),
      static_cast<uint8_t>(display.startFlush(operationOptions(2)).code));
  TEST_ASSERT_EQUAL_UINT32(beforeRejectedFlush, bus.writeCalls);

  TEST_ASSERT_TRUE(display.startResync(operationOptions(3)).ok());
  pollOperationUntilTerminal(display, bus, 1, 128);
  takeSuccessfulOperation(display, result);
  assertControlStateClean(display);
  display.setPixel(1, 1, true);
  TEST_ASSERT_TRUE(display.startFlush(operationOptions(4)).ok());
  TEST_ASSERT_TRUE(display.cancelOperation().ok());
  TEST_ASSERT_TRUE(display.takeOperationResult(result).ok());
}

void test_draw_text_n_and_touch_are_fixed_length_memory_only_and_never_wake() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.attach(makeConfig(bus)).ok());
  TEST_ASSERT_TRUE(display.startResync(operationOptions(1)).ok());
  pollOperationUntilTerminal(display, bus, 1, 128);
  SSD1315::OperationResult result;
  takeSuccessfulOperation(display, result);
  TEST_ASSERT_TRUE(display.startSleep(operationOptions(2)).ok());
  pollOperationUntilTerminal(display, bus, 1, 1);
  takeSuccessfulOperation(display, result);

  display.clearDirty();
  char text[15];
  memset(text, 'A', sizeof(text));
  const uint32_t beforeMemoryWork = bus.writeCalls;
  TEST_ASSERT_EQUAL_INT16(90, SSD1315::SSD1315::getTextWidthN(text, sizeof(text)));
  TEST_ASSERT_EQUAL_INT16(90, display.drawTextN(0, 0, text, sizeof(text), true));
  display.touch();
  display.setPixel(127, 63, true);
  TEST_ASSERT_EQUAL_UINT32(beforeMemoryWork, bus.writeCalls);
  TEST_ASSERT_TRUE(display.isDirty());
  SSD1315::SettingsSnapshot settings;
  TEST_ASSERT_TRUE(display.getSettings(settings).ok());
  TEST_ASSERT_TRUE(settings.sleeping);
}

void test_detach_and_destructor_cancel_local_state_with_zero_i2c() {
  FakeBus bus;
  uint32_t writesBeforeDestruction = 0;
  {
    SSD1315::SSD1315 display;
    TEST_ASSERT_TRUE(display.attach(makeConfig(bus)).ok());
    TEST_ASSERT_TRUE(display.startInitialize(operationOptions(1)).ok());
    TEST_ASSERT_TRUE(display.pollOperation(bus.nowMs, 1, 1).inProgress());
    const uint32_t beforeDetach = bus.writeCalls;
    display.detach();
    TEST_ASSERT_EQUAL_UINT32(beforeDetach, bus.writeCalls);
    TEST_ASSERT_FALSE(display.isAttached());
    display.detach();
    TEST_ASSERT_EQUAL_UINT32(beforeDetach, bus.writeCalls);

    TEST_ASSERT_TRUE(display.attach(makeConfig(bus)).ok());
    TEST_ASSERT_TRUE(display.startInitialize(operationOptions(2)).ok());
    writesBeforeDestruction = bus.writeCalls;
  }
  TEST_ASSERT_EQUAL_UINT32(writesBeforeDestruction, bus.writeCalls);
}

void test_repeated_initialize_shutdown_and_rebind_are_explicit() {
  FakeBus busA;
  FakeBus busB;
  uint8_t bufferA[1024] = {};
  uint8_t bufferB[1024] = {};
  SSD1315::Config cfgA = makeConfig(busA);
  cfgA.externalBuffer = bufferA;
  cfgA.externalBufferSizeBytes = sizeof(bufferA);
  SSD1315::Config cfgB = makeConfig(busB);
  cfgB.externalBuffer = bufferB;
  cfgB.externalBufferSizeBytes = sizeof(bufferB);
  SSD1315::SSD1315 display;
  SSD1315::OperationResult result;

  TEST_ASSERT_TRUE(display.attach(cfgA).ok());
  TEST_ASSERT_TRUE(display.startInitialize(operationOptions(1)).ok());
  pollOperationUntilTerminal(display, busA, 1, 1);
  takeSuccessfulOperation(display, result);
  TEST_ASSERT_TRUE(display.startShutdown(operationOptions(2)).ok());
  pollOperationUntilTerminal(display, busA, 1, 1);
  takeSuccessfulOperation(display, result);
  TEST_ASSERT_FALSE(display.isInitialized());

  TEST_ASSERT_TRUE(display.startInitialize(operationOptions(3)).ok());
  pollOperationUntilTerminal(display, busA, 1, 1);
  takeSuccessfulOperation(display, result);
  const uint32_t busAWrites = busA.writeCalls;
  TEST_ASSERT_TRUE(display.attach(cfgB).ok());
  TEST_ASSERT_EQUAL_UINT32(busAWrites, busA.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0, busB.writeCalls);
  TEST_ASSERT_EQUAL_PTR(bufferB, display.getBuffer());
  TEST_ASSERT_TRUE(display.startInitialize(operationOptions(1)).ok());
  pollOperationUntilTerminal(display, busB, 1, 1);
  takeSuccessfulOperation(display, result);
}

void test_offline_health_is_diagnostic_and_resync_still_attempts_i2c() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.offlineThreshold = 1;
  TEST_ASSERT_TRUE(display.attach(cfg).ok());
  TEST_ASSERT_TRUE(display.startResync(operationOptions(1)).ok());
  pollOperationUntilTerminal(display, bus, 1, 128);
  SSD1315::OperationResult result;
  takeSuccessfulOperation(display, result);

  bus.failResult = SSD1315::TransportResult::NackAddress(-9);
  bus.failOnWriteCall = bus.writeCalls + 1;
  TEST_ASSERT_TRUE(display.startSleep(operationOptions(2)).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::I2C_NACK_ADDR),
      static_cast<uint8_t>(display.pollOperation(bus.nowMs, 1, 1).code));
  TEST_ASSERT_TRUE(display.takeOperationResult(result).ok());
  SSD1315::SettingsSnapshot settings;
  TEST_ASSERT_TRUE(display.getSettings(settings).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::DriverState::OFFLINE),
                          static_cast<uint8_t>(settings.state));

  const uint32_t beforeResync = bus.writeCalls;
  TEST_ASSERT_TRUE(display.startResync(operationOptions(3)).ok());
  TEST_ASSERT_TRUE(display.pollOperation(bus.nowMs, 1, 1).inProgress());
  TEST_ASSERT_EQUAL_UINT32(beforeResync + 1, bus.writeCalls);
  pollOperationUntilTerminal(display, bus, 1, 128);
  takeSuccessfulOperation(display, result);
  TEST_ASSERT_TRUE(display.getSettings(settings).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::DriverState::READY),
                          static_cast<uint8_t>(settings.state));
}

void test_owner_safe_power_admission_and_wake_cancellation_dirty_state() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.displayOnDelayMs = 20;
  TEST_ASSERT_TRUE(display.attach(cfg).ok());
  TEST_ASSERT_TRUE(display.startInitialize(operationOptions(1)).ok());
  pollOperationUntilTerminal(display, bus, 1, 1);
  SSD1315::OperationResult result;
  takeSuccessfulOperation(display, result);

  const uint32_t beforeOffFlush = bus.writeCalls;
  TEST_ASSERT_TRUE(display.startFlush(operationOptions(2)).ok());
  TEST_ASSERT_TRUE(display.cancelOperation().ok());
  TEST_ASSERT_EQUAL_UINT32(beforeOffFlush, bus.writeCalls);
  TEST_ASSERT_TRUE(display.takeOperationResult(result).ok());

  TEST_ASSERT_TRUE(display.startWake(operationOptions(3)).ok());
  TEST_ASSERT_TRUE(display.pollOperation(bus.nowMs, 1, 1).inProgress());
  const uint32_t beforeCancel = bus.writeCalls;
  TEST_ASSERT_TRUE(display.cancelOperation().ok());
  TEST_ASSERT_EQUAL_UINT32(beforeCancel, bus.writeCalls);
  TEST_ASSERT_TRUE(display.takeOperationResult(result).ok());
  assertControlStateDirty(display, SSD1315::Err::CANCELLED);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::PanelPowerState::UNKNOWN),
                          static_cast<uint8_t>(result.power));

  TEST_ASSERT_TRUE(display.startResync(operationOptions(4)).ok());
  pollOperationUntilTerminal(display, bus, 1, 128);
  takeSuccessfulOperation(display, result);
  const uint32_t beforeDuplicateWake = bus.writeCalls;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::STATE_ERROR),
      static_cast<uint8_t>(display.startWake(operationOptions(5)).code));
  TEST_ASSERT_EQUAL_UINT32(beforeDuplicateWake, bus.writeCalls);

  TEST_ASSERT_TRUE(display.startSleep(operationOptions(6)).ok());
  pollOperationUntilTerminal(display, bus, 1, 1);
  takeSuccessfulOperation(display, result);
  SSD1315::OperationOptions deadline = operationOptions(7);
  deadline.useDeadline = true;
  deadline.deadlineMs = bus.nowMs + 1;
  TEST_ASSERT_TRUE(display.startWake(deadline).ok());
  TEST_ASSERT_TRUE(display.pollOperation(bus.nowMs, 1, 1).inProgress());
  const uint32_t beforeDeadline = bus.writeCalls;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::TIMEOUT),
      static_cast<uint8_t>(display.pollOperation(bus.nowMs + 1, 1, 1).code));
  TEST_ASSERT_EQUAL_UINT32(beforeDeadline, bus.writeCalls);
  TEST_ASSERT_TRUE(display.takeOperationResult(result).ok());
  assertControlStateDirty(display, SSD1315::Err::TIMEOUT);
}

void test_page_buffer_attach_is_safe_and_owner_flushes_while_off_before_wake() {
  FakeBus bus;
  uint8_t guardedBuffer[130] = {};
  guardedBuffer[0] = 0xA5;
  guardedBuffer[129] = 0x5A;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.pageBufferPages = 1;
  cfg.externalBuffer = guardedBuffer + 1;
  cfg.externalBufferSizeBytes = 128;
  SSD1315::SSD1315 display;

  TEST_ASSERT_TRUE(display.attach(cfg).ok());
  TEST_ASSERT_TRUE(display.isPageBufferMode());
  TEST_ASSERT_EQUAL_UINT32(128, static_cast<uint32_t>(display.getBufferSize()));
  display.firstPage();
  display.fill();
  TEST_ASSERT_EQUAL_HEX8(0xA5, guardedBuffer[0]);
  TEST_ASSERT_EQUAL_HEX8(0x5A, guardedBuffer[129]);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::UNSUPPORTED),
      static_cast<uint8_t>(display.startResync(operationOptions(1)).code));
  TEST_ASSERT_EQUAL_UINT32(0, bus.writeCalls);

  TEST_ASSERT_TRUE(display.startInitialize(operationOptions(2)).ok());
  pollOperationUntilTerminal(display, bus, 1, 1);
  SSD1315::OperationResult result;
  takeSuccessfulOperation(display, result);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::PanelPowerState::OFF),
                          static_cast<uint8_t>(result.power));

  bus.clearTransactions();
  TEST_ASSERT_TRUE(display.startFlush(operationOptions(3)).ok());
  pollOperationUntilTerminal(display, bus, 1, 128);
  takeSuccessfulOperation(display, result);
  TEST_ASSERT_EQUAL_UINT32(128, result.bytesCompleted);
  TEST_ASSERT_EQUAL_UINT32(128, countDataPayloadBytes(bus, 0xFF));
  TEST_ASSERT_EQUAL_UINT32(0, countCommand(bus, SSD1315::cmd::DISPLAY_ON));

  TEST_ASSERT_TRUE(display.startWake(operationOptions(4)).ok());
  pollOperationUntilTerminal(display, bus, 1, 1);
  takeSuccessfulOperation(display, result);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::PanelPowerState::ON),
                          static_cast<uint8_t>(result.power));
  TEST_ASSERT_EQUAL_HEX8(0xA5, guardedBuffer[0]);
  TEST_ASSERT_EQUAL_HEX8(0x5A, guardedBuffer[129]);
}

void test_deadline_clips_each_transport_attempt_timeout_including_wrap() {
  {
    FakeBus bus;
    SSD1315::SSD1315 display;
    SSD1315::Config cfg = makeConfig(bus);
    cfg.i2cTimeoutMs = 25;
    TEST_ASSERT_TRUE(display.attach(cfg).ok());
    SSD1315::OperationOptions options = operationOptions(1);
    options.useDeadline = true;
    options.deadlineMs = 107;
    TEST_ASSERT_TRUE(display.startInitialize(options).ok());
    TEST_ASSERT_TRUE(display.pollOperation(100, 1, 1).inProgress());
    TEST_ASSERT_EQUAL_UINT32(7, bus.transactions[0].timeoutMs);
    TEST_ASSERT_TRUE(display.pollOperation(106, 1, 1).inProgress());
    TEST_ASSERT_EQUAL_UINT32(1, bus.transactions[1].timeoutMs);
  }

  {
    FakeBus bus;
    SSD1315::SSD1315 display;
    SSD1315::Config cfg = makeConfig(bus);
    cfg.i2cTimeoutMs = 25;
    TEST_ASSERT_TRUE(display.attach(cfg).ok());
    SSD1315::OperationOptions options = operationOptions(2);
    options.useDeadline = true;
    options.deadlineMs = 2;
    TEST_ASSERT_TRUE(display.startInitialize(options).ok());
    TEST_ASSERT_TRUE(display.pollOperation(UINT32_MAX, 1, 1).inProgress());
    TEST_ASSERT_EQUAL_UINT32(3, bus.transactions[0].timeoutMs);
  }
}

void test_active_cooperative_operation_blocks_direct_i2c_and_legacy_flush_paths() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.attach(makeConfig(bus)).ok());
  TEST_ASSERT_TRUE(display.startResync(operationOptions(1)).ok());
  pollOperationUntilTerminal(display, bus, 1, 128);
  SSD1315::OperationResult result;
  takeSuccessfulOperation(display, result);

  display.setPixel(4, 4, true);
  TEST_ASSERT_TRUE(display.startFlush(operationOptions(2)).ok());
  const uint32_t writesBeforeRejectedCalls = bus.writeCalls;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::BUSY),
      static_cast<uint8_t>(display.sendCommand(SSD1315::cmd::NOP).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::BUSY),
      static_cast<uint8_t>(display.setContrast(0x55).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::BUSY),
      static_cast<uint8_t>(display.requestFlush().code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::BUSY),
      static_cast<uint8_t>(display.requestFlushRect(0, 0, 1, 1).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::BUSY),
      static_cast<uint8_t>(display.pollFlush(bus.nowMs, 1, 1).code));
  TEST_ASSERT_EQUAL_UINT32(writesBeforeRejectedCalls, bus.writeCalls);

  pollOperationUntilTerminal(display, bus, 1, 1);
  takeSuccessfulOperation(display, result);
}

void test_begin_does_not_probe_and_end_is_zero_i2c() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(makeConfig(bus)).ok());
  TEST_ASSERT_TRUE(bus.transactionCount > 0);
  TEST_ASSERT_TRUE(transactionHasCommand(bus.transactions[0],
                                         SSD1315::cmd::DISPLAY_OFF));
  TEST_ASSERT_EQUAL_UINT32(0, countCommand(bus, SSD1315::cmd::NOP));
  const uint32_t beforeEnd = bus.writeCalls;
  display.end();
  TEST_ASSERT_EQUAL_UINT32(beforeEnd, bus.writeCalls);
  TEST_ASSERT_FALSE(display.isAttached());
}

void test_version_header_uses_canonical_namespace() {
  std::string header;
  TEST_ASSERT_TRUE(loadTextFile("include/ssd1315/Version.h", header));
  TEST_ASSERT_NOT_NULL(strstr(header.c_str(), "namespace SSD1315 {"));
  TEST_ASSERT_NOT_NULL(strstr(header.c_str(), "namespace ssd1315 = SSD1315;"));
  TEST_ASSERT_NULL(strstr(header.c_str(), "namespace ssd1315 {"));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_status_ok);
  RUN_TEST(test_status_helpers);
  RUN_TEST(test_config_defaults);
  RUN_TEST(test_panel_profiles_apply_module_specific_defaults);
  RUN_TEST(test_canonical_api_symbols_exist);
  RUN_TEST(test_begin_requires_i2c_write_callback);
  RUN_TEST(test_begin_rejects_invalid_config_enums);
  RUN_TEST(test_begin_rejects_datasheet_invalid_multiplex_height);
  RUN_TEST(test_begin_rejects_non_ssd1315_i2c_address_and_contrast_zero);
  RUN_TEST(test_begin_accepts_charge_pump_disabled_reset_value);
  RUN_TEST(test_begin_uses_ssd1315_golden_init_sequence);
  RUN_TEST(test_wisevision_panel_profiles_drive_expected_init_values);
  RUN_TEST(test_clear_on_begin_can_skip_blocking_gddram_clear);
  RUN_TEST(test_failed_begin_during_clear_never_sends_display_on);
  RUN_TEST(test_raw_commands_require_begin);
  RUN_TEST(test_invalid_begin_after_success_preserves_binding_without_i2c);
  RUN_TEST(test_runtime_i2c_after_begin_updates_health);
  RUN_TEST(test_get_settings_snapshot);
  RUN_TEST(test_external_buffer_begin_uses_caller_storage_without_ownership);
  RUN_TEST(test_external_buffer_rejects_undersized_storage_before_i2c);
  RUN_TEST(test_external_buffer_size_contract_uses_page_buffer_pages);
  RUN_TEST(test_command_list_parameter_error_does_not_touch_bus_or_health);
  RUN_TEST(test_command_list_length_is_bounded_without_i2c_on_overflow);
  RUN_TEST(test_probe_requires_initialized_and_does_not_touch_stale_transport);
  RUN_TEST(test_probe_failure_does_not_update_health);
  RUN_TEST(test_probe_timeout_preserves_transport_error);
  RUN_TEST(test_probe_sends_ack_only_nop_transaction);
  RUN_TEST(test_probe_preserves_non_address_transport_errors);
  RUN_TEST(test_page_buffer_tick_preserves_done_for_next_page);
  RUN_TEST(test_page_buffer_tick_preserves_error_for_next_page_abort);
  RUN_TEST(test_page_buffer_clear_affects_current_window_only);
  RUN_TEST(test_page_buffer_fill_affects_current_window_only);
  RUN_TEST(test_page_buffer_full_iteration_clear_flushes_all_pages);
  RUN_TEST(test_page_buffer_full_iteration_fill_flushes_all_pages);
  RUN_TEST(test_page_buffer_docs_contract_is_not_contradicted_by_api_comments);
  RUN_TEST(test_invalid_scroll_and_fade_params_do_not_send_i2c);
  RUN_TEST(test_display_control_commands_send_expected_bytes);
  RUN_TEST(test_sleep_display_off_does_not_disable_charge_pump);
  RUN_TEST(test_clear_after_display_off_on_flushes_zero_bytes);
  RUN_TEST(test_sleep_or_display_off_failure_sets_control_dirty);
  RUN_TEST(test_recover_then_clear_resyncs_control_and_gddram_state);
  RUN_TEST(test_display_control_failures_mark_dirty_and_recover_clears);
  RUN_TEST(test_scroll_commands_send_expected_byte_sequences);
  RUN_TEST(test_vertical_scroll_offset_valid_for_default_area);
  RUN_TEST(test_vertical_scroll_offset_rejected_for_small_scroll_area);
  RUN_TEST(test_vertical_scroll_invalid_params_preserve_existing_scroll_state);
  RUN_TEST(test_set_vertical_scroll_area_validation);
  RUN_TEST(test_non_128_width_scroll_rejected_and_flush_uses_configured_width);
  RUN_TEST(test_scroll_failures_mark_control_state_dirty);
  RUN_TEST(test_scroll_active_blocks_flush_until_stopped_and_marks_dirty);
  RUN_TEST(test_control_state_dirty_after_scroll_mid_sequence_failure_and_recover_clears);
  RUN_TEST(test_control_state_dirty_survives_invalid_begin_until_successful_resync);
  RUN_TEST(test_default_recover_clears_gddram_before_display_on);
  RUN_TEST(test_end_is_idempotent_and_clears_transient_state);
  RUN_TEST(test_zoom_enable_requires_alternative_com_pins_without_i2c);
  RUN_TEST(test_wait_flush_without_clock_hook_uses_caller_time);
  RUN_TEST(test_flush_error_preserves_dirty_flags_and_updates_health_once);
  RUN_TEST(test_clear_dirty_if_idle_preserves_active_and_failed_retry_state);
  RUN_TEST(test_flush_retry_replays_failed_dirty_byte);
  RUN_TEST(test_flush_error_reaches_offline_immediately_when_threshold_is_one);
  RUN_TEST(test_clear_after_fill_flush_sends_zero_payload);
  RUN_TEST(test_active_flush_mutation_keeps_dirty_and_retry_sends_current_framebuffer);
  RUN_TEST(test_failed_partial_flush_retry_uses_current_framebuffer);
  RUN_TEST(test_full_frame_flush_transaction_count_and_chunking);
  RUN_TEST(test_poll_flush_address_window_uses_one_instruction_per_poll);
  RUN_TEST(test_poll_flush_byte_budget_limits_data_with_instruction_headroom);
  RUN_TEST(test_poll_flush_instruction_budget_limits_data_transactions);
  RUN_TEST(test_poll_flush_zero_instruction_queries_do_not_touch_i2c_or_advance);
  RUN_TEST(test_poll_flush_budget_stress_matrix_preserves_dirty_and_completes);
  RUN_TEST(test_poll_flush_page_address_failure_preserves_dirty_and_retries);
  RUN_TEST(test_checked_draw_bitmap_exact_size_draws_and_marks_dirty);
  RUN_TEST(test_checked_draw_bitmap_rejects_undersized_source_without_dirty);
  RUN_TEST(test_checked_draw_bitmap_clipped_offscreen_reads_validated_bounds_only);
  RUN_TEST(test_out_of_bounds_draws_preserve_external_buffer_guards);
  RUN_TEST(test_hostile_drawing_and_flush_rect_stress_preserves_external_buffer_guards);
  RUN_TEST(test_long_text_does_not_wrap_back_into_visible_buffer);
  RUN_TEST(test_display_on_delay_zero_is_immediate);
  RUN_TEST(test_v4_types_are_fixed_trivial_and_noexcept);
  RUN_TEST(test_attach_is_zero_i2c_atomic_and_retains_binding_after_init_failure);
  RUN_TEST(test_attach_rejects_invalid_write_capacities_before_i2c);
  RUN_TEST(test_all_cooperative_operations_respect_one_transaction_and_byte_budget);
  RUN_TEST(test_resync_with_129_byte_capacity_sends_eight_full_chunks_before_display_on);
  RUN_TEST(test_ambiguous_flush_timeout_retains_dirty_and_retry_readdresses);
  RUN_TEST(test_cancellation_is_zero_i2c_during_init_and_each_flush_phase);
  RUN_TEST(test_deadlines_are_exact_and_wrap_safe_without_late_i2c);
  RUN_TEST(test_request_ids_are_busy_until_result_is_consumed_and_take_is_once);
  RUN_TEST(test_raw_success_invalidates_control_and_resync_restores_flush_admission);
  RUN_TEST(test_draw_text_n_and_touch_are_fixed_length_memory_only_and_never_wake);
  RUN_TEST(test_detach_and_destructor_cancel_local_state_with_zero_i2c);
  RUN_TEST(test_repeated_initialize_shutdown_and_rebind_are_explicit);
  RUN_TEST(test_offline_health_is_diagnostic_and_resync_still_attempts_i2c);
  RUN_TEST(test_owner_safe_power_admission_and_wake_cancellation_dirty_state);
  RUN_TEST(test_page_buffer_attach_is_safe_and_owner_flushes_while_off_before_wake);
  RUN_TEST(test_deadline_clips_each_transport_attempt_timeout_including_wrap);
  RUN_TEST(test_active_cooperative_operation_blocks_direct_i2c_and_legacy_flush_paths);
  RUN_TEST(test_begin_does_not_probe_and_end_is_zero_i2c);
  RUN_TEST(test_version_header_uses_canonical_namespace);
  return UNITY_END();
}
