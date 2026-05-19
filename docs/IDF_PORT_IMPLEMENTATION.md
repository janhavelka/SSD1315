# SSD1315 ESP-IDF Port Implementation Notes

Date: 2026-05-19.
Branch: `feature/ssd1315-idf-port`.

## Scope

- Kept `include/ssd1315/` and `src/SSD1315.cpp` as a framework-neutral driver
  core with application-owned I2C callbacks.
- Added ESP-IDF component metadata and a native IDF example that shares the
  same colored interactive CLI source as the Arduino example.
- Preserved Arduino/PlatformIO example behavior and the existing callback API.
- Demonstrated deterministic IDF usage with a static framebuffer and explicit
  bus/device ownership in the shared CLI configuration path.

## Files Added

- `CMakeLists.txt`
- `idf_component.yml`
- `examples/espidf_basic/CMakeLists.txt`
- `examples/espidf_basic/main/CMakeLists.txt`
- `examples/espidf_basic/main/main.cpp`
- `examples/common/IdfArduinoCompat.h`
- `examples/common/IdfI2cTransport.h`
- `examples/common/IdfI2cTransport.cpp`
- `tools/check_idf_example_contract.py`

## Audit Resolution

- Pure ESP-IDF compile blocker:
  - Resolved by guarding Arduino fallback runtime calls and adding an
    `ESP_PLATFORM` path using `esp_timer_get_time()` and `vTaskDelay(1)`.
- Missing component metadata:
  - Resolved with a root component that builds `src/SSD1315.cpp`, exports
    `include/`, and declares `esp_timer` / `freertos` dependencies.
- Missing IDF adapter/example:
  - Resolved with a native `driver/i2c_master.h` adapter in
    `examples/common/IdfI2cTransport.*` that validates the configured address,
    maps `esp_err_t` to `SSD1315::Status`, and clamps transfer timeouts before
    passing them to ESP-IDF.
- CLI parity gap:
  - Resolved by making `examples/espidf_basic/main/main.cpp` define
    `SSD1315_EXAMPLE_PLATFORM_IDF`, include the example-local compatibility
    layer, and include `examples/01_basic_bringup_cli/main.cpp`.
  - ESP-IDF now exposes the same help grouping, ANSI coloring, display control,
    graphics, scroll, diagnostics, stress, self-test, and reset commands as the
    Arduino CLI.
  - `tools/check_idf_example_contract.py` statically guards the shared-source
    include, required CMake dependencies, compatibility shim, I2C adapter, and
    CLI command surface.
- Deterministic deployment guidance:
  - The IDF shared CLI path uses `Config::externalBuffer` so display RAM is
    owned by the application and no framebuffer allocation is required in the
    driver.

## Remaining Hardware Checks

- Build `examples/espidf_basic` with ESP-IDF v6.0.1 for `esp32s3` and `esp32s2`;
  `idf.py` was not available on PATH in this shell.
- Run a display smoke test for probe/begin, text draw, partial flush, full flush,
  timeout handling, and manual recovery on hardware.

## Verification

- `python -m platformio test -e native`: passed.
- `python -m platformio run -e esp32s3dev`: passed.
- `python -m platformio run -e esp32s2dev`: passed.
- `python tools/check_cli_contract.py`: passed.
- `python tools/check_idf_example_contract.py`: passed.
- `python tools/check_core_timing_guard.py`: passed.
- `python scripts/generate_version.py check`: passed.
- `git diff --check`: passed.
