/**
 * @file CommandTable.h
 * @brief SSD1315 command table definitions and helpers.
 *
 * Contains all SSD1315 commands from the datasheet command tables.
 * Use for direct command access via sendCommand() / sendCommandList().
 */

#pragma once

#include <stdint.h>

namespace SSD1315 {

/**
 * @brief SSD1315 command bytes.
 *
 * All commands from SSD1315 datasheet Tables 1-1, Charge Pump, Scrolling,
 * and Advance Graphic command tables.
 */
namespace cmd {

// ========== Fundamental commands (Table 1-1) ==========

// Addressing / pointers
static constexpr uint8_t SET_LOWER_COL_START = 0x00;   ///< 0x00-0x0F: Lower column start (Page mode)
static constexpr uint8_t SET_HIGHER_COL_START = 0x10;  ///< 0x10-0x17: Higher column start (Page mode)
static constexpr uint8_t SET_MEMORY_MODE = 0x20;       ///< Set memory addressing mode (+ 1 arg)
static constexpr uint8_t SET_COL_ADDR = 0x21;          ///< Set column address range (+ 2 args)
static constexpr uint8_t SET_PAGE_ADDR = 0x22;         ///< Set page address range (+ 2 args)
static constexpr uint8_t SET_PAGE_START = 0xB0;        ///< 0xB0-0xB7: Set page start (Page mode)
static constexpr uint8_t SET_START_LINE = 0x40;        ///< 0x40-0x7F: Set display start line

// Memory addressing modes
static constexpr uint8_t ADDR_MODE_HORIZONTAL = 0x00;  ///< Horizontal addressing mode
static constexpr uint8_t ADDR_MODE_VERTICAL = 0x01;    ///< Vertical addressing mode
static constexpr uint8_t ADDR_MODE_PAGE = 0x02;        ///< Page addressing mode

// Display enable / global modes
static constexpr uint8_t DISPLAY_OFF = 0xAE;           ///< Display OFF (sleep)
static constexpr uint8_t DISPLAY_ON = 0xAF;            ///< Display ON
static constexpr uint8_t DISPLAY_RAM = 0xA4;           ///< Resume to RAM content
static constexpr uint8_t DISPLAY_ALL_ON = 0xA5;        ///< Entire display ON (ignore RAM)
static constexpr uint8_t NORMAL_DISPLAY = 0xA6;        ///< Normal display (1=lit)
static constexpr uint8_t INVERT_DISPLAY = 0xA7;        ///< Inverse display (0=lit)

// Orientation
static constexpr uint8_t SEG_REMAP_OFF = 0xA0;         ///< Segment remap off (column 0 = SEG0)
static constexpr uint8_t SEG_REMAP_ON = 0xA1;          ///< Segment remap on (column 127 = SEG0)
static constexpr uint8_t COM_SCAN_INC = 0xC0;          ///< COM scan normal (row 0 = COM0)
static constexpr uint8_t COM_SCAN_DEC = 0xC8;          ///< COM scan remapped (row 0 = COM[N-1])

// Geometry / layout
static constexpr uint8_t SET_MULTIPLEX = 0xA8;         ///< Set multiplex ratio (+ 1 arg: mux-1)
static constexpr uint8_t SET_DISPLAY_OFFSET = 0xD3;    ///< Set display offset (+ 1 arg)
static constexpr uint8_t SET_COM_PINS = 0xDA;          ///< Set COM pins config (+ 1 arg)

// Analog / timing
static constexpr uint8_t SET_CONTRAST = 0x81;          ///< Set contrast (+ 1 arg: 0-255)
static constexpr uint8_t SET_CLOCK_DIV = 0xD5;         ///< Set clock divide / osc freq (+ 1 arg)
static constexpr uint8_t SET_PRECHARGE = 0xD9;         ///< Set pre-charge period (+ 1 arg)
static constexpr uint8_t SET_VCOMH = 0xDB;             ///< Set VCOMH level (+ 1 arg)

// Misc
static constexpr uint8_t NOP = 0xE3;                   ///< No operation

// SSD1315-specific (IREF selection)
static constexpr uint8_t SET_IREF = 0xAD;              ///< Set IREF selection (+ 1 arg)

// ========== Charge pump commands ==========

static constexpr uint8_t SET_CHARGE_PUMP = 0x8D;       ///< Charge pump setting (+ 1 arg)

// ========== Scrolling commands ==========

static constexpr uint8_t SCROLL_RIGHT = 0x26;          ///< Right horizontal scroll setup
static constexpr uint8_t SCROLL_LEFT = 0x27;           ///< Left horizontal scroll setup
static constexpr uint8_t SCROLL_VERT_RIGHT = 0x29;     ///< Vertical + right horizontal scroll
static constexpr uint8_t SCROLL_VERT_LEFT = 0x2A;      ///< Vertical + left horizontal scroll
static constexpr uint8_t SCROLL_RIGHT_ONE_COL = 0x2C;  ///< Right scroll by one column (content)
static constexpr uint8_t SCROLL_LEFT_ONE_COL = 0x2D;   ///< Left scroll by one column (content)
static constexpr uint8_t SCROLL_DEACTIVATE = 0x2E;     ///< Deactivate scroll
static constexpr uint8_t SCROLL_ACTIVATE = 0x2F;       ///< Activate scroll
static constexpr uint8_t SET_VERT_SCROLL_AREA = 0xA3;  ///< Set vertical scroll area (+ 2 args)

// ========== Advance graphic commands ==========

static constexpr uint8_t SET_FADE_BLINK = 0x23;        ///< Set fade out / blinking (+ 1 arg)
static constexpr uint8_t SET_ZOOM = 0xD6;              ///< Set zoom in (+ 1 arg)

// ========== Control bytes ==========

static constexpr uint8_t CTRL_COMMAND = 0x00;          ///< Control byte: following bytes are commands
static constexpr uint8_t CTRL_DATA = 0x40;             ///< Control byte: following bytes are GDDRAM data

}  // namespace cmd

/**
 * @brief Scroll speed / frame interval settings.
 *
 * Used with horizontal and vertical scroll setup commands.
 * Value determines frames between scroll steps.
 */
enum class ScrollSpeed : uint8_t {
  FRAMES_5 = 0x00,    ///< 5 frames per scroll step
  FRAMES_64 = 0x01,   ///< 64 frames per scroll step
  FRAMES_128 = 0x02,  ///< 128 frames per scroll step
  FRAMES_256 = 0x03,  ///< 256 frames per scroll step
  FRAMES_3 = 0x04,    ///< 3 frames per scroll step
  FRAMES_4 = 0x05,    ///< 4 frames per scroll step
  FRAMES_25 = 0x06,   ///< 25 frames per scroll step
  FRAMES_2 = 0x07     ///< 2 frames per scroll step (fastest)
};

/**
 * @brief Fade/blink mode settings.
 *
 * Used with SET_FADE_BLINK (0x23) command.
 */
enum class FadeMode : uint8_t {
  OFF = 0x00,         ///< Fade/blink disabled
  FADE_OUT = 0x20,    ///< Fade out mode (contrast decreases to off)
  BLINK = 0x30        ///< Blink mode (fade out then fade in loop)
};

}  // namespace SSD1315
