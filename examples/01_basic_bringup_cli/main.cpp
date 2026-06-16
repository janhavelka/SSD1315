/**
 * @file main.cpp
 * @brief Example 01: Unified Basic Bringup CLI
 *
 * This example demonstrates:
 * - Verbose health state monitoring
 * - Driver state transitions (UNINIT -> READY -> DEGRADED -> OFFLINE)
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
 *   recover        - Run software recover() (with tracking; no RES# toggle)
 *   stress [n]     - Run n rapid setContrast operations (default when omitted)
 *   flushstress [n]- Run n flush operations (default when omitted)
 *   burst [n]      - Burst n commands as fast as possible (default when omitted)
 *   simerr         - Simulate error (invalid contrast cmd)
 *   threshold <n>  - Show current threshold (read-only)
 *   counters       - Show raw counter values
 *   monitor [ms]   - Set health monitor interval (no arg = show current)
 *   contrast [v]   - Set contrast 1..255 (no arg = show current)
 *   invert [0|1]   - Set/get invert mode
 *   flipx [0|1]    - Set/get horizontal flip
 *   flipy [0|1]    - Set/get vertical flip
 *   display off/on - Display-off/on diagnostic alias
 *   sleep [0|1]    - Set/get sleep mode
 *   allon [0|1]    - Set/get all-pixels-on mode
 *   zoom [0|1]     - Set/get zoom mode
 *   fade ...       - Set/get fade/blink mode
 *   scrollh/...    - Hardware scroll commands (`scroll stop` stops motion)
 *   pattern ...    - Pattern fill commands
 *   line/rect/...  - Graphics primitive commands
 *   demo [n]       - Run n demo loops (default when omitted)
 *   featuretest    - Alias of selftest
 *   text <msg>     - Draw text and flush
 *   clear          - Clear display
 *   reset          - Software reinitialize display; does not toggle RES#
 *
 * Hardware: ESP32-S2 or ESP32-S3 with SSD1315 128x64 OLED
 */

#include <cstdlib>
#include <stdio.h>
#include <string.h>

#include <Arduino.h>

#include "ssd1315/SSD1315.h"
#include "ssd1315/Version.h"
#include "examples/common/BoardConfig.h"
#include "examples/common/BuildConfig.h"
#include "examples/common/CommandHandler.h"
#include "examples/common/I2cScanner.h"
#include "examples/common/I2cTransport.h"
#include "examples/common/CliStyle.h"
#include "examples/common/Log.h"
#include "examples/common/HealthDiag.h"

#define SSD1315_EXAMPLE_STRINGIFY_INNER(x) #x
#define SSD1315_EXAMPLE_STRINGIFY(x) SSD1315_EXAMPLE_STRINGIFY_INNER(x)

// Example configuration constants
static constexpr uint8_t OFFLINE_THRESHOLD = 5;  ///< Consecutive failures before OFFLINE state
static constexpr const char* HIL_PANEL_PROFILE = "example-default-128x64-internal-charge-pump";

#ifdef ARDUINO_BOARD
static constexpr const char* HIL_BUILD_TARGET = SSD1315_EXAMPLE_STRINGIFY(ARDUINO_BOARD);
#else
static constexpr const char* HIL_BUILD_TARGET = "unknown";
#endif

// Display instance
SSD1315::SSD1315 display;
static uint8_t displayFramebuffer[SSD1315::SSD1315::MAX_WIDTH *
                                  SSD1315::SSD1315::MAX_PAGES] = {};

// Health monitor for continuous tracking
diag::HealthMonitor healthMonitor;
bool monitorEnabled = false;
uint32_t monitorIntervalMs = 0;

// Stress test state
bool stressRunning = false;
uint32_t stressCount = 0;
uint32_t stressRemaining = 0;
uint32_t stressSuccessCount = 0;
uint32_t stressFailCount = 0;
uint32_t stressStartMs = 0;
bool verboseMode = false;
constexpr uint32_t DEFAULT_STRESS_COUNT = 10;
constexpr uint32_t DEFAULT_FLUSH_STRESS_COUNT = 10;
constexpr uint32_t DEFAULT_BURST_COUNT = 100;
constexpr uint32_t DEFAULT_DEMO_LOOPS = 3;
constexpr uint32_t DEMO_STEP_DELAY_MS = 350;

bool gInvertEnabled = false;
bool gFlipXEnabled = false;
bool gFlipYEnabled = false;
bool gSleepEnabled = false;
bool gAllPixelsOn = false;
bool gZoomEnabled = false;
bool gScrollActive = false;
SSD1315::FadeMode gFadeMode = SSD1315::FadeMode::OFF;
uint8_t gFadeInterval = 0;

static const uint8_t kDiagBitmap8x8[] = {
    0x81, 0x42, 0x24, 0x18,
    0x18, 0x24, 0x42, 0x81,
};

const char* fadeModeToString(SSD1315::FadeMode mode) {
  switch (mode) {
    case SSD1315::FadeMode::OFF:
      return "off";
    case SSD1315::FadeMode::FADE_OUT:
      return "fade";
    case SSD1315::FadeMode::BLINK:
      return "blink";
    default:
      return "unknown";
  }
}

const char* controllerProfileToString(SSD1315::ControllerProfile profile) {
  switch (profile) {
    case SSD1315::ControllerProfile::SSD1315:
      return "SSD1315";
    default:
      return "unknown";
  }
}

bool parseBoolValue(const char* token, bool& value) {
  if (!token || !token[0]) {
    return false;
  }
  if (strcasecmp(token, "1") == 0 || strcasecmp(token, "on") == 0 ||
      strcasecmp(token, "true") == 0 || strcasecmp(token, "yes") == 0) {
    value = true;
    return true;
  }
  if (strcasecmp(token, "0") == 0 || strcasecmp(token, "off") == 0 ||
      strcasecmp(token, "false") == 0 || strcasecmp(token, "no") == 0) {
    value = false;
    return true;
  }
  return false;
}

bool parseScrollSpeedArg(int raw, SSD1315::ScrollSpeed& speed) {
  if (raw < 0 || raw > 7) {
    return false;
  }
  speed = static_cast<SSD1315::ScrollSpeed>(raw);
  return true;
}

const char* goodIfZeroColor(uint32_t value) {
  return (value == 0U) ? LOG_COLOR_GREEN : LOG_COLOR_RED;
}

const char* goodIfNonZeroColor(uint32_t value) {
  return (value > 0U) ? LOG_COLOR_GREEN : LOG_COLOR_YELLOW;
}

const char* goodIfTrueColor(bool value) {
  return value ? LOG_COLOR_GREEN : LOG_COLOR_RED;
}

const char* onOffColor(bool enabled) {
  return enabled ? LOG_COLOR_GREEN : LOG_COLOR_RESET;
}

const char* skipCountColor(uint32_t value) {
  return (value > 0U) ? LOG_COLOR_YELLOW : LOG_COLOR_RESET;
}

const char* stateColor(SSD1315::DriverState state) {
  return diag::stateColor(state);
}

void printStatusResult(const char* op, const SSD1315::Status& st) {
  LOGI("%s: %s%s%s",
       op,
       LOG_COLOR_RESULT(st.ok()),
       st.ok() ? "OK" : diag::errToString(st.code),
       LOG_COLOR_RESET);
  if (!st.ok()) {
    LOGI("  detail=%ld msg=%s", static_cast<long>(st.detail), st.msg ? st.msg : "(null)");
  }
}

void configureDisplayConfig(SSD1315::Config& cfg) {
  cfg.width = pins::OLED_WIDTH;
  cfg.height = pins::OLED_HEIGHT;
  cfg.i2cAddress = pins::OLED_I2C_ADDR;
  cfg.i2cWrite = transport::wireWrite;
  cfg.i2cWriteRead = transport::wireWriteRead;
  cfg.i2cUser = transport::configUser();
  cfg.nowMs = transport::nowMs;
  cfg.cooperativeYield = transport::cooperativeYield;
  cfg.timeUser = transport::configUser();
  cfg.pageBufferPages = 8;
  cfg.externalBuffer = displayFramebuffer;
  cfg.byteBudgetPerTick = 256;    // Faster flushes for stress testing
  cfg.contrast = 0x7F;
  cfg.offlineThreshold = OFFLINE_THRESHOLD;

}

SSD1315::Status flushBlocking() {
  SSD1315::Status st = display.requestFlush();
  if (st.ok()) {
    st = display.waitFlush(millis());
  }
  return st;
}

void demoDelayMs(uint32_t delayMs) {
  const uint32_t startMs = millis();
  while ((millis() - startMs) < delayMs) {
    display.tick(millis());
    delay(1);
  }
}

void runFeatureDemo(uint32_t loops = DEFAULT_DEMO_LOOPS) {
  if (loops == 0) {
    loops = 1;
  }

  LOGI("Running feature demo loops=%lu...", static_cast<unsigned long>(loops));

  uint32_t passCount = 0;
  uint32_t failCount = 0;
  auto track = [&](const char* op, const SSD1315::Status& st) {
    printStatusResult(op, st);
    if (st.ok()) {
      ++passCount;
    } else {
      ++failCount;
    }
  };

  SSD1315::Status baseline = display.stopScroll();
  if (baseline.ok()) {
    gScrollActive = false;
  }
  track("demo.baseline_scroll_stop", baseline);
  if (!baseline.ok()) {
    return;
  }

  baseline = display.setAllPixelsOn(false);
  if (baseline.ok()) {
    gAllPixelsOn = false;
  }
  track("demo.baseline_display_ram", baseline);
  if (!baseline.ok()) {
    return;
  }

  baseline = display.setInvert(false);
  if (baseline.ok()) {
    gInvertEnabled = false;
  }
  track("demo.baseline_invert_off", baseline);
  if (!baseline.ok()) {
    return;
  }

  baseline = display.setSleep(false);
  if (baseline.ok()) {
    gSleepEnabled = false;
  }
  track("demo.baseline_display_on", baseline);
  if (!baseline.ok()) {
    return;
  }

  display.clear();
  baseline = flushBlocking();
  track("demo.baseline_clear", baseline);
  if (!baseline.ok()) {
    return;
  }

  for (uint32_t loopIndex = 0; loopIndex < loops; ++loopIndex) {
    LOGI("Demo cycle %lu/%lu", static_cast<unsigned long>(loopIndex + 1),
         static_cast<unsigned long>(loops));

    display.clear();
    display.drawRect(0, 0, 128, 64, true);
    display.drawLine(0, 0, 127, 63, true);
    display.drawLine(127, 0, 0, 63, true);
    display.drawCircle(64, 32, 16, true);
    display.drawText(32, 4, "GEOMETRY");
    track("demo.geometry", flushBlocking());
    demoDelayMs(DEMO_STEP_DELAY_MS);

    display.fillCheckerboard(static_cast<uint8_t>(4 + (loopIndex % 3)));
    track("demo.checker", flushBlocking());
    demoDelayMs(DEMO_STEP_DELAY_MS);

    display.fillVerticalStripes(static_cast<uint8_t>(2 + (loopIndex % 4)));
    track("demo.vstripes", flushBlocking());
    demoDelayMs(DEMO_STEP_DELAY_MS);

    display.fillHorizontalStripes(static_cast<uint8_t>(2 + (loopIndex % 4)));
    track("demo.hstripes", flushBlocking());
    demoDelayMs(DEMO_STEP_DELAY_MS);

    display.clear();
    display.drawBitmap(60, 20, kDiagBitmap8x8, 8, 8, true);
    display.drawText(8, 4, "TEXT+BITMAP");
    display.drawText(0, 52, "SSD1315 demo loop");
    track("demo.text_bitmap", flushBlocking());
    demoDelayMs(DEMO_STEP_DELAY_MS);

    SSD1315::Status st = display.startHorizontalScroll(
        false, 0, 7, SSD1315::ScrollSpeed::FRAMES_5);
    track("demo.scroll_start", st);
    demoDelayMs(800);
    st = display.stopScroll();
    track("demo.scroll_stop", st);
    demoDelayMs(200);
  }

  LOGI("Demo summary: pass=%s%lu%s fail=%s%lu%s",
       goodIfNonZeroColor(passCount),
       static_cast<unsigned long>(passCount),
       LOG_COLOR_RESET,
       goodIfZeroColor(failCount),
       static_cast<unsigned long>(failCount),
       LOG_COLOR_RESET);
}

void showHelp() {
  Serial.println();
  cli::printHelpHeader("SSD1315 CLI Help");

  cli::printHelpSection("Common");
  cli::printHelpItem("help / ?", "Show this help");
  cli::printHelpItem("version / ver", "Print firmware and library version info");
  cli::printHelpItem("scan", "Scan I2C bus");
  cli::printHelpItem("probe", "ACK-only address check; not controller identity");
  cli::printHelpItem("recover", "Software reinit/resync; does not toggle RES#");
  cli::printHelpItem("drv", "Driver health diagnostics");
  cli::printHelpItem("read", "Health one-line summary");
  cli::printHelpItem("cfg / settings", "Print active config");
  cli::printHelpItem("verbose [0|1]", "Toggle verbose command output");
  cli::printHelpItem("stress [N]", "N rapid setContrast() calls");
  cli::printHelpItem("stress_mix [N]", "N mixed display operations");
  cli::printHelpItem("selftest", "Safe command self-test report");
  cli::printHelpItem("featuretest", "Alias of selftest");

  cli::printHelpSection("Display Controls");
  cli::printHelpItem("contrast [1-255]", "Set/get contrast");
  cli::printHelpItem("bright [1-255]", "Set/get brightness (alias to contrast)");
  cli::printHelpItem("invert [0|1]", "Set/get invert");
  cli::printHelpItem("flipx [0|1]", "Set/get horizontal flip");
  cli::printHelpItem("flipy [0|1]", "Set/get vertical flip");
  cli::printHelpItem("display <off|on>", "Display-off/on diagnostic alias");
  cli::printHelpItem("sleep [0|1]", "Set/get display sleep");
  cli::printHelpItem("allon [0|1]", "Set/get all-pixels-on mode");
  cli::printHelpItem("zoom [0|1]", "Set/get zoom");
  cli::printHelpItem("fade [off|fade|blink] [interval]", "Set/get fade mode");
  cli::printHelpItem("scrollh <left|right> <startPage> <endPage> [speed]", "Horizontal hardware scroll");
  cli::printHelpItem("scrollv <left|right> <startPage> <endPage> <offset> [speed]", "Vertical hardware scroll");
  cli::printHelpItem("scroll stop / scrollstop", "Stop hardware scrolling");
  cli::printHelpItem("scrollarea <topRows> <scrollRows>", "Set vertical scroll area");

  cli::printHelpSection("Graphics");
  cli::printHelpItem("text <msg>", "Draw text and flush");
  cli::printHelpItem("clear", "Clear display and flush");
  cli::printHelpItem("fill", "Fill display and flush");
  cli::printHelpItem("pattern <checker|vstripes|hstripes> [size]", "Pattern fill");
  cli::printHelpItem("line <x0> <y0> <x1> <y1>", "Draw line");
  cli::printHelpItem("vline <x> <y> <h>", "Draw vertical line");
  cli::printHelpItem("rect <x> <y> <w> <h>", "Draw rectangle");
  cli::printHelpItem("fillrect <x> <y> <w> <h>", "Fill rectangle");
  cli::printHelpItem("circle <x> <y> <r>", "Draw circle");
  cli::printHelpItem("fillcircle <x> <y> <r>", "Fill circle");
  cli::printHelpItem("char <x> <y> <c>", "Draw single character");
  cli::printHelpItem("bitmap <x> <y>", "Draw built-in 8x8 test bitmap");
  cli::printHelpItem("pixel <x> <y> [0|1]", "Get/set framebuffer pixel");
  cli::printHelpItem("textw <msg>", "Measure text width in pixels");
  cli::printHelpItem("flush", "Request and wait for flush");
  cli::printHelpItem("flushrect <x> <y> <w> <h>", "Flush selected rectangle");
  cli::printHelpItem("demo [N]", "Run N feature-demo loops (default when omitted)");

  cli::printHelpSection("Diagnostics");
  cli::printHelpItem("health", "Verbose health diagnostics");
  cli::printHelpItem("brief", "One-line health summary");
  cli::printHelpItem("counters", "Raw health counters");
  cli::printHelpItem("threshold", "Offline threshold info");
  cli::printHelpItem("bufsize", "Print buffer/page iteration info");
  cli::printHelpItem("statex", "Print init/sleep/flush/iteration/dirty flags");
  cli::printHelpItem("buffer [N]", "Dump first N bytes of framebuffer");
  cli::printHelpItem("dirty", "Show dirty flag");
  cli::printHelpItem("dirty clear", "clearDirty()");
  cli::printHelpItem("dirty all", "markAllDirty()");
  cli::printHelpItem("dirty mark <p> <min> <max>", "markDirty(page,min,max)");
  cli::printHelpItem("touch", "touch() activity timestamp");
  cli::printHelpItem("clearerr", "clearError()");
  cli::printHelpItem("userpages [n]", "Set/get user page count");
  cli::printHelpItem("activepage [idx]", "Set/get active user page");
  cli::printHelpItem("pagecycle [ms]", "Set/get page cycle interval");
  cli::printHelpItem("autosleep [ms]", "Set/get auto-sleep timeout");
  cli::printHelpItem("pageiter [N]", "Exercise firstPage/nextPage iteration");
  cli::printHelpItem("cmd <b0>", "sendCommand");
  cli::printHelpItem("cmd2 <b0> <b1>", "sendCommand2");
  cli::printHelpItem("cmd3 <b0> <b1> <b2>", "sendCommand3");
  cli::printHelpItem("cmdlist <b0> ...", "sendCommandList");
  cli::printHelpItem("monitor [ms]", "Periodic health monitor");
  cli::printHelpItem("flushstress [N]", "N sequential flush operations");
  cli::printHelpItem("burst [N]", "N commands as fast as possible");
  cli::printHelpItem("reset", "Software reinitialize; does not toggle RES#");

  Serial.println();
  Serial.println("Safety: probe is ACK-only. Raw cmd*, scroll, allon, fill, high contrast,");
  Serial.println("and stress/demo commands can alter panel state or leave static OLED content.");
}

void printVersionInfo() {
  const SSD1315::SettingsSnapshot s = display.getSettings();
  LOGI("=== Version Info ===");
  LOGI("  Framework: Arduino");
  LOGI("  Build target: %s", HIL_BUILD_TARGET);
  LOGI("  Example firmware build: %s %s", __DATE__, __TIME__);
  LOGI("  SSD1315 library version: %s", SSD1315::VERSION);
  LOGI("  SSD1315 library full: %s", SSD1315::VERSION_FULL);
  LOGI("  SSD1315 library build: %s", SSD1315::BUILD_TIMESTAMP);
  LOGI("  SSD1315 library commit: %s (%s)", SSD1315::GIT_COMMIT, SSD1315::GIT_STATUS);
  LOGI("  Controller profile: %s", controllerProfileToString(s.controllerProfile));
  LOGI("  Panel profile: %s", HIL_PANEL_PROFILE);
  LOGI("  Active I2C address: 0x%02X", s.i2cAddress);
  LOGI("  Geometry: %ux%u pages=%u pageBufferPages=%u",
       static_cast<unsigned>(s.width),
       static_cast<unsigned>(s.height),
       static_cast<unsigned>(s.totalPages),
       static_cast<unsigned>(s.pageBufferPages));
}

/**
 * @brief Print raw counter values without formatting.
 */
void printRawCounters() {
  LOGI("Raw Health Counters:");
  LOGI("  state()              = %d (%s%s%s)",
       static_cast<int>(display.state()),
       stateColor(display.state()),
       diag::stateToString(display.state()),
       LOG_COLOR_RESET);
  LOGI("  isOnline()           = %s%s%s",
       goodIfTrueColor(display.isOnline()),
       display.isOnline() ? "true" : "false",
       LOG_COLOR_RESET);
  LOGI("  consecutiveFailures()= %s%u%s",
       goodIfZeroColor(display.consecutiveFailures()),
       display.consecutiveFailures(),
       LOG_COLOR_RESET);
  LOGI("  totalSuccess()       = %s%lu%s",
       goodIfNonZeroColor(display.totalSuccess()),
       static_cast<unsigned long>(display.totalSuccess()),
       LOG_COLOR_RESET);
  LOGI("  totalFailures()      = %s%lu%s",
       goodIfZeroColor(display.totalFailures()),
       static_cast<unsigned long>(display.totalFailures()),
       LOG_COLOR_RESET);
  LOGI("  lastOkMs()           = %lu", (unsigned long)display.lastOkMs());
  LOGI("  lastErrorMs()        = %lu", (unsigned long)display.lastErrorMs());
  
  SSD1315::Status err = display.lastError();
  LOGI("  lastError().code     = %d (%s%s%s)",
       static_cast<int>(err.code),
       LOG_COLOR_RESULT(err.ok()),
       diag::errToString(err.code),
       LOG_COLOR_RESET);
  LOGI("  lastError().detail   = %ld", (long)err.detail);
  LOGI("  lastError().msg      = \"%s\"", err.msg ? err.msg : "(null)");
}

/**
 * @brief Run probe() and show result with context.
 */
void runProbe() {
  LOGI("=== Probe Operation ===");
  LOGI("Note: probe() does not update health counters/state.");
  
  // Snapshot before
  diag::HealthSnapshot before;
  before.capture(display);
  
  LOGI("Before probe:");
  diag::printHealthOneLine(display);
  
  // Run probe
  SSD1315::Status st = display.probe();
  
  LOGI("Probe result: %s%s%s (code=%d: %s)",
       LOG_COLOR_RESULT(st.ok()),
       st.ok() ? "OK" : "FAILED",
       LOG_COLOR_RESET,
       static_cast<int>(st.code),
       diag::errToString(st.code));
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
  LOGI("=== Recover Operation ===");
  LOGI("Note: recover() updates health counters/state.");
  
  // Snapshot before
  diag::HealthSnapshot before;
  before.capture(display);
  
  LOGI("Before recover:");
  diag::printHealthOneLine(display);
  
  // Run recover
  SSD1315::Status st = display.recover();
  if (st.ok()) {
    gScrollActive = false;
  }
  
  LOGI("Recover result: %s%s%s (code=%d: %s)",
       LOG_COLOR_RESULT(st.ok()),
       st.ok() ? "OK" : "FAILED",
       LOG_COLOR_RESET,
       static_cast<int>(st.code),
       diag::errToString(st.code));
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
uint8_t validationContrastFromIndex(uint32_t index) {
  return static_cast<uint8_t>((index % 255U) + 1U);
}

void runContrastStress(uint32_t count) {
  LOGI("=== Contrast Stress Test ===");
  LOGI("Running %lu setContrast() calls", static_cast<unsigned long>(count));
  LOGI("Each call produces one tracked health event");
  
  diag::HealthSnapshot before;
  before.capture(display);
  
  uint32_t successCount = 0;
  uint32_t failCount = 0;
  uint32_t startMs = millis();
  bool hasFailure = false;
  SSD1315::Status firstFailure = SSD1315::Ok();
  SSD1315::Status lastFailure = SSD1315::Ok();
  
  for (uint32_t i = 0; i < count; i++) {
    uint8_t contrast = validationContrastFromIndex(i);
    SSD1315::Status st = display.setContrast(contrast);
    if (st.ok()) {
      successCount++;
    } else {
      failCount++;
      if (!hasFailure) {
        firstFailure = st;
        hasFailure = true;
      }
      lastFailure = st;
      LOGD("  Iteration %lu failed: %s", (unsigned long)i, diag::errToString(st.code));
    }
    
    // Log progress every 100 iterations
    if ((i + 1) % 100 == 0) {
      LOGI("  Progress: %lu/%lu (OK:%s%lu%s, FAIL:%s%lu%s)",
           (unsigned long)(i + 1), (unsigned long)count,
           goodIfNonZeroColor(successCount), (unsigned long)successCount, LOG_COLOR_RESET,
           goodIfZeroColor(failCount), (unsigned long)failCount, LOG_COLOR_RESET);
    }
  }
  
  uint32_t elapsed = millis() - startMs;
  
  diag::HealthSnapshot after;
  after.capture(display);
  
  LOGI("Results:");
  LOGI("  Total iterations: %lu", static_cast<unsigned long>(count));
  LOGI("  Successes: %s%lu%s",
       goodIfNonZeroColor(successCount), static_cast<unsigned long>(successCount), LOG_COLOR_RESET);
  LOGI("  Failures: %s%lu%s",
       goodIfZeroColor(failCount), static_cast<unsigned long>(failCount), LOG_COLOR_RESET);
  LOGI("  Time elapsed: %lu ms", static_cast<unsigned long>(elapsed));
  if (elapsed > 0) {
    LOGI("  Rate: %lu ops/sec", static_cast<unsigned long>(count * 1000 / elapsed));
  }
  if (hasFailure) {
    LOGI("First failure: code=%s detail=%ld msg=%s",
         diag::errToString(firstFailure.code),
         static_cast<long>(firstFailure.detail),
         firstFailure.msg ? firstFailure.msg : "");
    if (failCount > 1) {
      LOGI("Last failure: code=%s detail=%ld msg=%s",
           diag::errToString(lastFailure.code),
           static_cast<long>(lastFailure.detail),
           lastFailure.msg ? lastFailure.msg : "");
    }
  }
  
  LOGI("Health changes:");
  diag::printHealthDiff(before, after);
}

/**
 * @brief Run flush stress test.
 */
void runFlushStress(uint32_t count) {
  LOGI("=== Flush Stress Test ===");
  LOGI("Running %lu flush operations", static_cast<unsigned long>(count));
  LOGI("Each flush produces one tracked health event");
  
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
    SSD1315::Status st = display.waitFlush(millis());
    
    if (st.ok()) {
      successCount++;
    } else {
      failCount++;
      LOGD("  Flush %lu failed: %s", (unsigned long)(i + 1), diag::errToString(st.code));
    }
    
    // Log progress every 10 flushes
    if ((i + 1) % 10 == 0) {
      LOGI("  Progress: %lu/%lu (OK:%s%lu%s, FAIL:%s%lu%s)",
           (unsigned long)(i + 1), (unsigned long)count,
           goodIfNonZeroColor(successCount), (unsigned long)successCount, LOG_COLOR_RESET,
           goodIfZeroColor(failCount), (unsigned long)failCount, LOG_COLOR_RESET);
    }
  }
  
  uint32_t elapsed = millis() - startMs;
  
  diag::HealthSnapshot after;
  after.capture(display);
  
  LOGI("Results:");
  LOGI("  Total flushes: %lu", static_cast<unsigned long>(count));
  LOGI("  Successes: %s%lu%s",
       goodIfNonZeroColor(successCount), static_cast<unsigned long>(successCount), LOG_COLOR_RESET);
  LOGI("  Failures: %s%lu%s",
       goodIfZeroColor(failCount), static_cast<unsigned long>(failCount), LOG_COLOR_RESET);
  LOGI("  Time elapsed: %lu ms", static_cast<unsigned long>(elapsed));
  if (elapsed > 0) {
    LOGI("  Rate: %lu flushes/sec", static_cast<unsigned long>(count * 1000 / elapsed));
    LOGI("  Avg per flush: %lu ms", static_cast<unsigned long>(elapsed / count));
  }
  
  LOGI("Health changes:");
  diag::printHealthDiff(before, after);
  
  LOGI("Expected health counter change: %s+%lu successes%s, %s+%lu failures%s",
       goodIfNonZeroColor(successCount), (unsigned long)successCount, LOG_COLOR_RESET,
       goodIfZeroColor(failCount), (unsigned long)failCount, LOG_COLOR_RESET);
  LOGI("Actual change: %s+%lu successes%s, %s+%lu failures%s",
       goodIfNonZeroColor(after.totalSuccess - before.totalSuccess),
       (unsigned long)(after.totalSuccess - before.totalSuccess),
       LOG_COLOR_RESET,
       goodIfZeroColor(after.totalFailures - before.totalFailures),
       (unsigned long)(after.totalFailures - before.totalFailures),
       LOG_COLOR_RESET);
}

/**
 * @brief Run burst command test (as fast as possible).
 */
void runBurstTest(uint32_t count) {
  LOGI("=== Burst Command Test ===");
  LOGI("Sending %lu commands as fast as possible",
       static_cast<unsigned long>(count));
  
  diag::HealthSnapshot before;
  before.capture(display);
  
  uint32_t successCount = 0;
  uint32_t failCount = 0;
  uint32_t startMs = millis();
  
  // Mix of different commands
  for (uint32_t i = 0; i < count; i++) {
    SSD1315::Status st;
    switch (i % 4) {
      case 0:
        st = display.setContrast(validationContrastFromIndex(i));
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
  
  LOGI("Results:");
  LOGI("  Total commands: %lu", static_cast<unsigned long>(count));
  LOGI("  Successes: %s%lu%s",
       goodIfNonZeroColor(successCount), static_cast<unsigned long>(successCount), LOG_COLOR_RESET);
  LOGI("  Failures: %s%lu%s",
       goodIfZeroColor(failCount), static_cast<unsigned long>(failCount), LOG_COLOR_RESET);
  LOGI("  Time elapsed: %lu ms", static_cast<unsigned long>(elapsed));
  if (elapsed > 0) {
    LOGI("  Rate: %lu cmds/sec", static_cast<unsigned long>(count * 1000 / elapsed));
  }
  
  LOGI("Health changes:");
  diag::printHealthDiff(before, after);
}

/**
 * @brief Run mixed-operation stress test (closest equivalent to RV3032 stress_mix).
 */
void runStressMix(uint32_t count) {
  LOGI("=== Stress Mix Test ===");
  LOGI("Running %lu mixed operations", static_cast<unsigned long>(count));

  struct OpStats {
    const char* name;
    uint32_t ok;
    uint32_t fail;
  };
  OpStats ops[] = {
      {"setContrast", 0, 0},
      {"setInvert", 0, 0},
      {"text+flush", 0, 0},
      {"setSleep(false)", 0, 0},
  };
  const uint32_t opCount = sizeof(ops) / sizeof(ops[0]);

  diag::HealthSnapshot before;
  before.capture(display);
  const uint32_t startMs = millis();

  for (uint32_t i = 0; i < count; ++i) {
    const uint32_t op = i % opCount;
    SSD1315::Status st = SSD1315::Ok();
    switch (op) {
      case 0:
        st = display.setContrast(validationContrastFromIndex(i));
        break;
      case 1:
        st = display.setInvert((i & 1U) != 0U);
        break;
      case 2: {
        display.clear();
        char line[24];
        snprintf(line, sizeof(line), "mix #%lu", (unsigned long)(i + 1));
        display.drawText(0, 28, line);
        st = display.requestFlush();
        if (st.ok()) {
          st = display.waitFlush(millis());
        }
        break;
      }
      case 3:
        st = display.setSleep(false);
        break;
      default:
        break;
    }

    if (st.ok()) {
      ops[op].ok++;
    } else {
      ops[op].fail++;
      if (verboseMode) {
        LOGI("  [%lu] %s failed: %s", (unsigned long)i, ops[op].name, diag::errToString(st.code));
      }
    }
  }

  const uint32_t elapsed = millis() - startMs;
  diag::HealthSnapshot after;
  after.capture(display);

  uint32_t totalOk = 0;
  uint32_t totalFail = 0;
  for (uint32_t i = 0; i < opCount; ++i) {
    totalOk += ops[i].ok;
    totalFail += ops[i].fail;
  }

  LOGI("Results:");
  LOGI("  Total ops: %lu", static_cast<unsigned long>(count));
  LOGI("  Successes: %s%lu%s",
       goodIfNonZeroColor(totalOk), static_cast<unsigned long>(totalOk), LOG_COLOR_RESET);
  LOGI("  Failures: %s%lu%s",
       goodIfZeroColor(totalFail), static_cast<unsigned long>(totalFail), LOG_COLOR_RESET);
  LOGI("  Time elapsed: %lu ms", static_cast<unsigned long>(elapsed));
  if (elapsed > 0) {
    LOGI("  Rate: %lu ops/sec", static_cast<unsigned long>(count * 1000 / elapsed));
  }
  for (uint32_t i = 0; i < opCount; ++i) {
    LOGI("  %-14s %sok=%lu%s %sfail=%lu%s",
         ops[i].name,
         goodIfNonZeroColor(ops[i].ok), (unsigned long)ops[i].ok, LOG_COLOR_RESET,
         goodIfZeroColor(ops[i].fail), (unsigned long)ops[i].fail, LOG_COLOR_RESET);
  }
  LOGI("Health changes:");
  diag::printHealthDiff(before, after);
}

/**
 * @brief Run safe command self-test and print PASS/FAIL report.
 */
void runSelfTest() {
  struct SelfTestStats {
    uint32_t pass = 0;
    uint32_t fail = 0;
    uint32_t skip = 0;
  } stats;

  enum class SelftestOutcome : uint8_t { PASS, FAIL, SKIP };
  auto report = [&](const char* name, SelftestOutcome outcome, const char* note) {
    const bool ok = (outcome == SelftestOutcome::PASS);
    const bool skip = (outcome == SelftestOutcome::SKIP);
    const char* color = skip ? LOG_COLOR_YELLOW : LOG_COLOR_RESULT(ok);
    const char* tag = skip ? "SKIP" : (ok ? "PASS" : "FAIL");
    LOGI("  [%s%s%s] %s%s%s",
         color,
         tag,
         LOG_COLOR_RESET,
         name,
         (note && note[0]) ? " - " : "",
         (note && note[0]) ? note : "");
    if (skip) {
      stats.skip++;
    } else if (ok) {
      stats.pass++;
    } else {
      stats.fail++;
    }
  };
  auto reportCheck = [&](const char* name, bool ok, const char* note) {
    report(name, ok ? SelftestOutcome::PASS : SelftestOutcome::FAIL, note);
  };
  auto reportSkip = [&](const char* name, const char* note) {
    report(name, SelftestOutcome::SKIP, note);
  };

  LOGI("");
  LOGI("=== SSD1315 selftest (safe commands) ===");

  const uint32_t succBefore = display.totalSuccess();
  const uint32_t failBefore = display.totalFailures();
  const uint8_t consBefore = display.consecutiveFailures();

  SSD1315::Status st = display.probe();
  if (st.code == SSD1315::Err::NOT_INITIALIZED) {
    reportSkip("probe responds", "driver not initialized");
    reportSkip("remaining checks", "selftest aborted");
    LOGI("Selftest result: pass=%s%lu%s fail=%s%lu%s skip=%s%lu%s",
         goodIfNonZeroColor(stats.pass), (unsigned long)stats.pass, LOG_COLOR_RESET,
         goodIfZeroColor(stats.fail), (unsigned long)stats.fail, LOG_COLOR_RESET,
         skipCountColor(stats.skip), (unsigned long)stats.skip, LOG_COLOR_RESET);
    return;
  }
  reportCheck("probe responds", st.ok(), st.ok() ? "" : diag::errToString(st.code));
  const bool probeNoTrack = display.totalSuccess() == succBefore &&
                            display.totalFailures() == failBefore &&
                            display.consecutiveFailures() == consBefore;
  reportCheck("probe no-health-side-effects", probeNoTrack, "");

  st = display.setContrast(0x80);
  reportCheck("setContrast", st.ok(), st.ok() ? "" : diag::errToString(st.code));

  st = display.setInvert(true);
  reportCheck("setInvert(true)", st.ok(), st.ok() ? "" : diag::errToString(st.code));
  st = display.setInvert(false);
  reportCheck("setInvert(false)", st.ok(), st.ok() ? "" : diag::errToString(st.code));
  if (st.ok()) {
    gInvertEnabled = false;
  }

  st = display.setFlipX(false);
  reportCheck("setFlipX(false)", st.ok(), st.ok() ? "" : diag::errToString(st.code));
  if (st.ok()) {
    gFlipXEnabled = false;
  }
  st = display.setFlipY(false);
  reportCheck("setFlipY(false)", st.ok(), st.ok() ? "" : diag::errToString(st.code));
  if (st.ok()) {
    gFlipYEnabled = false;
  }

  st = display.setSleep(false);
  reportCheck("setSleep(false)", st.ok(), st.ok() ? "" : diag::errToString(st.code));
  if (st.ok()) {
    gSleepEnabled = false;
  }

  st = display.setAllPixelsOn(false);
  reportCheck("setAllPixelsOn(false)", st.ok(), st.ok() ? "" : diag::errToString(st.code));
  if (st.ok()) {
    gAllPixelsOn = false;
  }

  st = display.setZoom(false);
  reportCheck("setZoom(false)", st.ok(), st.ok() ? "" : diag::errToString(st.code));
  if (st.ok()) {
    gZoomEnabled = false;
  }

  st = display.setFadeMode(SSD1315::FadeMode::OFF, 0);
  reportCheck("setFadeMode(off)", st.ok(), st.ok() ? "" : diag::errToString(st.code));
  if (st.ok()) {
    gFadeMode = SSD1315::FadeMode::OFF;
    gFadeInterval = 0;
  }

  st = display.setVerticalScrollArea(0, pins::OLED_HEIGHT);
  reportCheck("setVerticalScrollArea", st.ok(), st.ok() ? "" : diag::errToString(st.code));

  st = display.startHorizontalScroll(false, 0, 7, SSD1315::ScrollSpeed::FRAMES_5);
  reportCheck("startHorizontalScroll", st.ok(), st.ok() ? "" : diag::errToString(st.code));
  if (st.ok()) {
    delay(20);
    st = display.stopScroll();
    reportCheck("stopScroll(h)", st.ok(), st.ok() ? "" : diag::errToString(st.code));
  }

  st = display.startVerticalScroll(true, 0, 7, SSD1315::ScrollSpeed::FRAMES_5, 1);
  reportCheck("startVerticalScroll", st.ok(), st.ok() ? "" : diag::errToString(st.code));
  if (st.ok()) {
    delay(20);
    st = display.stopScroll();
    reportCheck("stopScroll(v)", st.ok(), st.ok() ? "" : diag::errToString(st.code));
  }

  display.clear();
  display.drawRect(0, 0, 128, 64, true);
  display.drawLine(0, 0, 127, 63, true);
  display.drawCircle(64, 32, 15, true);
  display.drawText(0, 54, "selftest");
  st = flushBlocking();
  reportCheck("draw+flush", st.ok(), st.ok() ? "" : diag::errToString(st.code));

  st = display.requestFlushRect(0, 0, 32, 16);
  if (st.ok()) {
    st = display.waitFlush(millis());
  }
  reportCheck("flushRect", st.ok(), st.ok() ? "" : diag::errToString(st.code));

  st = display.recover();
  if (st.ok()) {
    gScrollActive = false;
  }
  reportCheck("recover", st.ok(), st.ok() ? "" : diag::errToString(st.code));
  reportCheck("isOnline", display.isOnline(), "");

  LOGI("Selftest result: pass=%s%lu%s fail=%s%lu%s skip=%s%lu%s",
       goodIfNonZeroColor(stats.pass), (unsigned long)stats.pass, LOG_COLOR_RESET,
       goodIfZeroColor(stats.fail), (unsigned long)stats.fail, LOG_COLOR_RESET,
       skipCountColor(stats.skip), (unsigned long)stats.skip, LOG_COLOR_RESET);
}

void setup() {
  log_begin(115200);
  delay(100);

  LOGI("");
  LOGI("=== SSD1315 01_basic_bringup_cli ===");

  // Initialize I2C
  LOGI("Initializing I2C on SDA=%d, SCL=%d @ %lu Hz",
       pins::SDA, pins::SCL, static_cast<unsigned long>(pins::I2C_FREQ));
  if (!transport::initWire(pins::SDA, pins::SCL, pins::I2C_FREQ, 50U,
                           pins::OLED_I2C_ADDR)) {
    LOGE("I2C init failed");
    while (true) { delay(1000); }
  }
  
  i2c_scanner::scanDefault();

  // Configure display with explicit threshold
  SSD1315::Config cfg;
  configureDisplayConfig(cfg);

  LOGI("Display config:");
  LOGI("  Dimensions:       %dx%d", cfg.width, cfg.height);
  LOGI("  I2C Address:      0x%02X", cfg.i2cAddress);
  LOGI("  Page Buffer:      %d pages", cfg.pageBufferPages);
  LOGI("  Byte Budget:      %d bytes/tick", cfg.byteBudgetPerTick);
  LOGI("  Offline Threshold: %d failures", cfg.offlineThreshold);
  LOGI("");

  // Capture health BEFORE begin
  LOGI("Health BEFORE begin():");
  LOGI("  state() = %s%s%s",
       stateColor(display.state()),
       diag::stateToString(display.state()),
       LOG_COLOR_RESET);
  LOGI("  totalSuccess = %lu", (unsigned long)display.totalSuccess());
  LOGI("  totalFailures = %lu", (unsigned long)display.totalFailures());
  LOGI("");

  SSD1315::Status st = display.begin(cfg);
  
  LOGI("begin() returned: %s%s%s (code=%d)",
       LOG_COLOR_RESULT(st.ok()),
       st.ok() ? "OK" : "ERROR",
       LOG_COLOR_RESET,
       static_cast<int>(st.code));
  
  if (!st.ok()) {
    LOGE("Display init failed: %s", st.msg);
    LOGE("Detail: %ld", (long)st.detail);
    LOGI("");
    LOGI("Health AFTER failed begin():");
    diag::printHealthVerbose(display);
    while (true) { delay(1000); }
  }

  gInvertEnabled = false;
  gFlipXEnabled = false;
  gFlipYEnabled = false;
  gSleepEnabled = false;
  gAllPixelsOn = false;
  gZoomEnabled = false;
  gScrollActive = false;
  gFadeMode = SSD1315::FadeMode::OFF;
  gFadeInterval = 0;

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

    } else if (cmd::match(cmdBuf, "version") || cmd::match(cmdBuf, "ver")) {
      printVersionInfo();

    } else if (cmd::match(cmdBuf, "drv")) {
      diag::printHealthVerbose(display);

    } else if (cmd::match(cmdBuf, "read")) {
      diag::printHealthOneLine(display);
      if (verboseMode) {
        printRawCounters();
      }

    } else if (cmd::match(cmdBuf, "cfg") || cmd::match(cmdBuf, "settings")) {
      const SSD1315::Config& cfg = display.getConfig();
      const SSD1315::SettingsSnapshot settings = display.getSettings();
      LOGI("Config:");
      LOGI("  width=%u height=%u addr=0x%02X", cfg.width, cfg.height, cfg.i2cAddress);
      LOGI("  controllerProfile=%s panelProfile=%s",
           controllerProfileToString(settings.controllerProfile),
           HIL_PANEL_PROFILE);
      LOGI("  pageBufferPages=%u byteBudgetPerTick=%u", cfg.pageBufferPages, cfg.byteBudgetPerTick);
      LOGI("  comPins=0x%02X chargePump=0x%02X iref=0x%02X vcomh=0x%02X",
           static_cast<unsigned>(static_cast<uint8_t>(cfg.comPins)),
           static_cast<unsigned>(static_cast<uint8_t>(cfg.chargePumpVoltage)),
           static_cast<unsigned>(static_cast<uint8_t>(cfg.iref)),
           static_cast<unsigned>(static_cast<uint8_t>(cfg.vcomh)));
      LOGI("  flushTimeoutMs=%lu i2cTimeoutMs=%lu offlineThreshold=%u",
           static_cast<unsigned long>(cfg.flushTimeoutMs),
           static_cast<unsigned long>(cfg.i2cTimeoutMs),
           static_cast<unsigned>(cfg.offlineThreshold));
      LOGI("  userPageCount=%u activeUserPage=%u pageCycleMs=%lu inactivitySleepMs=%lu",
           static_cast<unsigned>(display.getUserPageCount()),
           static_cast<unsigned>(display.getActiveUserPage()),
           static_cast<unsigned long>(cfg.pageCycleMs),
           static_cast<unsigned long>(cfg.inactivitySleepMs));
      LOGI("  clearOnBegin=%s clearOnRecover=%s scrollActive=%s",
           log_bool_str(cfg.clearOnBegin),
           log_bool_str(cfg.clearOnRecover),
           log_bool_str(gScrollActive));
      LOGI("  pageBufferMode=%s%s%s currentPage=%u totalPages=%u bufferSize=%u",
           onOffColor(display.isPageBufferMode()),
           display.isPageBufferMode() ? "true" : "false",
           LOG_COLOR_RESET,
           static_cast<unsigned>(display.currentPageIndex()),
           static_cast<unsigned>(display.totalPages()),
           static_cast<unsigned>(display.getBufferSize()));
      LOGI("  externalBuffer=%s ownsBuffer=%s",
           log_bool_str(settings.hasExternalBuffer),
           log_bool_str(settings.ownsBuffer));
      LOGI("  initialized=%s%s%s sleeping=%s%s%s flushing=%s%s%s pageIterating=%s%s%s dirty=%s%s%s controlDirty=%s%s%s",
           goodIfTrueColor(display.isInitialized()),
           log_bool_str(display.isInitialized()),
           LOG_COLOR_RESET,
           onOffColor(display.isSleeping()),
           log_bool_str(display.isSleeping()),
           LOG_COLOR_RESET,
           onOffColor(display.isFlushing()),
           log_bool_str(display.isFlushing()),
           LOG_COLOR_RESET,
           onOffColor(display.isPageIterating()),
           log_bool_str(display.isPageIterating()),
           LOG_COLOR_RESET,
           onOffColor(display.isDirty()),
           log_bool_str(display.isDirty()),
           LOG_COLOR_RESET,
           onOffColor(display.controlStateDirty()),
           log_bool_str(display.controlStateDirty()),
           LOG_COLOR_RESET);
      if (display.controlStateDirty()) {
        const SSD1315::Status controlErr = display.controlStateError();
        LOGI("  controlStateError=%s detail=%ld msg=%s",
             diag::errToString(controlErr.code),
             static_cast<long>(controlErr.detail),
             controlErr.msg ? controlErr.msg : "");
      }
      LOGI("  invert=%s%s%s flipX=%s%s%s flipY=%s%s%s sleep=%s%s%s allOn=%s%s%s zoom=%s%s%s",
           onOffColor(gInvertEnabled), log_bool_str(gInvertEnabled), LOG_COLOR_RESET,
           onOffColor(gFlipXEnabled), log_bool_str(gFlipXEnabled), LOG_COLOR_RESET,
           onOffColor(gFlipYEnabled), log_bool_str(gFlipYEnabled), LOG_COLOR_RESET,
           onOffColor(gSleepEnabled), log_bool_str(gSleepEnabled), LOG_COLOR_RESET,
           onOffColor(gAllPixelsOn), log_bool_str(gAllPixelsOn), LOG_COLOR_RESET,
           onOffColor(gZoomEnabled), log_bool_str(gZoomEnabled), LOG_COLOR_RESET);
      LOGI("  fade=%s interval=%u", fadeModeToString(gFadeMode), static_cast<unsigned>(gFadeInterval));

    } else if (cmd::match(cmdBuf, "health")) {
      diag::printHealthVerbose(display);

    } else if (cmd::match(cmdBuf, "brief")) {
      diag::printHealthOneLine(display);

    } else if (cmd::match(cmdBuf, "counters")) {
      printRawCounters();

    } else if (cmd::match(cmdBuf, "threshold")) {
      LOGI("Offline threshold: %u", OFFLINE_THRESHOLD);
      LOGI("Current consecutive failures: %s%u%s",
           goodIfZeroColor(display.consecutiveFailures()),
           display.consecutiveFailures(),
           LOG_COLOR_RESET);
      LOGI("State: %s%s%s",
           stateColor(display.state()),
           diag::stateToString(display.state()),
           LOG_COLOR_RESET);

    } else if (cmd::match(cmdBuf, "bufsize")) {
      LOGI("Buffer size: %u bytes", static_cast<unsigned>(display.getBufferSize()));
      LOGI("Page buffer mode: %s", display.isPageBufferMode() ? "true" : "false");
      LOGI("Current page index: %u / %u",
           static_cast<unsigned>(display.currentPageIndex()),
           static_cast<unsigned>(display.totalPages()));
      LOGI("Page Y offset: %d", static_cast<int>(display.pageBufferYOffset()));
      LOGI("Flags: initialized=%s sleeping=%s flushing=%s pageIterating=%s dirty=%s",
           log_bool_str(display.isInitialized()),
           log_bool_str(display.isSleeping()),
           log_bool_str(display.isFlushing()),
           log_bool_str(display.isPageIterating()),
           log_bool_str(display.isDirty()));

    } else if (cmd::match(cmdBuf, "statex")) {
      LOGI("initialized=%s sleeping=%s flushing=%s pageIterating=%s dirty=%s controlDirty=%s",
           log_bool_str(display.isInitialized()),
           log_bool_str(display.isSleeping()),
           log_bool_str(display.isFlushing()),
           log_bool_str(display.isPageIterating()),
           log_bool_str(display.isDirty()),
           log_bool_str(display.controlStateDirty()));
      if (display.controlStateDirty()) {
        const SSD1315::Status controlErr = display.controlStateError();
        LOGI("controlStateError=%s detail=%ld msg=%s",
             diag::errToString(controlErr.code),
             static_cast<long>(controlErr.detail),
             controlErr.msg ? controlErr.msg : "");
      }

    } else if (strcmp(cmdBuf, "buffer") == 0) {
      const uint8_t* buf = display.getBuffer();
      const size_t size = display.getBufferSize();
      const size_t dump = (size < 16U) ? size : 16U;
      LOGI("buffer=%p size=%u dump=%u", static_cast<const void*>(buf),
           static_cast<unsigned>(size), static_cast<unsigned>(dump));
      if (!buf || size == 0U) {
        LOGI("buffer unavailable");
      } else {
        for (size_t i = 0; i < dump; ++i) {
          LOGI("  [%02u] = 0x%02X", static_cast<unsigned>(i), static_cast<unsigned>(buf[i]));
        }
      }
    } else if (cmd::parseInt(cmdBuf, "buffer", &value)) {
      const uint8_t* buf = display.getBuffer();
      const size_t size = display.getBufferSize();
      if (value < 0) {
        LOGE("buffer count must be >=0");
      } else if (!buf || size == 0U) {
        LOGI("buffer unavailable");
      } else {
        size_t dump = static_cast<size_t>(value);
        if (dump > size) dump = size;
        LOGI("buffer=%p size=%u dump=%u", static_cast<const void*>(buf),
             static_cast<unsigned>(size), static_cast<unsigned>(dump));
        for (size_t i = 0; i < dump; ++i) {
          LOGI("  [%02u] = 0x%02X", static_cast<unsigned>(i), static_cast<unsigned>(buf[i]));
        }
      }

    } else if (strcmp(cmdBuf, "dirty") == 0) {
      LOGI("dirty=%s", log_bool_str(display.isDirty()));
    } else if (cmd::match(cmdBuf, "dirty clear")) {
      display.clearDirty();
      LOGI("dirty=%s", log_bool_str(display.isDirty()));
    } else if (cmd::match(cmdBuf, "dirty all")) {
      display.markAllDirty();
      LOGI("dirty=%s", log_bool_str(display.isDirty()));
    } else if (strncasecmp(cmdBuf, "dirty mark ", 10) == 0) {
      int p = 0, minCol = 0, maxCol = 0;
      if (sscanf(cmdBuf + 10, "%d %d %d", &p, &minCol, &maxCol) == 3 &&
          p >= 0 && p <= 255 && minCol >= 0 && minCol <= 255 && maxCol >= 0 && maxCol <= 255 &&
          minCol <= maxCol) {
        display.markDirty(static_cast<uint8_t>(p),
                          static_cast<uint8_t>(minCol),
                          static_cast<uint8_t>(maxCol));
        LOGI("dirty=%s", log_bool_str(display.isDirty()));
      } else {
        LOGE("Usage: dirty mark <page 0..255> <minCol 0..255> <maxCol 0..255>");
      }

    } else if (cmd::match(cmdBuf, "touch")) {
      display.touch();
      LOGI("touch(): %sOK%s", LOG_COLOR_GREEN, LOG_COLOR_RESET);

    } else if (cmd::match(cmdBuf, "clearerr")) {
      display.clearError();
      LOGI("lastError cleared");

    } else if (strcmp(cmdBuf, "userpages") == 0) {
      LOGI("userpages=%u", static_cast<unsigned>(display.getUserPageCount()));
    } else if (cmd::parseInt(cmdBuf, "userpages", &value)) {
      if (value < 1 || value > 255) {
        LOGE("userpages must be 1..255");
      } else {
        display.setUserPageCount(static_cast<uint8_t>(value));
        LOGI("userpages=%u", static_cast<unsigned>(display.getUserPageCount()));
      }

    } else if (strcmp(cmdBuf, "activepage") == 0) {
      LOGI("activepage=%u", static_cast<unsigned>(display.getActiveUserPage()));
    } else if (cmd::parseInt(cmdBuf, "activepage", &value)) {
      if (value < 0 || value > 255) {
        LOGE("activepage out of range");
      } else {
        display.setActiveUserPage(static_cast<uint8_t>(value));
        LOGI("activepage=%u", static_cast<unsigned>(display.getActiveUserPage()));
      }

    } else if (strcmp(cmdBuf, "pagecycle") == 0) {
      const SSD1315::Config& cfg = display.getConfig();
      LOGI("pagecycle=%lu", static_cast<unsigned long>(cfg.pageCycleMs));
    } else if (cmd::parseInt(cmdBuf, "pagecycle", &value)) {
      if (value < 0) {
        LOGE("pagecycle must be >=0");
      } else {
        display.setPageCycleInterval(static_cast<uint32_t>(value));
        const SSD1315::Config& cfg = display.getConfig();
        LOGI("pagecycle=%lu", static_cast<unsigned long>(cfg.pageCycleMs));
      }

    } else if (strcmp(cmdBuf, "autosleep") == 0) {
      const SSD1315::Config& cfg = display.getConfig();
      LOGI("autosleep=%lu", static_cast<unsigned long>(cfg.inactivitySleepMs));
    } else if (cmd::parseInt(cmdBuf, "autosleep", &value)) {
      if (value < 0) {
        LOGE("autosleep must be >=0");
      } else {
        display.setAutoSleep(static_cast<uint32_t>(value));
        const SSD1315::Config& cfg = display.getConfig();
        LOGI("autosleep=%lu", static_cast<unsigned long>(cfg.inactivitySleepMs));
      }

    } else if (strcmp(cmdBuf, "pageiter") == 0) {
      const int maxSteps = static_cast<int>(display.totalPages());
      display.firstPage();
      int steps = 0;
      do {
        display.tick(millis());
        steps++;
      } while (display.nextPage() && steps < maxSteps);
      LOGI("pageiter completed, steps=%d", steps);
    } else if (cmd::parseInt(cmdBuf, "pageiter", &value)) {
      if (value <= 0 || value > 256) {
        LOGE("pageiter count must be 1..256");
      } else {
        display.firstPage();
        int steps = 0;
        do {
          display.tick(millis());
          steps++;
        } while (display.nextPage() && steps < value);
        LOGI("pageiter completed, steps=%d", steps);
      }

    } else if (strncasecmp(cmdBuf, "cmd ", 4) == 0) {
      int b0 = 0;
      if (sscanf(cmdBuf + 4, "%i", &b0) == 1 && b0 >= 0 && b0 <= 255) {
        printStatusResult("cmd", display.sendCommand(static_cast<uint8_t>(b0)));
      } else {
        LOGE("Usage: cmd <byte>");
      }

    } else if (strncasecmp(cmdBuf, "cmd2 ", 5) == 0) {
      int b0 = 0, b1 = 0;
      if (sscanf(cmdBuf + 5, "%i %i", &b0, &b1) == 2 &&
          b0 >= 0 && b0 <= 255 && b1 >= 0 && b1 <= 255) {
        printStatusResult("cmd2", display.sendCommand2(static_cast<uint8_t>(b0), static_cast<uint8_t>(b1)));
      } else {
        LOGE("Usage: cmd2 <byte0> <byte1>");
      }

    } else if (strncasecmp(cmdBuf, "cmd3 ", 5) == 0) {
      int b0 = 0, b1 = 0, b2 = 0;
      if (sscanf(cmdBuf + 5, "%i %i %i", &b0, &b1, &b2) == 3 &&
          b0 >= 0 && b0 <= 255 && b1 >= 0 && b1 <= 255 && b2 >= 0 && b2 <= 255) {
        printStatusResult("cmd3", display.sendCommand3(static_cast<uint8_t>(b0),
                                                       static_cast<uint8_t>(b1),
                                                       static_cast<uint8_t>(b2)));
      } else {
        LOGE("Usage: cmd3 <byte0> <byte1> <byte2>");
      }

    } else if (strncasecmp(cmdBuf, "cmdlist ", 8) == 0) {
      uint8_t list[32] = {0};
      int count = 0;
      const char* p = cmdBuf + 8;
      while (*p != '\0' && count < 32) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;
        char* end = nullptr;
        long v = strtol(p, &end, 0);
        if (end == p || v < 0 || v > 255) {
          count = -1;
          break;
        }
        list[count++] = static_cast<uint8_t>(v);
        p = end;
      }
      if (count <= 0) {
        LOGE("Usage: cmdlist <b0> [b1 ... b31]");
      } else {
        printStatusResult("cmdlist", display.sendCommandList(list, static_cast<size_t>(count)));
      }

    } else if (cmd::match(cmdBuf, "probe")) {
      runProbe();

    } else if (cmd::match(cmdBuf, "recover")) {
      runRecover();

    } else if (cmd::match(cmdBuf, "scan")) {
      i2c_scanner::scanDefault();

    } else if (strcmp(cmdBuf, "stress_mix") == 0) {
      runStressMix(DEFAULT_BURST_COUNT);

    } else if (cmd::parseInt(cmdBuf, "stress_mix", &value)) {
      if (value > 0 && value <= 10000) {
        runStressMix(value);
      } else {
        LOGE("Count must be 1-10000");
      }

    } else if (strcmp(cmdBuf, "stress") == 0) {
      LOGI("stress: using default count=%lu", (unsigned long)DEFAULT_STRESS_COUNT);
      runContrastStress(DEFAULT_STRESS_COUNT);

    } else if (cmd::parseInt(cmdBuf, "stress", &value)) {
      if (value > 0 && value <= 10000) {
        runContrastStress(value);
      } else {
        LOGE("Count must be 1-10000");
      }

    } else if (strcmp(cmdBuf, "flushstress") == 0) {
      LOGI("flushstress: using default count=%lu",
           (unsigned long)DEFAULT_FLUSH_STRESS_COUNT);
      runFlushStress(DEFAULT_FLUSH_STRESS_COUNT);

    } else if (cmd::parseInt(cmdBuf, "flushstress", &value)) {
      if (value > 0 && value <= 1000) {
        runFlushStress(value);
      } else {
        LOGE("Count must be 1-1000");
      }

    } else if (strcmp(cmdBuf, "burst") == 0) {
      LOGI("burst: using default count=%lu", (unsigned long)DEFAULT_BURST_COUNT);
      runBurstTest(DEFAULT_BURST_COUNT);

    } else if (cmd::parseInt(cmdBuf, "burst", &value)) {
      if (value > 0 && value <= 10000) {
        runBurstTest(value);
      } else {
        LOGE("Count must be 1-10000");
      }

    } else if (strcmp(cmdBuf, "monitor") == 0) {
      if (monitorEnabled) {
        LOGI("Health monitor: %sON%s (interval=%lu ms)",
             onOffColor(true), LOG_COLOR_RESET, (unsigned long)monitorIntervalMs);
      } else {
        LOGI("Health monitor: %sOFF%s", onOffColor(false), LOG_COLOR_RESET);
      }

    } else if (cmd::parseInt(cmdBuf, "monitor", &value)) {
      if (value > 0) {
        monitorEnabled = true;
        monitorIntervalMs = static_cast<uint32_t>(value);
        healthMonitor.begin(value);
        LOGI("Health monitor: %sON%s (interval=%d ms)",
             onOffColor(true), LOG_COLOR_RESET, value);
      } else {
        monitorEnabled = false;
        monitorIntervalMs = 0;
        LOGI("Health monitor: %sOFF%s", onOffColor(false), LOG_COLOR_RESET);
      }

    } else if (cmd::parseInt(cmdBuf, "verbose", &value)) {
      verboseMode = (value != 0);
      LOGI("Verbose mode: %s%s%s",
           onOffColor(verboseMode), verboseMode ? "ON" : "OFF", LOG_COLOR_RESET);

    } else if (strcmp(cmdBuf, "verbose") == 0) {
      LOGI("Verbose mode: %s%s%s",
           onOffColor(verboseMode), verboseMode ? "ON" : "OFF", LOG_COLOR_RESET);

    } else if (strcmp(cmdBuf, "contrast") == 0) {
      const SSD1315::Config& cfg = display.getConfig();
      LOGI("Current contrast: %u", static_cast<unsigned>(cfg.contrast));

    } else if (cmd::parseInt(cmdBuf, "contrast", &value)) {
      if (value >= 1 && value <= 255) {
        diag::HealthSnapshot before, after;
        before.capture(display);
        
        SSD1315::Status st = display.setContrast((uint8_t)value);
        
        after.capture(display);
        
        printStatusResult("setContrast", st);
        LOGI("Health changes:");
        diag::printHealthDiff(before, after);
      } else {
        LOGE("Contrast must be 1-255");
      }

    } else if (strcmp(cmdBuf, "bright") == 0) {
      const SSD1315::Config& cfg = display.getConfig();
      LOGI("Current brightness: %u", static_cast<unsigned>(cfg.contrast));
    } else if (cmd::parseInt(cmdBuf, "bright", &value)) {
      if (value >= 1 && value <= 255) {
        diag::HealthSnapshot before, after;
        before.capture(display);

        SSD1315::Status st = display.setBrightness(static_cast<uint8_t>(value));

        after.capture(display);
        printStatusResult("setBrightness", st);
        LOGI("Health changes:");
        diag::printHealthDiff(before, after);
      } else {
        LOGE("Brightness must be 1-255");
      }

    } else if (strcmp(cmdBuf, "invert") == 0) {
      LOGI("invert=%s%s%s", onOffColor(gInvertEnabled), log_bool_str(gInvertEnabled), LOG_COLOR_RESET);
    } else if (strncasecmp(cmdBuf, "invert ", 7) == 0) {
      char token[16] = {0};
      if (sscanf(cmdBuf + 7, "%15s", token) == 1) {
        bool enable = false;
        if (!parseBoolValue(token, enable)) {
          LOGE("Usage: invert [0|1|off|on]");
        } else {
          SSD1315::Status st = display.setInvert(enable);
          if (st.ok()) {
            gInvertEnabled = enable;
          }
          printStatusResult("invert", st);
        }
      }

    } else if (strcmp(cmdBuf, "flipx") == 0) {
      LOGI("flipx=%s%s%s", onOffColor(gFlipXEnabled), log_bool_str(gFlipXEnabled), LOG_COLOR_RESET);
    } else if (strncasecmp(cmdBuf, "flipx ", 6) == 0) {
      char token[16] = {0};
      if (sscanf(cmdBuf + 6, "%15s", token) == 1) {
        bool enable = false;
        if (!parseBoolValue(token, enable)) {
          LOGE("Usage: flipx [0|1|off|on]");
        } else {
          SSD1315::Status st = display.setFlipX(enable);
          if (st.ok()) {
            gFlipXEnabled = enable;
          }
          printStatusResult("flipx", st);
        }
      }

    } else if (strcmp(cmdBuf, "flipy") == 0) {
      LOGI("flipy=%s%s%s", onOffColor(gFlipYEnabled), log_bool_str(gFlipYEnabled), LOG_COLOR_RESET);
    } else if (strncasecmp(cmdBuf, "flipy ", 6) == 0) {
      char token[16] = {0};
      if (sscanf(cmdBuf + 6, "%15s", token) == 1) {
        bool enable = false;
        if (!parseBoolValue(token, enable)) {
          LOGE("Usage: flipy [0|1|off|on]");
        } else {
          SSD1315::Status st = display.setFlipY(enable);
          if (st.ok()) {
            gFlipYEnabled = enable;
          }
          printStatusResult("flipy", st);
        }
      }

    } else if (strcmp(cmdBuf, "display") == 0) {
      const SSD1315::SettingsSnapshot s = display.getSettings();
      LOGI("display=%s", s.sleeping ? "off" : "on");
    } else if (strncasecmp(cmdBuf, "display ", 8) == 0) {
      char token[16] = {0};
      if (sscanf(cmdBuf + 8, "%15s", token) == 1) {
        bool sleep = false;
        if (strcasecmp(token, "off") == 0) {
          sleep = true;
        } else if (strcasecmp(token, "on") == 0) {
          sleep = false;
        } else {
          LOGE("Usage: display <off|on>");
          sleep = gSleepEnabled;
        }
        if (strcasecmp(token, "off") == 0 || strcasecmp(token, "on") == 0) {
          SSD1315::Status st = display.setSleep(sleep);
          if (st.ok()) {
            gSleepEnabled = sleep;
          }
          printStatusResult("display", st);
        }
      } else {
        LOGE("Usage: display <off|on>");
      }

    } else if (strcmp(cmdBuf, "sleep") == 0) {
      LOGI("sleep=%s%s%s", onOffColor(gSleepEnabled), log_bool_str(gSleepEnabled), LOG_COLOR_RESET);
    } else if (strncasecmp(cmdBuf, "sleep ", 6) == 0) {
      char token[16] = {0};
      if (sscanf(cmdBuf + 6, "%15s", token) == 1) {
        bool enable = false;
        if (!parseBoolValue(token, enable)) {
          LOGE("Usage: sleep [0|1|off|on]");
        } else {
          SSD1315::Status st = display.setSleep(enable);
          if (st.ok()) {
            gSleepEnabled = enable;
          }
          printStatusResult("sleep", st);
        }
      }

    } else if (strcmp(cmdBuf, "allon") == 0) {
      LOGI("allon=%s%s%s", onOffColor(gAllPixelsOn), log_bool_str(gAllPixelsOn), LOG_COLOR_RESET);
    } else if (strncasecmp(cmdBuf, "allon ", 6) == 0) {
      char token[16] = {0};
      if (sscanf(cmdBuf + 6, "%15s", token) == 1) {
        bool enable = false;
        if (!parseBoolValue(token, enable)) {
          LOGE("Usage: allon [0|1|off|on]");
        } else {
          SSD1315::Status st = display.setAllPixelsOn(enable);
          if (st.ok()) {
            gAllPixelsOn = enable;
          }
          printStatusResult("allon", st);
        }
      }

    } else if (strcmp(cmdBuf, "zoom") == 0) {
      LOGI("zoom=%s%s%s", onOffColor(gZoomEnabled), log_bool_str(gZoomEnabled), LOG_COLOR_RESET);
    } else if (strncasecmp(cmdBuf, "zoom ", 5) == 0) {
      char token[16] = {0};
      if (sscanf(cmdBuf + 5, "%15s", token) == 1) {
        bool enable = false;
        if (!parseBoolValue(token, enable)) {
          LOGE("Usage: zoom [0|1|off|on]");
        } else {
          SSD1315::Status st = display.setZoom(enable);
          if (st.ok()) {
            gZoomEnabled = enable;
          }
          printStatusResult("zoom", st);
        }
      }

    } else if (strcmp(cmdBuf, "fade") == 0) {
      LOGI("fade=%s interval=%u",
           fadeModeToString(gFadeMode),
           static_cast<unsigned>(gFadeInterval));
    } else if (strncasecmp(cmdBuf, "fade ", 5) == 0) {
      char modeToken[16] = {0};
      int interval = -1;
      const int count = sscanf(cmdBuf + 5, "%15s %d", modeToken, &interval);
      if (count < 1) {
        LOGE("Usage: fade [off|fade|blink] [interval 0..15]");
      } else {
        SSD1315::FadeMode mode = SSD1315::FadeMode::OFF;
        if (strcasecmp(modeToken, "off") == 0) {
          mode = SSD1315::FadeMode::OFF;
        } else if (strcasecmp(modeToken, "fade") == 0) {
          mode = SSD1315::FadeMode::FADE_OUT;
        } else if (strcasecmp(modeToken, "blink") == 0) {
          mode = SSD1315::FadeMode::BLINK;
        } else {
          LOGE("Unknown fade mode: %s", modeToken);
          mode = SSD1315::FadeMode::OFF;
          interval = -2;
        }
        if (interval != -2) {
          uint8_t fadeInterval = (count >= 2) ? static_cast<uint8_t>(interval) : gFadeInterval;
          if ((count >= 2) && (interval < 0 || interval > 15)) {
            LOGE("Fade interval must be 0..15");
          } else {
            SSD1315::Status st = display.setFadeMode(mode, fadeInterval);
            if (st.ok()) {
              gFadeMode = mode;
              gFadeInterval = fadeInterval;
            }
            printStatusResult("fade", st);
          }
        }
      }

    } else if (strncasecmp(cmdBuf, "scrollh ", 8) == 0) {
      char dir[8] = {0};
      int startPage = 0;
      int endPage = 0;
      int speedRaw = static_cast<int>(SSD1315::ScrollSpeed::FRAMES_5);
      const int count = sscanf(cmdBuf + 8, "%7s %d %d %d", dir, &startPage, &endPage, &speedRaw);
      if (count < 3) {
        LOGE("Usage: scrollh <left|right> <startPage> <endPage> [speed 0..7]");
      } else if (startPage < 0 || startPage > 7 || endPage < startPage || endPage > 7) {
        LOGE("scrollh pages must satisfy 0<=start<=end<=7");
      } else {
        const bool left = (dir[0] == 'l' || dir[0] == 'L');
        const bool right = (dir[0] == 'r' || dir[0] == 'R');
        SSD1315::ScrollSpeed speed = SSD1315::ScrollSpeed::FRAMES_5;
        if (!left && !right) {
          LOGE("scrollh direction must be left or right");
        } else if (!parseScrollSpeedArg(speedRaw, speed)) {
          LOGE("scrollh speed must be 0..7");
        } else {
          SSD1315::Status st = display.startHorizontalScroll(
              left,
              static_cast<uint8_t>(startPage),
              static_cast<uint8_t>(endPage),
              speed);
          if (st.ok()) {
            gScrollActive = true;
          }
          printStatusResult("scrollh", st);
        }
      }

    } else if (strncasecmp(cmdBuf, "scrollv ", 8) == 0) {
      char dir[8] = {0};
      int startPage = 0;
      int endPage = 0;
      int offset = 0;
      int speedRaw = static_cast<int>(SSD1315::ScrollSpeed::FRAMES_5);
      const int count = sscanf(cmdBuf + 8, "%7s %d %d %d %d", dir, &startPage, &endPage, &offset, &speedRaw);
      if (count < 4) {
        LOGE("Usage: scrollv <left|right> <startPage> <endPage> <offset 0..63> [speed 0..7]");
      } else if (startPage < 0 || startPage > 7 || endPage < startPage || endPage > 7) {
        LOGE("scrollv pages must satisfy 0<=start<=end<=7");
      } else if (offset < 0 || offset > 63) {
        LOGE("scrollv offset must be 0..63");
      } else {
        const bool left = (dir[0] == 'l' || dir[0] == 'L');
        const bool right = (dir[0] == 'r' || dir[0] == 'R');
        SSD1315::ScrollSpeed speed = SSD1315::ScrollSpeed::FRAMES_5;
        if (!left && !right) {
          LOGE("scrollv direction must be left or right");
        } else if (!parseScrollSpeedArg(speedRaw, speed)) {
          LOGE("scrollv speed must be 0..7");
        } else {
          SSD1315::Status st = display.startVerticalScroll(
              left,
              static_cast<uint8_t>(startPage),
              static_cast<uint8_t>(endPage),
              speed,
              static_cast<uint8_t>(offset));
          if (st.ok()) {
            gScrollActive = true;
          }
          printStatusResult("scrollv", st);
        }
      }

    } else if (strcasecmp(cmdBuf, "scroll stop") == 0 ||
               strcasecmp(cmdBuf, "scrollstop") == 0) {
      SSD1315::Status st = display.stopScroll();
      if (st.ok()) {
        gScrollActive = false;
      }
      printStatusResult("scroll stop", st);

    } else if (strncasecmp(cmdBuf, "scrollarea ", 11) == 0) {
      int topRows = 0;
      int scrollRows = 0;
      if (sscanf(cmdBuf + 11, "%d %d", &topRows, &scrollRows) == 2 &&
          topRows >= 0 && topRows <= 63 &&
          scrollRows > 0 && scrollRows <= 64) {
        SSD1315::Status st = display.setVerticalScrollArea(
            static_cast<uint8_t>(topRows),
            static_cast<uint8_t>(scrollRows));
        printStatusResult("scrollarea", st);
      } else {
        LOGE("Usage: scrollarea <topRows 0..63> <scrollRows 1..64>");
      }

    } else if (cmd::match(cmdBuf, "flush")) {
      SSD1315::Status st = flushBlocking();
      printStatusResult("flush", st);

    } else if (strncasecmp(cmdBuf, "flushrect ", 10) == 0) {
      int x = 0, y = 0, w = 0, h = 0;
      if (sscanf(cmdBuf + 10, "%d %d %d %d", &x, &y, &w, &h) == 4) {
        SSD1315::Status st = display.requestFlushRect(x, y, w, h);
        if (st.ok()) {
          st = display.waitFlush(millis());
        }
        printStatusResult("flushrect", st);
      } else {
        LOGE("Usage: flushrect <x> <y> <w> <h>");
      }

    } else if (strncasecmp(cmdBuf, "pattern ", 8) == 0) {
      char pattern[16] = {0};
      int size = 1;
      const int count = sscanf(cmdBuf + 8, "%15s %d", pattern, &size);
      if (count < 1) {
        LOGE("Usage: pattern <checker|vstripes|hstripes> [size]");
      } else if (size < 1 || size > 32) {
        LOGE("Pattern size must be 1..32");
      } else {
        if (strcasecmp(pattern, "checker") == 0) {
          display.fillCheckerboard(static_cast<uint8_t>(size));
        } else if (strcasecmp(pattern, "vstripes") == 0) {
          display.fillVerticalStripes(static_cast<uint8_t>(size));
        } else if (strcasecmp(pattern, "hstripes") == 0) {
          display.fillHorizontalStripes(static_cast<uint8_t>(size));
        } else {
          LOGE("Unknown pattern: %s", pattern);
          size = -1;
        }
        if (size != -1) {
          SSD1315::Status st = flushBlocking();
          printStatusResult("pattern", st);
        }
      }

    } else if (cmd::match(cmdBuf, "fill")) {
      display.fill();
      printStatusResult("fill", flushBlocking());

    } else if (strncasecmp(cmdBuf, "line ", 5) == 0) {
      int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
      if (sscanf(cmdBuf + 5, "%d %d %d %d", &x0, &y0, &x1, &y1) == 4) {
        display.drawLine(x0, y0, x1, y1, true);
        printStatusResult("line", flushBlocking());
      } else {
        LOGE("Usage: line <x0> <y0> <x1> <y1>");
      }

    } else if (strncasecmp(cmdBuf, "vline ", 6) == 0) {
      int x = 0, y = 0, h = 0;
      if (sscanf(cmdBuf + 6, "%d %d %d", &x, &y, &h) == 3) {
        display.drawVLine(x, y, h, true);
        printStatusResult("vline", flushBlocking());
      } else {
        LOGE("Usage: vline <x> <y> <h>");
      }

    } else if (strncasecmp(cmdBuf, "rect ", 5) == 0) {
      int x = 0, y = 0, w = 0, h = 0;
      if (sscanf(cmdBuf + 5, "%d %d %d %d", &x, &y, &w, &h) == 4) {
        display.drawRect(x, y, w, h, true);
        printStatusResult("rect", flushBlocking());
      } else {
        LOGE("Usage: rect <x> <y> <w> <h>");
      }

    } else if (strncasecmp(cmdBuf, "fillrect ", 9) == 0) {
      int x = 0, y = 0, w = 0, h = 0;
      if (sscanf(cmdBuf + 9, "%d %d %d %d", &x, &y, &w, &h) == 4) {
        display.fillRect(x, y, w, h, true);
        printStatusResult("fillrect", flushBlocking());
      } else {
        LOGE("Usage: fillrect <x> <y> <w> <h>");
      }

    } else if (strncasecmp(cmdBuf, "circle ", 7) == 0) {
      int x = 0, y = 0, r = 0;
      if (sscanf(cmdBuf + 7, "%d %d %d", &x, &y, &r) == 3) {
        display.drawCircle(x, y, r, true);
        printStatusResult("circle", flushBlocking());
      } else {
        LOGE("Usage: circle <x> <y> <r>");
      }

    } else if (strncasecmp(cmdBuf, "fillcircle ", 11) == 0) {
      int x = 0, y = 0, r = 0;
      if (sscanf(cmdBuf + 11, "%d %d %d", &x, &y, &r) == 3) {
        display.fillCircle(x, y, r, true);
        printStatusResult("fillcircle", flushBlocking());
      } else {
        LOGE("Usage: fillcircle <x> <y> <r>");
      }

    } else if (strncasecmp(cmdBuf, "pixel ", 6) == 0) {
      int x = 0, y = 0, v = -1;
      const int count = sscanf(cmdBuf + 6, "%d %d %d", &x, &y, &v);
      if (count < 2) {
        LOGE("Usage: pixel <x> <y> [0|1]");
      } else if (count == 2) {
        LOGI("pixel(%d,%d)=%s", x, y, display.getPixel(x, y) ? "1" : "0");
      } else if (v == 0 || v == 1) {
        display.setPixel(x, y, v == 1);
        printStatusResult("pixel", flushBlocking());
      } else {
        LOGE("pixel value must be 0 or 1");
      }

    } else if (strncasecmp(cmdBuf, "char ", 5) == 0) {
      int x = 0, y = 0;
      char token[32] = {0};
      if (sscanf(cmdBuf + 5, "%d %d %31s", &x, &y, token) == 3) {
        char c = token[0];
        if (token[1] != '\0') {
          char* end = nullptr;
          const long v = strtol(token, &end, 0);
          if (end && *end == '\0' && v >= 0 && v <= 255) {
            c = static_cast<char>(v);
          } else {
            LOGE("char payload must be single character or ASCII code 0..255");
            c = '\0';
          }
        }
        if (c != '\0') {
          display.drawChar(x, y, c, true);
          printStatusResult("char", flushBlocking());
        }
      } else {
        LOGE("Usage: char <x> <y> <c|ascii>");
      }

    } else if (strncasecmp(cmdBuf, "bitmap ", 7) == 0) {
      int x = 0, y = 0;
      if (sscanf(cmdBuf + 7, "%d %d", &x, &y) == 2) {
        display.drawBitmap(x, y, kDiagBitmap8x8, 8, 8, true);
        printStatusResult("bitmap", flushBlocking());
      } else {
        LOGE("Usage: bitmap <x> <y>");
      }

    } else if (strncasecmp(cmdBuf, "textw ", 6) == 0) {
      const int16_t w = SSD1315::SSD1315::getTextWidth(cmdBuf + 6);
      LOGI("text width: %d px", static_cast<int>(w));

    } else if (strcmp(cmdBuf, "demo") == 0) {
      LOGI("demo: using default loops=%lu", static_cast<unsigned long>(DEFAULT_DEMO_LOOPS));
      runFeatureDemo(DEFAULT_DEMO_LOOPS);

    } else if (cmd::parseInt(cmdBuf, "demo", &value)) {
      if (value >= 1 && value <= 100) {
        runFeatureDemo(static_cast<uint32_t>(value));
      } else {
        LOGE("Demo loops must be 1-100");
      }

    } else if (strncmp(cmdBuf, "text ", 5) == 0) {
      diag::HealthSnapshot before, after;
      before.capture(display);
      
      display.clear();
      display.drawText(0, 28, cmdBuf + 5);
      display.requestFlush();
      SSD1315::Status st = display.waitFlush(millis());
      
      after.capture(display);
      
      printStatusResult("text+flush", st);
      LOGI("Health changes:");
      diag::printHealthDiff(before, after);

    } else if (cmd::match(cmdBuf, "clear")) {
      diag::HealthSnapshot before, after;
      before.capture(display);
      
      display.clear();
      SSD1315::Status st = flushBlocking();
      
      after.capture(display);
      
      printStatusResult("clear+flush", st);
      LOGI("Health changes:");
      diag::printHealthDiff(before, after);

    } else if (cmd::match(cmdBuf, "reset")) {
      LOGI("Software reinitializing display (no RES# GPIO toggle)...");
      display.end();
      LOGI("After end():");
      diag::printHealthOneLine(display);
      
      SSD1315::Config cfg;
      configureDisplayConfig(cfg);
      
      SSD1315::Status st = display.begin(cfg);
      printStatusResult("begin", st);
      if (st.ok()) {
        gInvertEnabled = false;
        gFlipXEnabled = false;
        gFlipYEnabled = false;
        gSleepEnabled = false;
        gAllPixelsOn = false;
        gZoomEnabled = false;
        gScrollActive = false;
        gFadeMode = SSD1315::FadeMode::OFF;
        gFadeInterval = 0;
      }
      LOGI("After begin():");
      diag::printHealthOneLine(display);

    } else if (cmd::match(cmdBuf, "selftest") || cmd::match(cmdBuf, "featuretest")) {
      runSelfTest();

    } else {
      LOGE("Unknown command: %s", cmdBuf);
    }
  }

  delay(1);
}
