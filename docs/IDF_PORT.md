# SSD1315 ESP-IDF Portability Status

Last audited: 2026-03-01

## Current Reality
- Primary runtime remains PlatformIO + Arduino.
- I2C transport is callback-based (`Config.i2cWrite`).
- Optional timing hooks are available:
  - `Config.nowMs`
  - `Config.cooperativeYield`
  - `Config.timeUser`
- Core logic uses wrappers (`_nowMs`, `_cooperativeYield`) for waits/timeouts.
- Arduino calls are only fallback paths inside wrappers:
  - `_nowMs()` -> `millis()` when `Config.nowMs == nullptr`
  - `_cooperativeYield()` -> `yield()` when `Config.cooperativeYield == nullptr`

## ESP-IDF Adapter Requirements
To run under pure ESP-IDF, provide:
1. I2C write callback.
2. Optional timing callbacks (`nowMs`, `cooperativeYield`).

## Minimal Adapter Pattern
```cpp
static uint32_t idfNowMs(void*) {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

static void idfYield(void*) {
  taskYIELD();
}

SSD1315::Config cfg{};
cfg.i2cWrite = myI2cWrite;
cfg.nowMs = idfNowMs;
cfg.cooperativeYield = idfYield;
```

## Porting Notes
- Keep calling `tick(nowMs)` at regular cadence.
- Keep timeout behavior (`i2cTimeoutMs`, `flushTimeoutMs`) unchanged in adapter implementation.
- Polling paths rely on cooperative yield hook for scheduler friendliness.

## Verification Checklist
- `python tools/check_core_timing_guard.py` passes.
- Native tests pass (`pio test -e native`).
- Example builds pass on S2/S3 (`pio run -e esp32s2dev`, `pio run -e esp32s3dev`).
- No direct Arduino timing calls are introduced outside fallback wrappers.
