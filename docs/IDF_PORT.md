# SSD1315 — ESP-IDF Migration Prompt

> **Library**: SSD1315 (128×64 OLED display driver, I2C)
> **Current version**: 1.0.1 → **Target**: 2.0.0
> **Namespace**: `ssd1315` (lowercase)
> **Include path**: `#include "SSD1315/SSD1315.h"`
> **Difficulty**: Easy — ~15× `millis()` + 1× `yield()` replacement in .cpp only, I2C already callback-based

---

## Pre-Migration

```bash
git tag v1.0.1   # freeze Arduino-era version
```

---

## Current State — Arduino Dependencies (exact)

| API | Count | Locations |
|-----|-------|-----------|
| `#include <Arduino.h>` | 1 | `.cpp` only (not in header) |
| `millis()` | ~15 | Many in `resetActivityTimer` pattern and init waits |
| `yield()` | 1 | In polling/init wait |

I2C is write-only (display, no reads needed). Already abstracted via injected callbacks. Config is struct-based, time via `tick(uint32_t nowMs)`.

---

## Steps

### 1. Remove `#include <Arduino.h>`

### 2. Add timing helper at file scope in .cpp

```cpp
#include "esp_timer.h"

static inline uint32_t nowMs() {
    return (uint32_t)(esp_timer_get_time() / 1000);
}
```

### 3. Replace all ~15× `millis()` → `nowMs()`

The `resetActivityTimer` pattern uses `millis()` to track when the display was last updated. All call sites should use the `nowMs()` helper.

### 4. Replace 1× `yield()` → `taskYIELD()`

```cpp
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// yield() → taskYIELD();
```

### 5. Add `CMakeLists.txt` (library root)

```cmake
idf_component_register(
    SRCS "src/SSD1315.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_timer
)
```

Adjust `SRCS` to match actual source files (there may be additional .cpp files for font data, etc.).

### 6. Add `idf_component.yml` (library root)

```yaml
version: "2.0.0"
description: "SSD1315 128x64 OLED display driver (I2C, write-only)"
targets:
  - esp32s2
  - esp32s3
dependencies:
  idf: ">=5.0"
```

### 7. Version bump

- `library.json` → `2.0.0`
- `Version.h` (if present) → `2.0.0`

### 8. Replace Arduino example with ESP-IDF example

Create `examples/espidf_basic/main/main.cpp`:

```cpp
#include <cstdio>
#include "SSD1315/SSD1315.h"
#include "driver/i2c_master.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t dev;

static ssd1315::Status i2cWrite(uint8_t addr, const uint8_t* data, size_t len, void*) {
    esp_err_t err = i2c_master_transmit(dev, data, len, 100);
    return err == ESP_OK ? ssd1315::Status{ssd1315::Err::Ok}
                         : ssd1315::Status{ssd1315::Err::I2cNack, "transmit failed"};
}

extern "C" void app_main() {
    i2c_master_bus_config_t busCfg{};
    busCfg.i2c_port = I2C_NUM_0;
    busCfg.sda_io_num = GPIO_NUM_8;
    busCfg.scl_io_num = GPIO_NUM_9;
    busCfg.clk_source = I2C_CLK_SRC_DEFAULT;
    busCfg.glitch_ignore_cnt = 7;
    busCfg.flags.enable_internal_pullup = true;
    i2c_new_master_bus(&busCfg, &bus);

    i2c_device_config_t devCfg{};
    devCfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    devCfg.device_address = 0x3C;
    devCfg.scl_speed_hz = 400000;
    i2c_master_bus_add_device(bus, &devCfg, &dev);

    ssd1315::Config cfg{};
    cfg.i2cAddr = 0x3C;
    cfg.i2cWrite = i2cWrite;

    ssd1315::Display display;
    auto st = display.begin(cfg);
    if (st.err != ssd1315::Err::Ok) {
        printf("begin() failed: %s\n", st.msg);
    }

    while (true) {
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        display.tick(now);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
```

Create `examples/espidf_basic/main/CMakeLists.txt`:

```cmake
idf_component_register(SRCS "main.cpp" INCLUDE_DIRS "." REQUIRES driver esp_timer)
```

Create `examples/espidf_basic/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS "../..")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ssd1315_espidf_basic)
```

---

## Verification

```bash
cd examples/espidf_basic && idf.py set-target esp32s2 && idf.py build
```

- [ ] `idf.py build` succeeds
- [ ] Zero `#include <Arduino.h>` anywhere
- [ ] Zero `millis()`, `yield()` calls remaining
- [ ] Version bumped to 2.0.0
- [ ] `git tag v2.0.0`
