/// @file test_basic.cpp
/// @brief Native contract tests for SSD1315 lifecycle and health behavior.

#include <unity.h>

#include <fstream>
#include <sstream>
#include <string>

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
  uint32_t nowMs = 100;
  uint32_t writeCalls = 0;
  uint32_t yieldCalls = 0;
  int failWriteRemaining = 0;
  uint32_t failOnWriteCall = 0;
  bool sawChargePumpDisabled = false;
  SSD1315::Status failStatus = SSD1315::Error(SSD1315::Err::I2C_NACK_ADDR, -1, "forced fail");
};

SSD1315::Status fakeI2cWrite(uint8_t, const uint8_t* data, size_t len, uint32_t, void* user) {
  FakeBus* bus = static_cast<FakeBus*>(user);
  bus->writeCalls++;
  if (data == nullptr || len == 0) {
    return SSD1315::Error(SSD1315::Err::INVALID_CONFIG, "invalid fake write args");
  }
  if (len >= 3 && data[0] == SSD1315::cmd::CTRL_COMMAND &&
      data[1] == SSD1315::cmd::SET_CHARGE_PUMP && data[2] == 0x10) {
    bus->sawChargePumpDisabled = true;
  }
  if (bus->failOnWriteCall != 0 && bus->writeCalls == bus->failOnWriteCall) {
    bus->failOnWriteCall = 0;
    return bus->failStatus;
  }
  if (bus->failWriteRemaining > 0) {
    bus->failWriteRemaining--;
    return bus->failStatus;
  }
  return SSD1315::Ok();
}

SSD1315::Status fakeI2cWriteRead(uint8_t, const uint8_t*, size_t, uint8_t*, size_t,
                                 uint32_t, void*) {
  return SSD1315::Ok();
}

uint32_t fakeNowMs(void* user) {
  return static_cast<FakeBus*>(user)->nowMs;
}

void fakeYield(void* user) {
  static_cast<FakeBus*>(user)->yieldCalls++;
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
  return cfg;
}

}  // namespace

static bool loadTextFile(const char* relativePath, std::string& out) {
  std::ifstream in(relativePath, std::ios::in | std::ios::binary);
  if (!in.good()) {
    return false;
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  out = ss.str();
  return true;
}

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
  TEST_ASSERT_EQUAL_UINT8(128, cfg.width);
  TEST_ASSERT_EQUAL_UINT8(64, cfg.height);
  TEST_ASSERT_EQUAL_UINT8(0x3C, cfg.i2cAddress);
  TEST_ASSERT_NULL(cfg.i2cWriteRead);
  TEST_ASSERT_EQUAL_UINT8(8, cfg.pageBufferPages);
  TEST_ASSERT_EQUAL_UINT16(128, cfg.byteBudgetPerTick);
  TEST_ASSERT_EQUAL_UINT8(3, cfg.offlineThreshold);
}

void test_canonical_api_symbols_exist() {
  SSD1315::SSD1315 display;
  (void)display;
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

void test_begin_accepts_charge_pump_disabled_reset_value() {
  FakeBus bus;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.chargePumpVoltage = SSD1315::ChargePumpVoltage::OFF;

  SSD1315::SSD1315 display;
  const SSD1315::Status st = display.begin(cfg);

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(display.isInitialized());
  TEST_ASSERT_TRUE(bus.sawChargePumpDisabled);
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
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(display.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::DriverState::READY),
                          static_cast<uint8_t>(display.state()));
  TEST_ASSERT_TRUE(display.isOnline());
  TEST_ASSERT_EQUAL_UINT32(0u, display.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(0u, display.totalFailures());
  TEST_ASSERT_EQUAL_UINT32(0u, display.lastOkMs());
}

void test_invalid_begin_after_success_resets_without_i2c() {
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
  TEST_ASSERT_FALSE(display.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::DriverState::UNINIT),
                          static_cast<uint8_t>(display.state()));
  TEST_ASSERT_NULL(display.getBuffer());
  TEST_ASSERT_EQUAL_UINT32(0u, static_cast<uint32_t>(display.getBufferSize()));
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);

  SSD1315::SettingsSnapshot snap;
  TEST_ASSERT_TRUE(display.getSettings(snap).ok());
  TEST_ASSERT_FALSE(snap.initialized);
  TEST_ASSERT_EQUAL_UINT8(0x3C, snap.i2cAddress);
  TEST_ASSERT_EQUAL_UINT8(3u, snap.offlineThreshold);
  TEST_ASSERT_EQUAL_UINT32(0u, snap.totalSuccess);
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
  TEST_ASSERT_EQUAL_UINT32(0u, display.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(0u, display.lastOkMs());

  bus.nowMs = 333u;
  const SSD1315::Status st = display.sendCommand(SSD1315::cmd::NOP);

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(1u, display.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(0u, display.totalFailures());
  TEST_ASSERT_EQUAL_UINT32(333u, display.lastOkMs());
}

void test_get_settings_snapshot() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  SSD1315::Config cfg = makeConfig(bus);
  cfg.i2cWriteRead = fakeI2cWriteRead;
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
  TEST_ASSERT_EQUAL_UINT8(0x3C, snap.i2cAddress);
  TEST_ASSERT_EQUAL_UINT32(10u, snap.i2cTimeoutMs);
  TEST_ASSERT_EQUAL_UINT8(3u, snap.offlineThreshold);
  TEST_ASSERT_TRUE(snap.hasNowMsHook);
  TEST_ASSERT_TRUE(snap.hasI2cWriteReadHook);
  TEST_ASSERT_EQUAL_UINT8(128u, snap.width);
  TEST_ASSERT_EQUAL_UINT8(64u, snap.height);
  TEST_ASSERT_EQUAL_UINT8(8u, snap.pageBufferPages);
  TEST_ASSERT_FALSE(snap.pageBufferMode);
  TEST_ASSERT_EQUAL_UINT8(0u, snap.currentPageIndex);
  TEST_ASSERT_FALSE(snap.pageIterationActive);
  TEST_ASSERT_EQUAL_UINT32(128u * 8u, static_cast<uint32_t>(snap.bufferSize));
  TEST_ASSERT_FALSE(snap.flushing);
  TEST_ASSERT_TRUE(snap.lastError.is(SSD1315::Err::OK));
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

void test_probe_failure_does_not_update_health() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(makeConfig(bus)).ok());

  const uint32_t beforeSuccess = display.totalSuccess();
  const uint32_t beforeFailures = display.totalFailures();
  const uint8_t beforeConsecutive = display.consecutiveFailures();
  const SSD1315::DriverState beforeState = display.state();

  bus.failWriteRemaining = 1;
  bus.failStatus = SSD1315::Error(SSD1315::Err::I2C_NACK_ADDR, -9, "probe nack");
  const SSD1315::Status st = display.probe();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::DEVICE_NOT_FOUND),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(beforeSuccess, display.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(beforeFailures, display.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(beforeConsecutive, display.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(beforeState),
                          static_cast<uint8_t>(display.state()));
}

void test_recover_failure_updates_health() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  TEST_ASSERT_TRUE(display.begin(makeConfig(bus)).ok());

  bus.failWriteRemaining = 1;
  bus.failStatus = SSD1315::Error(SSD1315::Err::I2C_NACK_ADDR, -7, "recover probe nack");
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
  bus.failStatus = SSD1315::Error(SSD1315::Err::I2C_NACK_ADDR, -7, "recover probe nack");
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
  bus.failStatus = SSD1315::Error(SSD1315::Err::I2C_NACK_ADDR, -5, "recover fail");
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
  bus.failStatus = SSD1315::Error(SSD1315::Err::I2C_NACK_ADDR, -6, "offline fail");
  SSD1315::Status st = display.recover();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::DriverState::OFFLINE),
                          static_cast<uint8_t>(display.state()));

  const uint32_t writesBefore = bus.writeCalls;
  st = display.sendCommand(SSD1315::cmd::NOP);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::BUSY),
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
    bus.failStatus = SSD1315::Error(SSD1315::Err::I2C_NACK_ADDR, -10, "forced offline");
    (void)display.recover();
  }
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::DriverState::OFFLINE),
                          static_cast<uint8_t>(display.state()));

  bus.failStatus = SSD1315::Error(SSD1315::Err::I2C_TIMEOUT, -11, "recover apply failed");
  bus.failOnWriteCall = bus.writeCalls + 3u;  // probe + one tracked recovery success, then fail
  const SSD1315::Status st = display.recover();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::DriverState::OFFLINE),
                          static_cast<uint8_t>(display.state()));
  TEST_ASSERT_TRUE(display.consecutiveFailures() >= cfg.offlineThreshold);

  const uint32_t writesBefore = bus.writeCalls;
  const SSD1315::Status latched = display.sendCommand(SSD1315::cmd::NOP);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::BUSY),
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
  TEST_ASSERT_FALSE(display.isFlushing());

  bus.failWriteRemaining = 1;
  bus.failStatus = SSD1315::Error(SSD1315::Err::I2C_NACK_ADDR, -12, "forced offline");
  (void)display.recover();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::DriverState::OFFLINE),
                          static_cast<uint8_t>(display.state()));

  TEST_ASSERT_FALSE(display.nextPage());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::DriverState::OFFLINE),
                          static_cast<uint8_t>(display.state()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::BUSY),
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
  bus.failStatus = SSD1315::Error(SSD1315::Err::I2C_TIMEOUT, -33, "page flush fail");
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

  st = display.startVerticalScroll(false, 0, 1, SSD1315::ScrollSpeed::FRAMES_5, 64);
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
  TEST_ASSERT_EQUAL_UINT32(1u, display.totalSuccess());
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
  bus.failStatus = SSD1315::Error(SSD1315::Err::I2C_TIMEOUT, -22, "flush timeout");
  display.tick(bus.nowMs);
  TEST_ASSERT_TRUE(display.isDirty());
  TEST_ASSERT_FALSE(display.isFlushing());

  display.tick(bus.nowMs);
  TEST_ASSERT_TRUE(display.isDirty());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(display.lastError().code));
  TEST_ASSERT_EQUAL_UINT32(1u, display.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(1u, display.consecutiveFailures());
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
  RUN_TEST(test_canonical_api_symbols_exist);
  RUN_TEST(test_begin_requires_i2c_write_callback);
  RUN_TEST(test_begin_rejects_invalid_config_enums);
  RUN_TEST(test_begin_rejects_datasheet_invalid_multiplex_height);
  RUN_TEST(test_begin_accepts_charge_pump_disabled_reset_value);
  RUN_TEST(test_raw_commands_require_begin);
  RUN_TEST(test_begin_success_sets_ready_without_health_counts);
  RUN_TEST(test_invalid_begin_after_success_resets_without_i2c);
  RUN_TEST(test_failed_begin_apply_rolls_back_buffer_and_health);
  RUN_TEST(test_runtime_i2c_after_begin_updates_health);
  RUN_TEST(test_get_settings_snapshot);
  RUN_TEST(test_command_list_parameter_error_does_not_touch_bus_or_health);
  RUN_TEST(test_probe_failure_does_not_update_health);
  RUN_TEST(test_recover_failure_updates_health);
  RUN_TEST(test_recover_success_restores_ready);
  RUN_TEST(test_recover_reaches_offline_when_threshold_is_one);
  RUN_TEST(test_offline_latches_send_command_without_i2c);
  RUN_TEST(test_failed_recover_from_offline_keeps_latch_after_intermediate_success);
  RUN_TEST(test_next_page_does_not_clear_offline_after_completed_flush);
  RUN_TEST(test_page_buffer_tick_preserves_done_for_next_page);
  RUN_TEST(test_page_buffer_tick_preserves_error_for_next_page_abort);
  RUN_TEST(test_invalid_scroll_and_fade_params_do_not_send_i2c);
  RUN_TEST(test_zoom_enable_requires_alternative_com_pins_without_i2c);
  RUN_TEST(test_wait_flush_returns_timeout_when_time_source_stalls);
  RUN_TEST(test_wait_flush_without_clock_hook_uses_caller_time);
  RUN_TEST(test_flush_error_preserves_dirty_flags_and_updates_health_once);
  RUN_TEST(test_auto_sleep_timer_handles_wraparound);
  RUN_TEST(test_version_header_uses_canonical_namespace);
  return UNITY_END();
}
