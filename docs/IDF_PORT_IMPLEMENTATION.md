# SSD1315 ESP-IDF Port Implementation

Implementation status:
- `src/SSD1315.cpp` no longer includes Arduino or ESP-IDF runtime headers.
- `examples/common/IdfI2cTransport.*` maps native IDF I2C/timing/yield APIs to
  the framework-neutral display callbacks.
- `examples/espidf_basic/main/main.cpp` owns the native fixed-buffer display
  CLI and directly implements display workflows.
- The old `IdfArduinoCompat.h` compatibility shim is removed.

The command contract is enforced by `tools/check_idf_example_contract.py` and
`tools/check_cli_contract.py`. SSD1315 has display-specific commands, but IDF
bus, GPIO, timer, and CLI glue must remain native IDF.
