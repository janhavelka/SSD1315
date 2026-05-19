/**
 * @file I2cScanner.h
 * @brief Arduino Wire I2C scanner utility for examples.
 *
 * NOT part of the library API. ESP-IDF examples use native IDF scanner code in
 * their own `app_main()` source.
 */

#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "examples/common/Log.h"

namespace i2c_scanner {

inline void recoverBus(TwoWire& wire, int sda, int scl, uint32_t freqHz,
                       uint16_t timeoutMs) {
  pinMode(scl, OUTPUT);
  pinMode(sda, INPUT_PULLUP);
  for (uint8_t i = 0; i < 9; ++i) {
    digitalWrite(scl, LOW);
    delayMicroseconds(5);
    digitalWrite(scl, HIGH);
    delayMicroseconds(5);
  }
  pinMode(sda, OUTPUT);
  digitalWrite(sda, LOW);
  delayMicroseconds(5);
  digitalWrite(scl, HIGH);
  delayMicroseconds(5);
  digitalWrite(sda, HIGH);
  delayMicroseconds(5);
  wire.begin(sda, scl);
  wire.setClock(freqHz);
#if defined(ARDUINO_ARCH_ESP32)
  wire.setTimeOut(timeoutMs);
#else
  (void)timeoutMs;
#endif
}

inline void scan(TwoWire& wire, uint16_t timeoutMs = 50) {
#if defined(ARDUINO_ARCH_ESP32)
  wire.setTimeOut(timeoutMs);
#else
  (void)timeoutMs;
#endif

  uint8_t count = 0;
  LOGI("Scanning I2C bus...");
  for (uint8_t addr = 0x03; addr <= 0x77; ++addr) {
    wire.beginTransmission(addr);
    const uint8_t error = wire.endTransmission();
    if (error == 0) {
      LOG_SERIAL.printf("  0x%02X ACK\n", addr);
      ++count;
    }
    yield();
  }
  LOGI("Scan complete. Found %u device(s).", static_cast<unsigned>(count));
}

inline void scanDefault(uint16_t timeoutMs = 50) {
  scan(Wire, timeoutMs);
}

}  // namespace i2c_scanner
