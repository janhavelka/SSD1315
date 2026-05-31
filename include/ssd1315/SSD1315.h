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
 * - Non-blocking flush with configurable byte budget
 * - Hardware scroll and display effects
 * - Auto-sleep on inactivity
 * - Page cycling for multi-screen UIs
 * - Test patterns for manufacturing
 *
 * ## Threading model
 * Single-threaded only. Call all methods from the same task (typically loop()).
 * Public APIs are not ISR-safe.
 *
 * ## Memory model
 * All allocations in begin(). Zero heap allocations in steady state.
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

// Forward declarations for internal types
struct FlushJob;

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
 *   display.begin(cfg);
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
 *       // Iteration complete - restart or do other work
 *       display.firstPage();
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
   * @brief Destructor. Calls end() if initialized.
   */
  ~SSD1315();

  // Non-copyable
  SSD1315(const SSD1315&) = delete;
  SSD1315& operator=(const SSD1315&) = delete;
  SSD1315(SSD1315&&) = delete;
  SSD1315& operator=(SSD1315&&) = delete;

  // ========================================================================
  // Lifecycle
  // ========================================================================

  /**
   * @brief Initialize the display with given configuration.
   *
   * Must be called before any other method. Allocates or attaches the
   * framebuffer, probes the configured I2C address, sends the SSD1315
   * initialization sequence, optionally clears controller GDDRAM, then sends
   * DISPLAY_ON and starts the power-on timing guard.
   *
   * @param config Configuration struct. i2cWrite must not be null.
   * @return Status Ok on success, error on failure.
   *
   * @note Bounded blocking lifecycle call. It performs many I2C writes
   *       synchronously; each transaction is bounded by Config::i2cTimeoutMs
   *       if the injected transport honors that timeout.
   * @note With Config::clearOnBegin true (default), a 128x64 panel clears
   *       1024 GDDRAM bytes synchronously in 32-byte chunks.
   * @note Config::i2cAddress is restricted to SSD1315 7-bit addresses 0x3C
   *       and 0x3D. Probe confirms ACK only, not controller identity.
   * @note After begin(), the display is on but flushes are deferred until
   *       Config::displayOnDelayMs has elapsed through tick().
   * @note Safe to call multiple times after a successful begin(); the driver
   *       resets runtime state before applying the new configuration.
   */
  Status begin(const Config& config);

  /**
   * @brief Cooperative update function. Call every loop iteration.
   *
   * Drives the flush state machine, auto-sleep timer, page cycling, and
   * power-on timing. Returns immediately after bounded work.
   *
   * @param nowMs Current monotonic time in milliseconds.
   *
   * @note Non-blocking. Sends at most byteBudgetPerTick bytes per call.
   * @note Handles 32-bit millisecond counter wraparound correctly.
   * @note Does nothing if not initialized.
   */
  void tick(uint32_t nowMs);

  /**
   * @brief Stop the driver and release resources.
   *
   * Sends display OFF command, disables the internal charge pump when the
   * active profile enabled it, and frees allocated buffer.
   * Safe to call multiple times or if not initialized.
   *
   * @note Best-effort shutdown: this API intentionally returns void so it can
   *       be used from destructors. The final display-off / charge-pump-off
   *       writes use an untracked raw path so an OFFLINE latch alone does not
   *       prevent the attempt. These best-effort writes do not update health
   *       counters; the framebuffer is released even if the bus is unavailable.
   */
  void end();

  /**
   * @brief Check if driver is initialized.
   * @return true if begin() succeeded and end() not called.
   */
  bool isInitialized() const { return _initialized; }

  /**
   * @brief Get current configuration.
   * @return Reference to active configuration.
   * @note Only valid if isInitialized() returns true.
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
   *
   * Requires begin() so the transport callbacks are configured.
   * Useful for:
   * - Checking if the configured device is present without affecting health state
   * - Diagnosing bus connectivity after initialization
   *
   * @return Status Ok if device ACK'd, DEVICE_NOT_FOUND for definite address
   *         NACK, otherwise the original transport error.
   *
   * @pre begin() must have succeeded so the transport callback is configured.
   *
   * @note SSD1315 has no WHOAMI register. Probe sends a NOP command (0xE3)
   *       and checks for ACK. Does NOT call _updateHealth().
   */
  Status probe();

  /**
   * @brief Attempt to recover the device from OFFLINE or DEGRADED state.
   *
   * Bounded blocking software recovery operation that:
   * 1. Probes device presence
   * 2. Re-sends the SSD1315 initialization sequence
   * 3. Optionally clears controller GDDRAM according to Config::clearOnRecover
   * 4. Sends DISPLAY_ON and marks framebuffer pages dirty for redraw
   *
   * @return Status Ok on success, error on failure.
   *
   * @note recover() does not own or toggle RES#. Hardware reset sequencing is
   *       an application/platform responsibility.
   * @note On success: state -> READY via _updateHealth().
   * @note On failure: state updated via _updateHealth().
   * @note Requires `_initialized == true`.
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
   * orientation/display mode changes, or a failed recovery/init resync. Cleared
   * only by a successful begin() or recover() full control-state resync.
   *
   * @return true when the application should call recover() or perform a full
   *         verified reinitialization before trusting cached panel controls.
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
   * @pre begin() must have completed successfully.
   * @note Blocks for I2C transaction (typically < 1ms).
   */
  Status sendCommand(uint8_t cmd);

  /**
   * @brief Send a command with one argument byte.
   *
   * @param cmd Command byte
   * @param arg Argument byte
   * @return Status Ok on success, I2C error on failure.
   *
   * @pre begin() must have completed successfully.
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
   * @pre begin() must have completed successfully.
   */
  Status sendCommand3(uint8_t cmd, uint8_t arg1, uint8_t arg2);

  /**
   * @brief Send a list of command bytes.
   *
   * @param cmds Pointer to command bytes
   * @param len Number of bytes
   * @return Status Ok on success, I2C error on failure.
   *
   * @pre begin() must have completed successfully.
   * @note A zero-length list is a no-op. A null pointer with len > 0 returns
   *       INVALID_CONFIG before any I2C transaction.
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
   * When enabled, display automatically sleeps after no activity for the
   * specified duration. Any draw call or touch() resets the timer.
   *
   * @param inactivityMs Timeout in milliseconds. 0 = disabled.
   */
  void setAutoSleep(uint32_t inactivityMs);

  /**
   * @brief Reset inactivity timer and wake display if sleeping.
   *
   * Call when user interaction occurs (button press, etc.) to keep
   * display awake and reset the auto-sleep timer.
   */
  void touch();

  // ========================================================================
  // Page cycling (multi-screen UI)
  // ========================================================================

  /**
   * @brief Set number of user-defined UI pages for cycling.
   *
   * @param count Number of pages (1-255). 0 or 1 disables cycling.
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
   * @param intervalMs Time between page switches. 0 = disabled.
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
   * @brief Draw a bitmap from PROGMEM or RAM.
   *
   * @param x Top-left X
   * @param y Top-left Y
   * @param bitmap Pointer to bitmap data (1 bit per pixel, MSB first)
   * @param w Bitmap width
   * @param h Bitmap height
   * @param on Pixel state for '1' bits
   *
   * @note Bitmap format: each byte is 8 horizontal pixels, MSB = leftmost.
   *       Rows are packed (no padding).
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
   */
  int16_t drawText(int16_t x, int16_t y, const char* str, bool on = true);

  /**
   * @brief Get width of a string in pixels.
   *
   * @param str Null-terminated string
   * @return Width in pixels (6 pixels per character).
   */
  static int16_t getTextWidth(const char* str);

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
   * @brief Clear last error status.
   */
  void clearError() { _lastError = Ok(); }

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
   * @warning Blocks; use sparingly. Prefer tick()-based async flush.
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
   * @brief Clear all dirty flags.
   */
  void clearDirty();

  /**
   * @brief Check if any pages are dirty.
   * @return true if any page needs flush.
   */
  bool isDirty() const;

 private:
  // ========== Internal types ==========

  enum class FlushState : uint8_t {
    IDLE,           ///< No flush in progress
    SET_ADDR,       ///< Setting column/page address
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

  Status initDisplay();
  Status clearGddram();  // Clear display GDDRAM directly (blocking)
  Status _sendCommand(uint8_t command);
  Status _sendCommand2(uint8_t command, uint8_t arg);
  Status _sendCommand3(uint8_t command, uint8_t arg1, uint8_t arg2);
  Status sendData(const uint8_t* data, size_t len);
  void tickFlush(uint32_t nowMs);
  void tickAutoSleep(uint32_t nowMs);
  void tickPageCycle(uint32_t nowMs);
  void tickPowerOn(uint32_t nowMs);
  void resetActivityTimer(uint32_t nowMs);
  void wakeIfSleeping();
  Status flushPageBlocking(uint8_t page);
  Status setAddressWindow(uint8_t colStart, uint8_t colEnd, uint8_t pageStart, uint8_t pageEnd);

  // Buffer helpers
  size_t bufferIndex(int16_t x, int16_t y) const;
  uint8_t bufferBit(int16_t y) const;
  bool isInBuffer(int16_t x, int16_t y) const;

  // Health tracking helpers
  uint32_t _nowMs() const;
  void _cooperativeYield() const;
  Status _updateHealth(const Status& st);
  Status _i2cWriteRaw(const uint8_t* data, size_t len);
  Status _i2cWriteTracked(const uint8_t* data, size_t len);
  Status _offlineStatus() const;
  void _reassertOfflineLatch();
  Status _applyConfig(bool clearDisplayRam);
  Status _turnDisplayOnAfterInit();
  void _markControlStateDirty(const Status& st);
  void _clearControlStateDirty();
  void _resetRuntimeState();

  // ========== State ==========

  Config _config{};
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
  Status _lastError{};

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
  bool _allowOfflineI2c = false;
  Status _flushError{};  // Accumulated error for flush tracking
  bool _controlStateDirty = false;
  Status _controlStateError{};
};

}  // namespace SSD1315
