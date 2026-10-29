# SSD1315 ESP-IDF Port

The ESP-IDF example is a native IDF application in `examples/espidf_basic`.
It does not include the Arduino bring-up CLI and does not provide Arduino
compatibility facades. It is a bring-up diagnostic, not a production shared-bus
template.

The component manifest requires ESP-IDF `>=5.3.0,<6.0.0`. CI defines native
example builds for ESP32-S2 and ESP32-S3 on IDF v5.3.5 and v5.5.5. The example defaults
are SDA GPIO8, SCL GPIO9, address `0x3C`, 400 kHz, and a 50 ms
`Config::i2cTimeoutMs` callback budget;
they are bring-up values to replace for the target board and bus owner.

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

The driver core remains framework-neutral. Cooperative operations use the
owner's `pollOperation(nowMs, ...)` timestamp. `Config::nowMs` supplies time for
blocking compatibility waits and health stamps; blocking `begin()`/`recover()`
paths that perform a display-on delay require it. `Config::cooperativeYield`
provides the scheduler hook for those waits. The native example installs both
callbacks. Without them, internal clock reads return zero and blocking loops
do not yield; this is not a substitute for the required lifecycle clock.

Implementation notes:

- `src/SSD1315.cpp` does not include Arduino or ESP-IDF runtime headers.
- `examples/common/IdfI2cTransport.*` maps native ESP-IDF I2C, timing, and
  yield APIs to framework-neutral driver callbacks. Its `TransportResult` is
  terminal for one callback invocation that permits at most one physical bus
  transaction, without retry or recovery. The driver supplies one total callback
  budget from `Config::i2cTimeoutMs`, possibly reduced by an operation deadline:
  time spent waiting for the example mutex is rounded up to whole milliseconds
  and subtracted before `i2c_master_transmit()`.
- `examples/espidf_basic/components/SSD1315/CMakeLists.txt` gives the local
  example a stable `SSD1315` component name inside CI containers, independent of
  the checkout directory name.
- `examples/espidf_basic/main/main.cpp` owns the native fixed-buffer display
  CLI and directly implements the validation commands.
- Required command names, help text and native boundaries are checked
  structurally by `tools/check_idf_example_contract.py` and
  `tools/check_cli_contract.py`. These checks do not execute the CLI or prove
  runtime behavior.
- Status output uses the driver's canonical error names and retains the raw
  detail code and message. The adapter maps `ESP_ERR_TIMEOUT` to timeout and
  other native transmit failures to bus error; IDF does not distinguish
  address NACK from data NACK on this path.

The example initializes and owns its demonstration bus/device handles and
mutex using `transport::initIdfI2c(sda, scl, freq, address)`. All four arguments
are explicit: GPIO numbers, clock frequency in Hz, and the unshifted 7-bit
address. This synchronous resource setup first releases any previous example
resources, then allocates the mutex and native bus/device. It does not set
transfer deadlines. Setup and `deinitWire()` require application serialization
against all resource users; the transfer mutex does not protect their lifetime.
Initialization failure returns `false` with raw `esp_err_t` in `lastInitError()`.

Migrate old native `initWire(sda, scl, freq, timeoutMs, address)` calls by
renaming to `initIdfI2c` and removing the timeout argument. The old native name
is removed so a stale four-argument call cannot silently bind a timeout as an
address. Set the driver callback budget through `Config::i2cTimeoutMs` (the
example uses `I2C_TIMEOUT_MS`). Arduino's separate `initWire()` keeps its
meaningful Wire/startup bus-clear timeout.

The diagnostic `scanI2c()` bypasses the driver callback: it independently allows
`I2C_TIMEOUT_MS` for mutex acquisition and again for each `i2c_master_probe()`.
Those are separate bounds, not the combined `wireWrite()` callback budget;
changing `Config::i2cTimeoutMs` alone does not change scan timing.

Production firmware should instead bind SSD1315 inside its existing bus
owner, provide the application-owned handles/serialization policy, and schedule
`attach()` plus cooperative start/poll/result operations. The core does not
create a bus, acquire locks, retry transport calls, recover the bus, or toggle
reset GPIO. Its explicit `recover()` API resynchronizes display state.

Run the static contract checks from the repository root after touching the IDF
example:

```sh
python tools/check_idf_example_contract.py
python tools/check_cli_contract.py
python tools/check_core_timing_guard.py
```

Build the actual native adapter in an activated ESP-IDF environment:

```sh
idf.py -C examples/espidf_basic set-target esp32s3
idf.py -C examples/espidf_basic build
```

Repeat for `esp32s2` and both CI SDK versions, using isolated build and
configuration state for each combination as the CI containers do. The main
component compiles `examples/common/IdfI2cTransport.cpp`; an Arduino PlatformIO
build does not validate this source path. Report unavailable SDK builds as not
run. See the [documentation map](DOCUMENTATION.md) for the hardware runbook and
qualification ledger; compilation does not establish hardware results.
