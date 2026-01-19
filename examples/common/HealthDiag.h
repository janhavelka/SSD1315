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

#include "ssd1315/Ssd1315.h"
#include "ssd1315/Status.h"
#include "examples/common/Log.h"

namespace diag {

/**
 * @brief Convert DriverState enum to human-readable string.
 */
inline const char* stateToString(ssd1315::DriverState state) {
  switch (state) {
    case ssd1315::DriverState::UNINIT:   return "UNINIT";
    case ssd1315::DriverState::READY:    return "READY";
    case ssd1315::DriverState::DEGRADED: return "DEGRADED";
    case ssd1315::DriverState::OFFLINE:  return "OFFLINE";
    default:                              return "UNKNOWN";
  }
}

/**
 * @brief Convert Err enum to human-readable string.
 */
inline const char* errToString(ssd1315::Err err) {
  switch (err) {
    case ssd1315::Err::OK:                return "OK";
    case ssd1315::Err::INVALID_CONFIG:    return "INVALID_CONFIG";
    case ssd1315::Err::INVALID_DIMENSIONS: return "INVALID_DIMENSIONS";
    case ssd1315::Err::INVALID_PAGE_COUNT: return "INVALID_PAGE_COUNT";
    case ssd1315::Err::NOT_INITIALIZED:   return "NOT_INITIALIZED";
    case ssd1315::Err::STATE_ERROR:       return "STATE_ERROR";
    case ssd1315::Err::BUSY:              return "BUSY";
    case ssd1315::Err::PANEL_NOT_READY:   return "PANEL_NOT_READY";
    case ssd1315::Err::I2C_NACK_ADDR:     return "I2C_NACK_ADDR";
    case ssd1315::Err::I2C_NACK_DATA:     return "I2C_NACK_DATA";
    case ssd1315::Err::I2C_TIMEOUT:       return "I2C_TIMEOUT";
    case ssd1315::Err::I2C_BUS_ERROR:     return "I2C_BUS_ERROR";
    case ssd1315::Err::TIMEOUT:           return "TIMEOUT";
    case ssd1315::Err::BUFFER_OVERFLOW:   return "BUFFER_OVERFLOW";
    case ssd1315::Err::UNSUPPORTED:       return "UNSUPPORTED";
    case ssd1315::Err::INTERNAL_ERROR:    return "INTERNAL_ERROR";
    case ssd1315::Err::DEVICE_NOT_FOUND:  return "DEVICE_NOT_FOUND";
    case ssd1315::Err::IN_PROGRESS:       return "IN_PROGRESS";
    default:                               return "UNKNOWN";
  }
}

/**
 * @brief Get state color indicator for terminal output.
 * @return ANSI color code string (green/yellow/red/gray)
 */
inline const char* stateColor(ssd1315::DriverState state) {
  switch (state) {
    case ssd1315::DriverState::READY:    return "\033[32m";  // Green
    case ssd1315::DriverState::DEGRADED: return "\033[33m";  // Yellow
    case ssd1315::DriverState::OFFLINE:  return "\033[31m";  // Red
    case ssd1315::DriverState::UNINIT:   return "\033[90m";  // Gray
    default:                              return "\033[0m";   // Reset
  }
}

/// Reset ANSI color
inline const char* colorReset() { return "\033[0m"; }

/**
 * @brief Print a compact one-line health summary.
 */
inline void printHealthOneLine(ssd1315::Ssd1315& display) {
  ssd1315::DriverState st = display.state();
  LOGI("Health: %s%s%s | Online:%d | ConsecFail:%u | Total OK:%lu Fail:%lu",
       stateColor(st), stateToString(st), colorReset(),
       display.isOnline() ? 1 : 0,
       display.consecutiveFailures(),
       (unsigned long)display.totalSuccess(),
       (unsigned long)display.totalFailures());
}

/**
 * @brief Print detailed verbose health diagnostics.
 */
inline void printHealthVerbose(ssd1315::Ssd1315& display) {
  ssd1315::DriverState st = display.state();
  ssd1315::Status lastErr = display.lastError();
  uint32_t now = millis();
  
  LOG_SERIAL.println();
  LOGI("╔══════════════════════════════════════════════════════════════╗");
  LOGI("║              SSD1315 DRIVER HEALTH DIAGNOSTICS               ║");
  LOGI("╠══════════════════════════════════════════════════════════════╣");
  
  // State with color
  LOGI("║ Driver State:    %s%-10s%s                                  ║",
       stateColor(st), stateToString(st), colorReset());
  LOGI("║ Is Online:       %-5s                                       ║",
       display.isOnline() ? "YES" : "NO");
  
  LOGI("╠══════════════════════════════════════════════════════════════╣");
  LOGI("║                      HEALTH COUNTERS                         ║");
  LOGI("╠══════════════════════════════════════════════════════════════╣");
  
  LOGI("║ Consecutive Failures:  %-6u                                ║",
       display.consecutiveFailures());
  LOGI("║ Total Successes:       %-10lu                            ║",
       (unsigned long)display.totalSuccess());
  LOGI("║ Total Failures:        %-10lu                            ║",
       (unsigned long)display.totalFailures());
  
  // Calculate success rate
  uint32_t total = display.totalSuccess() + display.totalFailures();
  float successRate = (total > 0) ? (100.0f * display.totalSuccess() / total) : 0.0f;
  LOGI("║ Success Rate:          %.1f%%                                 ║",
       successRate);
  
  LOGI("╠══════════════════════════════════════════════════════════════╣");
  LOGI("║                        TIMESTAMPS                            ║");
  LOGI("╠══════════════════════════════════════════════════════════════╣");
  
  uint32_t lastOk = display.lastOkMs();
  uint32_t lastFail = display.lastErrorMs();
  
  if (lastOk > 0) {
    LOGI("║ Last Success:    %lu ms ago (at %lu ms)                     ║",
         (unsigned long)(now - lastOk), (unsigned long)lastOk);
  } else {
    LOGI("║ Last Success:    Never                                      ║");
  }
  
  if (lastFail > 0) {
    LOGI("║ Last Failure:    %lu ms ago (at %lu ms)                     ║",
         (unsigned long)(now - lastFail), (unsigned long)lastFail);
  } else {
    LOGI("║ Last Failure:    Never                                      ║");
  }
  
  LOGI("╠══════════════════════════════════════════════════════════════╣");
  LOGI("║                      LAST ERROR                              ║");
  LOGI("╠══════════════════════════════════════════════════════════════╣");
  
  if (lastErr.ok()) {
    LOGI("║ Last Error:      None                                       ║");
  } else {
    LOGI("║ Error Code:      %-20s                       ║", errToString(lastErr.code));
    LOGI("║ Error Detail:    %ld                                        ║", (long)lastErr.detail);
    LOGI("║ Error Message:   %-40s ║", lastErr.msg ? lastErr.msg : "(null)");
  }
  
  LOGI("╚══════════════════════════════════════════════════════════════╝");
  LOG_SERIAL.println();
}

/**
 * @brief Print health changes with before/after comparison.
 */
struct HealthSnapshot {
  ssd1315::DriverState state;
  uint8_t consecutiveFailures;
  uint32_t totalSuccess;
  uint32_t totalFailures;
  uint32_t timestamp;
  
  void capture(ssd1315::Ssd1315& display) {
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
    LOGI("  State: %s%s%s → %s%s%s",
         stateColor(before.state), stateToString(before.state), colorReset(),
         stateColor(after.state), stateToString(after.state), colorReset());
    changed = true;
  }
  
  if (before.consecutiveFailures != after.consecutiveFailures) {
    LOGI("  ConsecFail: %u → %u", before.consecutiveFailures, after.consecutiveFailures);
    changed = true;
  }
  
  if (before.totalSuccess != after.totalSuccess) {
    LOGI("  TotalOK: %lu → %lu (+%lu)",
         (unsigned long)before.totalSuccess,
         (unsigned long)after.totalSuccess,
         (unsigned long)(after.totalSuccess - before.totalSuccess));
    changed = true;
  }
  
  if (before.totalFailures != after.totalFailures) {
    LOGI("  TotalFail: %lu → %lu (+%lu)",
         (unsigned long)before.totalFailures,
         (unsigned long)after.totalFailures,
         (unsigned long)(after.totalFailures - before.totalFailures));
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
    _lastState = ssd1315::DriverState::UNINIT;
    _lastConsecFail = 0;
  }
  
  /**
   * @brief Check and optionally log health changes.
   * @param display Display instance to monitor
   * @param forceLog If true, always log even if no change
   */
  void tick(ssd1315::Ssd1315& display, bool forceLog = false) {
    uint32_t now = millis();
    ssd1315::DriverState currentState = display.state();
    uint8_t currentFail = display.consecutiveFailures();
    
    bool stateChanged = (currentState != _lastState);
    bool failChanged = (currentFail != _lastConsecFail);
    bool intervalElapsed = (_intervalMs > 0 && (now - _lastLogMs >= _intervalMs));
    
    if (stateChanged || failChanged || intervalElapsed || forceLog) {
      if (stateChanged) {
        LOGI("[HEALTH] State transition: %s%s%s → %s%s%s",
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
  ssd1315::DriverState _lastState = ssd1315::DriverState::UNINIT;
  uint8_t _lastConsecFail = 0;
};

}  // namespace diag
