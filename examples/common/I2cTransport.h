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
 * @return Status OK on success, I2C error on failure
 */
inline SSD1315::Status wireWrite(uint8_t addr, const uint8_t* data, size_t len,
                                  uint32_t timeoutMs, void* user) {
  TwoWire* wire = static_cast<TwoWire*>(user);
  if (wire == nullptr) {
    return SSD1315::Error(SSD1315::Err::INVALID_CONFIG, "Wire instance is null");
  }
  
  // Check for oversized writes (ESP32 Wire buffer is 128 bytes)
  if (len > 128) {
    return SSD1315::Error(SSD1315::Err::BUFFER_OVERFLOW, 
                          static_cast<int32_t>(len), "Write exceeds I2C buffer");
  }

  // Set timeout if supported (ESP32 Arduino core supports this)
#if defined(ARDUINO_ARCH_ESP32)
  wire->setTimeOut(static_cast<uint16_t>(timeoutMs));
#else
  (void)timeoutMs;
#endif

  wire->beginTransmission(addr);
  size_t written = wire->write(data, len);

  if (written != len) {
    // Return detailed error with actual bytes written
    return SSD1315::Error(SSD1315::Err::I2C_BUS_ERROR, 
                          static_cast<int32_t>(written), "Wire write incomplete");
  }

  uint8_t result = wire->endTransmission(true);  // Send STOP

  switch (result) {
    case 0:  // Success
      return SSD1315::Ok();
    case 1:  // Data too long
      return SSD1315::Error(SSD1315::Err::BUFFER_OVERFLOW, "I2C data too long");
    case 2:  // NACK on address
      return SSD1315::Error(SSD1315::Err::I2C_NACK_ADDR, "I2C address NACK");
    case 3:  // NACK on data
      return SSD1315::Error(SSD1315::Err::I2C_NACK_DATA, "I2C data NACK");
    case 4:  // Other error
      return SSD1315::Error(SSD1315::Err::I2C_BUS_ERROR, "I2C bus error");
    case 5:  // Timeout
      return SSD1315::Error(SSD1315::Err::I2C_TIMEOUT, "I2C timeout");
    default:
      return SSD1315::Error(SSD1315::Err::I2C_BUS_ERROR, static_cast<int32_t>(result),
                            "I2C unknown error");
  }
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

  Wire.begin(sda, scl);
  Wire.setClock(freq);
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

}  // namespace transport

