# SSD1315 ESP-IDF Port Guide

This library is maintained with PlatformIO + Arduino as the primary target.
The core API is now portability-oriented and can be ported to pure ESP-IDF with a thin adapter layer.

## Current Canonical API
- Public include: `#include <SSD1315.h>`
- Namespace: `SSD1315`
- Driver type: `SSD1315::SSD1315`

## Porting Strategy
1. Keep the library core unchanged.
2. Implement only transport/time adapters for ESP-IDF.
3. Preserve behavior parity with Arduino build (timeouts, recovery, health counters).

## Required Adapters
### I2C write callback
Provide `Config::i2cWrite` mapped to ESP-IDF I2C master transmit.

### Timing callbacks
Provide `Config::nowMs` and optionally `Config::cooperativeYield`.
- `nowMs`: use `esp_timer_get_time() / 1000`.
- `cooperativeYield`: use `taskYIELD()` if needed.

## Minimal ESP-IDF Callback Sketch
```cpp
#include "driver/i2c_master.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <SSD1315.h>

static i2c_master_dev_handle_t gDev = nullptr;

static SSD1315::Status idfI2cWrite(uint8_t, const uint8_t* data, size_t len,
                                   uint32_t timeoutMs, void*) {
  const TickType_t ticks = pdMS_TO_TICKS(timeoutMs == 0 ? 1 : timeoutMs);
  const esp_err_t rc = i2c_master_transmit(gDev, data, len, ticks);
  if (rc == ESP_OK) return SSD1315::Ok();
  if (rc == ESP_ERR_TIMEOUT) return SSD1315::Error(SSD1315::Err::I2C_TIMEOUT, rc, "i2c timeout");
  return SSD1315::Error(SSD1315::Err::I2C_BUS_ERROR, rc, "i2c transmit failed");
}

static uint32_t idfNowMs(void*) {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

static void idfYield(void*) {
  taskYIELD();
}
```

## Validation Checklist
- Build succeeds with ESP-IDF component integration.
- No direct Arduino calls required in your adapter path.
- Driver state transitions match Arduino behavior (`UNINIT/READY/DEGRADED/OFFLINE`).
- Recovery and timeout behavior matches existing tests.

## Notes
- Portability changes must never reduce runtime stability or diagnostics quality.
- Keep API and health semantics aligned with the unified I2C contract.
