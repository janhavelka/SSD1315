/// @file test_i2c_transport.cpp
/// @brief Native contract tests for the example-only Arduino Wire adapter.

#include <unity.h>

#ifndef ARDUINO_ARCH_ESP32
#define SSD1315_NATIVE_DEFINED_ARDUINO_ARCH_ESP32
#define ARDUINO_ARCH_ESP32 1
#endif

#include "common/I2cTransport.h"

#ifdef SSD1315_NATIVE_DEFINED_ARDUINO_ARCH_ESP32
#undef ARDUINO_ARCH_ESP32
#undef SSD1315_NATIVE_DEFINED_ARDUINO_ARCH_ESP32
#endif

namespace {

void assertTransportResult(const SSD1315::TransportResult& result,
                           SSD1315::TransportCode expectedCode,
                           int32_t expectedDetail) {
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expectedCode),
                          static_cast<uint8_t>(result.code));
  TEST_ASSERT_EQUAL_INT32(expectedDetail, result.detail);
}

}  // namespace

void test_init_wire_propagates_pins_clock_and_timeout() {
  Wire.reset();
  Wire.beginResult = true;

  TEST_ASSERT_TRUE(transport::initWire(8, 9, 400000, 50, 0x3C));
  TEST_ASSERT_EQUAL_UINT32(1, Wire.beginCalls);
  TEST_ASSERT_EQUAL_INT(8, Wire.beginSda);
  TEST_ASSERT_EQUAL_INT(9, Wire.beginScl);
  TEST_ASSERT_EQUAL_UINT32(400000, Wire.clockHz);
  TEST_ASSERT_EQUAL_UINT32(1, Wire.setTimeOutCalls);
  TEST_ASSERT_EQUAL_UINT32(50, Wire.timeoutMs);
}

void test_init_wire_propagates_begin_failure() {
  Wire.reset();
  Wire.beginResult = false;

  TEST_ASSERT_FALSE(transport::initWire(4, 5, 100000, 20, 0x3D));
  TEST_ASSERT_EQUAL_UINT32(1, Wire.beginCalls);
  TEST_ASSERT_EQUAL_INT(4, Wire.beginSda);
  TEST_ASSERT_EQUAL_INT(5, Wire.beginScl);
  TEST_ASSERT_EQUAL_UINT32(100000, Wire.clockHz);
  TEST_ASSERT_EQUAL_UINT32(0, Wire.setTimeOutCalls);
  TEST_ASSERT_EQUAL_UINT32(0, Wire.endCalls);
}

void test_wire_write_accepts_128_and_rejects_129_before_bus_use() {
  uint8_t bytes[129] = {};
  for (size_t i = 0; i < sizeof(bytes); ++i) {
    bytes[i] = static_cast<uint8_t>(i);
  }

  Wire.reset();
  const SSD1315::TransportResult accepted =
      transport::wireWrite(0x3C, bytes, 128, 70000, &Wire);
  assertTransportResult(accepted, SSD1315::TransportCode::OK, 0);
  TEST_ASSERT_EQUAL_UINT32(1, Wire.setTimeOutCalls);
  TEST_ASSERT_EQUAL_UINT32(65535, Wire.timeoutMs);
  TEST_ASSERT_EQUAL_UINT32(1, Wire.beginTransmissionCalls);
  TEST_ASSERT_EQUAL_HEX8(0x3C, Wire.transmissionAddress);
  TEST_ASSERT_EQUAL_UINT32(1, Wire.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(128, static_cast<uint32_t>(Wire.requestedWriteLength));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(bytes, Wire.writtenData, 128);
  TEST_ASSERT_EQUAL_UINT32(1, Wire.endTransmissionCalls);
  TEST_ASSERT_TRUE(Wire.endTransmissionSendStop);

  Wire.reset();
  const SSD1315::TransportResult rejected =
      transport::wireWrite(0x3C, bytes, 129, 50, &Wire);
  assertTransportResult(rejected, SSD1315::TransportCode::BUS_ERROR, 129);
  TEST_ASSERT_EQUAL_UINT32(0, Wire.setTimeOutCalls);
  TEST_ASSERT_EQUAL_UINT32(0, Wire.beginTransmissionCalls);
  TEST_ASSERT_EQUAL_UINT32(0, Wire.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0, Wire.endTransmissionCalls);
}

void test_wire_write_partial_result_stops_before_end_transmission() {
  uint8_t bytes[16] = {};
  Wire.reset();
  Wire.writeResult = 7;

  const SSD1315::TransportResult result =
      transport::wireWrite(0x3D, bytes, sizeof(bytes), 25, &Wire);

  assertTransportResult(result, SSD1315::TransportCode::BUS_ERROR, 7);
  TEST_ASSERT_EQUAL_UINT32(1, Wire.setTimeOutCalls);
  TEST_ASSERT_EQUAL_UINT32(25, Wire.timeoutMs);
  TEST_ASSERT_EQUAL_UINT32(1, Wire.beginTransmissionCalls);
  TEST_ASSERT_EQUAL_HEX8(0x3D, Wire.transmissionAddress);
  TEST_ASSERT_EQUAL_UINT32(1, Wire.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0, Wire.endTransmissionCalls);
}

void test_wire_result_mapping_is_exact() {
  struct Mapping {
    uint8_t wireCode;
    SSD1315::TransportCode expectedCode;
  };
  const Mapping mappings[] = {
      {0, SSD1315::TransportCode::OK},
      {2, SSD1315::TransportCode::NACK_ADDRESS},
      {3, SSD1315::TransportCode::NACK_DATA},
      {5, SSD1315::TransportCode::TIMEOUT},
      {1, SSD1315::TransportCode::BUS_ERROR},
      {4, SSD1315::TransportCode::BUS_ERROR},
      {99, SSD1315::TransportCode::BUS_ERROR},
  };

  for (const Mapping& mapping : mappings) {
    const SSD1315::TransportResult result =
        transport::mapWireResult(mapping.wireCode);
    const int32_t expectedDetail = mapping.wireCode == 0 ? 0 : mapping.wireCode;
    assertTransportResult(result, mapping.expectedCode, expectedDetail);
  }
}
