# SSD1315 ESP-IDF Port

The ESP-IDF example is a native IDF application in `examples/espidf_basic`.
It does not include the Arduino bring-up CLI and does not provide Arduino
compatibility facades. It is a bring-up diagnostic, not a production shared-bus
template.

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

Current implementation status:

- `src/SSD1315.cpp` does not include Arduino or ESP-IDF runtime headers.
- `examples/common/IdfI2cTransport.*` maps native ESP-IDF I2C, timing, and
  yield APIs to framework-neutral driver callbacks. Its `TransportResult` is
  terminal for one callback invocation that permits at most one physical bus
  transaction. The supplied timeout is one total callback
  budget: time spent waiting for the example mutex is subtracted before
  `i2c_master_transmit()`.
- `examples/espidf_basic/components/SSD1315/CMakeLists.txt` gives the local
  example a stable `SSD1315` component name inside CI containers, independent of
  the checkout directory name.
- `examples/espidf_basic/main/main.cpp` owns the native fixed-buffer display
  CLI and directly implements the validation commands.
- The previous Arduino compatibility shim is removed.
- Command parity is checked by `tools/check_idf_example_contract.py` and
  `tools/check_cli_contract.py`.

The example initializes and owns its demonstration bus/device handles and
mutex. Production firmware should instead bind SSD1315 inside its existing bus
owner, provide the application-owned handles/serialization policy, and schedule
`attach()` plus cooperative start/poll/result operations. The core does not
create a bus, acquire locks, retry, recover, or toggle reset GPIO.

Run the static contract check after touching the IDF example:

```sh
python tools/check_idf_example_contract.py
python tools/check_core_timing_guard.py
```
