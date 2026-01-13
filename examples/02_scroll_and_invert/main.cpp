/**
 * @file main.cpp
 * @brief Example 02: Hardware scroll and display effects (Interactive).
 *
 * This example demonstrates:
 * - Hardware horizontal scrolling (smooth, zero CPU)
 * - Hardware vertical + horizontal scrolling
 * - Display invert effect
 * - Contrast adjustment
 * - Auto-sleep feature
 * - Test patterns (checkerboard, stripes)
 * - Interactive serial commands
 *
 * Serial Commands:
 *   help            - Show command list
 *   scan            - Scan I2C bus
 *   clear           - Clear display
 *   test            - Show test pattern
 *   text <msg>      - Draw text
 *   contrast <0-255> - Set contrast
 *   invert <0|1>    - Set invert mode
 *   sleep <ms>      - Set auto-sleep timeout (0=disable)
 *   scroll <dir>    - Start scroll (right, left, up, down, stop)
 *   demo            - Run auto demo
 *   reset           - Reset display
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

// Demo mode state
bool autoDemoEnabled = false;
bool invertState = false;  // Track invert toggle state

// Demo state machine (for auto demo)
enum class DemoState {
  INTRO,
  SCROLL_RIGHT,
  SCROLL_LEFT,
  SCROLL_VERTICAL,
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
uint32_t stateDurationMs = 4000;  // Default 4 seconds per state
uint8_t subStep = 0;

void showHelp() {
  LOGI("");
  LOGI("=== SSD1315 Interactive Commands ===");
  LOGI("help            - Show this help");
  LOGI("scan            - Scan I2C bus for devices");
  LOGI("clear           - Clear display");
  LOGI("test            - Show checkerboard test pattern");
  LOGI("text <message>  - Draw text at top (e.g., 'text Hello')");
  LOGI("contrast <n>    - Set contrast 0-255 (e.g., 'contrast 128')");
  LOGI("invert          - Toggle invert mode");
  LOGI("sleep <ms>      - Set auto-sleep timeout (0=off, e.g., 'sleep 5000')");
  LOGI("scroll <dir>    - Start scroll: right, left, up, down, stop");
  LOGI("demo            - Run automatic demo sequence");
  LOGI("reset           - Reset display to defaults");
  LOGI("====================================");
  LOGI("");
}

void drawIntroScreen() {
  display.clear();
  display.drawText(10, 0, "SSD1315 Effects");
  display.drawHLine(0, 9, 128);
  display.drawText(0, 16, "Hardware Scroll");
  display.drawText(0, 26, "Invert / Contrast");
  display.drawText(0, 36, "Test Patterns");
  display.drawText(0, 46, "Auto-Sleep");
  display.drawText(20, 56, "Watch the demo!");
  display.requestFlush();
}

void drawScrollContent() {
  display.clear();

  // Draw text that will scroll
  display.drawText(0, 0, ">>> SCROLLING >>>");
  display.drawText(0, 10, "Hardware scroll!");
  display.drawText(0, 20, "Zero CPU usage");

  // Draw some shapes
  for (int i = 0; i < 8; i++) {
    display.fillRect(i * 16 + 2, 32, 12, 12);
  }

  // Draw pattern at bottom
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

  // Draw half-filled pattern
  display.fillRect(0, 16, 64, 40);
  display.drawRect(64, 16, 64, 40);

  display.drawText(8, 28, "ON", false);  // Inverted text
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

  // Draw brightness indicator bar
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
  display.drawText(0, 32, "Activity wakes up");
  display.drawText(0, 44, "Press reset to");
  display.drawText(0, 54, "wake (simulated)");
  display.requestFlush();
}

void setup() {
  log_begin(115200);
  delay(500);  // Longer delay for serial to stabilize

  LOGI("SSD1315 Example 02: Scroll and Effects (Interactive)");
  LOGI("=====================================================");
  LOG_SERIAL.flush();

  // Initialize I2C with timeout and bus recovery
  LOGI("Initializing I2C on SDA=%d, SCL=%d @ %lu Hz",
       pins::SDA, pins::SCL, (unsigned long)pins::I2C_FREQ);
  LOG_SERIAL.flush();
  transport::initWire(pins::SDA, pins::SCL, pins::I2C_FREQ, 100);  // 100ms timeout
  LOGI("I2C initialized");
  LOG_SERIAL.flush();
  
  // Quick I2C test - probe the OLED address
  LOGI("Testing I2C to 0x%02X...", pins::OLED_I2C_ADDR);
  LOG_SERIAL.flush();
  Wire.beginTransmission(pins::OLED_I2C_ADDR);
  uint8_t i2cResult = Wire.endTransmission();
  if (i2cResult == 0) {
    LOGI("I2C probe OK - device found");
  } else {
    LOGE("I2C probe FAILED: error=%d", i2cResult);
  }
  LOG_SERIAL.flush();

  // Configure display
  ssd1315::Config cfg;
  cfg.width = pins::OLED_WIDTH;
  cfg.height = pins::OLED_HEIGHT;
  cfg.i2cAddress = pins::OLED_I2C_ADDR;
  cfg.i2cWrite = transport::wireWrite;
  cfg.i2cUser = &Wire;
  cfg.pageBufferPages = 8;       // Full buffer for smooth updates
  cfg.byteBudgetPerTick = 256;   // Faster flush
  cfg.contrast = 0x7F;

  LOGI("Calling display.begin()...");
  LOG_SERIAL.flush();

  ssd1315::Status st = display.begin(cfg);
  if (!st.ok()) {
    LOGE("Display init failed: %s (code=%d, detail=%d)", 
         st.msg, (int)st.code, st.detail);
    LOGE("Check wiring: SDA=%d, SCL=%d", pins::SDA, pins::SCL);
    while (true) delay(1000);
  }

  LOGI("Display initialized OK!");
  LOG_SERIAL.flush();
  
  // Show welcome screen
  LOGI("Drawing welcome screen...");
  LOG_SERIAL.flush();
  display.clear();
  display.drawText(8, 4, "SSD1315 Ready!");
  display.drawHLine(0, 14, 128);
  display.drawText(0, 20, "Type 'help' for");
  display.drawText(0, 30, "commands");
  display.drawHLine(0, 42, 128);
  display.drawText(16, 48, "Waiting...");
  
  LOGI("Requesting flush...");
  LOG_SERIAL.flush();
  display.requestFlush();
  
  // Wait for welcome screen to appear (blocking)
  LOGI("Waiting for flush (up to 5s)...");
  LOG_SERIAL.flush();
  ssd1315::Status flushSt = display.waitFlush(millis(), 5000);
  if (!flushSt.ok()) {
    LOGE("Flush failed: %s (code=%d)", flushSt.msg, (int)flushSt.code);
  } else {
    LOGI("Welcome screen displayed!");
  }
  LOG_SERIAL.flush();

  showHelp();
  LOGI("Ready for commands. Type 'help' for list.");
  LOG_SERIAL.flush();
}

void loop() {
  uint32_t now = millis();

  // Drive display state machine
  display.tick(now);

  // Check for serial commands
  char cmdBuf[64];
  if (cmd::readLine(cmdBuf, sizeof(cmdBuf))) {
    LOGI("> %s", cmdBuf);
    LOG_SERIAL.flush();  // Force output before any I2C operation

    int value;

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

    } else if (cmd::parseInt(cmdBuf, "contrast", &value)) {
      if (value >= 0 && value <= 255) {
        display.setContrast((uint8_t)value);
        LOGI("Contrast set to %d", value);
      } else {
        LOGE("Contrast must be 0-255");
      }

    } else if (cmd::match(cmdBuf, "invert")) {
      invertState = !invertState;
      ssd1315::Status st = display.setInvert(invertState);
      if (st.ok()) {
        LOGI("Invert %s", invertState ? "ON" : "OFF");
      } else {
        LOGE("Invert failed: %s (code=%d)", st.msg, (int)st.code);
      }

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
      } else if (strcmp(dir, "up") == 0) {
        display.stopScroll();
        delay(10);
        display.setVerticalScrollArea(0, 64);
        // SSD1315 only supports diagonal scroll (vertical + horizontal)
        display.startVerticalScroll(true, 0, 7, ssd1315::ScrollSpeed::FRAMES_5, 1);
        LOGI("Scrolling up+left (diagonal - hardware limitation)");
      } else if (strcmp(dir, "down") == 0) {
        display.stopScroll();
        delay(10);
        display.setVerticalScrollArea(0, 64);
        // SSD1315 only supports diagonal scroll (vertical + horizontal)
        display.startVerticalScroll(false, 0, 7, ssd1315::ScrollSpeed::FRAMES_5, 1);
        LOGI("Scrolling down+right (diagonal - hardware limitation)");
      } else if (strcmp(dir, "stop") == 0) {
        display.stopScroll();
        LOGI("Scroll stopped");
      } else {
        LOGE("Unknown direction. Use: right, left, up, down, stop");
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
        // Wait for intro screen to be visible
        display.waitFlush(millis());
      } else {
        LOGI("Auto demo stopped");
        display.stopScroll();
        display.setAutoSleep(0);
        display.setInvert(false);
      }
  invertState = false;
      
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

  // State duration check
  uint32_t elapsed = now - stateStartMs;

  switch (state) {
    case DemoState::INTRO:
      if (elapsed >= stateDurationMs) {
        LOGI("Starting scroll right demo");
        drawScrollContent();
        // Wait for flush before starting scroll
        while (display.isFlushing()) {
          display.tick(millis());
          delay(1);
        }
        display.startHorizontalScroll(false, 0, 7, ssd1315::ScrollSpeed::FRAMES_5);
        state = DemoState::SCROLL_RIGHT;
        stateStartMs = now;
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
        LOGI("Starting vertical scroll");
        display.stopScroll();
        delay(10);
        drawScrollContent();
        while (display.isFlushing()) {
          display.tick(millis());
          delay(1);
        }
        display.setVerticalScrollArea(0, 64);
        display.startVerticalScroll(false, 0, 7, ssd1315::ScrollSpeed::FRAMES_4, 1);
        state = DemoState::SCROLL_VERTICAL;
        stateStartMs = millis();
      }
      break;

    case DemoState::SCROLL_VERTICAL:
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
      // Toggle invert every 500ms
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
      // Sweep contrast
      static uint8_t contrast = 0;
      if (elapsed >= 50) {
        contrast += 8;
        display.setContrast(contrast);
        drawContrastDemo(contrast);
        stateStartMs = now;

        if (contrast >= 248) {
          display.setContrast(0x7F);  // Reset to normal
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
        display.setAutoSleep(3000);  // Sleep after 3 seconds
        subStep = 1;
      }
      // After showing sleep for a bit, wake and restart
      if (elapsed >= 5000 && subStep == 1) {
        LOGI("Waking display (simulated activity)");
        display.touch();  // Wake up
        subStep = 2;
      }
      if (elapsed >= stateDurationMs) {
        display.setAutoSleep(0);  // Disable auto-sleep
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
        // Restart demo
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
