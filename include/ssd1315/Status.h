/**
 * @file Status.h
 * @brief Error handling types for SSD1315 OLED driver library.
 *
 * Provides a lightweight, zero-allocation error model. All error messages
 * are static string literals. Never allocate error strings dynamically.
 */

#pragma once

#include <cstddef>
#include <stdint.h>

namespace SSD1315 {

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
  INVALID_DIMENSIONS, ///< Invalid width/height (height must be 16..64, multiple of 8)
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
  I2C_BUS = I2C_BUS_ERROR, ///< Compatibility alias for I2C bus error

  // General errors
  TIMEOUT,            ///< Generic operation timeout
  BUFFER_OVERFLOW,    ///< Buffer size exceeded
  UNSUPPORTED,        ///< Operation not supported (e.g., read in I2C mode)
  INTERNAL_ERROR,     ///< Internal logic error (bug in library code)
  DEVICE_NOT_FOUND,   ///< Device not present at expected address
  IN_PROGRESS         ///< Operation in progress (not an error)
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
   * @brief Check whether this status matches a specific code.
   * @param err Error code to compare against.
   * @return true if code == err
   */
  constexpr bool is(Err err) const { return code == err; }

  /**
   * @brief Check if operation is still in progress (not an error).
   * @return true if code == Err::IN_PROGRESS
   */
  constexpr bool inProgress() const { return code == Err::IN_PROGRESS; }

  /**
   * @brief Implicit boolean conversion for success checks.
   * @return true if operation succeeded
   */
  explicit constexpr operator bool() const { return ok(); }

  /**
   * @brief Create a success Status.
   * @return Status with Err::OK
   */
  static constexpr Status Ok() { return Status(Err::OK, 0, ""); }

  /**
   * @brief Create an error Status with message.
   * @param c Error code
   * @param m Static string message
   * @return Status with error
   */
  static constexpr Status Error(Err c, const char* m) { return Status(c, 0, m); }

  /**
   * @brief Create an error Status with message and detail.
   * @param c Error code
   * @param d Detail/vendor error code
   * @param m Static string message
   * @return Status with error
   */
  static constexpr Status Error(Err c, int32_t d, const char* m) {
    return Status(c, d, m);
  }

  /**
   * @brief Create an error Status with message and detail.
   * @param c Error code
   * @param m Static string message
   * @param d Detail/vendor error code
   * @return Status with error
   */
  static constexpr Status Error(Err c, const char* m, int32_t d) {
    return Status(c, d, m);
  }

};

/**
 * @brief Create a success Status.
 * @return Status with Err::OK
 */
inline constexpr Status Ok() { return Status::Ok(); }

/**
 * @brief Create an error Status with message.
 * @param c Error code
 * @param m Static string message
 * @return Status with error
 */
inline constexpr Status Error(Err c, const char* m) { return Status::Error(c, m); }

/**
 * @brief Create an error Status with message and detail.
 * @param c Error code
 * @param d Detail/vendor error code
 * @param m Static string message
 * @return Status with error
 */
inline constexpr Status Error(Err c, int32_t d, const char* m) {
  return Status::Error(c, d, m);
}

/**
 * @brief Create an error Status with message and detail.
 * @param c Error code
 * @param m Static string message
 * @param d Detail/vendor error code
 * @return Status with error
 */
inline constexpr Status Error(Err c, const char* m, int32_t d) {
  return Status::Error(c, m, d);
}

/**
 * @brief Driver health indicator (NOT a lifecycle state machine).
 *
 * DriverState reflects the outcome of recent I2C transactions. It is NOT
 * a lifecycle FSM - there are no time-based transitions, no background
 * checks, and no automatic state changes without actual I2C activity.
 *
 * State changes occur ONLY when:
 * - An I2C transaction succeeds (→ READY)
 * - An I2C transaction fails (→ DEGRADED or OFFLINE based on threshold)
 * - begin() or end() is called (→ UNINIT or READY)
 *
 * IMPORTANT: OFFLINE is latched for normal public operations. Once OFFLINE,
 * those operations return BUSY without touching I2C until the application calls
 * recover(). Explicit diagnostics/recovery APIs may still use I2C.
 */
enum class DriverState : uint8_t {
  UNINIT,    ///< Driver not initialized or init in progress.
             ///< This is a structural state, not a health state.
             ///< During init (when _initialized==true): first I2C success → READY,
             ///< first I2C failure → DEGRADED (or OFFLINE if threshold is 1).

  READY,     ///< Last I2C transaction succeeded. Device is healthy.
             ///< This is the normal operating state.

  DEGRADED,  ///< 1 to (N-1) consecutive failures occurred.
             ///< Device may still respond - worth retrying.
             ///< Success before OFFLINE returns READY. Nth failure -> OFFLINE.

  OFFLINE    ///< N or more consecutive failures occurred.
             ///< Device assumed missing or unresponsive.
             ///< Application should call recover() or investigate.
};

/// @brief Snapshot of configuration and runtime state without performing I2C.
struct SettingsSnapshot {
  bool initialized = false;
  DriverState state = DriverState::UNINIT;
  uint8_t i2cAddress = 0x3C;
  uint32_t i2cTimeoutMs = 25;
  uint8_t offlineThreshold = 3;
  bool hasNowMsHook = false;
  bool hasCooperativeYieldHook = false;
  bool hasI2cWriteReadHook = false;

  uint8_t width = 128;
  uint8_t height = 64;
  uint8_t pageBufferPages = 8;
  uint8_t totalPages = 8;
  bool pageBufferMode = false;
  bool sleeping = true;
  bool allPixelsOn = false;
  uint8_t userPageCount = 1;
  uint8_t activeUserPage = 0;
  uint8_t currentPageIndex = 0;
  bool pageIterationActive = false;
  uint32_t byteBudgetPerTick = 128;
  uint32_t flushTimeoutMs = 1000;
  uint32_t displayOnDelayMs = 100;
  uint32_t inactivitySleepMs = 0;
  uint32_t pageCycleMs = 0;
  bool flipX = false;
  bool flipY = false;
  bool invert = false;
  uint8_t contrast = 0x7F;
  bool hasExternalBuffer = false;
  bool ownsBuffer = false;
  size_t bufferSize = 0;
  uint8_t dirtyPages = 0;
  bool flushing = false;

  uint32_t lastOkMs = 0;
  uint32_t lastErrorMs = 0;
  uint8_t consecutiveFailures = 0;
  uint32_t totalFailures = 0;
  uint32_t totalSuccess = 0;
  Status lastError = Status::Ok();
};

}  // namespace SSD1315
