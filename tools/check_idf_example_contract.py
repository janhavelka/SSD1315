#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
IDF_MAIN = ROOT / "examples" / "espidf_basic" / "main"

MANDATORY_COMMANDS = [
    "help",
    "version",
    "scan",
    "probe",
    "recover",
    "drv",
    "health",
    "read",
    "cfg",
    "stress",
    "stress_mix",
    "selftest",
    "contrast",
    "invert",
    "flipx",
    "flipy",
    "display",
    "sleep",
    "allon",
    "zoom",
    "fade",
    "scrollh",
    "scrollv",
    "text",
    "clear",
    "fill",
    "pattern",
    "line",
    "rect",
    "circle",
    "pixel",
    "flush",
    "flushrect",
    "demo",
    "monitor",
    "reset",
]

REQUIRED_IDF_TOKENS = [
    'extern "C" void app_main',
    "#include <driver/i2c_master.h>",
    "i2c_master_probe",
    "IdfI2cTransport.cpp",
    "transport::wireWrite",
    "transport::nowMs",
    "transport::cooperativeYield",
    "vTaskDelay",
    "xSemaphoreCreateMutex",
    "xSemaphoreTake",
    "fcntl(",
    "O_NONBLOCK",
    "getchar()",
    "char input[",
]

FORBIDDEN_PATTERNS = [
    r"ArduinoCompat",
    r"IdfArduinoCompat",
    r"Arduino\.h",
    r"Wire\.h",
    r"\bString\b",
    r"\bSerial\b",
    r"\bTwoWire\b",
    r"01_basic_bringup_cli/main\.cpp",
    r"driver/i2c\.h",
    r"i2c_cmd_link",
    r"i2c_driver_install",
]


def fail(msg: str) -> None:
    print(f"IDF example contract FAILED: {msg}")
    raise SystemExit(1)


def read(path: pathlib.Path, label: str) -> str:
    if not path.exists():
        fail(f"missing {label}: {path.as_posix()}")
    return path.read_text(encoding="utf-8", errors="replace")


def main() -> int:
    main_text = read(IDF_MAIN / "main.cpp", "native ESP-IDF main")
    cmake_text = read(IDF_MAIN / "CMakeLists.txt", "ESP-IDF example CMake")
    transport_text = read(ROOT / "examples" / "common" / "IdfI2cTransport.cpp", "native I2C adapter")
    combined = "\n".join([main_text, cmake_text, transport_text])

    for token in REQUIRED_IDF_TOKENS:
        if token not in combined:
            fail(f"required native ESP-IDF token missing: {token}")

    for component in ("SSD1315", "esp_driver_i2c", "esp_driver_gpio", "esp_timer", "freertos"):
        if re.search(rf"\b{re.escape(component)}\b", cmake_text) is None:
            fail(f"ESP-IDF CMake missing required component '{component}'")

    for pattern in FORBIDDEN_PATTERNS:
        if re.search(pattern, combined):
            fail(f"forbidden Arduino/legacy token present: {pattern}")

    if (ROOT / "examples" / "common" / "IdfArduinoCompat.h").exists():
        fail("stale IdfArduinoCompat.h remains")

    for command in MANDATORY_COMMANDS:
        if re.search(rf'"{re.escape(command)}"', main_text) is None:
            fail(f"native CLI missing command '{command}'")

    manifest = read(ROOT / "idf_component.yml", "ESP-IDF component manifest")
    for token in ("esp32s2", "esp32s3", 'idf: ">=5.3.0"'):
        if token not in manifest:
            fail(f"idf_component.yml missing '{token}'")

    print("IDF example contract PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
