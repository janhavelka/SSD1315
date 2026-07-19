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
 * @brief Terminal result code returned by one transport callback invocation.
 *
 * Transport callbacks are synchronous and may return only these terminal
 * outcomes. Library operation state such as in-progress or cancelled is
 * represented separately by Status/Err.
 */
enum class TransportCode : uint8_t {
  OK = 0,       ///< The complete physical transaction was confirmed successful.
  NACK_ADDRESS, ///< The addressed device did not acknowledge its address.
  NACK_DATA,    ///< A transmitted data byte was not acknowledged.
  TIMEOUT,      ///< The physical transaction did not complete before its timeout.
  BUS_ERROR     ///< Arbitration, bus, or another terminal transport failure.
};

/**
 * @brief Fixed-value result of one synchronous transport callback invocation.
 *
 * This type owns no resources and is trivially copyable. The optional detail
 * value preserves a platform/vendor error code without borrowing an error
 * message pointer.
 */
struct TransportResult {
  TransportCode code = TransportCode::OK;  ///< Terminal transport outcome.
  int32_t detail = 0;                      ///< Optional platform/vendor error code.

  /** @brief Construct a successful transport result. */
  constexpr TransportResult() : code(TransportCode::OK), detail(0) {}

  /**
   * @brief Construct a terminal transport result.
   * @param resultCode Terminal outcome code.
   * @param resultDetail Optional platform/vendor error code.
   */
  constexpr TransportResult(TransportCode resultCode, int32_t resultDetail = 0)
      : code(resultCode), detail(resultDetail) {}

  /**
   * @brief Check whether the physical transaction was confirmed successful.
   * @return true only for TransportCode::OK.
   */
  constexpr bool ok() const { return code == TransportCode::OK; }

  /** @brief Create a confirmed-success result. */
  static constexpr TransportResult Ok() {
    return TransportResult(TransportCode::OK);
  }

  /**
   * @brief Create an address-NACK result.
   * @param detail Optional platform/vendor error code.
   */
  static constexpr TransportResult NackAddress(int32_t detail = 0) {
    return TransportResult(TransportCode::NACK_ADDRESS, detail);
  }

  /**
   * @brief Create a data-NACK result.
   * @param detail Optional platform/vendor error code.
   */
  static constexpr TransportResult NackData(int32_t detail = 0) {
    return TransportResult(TransportCode::NACK_DATA, detail);
  }

  /**
   * @brief Create a timeout result.
   * @param detail Optional platform/vendor error code.
   */
  static constexpr TransportResult Timeout(int32_t detail = 0) {
    return TransportResult(TransportCode::TIMEOUT, detail);
  }

  /**
   * @brief Create a bus-error result.
   * @param detail Optional platform/vendor error code.
   */
  static constexpr TransportResult BusError(int32_t detail = 0) {
    return TransportResult(TransportCode::BUS_ERROR, detail);
  }
};

/**
 * @brief Return a library-owned static name for a transport result code.
 * @param code Transport result code to describe.
 * @return Static string literal with process lifetime; never null.
 */
inline const char* toString(TransportCode code) {
  switch (code) {
    case TransportCode::OK: return "OK";
    case TransportCode::NACK_ADDRESS: return "NACK_ADDRESS";
    case TransportCode::NACK_DATA: return "NACK_DATA";
    case TransportCode::TIMEOUT: return "TIMEOUT";
    case TransportCode::BUS_ERROR: return "BUS_ERROR";
  }
  return "UNKNOWN";
}

/**
 * @brief Supported controller initialization/profile contract.
 *
 * The driver currently implements and validates the SSD1315 command profile.
 * SSD1306-like panels may accept many of the same commands, but they are not
 * represented by this enum until SSD1306-specific command guards and hardware
 * validation exist.
 */
enum class ControllerProfile : uint8_t {
  SSD1315 = 0  ///< SSD1315 controller profile; includes SSD1315 SET_IREF.
};

/**
 * @brief Return a library-owned static controller-profile name.
 * @param profile Controller profile value.
 * @return Static string literal with process lifetime; never null.
 */
inline const char* toString(ControllerProfile profile) {
  switch (profile) {
    case ControllerProfile::SSD1315: return "SSD1315";
  }
  return "UNKNOWN";
}

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
  NOT_INITIALIZED,    ///< Controller initialization has not completed successfully
  STATE_ERROR,        ///< Invalid state for requested operation
  BUSY,               ///< Transient operation conflict; try again later
  PANEL_NOT_READY,    ///< Requested work requires command-confirmed modeled power
  CANCELLED,          ///< Operation was explicitly cancelled before completion
  CONTROL_STATE_UNKNOWN, ///< Cached controls require complete resynchronization
  RESULT_NOT_AVAILABLE,  ///< No unconsumed terminal operation result is available

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
  IN_PROGRESS,        ///< Operation in progress (not an error)
  BUFFER_TOO_SMALL,   ///< Caller-provided buffer is smaller than required
  DRIVER_OFFLINE      ///< Legacy compatibility code; OFFLINE is diagnostic-only
};

/**
 * @brief Return a library-owned static name for a driver status code.
 * @param code Driver status code to describe.
 * @return Static string literal with process lifetime; never null.
 */
inline const char* toString(Err code) {
  switch (code) {
    case Err::OK: return "OK";
    case Err::INVALID_CONFIG: return "INVALID_CONFIG";
    case Err::INVALID_DIMENSIONS: return "INVALID_DIMENSIONS";
    case Err::INVALID_PAGE_COUNT: return "INVALID_PAGE_COUNT";
    case Err::NOT_INITIALIZED: return "NOT_INITIALIZED";
    case Err::STATE_ERROR: return "STATE_ERROR";
    case Err::BUSY: return "BUSY";
    case Err::PANEL_NOT_READY: return "PANEL_NOT_READY";
    case Err::CANCELLED: return "CANCELLED";
    case Err::CONTROL_STATE_UNKNOWN: return "CONTROL_STATE_UNKNOWN";
    case Err::RESULT_NOT_AVAILABLE: return "RESULT_NOT_AVAILABLE";
    case Err::I2C_NACK_ADDR: return "I2C_NACK_ADDR";
    case Err::I2C_NACK_DATA: return "I2C_NACK_DATA";
    case Err::I2C_TIMEOUT: return "I2C_TIMEOUT";
    case Err::I2C_BUS_ERROR: return "I2C_BUS_ERROR";
    case Err::TIMEOUT: return "TIMEOUT";
    case Err::BUFFER_OVERFLOW: return "BUFFER_OVERFLOW";
    case Err::UNSUPPORTED: return "UNSUPPORTED";
    case Err::INTERNAL_ERROR: return "INTERNAL_ERROR";
    case Err::DEVICE_NOT_FOUND: return "DEVICE_NOT_FOUND";
    case Err::IN_PROGRESS: return "IN_PROGRESS";
    case Err::BUFFER_TOO_SMALL: return "BUFFER_TOO_SMALL";
    case Err::DRIVER_OFFLINE: return "DRIVER_OFFLINE";
  }
  return "UNKNOWN";
}

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
 *   handleDisplayError(st.code, st.detail, st.msg);
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
 * Direct compatibility calls update health from their terminal I2C attempt.
 * A cooperative multi-transaction operation suppresses intermediate health
 * publication and contributes exactly one success or failure when the whole
 * operation reaches a terminal state. Cancellation does not count as a
 * communication failure. attach()/detach() reset the diagnostic counters.
 *
 * OFFLINE is diagnostic only. It reports accumulated communication failures but
 * never takes admission or recovery authority away from the external bus owner.
 * A later explicit owner operation remains eligible to attempt transport I/O.
 */
enum class DriverState : uint8_t {
  UNINIT,    ///< Controller lifecycle is uninitialized for this binding.
             ///< Diagnostic counters can still record a failed init attempt.

  READY,     ///< Last counted direct callback or complete operation succeeded.
             ///< This is a communication diagnostic, not hardware readback.

  DEGRADED,  ///< 1 to (N-1) consecutive counted failures occurred.
             ///< The external owner decides whether/when to retry.
             ///< Success before OFFLINE returns READY. Nth failure -> OFFLINE.

  OFFLINE    ///< N or more consecutive failures occurred.
             ///< Device may be missing or unresponsive. This diagnostic does
             ///< not block a later explicit owner-directed operation.
};

/**
 * @brief Command-confirmed, locally modeled panel power state.
 * @note SSD1315 I2C provides no hardware-state readback. No value proves
 *       controller identity or visible/electrical panel state.
 */
enum class PanelPowerState : uint8_t {
  UNKNOWN = 0, ///< No trustworthy complete command sequence models the state.
  OFF,         ///< DISPLAY_OFF write returned terminal transport success.
  STARTING,    ///< DISPLAY_ON write succeeded; configured timing guard is active.
  ON           ///< DISPLAY_ON succeeded and the configured timing guard elapsed.
};

/**
 * @brief Public snapshot phase for the framebuffer flush job.
 *
 * Each command or data transaction is one instruction. Column and page address
 * setup are represented separately so callers can verify instruction budgets.
 */
enum class FlushPhase : uint8_t {
  IDLE,           ///< No flush job is active.
  SET_COL_ADDR,   ///< Next instruction sets the column address window.
  SET_PAGE_ADDR,  ///< Next instruction sets the page address window.
  SEND_DATA,      ///< Next instruction sends one bounded data chunk.
  DONE,           ///< Flush completed and awaits final accounting.
  ERROR           ///< Flush failed and awaits final accounting.
};

/**
 * @brief Snapshot of the current flush job without performing I2C.
 */
struct FlushStatus {
  FlushPhase phase = FlushPhase::IDLE;  ///< Current flush phase.
  bool inProgress = false;              ///< true while command/data instructions remain.
  uint8_t dirtyPages = 0;               ///< Dirty page bitmask.
  uint8_t currentPage = 0;              ///< Physical page currently being flushed.
  uint8_t endPage = 0;                  ///< Last page considered by the active job.
  uint16_t currentColumn = 0;           ///< Next data column for SEND_DATA.
  uint8_t minColumn = 0;                ///< Current page dirty-window minimum column.
  uint8_t maxColumn = 0;                ///< Current page dirty-window maximum column.
  uint32_t bytesCompleted = 0;          ///< Confirmed framebuffer payload bytes.
  uint16_t dataChunkCount = 0;          ///< Confirmed data transactions.
  uint16_t transactionCount = 0;        ///< Transport callback invocations attempted.
  Status lastError = Status::Ok();      ///< Most recent flush error, if any.
};

/**
 * @brief Snapshot of configuration and locally modeled runtime state without I2C.
 * @note The controller is write-only. Cached control booleans are trustworthy
 *       only while controlStateDirty is false; power is qualified separately.
 */
struct SettingsSnapshot {
  bool attached = false;    ///< Validated transport/framebuffer binding is present.
  bool initialized = false;
  DriverState state = DriverState::UNINIT;
  ControllerProfile controllerProfile = ControllerProfile::SSD1315;
  uint8_t i2cAddress = 0x3C;
  uint32_t i2cTimeoutMs = 25;
  uint16_t maxWriteBytes = 65;  ///< Total transport write capacity including control byte.
  uint8_t offlineThreshold = 3;
  bool hasNowMsHook = false;
  bool hasCooperativeYieldHook = false;

  uint8_t width = 128;
  uint8_t height = 64;
  uint8_t pageBufferPages = 8;
  uint8_t totalPages = 8;
  bool pageBufferMode = false;
  bool sleeping = true;     ///< Cached command model; not hardware readback.
  bool allPixelsOn = false; ///< Cached command model; check controlStateDirty.
  PanelPowerState panelPowerState = PanelPowerState::UNKNOWN; ///< Power certainty.
  uint8_t userPageCount = 1;
  uint8_t activeUserPage = 0;
  uint8_t currentPageIndex = 0;
  bool pageIterationActive = false;
  uint32_t byteBudgetPerTick = 128;
  uint32_t flushTimeoutMs = 1000;
  uint32_t displayOnDelayMs = 100;
  bool clearOnBegin = true;
  bool clearOnRecover = true;
  uint32_t inactivitySleepMs = 0;
  uint32_t pageCycleMs = 0;
  bool flipX = false;  ///< Cached configuration; check controlStateDirty.
  bool flipY = false;  ///< Cached configuration; check controlStateDirty.
  bool invert = false; ///< Cached configuration; check controlStateDirty.
  uint8_t contrast = 0x7F;
  uint8_t comPins = 0x12;
  uint8_t chargePumpVoltage = 0x14;
  uint8_t iref = 0x10;
  uint8_t vcomh = 0x20;
  uint8_t clockDivide = 1;
  uint8_t oscFrequency = 8;
  uint8_t prechargePhase1 = 2;
  uint8_t prechargePhase2 = 2;
  bool scrollActive = false; ///< Cached command model; check controlStateDirty.
  bool hasExternalBuffer = false;
  bool ownsBuffer = false;
  size_t bufferSize = 0;
  uint8_t dirtyPages = 0;
  bool gddramSynchronized = false; ///< All visible bytes had successful writes; no readback.
  bool flushing = false;
  bool controlStateDirty = false;
  Status controlStateError = Status::Ok();

  uint32_t lastOkMs = 0;
  uint32_t lastErrorMs = 0;
  uint8_t consecutiveFailures = 0;
  uint32_t totalFailures = 0;
  uint32_t totalSuccess = 0;
  Status lastError = Status::Ok();
};

}  // namespace SSD1315
