/**
 * @file main.cpp
 * @brief Example 01: Page buffer mode demonstration.
 *
 * This example demonstrates:
 * - Page buffer mode (minimal RAM usage)
 * - firstPage() / nextPage() iteration (u8g2-style)
 * - Rendering content page-by-page
 * - Suitable for memory-constrained applications
 *
 * In page buffer mode, only a portion of the framebuffer is in RAM.
 * You must render the full screen content in a loop, and the driver
 * flushes each page as you iterate.
 *
 * Hardware: ESP32-S2 or ESP32-S3 with SSD1315/SSD1306 128x64 OLED
 * Wiring: SDA=GPIO8, SCL=GPIO9 (adjust in BoardPins.h for your board)
 */

#include <Arduino.h>

#include "ssd1315/Ssd1315.h"
#include "examples/common/BoardPins.h"
#include "examples/common/BuildConfig.h"
#include "examples/common/I2cTransport.h"
#include "examples/common/Log.h"

// Display instance
ssd1315::Ssd1315 display;

// Animation state
uint32_t frameCount = 0;
uint32_t lastDrawMs = 0;
constexpr uint32_t DRAW_INTERVAL_MS = 50;  // 20 FPS target

/**
 * @brief Draw content for the current display region.
 *
 * In page buffer mode, this function is called for each page set.
 * The pageBufferYOffset() tells you what Y range is currently valid
 * for drawing. Drawing outside this range has no effect.
 *
 * @param yOffset Y offset for current page buffer
 */
void drawContent(int16_t yOffset) {
  // Calculate animation values
  float phase = frameCount * 0.05f;
  int16_t ballX = 64 + static_cast<int16_t>(50.0f * sinf(phase));
  int16_t ballY = 32 + static_cast<int16_t>(24.0f * cosf(phase * 0.7f));

  // Draw static title (top area)
  display.drawText(10, 0, "Page Buffer Mode");
  display.drawHLine(0, 9, 128);

  // Draw frame counter
  char buf[24];
  snprintf(buf, sizeof(buf), "F:%lu", (unsigned long)frameCount);
  display.drawText(90, 0, buf);

  // Draw bouncing ball
  display.fillCircle(ballX, ballY, 8);

  // Draw border rectangle
  display.drawRect(0, 12, 128, 52);

  // Draw some reference lines
  display.drawVLine(64, 12, 52);  // Center vertical
  display.drawHLine(0, 38, 128);  // Center horizontal

  // Draw corners markers
  display.fillRect(2, 14, 4, 4);    // Top-left
  display.fillRect(122, 14, 4, 4);  // Top-right
  display.fillRect(2, 58, 4, 4);    // Bottom-left
  display.fillRect(122, 58, 4, 4);  // Bottom-right
}

void setup() {
  log_begin(115200);
  delay(100);

  LOGI("SSD1315 Example 01: Page Buffer Mode");
  LOGI("=====================================");

  // Initialize I2C
  transport::initWire(pins::SDA, pins::SCL, pins::I2C_FREQ);

  // Configure display with PAGE BUFFER MODE
  // Using 1 page buffer = 128 bytes RAM (vs 1024 for full buffer)
  ssd1315::Config cfg;
  cfg.width = pins::OLED_WIDTH;
  cfg.height = pins::OLED_HEIGHT;
  cfg.i2cAddress = pins::OLED_I2C_ADDR;
  cfg.i2cWrite = transport::wireWrite;
  cfg.i2cUser = &Wire;
  cfg.pageBufferPages = 1;         // Page buffer mode: 1 page at a time
  cfg.byteBudgetPerTick = 0;       // Unlimited (blocking flush in nextPage)
  cfg.contrast = 0xCF;             // Brighter for visibility

  LOGI("Initializing display: %dx%d, pageBufferPages=%d",
       cfg.width, cfg.height, cfg.pageBufferPages);
  LOGI("RAM usage: %lu bytes (vs %lu for full buffer)",
       (unsigned long)(cfg.width * cfg.pageBufferPages),
       (unsigned long)(cfg.width * cfg.height / 8));

  ssd1315::Status st = display.begin(cfg);
  if (!st.ok()) {
    LOGE("Display init failed: %s", st.msg);
    while (true) delay(1000);
  }

  LOGI("Display initialized in page buffer mode!");
  lastDrawMs = millis();
}

void loop() {
  uint32_t now = millis();

  // Call tick to handle internal timers (auto-sleep, etc.)
  display.tick(now);

  // Draw at target frame rate
  if (now - lastDrawMs >= DRAW_INTERVAL_MS) {
    lastDrawMs = now;
    frameCount++;

    // Page buffer render loop (u8g2-style)
    // firstPage() clears buffer and starts at page 0
    // nextPage() flushes current page and advances; returns false when done
    display.firstPage();
    do {
      // Get Y offset for coordinate mapping (optional - helps with logic)
      int16_t yOffset = display.pageBufferYOffset();

      // Draw all content - driver handles clipping to current page
      drawContent(yOffset);

    } while (display.nextPage());

    // Log every 100 frames
    if (frameCount % 100 == 0) {
      LOGI("Frame %lu completed", (unsigned long)frameCount);
    }
  }

  // Small delay
  delay(1);
}
