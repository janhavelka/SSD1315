/**
 * @file main.cpp
 * @brief Example 02: Health Tracking & Stress Test Demo
 *
 * This example demonstrates:
 * - Verbose health state monitoring
 * - Driver state transitions (UNINIT → READY → DEGRADED → OFFLINE)
 * - Health counter behavior
 * - probe() vs recover() semantics
 * - Stress testing with rapid I2C operations
 * - Simulated device disconnection scenarios
 *
 * Serial Commands:
 *   help           - Show command list
 *   health         - Print verbose health diagnostics
 *   brief          - Print one-line health summary
 *   probe          - Run probe() (no tracking)
 *   recover        - Run recover() (with tracking)
 *   stress <n>     - Run n rapid setContrast operations
 *   flushstress <n>- Run n flush operations
 *   burst <n>      - Burst n commands as fast as possible
 *   simerr         - Simulate error (invalid contrast cmd)
 *   threshold <n>  - Show current threshold (read-only)
 *   counters       - Show raw counter values
 *   monitor <ms>   - Start health monitor (0=off)
 *   contrast <v>   - Set contrast (tracks health)
 *   text <msg>     - Draw text and flush
 *   clear          - Clear display
 *   reset          - Reset display
 *
 * Hardware: ESP32-S2 or ESP32-S3 with SSD1315/SSD1306 128x64 OLED
 */

#include <Arduino.h>

#include "ssd1315/Ssd1315.h"
#include "examples/common/BoardConfig.h"
#include "examples/common/BuildConfig.h"
#include "examples/common/CommandHandler.h"
#include "examples/common/I2cScanner.h"
#include "examples/common/I2cTransport.h"
#include "examples/common/Log.h"
#include "examples/common/HealthDiag.h"

// Example configuration constants
static constexpr uint8_t OFFLINE_THRESHOLD = 5;  ///< Consecutive failures before OFFLINE state

// Display instance
ssd1315::Ssd1315 display;

// Health monitor for continuous tracking
diag::HealthMonitor healthMonitor;
bool monitorEnabled = false;

// Stress test state
bool stressRunning = false;
uint32_t stressCount = 0;
uint32_t stressRemaining = 0;
uint32_t stressSuccessCount = 0;
uint32_t stressFailCount = 0;
uint32_t stressStartMs = 0;

void showHelp() {
  LOGI("");
  LOGI("+----------------------------------------------------------+");
  LOGI("|       SSD1315 HEALTH TRACKING & STRESS TEST              |");
  LOGI("+----------------------------------------------------------+");
  LOGI("| DIAGNOSTICS:                                             |");
  LOGI("|   health           - Verbose health diagnostics          |");
  LOGI("|   brief            - One-line health summary             |");
  LOGI("|   counters         - Raw counter values                  |");
  LOGI("|   threshold        - Show offline threshold              |");
  LOGI("|   monitor <ms>     - Periodic health log (0=off)         |");
  LOGI("+----------------------------------------------------------+");
  LOGI("| DEVICE OPERATIONS:                                       |");
  LOGI("|   probe            - Device presence check (no tracking) |");
  LOGI("|   recover          - Attempt recovery (with tracking)    |");
  LOGI("|   contrast <0-255> - Set contrast (tracks health)        |");
  LOGI("|   text <msg>       - Draw text and flush                 |");
  LOGI("|   clear            - Clear display                       |");
  LOGI("+----------------------------------------------------------+");
  LOGI("| STRESS TESTS:                                            |");
  LOGI("|   stress <n>       - n rapid setContrast() calls         |");
  LOGI("|   flushstress <n>  - n sequential flush operations       |");
  LOGI("|   burst <n>        - n commands as fast as possible      |");
  LOGI("+----------------------------------------------------------+");
  LOGI("| OTHER:                                                   |");
  LOGI("|   help             - Show this help                      |");
  LOGI("|   scan             - Scan I2C bus                        |");
  LOGI("|   reset            - Reinitialize display                |");
  LOGI("+----------------------------------------------------------+");
  LOGI("");
}

/**
 * @brief Print raw counter values without formatting.
 */
void printRawCounters() {
  LOGI("Raw Health Counters:");
  LOGI("  state()              = %d (%s)", 
       (int)display.state(), diag::stateToString(display.state()));
  LOGI("  isOnline()           = %s", display.isOnline() ? "true" : "false");
  LOGI("  consecutiveFailures()= %u", display.consecutiveFailures());
  LOGI("  totalSuccess()       = %lu", (unsigned long)display.totalSuccess());
  LOGI("  totalFailures()      = %lu", (unsigned long)display.totalFailures());
  LOGI("  lastOkMs()           = %lu", (unsigned long)display.lastOkMs());
  LOGI("  lastErrorMs()        = %lu", (unsigned long)display.lastErrorMs());
  
  ssd1315::Status err = display.lastError();
  LOGI("  lastError().code     = %d (%s)", (int)err.code, diag::errToString(err.code));
  LOGI("  lastError().detail   = %ld", (long)err.detail);
  LOGI("  lastError().msg      = \"%s\"", err.msg ? err.msg : "(null)");
}

/**
 * @brief Run probe() and show result with context.
 */
void runProbe() {
  LOGI("+---------------------------------------+");
  LOGI("|           PROBE OPERATION             |");
  LOGI("+---------------------------------------+");
  LOGI("| Note: probe() does NOT update health  |");
  LOGI("| counters or state. It's diagnostic    |");
  LOGI("| only.                                 |");
  LOGI("+---------------------------------------+");
  
  // Snapshot before
  diag::HealthSnapshot before;
  before.capture(display);
  
  LOGI("Before probe:");
  diag::printHealthOneLine(display);
  
  // Run probe
  ssd1315::Status st = display.probe();
  
  LOGI("Probe result: %s (code=%d: %s)", 
       st.ok() ? "SUCCESS" : "FAILED",
       (int)st.code, diag::errToString(st.code));
  if (!st.ok()) {
    LOGI("  Message: %s", st.msg);
    LOGI("  Detail: %ld", (long)st.detail);
  }
  
  // Snapshot after
  diag::HealthSnapshot after;
  after.capture(display);
  
  LOGI("After probe:");
  diag::printHealthOneLine(display);
  
  LOGI("Changes:");
  diag::printHealthDiff(before, after);
}

/**
 * @brief Run recover() and show result with context.
 */
void runRecover() {
  LOGI("+---------------------------------------+");
  LOGI("|          RECOVER OPERATION            |");
  LOGI("+---------------------------------------+");
  LOGI("| Note: recover() DOES update health    |");
  LOGI("| counters and state. It runs probe()   |");
  LOGI("| then reinitializes the display.       |");
  LOGI("+---------------------------------------+");
  
  // Snapshot before
  diag::HealthSnapshot before;
  before.capture(display);
  
  LOGI("Before recover:");
  diag::printHealthOneLine(display);
  
  // Run recover
  ssd1315::Status st = display.recover();
  
  LOGI("Recover result: %s (code=%d: %s)", 
       st.ok() ? "SUCCESS" : "FAILED",
       (int)st.code, diag::errToString(st.code));
  if (!st.ok()) {
    LOGI("  Message: %s", st.msg);
    LOGI("  Detail: %ld", (long)st.detail);
  }
  
  // Snapshot after
  diag::HealthSnapshot after;
  after.capture(display);
  
  LOGI("After recover:");
  diag::printHealthOneLine(display);
  
  LOGI("Changes:");
  diag::printHealthDiff(before, after);
}

/**
 * @brief Run contrast stress test.
 */
void runContrastStress(uint32_t count) {
  LOGI("+---------------------------------------+");
  LOGI("|      CONTRAST STRESS TEST             |");
  LOGI("+---------------------------------------+");
  LOGI("| Running %5lu setContrast() calls     |", (unsigned long)count);
  LOGI("| Each call = 1 health tracking event   |");
  LOGI("+---------------------------------------+");
  
  diag::HealthSnapshot before;
  before.capture(display);
  
  uint32_t successCount = 0;
  uint32_t failCount = 0;
  uint32_t startMs = millis();
  
  for (uint32_t i = 0; i < count; i++) {
    uint8_t contrast = (i % 256);
    ssd1315::Status st = display.setContrast(contrast);
    if (st.ok()) {
      successCount++;
    } else {
      failCount++;
      LOGD("  Iteration %lu failed: %s", (unsigned long)i, diag::errToString(st.code));
    }
    
    // Log progress every 100 iterations
    if ((i + 1) % 100 == 0) {
      LOGI("  Progress: %lu/%lu (OK:%lu, FAIL:%lu)",
           (unsigned long)(i + 1), (unsigned long)count,
           (unsigned long)successCount, (unsigned long)failCount);
    }
  }
  
  uint32_t elapsed = millis() - startMs;
  
  diag::HealthSnapshot after;
  after.capture(display);
  
  LOGI("");
  LOGI("+---------------------------------------+");
  LOGI("|          STRESS TEST RESULTS          |");
  LOGI("+---------------------------------------+");
  LOGI("| Total iterations:  %5lu              |", (unsigned long)count);
  LOGI("| Successes:         %5lu              |", (unsigned long)successCount);
  LOGI("| Failures:          %5lu              |", (unsigned long)failCount);
  LOGI("| Time elapsed:      %5lu ms           |", (unsigned long)elapsed);
  if (elapsed > 0) {
    LOGI("| Rate:              %5lu ops/sec      |", (unsigned long)(count * 1000 / elapsed));
  }
  LOGI("+---------------------------------------+");
  
  LOGI("Health changes:");
  diag::printHealthDiff(before, after);
}

/**
 * @brief Run flush stress test.
 */
void runFlushStress(uint32_t count) {
  LOGI("+---------------------------------------+");
  LOGI("|        FLUSH STRESS TEST              |");
  LOGI("+---------------------------------------+");
  LOGI("| Running %5lu flush operations        |", (unsigned long)count);
  LOGI("| Each flush = 1 health tracking event  |");
  LOGI("| (regardless of page count)            |");
  LOGI("+---------------------------------------+");
  
  diag::HealthSnapshot before;
  before.capture(display);
  
  uint32_t successCount = 0;
  uint32_t failCount = 0;
  uint32_t startMs = millis();
  
  for (uint32_t i = 0; i < count; i++) {
    // Draw something different each time
    display.clear();
    char buf[32];
    snprintf(buf, sizeof(buf), "Flush #%lu", (unsigned long)(i + 1));
    display.drawText(20, 28, buf);
    
    // Request and wait for flush
    display.requestFlush();
    ssd1315::Status st = display.waitFlush(millis());
    
    if (st.ok()) {
      successCount++;
    } else {
      failCount++;
      LOGD("  Flush %lu failed: %s", (unsigned long)(i + 1), diag::errToString(st.code));
    }
    
    // Log progress every 10 flushes
    if ((i + 1) % 10 == 0) {
      LOGI("  Progress: %lu/%lu (OK:%lu, FAIL:%lu)",
           (unsigned long)(i + 1), (unsigned long)count,
           (unsigned long)successCount, (unsigned long)failCount);
    }
  }
  
  uint32_t elapsed = millis() - startMs;
  
  diag::HealthSnapshot after;
  after.capture(display);
  
  LOGI("");
  LOGI("+---------------------------------------+");
  LOGI("|       FLUSH STRESS TEST RESULTS       |");
  LOGI("+---------------------------------------+");
  LOGI("| Total flushes:     %5lu              |", (unsigned long)count);
  LOGI("| Successes:         %5lu              |", (unsigned long)successCount);
  LOGI("| Failures:          %5lu              |", (unsigned long)failCount);
  LOGI("| Time elapsed:      %5lu ms           |", (unsigned long)elapsed);
  if (elapsed > 0) {
    LOGI("| Rate:              %5lu flushes/sec  |", (unsigned long)(count * 1000 / elapsed));
    LOGI("| Avg per flush:     %5lu ms           |", (unsigned long)(elapsed / count));
  }
  LOGI("+---------------------------------------+");
  
  LOGI("Health changes:");
  diag::printHealthDiff(before, after);
  
  LOGI("Expected health counter change: +%lu successes, +%lu failures",
       (unsigned long)successCount, (unsigned long)failCount);
  LOGI("Actual change: +%lu successes, +%lu failures",
       (unsigned long)(after.totalSuccess - before.totalSuccess),
       (unsigned long)(after.totalFailures - before.totalFailures));
}

/**
 * @brief Run burst command test (as fast as possible).
 */
void runBurstTest(uint32_t count) {
  LOGI("+---------------------------------------+");
  LOGI("|         BURST COMMAND TEST            |");
  LOGI("+---------------------------------------+");
  LOGI("| Sending %5lu commands as fast as     |", (unsigned long)count);
  LOGI("| possible (no delays).                 |");
  LOGI("+---------------------------------------+");
  
  diag::HealthSnapshot before;
  before.capture(display);
  
  uint32_t successCount = 0;
  uint32_t failCount = 0;
  uint32_t startMs = millis();
  
  // Mix of different commands
  for (uint32_t i = 0; i < count; i++) {
    ssd1315::Status st;
    switch (i % 4) {
      case 0:
        st = display.setContrast(i % 256);
        break;
      case 1:
        st = display.setInvert(i % 2);
        break;
      case 2:
        st = display.setAllPixelsOn(false);
        break;
      case 3:
        st = display.setSleep(false);
        break;
    }
    
    if (st.ok()) {
      successCount++;
    } else {
      failCount++;
    }
  }
  
  uint32_t elapsed = millis() - startMs;
  
  diag::HealthSnapshot after;
  after.capture(display);
  
  LOGI("+---------------------------------------+");
  LOGI("|        BURST TEST RESULTS             |");
  LOGI("+---------------------------------------+");
  LOGI("| Total commands:    %5lu              |", (unsigned long)count);
  LOGI("| Successes:         %5lu              |", (unsigned long)successCount);
  LOGI("| Failures:          %5lu              |", (unsigned long)failCount);
  LOGI("| Time elapsed:      %5lu ms           |", (unsigned long)elapsed);
  if (elapsed > 0) {
    LOGI("| Rate:              %5lu cmds/sec     |", (unsigned long)(count * 1000 / elapsed));
  }
  LOGI("+---------------------------------------+");
  
  LOGI("Health changes:");
  diag::printHealthDiff(before, after);
}

void setup() {
  log_begin(115200);
  delay(100);

  LOGI("");
  LOGI("+----------------------------------------------------------+");
  LOGI("|  SSD1315 Example 02: Health Tracking & Stress Test       |");
  LOGI("+----------------------------------------------------------+");
  LOGI("");

  // Initialize I2C
  LOGI("Initializing I2C on SDA=%d, SCL=%d @ %u Hz",
       pins::SDA, pins::SCL, pins::I2C_FREQ);
  transport::initWire(pins::SDA, pins::SCL, pins::I2C_FREQ);
  
  i2c_scanner::scan(Wire);

  // Configure display with explicit threshold
  ssd1315::Config cfg;
  cfg.width = pins::OLED_WIDTH;
  cfg.height = pins::OLED_HEIGHT;
  cfg.i2cAddress = pins::OLED_I2C_ADDR;
  cfg.i2cWrite = transport::wireWrite;
  cfg.i2cUser = &Wire;
  cfg.pageBufferPages = 8;
  cfg.byteBudgetPerTick = 256;    // Faster flushes for stress testing
  cfg.contrast = 0x7F;
  cfg.offlineThreshold = OFFLINE_THRESHOLD;

  LOGI("Display config:");
  LOGI("  Dimensions:       %dx%d", cfg.width, cfg.height);
  LOGI("  I2C Address:      0x%02X", cfg.i2cAddress);
  LOGI("  Page Buffer:      %d pages", cfg.pageBufferPages);
  LOGI("  Byte Budget:      %d bytes/tick", cfg.byteBudgetPerTick);
  LOGI("  Offline Threshold: %d failures", cfg.offlineThreshold);
  LOGI("");

  // Capture health BEFORE begin
  LOGI("Health BEFORE begin():");
  LOGI("  state() = %s", diag::stateToString(display.state()));
  LOGI("  totalSuccess = %lu", (unsigned long)display.totalSuccess());
  LOGI("  totalFailures = %lu", (unsigned long)display.totalFailures());
  LOGI("");

  ssd1315::Status st = display.begin(cfg);
  
  LOGI("begin() returned: %s (code=%d)", 
       st.ok() ? "OK" : "ERROR", (int)st.code);
  
  if (!st.ok()) {
    LOGE("Display init failed: %s", st.msg);
    LOGE("Detail: %ld", (long)st.detail);
    LOGI("");
    LOGI("Health AFTER failed begin():");
    diag::printHealthVerbose(display);
    while (true) { delay(1000); }
  }

  LOGI("");
  LOGI("Health AFTER successful begin():");
  diag::printHealthVerbose(display);

  // Draw initial screen
  display.clear();
  display.drawText(8, 0, "Health Monitor");
  display.drawHLine(0, 9, 128);
  display.drawText(0, 16, "Type 'health' for");
  display.drawText(0, 26, "verbose diagnostics");
  display.drawText(0, 42, "Type 'help' for");
  display.drawText(0, 52, "all commands");
  display.requestFlush();
  display.waitFlush(millis());

  // Initialize health monitor (disabled by default)
  healthMonitor.begin(0);

  showHelp();
  LOGI("");
  LOGI("Ready for commands!");
}

void loop() {
  uint32_t now = millis();

  // Drive display state machine
  display.tick(now);

  // Run health monitor if enabled
  if (monitorEnabled) {
    healthMonitor.tick(display);
  }

  // Check for serial commands
  char cmdBuf[64];
  if (cmd::readLine(cmdBuf, sizeof(cmdBuf))) {
    LOGI("> %s", cmdBuf);

    int value;

    if (cmd::match(cmdBuf, "help")) {
      showHelp();

    } else if (cmd::match(cmdBuf, "health")) {
      diag::printHealthVerbose(display);

    } else if (cmd::match(cmdBuf, "brief")) {
      diag::printHealthOneLine(display);

    } else if (cmd::match(cmdBuf, "counters")) {
      printRawCounters();

    } else if (cmd::match(cmdBuf, "threshold")) {
      LOGI("Offline threshold: %u", OFFLINE_THRESHOLD);
      LOGI("Current consecutive failures: %u", display.consecutiveFailures());
      LOGI("State: %s", diag::stateToString(display.state()));

    } else if (cmd::match(cmdBuf, "probe")) {
      runProbe();

    } else if (cmd::match(cmdBuf, "recover")) {
      runRecover();

    } else if (cmd::match(cmdBuf, "scan")) {
      i2c_scanner::scan(Wire);

    } else if (cmd::parseInt(cmdBuf, "stress", &value)) {
      if (value > 0 && value <= 10000) {
        runContrastStress(value);
      } else {
        LOGE("Count must be 1-10000");
      }

    } else if (cmd::parseInt(cmdBuf, "flushstress", &value)) {
      if (value > 0 && value <= 1000) {
        runFlushStress(value);
      } else {
        LOGE("Count must be 1-1000");
      }

    } else if (cmd::parseInt(cmdBuf, "burst", &value)) {
      if (value > 0 && value <= 10000) {
        runBurstTest(value);
      } else {
        LOGE("Count must be 1-10000");
      }

    } else if (cmd::parseInt(cmdBuf, "monitor", &value)) {
      if (value > 0) {
        monitorEnabled = true;
        healthMonitor.begin(value);
        LOGI("Health monitor enabled, interval=%d ms", value);
      } else {
        monitorEnabled = false;
        LOGI("Health monitor disabled");
      }

    } else if (cmd::parseInt(cmdBuf, "contrast", &value)) {
      if (value >= 0 && value <= 255) {
        diag::HealthSnapshot before, after;
        before.capture(display);
        
        ssd1315::Status st = display.setContrast((uint8_t)value);
        
        after.capture(display);
        
        LOGI("setContrast(%d) result: %s", value, 
             st.ok() ? "OK" : diag::errToString(st.code));
        LOGI("Health changes:");
        diag::printHealthDiff(before, after);
      } else {
        LOGE("Contrast must be 0-255");
      }

    } else if (strncmp(cmdBuf, "text ", 5) == 0) {
      diag::HealthSnapshot before, after;
      before.capture(display);
      
      display.clear();
      display.drawText(0, 28, cmdBuf + 5);
      display.requestFlush();
      ssd1315::Status st = display.waitFlush(millis());
      
      after.capture(display);
      
      LOGI("Text+flush result: %s", st.ok() ? "OK" : diag::errToString(st.code));
      LOGI("Health changes:");
      diag::printHealthDiff(before, after);

    } else if (cmd::match(cmdBuf, "clear")) {
      diag::HealthSnapshot before, after;
      before.capture(display);
      
      display.clear();
      display.requestFlush();
      ssd1315::Status st = display.waitFlush(millis());
      
      after.capture(display);
      
      LOGI("Clear+flush result: %s", st.ok() ? "OK" : diag::errToString(st.code));
      LOGI("Health changes:");
      diag::printHealthDiff(before, after);

    } else if (cmd::match(cmdBuf, "reset")) {
      LOGI("Reinitializing display...");
      display.end();
      LOGI("After end():");
      diag::printHealthOneLine(display);
      
      ssd1315::Config cfg;
      cfg.width = pins::OLED_WIDTH;
      cfg.height = pins::OLED_HEIGHT;
      cfg.i2cAddress = pins::OLED_I2C_ADDR;
      cfg.i2cWrite = transport::wireWrite;
      cfg.i2cUser = &Wire;
      cfg.pageBufferPages = 8;
      cfg.byteBudgetPerTick = 256;
      cfg.contrast = 0x7F;
      cfg.offlineThreshold = OFFLINE_THRESHOLD;
      
      ssd1315::Status st = display.begin(cfg);
      LOGI("begin() result: %s", st.ok() ? "OK" : diag::errToString(st.code));
      LOGI("After begin():");
      diag::printHealthOneLine(display);

    } else {
      LOGE("Unknown command: %s", cmdBuf);
    }
  }

  delay(1);
}
