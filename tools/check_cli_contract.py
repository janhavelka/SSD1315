#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]

REQUIRED_COMMON = [
    "BoardConfig.h",
    "BuildConfig.h",
    "Log.h",
    "I2cTransport.h",
    "I2cScanner.h",
    "IdfArduinoCompat.h",
    "IdfI2cTransport.h",
    "IdfI2cTransport.cpp",
    "CommandHandler.h",
    "TransportAdapter.h",
    "BusDiag.h",
    "CliShell.h",
    "CliStyle.h",
    "HealthView.h",
    "HealthDiag.h",
]

MANDATORY_COMMANDS = ["help", "scan", "probe", "recover", "drv", "read", "verbose", "stress"]


def fail(msg: str) -> None:
    print(f"CLI contract FAILED: {msg}")
    raise SystemExit(1)


def ensure_exists(path: pathlib.Path, label: str) -> None:
    if not path.exists():
        fail(f"missing {label}: {path.as_posix()}")


def ensure_missing(path: pathlib.Path, label: str) -> None:
    if path.exists():
        fail(f"forbidden {label} still present: {path.as_posix()}")


def main() -> int:
    common_dir = ROOT / "examples" / "common"
    bringup_main = ROOT / "examples" / "01_basic_bringup_cli" / "main.cpp"
    root_cmake = ROOT / "CMakeLists.txt"
    manifest = ROOT / "idf_component.yml"
    idf_main_dir = ROOT / "examples" / "espidf_basic" / "main"
    idf_main = idf_main_dir / "main.cpp"
    idf_adapter = common_dir / "IdfI2cTransport.cpp"
    idf_adapter_header = common_dir / "IdfI2cTransport.h"
    idf_compat = common_dir / "IdfArduinoCompat.h"
    idf_cmake = idf_main_dir / "CMakeLists.txt"

    ensure_exists(common_dir, "common example directory")
    ensure_exists(bringup_main, "bringup CLI example")
    ensure_exists(root_cmake, "ESP-IDF component CMakeLists.txt")
    ensure_exists(manifest, "ESP-IDF component manifest")
    ensure_exists(idf_main, "ESP-IDF example main")
    ensure_exists(idf_adapter, "common ESP-IDF I2C adapter")
    ensure_exists(idf_adapter_header, "common ESP-IDF I2C adapter header")
    ensure_exists(idf_compat, "ESP-IDF Arduino compatibility shim")
    ensure_exists(idf_cmake, "ESP-IDF example component CMakeLists.txt")
    ensure_missing(idf_main_dir / "ssd1315_idf_i2c.cpp", "old ESP-IDF I2C adapter source")
    ensure_missing(idf_main_dir / "ssd1315_idf_i2c.h", "old ESP-IDF I2C adapter header")

    ensure_missing(ROOT / "examples" / "00_smoke_boot", "deprecated example 00_smoke_boot")
    ensure_missing(
        ROOT / "examples" / "03_feature_walkthrough",
        "deprecated example 03_feature_walkthrough",
    )

    for name in REQUIRED_COMMON:
        ensure_exists(common_dir / name, f"common helper {name}")

    text = bringup_main.read_text(encoding="utf-8", errors="replace")

    for cmd in MANDATORY_COMMANDS:
        if re.search(rf"\b{re.escape(cmd)}\b", text) is None:
            fail(f"mandatory command '{cmd}' missing in {bringup_main.as_posix()}")

    if re.search(r"\bcfg\b", text) is None and re.search(r"\bsettings\b", text) is None:
        fail("either 'cfg' or 'settings' command must be present")

    source_text = (ROOT / "src" / "SSD1315.cpp").read_text(
        encoding="utf-8", errors="replace"
    )
    if "defined(ESP_PLATFORM)" not in source_text:
        fail("core source must include an explicit ESP_PLATFORM runtime path")
    if "esp_timer_get_time()" not in source_text:
        fail("core source must use esp_timer_get_time() for ESP-IDF fallback time")
    if "vTaskDelay" not in source_text:
        fail("core source must use a FreeRTOS yield fallback for ESP-IDF")

    root_cmake_text = root_cmake.read_text(encoding="utf-8", errors="replace")
    for token in ('SRCS "src/SSD1315.cpp"', 'INCLUDE_DIRS "include"', "esp_timer", "freertos"):
        if token not in root_cmake_text:
            fail(f"ESP-IDF component CMake missing '{token}'")

    idf_cmake_text = idf_cmake.read_text(encoding="utf-8", errors="replace")
    for token in ("IdfI2cTransport.cpp", "esp_driver_i2c", "esp_timer", "freertos", "vfs"):
        if token not in idf_cmake_text:
            fail(f"ESP-IDF example CMake missing '{token}'")

    idf_main_text = idf_main.read_text(encoding="utf-8", errors="replace")
    for token in (
        "#define SSD1315_EXAMPLE_PLATFORM_IDF 1",
        '#include "examples/01_basic_bringup_cli/main.cpp"',
        "setup();",
        "loop();",
    ):
        if token not in idf_main_text:
            fail(f"ESP-IDF example main missing '{token}'")

    idf_adapter_text = idf_adapter.read_text(
        encoding="utf-8", errors="replace"
    ) + idf_adapter_header.read_text(encoding="utf-8", errors="replace")
    for token in ("driver/i2c_master.h", "i2c_new_master_bus", "i2c_master_transmit", "INT_MAX"):
        if token not in idf_adapter_text:
            fail(f"ESP-IDF adapter missing '{token}'")

    print("CLI contract PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
