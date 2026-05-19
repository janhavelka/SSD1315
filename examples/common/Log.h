/**
 * @file Log.h
 * @brief Simple serial logging macros for examples.
 *
 * NOT part of the library API. The library itself does not log.
 * These macros are for example/application code only.
 */

#pragma once

#if defined(SSD1315_EXAMPLE_PLATFORM_IDF)
#include "examples/common/IdfArduinoCompat.h"
#else
#include <Arduino.h>
#endif

#include "examples/common/BuildConfig.h"

// Compile-time validation
#if LOG_LEVEL < 0 || LOG_LEVEL > 4
#error "LOG_LEVEL must be 0-4 (0=off, 1=error, 2=info, 3=debug, 4=trace)"
#endif

// Serial output handle - uses Serial on all platforms.
// ESP32-S2/S3 examples enable native USB CDC, so Serial is the USB monitor.
#ifndef LOG_SERIAL
#define LOG_SERIAL Serial
#endif

#define LOG_COLOR_RESET  "\033[0m"
#define LOG_COLOR_RED    "\033[31m"
#define LOG_COLOR_GREEN  "\033[32m"
#define LOG_COLOR_YELLOW "\033[33m"
#define LOG_COLOR_BLUE   "\033[34m"
#define LOG_COLOR_CYAN   "\033[36m"
#define LOG_COLOR_GRAY   "\033[90m"
#define LOG_COLOR_RESULT(ok) ((ok) ? LOG_COLOR_GREEN : LOG_COLOR_RED)
#define LOG_COLOR_STATE(online, failures) \
  ((online) ? (((failures) > 0U) ? LOG_COLOR_YELLOW : LOG_COLOR_GREEN) : LOG_COLOR_RED)

inline const char* log_bool_str(bool value) { return value ? "yes" : "no"; }

/**
 * @brief Initialize serial for logging.
 * @param baud Baud rate (default: 115200).
 */
inline void log_begin(unsigned long baud = 115200) {
  LOG_SERIAL.begin(baud);
#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
#if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
  const uint32_t startMs = millis();
  while (!LOG_SERIAL && (millis() - startMs) < 1500U) {
    delay(10);
  }
  delay(50);
#endif
#endif
}

// Colorize only the severity tag; keep message text in terminal default color.
#define LOG_PRINT_WITH_TAG(tagColor, tag, fmt, ...) \
  LOG_SERIAL.printf(tagColor "[" tag "]" LOG_COLOR_RESET " " fmt "\n", ##__VA_ARGS__)

/// @brief Log error message (level >= 1)
#define LOGE(fmt, ...) \
  do { \
    if (LOG_LEVEL >= 1) LOG_PRINT_WITH_TAG(LOG_COLOR_RED, "E", fmt, ##__VA_ARGS__); \
  } while (0)

/// @brief Log warning message (level >= 2)
#define LOGW(fmt, ...) \
  do { \
    if (LOG_LEVEL >= 2) LOG_PRINT_WITH_TAG(LOG_COLOR_YELLOW, "W", fmt, ##__VA_ARGS__); \
  } while (0)

/// @brief Log info message (level >= 2)
#define LOGI(fmt, ...) \
  do { \
    if (LOG_LEVEL >= 2) LOG_PRINT_WITH_TAG(LOG_COLOR_CYAN, "I", fmt, ##__VA_ARGS__); \
  } while (0)

/// @brief Log debug message (level >= 3)
#define LOGD(fmt, ...) \
  do { \
    if (LOG_LEVEL >= 3) LOG_PRINT_WITH_TAG(LOG_COLOR_BLUE, "D", fmt, ##__VA_ARGS__); \
  } while (0)

/// @brief Log trace message (level >= 4)
#define LOGT(fmt, ...) \
  do { \
    if (LOG_LEVEL >= 4) LOG_PRINT_WITH_TAG(LOG_COLOR_GRAY, "T", fmt, ##__VA_ARGS__); \
  } while (0)

// Conditional verbose logging (runtime switch)
#define LOGV(verbose, fmt, ...) \
  do { \
    if (verbose) { \
      LOG_PRINT_WITH_TAG(LOG_COLOR_GRAY, "V", fmt, ##__VA_ARGS__); \
    } \
  } while (0)
