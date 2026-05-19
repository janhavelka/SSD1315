/**
 * @file HealthDiag.h
 * @brief Verbose health diagnostic helpers for SSD1315 examples.
 *
 * Provides detailed logging of driver health state, counters, and
 * state transitions for debugging and monitoring.
 *
 * NOT part of the library API. Example-only.
 */

#pragma once

#include <Arduino.h>

#include "ssd1315/SSD1315.h"
#include "ssd1315/Status.h"
#include "examples/common/Log.h"

namespace diag {

/**
 * @brief Convert DriverState enum to human-readable string.
 */
inline const char* stateToString(SSD1315::DriverState state) {
  switch (state) {
    case SSD1315::DriverState::UNINIT:   return "UNINIT";
    case SSD1315::DriverState::READY:    return "READY";
    case SSD1315::DriverState::DEGRADED: return "DEGRADED";
    case SSD1315::DriverState::OFFLINE:  return "OFFLINE";
    default:                              return "UNKNOWN";
  }
}

/**
 * @brief Convert Err enum to human-readable string.
 */
inline const char* errToString(SSD1315::Err err) {
  switch (err) {
    case SSD1315::Err::OK:                return "OK";
    case SSD1315::Err::INVALID_CONFIG:    return "INVALID_CONFIG";
    case SSD1315::Err::INVALID_DIMENSIONS: return "INVALID_DIMENSIONS";
    case SSD1315::Err::INVALID_PAGE_COUNT: return "INVALID_PAGE_COUNT";
    case SSD1315::Err::NOT_INITIALIZED:   return "NOT_INITIALIZED";
    case SSD1315::Err::STATE_ERROR:       return "STATE_ERROR";
    case SSD1315::Err::BUSY:              return "BUSY";
    case SSD1315::Err::PANEL_NOT_READY:   return "PANEL_NOT_READY";
    case SSD1315::Err::I2C_NACK_ADDR:     return "I2C_NACK_ADDR";
    case SSD1315::Err::I2C_NACK_DATA:     return "I2C_NACK_DATA";
    case SSD1315::Err::I2C_TIMEOUT:       return "I2C_TIMEOUT";
    case SSD1315::Err::I2C_BUS_ERROR:     return "I2C_BUS_ERROR";
    case SSD1315::Err::TIMEOUT:           return "TIMEOUT";
    case SSD1315::Err::BUFFER_OVERFLOW:   return "BUFFER_OVERFLOW";
    case SSD1315::Err::UNSUPPORTED:       return "UNSUPPORTED";
    case SSD1315::Err::INTERNAL_ERROR:    return "INTERNAL_ERROR";
    case SSD1315::Err::DEVICE_NOT_FOUND:  return "DEVICE_NOT_FOUND";
    case SSD1315::Err::IN_PROGRESS:       return "IN_PROGRESS";
    default:                               return "UNKNOWN";
  }
}

/**
 * @brief Get state color indicator for terminal output.
 * @return ANSI color code string (green/yellow/red/gray)
 */
inline const char* stateColor(SSD1315::DriverState state) {
  switch (state) {
    case SSD1315::DriverState::READY:    return LOG_COLOR_GREEN;
    case SSD1315::DriverState::DEGRADED: return LOG_COLOR_YELLOW;
    case SSD1315::DriverState::OFFLINE:  return LOG_COLOR_RED;
    case SSD1315::DriverState::UNINIT:   return LOG_COLOR_GRAY;
    default:                              return LOG_COLOR_RESET;
  }
}

/// Reset ANSI color
inline const char* colorReset() { return LOG_COLOR_RESET; }

inline const char* boolColor(bool value) {
  return value ? LOG_COLOR_GREEN : LOG_COLOR_RED;
}

inline const char* successRateColor(float pct) {
  if (pct >= 99.9f) return LOG_COLOR_GREEN;
  if (pct >= 80.0f) return LOG_COLOR_YELLOW;
  return LOG_COLOR_RED;
}

inline const char* failureCountColor(uint32_t failures) {
  if (failures == 0U) return LOG_COLOR_GREEN;
  if (failures < 3U) return LOG_COLOR_YELLOW;
  return LOG_COLOR_RED;
}

inline const char* successCountColor(uint32_t successes) {
  return (successes > 0U) ? LOG_COLOR_GREEN : LOG_COLOR_GRAY;
}

inline const char* totalFailureColor(uint32_t failures) {
  return (failures == 0U) ? LOG_COLOR_GREEN : LOG_COLOR_RED;
}

/**
 * @brief Print a compact one-line health summary.
 */
inline void printHealthOneLine(SSD1315::SSD1315& display) {
  SSD1315::DriverState st = display.state();
  LOGI("Health: state=%s%s%s online=%s%s%s consecFail=%s%u%s ok=%s%lu%s fail=%s%lu%s",
       stateColor(st), stateToString(st), colorReset(),
       boolColor(display.isOnline()), display.isOnline() ? "true" : "false", colorReset(),
       failureCountColor(display.consecutiveFailures()), display.consecutiveFailures(), colorReset(),
       successCountColor(display.totalSuccess()), (unsigned long)display.totalSuccess(), colorReset(),
       totalFailureColor(display.totalFailures()), (unsigned long)display.totalFailures(), colorReset());
}

/**
 * @brief Print detailed verbose health diagnostics.
 */
inline void printHealthVerbose(SSD1315::SSD1315& display) {
  SSD1315::DriverState st = display.state();
  SSD1315::Status lastErr = display.lastError();
  uint32_t now = millis();

  const bool online = display.isOnline();
  const uint32_t totalSuccess = display.totalSuccess();
  const uint32_t totalFailures = display.totalFailures();
  uint32_t total = display.totalSuccess() + display.totalFailures();
  float successRate = (total > 0) ? (100.0f * display.totalSuccess() / total) : 0.0f;

  LOG_SERIAL.println();
  LOGI("=== Driver Health ===");
  LOGI("  State: %s%s%s", stateColor(st), stateToString(st), colorReset());
  LOGI("  Online: %s%s%s", boolColor(online), online ? "true" : "false", colorReset());
  LOGI("  Consecutive failures: %s%u%s",
       failureCountColor(display.consecutiveFailures()),
       display.consecutiveFailures(),
       colorReset());
  LOGI("  Total success: %s%lu%s",
       successCountColor(totalSuccess),
       (unsigned long)totalSuccess,
       colorReset());
  LOGI("  Total failures: %s%lu%s",
       totalFailureColor(totalFailures),
       (unsigned long)totalFailures,
       colorReset());
  LOGI("  Success rate: %s%.1f%%%s", successRateColor(successRate), successRate, colorReset());

  LOGI("=== Timestamps ===");
  
  uint32_t lastOk = display.lastOkMs();
  uint32_t lastFail = display.lastErrorMs();
  
  if (lastOk > 0) {
    LOGI("  Last success: %lu ms ago (at %lu ms)",
         (unsigned long)(now - lastOk), (unsigned long)lastOk);
  } else {
    LOGI("  Last success: never");
  }
  
  if (lastFail > 0) {
    LOGI("  Last failure: %lu ms ago (at %lu ms)",
         (unsigned long)(now - lastFail), (unsigned long)lastFail);
  } else {
    LOGI("  Last failure: never");
  }
  LOGI("=== Last Error ===");
  
  if (lastErr.ok()) {
    LOGI("  Last error: %snone%s", LOG_COLOR_GREEN, colorReset());
  } else {
    LOGI("  Code: %s%s%s", LOG_COLOR_RED, errToString(lastErr.code), colorReset());
    LOGI("  Detail: %ld", (long)lastErr.detail);
    LOGI("  Message: %s", lastErr.msg ? lastErr.msg : "(null)");
  }
  LOG_SERIAL.println();
}

/**
 * @brief Print health changes with before/after comparison.
 */
struct HealthSnapshot {
  SSD1315::DriverState state;
  uint8_t consecutiveFailures;
  uint32_t totalSuccess;
  uint32_t totalFailures;
  uint32_t timestamp;
  
  void capture(SSD1315::SSD1315& display) {
    state = display.state();
    consecutiveFailures = display.consecutiveFailures();
    totalSuccess = display.totalSuccess();
    totalFailures = display.totalFailures();
    timestamp = millis();
  }
};

/**
 * @brief Compare two snapshots and print differences.
 */
inline void printHealthDiff(const HealthSnapshot& before, const HealthSnapshot& after) {
  bool changed = false;
  
  if (before.state != after.state) {
    LOGI("  State: %s%s%s -> %s%s%s",
         stateColor(before.state), stateToString(before.state), colorReset(),
         stateColor(after.state), stateToString(after.state), colorReset());
    changed = true;
  }
  
  if (before.consecutiveFailures != after.consecutiveFailures) {
    const bool improved = after.consecutiveFailures < before.consecutiveFailures;
    const char* color = improved ? LOG_COLOR_GREEN : LOG_COLOR_RED;
    LOGI("  ConsecFail: %s%u -> %u%s",
         color,
         before.consecutiveFailures,
         after.consecutiveFailures,
         colorReset());
    changed = true;
  }
  
  if (before.totalSuccess != after.totalSuccess) {
    LOGI("  TotalOK: %lu -> %s%lu (+%lu)%s",
         (unsigned long)before.totalSuccess,
         LOG_COLOR_GREEN,
         (unsigned long)after.totalSuccess,
         (unsigned long)(after.totalSuccess - before.totalSuccess),
         colorReset());
    changed = true;
  }
  
  if (before.totalFailures != after.totalFailures) {
    LOGI("  TotalFail: %lu -> %s%lu (+%lu)%s",
         (unsigned long)before.totalFailures,
         LOG_COLOR_RED,
         (unsigned long)after.totalFailures,
         (unsigned long)(after.totalFailures - before.totalFailures),
         colorReset());
    changed = true;
  }
  
  if (!changed) {
    LOGI("  (no changes)");
  }
}

/**
 * @brief Continuously monitor health with periodic logging.
 * Call from loop() for real-time monitoring.
 */
class HealthMonitor {
public:
  /**
   * @brief Initialize monitor with logging interval.
   * @param intervalMs How often to log (0 = only on change)
   */
  void begin(uint32_t intervalMs = 1000) {
    _intervalMs = intervalMs;
    _lastLogMs = 0;
    _lastState = SSD1315::DriverState::UNINIT;
    _lastConsecFail = 0;
  }
  
  /**
   * @brief Check and optionally log health changes.
   * @param display Display instance to monitor
   * @param forceLog If true, always log even if no change
   */
  void tick(SSD1315::SSD1315& display, bool forceLog = false) {
    uint32_t now = millis();
    SSD1315::DriverState currentState = display.state();
    uint8_t currentFail = display.consecutiveFailures();
    
    bool stateChanged = (currentState != _lastState);
    bool failChanged = (currentFail != _lastConsecFail);
    bool intervalElapsed = (_intervalMs > 0 && (now - _lastLogMs >= _intervalMs));
    
    if (stateChanged || failChanged || intervalElapsed || forceLog) {
      if (stateChanged) {
        LOGI("[HEALTH] State transition: %s%s%s -> %s%s%s",
             stateColor(_lastState), stateToString(_lastState), colorReset(),
             stateColor(currentState), stateToString(currentState), colorReset());
      }
      
      if (intervalElapsed || forceLog) {
        printHealthOneLine(display);
      }
      
      _lastState = currentState;
      _lastConsecFail = currentFail;
      _lastLogMs = now;
    }
  }
  
private:
  uint32_t _intervalMs = 1000;
  uint32_t _lastLogMs = 0;
  SSD1315::DriverState _lastState = SSD1315::DriverState::UNINIT;
  uint8_t _lastConsecFail = 0;
};

}  // namespace diag
