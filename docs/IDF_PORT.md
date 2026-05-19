# SSD1315 ESP-IDF v6.0.1 Port

Scope: keep the SSD1315 display driver usable from both Arduino/PlatformIO and
pure ESP-IDF while preserving the existing callback API and Arduino example
functionality.

## Result

- Core driver I2C remains framework-neutral. All bus access goes through
  `Config::i2cWrite`; `Config::i2cWriteRead` remains available for uniform
  application wiring.
- `src/SSD1315.cpp` no longer requires Arduino headers for ESP-IDF builds.
- If `Config::nowMs` is not supplied, Arduino/native-test builds use
  `millis()` and ESP-IDF builds use `esp_timer_get_time() / 1000`.
- If `Config::cooperativeYield` is not supplied, Arduino/native-test builds use
  `yield()` and ESP-IDF builds use `vTaskDelay(1)`.
- Root `CMakeLists.txt` and `idf_component.yml` make the library consumable as
  an ESP-IDF component.
- `examples/espidf_basic` demonstrates native ESP-IDF v6 bus/device ownership,
  a bounded `driver/i2c_master.h` transport adapter, and a static framebuffer.

## Current State

- The public driver API is already transport-agnostic. `Config` accepts
  `i2cWrite`, optional `i2cWriteRead`, `nowMs`, and `cooperativeYield` callbacks.
- Public headers under `include/ssd1315` do not require Arduino `Wire`.
- `src/SSD1315.cpp` uses guarded runtime fallback paths for Arduino,
  native-test, and ESP-IDF builds.
- I2C health tracking already goes through raw/tracked wrappers. `probe()` uses
  raw I2C and operational writes use tracked I2C.
- The driver allocates a framebuffer at `begin()` unless `externalBuffer` is
  supplied. That is acceptable for current design, but IDF examples should show
  static/external-buffer use for deterministic deployments.
- `examples/common/I2cTransport.h` remains an Arduino `Wire` adapter.
- `examples/espidf_basic/main/ssd1315_idf_i2c.*` is the ESP-IDF adapter.

## Previous Blockers Resolved

1. Guarded the Arduino runtime fallback so pure ESP-IDF builds do not require
   `<Arduino.h>`.
2. Added ESP-IDF fallback clock and cooperative-yield behavior.
3. Added ESP-IDF component metadata.
4. Added an IDF I2C adapter/example using `driver/i2c_master.h`.

## Exact Files/APIs To Change

- `src/SSD1315.cpp`
  - Guard Arduino fallback code with platform detection.
  - Under `ESP_PLATFORM`, use `esp_timer_get_time() / 1000` for `_nowMs()` when
    no callback is provided.
  - Under `ESP_PLATFORM`, use `vTaskDelay(1)` or `taskYIELD()` for
    `_cooperativeYield()` when no callback is provided.
  - Keep all I2C access routed through `_i2cWriteRaw` and `_i2cWriteTracked`.
- `include/ssd1315/Config.h`
  - Update comments that currently say fallback is Arduino `millis()`/`yield()`.
    The API can remain unchanged.
  - Do not include ESP-IDF headers in the public config.
- `examples/common/I2cTransport.h`
  - Leave as Arduino-only helper.
- IDF example adapter:
  `examples/espidf_basic/main/ssd1315_idf_i2c.*`.
- Root `CMakeLists.txt` and `idf_component.yml` are present.

## Architecture Preserving Arduino Compatibility

Keep the current callback contract. Arduino and ESP-IDF differ only in adapter
code:

- Arduino: existing `Wire` adapter supplies `i2cWrite`; `millis()` and `yield()`
  fallback remain available when compiled under Arduino.
- ESP-IDF: example/app owns the I2C master bus and device handles, supplies
  callbacks backed by `i2c_master_transmit` and optional
  `i2c_master_transmit_receive`, and supplies clock/yield callbacks.
- Core display code remains framework-neutral and keeps the existing lifecycle:
  `begin`, `tick`, `requestFlush`, `waitFlush`, `recover`, and `end`.

## Adapter Contract

IDF adapter context:

```cpp
struct Ssd1315IdfI2c {
  i2c_master_bus_handle_t bus;
  i2c_master_dev_handle_t dev;
  uint8_t address;
};
```

Callback behavior:

- `i2cWrite(addr, data, len, timeoutMs, user)` should verify `addr` matches the
  configured device address, then call
  `i2c_master_transmit(ctx->dev, data, len, timeoutMs)`.
- `i2cWriteRead(...)`, if supplied, should call
  `i2c_master_transmit_receive(ctx->dev, txData, txLen, rxData, rxLen,
  timeoutMs)`.
- Map `ESP_OK` to `Status::Ok()`.
- Map `ESP_ERR_TIMEOUT` to `Err::I2C_TIMEOUT`.
- Map invalid arguments to `Err::INVALID_PARAM`.
- Map `ESP_ERR_INVALID_RESPONSE` to an I2C NACK-related status. The simple
  ESP-IDF master APIs do not expose address-vs-data phase, so use the closest
  generic I2C error with `detail = ESP_ERR_INVALID_RESPONSE` unless a custom
  adapter can prove the NACK phase.
- Map other `esp_err_t` values to the closest I2C error with
  `detail = static_cast<int32_t>(err)`.
- Clamp or reject `timeoutMs` before passing it to ESP-IDF's signed
  `xfer_timeout_ms`; never allow overflow to become `-1` because `-1` waits
  forever.
- Do not register `i2c_master_register_event_callbacks()` on this device handle
  unless the adapter waits for transfer completion before returning.
- `nowMs(user)` returns `esp_timer_get_time() / 1000`.
- `cooperativeYield(user)` calls `vTaskDelay(1)` in task context.

The library must not create or delete I2C buses/devices. The example or
application owns bus configuration, SDA/SCL pull-ups, clock source, optional
glitch filtering, `i2c_new_master_bus`, `i2c_master_bus_add_device`,
`i2c_master_bus_rm_device`, and `i2c_del_master_bus`.

## CMake/Component Plan

Core component:

```cmake
idf_component_register(
  SRCS "src/SSD1315.cpp"
  INCLUDE_DIRS "include"
  REQUIRES esp_timer freertos
)
```

IDF example component:

```cmake
idf_component_register(
  SRCS "main.cpp" "ssd1315_idf_i2c.cpp"
  INCLUDE_DIRS "."
  REQUIRES SSD1315 esp_driver_i2c esp_timer freertos
)
```

Use ESP-IDF v6 headers/components:

- I2C: `driver/i2c_master.h` from `esp_driver_i2c`
- Time: `esp_timer.h` from `esp_timer`
- Yield/task delay: FreeRTOS headers from `freertos`

## Examples

Arduino example remains callback based:

```cpp
SSD1315::Config cfg{};
cfg.i2cWrite = examples::wireWrite;
cfg.i2cUser = &Wire;
display.begin(cfg);
```

IDF example should show explicit bus ownership:

```cpp
i2c_master_bus_config_t bus_cfg = {};
i2c_device_config_t dev_cfg = {};
dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
dev_cfg.device_address = 0x3C;
dev_cfg.scl_speed_hz = 400000;

i2c_new_master_bus(&bus_cfg, &ctx.bus);
i2c_master_bus_add_device(ctx.bus, &dev_cfg, &ctx.dev);

SSD1315::Config cfg{};
cfg.i2cAddress = 0x3C;
cfg.i2cWrite = ssd1315IdfWrite;
cfg.i2cWriteRead = ssd1315IdfWriteRead;
cfg.nowMs = ssd1315IdfNowMs;
cfg.cooperativeYield = ssd1315IdfYield;
cfg.i2cUser = &ctx;
cfg.externalBuffer = framebuffer;
display.begin(cfg);
```

## Test And Validation Plan

- Header compile test with `ESP_PLATFORM` and no Arduino headers.
- Fake I2C tests for init sequence, command write failures, flush chunking,
  recovery, and health transitions.
- IDF build tests for ESP32-S2 and ESP32-S3.
- Hardware smoke test: probe, begin, contrast/display toggles, page flush,
  full-buffer flush, simulated I2C timeout/recover.
- Arduino regression build for the existing example.

## IDF v6.0.1 Hazards

- Use `driver/i2c_master.h` and component `esp_driver_i2c`; do not use the legacy
  command-link I2C driver for new code.
- I2C device handles already encode the 7-bit address. The callback `addr`
  should be validated, not used to create devices dynamically per transaction.
- `i2c_master_transmit_receive` is only needed for probes/readback. Display data
  writes should be plain transmit calls.
- `vTaskDelay(1)` yields for at least one tick; do not call `waitFlush` from
  timing-critical or ISR context.
- SSD1315 frame flushes can exceed a short control-loop budget. Prefer
  `requestFlush` plus `tick(nowMs)` in IDF applications.
- If heap allocation is not acceptable, require `externalBuffer` and set the
  buffer size from display geometry.

## Validation

Completed locally:

- `python -m platformio test -e native`
- `python -m platformio run -e esp32s3dev`
- `python -m platformio run -e esp32s2dev`
- `python tools/check_cli_contract.py`
- `python tools/check_core_timing_guard.py`
- `python scripts/generate_version.py check`
- `git diff --check`

Pending in this shell:

- `idf.py build` for `examples/espidf_basic`

`idf.py` was not available on PATH during this implementation pass, so the
ESP-IDF example is implemented and documented but still needs a real ESP-IDF
toolchain build before release.
