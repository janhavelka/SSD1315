/**
 * @file main.cpp
 * @brief Example 00: Basic text and pixel drawing with partial flush.
 *
 * This example demonstrates:
 * - Full buffer mode initialization
 * - Drawing text, pixels, and shapes
 * - Non-blocking partial flush via tick()
 * - Activity timer and display updates
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

// State
uint32_t frameCount = 0;
uint32_t lastUpdateMs = 0;
constexpr uint32_t UPDATE_INTERVAL_MS = 1000;  // Update every second

void setup() {
  log_begin(115200);
  delay(100);  // Let serial stabilize

  LOGI("SSD1315 Example 00: Basic Text and Pixels");
  LOGI("==========================================");

  // Initialize I2C
  LOGI("Initializing I2C on SDA=%d, SCL=%d @ %lu Hz",
       pins::SDA, pins::SCL, pins::I2C_FREQ);
  transport::initWire(pins::SDA, pins::SCL, pins::I2C_FREQ);

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
    LOGE("Display init failed: %s (code=%d)", st.msg, (int)st.code);
    while (true) {
      delay(1000);  // Halt
    }
  }

  LOGI("Display initialized successfully!");

  // Draw initial content
  display.clear();

  // Title
  display.drawText(0, 0, "SSD1315 Driver");
  display.drawHLine(0, 9, 128);

  // Info text
  display.drawText(0, 16, "128x64 I2C OLED");
  display.drawText(0, 26, "Full buffer mode");

  // Draw some shapes
  display.drawRect(100, 16, 24, 24);
  display.fillRect(105, 21, 14, 14);

  // Draw a circle
  display.drawCircle(64, 48, 12);
  display.fillCircle(64, 48, 6);

  // Frame counter placeholder
  display.drawText(0, 56, "Frame: 0");

  // Request flush (async)
  display.requestFlush();

  LOGI("Initial frame drawn, flushing...");
  lastUpdateMs = millis();
}

void loop() {
  uint32_t now = millis();

  // Drive the display state machine
  display.tick(now);

  // Update display once per second
  if (now - lastUpdateMs >= UPDATE_INTERVAL_MS) {
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
      int16_t cy = 48 + static_cast<int16_t>(8.0f * cosf(frameCount * 0.15f));

      // Clear old circle area and draw new
      display.fillRect(40, 38, 50, 22, false);
      display.drawCircle(cx, cy, 10);

      // Request partial flush of changed areas
      display.requestFlushRect(0, 56, 128, 8);    // Frame counter
      display.requestFlushRect(40, 38, 50, 22);   // Animated circle

      if (frameCount % 10 == 0) {
        LOGD("Frame %lu, flushing %s", (unsigned long)frameCount,
             display.isDirty() ? "dirty" : "clean");
      }
    }
  }

  // Small delay to prevent tight loop
  delay(1);
}
