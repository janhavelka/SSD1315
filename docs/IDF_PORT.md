# SSD1315 ESP-IDF Port

The ESP-IDF example is a native IDF application in `examples/espidf_basic`.
It does not include the Arduino bring-up CLI and does not provide Arduino
compatibility facades.

Native boundaries:
- Entry point: `app_main()`.
- I2C: `driver/i2c_master.h` through `examples/common/IdfI2cTransport.*`.
- CLI input: fixed C buffer using `getchar()`.
- Timing/yield: `esp_timer_get_time()` and FreeRTOS task APIs are injected
  through the example adapter.
- Display drawing: native CLI commands call SSD1315 drawing APIs directly.
- Forbidden in IDF examples: `Arduino.h`, `Wire.h`, `String`, `Serial`,
  `TwoWire`, `ArduinoCompat`, `IdfArduinoCompat`, and including
  `examples/01_basic_bringup_cli/main.cpp`.

The driver core remains framework-neutral. SSD1315 timing/yield behavior comes
from `Config::nowMs` and `Config::cooperativeYield`; if unset, core time reads
as 0 and wait loops do not call a scheduler hook.

Run the static contract check after touching the IDF example:

```sh
python tools/check_idf_example_contract.py
python tools/check_core_timing_guard.py
```
