/// @file test_basic.cpp
/// @brief Native contract tests for SSD1315 lifecycle and health behavior.

#include <unity.h>

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
  int failWriteRemaining = 0;
  SSD1315::Status failStatus = SSD1315::Error(SSD1315::Err::I2C_NACK_ADDR, -1, "forced fail");
};

SSD1315::Status fakeI2cWrite(uint8_t, const uint8_t* data, size_t len, uint32_t, void* user) {
  FakeBus* bus = static_cast<FakeBus*>(user);
  bus->writeCalls++;
  if (data == nullptr || len == 0) {
    return SSD1315::Error(SSD1315::Err::INVALID_CONFIG, "invalid fake write args");
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

SSD1315::Config makeConfig(FakeBus& bus) {
  SSD1315::Config cfg;
  cfg.i2cWrite = fakeI2cWrite;
  cfg.i2cUser = &bus;
  cfg.nowMs = fakeNowMs;
  cfg.timeUser = &bus;
  cfg.i2cTimeoutMs = 10;
  cfg.offlineThreshold = 3;
  cfg.byteBudgetPerTick = 64;
  return cfg;
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

void test_begin_success_sets_ready_and_health() {
  FakeBus bus;
  SSD1315::SSD1315 display;
  const SSD1315::Status st = display.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(display.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SSD1315::DriverState::READY),
                          static_cast<uint8_t>(display.state()));
  TEST_ASSERT_TRUE(display.isOnline());
  TEST_ASSERT_GREATER_THAN_UINT32(0u, display.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(0u, display.totalFailures());
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

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_status_ok);
  RUN_TEST(test_status_helpers);
  RUN_TEST(test_config_defaults);
  RUN_TEST(test_canonical_api_symbols_exist);
  RUN_TEST(test_begin_requires_i2c_write_callback);
  RUN_TEST(test_begin_success_sets_ready_and_health);
  RUN_TEST(test_get_settings_snapshot);
  RUN_TEST(test_probe_failure_does_not_update_health);
  RUN_TEST(test_recover_failure_updates_health);
  RUN_TEST(test_recover_success_restores_ready);
  RUN_TEST(test_recover_reaches_offline_when_threshold_is_one);
  RUN_TEST(test_auto_sleep_timer_handles_wraparound);
  return UNITY_END();
}
