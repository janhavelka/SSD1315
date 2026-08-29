/**
 * @file I2cTransport.h
 * @brief Wire-based I2C transport adapter for SSD1315 examples.
 *
 * This file provides a Wire-compatible I2C write callback that can be
 * used with the SSD1315 driver. The library does not depend on Wire
 * directly; this adapter bridges them.
 *
 * NOT part of the library API. Example-only.
 */

#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "ssd1315/Status.h"

namespace transport {

inline SSD1315::TransportResult mapWireResult(uint8_t result) {
  switch (result) {
    case 0:
      return SSD1315::TransportResult::Ok();
    case 2:
      return SSD1315::TransportResult::NackAddress(result);
    case 3:
      return SSD1315::TransportResult::NackData(result);
    case 5:
      return SSD1315::TransportResult::Timeout(result);
    case 1:
    case 4:
    default:
      return SSD1315::TransportResult::BusError(result);
  }
}

/**
 * @brief Wire-based I2C write implementation.
 *
 * This callback wraps Arduino Wire for use with the SSD1315 driver.
 * Pass to Config::i2cWrite, and pass &Wire (or custom TwoWire*) to i2cUser.
 *
 * @param addr I2C 7-bit address
 * @param data Data buffer to send
 * @param len Number of bytes
 * @param timeoutMs Timeout (used to set Wire timeout if supported)
 * @param user Pointer to TwoWire instance
 * @return Terminal callback result. OK confirms one complete physical write;
 *         a pre-bus adapter rejection is also terminal.
 */
inline SSD1315::TransportResult wireWrite(uint8_t addr, const uint8_t* data,
                                          size_t len, uint32_t timeoutMs,
                                          void* user) {
  TwoWire* wire = static_cast<TwoWire*>(user);
  if (wire == nullptr) {
    return SSD1315::TransportResult::BusError(-1);
  }
  
  // Check for oversized writes (ESP32 Wire buffer is 128 bytes)
  if (len > 128) {
    return SSD1315::TransportResult::BusError(static_cast<int32_t>(len));
  }

  // Set timeout if supported (ESP32 Arduino core supports this)
#if defined(ARDUINO_ARCH_ESP32)
  wire->setTimeOut(static_cast<uint16_t>(timeoutMs > 65535U ? 65535U : timeoutMs));
#else
  (void)timeoutMs;
#endif

  wire->beginTransmission(addr);
  size_t written = wire->write(data, len);

  if (written != len) {
    // Close the transmission we opened. arduino-esp32 takes its bus lock in
    // beginTransmission() and releases it only in endTransmission(); returning
    // here without it strands the lock for every other device on the bus.
    (void)wire->endTransmission(true);
    return SSD1315::TransportResult::BusError(static_cast<int32_t>(written));
  }

  uint8_t result = wire->endTransmission(true);  // Send STOP
  return mapWireResult(result);
}

/**
 * @brief Initialize Wire with default pins and frequency.
 *
 * @param sda SDA pin number
 * @param scl SCL pin number
 * @param freq I2C clock frequency in Hz (default 400kHz)
 * @param timeoutMs I2C timeout in milliseconds (default 50ms)
 * @return true on success
 */
inline bool initWire(int sda, int scl, uint32_t freq = 400000, uint16_t timeoutMs = 50,
                     uint8_t address = 0x3C) {
  (void)address;
  // First, try to recover the bus in case it's stuck from a previous crash
#if defined(ARDUINO_ARCH_ESP32)
  // Toggle SCL to release any stuck slave
  pinMode(scl, OUTPUT);
  pinMode(sda, INPUT_PULLUP);
  for (int i = 0; i < 9; i++) {
    digitalWrite(scl, LOW);
    delayMicroseconds(5);
    digitalWrite(scl, HIGH);
    delayMicroseconds(5);
  }
  // Generate STOP condition
  pinMode(sda, OUTPUT);
  digitalWrite(sda, LOW);
  delayMicroseconds(5);
  digitalWrite(scl, HIGH);
  delayMicroseconds(5);
  digitalWrite(sda, HIGH);
  delayMicroseconds(5);
#endif

  // Configure the clock as part of bus creation. A redundant setClock() call
  // can report failure on some Arduino-ESP32 I2C HAL versions even though the
  // requested frequency was applied and no device handle exists yet.
  if (!Wire.begin(sda, scl, freq)) {
    return false;
  }
#if defined(ARDUINO_ARCH_ESP32)
  Wire.setTimeOut(timeoutMs);  // Critical: set timeout to prevent I2C hangs
#else
  (void)timeoutMs;
#endif
  return true;
}

inline void* configUser() {
  return &Wire;
}

inline uint32_t nowMs(void* user) {
  (void)user;
  return millis();
}

inline void cooperativeYield(void* user) {
  (void)user;
  yield();
}

}  // namespace transport
