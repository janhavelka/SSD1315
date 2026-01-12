/**
 * @file BoardPins.h
 * @brief Example default pin mapping for ESP32-S2 / ESP32-S3 reference hardware.
 *
 * These are convenience defaults for our reference designs only.
 * NOT part of the library API. Override for your hardware.
 *
 * @warning The library itself is pin-agnostic. All pins are passed via Config.
 *          These defaults are provided for examples only.
 */

#pragma once

#include <stdint.h>

namespace pins {

// ====================================================================
// EXAMPLE DEFAULT PIN MAPPING - ESP32-S2 / ESP32-S3 REFERENCE HARDWARE
// ====================================================================
// These pins are NOT library defaults. They are example-only values.
// Override them for your board by creating your own BoardPins.h or
// passing explicit values to Config structs in your application.
// ====================================================================

/// @brief I2C SDA pin (data line). Example default for ESP32-S2/S3.
/// Override for your hardware.
static constexpr int SDA = 8;

/// @brief I2C SCL pin (clock line). Example default for ESP32-S2/S3.
/// Override for your hardware.
static constexpr int SCL = 9;

/// @brief SPI MOSI pin (master out, slave in). Example default for ESP32-S2/S3.
/// Override for your hardware.
static constexpr int SPI_MOSI = 11;

/// @brief SPI SCK pin (serial clock). Example default for ESP32-S2/S3.
/// Override for your hardware.
static constexpr int SPI_SCK = 12;

/// @brief SPI MISO pin (master in, slave out). Example default for ESP32-S2/S3.
/// Override for your hardware.
static constexpr int SPI_MISO = 13;

/// @brief LED pin. Example default for ESP32-S3 (RGB LED on GPIO48).
/// Override for your hardware. Set to -1 to disable.
static constexpr int LED = 48;

// ====================================================================
// OLED DISPLAY CONFIGURATION
// ====================================================================

/// @brief OLED I2C address. Most modules use 0x3C.
/// Some use 0x3D if SA0/D/C# pin is tied high.
static constexpr uint8_t OLED_I2C_ADDR = 0x3C;

/// @brief OLED display width in pixels.
static constexpr uint8_t OLED_WIDTH = 128;

/// @brief OLED display height in pixels.
static constexpr uint8_t OLED_HEIGHT = 64;

/// @brief I2C clock frequency in Hz.
/// SSD1315 supports up to 400kHz (Fast Mode).
static constexpr uint32_t I2C_FREQ = 400000;

}  // namespace pins
