/**
 * @file Log.h
 * @brief Simple serial logging macros for examples.
 *
 * NOT part of the library API. The library itself does not log.
 * These macros are for example/application code only.
 */

#pragma once

#include <Arduino.h>

#include "examples/common/BuildConfig.h"

// Compile-time validation
#if LOG_LEVEL < 0 || LOG_LEVEL > 4
#error "LOG_LEVEL must be 0-4 (0=off, 1=error, 2=info, 3=debug, 4=trace)"
#endif

// Serial output handle - uses Serial on all platforms
// ESP32-S3 with ARDUINO_USB_CDC_ON_BOOT uses USB as Serial
// ESP32-S2 and others use hardware UART as Serial
#define LOG_SERIAL Serial

/**
 * @brief Initialize serial for logging.
 * @param baud Baud rate (default: 115200).
 */
inline void log_begin(unsigned long baud = 115200) {
  LOG_SERIAL.begin(baud);
  // Give USB CDC time to initialize on ESP32-S3
  #if defined(CONFIG_IDF_TARGET_ESP32S3) && ARDUINO_USB_CDC_ON_BOOT
  delay(100);
  #endif
}

/// @brief Log error message (level >= 1)
#define LOGE(fmt, ...) \
  do { \
    if (LOG_LEVEL >= 1) LOG_SERIAL.printf("[E] " fmt "\n", ##__VA_ARGS__); \
  } while (0)

/// @brief Log info message (level >= 2)
#define LOGI(fmt, ...) \
  do { \
    if (LOG_LEVEL >= 2) LOG_SERIAL.printf("[I] " fmt "\n", ##__VA_ARGS__); \
  } while (0)

/// @brief Log debug message (level >= 3)
#define LOGD(fmt, ...) \
  do { \
    if (LOG_LEVEL >= 3) LOG_SERIAL.printf("[D] " fmt "\n", ##__VA_ARGS__); \
  } while (0)

/// @brief Log trace message (level >= 4)
#define LOGT(fmt, ...) \
  do { \
    if (LOG_LEVEL >= 4) LOG_SERIAL.printf("[T] " fmt "\n", ##__VA_ARGS__); \
  } while (0)
