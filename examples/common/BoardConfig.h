/**
 * @file BoardConfig.h
 * @brief Example default board configuration for ESP32-S2 / ESP32-S3 reference hardware.
 *
 * These are convenience defaults for our reference designs only.
 * NOT part of the library API. Override for your hardware.
 *
 * @warning The library itself is pin-agnostic. The application transport owns
 *          pins and bus setup; these defaults are provided for examples only.
 */

#pragma once

#include <stdint.h>

namespace pins {

// ====================================================================
// EXAMPLE DEFAULT BOARD CONFIG - ESP32-S2 / ESP32-S3 REFERENCE HARDWARE
// ====================================================================
// These pins are NOT library defaults. They are example-only values.
// Override them for your board by creating your own BoardConfig.h or
// passing explicit values to Config structs in your application.
// ====================================================================

/// @brief I2C SDA pin (data line). Example default for ESP32-S2/S3.
/// Override for your hardware.
static constexpr int SDA = 8;

/// @brief I2C SCL pin (clock line). Example default for ESP32-S2/S3.
/// Override for your hardware.
static constexpr int SCL = 9;

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
