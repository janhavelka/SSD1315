/**
 * @file main.cpp
 * @brief Example 00: Interactive SSD1315 demo (shapes, scroll, effects).
 *
 * This example demonstrates:
 * - Full buffer mode with non-blocking flush
 * - Drawing primitives (text, pixels, lines, rectangles, circles)
 * - Hardware scrolling (smooth, zero CPU)
 * - Display effects (invert, contrast, patterns)
 * - Auto-sleep feature
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
 *   invert          - Toggle invert mode
 *   sleep <ms>      - Set auto-sleep timeout (0=disable)
 *   scroll <dir>    - Start scroll (right, left, stop)
 *   demo            - Toggle auto demo sequence
 *   reset           - Reset display to defaults
 *
 * Hardware: ESP32-S2 or ESP32-S3 with SSD1315/SSD1306 128x64 OLED
 * Wiring: SDA=GPIO8, SCL=GPIO9 (adjust in BoardConfig.h for your board)
 */

#include <Arduino.h>

#include "ssd1315/Ssd1315.h"
#include "examples/common/BoardConfig.h"
#include "examples/common/BuildConfig.h"
#include "examples/common/CommandHandler.h"
#include "examples/common/I2cScanner.h"
#include "examples/common/I2cTransport.h"
#include "examples/common/Log.h"

// Display instance
ssd1315::Ssd1315 display;

// State
bool autoDemoEnabled = false;
bool invertState = false;  // Track invert toggle state

// Demo state machine
enum class DemoState {
  INTRO,
  SHAPES,
  SCROLL_RIGHT,
  SCROLL_LEFT,
  INVERT_DEMO,
  CONTRAST_DEMO,
  PATTERN_CHECKER,
  PATTERN_VSTRIPES,
  PATTERN_HSTRIPES,
  AUTO_SLEEP_DEMO,
  DONE
};

DemoState state = DemoState::INTRO;
uint32_t stateStartMs = 0;
uint32_t stateDurationMs = 4000;
uint8_t subStep = 0;

void showHelp() {
  LOGI("");
  LOGI("=== SSD1315 Interactive Demo ===");
  LOGI("help                    - Show this help");
  LOGI("scan                    - Scan I2C bus");
  LOGI("clear                   - Clear display");
  LOGI("test                    - Test pattern");
  LOGI("text <message>          - Draw text (e.g., 'text Hello')");
  LOGI("pixel <x> <y>           - Draw pixel");
  LOGI("line <x1> <y1> <x2> <y2> - Draw line");
  LOGI("rect <x> <y> <w> <h>    - Draw rectangle");
  LOGI("circle <x> <y> <r>      - Draw circle");
  LOGI("contrast <0-255>        - Set contrast");
  LOGI("invert                  - Toggle invert mode");
  LOGI("sleep <ms>              - Auto-sleep timeout (0=off)");
  LOGI("scroll <dir>            - Start scroll: right, left, stop");
  LOGI("demo                    - Toggle auto demo");
  LOGI("reset                   - Reset to defaults");
  LOGI("================================");
  LOGI("");
}

void drawIntroScreen() {
  display.clear();
  display.drawText(10, 0, "SSD1315 Demo");
  display.drawHLine(0, 9, 128);
  display.drawText(0, 16, "Drawing & Shapes");
  display.drawText(0, 26, "Hardware Scroll");
  display.drawText(0, 36, "Visual Effects");
  display.drawText(0, 46, "Test Patterns");
  display.drawText(20, 56, "Enjoy the demo!");
  display.requestFlush();
}

void drawShapesDemo() {
  display.clear();
  display.drawText(16, 0, "Shape Drawing");
  display.drawHLine(0, 9, 128);
  
  // Various shapes
  display.drawRect(4, 14, 20, 20);
  display.fillRect(28, 14, 20, 20);
  display.drawCircle(68, 24, 10);
  display.fillCircle(98, 24, 10);
  
  // Lines
  display.drawLine(4, 40, 124, 40);
  display.drawLine(64, 42, 64, 62);
  
  display.drawText(8, 54, "Primitives Demo");
  display.requestFlush();
}

void drawScrollContent() {
  display.clear();
  display.drawText(0, 0, ">>> SCROLLING >>>");
  display.drawText(0, 10, "Hardware scroll!");
  display.drawText(0, 20, "Zero CPU usage");
  
  for (int i = 0; i < 8; i++) {
    display.fillRect(i * 16 + 2, 32, 12, 12);
  }
  
  for (int x = 0; x < 128; x += 8) {
    display.drawVLine(x, 48, 16);
  }
  
  display.drawText(10, 56, "<<< SMOOTH <<<");
  display.requestFlush();
}

void drawInvertDemo() {
  display.clear();
  display.drawText(16, 0, "Invert Demo");
  display.drawHLine(0, 9, 128);
  
  display.fillRect(0, 16, 64, 40);
  display.drawRect(64, 16, 64, 40);
  
  display.drawText(8, 28, "ON", false);
  display.drawText(80, 28, "OFF");
  display.drawText(20, 56, "Toggle invert");
  display.requestFlush();
}

void drawContrastDemo(uint8_t contrast) {
  display.clear();
  display.drawText(10, 0, "Contrast Demo");
  display.drawHLine(0, 9, 128);
  
  char buf[32];
  snprintf(buf, sizeof(buf), "Level: %d/255", contrast);
  display.drawText(20, 24, buf);
  
  display.drawRect(10, 40, 108, 12);
  int barWidth = (contrast * 104) / 255;
  display.fillRect(12, 42, barWidth, 8);
  display.requestFlush();
}

void drawAutoSleepDemo() {
  display.clear();
  display.drawText(4, 0, "Auto-Sleep Demo");
  display.drawHLine(0, 9, 128);
  display.drawText(0, 20, "Sleep in 3 sec...");
  display.drawText(0, 32, "Activity wakes");
  display.drawText(0, 44, "Press reset to");
  display.drawText(0, 54, "wake (simulated)");
  display.requestFlush();
}

void setup() {
  log_begin(115200);
  delay(100);  // Let serial stabilize

  LOGI("SSD1315 Example 00: Interactive Demo");
  LOGI("=====================================");

  // Initialize I2C and scan bus
  LOGI("Initializing I2C on SDA=%d, SCL=%d @ %u Hz",
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
  display.drawText(16, 2, "SSD1315 Ready!");
  display.drawHLine(0, 12, 128);
  display.drawText(0, 18, "Type 'help' for");
  display.drawText(0, 28, "command list");
  display.drawText(0, 44, "or 'demo' for");
  display.drawText(0, 54, "auto sequence");
  display.requestFlush();

  showHelp();
  LOGI("Ready for commands. Type 'help' for list.");
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
      display.waitFlush(millis());
      LOGI("Display cleared");

    } else if (cmd::match(cmdBuf, "test")) {
      display.fillCheckerboard(4);
      display.drawText(20, 28, "Test Pattern", false);
      display.requestFlush();
      display.waitFlush(millis());
      LOGI("Test pattern displayed");

    } else if (strncmp(cmdBuf, "text ", 5) == 0) {
      display.clear();
      display.drawText(0, 4, cmdBuf + 5);
      display.requestFlush();
      display.waitFlush(millis());
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

    } else if (cmd::match(cmdBuf, "invert")) {
      invertState = !invertState;
      display.setInvert(invertState);
      LOGI("Invert %s", invertState ? "ON" : "OFF");

    } else if (cmd::parseInt(cmdBuf, "sleep", &value)) {
      display.setAutoSleep(value);
      LOGI("Auto-sleep %s", value > 0 ? "enabled" : "disabled");

    } else if (strncmp(cmdBuf, "scroll ", 7) == 0) {
      const char* dir = cmdBuf + 7;
      if (strcmp(dir, "right") == 0) {
        display.stopScroll();
        delay(10);
        display.startHorizontalScroll(false, 0, 7, ssd1315::ScrollSpeed::FRAMES_5);
        LOGI("Scrolling right");
      } else if (strcmp(dir, "left") == 0) {
        display.stopScroll();
        delay(10);
        display.startHorizontalScroll(true, 0, 7, ssd1315::ScrollSpeed::FRAMES_5);
        LOGI("Scrolling left");
      } else if (strcmp(dir, "stop") == 0) {
        display.stopScroll();
        LOGI("Scroll stopped");
      } else {
        LOGE("Unknown direction. Use: right, left, stop");
      }

    } else if (cmd::match(cmdBuf, "demo")) {
      autoDemoEnabled = !autoDemoEnabled;
      if (autoDemoEnabled) {
        LOGI("Auto demo started");
        state = DemoState::INTRO;
        stateStartMs = millis();
        stateDurationMs = 4000;
        subStep = 0;
        drawIntroScreen();
        display.waitFlush(millis());
      } else {
        LOGI("Auto demo stopped");
        display.stopScroll();
        display.setAutoSleep(0);
        display.setInvert(false);
        invertState = false;
      }

    } else if (cmd::match(cmdBuf, "reset")) {
      display.stopScroll();
      display.setInvert(false);
      invertState = false;
      display.setContrast(0x7F);
      display.setAutoSleep(0);
      display.clear();
      display.drawText(20, 28, "Reset done!");
      display.requestFlush();
      LOGI("Display reset to defaults");

    } else {
      LOGE("Unknown command: %s (type 'help' for list)", cmdBuf);
    }
  }

  // Run auto demo if enabled
  if (!autoDemoEnabled) {
    delay(10);
    return;
  }

  // Wait for flush to complete before state transitions
  if (display.isFlushing()) {
    delay(1);
    return;
  }

  // Recapture current time for accurate state timing
  now = millis();

  // State duration check
  uint32_t elapsed = now - stateStartMs;

  switch (state) {
    case DemoState::INTRO:
      if (elapsed >= stateDurationMs) {
        LOGI("Shapes demo");
        drawShapesDemo();
        state = DemoState::SHAPES;
        stateStartMs = now;
        stateDurationMs = 3000;
      }
      break;

    case DemoState::SHAPES:
      if (elapsed >= stateDurationMs) {
        LOGI("Starting scroll right demo");
        drawScrollContent();
        while (display.isFlushing()) {
          display.tick(millis());
          delay(1);
        }
        display.startHorizontalScroll(false, 0, 7, ssd1315::ScrollSpeed::FRAMES_5);
        state = DemoState::SCROLL_RIGHT;
        stateStartMs = now;
        stateDurationMs = 4000;
      }
      break;

    case DemoState::SCROLL_RIGHT:
      if (elapsed >= stateDurationMs) {
        LOGI("Switching to scroll left");
        display.stopScroll();
        delay(10);
        drawScrollContent();
        while (display.isFlushing()) {
          display.tick(millis());
          delay(1);
        }
        display.startHorizontalScroll(true, 0, 7, ssd1315::ScrollSpeed::FRAMES_3);
        state = DemoState::SCROLL_LEFT;
        stateStartMs = millis();
      }
      break;

    case DemoState::SCROLL_LEFT:
      if (elapsed >= stateDurationMs) {
        LOGI("Starting invert demo");
        display.stopScroll();
        delay(10);
        drawInvertDemo();
        subStep = 0;
        state = DemoState::INVERT_DEMO;
        stateStartMs = millis();
        stateDurationMs = 3000;
      }
      break;

    case DemoState::INVERT_DEMO:
      if (elapsed >= 500 && subStep < 6) {
        subStep++;
        display.setInvert(subStep & 1);
        stateStartMs = now;
      } else if (subStep >= 6) {
        display.setInvert(false);
        LOGI("Starting contrast demo");
        subStep = 0;
        state = DemoState::CONTRAST_DEMO;
        stateStartMs = now;
      }
      break;

    case DemoState::CONTRAST_DEMO: {
      static uint8_t contrast = 0;
      if (elapsed >= 50) {
        contrast += 8;
        display.setContrast(contrast);
        drawContrastDemo(contrast);
        stateStartMs = now;

        if (contrast >= 248) {
          display.setContrast(0x7F);
          LOGI("Starting pattern demos");
          state = DemoState::PATTERN_CHECKER;
          stateStartMs = millis();
          stateDurationMs = 2000;
        }
      }
      break;
    }

    case DemoState::PATTERN_CHECKER:
      if (elapsed == 0 || subStep == 0) {
        LOGI("Checkerboard pattern");
        display.fillCheckerboard(4);
        display.drawText(20, 28, "Checkerboard", false);
        display.requestFlush();
        subStep = 1;
      }
      if (elapsed >= stateDurationMs) {
        state = DemoState::PATTERN_VSTRIPES;
        stateStartMs = now;
        subStep = 0;
      }
      break;

    case DemoState::PATTERN_VSTRIPES:
      if (subStep == 0) {
        LOGI("Vertical stripes pattern");
        display.fillVerticalStripes(4);
        display.drawText(16, 28, "V-Stripes", false);
        display.requestFlush();
        subStep = 1;
      }
      if (elapsed >= stateDurationMs) {
        state = DemoState::PATTERN_HSTRIPES;
        stateStartMs = now;
        subStep = 0;
      }
      break;

    case DemoState::PATTERN_HSTRIPES:
      if (subStep == 0) {
        LOGI("Horizontal stripes pattern");
        display.fillHorizontalStripes(4);
        display.drawText(16, 28, "H-Stripes", false);
        display.requestFlush();
        subStep = 1;
      }
      if (elapsed >= stateDurationMs) {
        state = DemoState::AUTO_SLEEP_DEMO;
        stateStartMs = now;
        subStep = 0;
        stateDurationMs = 8000;
      }
      break;

    case DemoState::AUTO_SLEEP_DEMO:
      if (subStep == 0) {
        LOGI("Auto-sleep demo");
        drawAutoSleepDemo();
        display.setAutoSleep(3000);
        subStep = 1;
      }
      if (elapsed >= 5000 && subStep == 1) {
        LOGI("Waking display");
        display.touch();
        subStep = 2;
      }
      if (elapsed >= stateDurationMs) {
        display.setAutoSleep(0);
        state = DemoState::DONE;
        stateStartMs = now;
      }
      break;

    case DemoState::DONE:
      if (subStep == 0) {
        LOGI("Demo complete! Restarting...");
        display.clear();
        display.drawText(20, 24, "Demo Complete!");
        display.drawText(12, 40, "Restarting...");
        display.requestFlush();
        subStep = 1;
      }
      if (elapsed >= 3000) {
        state = DemoState::INTRO;
        stateStartMs = millis();
        stateDurationMs = 4000;
        subStep = 0;
        drawIntroScreen();
      }
      break;
  }

  delay(1);
}
