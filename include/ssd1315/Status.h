/**
 * @file Status.h
 * @brief Error handling types for SSD1315 OLED driver library.
 *
 * Provides a lightweight, zero-allocation error model. All error messages
 * are static string literals. Never allocate error strings dynamically.
 */

#pragma once

#include <stdint.h>

namespace ssd1315 {

/**
 * @brief Error code enumeration for SSD1315 driver operations.
 *
 * Covers all common embedded error scenarios and I2C-specific failures.
 * When using transport callbacks, map third-party errors to the most
 * appropriate Err code and store the original value in Status::detail.
 */
enum class Err : uint16_t {
  OK = 0,             ///< Success, no error

  // Configuration errors
  INVALID_CONFIG,     ///< Invalid configuration parameter (null pointer, out of range)
  INVALID_DIMENSIONS, ///< Invalid width/height (must be multiple of 8 for height)
  INVALID_PAGE_COUNT, ///< pageBufferPages out of valid range [1..totalPages]

  // State errors
  NOT_INITIALIZED,    ///< Library not initialized; begin() not called or failed
  STATE_ERROR,        ///< Invalid state for requested operation
  BUSY,               ///< Operation in progress; try again later
  PANEL_NOT_READY,    ///< Panel power-on timing not yet satisfied

  // I2C transport errors
  I2C_NACK_ADDR,      ///< I2C address not acknowledged (device not found)
  I2C_NACK_DATA,      ///< I2C data byte not acknowledged
  I2C_TIMEOUT,        ///< I2C operation timed out
  I2C_BUS_ERROR,      ///< I2C bus error (arbitration lost, etc.)

  // General errors
  TIMEOUT,            ///< Generic operation timeout
  BUFFER_OVERFLOW,    ///< Buffer size exceeded
  UNSUPPORTED,        ///< Operation not supported (e.g., read in I2C mode)
  INTERNAL_ERROR      ///< Internal logic error (bug in library code)
};

/**
 * @brief Operation result with error details.
 *
 * Returned by all fallible operations. Check with ok() or inspect code/msg.
 *
 * @note The msg field MUST point to a static string literal. Never assign
 *       dynamically allocated strings. This ensures zero heap allocation
 *       in error paths and safe usage across function boundaries.
 *
 * Example:
 * @code
 * Status st = display.begin(config);
 * if (!st.ok()) {
 *   Serial.printf("Error: %s (code=%d, detail=%ld)\n",
 *                 st.msg, (int)st.code, (long)st.detail);
 * }
 * @endcode
 */
struct Status {
  Err code = Err::OK;       ///< Error category
  int32_t detail = 0;       ///< Transport/vendor-specific error code (optional)
  const char* msg = "";     ///< Human-readable message (STATIC STRING ONLY)

  /**
   * @brief Default constructor - creates OK status.
   */
  constexpr Status() : code(Err::OK), detail(0), msg("") {}

  /**
   * @brief Constructor with all fields.
   * @param c Error code
   * @param d Detail/vendor error code
   * @param m Static string message
   */
  constexpr Status(Err c, int32_t d, const char* m) : code(c), detail(d), msg(m) {}

  /**
   * @brief Convenience constructor with code and message.
   * @param c Error code
   * @param m Static string message
   */
  constexpr Status(Err c, const char* m) : code(c), detail(0), msg(m) {}

  /**
   * @brief Check if operation succeeded.
   * @return true if code == Err::OK
   */
  constexpr bool ok() const { return code == Err::OK; }

  /**
   * @brief Implicit boolean conversion for if-style checks.
   * @return true if operation succeeded
   */
  explicit constexpr operator bool() const { return ok(); }
};

/**
 * @brief Create a success Status.
 * @return Status with Err::OK
 */
inline constexpr Status Ok() { return Status(Err::OK, 0, ""); }

/**
 * @brief Create an error Status with message.
 * @param c Error code
 * @param m Static string message
 * @return Status with error
 */
inline constexpr Status Error(Err c, const char* m) { return Status(c, 0, m); }

/**
 * @brief Create an error Status with message and detail.
 * @param c Error code
 * @param d Detail/vendor error code
 * @param m Static string message
 * @return Status with error
 */
inline constexpr Status Error(Err c, int32_t d, const char* m) { return Status(c, d, m); }

}  // namespace ssd1315
