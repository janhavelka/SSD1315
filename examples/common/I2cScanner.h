/**
 * @file I2cScanner.h
 * @brief Simple I2C bus scanner utility for examples.
 *
 * NOT part of the library API. This is a diagnostic tool for examples.
 */

#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "examples/common/Log.h"

namespace i2c_scanner {

/**
 * @brief Attempt to recover a stuck I2C bus by toggling SCL.
 * @param sda SDA pin number
 * @param scl SCL pin number  
 */
inline void recoverBus(int sda, int scl) {
  // Release SDA/SCL from Wire
  Wire.end();
  
  // Manually toggle SCL to release any stuck slave
  pinMode(scl, OUTPUT);
  pinMode(sda, INPUT_PULLUP);
  
  for (int i = 0; i < 9; i++) {
    digitalWrite(scl, LOW);
    delayMicroseconds(5);
    digitalWrite(scl, HIGH);
    delayMicroseconds(5);
    // Check if SDA is released
    if (digitalRead(sda)) {
      break;
    }
  }
  
  // Generate STOP condition
  pinMode(sda, OUTPUT);
  digitalWrite(sda, LOW);
  delayMicroseconds(5);
  digitalWrite(scl, HIGH);
  delayMicroseconds(5);
  digitalWrite(sda, HIGH);
  delayMicroseconds(5);
  
  // Reinitialize Wire
  Wire.begin(sda, scl);
}

/**
 * @brief Scan I2C bus and print found devices.
 * @param wire Reference to Wire object (must be initialized).
 * @param timeoutMs Timeout per address probe in milliseconds (default 50ms).
 */
inline void scan(TwoWire& wire, uint16_t timeoutMs = 50) {
  LOGI("Scanning I2C bus (timeout=%dms)...", timeoutMs);
  LOG_SERIAL.flush();
  
  // Set Wire timeout (ESP32 Arduino core)
#if defined(ARDUINO_ARCH_ESP32)
  wire.setTimeOut(timeoutMs);
#endif

  LOGI("     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F");
  LOG_SERIAL.flush();

  uint8_t count = 0;
  for (uint8_t row = 0; row < 8; row++) {
    LOG_SERIAL.printf("%02X: ", row * 16);
    LOG_SERIAL.flush();

    for (uint8_t col = 0; col < 16; col++) {
      uint8_t addr = row * 16 + col;

      // Skip reserved addresses
      if (addr < 0x08 || addr > 0x77) {  // Skip 0x00-0x07 reserved range
        LOG_SERIAL.print("   ");
        continue;
      }

      // Probe address with timeout protection
      wire.beginTransmission(addr);
      uint8_t error = wire.endTransmission(true);  // Send STOP

      if (error == 0) {
        LOG_SERIAL.printf("%02X ", addr);
        count++;
      } else if (error == 5) {
        // Timeout - mark differently
        LOG_SERIAL.print("TO ");
      } else {
        LOG_SERIAL.print("-- ");
      }
      
      // Yield to prevent watchdog reset
      yield();
      delay(1);  // Small delay between probes
    }
    LOG_SERIAL.println();
    LOG_SERIAL.flush();
  }

  LOGI("Scan complete. Found %d device(s).", count);
  LOG_SERIAL.flush();

  if (count > 0) {
    LOGI("Common addresses: 0x3C/0x3D=OLED, 0x48-0x4B=ADS1115, 0x51=RV3032, 0x76/0x77=BME280");
  }
}

}  // namespace i2c_scanner
