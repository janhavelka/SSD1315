/**
 * @file SSD1315.h
 * @brief Main SSD1315 OLED display driver class.
 *
 * Hardened I2C driver for SSD1315 OLED displays.
 * Supports partial updates, page-buffer mode, and deterministic tick-based flushing.
 *
 * ## Features
 * - Full buffer or page-buffer memory modes
 * - Dirty tracking with partial flush support
 * - Cooperative panel operations with caller-controlled transaction budgets
 * - Hardware scroll and display effects
 * - Test patterns for manufacturing
 *
 * ## Threading model
 * Single-threaded only. Call all methods from the same task (typically loop()).
 * Public APIs are not ISR-safe.
 *
 * ## Memory model
 * Allocation is limited to attach()/begin(). Zero heap allocations occur in
 * polling, drawing, control, and steady-state paths.
 *
 * ## Controller contract
 * This library targets SSD1315 I2C OLED controllers. `probe()` is ACK-only and
 * does not prove controller identity. Hardware reset, bus locking, pins, and
 * timeout policy are owned by the application or platform adapter.
 *
 * @see Config.h for configuration options
 * @see CommandTable.h for raw command access
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ssd1315/CommandTable.h"
#include "ssd1315/Config.h"
#include "ssd1315/Status.h"
#include "ssd1315/Version.h"

namespace SSD1315 {

/** @brief Kind of the single active cooperative panel operation. */
enum class OperationKind : uint8_t {
  NONE = 0,   ///< No cooperative operation.
  INITIALIZE, ///< Configure the controller and leave the panel off.
  FLUSH,      ///< Transfer dirty framebuffer ranges.
  SLEEP,      ///< Send DISPLAY_OFF.
  WAKE,       ///< Send DISPLAY_ON and observe the configured non-I2C delay.
  RESYNC,     ///< Initialize off, transfer a full frame, then display it.
  SHUTDOWN    ///< Display off and disable the configured internal charge pump.
};

/** @brief Observable phase within a cooperative panel operation. */
enum class OperationPhase : uint8_t {
  IDLE = 0,          ///< No active work.
  INITIALIZE_COMMAND,///< Sending one fixed SSD1315 initialization command.
  SET_COL_ADDR,      ///< Setting the next dirty column address range.
  SET_PAGE_ADDR,     ///< Setting the next dirty page address range.
  SEND_DATA,         ///< Sending one bounded framebuffer data transaction.
  DISPLAY_OFF,       ///< Sending DISPLAY_OFF.
  DISPLAY_ON,        ///< Sending DISPLAY_ON.
  DISPLAY_ON_DELAY,  ///< Waiting without I2C for the panel-on interval.
  CHARGE_PUMP_OFF,   ///< Disabling the internal charge pump during shutdown.
  COMPLETE           ///< Terminal snapshot phase.
};

/** @brief Lifecycle state of a cooperative operation. */
enum class OperationState : uint8_t {
  IDLE = 0,  ///< No active or terminal result is present.
  ACTIVE,    ///< More polling is required.
  SUCCEEDED, ///< Operation completed successfully.
  FAILED,    ///< Operation failed with a terminal error.
  CANCELLED, ///< Caller cancelled the operation without I2C.
  TIMED_OUT  ///< Caller-supplied deadline expired before another transaction.
};

/** @brief Certainty of the operation's physical panel effects. */
enum class EffectState : uint8_t {
  NONE = 0,     ///< No physical transfer was attempted.
  CONFIRMED,    ///< All requested effects were confirmed successful.
  PARTIAL,      ///< A confirmed prefix completed before cancellation/timeout/failure.
  INDETERMINATE ///< A failing physical attempt may have affected controller state.
};

/** @brief Verified/cached panel power state. */
enum class PanelPowerState : uint8_t {
  UNKNOWN = 0, ///< Hardware state cannot be trusted until resynchronization.
  OFF,         ///< DISPLAY_OFF is confirmed.
  STARTING,    ///< DISPLAY_ON succeeded; the non-I2C timing interval is active.
  ON           ///< DISPLAY_ON and its timing interval completed.
};

/** @brief Admission metadata for a cooperative operation. */
struct OperationOptions {
  uint32_t requestId = 0; ///< Nonzero caller identity; adjacent requests must differ.
  bool useDeadline = false; ///< true when deadlineMs is an absolute wrap-safe deadline.
  uint32_t deadlineMs = 0; ///< Absolute monotonic deadline no more than INT32_MAX ms ahead.
};

/** @brief Stable, allocation-free operation progress snapshot. */
struct OperationProgress {
  uint32_t requestId = 0; ///< Caller identity supplied at admission.
  OperationKind kind = OperationKind::NONE; ///< Admitted operation kind.
  OperationPhase phase = OperationPhase::IDLE; ///< Current fixed controller phase.
  OperationState state = OperationState::IDLE; ///< Active or terminal state.
  EffectState effect = EffectState::NONE; ///< Certainty of physical effects.
  PanelPowerState power = PanelPowerState::UNKNOWN; ///< Verified cached panel power.
  uint8_t currentPage = 0; ///< Current physical GDDRAM page, range [0..7].
  uint16_t currentColumn = 0; ///< Next data column, range [0..128].
  uint32_t bytesCompleted = 0; ///< Confirmed framebuffer payload bytes transferred.
  uint16_t dataChunkCount = 0; ///< Confirmed framebuffer data transactions.
  uint16_t transactionCount = 0; ///< Physical transport attempts, including failures.
  Status status = Status::Ok(); ///< Current terminal/error status or IN_PROGRESS.
};

/** @brief Consume-once terminal result for one cooperative operation. */
using OperationResult = OperationProgress;

/** @brief Convert an operation kind to a library-owned static string. @param kind Value. @return Static string. */
const char* toString(OperationKind kind);
/** @brief Convert an operation phase to a library-owned static string. @param phase Value. @return Static string. */
const char* toString(OperationPhase phase);
/** @brief Convert an operation state to a library-owned static string. @param state Value. @return Static string. */
const char* toString(OperationState state);
/** @brief Convert effect certainty to a library-owned static string. @param effect Value. @return Static string. */
const char* toString(EffectState effect);
/** @brief Convert panel power state to a library-owned static string. @param state Value. @return Static string. */
const char* toString(PanelPowerState state);

/**
 * @brief SSD1315 OLED display driver.
 *
 * Usage (full buffer mode):
 * @code
 * SSD1315::SSD1315 display;
 * SSD1315::Config cfg;
 * cfg.width = 128;
 * cfg.height = 64;
 * cfg.i2cAddress = 0x3C;
 * cfg.i2cWrite = myI2cWrite;
 * cfg.nowMs = myNowMs;
 * cfg.cooperativeYield = myYield;
 * cfg.pageBufferPages = 8;  // Full buffer
 *
 * void setup() {
 *   auto st = display.begin(cfg);
 *   if (!st.ok()) { handleError(st); }
 *   display.clear();
 *   display.drawText(0, 0, "Hello!");
 *   display.requestFlush();
 * }
 *
 * void loop() {
 *   display.tick(nowMs);
 * }
 * @endcode
 *
 * Usage (page buffer mode - non-blocking):
 * @code
 * SSD1315::Config cfg;
 * cfg.pageBufferPages = 1;  // Minimal RAM
 *
 * void setup() {
 *   display.begin(cfg);     // Initializes and leaves the panel confirmed OFF
 *   display.firstPage();  // Start iteration
 * }
 *
 * void loop() {
 *   display.tick(nowMs);
 *
 *   // Draw when not flushing and iteration active
 *   if (display.isPageIterating() && !display.isFlushing()) {
 *     int16_t yOff = display.pageBufferYOffset();
 *     // Draw content for this page (use yOff for Y coordinates)
 *     display.drawText(0, yOff, "Hello");
 *
 *     if (!display.nextPage()) {
 *       // Iteration complete. Admit/poll startWake() before presentation.
 *     }
 *   }
 * }
 * @endcode
 */
class SSD1315 {
 public:
  /// @brief Maximum supported display width
  static constexpr uint8_t MAX_WIDTH = 128;
  /// @brief Maximum supported display height
  static constexpr uint8_t MAX_HEIGHT = 64;
  /// @brief Maximum number of pages (MAX_HEIGHT / 8)
  static constexpr uint8_t MAX_PAGES = 8;

  /**
   * @brief Default constructor.
   */
  SSD1315();

  /**
   * @brief Destructor. Releases owned memory and performs no I2C.
   */
  ~SSD1315() noexcept;

  // Non-copyable
  SSD1315(const SSD1315&) = delete;
  SSD1315& operator=(const SSD1315&) = delete;
  SSD1315(SSD1315&&) = delete;
  SSD1315& operator=(SSD1315&&) = delete;

  // ========================================================================
  // Lifecycle
  // ========================================================================

  /**
   * @brief Validate and bind configuration without performing I2C.
   *
   * Candidate validation and allocation are completed before replacing an
   * existing binding. A rejected candidate leaves the prior binding intact.
   *
   * @param config Transport, profile, limits, timing, and framebuffer binding.
   * @return Ok on success or a configuration/allocation error.
   * @note May allocate one framebuffer when externalBuffer is null. No I2C,
   *       retry, delay, logging, or bus recovery occurs.
   */
  Status attach(const Config& config);

  /**
   * @brief Release the binding and owned memory without performing I2C.
   * @note Cancels/discards any local operation state. Call startShutdown() and
   *       consume its result first when physical panel shutdown is required.
   */
  void detach() noexcept;

  /** @brief Return true when a validated transport/buffer binding is present. */
  bool isAttached() const { return _attached; }

  /**
   * @brief Validate a candidate configuration without allocation or I2C.
   * @param config Candidate transport, geometry, capacity, and buffer contract.
   * @return Ok when attach() can accept the values, otherwise a stable error.
   */
  Status validateConfig(const Config& config) const;

  /**
   * @brief Start cooperative controller initialization, leaving the panel off.
   * @param options Nonzero request identity and optional absolute deadline.
   * @return Ok on admission; performs no I2C.
   */
  Status startInitialize(const OperationOptions& options);
  /**
   * @brief Start a cooperative dirty-framebuffer flush with confirmed power.
   * @param options Nonzero request identity and optional absolute deadline.
   * @return Ok on admission; performs no I2C.
   */
  Status startFlush(const OperationOptions& options);
  /**
   * @brief Start cooperative DISPLAY_OFF.
   * @param options Nonzero request identity and optional absolute deadline.
   * @return Ok on admission; performs no I2C.
   */
  Status startSleep(const OperationOptions& options);
  /**
   * @brief Start cooperative DISPLAY_ON plus its non-I2C timing interval.
   * @param options Nonzero request identity and optional absolute deadline.
   * @return Ok on admission; requires confirmed OFF state and performs no I2C.
   */
  Status startWake(const OperationOptions& options);
  /**
   * @brief Start full initialize-off, full-frame flush, and display-on resync.
   * @param options Nonzero request identity and optional absolute deadline.
   * @return Ok on admission; UNSUPPORTED in page-buffer mode. Performs no I2C.
   */
  Status startResync(const OperationOptions& options);
  /**
   * @brief Start cooperative display-off and charge-pump shutdown.
   * @param options Nonzero request identity and optional absolute deadline.
   * @return Ok on admission; performs no I2C.
   */
  Status startShutdown(const OperationOptions& options);

  /**
   * @brief Advance the single active cooperative operation.
   * @param nowMs Current monotonic milliseconds used for deadline/delay checks.
   * @param maxTransactions Maximum physical transport attempts this call; 0
   *        performs no I2C and only evaluates time/zero-I2C phases.
   * @param byteBudget Maximum framebuffer payload bytes this call. A zero value
   *        uses Config::byteBudgetPerTick when transactions are permitted.
   * @return IN_PROGRESS while active, the stable terminal status when the call
   *         completes/fails, or Ok when no operation is active.
   * @note At most eight transactions may be requested per call. Use one for a
   *       normal external bus-owner poll.
   */
  Status pollOperation(uint32_t nowMs, uint8_t maxTransactions,
                       uint16_t byteBudget = 0);

  /**
   * @brief Cancel the active operation without I2C.
   * @return Ok after publishing CANCELLED, STATE_ERROR when none is active.
   * @note Unconfirmed framebuffer data remains dirty for a later request.
   */
  Status cancelOperation();

  /** @brief Snapshot active or unconsumed terminal progress without I2C. */
  OperationProgress getOperationProgress() const { return _operation; }

  /**
   * @brief Retrieve one terminal operation result exactly once.
   * @param[out] out Stable terminal snapshot.
   * @return Ok on retrieval or RESULT_NOT_AVAILABLE before/after consumption.
   */
  Status takeOperationResult(OperationResult& out);

  /**
   * @brief Mark cached panel controls/power unknown after external reset/work.
   * @note Memory-only and zero-I2C. startResync() is the recovery path.
   */
  void invalidatePanelState();

  /** @brief Return the verified cached power state without I2C. */
  PanelPowerState panelPowerState() const { return _panelPowerState; }

  /**
   * @brief Initialize the display with given configuration.
   *
   * Blocking compatibility facade over attach() and the same cooperative
   * initialize/resync state machine used by external owners.
   *
   * @param config Configuration struct. i2cWrite must not be null.
   * @return Status Ok on success, error on failure.
   *
   * @note Bounded blocking lifecycle call. Each transport attempt is bounded
   *       by Config::i2cTimeoutMs. Use attach()/start...()/pollOperation() in a
   *       shared-bus owner task.
   * @note In full-buffer mode, clearOnBegin=true performs initialize-off, a
   *       full GDDRAM transfer, DISPLAY_ON, and its non-I2C interval. false runs
   *       the 17-command initialize sequence and returns with the panel off.
   *       Page-buffer mode always returns initialized-off; populate every page
   *       and explicitly wake before presenting the first frame.
   * @note No hidden probe is performed. A transport address NACK remains an
   *       initialization result and the validated binding is retained.
   */
  Status begin(const Config& config);

  /**
   * @brief Cooperative update function. Call every loop iteration.
   *
   * Advances at most one active operation or legacy flush transaction and the
   * memory-only power-on timing state. Returns immediately after bounded work.
   *
   * @param nowMs Current monotonic time in milliseconds.
   *
   * @note Non-blocking. Runs at most one flush instruction per call. Data
   *       payloads are bounded by Config::byteBudgetPerTick.
   * @note Handles 32-bit millisecond counter wraparound correctly.
   * @note Does nothing if not attached. It never admits sleep or page policy.
   */
  void tick(uint32_t nowMs);

  /**
   * @brief Stop the driver and release resources.
   *
   * Compatibility alias for detach(). Safe to call repeatedly.
   * @note Performs no I2C. Use startShutdown() before end() when physical panel
   *       shutdown is required.
   */
  void end();

  /**
   * @brief Check if driver is initialized.
   * @return true after a successful initialization until detach()/end().
   */
  bool isInitialized() const { return _initialized; }

  /**
   * @brief Get current configuration.
   * @return Reference to the bound configuration or defaults when detached.
   * @note Valid after attach(), including after failed initialization.
   */
  const Config& getConfig() const { return _config; }

  /**
   * @brief Get a snapshot of configuration and runtime state.
   * @param[out] out Snapshot populated without performing I2C.
   * @return Status::Ok() on success.
   */
  Status getSettings(SettingsSnapshot& out) const;

  /**
   * @brief Get a snapshot of configuration and runtime state.
   * @return Snapshot populated without performing I2C.
   */
  SettingsSnapshot getSettings() const {
    SettingsSnapshot out;
    (void)getSettings(out);
    return out;
  }

  // ========================================================================
  // Health tracking and diagnostics
  // ========================================================================

  /**
   * @brief Check if device is present at configured I2C address.
   *
   * Sends a minimal I2C transaction to verify device responds with ACK.
   *
   * IMPORTANT LIMITATIONS:
   * - Does NOT initialize the device or change driver state
   * - Does NOT update health tracking (probe is diagnostic-only)
   * - Does NOT verify chip identity (SSD1315 has no WHOAMI register)
   * - ACK only confirms "something responds at this address"
   * - Modules that do not connect SDAOUT/ACK make NACK policy board-specific
   *
   * Requires attach() so the transport callback is configured.
   * Useful for:
   * - Checking if the configured device is present without affecting health state
   * - Diagnosing bus connectivity after initialization
   *
   * @return Status Ok if device ACK'd, DEVICE_NOT_FOUND for definite address
   *         NACK on ACK-capable wiring, otherwise the original transport error.
   *
   * @pre attach() must have succeeded so the transport callback is configured.
   *
   * @note SSD1315 has no WHOAMI register. Probe sends a NOP command (0xE3)
   *       and checks for ACK. Does NOT call _updateHealth().
   */
  Status probe();

  /**
   * @brief Run a bounded blocking full resynchronization compatibility facade.
   *
   * @return Status Ok on success, error on failure.
   *
   * @note recover() does not own or toggle RES#. Hardware reset sequencing is
   *       an application/platform responsibility.
   * @note Uses startResync()/pollOperation() and retains the binding on failure.
   *       It performs no hidden ACK probe, retry, recovery, or bus reset.
   * @note Prefer startResync() in an external owner task.
   */
  Status recover();

  /**
   * @brief Get current driver state (health indicator).
   * @return Current state (UNINIT/READY/DEGRADED/OFFLINE).
   */
  DriverState state() const { return _driverState; }

  /**
   * @brief Alias for state() for cross-driver diagnostics.
   * @return Current state (UNINIT/READY/DEGRADED/OFFLINE).
   */
  DriverState driverState() const { return state(); }

  /**
   * @brief Check if device is operational.
   * @return true if READY or DEGRADED (device may respond).
   */
  bool isOnline() const {
    return _driverState == DriverState::READY ||
           _driverState == DriverState::DEGRADED;
  }

  /**
   * @brief Get timestamp of last successful I2C operation.
   * @return Monotonic millisecond value at last success, or 0 if none.
   */
  uint32_t lastOkMs() const { return _lastOkMs; }

  /**
   * @brief Get timestamp of last failed I2C operation.
   * @return Monotonic millisecond value at last error, or 0 if none.
   */
  uint32_t lastErrorMs() const { return _lastErrorMs; }

  /**
   * @brief Get most recent error status.
   * @return Last error, or Ok() if none.
   */
  Status lastError() const { return _lastError; }

  /**
   * @brief Check whether panel control state may differ from cached settings.
   *
   * Set after a failed panel-control I2C operation such as scroll setup,
   * orientation/display mode changes, raw commands, or a failed initialization
   * or resynchronization. Cleared by a successful initialize/resync operation.
   *
   * @return true when the application should startResync() (or use recover())
   *         before trusting cached panel controls.
   */
  bool controlStateDirty() const { return _controlStateDirty; }

  /**
   * @brief Get the status that first/most recently marked control state dirty.
   * @return Static Status captured from the failed control-state operation.
   */
  Status controlStateError() const { return _controlStateError; }

  /**
   * @brief Get consecutive failure count.
   * @return Number of failures since last success.
   */
  uint8_t consecutiveFailures() const { return _consecutiveFailures; }

  /**
   * @brief Get lifetime failure count.
   * @return Total failures since begin().
   */
  uint32_t totalFailures() const { return _totalFailures; }

  /**
   * @brief Get lifetime success count.
   * @return Total successes since begin().
   */
  uint32_t totalSuccess() const { return _totalSuccess; }

  // ========================================================================
  // Raw command access
  // ========================================================================

  /**
   * @brief Send a single command byte to the display.
   *
   * @param cmd Command byte (see CommandTable.h)
   * @return Status Ok on success, I2C error on failure.
   *
   * @pre Controller initialization must have completed successfully through
   *      startInitialize(), startResync(), or begin().
   * @note Raw passthrough does not validate arbitrary command bit patterns.
   *       Use documented SSD1315 command encodings only.
   * @note Success invalidates cached panel control/power state; startResync()
   *       is required before a normal owner-safe flush or wake.
   * @note Performs one blocking attempt bounded by Config::i2cTimeoutMs.
   */
  Status sendCommand(uint8_t cmd);

  /**
   * @brief Send a command with one argument byte.
   *
   * @param cmd Command byte
   * @param arg Argument byte
   * @return Status Ok on success, I2C error on failure.
   *
   * @pre Controller initialization must have completed successfully through
   *      startInitialize(), startResync(), or begin().
   * @note Raw passthrough does not validate arbitrary command/argument
   *       patterns. Use documented SSD1315 encodings only.
   * @note Success invalidates cached panel control/power state.
   */
  Status sendCommand2(uint8_t cmd, uint8_t arg);

  /**
   * @brief Send a command with two argument bytes.
   *
   * @param cmd Command byte
   * @param arg1 First argument byte
   * @param arg2 Second argument byte
   * @return Status Ok on success, I2C error on failure.
   *
   * @pre Controller initialization must have completed successfully through
   *      startInitialize(), startResync(), or begin().
   * @note Raw passthrough does not validate arbitrary command/argument
   *       patterns. Use documented SSD1315 encodings only.
   * @note Success invalidates cached panel control/power state.
   */
  Status sendCommand3(uint8_t cmd, uint8_t arg1, uint8_t arg2);

  /**
   * @brief Send a list of command bytes.
   *
   * @param cmds Pointer to command bytes
   * @param len Number of bytes
   * @return Status Ok on success, I2C error on failure.
   *
   * @pre Controller initialization must have completed successfully through
   *      startInitialize(), startResync(), or begin().
   * @note A zero-length list is a no-op. A null pointer with len > 0 returns
   *       INVALID_CONFIG before any I2C transaction.
   * @note This blocking convenience API accepts at most 32 command bytes per
   *       call. Longer setup sequences should be represented as explicit
   *       bounded operations rather than one unbounded raw passthrough.
   *       Depending on maxWriteBytes it performs at most 11 physical attempts.
   * @note Raw passthrough does not validate arbitrary command sequences.
   *       Use documented SSD1315 command encodings only.
   * @note A success invalidates cached panel control/power state. Any failure
   *       after a confirmed prefix marks it uncertain.
   */
  Status sendCommandList(const uint8_t* cmds, size_t len);

  // ========================================================================
  // Display control
  // ========================================================================

  /**
   * @brief Set display contrast (brightness).
   *
   * @param contrast Value 1-255. Higher = brighter.
   * @return Status Ok on success, INVALID_CONFIG for 0.
   *
   * @note OLED pixels are self-emitting (no backlight). This controls
   *       the pixel drive current, effectively controlling brightness.
   */
  Status setContrast(uint8_t contrast);

  /**
   * @brief Alias for setContrast - set display brightness.
   *
   * @param brightness Value 1-255. Higher = brighter.
   * @return Status Ok on success, INVALID_CONFIG for 0.
   */
  Status setBrightness(uint8_t brightness) { return setContrast(brightness); }

  /**
   * @brief Invert display colors.
   *
   * @param invert true = inverted (0 pixels lit), false = normal.
   * @return Status Ok on success.
   */
  Status setInvert(bool invert);

  /**
   * @brief Flip display horizontally (segment remap).
   *
   * @param flip true = flip, false = normal.
   * @return Status Ok on success.
   *
   * @note Takes effect immediately. May need flush to see change.
   */
  Status setFlipX(bool flip);

  /**
   * @brief Flip display vertically (COM scan direction).
   *
   * @param flip true = flip, false = normal.
   * @return Status Ok on success.
   */
  Status setFlipY(bool flip);

  /**
   * @brief Set display sleep mode.
   *
   * @param sleep true = display OFF (low power), false = display ON.
   * @return Status Ok on success.
   *
   * @note When waking from sleep, power-on timing guard is applied.
   * @note This direct one-attempt compatibility API has no request/result ID.
   *       External owner tasks should use startSleep()/startWake().
   */
  Status setSleep(bool sleep);

  /**
   * @brief Check if display is currently sleeping.
   * @return true if display is off / sleeping.
   */
  bool isSleeping() const { return _sleeping; }

  /**
   * @brief Set entire display ON (ignore RAM) or resume normal mode.
   *
   * @param allOn true = all pixels on, false = show RAM content.
   * @return Status Ok on success.
   *
   * @note Useful for quick display test without modifying buffer.
   */
  Status setAllPixelsOn(bool allOn);

  // ========================================================================
  // Auto-sleep and activity
  // ========================================================================

  /**
   * @brief Configure auto-sleep timeout.
   *
   * Retained only for source compatibility. The passive core stores the value
   * for diagnostics but does not admit power operations from tick().
   *
   * @param inactivityMs Timeout in milliseconds. 0 = disabled.
   */
  void setAutoSleep(uint32_t inactivityMs);

  /**
   * @brief Record an activity timestamp without changing panel power.
   *
   * Retained as a memory-only compatibility hook. It performs no wake or I2C.
   */
  void touch();

  // ========================================================================
  // Page cycling (multi-screen UI)
  // ========================================================================

  /**
   * @brief Set number of user-defined UI pages for cycling.
   *
   * @param count Number of pages (1-255). Stored only; application owns policy.
   */
  void setUserPageCount(uint8_t count);

  /**
   * @brief Get number of user-defined UI pages.
   * @return Current page count.
   */
  uint8_t getUserPageCount() const { return _userPageCount; }

  /**
   * @brief Set active user page index.
   *
   * @param index Page index (0 to pageCount-1).
   * @note Clipped to valid range if out of bounds.
   */
  void setActiveUserPage(uint8_t index);

  /**
   * @brief Get active user page index.
   * @return Current page index.
   */
  uint8_t getActiveUserPage() const { return _activeUserPage; }

  /**
   * @brief Configure automatic page cycling interval.
   *
   * @param intervalMs Compatibility value only. tick() does not cycle pages.
   */
  void setPageCycleInterval(uint32_t intervalMs);

  // ========================================================================
  // Drawing primitives
  // ========================================================================

  /**
   * @brief Clear entire framebuffer (set all pixels off).
   *
   * @note Marks all pages as dirty. Call requestFlush() to send to display.
   * @note If called while a flush is active, affected pages stay dirty for a
   *       later retry after the current flush completes or fails.
   * @note In page buffer mode, this clears only the current buffer window.
   */
  void clear();

  /**
   * @brief Fill entire framebuffer (set all pixels on).
   *
   * @note Marks all pages as dirty.
   * @note If called while a flush is active, affected pages stay dirty for a
   *       later retry after the current flush completes or fails.
   * @note In page buffer mode, this fills only the current buffer window.
   */
  void fill();

  /**
   * @brief Set a single pixel.
   *
   * @param x X coordinate (0 to width-1)
   * @param y Y coordinate (0 to height-1)
   * @param on true = pixel on, false = pixel off
   *
   * @note Out-of-bounds coordinates are silently ignored.
   * @note Marks containing page as dirty.
   */
  void setPixel(int16_t x, int16_t y, bool on = true);

  /**
   * @brief Get a pixel value.
   *
   * @param x X coordinate
   * @param y Y coordinate
   * @return true if pixel is on, false if off or out of bounds.
   *
   * @note Only works in full buffer mode. Returns false in page buffer mode
   *       for pages not currently in buffer.
   */
  bool getPixel(int16_t x, int16_t y) const;

  /**
   * @brief Draw a horizontal line.
   *
   * @param x Start X coordinate
   * @param y Y coordinate
   * @param w Width in pixels
   * @param on Pixel state
   */
  void drawHLine(int16_t x, int16_t y, int16_t w, bool on = true);

  /**
   * @brief Draw a vertical line.
   *
   * @param x X coordinate
   * @param y Start Y coordinate
   * @param h Height in pixels
   * @param on Pixel state
   */
  void drawVLine(int16_t x, int16_t y, int16_t h, bool on = true);

  /**
   * @brief Draw a rectangle outline.
   *
   * @param x Top-left X
   * @param y Top-left Y
   * @param w Width
   * @param h Height
   * @param on Pixel state
   */
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, bool on = true);

  /**
   * @brief Draw a filled rectangle.
   *
   * @param x Top-left X
   * @param y Top-left Y
   * @param w Width
   * @param h Height
   * @param on Pixel state
   */
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, bool on = true);

  /**
   * @brief Draw a line between two points.
   *
   * @param x0 Start X
   * @param y0 Start Y
   * @param x1 End X
   * @param y1 End Y
   * @param on Pixel state
   */
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, bool on = true);

  /**
   * @brief Draw a circle outline.
   *
   * @param cx Center X
   * @param cy Center Y
   * @param r Radius
   * @param on Pixel state
   */
  void drawCircle(int16_t cx, int16_t cy, int16_t r, bool on = true);

  /**
   * @brief Draw a filled circle.
   *
   * @param cx Center X
   * @param cy Center Y
   * @param r Radius
   * @param on Pixel state
   */
  void fillCircle(int16_t cx, int16_t cy, int16_t r, bool on = true);

  /**
   * @brief Draw a bitmap from PROGMEM or RAM using a checked source size.
   *
   * @param x Top-left X
   * @param y Top-left Y
   * @param bitmap Pointer to bitmap data (1 bit per pixel, MSB first)
   * @param w Bitmap width
   * @param h Bitmap height
   * @param bitmapSizeBytes Source bitmap size in bytes
   * @param on Pixel state for '1' bits
   * @return Ok when drawn or clipped/no-op; BUFFER_TOO_SMALL if the source
   *         buffer is smaller than ((w + 7) / 8) * h.
   *
   * @note Bitmap format: each byte is 8 horizontal pixels, MSB = leftmost.
   *       Rows are packed (no padding).
   * @note Nonpositive width/height are treated as no-op success.
   */
  Status drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap,
                    int16_t w, int16_t h, size_t bitmapSizeBytes,
                    bool on = true);

  /**
   * @brief Draw a bitmap from PROGMEM or RAM using the legacy unchecked API.
   *
   * @param x Top-left X
   * @param y Top-left Y
   * @param bitmap Pointer to bitmap data (1 bit per pixel, MSB first)
   * @param w Bitmap width
   * @param h Bitmap height
   * @param on Pixel state for '1' bits
   *
   * @note Caller must ensure bitmap points to at least
   *       ((w + 7) / 8) * h bytes. Prefer the Status-returning overload when
   *       the source length is known.
   */
  void drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap,
                  int16_t w, int16_t h, bool on = true);

  // ========================================================================
  // Text rendering (built-in 5x7 font)
  // ========================================================================

  /**
   * @brief Draw a single ASCII character.
   *
   * Uses built-in 5x7 fixed-width font. Characters outside printable ASCII
   * range (32-126) are rendered as a box.
   *
   * @param x Top-left X of character cell
   * @param y Top-left Y of character cell
   * @param c Character to draw
   * @param on Pixel state
   *
   * @note Character cell is 6x8 pixels (5x7 glyph + 1px spacing).
   */
  void drawChar(int16_t x, int16_t y, char c, bool on = true);

  /**
   * @brief Draw a null-terminated string.
   *
   * @param x Starting X position
   * @param y Starting Y position
   * @param str Null-terminated string
   * @param on Pixel state
   *
   * @return X position after last character (for continuation).
   *
   * @note Processes at most 512 input characters per call to keep malformed
   *       unterminated strings from becoming an unbounded steady-path scan.
   */
  int16_t drawText(int16_t x, int16_t y, const char* str, bool on = true);

  /**
   * @brief Draw bounded bytes without scanning for a terminator.
   * @param x Left cursor position in pixels.
   * @param y Top cursor position in pixels.
   * @param data Character bytes; may be non-null-terminated.
   * @param length Number of bytes available at @p data.
   * @param on true to set glyph pixels, false to clear them.
   * @return Cursor X after the bounded text, unchanged for null/zero input.
   * @note Memory-only; reads at most min(length, 512) bytes and performs no I2C.
   */
  int16_t drawTextN(int16_t x, int16_t y, const char* data, size_t length,
                    bool on = true);

  /**
   * @brief Get width of a string in pixels.
   *
   * @param str Null-terminated string
   * @return Width in pixels (6 pixels per character).
   *
   * @note Processes at most 512 input characters.
   */
  static int16_t getTextWidth(const char* str);

  /**
   * @brief Calculate width for exactly @p length character bytes.
   * @param data Character bytes; may be non-null-terminated.
   * @param length Number of bytes available at @p data.
   * @return Saturated pixel width; zero for null/zero input.
   * @note Pure helper; examines at most min(length, 512) bytes and performs no
   *       I2C or allocation.
   */
  static int16_t getTextWidthN(const char* data, size_t length);

  // ========================================================================
  // Test patterns (manufacturing/debug)
  // ========================================================================

  /**
   * @brief Fill buffer with checkerboard pattern.
   *
   * Useful for display testing and alignment verification.
   *
   * @param size Checker size in pixels (1 = every pixel, 8 = 8x8 squares)
   */
  void fillCheckerboard(uint8_t size = 1);

  /**
   * @brief Fill buffer with vertical stripes.
   *
   * @param width Stripe width in pixels
   */
  void fillVerticalStripes(uint8_t width = 1);

  /**
   * @brief Fill buffer with horizontal stripes.
   *
   * @param height Stripe height in pixels
   */
  void fillHorizontalStripes(uint8_t height = 1);

  // ========================================================================
  // Page buffer mode (u8g2-style iteration)
  // ========================================================================

  /**
   * @brief Check if driver is in page buffer mode.
   * @return true if pageBufferPages < totalPages.
   */
  bool isPageBufferMode() const;

  /**
   * @brief Begin page buffer iteration.
   *
   * Call before drawing in page buffer mode. Clears buffer and sets
   * current page to 0.
   *
   * @note Only meaningful in page buffer mode. In full buffer mode,
   *       this just clears the buffer.
   */
  void firstPage();

  /**
   * @brief Advance to next page in iteration (non-blocking).
   *
   * Marks current buffer as dirty, requests flush, and prepares for next page.
   * Does NOT block; actual flush happens in tick().
   *
   * @return true if more pages remain (call tick() then nextPage() again),
   *         false if iteration complete or error occurred.
   *
   * @note Call pattern for page buffer mode:
   * @code
   * void loop() {
   *   display.tick(nowMs);
   *   if (display.isPageIterating() && !display.isFlushing()) {
   *     // Draw for current page
   *     drawContent(display.currentPageIndex());
   *     if (!display.nextPage()) {
   *       // Iteration complete (or error; check lastError())
   *     }
   *   }
   * }
   * @endcode
   *
   * @note If called while flush is in progress, returns true without advancing.
   *       This is safe; just call tick() and try again.
   * @note If a flush error occurred, returns false and sets lastError().
   * @note In full buffer mode, always returns false (single iteration).
   */
  bool nextPage();

  /**
   * @brief Check if page buffer iteration is in progress.
   * @return true if between firstPage() and iteration completion.
   */
  bool isPageIterating() const { return _inPageIteration; }

  /**
   * @brief Get current page index in page buffer iteration.
   * @return Page index (0 to totalPages-1).
   */
  uint8_t currentPageIndex() const { return _currentBufferPage; }

  /**
   * @brief Get total number of display pages (height / 8).
   * @return Number of pages.
   */
  uint8_t totalPages() const { return _totalPages; }

  /**
   * @brief Get Y offset for current page buffer.
   *
   * In page buffer mode, drawing Y coordinates should be offset by this value.
   *
   * @return Y offset in pixels (currentPageIndex * 8 * pageBufferPages).
   */
  int16_t pageBufferYOffset() const;

  // ========================================================================
  // Flush control
  // ========================================================================

  /**
   * @brief Request flush of all dirty pages.
   *
   * Starts async flush operation. Actual data transfer happens in tick().
   *
   * @return Status Ok if flush started, BUSY if already flushing.
   *
   * @note If a flush fails, dirty flags for unsent or partially sent pages are
   *       preserved so a later requestFlush() can retry.
   * @note If framebuffer content is changed while a flush is active, affected
   *       pages remain dirty and require a later requestFlush() retry. This
   *       prevents older bytes already sent to GDDRAM from being treated as
   *       synchronized with newer framebuffer content.
   * @note While hardware scroll is active, returns STATE_ERROR and preserves
   *       dirty framebuffer state. Stop scroll before writing GDDRAM.
   */
  Status requestFlush();

  /**
   * @brief Request flush of a specific rectangular region.
   *
   * @param x Left edge (column)
   * @param y Top edge (row)
   * @param w Width in pixels
   * @param h Height in pixels
   * @return Status Ok if flush started, BUSY if already flushing.
   *
   * @note Coordinates are clipped to display bounds.
   * @note Region is expanded to page boundaries vertically.
   * @note If a flush fails, dirty flags for affected pages are preserved
   *       conservatively for retry.
   * @note While hardware scroll is active, this marks the clipped region dirty
   *       and returns STATE_ERROR through requestFlush(); stop scroll before
   *       writing GDDRAM.
   */
  Status requestFlushRect(int16_t x, int16_t y, int16_t w, int16_t h);

  /**
   * @brief Check if flush operation is in progress.
   * @return true if flushing.
   */
  bool isFlushing() const;

  /**
   * @brief Progress an active flush by explicit instruction and byte budgets.
   *
   * One SSD1315 command/control-byte transaction or one data/control-byte
   * transaction counts as one instruction. Data instructions also consume
   * @p byteBudget payload bytes, excluding the 0x40 control byte.
   *
   * @param nowMs Current monotonic time in milliseconds.
   * @param maxInstructions Maximum command/data transactions to issue. 0
   *        performs no I2C and returns IN_PROGRESS while a flush is active.
   * @param byteBudget Maximum data payload bytes to send; must be > 0 when
   *        maxInstructions is nonzero.
   * @return Ok when no active work remains, IN_PROGRESS when more polls are
   *         needed, or an error status if the flush failed.
   *
   * @note Steady-path API for owners that need visible poll budgets. The
   *       regular tick() path calls this with one instruction and
   *       Config::byteBudgetPerTick.
   */
  Status pollFlush(uint32_t nowMs, uint8_t maxInstructions, uint16_t byteBudget);

  /**
   * @brief Snapshot current flush progress without performing I2C.
   * @return Flush status, dirty mask, active page/window, and last error.
   */
  FlushStatus getFlushStatus() const;

  /**
   * @brief Clear last error status.
   *
   * @note Does not clear flush error state, dirty retry state, or
   *       controlStateDirty(); a successful initialize/resync operation clears
   *       controlStateDirty().
   */
  void clearLastError() { _lastError = Ok(); }

  /**
   * @brief Compatibility alias for clearLastError().
   */
  void clearError() { clearLastError(); }

  /**
   * @brief Block until current flush completes.
   *
   * Calls tick() internally until flush finishes or times out.
   * Does NOT call delay(); it uses the configured cooperativeYield hook or the
   * active platform yield between polls and has a finite guard for stalled
   * injected clocks.
   *
   * @param nowMs Current time in milliseconds
   * @param timeoutMs Maximum time to wait (0 = use flushTimeoutMs from config)
   * @return Status Ok if flush completed, TIMEOUT or I2C error on failure.
   *
   * @warning Blocking CLI/test convenience helper. Do not use from unattended
   *          steady firmware paths; prefer requestFlush() with tick() or
   *          pollFlush().
   */
  Status waitFlush(uint32_t nowMs, uint32_t timeoutMs = 0);

  // ========================================================================
  // Hardware scrolling
  // ========================================================================

  /**
   * @brief Configure and start horizontal scrolling.
   *
   * @param left true = scroll left, false = scroll right
   * @param startPage First page to scroll (0-7)
   * @param endPage Last page to scroll (0-7, >= startPage)
   * @param speed Scroll speed (frames per step)
   * @return Status Ok on success.
   *
   * @note Deactivates any prior scroll before setup. If a prior scroll was
   *       active, the framebuffer is marked dirty because controller RAM must
   *       be rewritten after scroll is stopped.
   * @note While scrolling is active, framebuffer flush is blocked to avoid RAM
   *       writes during SSD1315 scroll mode.
   * @note Hardware scroll is currently supported only for 128-column panels.
   *       Non-128-wide configs return UNSUPPORTED for scroll setup.
   */
  Status startHorizontalScroll(bool left, uint8_t startPage, uint8_t endPage,
                               ScrollSpeed speed = ScrollSpeed::FRAMES_5);

  /**
   * @brief Configure and start vertical + horizontal scrolling.
   *
   * @param left true = scroll left, false = scroll right
   * @param startPage First page to scroll
   * @param endPage Last page to scroll
   * @param speed Scroll speed
   * @param verticalOffset Vertical scroll offset per step (0-63)
   * @return Status Ok on success.
   *
   * @note Uses the SSD1315 vertical+horizontal setup form with full-width
   *       column range 0..127 and one-column horizontal offset.
   * @note verticalOffset must be less than the currently configured vertical
   *       scroll area row count. Default area is the full display height.
   * @note Hardware scroll is currently supported only for 128-column panels.
   *       Non-128-wide configs return UNSUPPORTED for scroll setup.
   * @note While scrolling is active, framebuffer flush is blocked to avoid RAM
   *       writes during SSD1315 scroll mode.
   */
  Status startVerticalScroll(bool left, uint8_t startPage, uint8_t endPage,
                             ScrollSpeed speed, uint8_t verticalOffset);

  /**
   * @brief Stop scrolling.
   *
   * @return Status Ok on success.
   *
   * @note On success after an active scroll, all framebuffer pages are marked
   *       dirty. Redraw or request a flush to restore controller RAM.
   */
  Status stopScroll();

  /**
   * @brief Set vertical scroll area.
   *
   * @param topFixedRows Number of rows fixed at top (not scrolled)
   * @param scrollRows Number of rows in scroll area
   * @return Status Ok on success.
   *
   * @note startVerticalScroll() validates its verticalOffset against this
   *       cached area. Call recover()/begin() to reset the area to full height.
   */
  Status setVerticalScrollArea(uint8_t topFixedRows, uint8_t scrollRows);

  // ========================================================================
  // Advanced display features
  // ========================================================================

  /**
   * @brief Set fade out or blink mode.
   *
   * @param mode Fade mode (OFF, FADE_OUT, or BLINK)
   * @param interval Fade step interval (0-15, see datasheet)
   * @return Status Ok on success.
   */
  Status setFadeMode(FadeMode mode, uint8_t interval = 0);

  /**
   * @brief Enable or disable zoom mode.
   *
   * @param enable true = 2x vertical zoom, false = normal.
   * @return Status Ok on success; INVALID_CONFIG when enabling zoom with
   *         sequential COM pin mode.
   * @note Panel must be in alternative COM pin mode for zoom to work.
   */
  Status setZoom(bool enable);

  // ========================================================================
  // Buffer access (advanced)
  // ========================================================================

  /**
   * @brief Get pointer to framebuffer.
   *
   * @return Pointer to buffer, or nullptr if not initialized.
   *
   * @note Buffer format: columns x pages bytes. Each byte is 8 vertical pixels
   *       with LSB at top. Buffer[x + page*width] contains column x, page p.
   * @note Modifying buffer directly does NOT update dirty tracking.
   *       Call markDirty() after direct modifications.
   */
  uint8_t* getBuffer() { return _buffer; }
  const uint8_t* getBuffer() const { return _buffer; }

  /**
   * @brief Get buffer size in bytes.
   * @return width x pageBufferPages, or 0 if not initialized.
   */
  size_t getBufferSize() const;

  /**
   * @brief Mark a page as dirty (needs flush).
   *
   * @param page Page index (0 to totalPages-1)
   * @param minCol Minimum dirty column (default 0)
   * @param maxCol Maximum dirty column (default width-1)
   *
   * @note Call after direct buffer modifications.
   * @note In page buffer mode, pages outside the current buffer window are ignored.
   */
  void markDirty(uint8_t page, uint8_t minCol = 0, uint8_t maxCol = 255);

  /**
   * @brief Mark all pages as dirty.
   * @note In page buffer mode, this marks only the current buffer window.
   */
  void markAllDirty();

  /**
   * @brief Mark a clipped rectangle dirty without starting a flush.
   * @param x Left column in pixels.
   * @param y Top row in pixels.
   * @param w Width in pixels.
   * @param h Height in pixels.
   * @note Memory-only and safe for hostile signed coordinates.
   */
  void markDirtyRect(int16_t x, int16_t y, int16_t w, int16_t h);

  /**
   * @brief Force-clear all dirty flags.
   *
   * @note This is a direct-buffer ownership escape hatch. It can discard
   *       pending retry state after failed or partial flushes. Prefer
   *       clearDirtyIfIdle() for normal application code.
   */
  void clearDirty();

  /**
   * @brief Clear dirty flags only when no flush/retry state would be lost.
   *
   * @return OK if dirty tracking was cleared, BUSY while a flush is active, or
   *         STATE_ERROR if a failed flush left dirty retry state that must be
   *         retried or force-cleared explicitly.
   */
  Status clearDirtyIfIdle();

  /**
   * @brief Check if any pages are dirty.
   * @return true if any page needs flush.
   */
  bool isDirty() const;

 private:
  // ========== Internal types ==========

  enum class FlushState : uint8_t {
    IDLE,           ///< No flush in progress
    SET_COL_ADDR,   ///< Setting column address for the current page/window
    SET_PAGE_ADDR,  ///< Setting page address for the current page/window
    SEND_DATA,      ///< Sending framebuffer data
    DONE,           ///< Flush completed successfully
    ERROR           ///< Flush failed
  };

  enum class PowerState : uint8_t {
    OFF,            ///< Display off, not initialized
    INIT_DELAY,     ///< Waiting for power-on timing
    READY           ///< Display ready for operation
  };

  // ========== Internal methods ==========

  Status _sendCommand(uint8_t command);
  Status _sendCommand2(uint8_t command, uint8_t arg);
  Status _sendCommand3(uint8_t command, uint8_t arg1, uint8_t arg2);
  Status _sendCommandList(const uint8_t* commands, size_t length);
  void tickFlush(uint32_t nowMs);
  void tickPowerOn(uint32_t nowMs);
  void resetActivityTimer(uint32_t nowMs);

  // Buffer helpers
  size_t bufferIndex(int16_t x, int16_t y) const;
  uint8_t bufferBit(int16_t y) const;
  bool isInBuffer(int16_t x, int16_t y) const;

  // Health tracking helpers
  uint32_t _nowMs() const;
  void _cooperativeYield() const;
  Status _updateHealth(const Status& st);
  Status _checkDirectI2cAdmission() const;
  Status _i2cWriteRaw(const uint8_t* data, size_t len);
  Status _i2cWriteTracked(const uint8_t* data, size_t len);
  Status _probeRaw();
  void _markControlStateDirty(const Status& st);
  void _clearControlStateDirty();
  void _resetRuntimeState();
  Status _validateConfig(const Config& config, size_t& requiredBufferSize) const;
  Status _startOperation(OperationKind kind, const OperationOptions& options);
  Status _sendInitStep(uint8_t step);
  Status _pollInitializePhase();
  Status _pollFlushPhase(uint32_t nowMs, uint8_t maxTransactions,
                         uint16_t byteBudget);
  Status _pollFlushInternal(uint32_t nowMs, uint8_t maxInstructions,
                            uint16_t byteBudget);
  Status _requestFlushInternal();
  void _finishOperation();
  Status _failOperation(const Status& status, EffectState effect,
                        OperationState state = OperationState::FAILED);
  Status _runBlockingOperation(OperationKind kind);
  void _syncOperationFromFlush();
  bool _operationActive() const {
    return _operation.state == OperationState::ACTIVE;
  }
  static bool _deadlineReached(uint32_t nowMs, uint32_t deadlineMs);
  static EffectState _effectForFailure(const Status& status,
                                       bool hasConfirmedPrefix);
  static Status _mapTransportResult(const TransportResult& result);

  // ========== State ==========

  Config _config{};
  bool _attached = false;
  bool _initialized = false;
  bool _sleeping = true;
  bool _allPixelsOn = false;
  bool _scrollActive = false;

  // Buffer
  uint8_t* _buffer = nullptr;
  bool _ownsBuffer = false;
  uint8_t _totalPages = 0;

  // Dirty tracking
  uint8_t _dirtyPages = 0;  // Bitmask of dirty pages
  uint8_t _dirtyMinCol[MAX_PAGES] = {};
  uint8_t _dirtyMaxCol[MAX_PAGES] = {};
  uint32_t _dirtyGeneration[MAX_PAGES] = {};

  // Flush state machine
  FlushState _flushState = FlushState::IDLE;
  uint8_t  _flushPage = 0;
  uint16_t _flushCol = 0;       // One-past-end can reach 128; needs uint16_t
  uint8_t  _flushEndPage = 0;
  uint8_t  _flushMinCol = 0;    // Hardware column address: 0-127, fits uint8_t
  uint8_t  _flushMaxCol = 0;    // Hardware column address: 0-127, fits uint8_t
  uint32_t _flushPageGeneration = 0;
  uint32_t _flushStartMs = 0;
  bool     _flushStarted = false;  // True once _flushStartMs is valid
  bool     _flushAccounted = false; // True once terminal state updated health
  Status _lastError{};
  uint32_t _flushBytesCompleted = 0;
  uint16_t _flushDataChunkCount = 0;
  uint16_t _flushTransactionCount = 0;

  // Single cooperative operation and consume-once result state.
  OperationOptions _operationOptions{};
  OperationProgress _operation{};
  bool _operationResultReady = false;
  uint32_t _lastOperationRequestId = 0;
  uint8_t _initStep = 0;
  uint32_t _operationDelayStartMs = 0;
  bool _operationDelayStarted = false;
  uint32_t _compatRequestId = 0x80000000u;
  PanelPowerState _panelPowerState = PanelPowerState::UNKNOWN;
  uint32_t _transportTimeoutMs = 25;

  // Power-on timing
  PowerState _powerState = PowerState::OFF;
  uint32_t _powerOnMs = 0;
  bool _powerOnDelayStarted = false;

  // Hardware scroll area cached for validating vertical scroll setup.
  uint8_t _verticalScrollTopRows = 0;
  uint8_t _verticalScrollRows = 64;

  // Activity tracking
  uint32_t _lastActivityMs = 0;
  uint32_t _lastWakeAttemptMs = 0;
  uint32_t _autoSleepMs = 0;

  // Page cycling
  uint8_t _userPageCount = 1;
  uint8_t _activeUserPage = 0;
  uint32_t _pageCycleMs = 0;
  uint32_t _lastPageCycleMs = 0;

  // Page buffer iteration
  uint8_t _currentBufferPage = 0;
  bool _inPageIteration = false;

  // Health tracking
  DriverState _driverState = DriverState::UNINIT;
  uint32_t _lastOkMs = 0;
  uint32_t _lastErrorMs = 0;
  uint8_t _consecutiveFailures = 0;
  uint32_t _totalFailures = 0;
  uint32_t _totalSuccess = 0;
  Status _flushError{};  // Accumulated error for flush tracking
  bool _controlStateDirty = false;
  Status _controlStateError{};
};

}  // namespace SSD1315
