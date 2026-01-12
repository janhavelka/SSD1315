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
inline ssd1315::Status wireWrite(uint8_t addr, const uint8_t* data, size_t len,
                                  uint32_t timeoutMs, void* user) {
  TwoWire* wire = static_cast<TwoWire*>(user);
  if (wire == nullptr) {
    return ssd1315::Error(ssd1315::Err::INVALID_CONFIG, "Wire instance is null");
  }

  // Set timeout if supported (ESP32 Arduino core supports this)
#if defined(ESP32)
  wire->setTimeOut(static_cast<uint16_t>(timeoutMs));
#else
  (void)timeoutMs;
#endif

  wire->beginTransmission(addr);
  size_t written = wire->write(data, len);

  if (written != len) {
    return ssd1315::Error(ssd1315::Err::I2C_BUS_ERROR, "Wire write incomplete");
  }

  uint8_t result = wire->endTransmission();

  switch (result) {
    case 0:  // Success
      return ssd1315::Ok();
    case 1:  // Data too long
      return ssd1315::Error(ssd1315::Err::BUFFER_OVERFLOW, "I2C data too long");
    case 2:  // NACK on address
      return ssd1315::Error(ssd1315::Err::I2C_NACK_ADDR, "I2C address NACK");
    case 3:  // NACK on data
      return ssd1315::Error(ssd1315::Err::I2C_NACK_DATA, "I2C data NACK");
    case 4:  // Other error
      return ssd1315::Error(ssd1315::Err::I2C_BUS_ERROR, "I2C bus error");
    case 5:  // Timeout
      return ssd1315::Error(ssd1315::Err::I2C_TIMEOUT, "I2C timeout");
    default:
      return ssd1315::Error(ssd1315::Err::I2C_BUS_ERROR, static_cast<int32_t>(result),
                            "I2C unknown error");
  }
}

/**
 * @brief Initialize Wire with default pins and frequency.
 *
 * @param sda SDA pin number
 * @param scl SCL pin number
 * @param freq I2C clock frequency in Hz (default 400kHz)
 * @return true on success
 */
inline bool initWire(int sda, int scl, uint32_t freq = 400000) {
  Wire.begin(sda, scl);
  Wire.setClock(freq);
  return true;
}

}  // namespace transport
