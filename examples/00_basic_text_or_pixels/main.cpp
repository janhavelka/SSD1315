/**
 * @file main.cpp
 * @brief Example 00: Basic text and pixel drawing (Interactive).
 *
 * This example demonstrates:
 * - Full buffer mode initialization
 * - Drawing text, pixels, and shapes
 * - Non-blocking partial flush via tick()
 * - Interactive serial commands
 *
 * Serial Commands:
 *   help            - Show command list
 *   scan            - Scan I2C bus
 *   clear           - Clear display
 *   test            - Show test pattern
 *   text <msg>      - Draw text (e.g., 'text Hello World!')
 *   pixel <x> <y>   - Draw pixel (e.g., 'pixel 64 32')
 *   line <x1> <y1> <x2> <y2> - Draw line
 *   rect <x> <y> <w> <h> - Draw rectangle
 *   circle <x> <y> <r> - Draw circle
 *   contrast <0-255> - Set contrast
 *   invert <0|1>    - Set invert mode
 *   demo            - Toggle auto animation
 *
 * Hardware: ESP32-S2 or ESP32-S3 with SSD1315/SSD1306 128x64 OLED
 * Wiring: SDA=GPIO8, SCL=GPIO9 (adjust in BoardPins.h for your board)
 */

#include <Arduino.h>

#include "ssd1315/Ssd1315.h"
#include "examples/common/BoardPins.h"
#include "examples/common/BuildConfig.h"
#include "examples/common/CommandHandler.h"
#include "examples/common/I2cScanner.h"
#include "examples/common/I2cTransport.h"
#include "examples/common/Log.h"

// Display instance
ssd1315::Ssd1315 display;

// State
bool autoDemoEnabled = false;
uint32_t frameCount = 0;
uint32_t lastUpdateMs = 0;
constexpr uint32_t UPDATE_INTERVAL_MS = 1000;  // Update every second

void showHelp() {
  LOGI("");
  LOGI("=== SSD1315 Basic Commands ===");
  LOGI("help                    - Show this help");
  LOGI("scan                    - Scan I2C bus");
  LOGI("clear                   - Clear display");
  LOGI("test                    - Test pattern");
  LOGI("text <message>          - Draw text (e.g., 'text Hi')");
  LOGI("pixel <x> <y>           - Draw pixel");
  LOGI("line <x1> <y1> <x2> <y2> - Draw line");
  LOGI("rect <x> <y> <w> <h>    - Draw rectangle");
  LOGI("circle <x> <y> <r>      - Draw circle");
  LOGI("contrast <0-255>        - Set contrast");
  LOGI("invert <0|1>            - Invert display");
  LOGI("demo                    - Toggle animation");
  LOGI("==============================");
  LOGI("");
}

void setup() {
  log_begin(115200);
  delay(100);  // Let serial stabilize

  LOGI("SSD1315 Example 00: Basic Drawing (Interactive)");
  LOGI("=================================================");

  // Initialize I2C and scan bus
  LOGI("Initializing I2C on SDA=%d, SCL=%d @ %lu Hz",
       pins::SDA, pins::SCL, pins::I2C_FREQ);
  transport::initWire(pins::SDA, pins::SCL, pins::I2C_FREQ);
  
  i2c_scanner::scan(Wire);

  // Configure display
  ssd1315::Config cfg;
  cfg.width = pins::OLED_WIDTH;
  cfg.height = pins::OLED_HEIGHT;
  cfg.i2cAddress = pins::OLED_I2C_ADDR;
  cfg.i2cWrite = transport::wireWrite;
  cfg.i2cUser = &Wire;
  cfg.pageBufferPages = 8;       // Full buffer mode (128x64 = 8 pages)
  cfg.byteBudgetPerTick = 128;   // ~2.5ms per tick at 400kHz
  cfg.contrast = 0x7F;           // Medium brightness

  LOGI("Initializing display: %dx%d @ 0x%02X", cfg.width, cfg.height, cfg.i2cAddress);

  ssd1315::Status st = display.begin(cfg);
  if (!st.ok()) {
    LOGE("Display init failed: %s (code=%d, detail=%d)", 
         st.msg, (int)st.code, st.detail);
    while (true) {
      delay(1000);  // Halt
    }
  }

  LOGI("Display initialized successfully!");

  // Draw welcome screen
  display.clear();
  display.drawText(8, 4, "SSD1315 Ready!");
  display.drawHLine(0, 14, 128);
  display.drawText(0, 20, "Type 'help' for");
  display.drawText(0, 30, "commands");
  display.drawRect(2, 48, 124, 14);
  display.drawText(10, 52, "Interactive Mode");
  display.requestFlush();

  showHelp();
  LOGI("Ready for commands. Type 'help' for list.");
  lastUpdateMs = millis();
}

void loop() {
  uint32_t now = millis();

  // Drive the display state machine
  display.tick(now);

  // Check for serial commands
  char cmdBuf[64];
  if (cmd::readLine(cmdBuf, sizeof(cmdBuf))) {
    LOGI("> %s", cmdBuf);

    int x, y, x2, y2, w, h, r, value;

    if (cmd::match(cmdBuf, "help")) {
      showHelp();

    } else if (cmd::match(cmdBuf, "scan")) {
      i2c_scanner::scan(Wire);

    } else if (cmd::match(cmdBuf, "clear")) {
      display.clear();
      display.requestFlush();
      display.waitFlush(millis());  // Block until clear is visible
      LOGI("Display cleared");

    } else if (cmd::match(cmdBuf, "test")) {
      display.fillCheckerboard(4);
      display.requestFlush();
      LOGI("Test pattern displayed");

    } else if (strncmp(cmdBuf, "text ", 5) == 0) {
      display.drawText(0, 4, cmdBuf + 5);
      display.requestFlush();
      LOGI("Text drawn: %s", cmdBuf + 5);

    } else if (sscanf(cmdBuf, "pixel %d %d", &x, &y) == 2) {
      display.setPixel(x, y);
      display.requestFlush();
      LOGI("Pixel at (%d, %d)", x, y);

    } else if (sscanf(cmdBuf, "line %d %d %d %d", &x, &y, &x2, &y2) == 4) {
      display.drawLine(x, y, x2, y2);
      display.requestFlush();
      LOGI("Line from (%d,%d) to (%d,%d)", x, y, x2, y2);

    } else if (sscanf(cmdBuf, "rect %d %d %d %d", &x, &y, &w, &h) == 4) {
      display.drawRect(x, y, w, h);
      display.requestFlush();
      LOGI("Rectangle at (%d,%d) size %dx%d", x, y, w, h);

    } else if (sscanf(cmdBuf, "circle %d %d %d", &x, &y, &r) == 3) {
      display.drawCircle(x, y, r);
      display.requestFlush();
      LOGI("Circle at (%d,%d) radius %d", x, y, r);

    } else if (cmd::parseInt(cmdBuf, "contrast", &value)) {
      if (value >= 0 && value <= 255) {
        display.setContrast((uint8_t)value);
        LOGI("Contrast set to %d", value);
      } else {
        LOGE("Contrast must be 0-255");
      }

    } else if (cmd::parseInt(cmdBuf, "invert", &value)) {
      display.setInvert(value != 0);
      LOGI("Invert %s", value ? "ON" : "OFF");

    } else if (cmd::match(cmdBuf, "demo")) {
      autoDemoEnabled = !autoDemoEnabled;
      LOGI("Auto demo %s", autoDemoEnabled ? "enabled" : "disabled");
      if (autoDemoEnabled) {
        frameCount = 0;
        lastUpdateMs = now;
      }

    } else {
      LOGE("Unknown command: %s (type 'help' for list)", cmdBuf);
    }
  }

  // Run auto demo if enabled
  if (autoDemoEnabled && now - lastUpdateMs >= UPDATE_INTERVAL_MS) {
    lastUpdateMs = now;
    frameCount++;

    // Only update if not currently flushing
    if (!display.isFlushing()) {
      // Clear the frame counter area
      display.fillRect(0, 56, 128, 8, false);

      // Draw updated frame count
      char buf[32];
      snprintf(buf, sizeof(buf), "Frame: %lu", (unsigned long)frameCount);
      display.drawText(0, 56, buf);

      // Animate the small circle
      int16_t cx = 64 + static_cast<int16_t>(20.0f * sinf(frameCount * 0.1f));
      int16_t cy = 32 + static_cast<int16_t>(12.0f * cosf(frameCount * 0.15f));

      // Clear old circle area and draw new
      display.fillRect(40, 18, 50, 30, false);
      display.drawCircle(cx, cy, 10);

      // Request partial flush of changed areas
      display.requestFlushRect(0, 56, 128, 8);    // Frame counter
      display.requestFlushRect(40, 18, 50, 30);   // Animated circle

      if (frameCount % 10 == 0) {
        LOGD("Frame %lu", (unsigned long)frameCount);
      }
    }
  }

  // Small delay to prevent tight loop
  delay(10);
}
