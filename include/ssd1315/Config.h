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

namespace SSD1315 {

/**
 * @brief I2C write callback function type.
 *
 * This function is called by the driver to send data over I2C. The application
 * must implement this using its platform I2C driver or I2C manager.
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

/// @brief Optional I2C write-read callback for upper-layer uniformity.
///
/// SSD1315 does not use combined write-read transactions internally, but the
/// field is provided so higher-level glue can treat this library like the
/// others in the workspace.
using I2cWriteReadFn = Status (*)(uint8_t addr, const uint8_t* txData, size_t txLen,
                                  uint8_t* rxData, size_t rxLen, uint32_t timeoutMs,
                                  void* user);

/// @brief Monotonic millisecond clock callback.
using NowMsFn = uint32_t (*)(void* user);

/// @brief Optional cooperative scheduler yield callback.
using CooperativeYieldFn = void (*)(void* user);

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
  OFF = 0x10,   ///< Disable internal charge pump (reset/off sequence)
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
  IREF_EXTERNAL = 0x00,      ///< External IREF resistor mode
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
 * @brief Narrow SSD1315 panel presets derived from local module documentation.
 *
 * These are panel/electrical profiles, not controller compatibility profiles.
 * They do not configure I2C pins, bus speed, reset GPIO ownership, transport
 * callbacks, address, timeout, or buffering policy.
 */
enum class PanelProfile : uint8_t {
  GENERIC_128X64_INTERNAL_CHARGE_PUMP = 0,
  WISEVISION_X096_2864KSWPG01_H30_INTERNAL_DC_DC,
  WISEVISION_X096_2864KSWPG01_H30_EXTERNAL_VCC
};

/**
 * @brief Configuration for SSD1315 driver initialization.
 *
 * Pass to SSD1315::begin() to configure the driver. Transport callback is required;
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
 * SSD1315::Config cfg;
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

  /// @brief Controller command/init profile.
  /// @note Only ControllerProfile::SSD1315 is currently supported. The profile
  ///       includes SSD1315-specific commands such as SET_IREF (0xAD).
  ControllerProfile controllerProfile = ControllerProfile::SSD1315;

  /// @brief Display width in pixels. Common values: 128, 96, 72, 64.
  /// @note Must be in range [1..128]. Validated in begin().
  uint8_t width = 128;

  /// @brief Display height in pixels. Must be multiple of 8.
  /// @note Common values: 64, 32, 16. Datasheet multiplex range is [16..64].
  uint8_t height = 64;

  // ========== I2C transport ==========

  /// @brief 7-bit SSD1315 I2C slave address.
  /// @note Valid SSD1315 I2C addresses are 0x3C and 0x3D, determined by the
  ///       SA0 / D-C# pin. Do not pass 8-bit address forms 0x78 or 0x7A.
  uint8_t i2cAddress = 0x3C;

  /// @brief I2C write callback. REQUIRED - must not be null.
  /// @note The driver does not own the I2C bus. Application provides transport.
  I2cWriteFn i2cWrite = nullptr;

  /// @brief Optional I2C write-read callback for uniform upper-layer wiring.
  /// @note SSD1315 remains write-only internally and does not call this hook.
  I2cWriteReadFn i2cWriteRead = nullptr;

  /// @brief User context pointer passed to i2cWrite callback.
  /// @note Typically points to a platform bus context or custom I2C manager.
  void* i2cUser = nullptr;

  // ========== Optional timing hooks ==========

  /// @brief Optional millisecond clock callback.
  /// @note If null, time-based features use 0. Framework examples should
  ///       provide a platform monotonic millisecond clock from their adapter
  ///       layer.
  NowMsFn nowMs = nullptr;

  /// @brief Optional cooperative yield callback used in wait loops.
  /// @note If null, wait loops do not call a scheduler hook. Framework
  ///       examples should inject the platform's cooperative task yield.
  CooperativeYieldFn cooperativeYield = nullptr;

  /// @brief User context for timing callbacks.
  void* timeUser = nullptr;

  // ========== Buffering strategy ==========

  /// @brief Number of pages in the RAM buffer. Range: [1..totalPages].
  /// @note totalPages = height/8. If pageBufferPages == totalPages, full buffer
  ///       mode is used. If less, page-buffer iteration mode is required.
  /// @note Buffer size = width × pageBufferPages bytes.
  uint8_t pageBufferPages = 8;

  /// @brief Maximum data payload bytes for a tick() flush data instruction.
  /// @note tick() issues at most one flush instruction per call. Command
  ///       instructions do not consume this byte budget; data instructions are
  ///       additionally capped by the driver's I2C transfer chunk size.
  /// @note Use pollFlush() when an owner wants to allow multiple explicit
  ///       command/data instructions per poll.
  /// @note Values above the driver's data chunk size do not make one tick()
  ///       issue more than one transaction.
  /// @note Must be > 0; use an explicit blocking flush API for full-page waits.
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

  /// @brief Clear controller GDDRAM synchronously during begin().
  /// @note Default true preserves historical behavior and prevents stale RAM
  ///       from being shown before the first application flush. If false,
  ///       begin() still performs the blocking command init but skips the
  ///       blocking full-screen clear; the framebuffer is marked dirty so the
  ///       application can resync GDDRAM through normal flush scheduling.
  bool clearOnBegin = true;

  /// @brief Clear controller GDDRAM synchronously during recover().
  /// @note Default true preserves historical behavior. Production shared-bus
  ///       applications may set this false to avoid a full blocking GDDRAM
  ///       clear during recovery, then redraw/flush from the framebuffer.
  bool clearOnRecover = true;

  // ========== Feature timers ==========

  /// @brief Auto-sleep after inactivity timeout in milliseconds. 0 = disabled.
  /// @note When enabled, display sleeps after no draw activity for this duration.
  ///       Activity (draw calls, touch()) resets the timer and wakes display.
  uint32_t inactivitySleepMs = 0;

  /// @brief Page cycling interval in milliseconds. 0 = disabled.
  /// @note When enabled, driver automatically cycles through user pages at this
  ///       interval. Use setUserPageCount() and setActiveUserPage() to configure.
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

  /// @brief Initial contrast value (1-255). Default: 0x7F.
  /// @note SSD1315 command table defines 0x01..0xFF. begin() rejects 0x00.
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
  /// @note If provided, must point to width x pageBufferPages bytes.
  /// @note Useful for memory-constrained systems or DMA buffers.
  uint8_t* externalBuffer = nullptr;

  /// @brief Size of externalBuffer in bytes.
  /// @note Required when externalBuffer is non-null. begin() rejects buffers
  ///       smaller than width x pageBufferPages before any I2C transaction.
  /// @note Set to 0 when externalBuffer is null.
  size_t externalBufferSizeBytes = 0;

  // ========== Health tracking ==========

  /// @brief Consecutive failure threshold before OFFLINE state.
  /// @note Default: 3. Clamped to minimum 1 in begin().
  uint8_t offlineThreshold = 3;
};

/**
 * @brief Apply a documented panel preset to an existing Config.
 *
 * The caller still owns transport callbacks, I2C address selection, bus timing,
 * reset GPIO policy, and framebuffer strategy. Use this helper before begin().
 *
 * @param cfg Configuration to update in place.
 * @param profile Panel profile to apply.
 * @return Ok on success, INVALID_CONFIG for unsupported enum values.
 */
inline Status applyPanelProfile(Config& cfg, PanelProfile profile) {
  Config next = cfg;
  next.controllerProfile = ControllerProfile::SSD1315;
  next.width = 128;
  next.height = 64;
  next.displayOffset = 0;
  next.startLine = 0;
  next.comPins = ComPinsConfig::ALTERNATIVE_NO_REMAP;
  next.prechargePhase1 = 2;
  next.prechargePhase2 = 2;

  switch (profile) {
    case PanelProfile::GENERIC_128X64_INTERNAL_CHARGE_PUMP:
      next.flipX = false;
      next.flipY = false;
      next.contrast = 0x7F;
      next.clockDivide = 1;
      next.oscFrequency = 8;
      next.vcomh = VcomhLevel::V_077_VCC;
      next.chargePumpVoltage = ChargePumpVoltage::V7_5;
      next.iref = IrefSelection::INTERNAL_19UA;
      break;

    case PanelProfile::WISEVISION_X096_2864KSWPG01_H30_INTERNAL_DC_DC:
      next.flipX = true;
      next.flipY = true;
      next.contrast = 0xB0;
      next.clockDivide = 1;
      next.oscFrequency = 9;
      next.vcomh = VcomhLevel::V_083_VCC;
      next.chargePumpVoltage = ChargePumpVoltage::V7_5;
      next.iref = IrefSelection::IREF_EXTERNAL;
      break;

    case PanelProfile::WISEVISION_X096_2864KSWPG01_H30_EXTERNAL_VCC:
      next.flipX = true;
      next.flipY = true;
      next.contrast = 0xB0;
      next.clockDivide = 1;
      next.oscFrequency = 9;
      next.vcomh = VcomhLevel::V_083_VCC;
      next.chargePumpVoltage = ChargePumpVoltage::OFF;
      next.iref = IrefSelection::IREF_EXTERNAL;
      break;

    default:
      return Error(Err::INVALID_CONFIG, "unsupported panel profile");
  }

  cfg = next;
  return Ok();
}

}  // namespace SSD1315
