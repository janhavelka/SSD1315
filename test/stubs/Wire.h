/// @file Wire.h
/// @brief Minimal Wire stub for native testing
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

class TwoWire {
public:
  static constexpr size_t WRITE_ALL = static_cast<size_t>(-1);

  bool beginResult = true;
  size_t writeResult = WRITE_ALL;
  uint8_t endTransmissionResult = 0;

  uint32_t beginCalls = 0;
  uint32_t setTimeOutCalls = 0;
  uint32_t endCalls = 0;
  uint32_t beginTransmissionCalls = 0;
  uint32_t writeCalls = 0;
  uint32_t endTransmissionCalls = 0;

  int beginSda = -1;
  int beginScl = -1;
  uint32_t clockHz = 0;
  uint32_t timeoutMs = 0;
  uint8_t transmissionAddress = 0;
  bool endTransmissionSendStop = false;
  size_t requestedWriteLength = 0;
  uint8_t writtenData[128] = {};

  bool begin(int sda = -1, int scl = -1, uint32_t frequency = 0) {
    beginCalls++;
    beginSda = sda;
    beginScl = scl;
    clockHz = frequency;
    return beginResult;
  }

  void setTimeOut(uint32_t timeout) {
    setTimeOutCalls++;
    timeoutMs = timeout;
  }

  void beginTransmission(uint8_t address) {
    beginTransmissionCalls++;
    transmissionAddress = address;
  }

  size_t write(const uint8_t* data, size_t len) {
    writeCalls++;
    requestedWriteLength = len;
    const size_t reported = writeResult == WRITE_ALL ? len : writeResult;
    const size_t copied = len < sizeof(writtenData) ? len : sizeof(writtenData);
    if (data != nullptr && copied > 0) {
      std::memcpy(writtenData, data, copied);
    }
    return reported;
  }

  uint8_t endTransmission(bool sendStop) {
    endTransmissionCalls++;
    endTransmissionSendStop = sendStop;
    return endTransmissionResult;
  }

  void end() { endCalls++; }

  void reset() {
    *this = TwoWire{};
  }
};

extern TwoWire Wire;
