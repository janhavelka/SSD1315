/**
 * @file Config.h
 * @brief Configuration structure for SSD1315 OLED driver.
 *
 * All hardware-specific parameters (I2C transport, pins) are injected via this
 * struct. The library never hardcodes pin values or owns bus resources.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ssd1315/Status.h"

namespace ssd1315 {

/**
 * @brief I2C write callback function type.
 *
 * This function is called by the driver to send data over I2C. The application
 * must implement this using its I2C driver (Wire, esp_i2c, etc.).
 *
 * @param addr      7-bit I2C slave address (0x3C or 0x3D)
 * @param data      Pointer to data buffer to send (includes control byte)
 * @param len       Number of bytes to send
 * @param timeoutMs Maximum time to wait for completion (milliseconds)
 * @param user      User context pointer from Config::i2cUser
 * @return Status   Ok on success, I2C error on failure
 *
 * @note The first byte of data is always the control byte (0x00 for commands,
 *       0x40 for data). The callback should send all bytes in a single I2C
 *       transaction: START + addr + data[0..len-1] + STOP.
 * @note This callback MUST NOT block indefinitely; respect timeoutMs.
 * @note Return I2C_NACK_ADDR if address not ACKed, I2C_NACK_DATA if data not ACKed,
 *       I2C_TIMEOUT if timeout, I2C_BUS_ERROR for other bus errors.
 */
using I2cWriteFn = Status (*)(uint8_t addr, const uint8_t* data, size_t len,
                              uint32_t timeoutMs, void* user);

/**
 * @brief Hardware COM pins configuration for SSD1315.
 *
 * Maps to command 0xDA. Different panel layouts require different settings.
 */
enum class ComPinsConfig : uint8_t {
  SEQUENTIAL_NO_REMAP = 0x02,    ///< Sequential COM, left/right remap disabled
  ALTERNATIVE_NO_REMAP = 0x12,  ///< Alternative COM, left/right remap disabled (128x64 default)
  SEQUENTIAL_REMAP = 0x22,      ///< Sequential COM, left/right remap enabled
  ALTERNATIVE_REMAP = 0x32      ///< Alternative COM, left/right remap enabled
};

/**
 * @brief Charge pump voltage selection.
 *
 * Maps to command 0x8D argument. Higher voltage = brighter but more power.
 */
enum class ChargePumpVoltage : uint8_t {
  V7_5 = 0x14,  ///< 7.5V output (default, lower power)
  V8_5 = 0x94,  ///< 8.5V output
  V9_0 = 0x95   ///< 9.0V output (brightest, highest power)
};

/**
 * @brief Internal IREF current selection (SSD1315-specific).
 *
 * Maps to command 0xAD. Controls segment drive current.
 */
enum class IrefSelection : uint8_t {
  IREF_EXTERNAL = 0x00,      ///< External IREF (default)
  INTERNAL_19UA = 0x10,      ///< Internal IREF ~19µA, max segment current ~150µA
  INTERNAL_30UA = 0x30       ///< Internal IREF ~30µA, max segment current ~240µA
};

/**
 * @brief VCOMH deselect voltage level.
 *
 * Maps to command 0xDB. Affects display contrast range.
 */
enum class VcomhLevel : uint8_t {
  V_065_VCC = 0x00,  ///< ~0.65 × VCC
  V_071_VCC = 0x10,  ///< ~0.71 × VCC
  V_077_VCC = 0x20,  ///< ~0.77 × VCC (default)
  V_083_VCC = 0x30   ///< ~0.83 × VCC
};

/**
 * @brief Configuration for SSD1315 driver initialization.
 *
 * Pass to Ssd1315::begin() to configure the driver. Transport callback is required;
 * all other parameters have sensible defaults for 128x64 panels.
 *
 * @note Pin values and bus ownership belong to the application. The driver uses
 *       only the i2cWrite callback for all I2C operations.
 *
 * ## Memory modes:
 * - **Full buffer mode**: pageBufferPages == totalPages (height/8)
 *   Allocates width × height/8 bytes. Draw anywhere, flush dirty regions.
 *
 * - **Page buffer mode**: pageBufferPages < totalPages
 *   Allocates width × pageBufferPages bytes. Render and flush page-by-page
 *   using firstPage()/nextPage() iteration (u8g2-style).
 *
 * ## Example configuration:
 * @code
 * ssd1315::Config cfg;
 * cfg.width = 128;
 * cfg.height = 64;
 * cfg.i2cAddress = 0x3C;
 * cfg.i2cWrite = myI2cWriteCallback;
 * cfg.i2cUser = &myWireInstance;
 * cfg.pageBufferPages = 8;  // Full buffer
 * cfg.byteBudgetPerTick = 128;
 * @endcode
 */
struct Config {
  // ========== Display geometry ==========

  /// @brief Display width in pixels. Common values: 128, 96, 72, 64.
  /// @note Must be in range [1..128]. Validated in begin().
  uint8_t width = 128;

  /// @brief Display height in pixels. Must be multiple of 8.
  /// @note Common values: 64, 32, 16. Must be in range [8..64].
  uint8_t height = 64;

  // ========== I2C transport ==========

  /// @brief 7-bit I2C slave address. Valid range: 0x03–0x77 (user addresses).
  /// @note SSD1315 typically uses 0x3C or 0x3D, determined by SA0 pin.
  /// @note Addresses 0x00–0x02 and 0x78–0x7F are reserved for I2C protocol.
  uint8_t i2cAddress = 0x3C;

  /// @brief I2C write callback. REQUIRED - must not be null.
  /// @note The driver does not own the I2C bus. Application provides transport.
  I2cWriteFn i2cWrite = nullptr;

  /// @brief User context pointer passed to i2cWrite callback.
  /// @note Typically points to Wire instance or custom I2C manager.
  void* i2cUser = nullptr;

  // ========== Buffering strategy ==========

  /// @brief Number of pages in the RAM buffer. Range: [1..totalPages].
  /// @note totalPages = height/8. If pageBufferPages == totalPages, full buffer
  ///       mode is used. If less, page-buffer iteration mode is required.
  /// @note Buffer size = width × pageBufferPages bytes.
  uint8_t pageBufferPages = 8;

  /// @brief Maximum bytes to send per tick() call during flush.
  /// @note Larger = faster flush, but longer tick() blocking time.
  /// @note Typical values: 64, 128, 256. At 400kHz I2C, 128 bytes ≈ 2.5ms.
  /// @note Set to 0 to flush a full page per tick (blocking per page).
  uint16_t byteBudgetPerTick = 128;

  // ========== Timeouts ==========

  /// @brief I2C transaction timeout in milliseconds.
  /// @note Applied to each i2cWrite call. Must be > 0.
  uint32_t i2cTimeoutMs = 25;

  /// @brief Total flush operation timeout in milliseconds.
  /// @note If flush takes longer, it fails with TIMEOUT. 0 = no timeout.
  uint32_t flushTimeoutMs = 1000;

  // ========== Power-on timing ==========

  /// @brief Delay after display ON command before panel is fully active (ms).
  /// @note SSD1315 specifies ~100ms (tAF). Driver enforces this non-blocking.
  uint32_t displayOnDelayMs = 100;

  // ========== Feature timers ==========

  /// @brief Auto-sleep after inactivity timeout in milliseconds. 0 = disabled.
  /// @note When enabled, display sleeps after no draw activity for this duration.
  ///       Activity (draw calls, touch()) resets the timer and wakes display.
  uint32_t inactivitySleepMs = 0;

  /// @brief Page cycling interval in milliseconds. 0 = disabled.
  /// @note When enabled, driver automatically cycles through user pages at this
  ///       interval. Use setPageCount() and setActivePage() to configure.
  uint32_t pageCycleMs = 0;

  // ========== Display orientation ==========

  /// @brief Flip display horizontally (segment remap). Default: false.
  /// @note Maps to command 0xA0 (false) / 0xA1 (true).
  bool flipX = false;

  /// @brief Flip display vertically (COM scan direction). Default: false.
  /// @note Maps to command 0xC0 (false) / 0xC8 (true).
  bool flipY = false;

  /// @brief Invert display colors (swap on/off pixels). Default: false.
  /// @note Maps to command 0xA6 (false) / 0xA7 (true).
  bool invert = false;

  // ========== Hardware configuration ==========

  /// @brief Initial contrast value (0-255). Default: 0x7F.
  uint8_t contrast = 0x7F;

  /// @brief COM pins hardware configuration.
  /// @note Default is ALTERNATIVE_NO_REMAP (0x12) for 128x64 panels.
  ComPinsConfig comPins = ComPinsConfig::ALTERNATIVE_NO_REMAP;

  /// @brief Charge pump voltage selection.
  ChargePumpVoltage chargePumpVoltage = ChargePumpVoltage::V7_5;

  /// @brief IREF current selection (SSD1315-specific).
  IrefSelection iref = IrefSelection::INTERNAL_19UA;

  /// @brief VCOMH deselect voltage level.
  VcomhLevel vcomh = VcomhLevel::V_077_VCC;

  /// @brief Display clock divide ratio (1-16). Default: 1.
  /// @note Lower = faster refresh but higher power.
  uint8_t clockDivide = 1;

  /// @brief Oscillator frequency (0-15). Default: 8.
  /// @note Higher = faster refresh but may cause flicker.
  uint8_t oscFrequency = 8;

  /// @brief Pre-charge period phase 1 (1-15 DCLKs). Default: 2.
  uint8_t prechargePhase1 = 2;

  /// @brief Pre-charge period phase 2 (1-15 DCLKs). Default: 2.
  uint8_t prechargePhase2 = 2;

  /// @brief Display vertical offset (0-63). Default: 0.
  /// @note Maps to command 0xD3. Shifts display content vertically.
  uint8_t displayOffset = 0;

  /// @brief Display start line (0-63). Default: 0.
  /// @note Maps to command 0x40-0x7F. Sets RAM line shown at row 0.
  uint8_t startLine = 0;

  // ========== Externally managed buffer (advanced) ==========

  /// @brief External framebuffer pointer. If null, driver allocates buffer.
  /// @note If provided, must point to width × pageBufferPages bytes.
  /// @note Useful for memory-constrained systems or DMA buffers.
  uint8_t* externalBuffer = nullptr;
};

}  // namespace ssd1315
