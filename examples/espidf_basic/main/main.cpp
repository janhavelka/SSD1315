/**
 * @file main.cpp
 * @brief Native ESP-IDF bring-up CLI for SSD1315.
 */

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#include <driver/i2c_master.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "sdkconfig.h"
#include "examples/common/IdfI2cTransport.h"
#include "ssd1315/SSD1315.h"

namespace {

constexpr int I2C_SDA = 8;
constexpr int I2C_SCL = 9;
constexpr uint8_t I2C_ADDRESS = 0x3C;
constexpr uint32_t I2C_FREQ_HZ = 400000U;
constexpr uint32_t I2C_TIMEOUT_MS = 50U;
constexpr size_t INPUT_MAX = 192;
constexpr const char* HIL_PANEL_PROFILE = "example-default-128x64-internal-charge-pump";

#ifdef CONFIG_IDF_TARGET
constexpr const char* HIL_BUILD_TARGET = CONFIG_IDF_TARGET;
#else
constexpr const char* HIL_BUILD_TARGET = "unknown";
#endif

SSD1315::SSD1315 display;
bool monitorMode = false;
uint32_t monitorNextMs = 0;
uint32_t monitorIntervalMs = 1000U;
bool gScrollActive = false;

void configureNonBlockingStdin() {
  const int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  if (flags >= 0) {
    (void)fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
  }
}

char* nextToken(char** save) {
  return strtok_r(nullptr, " \t", save);
}

void lowerInPlace(char* text) {
  for (; text != nullptr && *text != '\0'; ++text) {
    *text = static_cast<char>(tolower(static_cast<unsigned char>(*text)));
  }
}

bool parseU32(const char* text, uint32_t& out) {
  if (text == nullptr || text[0] == '\0') return false;
  char* end = nullptr;
  const unsigned long value = strtoul(text, &end, 0);
  if (end == text || *end != '\0') return false;
  out = static_cast<uint32_t>(value);
  return true;
}

bool parseI32(const char* text, int32_t& out) {
  if (text == nullptr || text[0] == '\0') return false;
  char* end = nullptr;
  const long value = strtol(text, &end, 0);
  if (end == text || *end != '\0') return false;
  out = static_cast<int32_t>(value);
  return true;
}

bool parseBool(const char* text, bool& out) {
  if (text == nullptr) return false;
  if (strcmp(text, "1") == 0 || strcmp(text, "on") == 0 || strcmp(text, "true") == 0) {
    out = true;
    return true;
  }
  if (strcmp(text, "0") == 0 || strcmp(text, "off") == 0 || strcmp(text, "false") == 0) {
    out = false;
    return true;
  }
  return false;
}

bool parseScrollDirection(char* text, bool& left) {
  if (text == nullptr) return false;
  lowerInPlace(text);
  if (strcmp(text, "left") == 0 || strcmp(text, "l") == 0) {
    left = true;
    return true;
  }
  if (strcmp(text, "right") == 0 || strcmp(text, "r") == 0) {
    left = false;
    return true;
  }
  return false;
}

bool parseScrollSpeed(char* text, SSD1315::ScrollSpeed& speed) {
  if (text == nullptr) {
    speed = SSD1315::ScrollSpeed::FRAMES_5;
    return true;
  }
  uint32_t raw = 0;
  if (!parseU32(text, raw) || raw > 7U) return false;
  speed = static_cast<SSD1315::ScrollSpeed>(raw);
  return true;
}

const char* errToStr(SSD1315::Err err) {
  switch (err) {
    case SSD1315::Err::OK: return "OK";
    case SSD1315::Err::INVALID_CONFIG: return "INVALID_CONFIG";
    case SSD1315::Err::INVALID_DIMENSIONS: return "INVALID_DIMENSIONS";
    case SSD1315::Err::INVALID_PAGE_COUNT: return "INVALID_PAGE_COUNT";
    case SSD1315::Err::NOT_INITIALIZED: return "NOT_INITIALIZED";
    case SSD1315::Err::STATE_ERROR: return "STATE_ERROR";
    case SSD1315::Err::BUSY: return "BUSY";
    case SSD1315::Err::PANEL_NOT_READY: return "PANEL_NOT_READY";
    case SSD1315::Err::I2C_NACK_ADDR: return "I2C_NACK_ADDR";
    case SSD1315::Err::I2C_NACK_DATA: return "I2C_NACK_DATA";
    case SSD1315::Err::I2C_TIMEOUT: return "I2C_TIMEOUT";
    case SSD1315::Err::I2C_BUS_ERROR: return "I2C_BUS_ERROR";
    case SSD1315::Err::TIMEOUT: return "TIMEOUT";
    case SSD1315::Err::BUFFER_OVERFLOW: return "BUFFER_OVERFLOW";
    case SSD1315::Err::UNSUPPORTED: return "UNSUPPORTED";
    case SSD1315::Err::INTERNAL_ERROR: return "INTERNAL_ERROR";
    case SSD1315::Err::DEVICE_NOT_FOUND: return "DEVICE_NOT_FOUND";
    case SSD1315::Err::IN_PROGRESS: return "IN_PROGRESS";
    default: return "UNKNOWN";
  }
}

const char* stateToStr(SSD1315::DriverState state) {
  switch (state) {
    case SSD1315::DriverState::UNINIT: return "UNINIT";
    case SSD1315::DriverState::READY: return "READY";
    case SSD1315::DriverState::DEGRADED: return "DEGRADED";
    case SSD1315::DriverState::OFFLINE: return "OFFLINE";
    default: return "UNKNOWN";
  }
}

const char* controllerProfileToStr(SSD1315::ControllerProfile profile) {
  switch (profile) {
    case SSD1315::ControllerProfile::SSD1315: return "SSD1315";
    default: return "UNKNOWN";
  }
}

void printVersionInfo() {
  const SSD1315::SettingsSnapshot s = display.getSettings();
  puts("=== Version Info ===");
  puts("  Framework: ESP-IDF");
  printf("  Build target: %s\n", HIL_BUILD_TARGET);
  printf("  Example firmware build: %s %s\n", __DATE__, __TIME__);
  printf("  SSD1315 library version: %s\n", SSD1315::VERSION);
  printf("  SSD1315 library full: %s\n", SSD1315::VERSION_FULL);
  printf("  SSD1315 library build: %s\n", SSD1315::BUILD_TIMESTAMP);
  printf("  SSD1315 library commit: %s (%s)\n", SSD1315::GIT_COMMIT, SSD1315::GIT_STATUS);
  printf("  Controller profile: %s\n", controllerProfileToStr(s.controllerProfile));
  printf("  Panel profile: %s\n", HIL_PANEL_PROFILE);
  printf("  Active I2C address: 0x%02X\n", s.i2cAddress);
  printf("  Geometry: %ux%u pages=%u pageBufferPages=%u\n",
         s.width, s.height, s.totalPages, s.pageBufferPages);
}

void printStatus(SSD1315::Status st) {
  printf("  Status: %s (code=%u, detail=%ld)\n", errToStr(st.code),
         static_cast<unsigned>(st.code), static_cast<long>(st.detail));
  if (st.msg != nullptr && st.msg[0] != '\0') printf("  Message: %s\n", st.msg);
}

SSD1315::Config makeConfig() {
  SSD1315::Config cfg{};
  cfg.i2cAddress = I2C_ADDRESS;
  cfg.i2cWrite = transport::wireWrite;
  cfg.i2cWriteRead = transport::wireWriteRead;
  cfg.i2cUser = transport::configUser();
  cfg.nowMs = transport::nowMs;
  cfg.cooperativeYield = transport::cooperativeYield;
  cfg.timeUser = transport::configUser();
  cfg.width = 128;
  cfg.height = 64;
  cfg.pageBufferPages = 8;
  cfg.i2cTimeoutMs = I2C_TIMEOUT_MS;
  cfg.byteBudgetPerTick = 128;
  cfg.offlineThreshold = 3;
  return cfg;
}

void printHelp() {
  puts("\n=== SSD1315 native ESP-IDF CLI ===");
  puts("Common: help version scan probe recover drv health monitor [0|1|ms] reset cfg read");
  puts("Reset: recover/reset are software-only; they do not toggle RES#");
  puts("Probe: probe is ACK-only; not SSD1315 identity; no health tracking");
  puts("Display: contrast <1..255> invert <0|1> flipx <0|1> flipy <0|1> display <off|on> sleep <0|1>");
  puts("Display: allon <0|1> zoom <0|1> fade <off|out|blink> [0..15]");
  puts("Scroll: scrollh <left|right> <startPage> <endPage> [speed] scrollv <left|right> <start> <end> <offset> [speed] scroll stop");
  puts("Draw: text <x> <y> <message> clear fill pattern <checker|vstripes|hstripes>");
  puts("Draw: line <x0> <y0> <x1> <y1> rect <x> <y> <w> <h> circle <x> <y> <r> pixel <x> <y> [0|1]");
  puts("Flush: flush flushrect <x> <y> <w> <h> demo stress [n] stress_mix [n] selftest");
}

void printHealth() {
  SSD1315::SettingsSnapshot s = display.getSettings();
  printf("Display: state=%s online=%s init=%s sleep=%s flush=%s dirty=0x%02X controlDirty=%s ok=%lu fail=%lu consec=%u\n",
         stateToStr(display.state()), display.isOnline() ? "yes" : "no",
         s.initialized ? "yes" : "no", s.sleeping ? "yes" : "no",
         s.flushing ? "yes" : "no", s.dirtyPages,
         s.controlStateDirty ? "yes" : "no",
         static_cast<unsigned long>(s.totalSuccess),
         static_cast<unsigned long>(s.totalFailures),
         static_cast<unsigned>(s.consecutiveFailures));
  if (s.controlStateDirty) printStatus(s.controlStateError);
  if (!s.lastError.ok()) printStatus(s.lastError);
}

void scanI2c() {
  printf("Scanning I2C bus (timeout=%lums)...\n",
         static_cast<unsigned long>(I2C_TIMEOUT_MS));
  Ssd1315IdfI2c& ctx = transport::idfContext();
  if (ctx.bus == nullptr) {
    puts("I2C scan unavailable: bus not initialized");
    return;
  }

  puts("     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F");

  uint8_t count = 0;
  for (uint8_t row = 0; row < 8; ++row) {
    printf("%02X: ", row * 16);
    for (uint8_t col = 0; col < 16; ++col) {
      const uint8_t addr = static_cast<uint8_t>(row * 16 + col);
      if (addr < 0x08 || addr > 0x77) {
        printf("   ");
        continue;
      }

      esp_err_t err = ESP_ERR_TIMEOUT;
      if (ctx.mutex != nullptr &&
          xSemaphoreTake(ctx.mutex, pdMS_TO_TICKS(I2C_TIMEOUT_MS)) == pdTRUE) {
        err = i2c_master_probe(ctx.bus, addr, static_cast<int>(I2C_TIMEOUT_MS));
        xSemaphoreGive(ctx.mutex);
      }
      if (err == ESP_OK) {
        printf("%02X ", addr);
        ++count;
      } else if (err == ESP_ERR_TIMEOUT) {
        printf("TO ");
      } else {
        printf("-- ");
      }
      vTaskDelay(pdMS_TO_TICKS(1));
    }
    putchar('\n');
  }

  printf("Scan complete. Found %u device(s).\n", static_cast<unsigned>(count));
  if (count > 0) {
    puts("Common addresses: 0x3C/0x3D=OLED, 0x48-0x4B=ADS1115, 0x51=RV3032, 0x76/0x77=BME280");
  }
}

SSD1315::Status requestAndWaitFlush() {
  SSD1315::Status st = display.requestFlush();
  if (!st.ok() && !st.inProgress()) {
    printStatus(st);
    return st;
  }
  st = display.waitFlush(transport::nowMs(nullptr), 1000);
  printStatus(st);
  return st;
}

void drawDemo() {
  SSD1315::Status st = display.stopScroll();
  printStatus(st);
  if (!st.ok()) return;
  gScrollActive = false;

  st = display.setAllPixelsOn(false);
  printStatus(st);
  if (!st.ok()) return;

  st = display.setInvert(false);
  printStatus(st);
  if (!st.ok()) return;

  st = display.setSleep(false);
  printStatus(st);
  if (!st.ok()) return;

  display.clear();
  st = requestAndWaitFlush();
  if (!st.ok()) return;

  display.drawText(0, 0, "SSD1315 ESP-IDF");
  display.drawRect(0, 12, 127, 51);
  display.drawLine(0, 63, 127, 12);
  display.drawCircle(96, 38, 16);
  requestAndWaitFlush();
}

void runStress(uint32_t count, bool mixed) {
  uint32_t ok = 0;
  uint32_t fail = 0;
  for (uint32_t i = 0; i < count; ++i) {
    if (mixed) {
      display.clear();
      display.drawText(0, 0, "mix");
      display.drawRect(static_cast<int16_t>(i % 64), 16, 24, 16);
    } else {
      (i % 2U) == 0U ? display.fillCheckerboard(8) : display.clear();
    }
    const SSD1315::Status st = display.requestFlush();
    display.tick(transport::nowMs(nullptr));
    st.ok() || st.inProgress() ? ++ok : ++fail;
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  printf("Stress: ok=%lu fail=%lu\n", static_cast<unsigned long>(ok),
         static_cast<unsigned long>(fail));
  printHealth();
}

void processCommand(char* line) {
  char original[INPUT_MAX];
  strncpy(original, line, sizeof(original) - 1);
  original[sizeof(original) - 1] = '\0';

  char* save = nullptr;
  char* cmd = strtok_r(line, " \t", &save);
  if (cmd == nullptr) return;
  lowerInPlace(cmd);

  if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
    printHelp();
  } else if (strcmp(cmd, "version") == 0) {
    printVersionInfo();
  } else if (strcmp(cmd, "scan") == 0) {
    scanI2c();
  } else if (strcmp(cmd, "probe") == 0) {
    printStatus(display.probe());
  } else if (strcmp(cmd, "recover") == 0) {
    const SSD1315::Status st = display.recover();
    if (st.ok()) {
      gScrollActive = false;
    }
    printStatus(st);
  } else if (strcmp(cmd, "drv") == 0 || strcmp(cmd, "health") == 0 || strcmp(cmd, "read") == 0) {
    printHealth();
  } else if (strcmp(cmd, "monitor") == 0) {
    char* arg = nextToken(&save);
    if (arg == nullptr) {
      monitorMode = !monitorMode;
    } else {
      lowerInPlace(arg);
      bool requested = false;
      uint32_t interval = 0;
      if (parseBool(arg, requested)) {
        monitorMode = requested;
        if (monitorMode) {
          monitorIntervalMs = 1000U;
        }
      } else if (parseU32(arg, interval)) {
        monitorMode = interval != 0U;
        if (monitorMode) {
          monitorIntervalMs = interval;
        }
      } else {
        puts("Usage: monitor [0|1|ms]");
        return;
      }
    }
    monitorNextMs = 0;
    printf("Monitor: %s interval=%lums\n",
           monitorMode ? "ON" : "OFF",
           static_cast<unsigned long>(monitorIntervalMs));
  } else if (strcmp(cmd, "reset") == 0) {
    display.end();
    const SSD1315::Status st = display.begin(makeConfig());
    if (st.ok()) {
      gScrollActive = false;
    }
    printStatus(st);
  } else if (strcmp(cmd, "cfg") == 0) {
    SSD1315::SettingsSnapshot s = display.getSettings();
    printf("Config: addr=0x%02X geometry=%ux%u pages=%u pageBuffer=%u\n",
           s.i2cAddress, s.width, s.height, s.totalPages, s.pageBufferPages);
    printf("Config: controllerProfile=%s panelProfile=%s\n",
           controllerProfileToStr(s.controllerProfile), HIL_PANEL_PROFILE);
    printf("Config: timeout=%lu flushTimeout=%lu budget=%u contrast=%u\n",
           static_cast<unsigned long>(s.i2cTimeoutMs),
           static_cast<unsigned long>(s.flushTimeoutMs),
           s.byteBudgetPerTick, s.contrast);
    printf("Config: clearOnBegin=%s clearOnRecover=%s scrollActive=%s\n",
           s.clearOnBegin ? "yes" : "no",
           s.clearOnRecover ? "yes" : "no",
           gScrollActive ? "yes" : "no");
    printf("Config: initialized=%s flush=%s dirty=0x%02X controlDirty=%s invert=%s flipx=%s flipy=%s\n",
           s.initialized ? "yes" : "no",
           s.flushing ? "yes" : "no",
           s.dirtyPages,
           s.controlStateDirty ? "yes" : "no",
           s.invert ? "yes" : "no",
           s.flipX ? "yes" : "no",
           s.flipY ? "yes" : "no");
    if (s.controlStateDirty) {
      printStatus(s.controlStateError);
    }
  } else if (strcmp(cmd, "contrast") == 0) {
    uint32_t value = 0;
    if (!parseU32(nextToken(&save), value) || value == 0U || value > 255U) {
      puts("Usage: contrast <1..255>");
    } else {
      printStatus(display.setContrast(static_cast<uint8_t>(value)));
    }
  } else if (strcmp(cmd, "display") == 0) {
    char* arg = nextToken(&save);
    if (arg != nullptr) lowerInPlace(arg);
    if (arg == nullptr) {
      puts("Usage: display <off|on>");
    } else if (strcmp(arg, "off") == 0) {
      printStatus(display.setSleep(true));
    } else if (strcmp(arg, "on") == 0) {
      printStatus(display.setSleep(false));
    } else {
      puts("Usage: display <off|on>");
    }
  } else if (strcmp(cmd, "invert") == 0 || strcmp(cmd, "flipx") == 0 || strcmp(cmd, "flipy") == 0 ||
             strcmp(cmd, "sleep") == 0 || strcmp(cmd, "allon") == 0 || strcmp(cmd, "zoom") == 0) {
    char* arg = nextToken(&save);
    if (arg != nullptr) lowerInPlace(arg);
    bool value = false;
    if (!parseBool(arg, value)) {
      printf("Usage: %s <0|1>\n", cmd);
      return;
    }
    if (strcmp(cmd, "invert") == 0) printStatus(display.setInvert(value));
    if (strcmp(cmd, "flipx") == 0) printStatus(display.setFlipX(value));
    if (strcmp(cmd, "flipy") == 0) printStatus(display.setFlipY(value));
    if (strcmp(cmd, "sleep") == 0) printStatus(display.setSleep(value));
    if (strcmp(cmd, "allon") == 0) printStatus(display.setAllPixelsOn(value));
    if (strcmp(cmd, "zoom") == 0) printStatus(display.setZoom(value));
  } else if (strcmp(cmd, "fade") == 0) {
    char* mode = nextToken(&save);
    uint32_t interval = 0; parseU32(nextToken(&save), interval);
    SSD1315::FadeMode fade = SSD1315::FadeMode::OFF;
    if (mode != nullptr && strcmp(mode, "out") == 0) fade = SSD1315::FadeMode::FADE_OUT;
    if (mode != nullptr && strcmp(mode, "blink") == 0) fade = SSD1315::FadeMode::BLINK;
    printStatus(display.setFadeMode(fade, static_cast<uint8_t>(interval)));
  } else if (strcmp(cmd, "scrollh") == 0) {
    char* dir = nextToken(&save);
    uint32_t start = 0, end = 0;
    bool left = false;
    SSD1315::ScrollSpeed speed = SSD1315::ScrollSpeed::FRAMES_5;
    if (!parseScrollDirection(dir, left) ||
        !parseU32(nextToken(&save), start) ||
        !parseU32(nextToken(&save), end) ||
        !parseScrollSpeed(nextToken(&save), speed)) {
      puts("Usage: scrollh <left|right> <startPage> <endPage> [speed 0..7]");
    } else if (start > end || end > 7U) {
      puts("scrollh pages must satisfy 0<=start<=end<=7");
    } else {
      const SSD1315::Status st = display.startHorizontalScroll(left,
                                                               static_cast<uint8_t>(start),
                                                               static_cast<uint8_t>(end),
                                                               speed);
      if (st.ok()) {
        gScrollActive = true;
      }
      printStatus(st);
    }
  } else if (strcmp(cmd, "scrollv") == 0) {
    char* dir = nextToken(&save);
    uint32_t start = 0, end = 0, offset = 0;
    bool left = false;
    SSD1315::ScrollSpeed speed = SSD1315::ScrollSpeed::FRAMES_5;
    if (!parseScrollDirection(dir, left) ||
        !parseU32(nextToken(&save), start) ||
        !parseU32(nextToken(&save), end) ||
        !parseU32(nextToken(&save), offset) ||
        !parseScrollSpeed(nextToken(&save), speed)) {
      puts("Usage: scrollv <left|right> <startPage> <endPage> <offset 0..63> [speed 0..7]");
    } else if (start > end || end > 7U) {
      puts("scrollv pages must satisfy 0<=start<=end<=7");
    } else if (offset > 63U) {
      puts("scrollv offset must be 0..63");
    } else {
      const SSD1315::Status st = display.startVerticalScroll(left,
                                                             static_cast<uint8_t>(start),
                                                             static_cast<uint8_t>(end),
                                                             speed,
                                                             static_cast<uint8_t>(offset));
      if (st.ok()) {
        gScrollActive = true;
      }
      printStatus(st);
    }
  } else if (strcmp(cmd, "scroll") == 0) {
    char* sub = nextToken(&save);
    if (sub != nullptr) lowerInPlace(sub);
    if (sub != nullptr && strcmp(sub, "stop") == 0) {
      const SSD1315::Status st = display.stopScroll();
      if (st.ok()) {
        gScrollActive = false;
      }
      printStatus(st);
    } else {
      puts("Usage: scroll stop");
    }
  } else if (strcmp(cmd, "scrollstop") == 0) {
    const SSD1315::Status st = display.stopScroll();
    if (st.ok()) {
      gScrollActive = false;
    }
    printStatus(st);
  } else if (strcmp(cmd, "text") == 0) {
    int32_t x = 0, y = 0;
    char* xs = nextToken(&save); char* ys = nextToken(&save);
    if (!parseI32(xs, x) || !parseI32(ys, y)) return;
    char* text = original;
    for (int spaces = 0; *text != '\0' && spaces < 3; ++text) if (*text == ' ') ++spaces;
    display.drawText(static_cast<int16_t>(x), static_cast<int16_t>(y), text);
    requestAndWaitFlush();
  } else if (strcmp(cmd, "clear") == 0) {
    display.clear(); requestAndWaitFlush();
  } else if (strcmp(cmd, "fill") == 0) {
    display.fill(); requestAndWaitFlush();
  } else if (strcmp(cmd, "pattern") == 0) {
    char* pattern = nextToken(&save);
    if (pattern != nullptr) lowerInPlace(pattern);
    if (pattern != nullptr && strcmp(pattern, "checker") == 0) {
      display.fillCheckerboard(8);
      requestAndWaitFlush();
    } else if (pattern != nullptr && strcmp(pattern, "vstripes") == 0) {
      display.fillVerticalStripes(4);
      requestAndWaitFlush();
    } else if (pattern != nullptr && strcmp(pattern, "hstripes") == 0) {
      display.fillHorizontalStripes(4);
      requestAndWaitFlush();
    } else {
      puts("Usage: pattern <checker|vstripes|hstripes>");
    }
  } else if (strcmp(cmd, "line") == 0) {
    int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    if (parseI32(nextToken(&save), x0) && parseI32(nextToken(&save), y0) &&
        parseI32(nextToken(&save), x1) && parseI32(nextToken(&save), y1)) {
      display.drawLine(x0, y0, x1, y1); requestAndWaitFlush();
    }
  } else if (strcmp(cmd, "rect") == 0) {
    int32_t x = 0, y = 0, w = 0, h = 0;
    if (parseI32(nextToken(&save), x) && parseI32(nextToken(&save), y) &&
        parseI32(nextToken(&save), w) && parseI32(nextToken(&save), h)) {
      display.drawRect(x, y, w, h); requestAndWaitFlush();
    }
  } else if (strcmp(cmd, "circle") == 0) {
    int32_t x = 0, y = 0, r = 0;
    if (parseI32(nextToken(&save), x) && parseI32(nextToken(&save), y) &&
        parseI32(nextToken(&save), r)) {
      display.drawCircle(x, y, r); requestAndWaitFlush();
    }
  } else if (strcmp(cmd, "pixel") == 0) {
    int32_t x = 0, y = 0; bool on = true;
    if (parseI32(nextToken(&save), x) && parseI32(nextToken(&save), y)) {
      parseBool(nextToken(&save), on);
      display.setPixel(x, y, on); requestAndWaitFlush();
    }
  } else if (strcmp(cmd, "flush") == 0) {
    requestAndWaitFlush();
  } else if (strcmp(cmd, "flushrect") == 0) {
    int32_t x = 0, y = 0, w = 0, h = 0;
    if (parseI32(nextToken(&save), x) && parseI32(nextToken(&save), y) &&
        parseI32(nextToken(&save), w) && parseI32(nextToken(&save), h)) {
      printStatus(display.requestFlushRect(x, y, w, h));
    }
  } else if (strcmp(cmd, "demo") == 0) {
    drawDemo();
  } else if (strcmp(cmd, "stress") == 0 || strcmp(cmd, "stress_mix") == 0) {
    uint32_t count = strcmp(cmd, "stress_mix") == 0 ? 50 : 10;
    parseU32(nextToken(&save), count);
    runStress(count, strcmp(cmd, "stress_mix") == 0);
  } else if (strcmp(cmd, "selftest") == 0) {
    puts("Selftest:");
    printStatus(display.probe());
    printStatus(display.setContrast(0x7F));
    printStatus(display.setInvert(true));
    printStatus(display.setInvert(false));
    drawDemo();
  } else {
    printf("Unknown command: %s\n", cmd);
  }
}

void cliLoop() {
  static char input[INPUT_MAX];
  size_t len = 0;
  printf("> ");
  while (true) {
    const uint32_t now = transport::nowMs(nullptr);
    display.tick(now);
    if (monitorMode &&
        (monitorNextMs == 0 || static_cast<int32_t>(now - monitorNextMs) >= 0)) {
      printHealth();
      monitorNextMs = now + monitorIntervalMs;
    }
    const int c = getchar();
    if (c == EOF) {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }
    if (c == '\b' || c == 0x7F) {
      if (len > 0) --len;
      continue;
    }
    if (c == '\n' || c == '\r') {
      if (len > 0) {
        input[len] = '\0';
        processCommand(input);
        len = 0;
        printf("> ");
      }
      continue;
    }
    if (len < sizeof(input) - 1) input[len++] = static_cast<char>(c);
  }
}

}  // namespace

extern "C" void app_main(void) {
  setvbuf(stdin, nullptr, _IONBF, 0);
  setvbuf(stdout, nullptr, _IONBF, 0);
  configureNonBlockingStdin();

  puts("=== SSD1315 native ESP-IDF bringup ===");
  if (!transport::initWire(I2C_SDA, I2C_SCL, I2C_FREQ_HZ, I2C_TIMEOUT_MS, I2C_ADDRESS)) {
    printf("I2C init failed: %d\n", static_cast<int>(transport::lastInitError()));
    transport::deinitWire();
    return;
  }
  scanI2c();
  const SSD1315::Status beginStatus = display.begin(makeConfig());
  printStatus(beginStatus);
  if (beginStatus.ok()) {
    drawDemo();
  } else {
    puts("Display begin failed; CLI remains available for scan/reset diagnostics.");
  }
  puts("Type 'help' for commands.");
  cliLoop();
}
