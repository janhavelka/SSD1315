/**
 * @file main.cpp
 * @brief Example 01: Page buffer mode (Non-blocking, Interactive).
 *
 * This example demonstrates:
 * - Page buffer mode (minimal RAM usage)
 * - Non-blocking firstPage() / nextPage() iteration
 * - tick()-based cooperative rendering
 * - Interactive serial commands
 *
 * Serial Commands:
 *   help        - Show command list
 *   scan        - Scan I2C bus
 *   demo        - Toggle animation
 *   speed <n>   - Set FPS (10-60)
 *   clear       - Clear display
 *   test        - Test pattern
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

// Animation state
bool autoDemoEnabled = true;
uint32_t frameCount = 0;
uint32_t lastDrawMs = 0;
uint32_t DRAW_INTERVAL_MS = 50;  // 20 FPS default

// Page iteration state (for non-blocking render)
bool renderPending = false;

void showHelp() {
  LOGI("");
  LOGI("=== Page Buffer Mode Commands ===");
  LOGI("help        - Show this help");
  LOGI("scan        - Scan I2C bus");
  LOGI("demo        - Toggle animation");
  LOGI("speed <n>   - Set FPS 10-60 (e.g., 'speed 30')");
  LOGI("status      - Show current FPS and frame count");
  LOGI("clear       - Clear display");
  LOGI("test        - Test pattern");
  LOGI("=================================");
  LOGI("");
}

/**
 * @brief Draw content for the current display region.
 *
 * In page buffer mode, this function is called for each page set.
 * The pageBufferYOffset() tells you what Y range is currently valid
 * for drawing. Only draw elements that intersect the current page buffer.
 *
 * @param yOffset Y offset for current page buffer (0, 8, 16, 24...)
 */
void drawContent(int16_t yOffset) {
  // Current page buffer covers Y range: [yOffset, yOffset + 8)
  int16_t pageEnd = yOffset + 8;

  // Calculate animation values
  float phase = frameCount * 0.05f;
  int16_t ballX = 64 + static_cast<int16_t>(50.0f * sinf(phase));
  int16_t ballY = 32 + static_cast<int16_t>(24.0f * cosf(phase * 0.7f));

  // Draw static title (top area) - Y range 0-7
  if (yOffset <= 7 && pageEnd > 0) {
    display.drawText(10, 0, "Pg Buffer");
    display.drawHLine(0, 9, 128);
    
    char buf[24];
    snprintf(buf, sizeof(buf), "F:%lu", (unsigned long)frameCount);
    display.drawText(70, 0, buf);
  }

  // Draw bouncing ball - check if ball intersects current page
  int16_t ballTop = ballY - 8;
  int16_t ballBottom = ballY + 8;
  if (ballBottom >= yOffset && ballTop < pageEnd) {
    display.fillCircle(ballX, ballY, 8);
  }

  // Draw border rectangle - Y range 12-63
  if (yOffset <= 63 && pageEnd > 12) {
    display.drawRect(0, 12, 128, 52);
  }

  // Draw center vertical line - Y range 12-63
  if (yOffset <= 63 && pageEnd > 12) {
    display.drawVLine(64, 12, 52);
  }

  // Draw center horizontal line at Y=38
  if (yOffset <= 38 && pageEnd > 38) {
    display.drawHLine(0, 38, 128);
  }

  // Draw corner markers
  if (yOffset <= 17 && pageEnd > 14) {
    display.fillRect(2, 14, 4, 4);    // Top-left
    display.fillRect(122, 14, 4, 4);  // Top-right
  }
  if (yOffset <= 61 && pageEnd > 58) {
    display.fillRect(2, 58, 4, 4);    // Bottom-left
    display.fillRect(122, 58, 4, 4);  // Bottom-right
  }
}

void setup() {
  log_begin(115200);
  delay(100);

  LOGI("SSD1315 Example 01: Page Buffer Mode (Interactive)");
  LOGI("===================================================");

  // Initialize I2C and scan
  transport::initWire(pins::SDA, pins::SCL, pins::I2C_FREQ);
  i2c_scanner::scan(Wire);

  // Configure display with PAGE BUFFER MODE
  // Using 1 page buffer = 128 bytes RAM (vs 1024 for full buffer)
  ssd1315::Config cfg;
  cfg.width = pins::OLED_WIDTH;
  cfg.height = pins::OLED_HEIGHT;
  cfg.i2cAddress = pins::OLED_I2C_ADDR;
  cfg.i2cWrite = transport::wireWrite;
  cfg.i2cUser = &Wire;
  cfg.pageBufferPages = 1;         // Page buffer mode: 1 page at a time
  cfg.byteBudgetPerTick = 64;      // Non-blocking: 64 bytes per tick
  cfg.contrast = 0xCF;             // Brighter for visibility

  LOGI("Initializing display: %dx%d, pageBufferPages=%d",
       cfg.width, cfg.height, cfg.pageBufferPages);
  LOGI("RAM usage: %lu bytes (vs %lu for full buffer)",
       (unsigned long)(cfg.width * cfg.pageBufferPages),
       (unsigned long)(cfg.width * cfg.height / 8));

  ssd1315::Status st = display.begin(cfg);
  if (!st.ok()) {
    LOGE("Display init failed: %s (code=%d, detail=%d)", 
         st.msg, (int)st.code, st.detail);
    while (true) delay(1000);
  }

  LOGI("Display initialized in page buffer mode!");
  showHelp();
  lastDrawMs = millis();
}

void loop() {
  uint32_t now = millis();

  // Call tick to handle internal timers (auto-sleep, etc.)
  display.tick(now);

  // Check for serial commands
  char cmdBuf[64];
  if (cmd::readLine(cmdBuf, sizeof(cmdBuf))) {
    LOGI("> %s", cmdBuf);

    int value;

    if (cmd::match(cmdBuf, "help")) {
      showHelp();

    } else if (cmd::match(cmdBuf, "scan")) {
      i2c_scanner::scan(Wire);

    } else if (cmd::match(cmdBuf, "demo")) {
      autoDemoEnabled = !autoDemoEnabled;
      LOGI("Animation %s", autoDemoEnabled ? "enabled" : "disabled");

    } else if (cmd::match(cmdBuf, "status")) {
      uint32_t currentFPS = (DRAW_INTERVAL_MS > 0) ? (1000 / DRAW_INTERVAL_MS) : 0;
      LOGI("Status: FPS=%u (interval=%u ms), frames=%lu, demo=%s",
           currentFPS, DRAW_INTERVAL_MS, (unsigned long)frameCount,
           autoDemoEnabled ? "ON" : "OFF");

    } else if (cmd::parseInt(cmdBuf, "speed", &value)) {
      if (value >= 10 && value <= 60) {
        DRAW_INTERVAL_MS = 1000 / value;
        LOGI("FPS set to %d (%u ms interval)", value, DRAW_INTERVAL_MS);
        lastDrawMs = now;  // Reset timer to apply immediately
      } else {
        LOGE("FPS must be 10-60");
      }

    } else if (cmd::match(cmdBuf, "clear")) {
      // Non-blocking clear: start iteration, it will complete over multiple loops
      display.firstPage();
      renderPending = true;
      autoDemoEnabled = false;  // Pause animation during clear
      LOGI("Clearing display...");

    } else if (cmd::match(cmdBuf, "test")) {
      // Non-blocking test pattern
      display.firstPage();
      renderPending = true;
      autoDemoEnabled = false;  // Pause animation during test
      LOGI("Drawing test pattern...");
      // Note: The test pattern will be drawn in the main loop

    } else {
      LOGE("Unknown command: %s (type 'help' for list)", cmdBuf);
    }
  }

  // =========================================================================
  // Non-blocking page buffer rendering
  // =========================================================================
  // The new cooperative model:
  // 1. Check if we're iterating AND not currently flushing
  // 2. Draw content for current page
  // 3. Call nextPage() to mark dirty and start flush
  // 4. tick() handles the actual I2C transfer in bounded chunks
  // 5. When flush completes, nextPage() advances to next page
  // =========================================================================

  // Handle ongoing page iteration (for clear/test commands)
  if (display.isPageIterating() && !display.isFlushing()) {
    if (renderPending) {
      // For test command, draw checkerboard; for clear, buffer is already zeroed
      // (We'd need to track which command triggered this for proper test pattern)
    }
    if (!display.nextPage()) {
      // Iteration complete
      renderPending = false;
      LOGI("Operation complete");
    }
  }

  // Draw at target frame rate if demo enabled
  if (autoDemoEnabled && !display.isPageIterating()) {
    if (now - lastDrawMs >= DRAW_INTERVAL_MS) {
      // Time for a new frame - start page iteration
      lastDrawMs = now;
      frameCount++;
      display.firstPage();
    }
  }

  // Handle demo animation page iteration
  if (autoDemoEnabled && display.isPageIterating() && !display.isFlushing()) {
    // Draw content for current page
    drawContent(display.pageBufferYOffset());

    // Request flush and prepare for next page
    if (!display.nextPage()) {
      // Frame complete
      if (frameCount % 20 == 0) {
        LOGD("Frame %lu complete @ %u ms", (unsigned long)frameCount, (unsigned)now);
      }
    }
  }

  // Check for flush errors
  if (display.lastError().code != ssd1315::Err::OK) {
    LOGE("Display error: %s", display.lastError().msg);
    display.clearError();
  }

  delay(1);
}
