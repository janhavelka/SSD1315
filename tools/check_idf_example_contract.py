#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]

IDF_EXAMPLE_MACRO = "SSD1315_EXAMPLE_PLATFORM_IDF"
CLI_SOURCE_INCLUDE = '#include "examples/01_basic_bringup_cli/main.cpp"'
REQUIRED_COMPONENTS = [
    "SSD1315",
    "esp_driver_i2c",
    "esp_driver_gpio",
    "esp_timer",
    "esp_rom",
    "freertos",
    "vfs",
]
REQUIRED_FILES = [
    "CMakeLists.txt",
    "idf_component.yml",
    "examples/common/IdfArduinoCompat.h",
    "examples/common/IdfI2cTransport.h",
    "examples/common/IdfI2cTransport.cpp",
    "examples/espidf_basic/CMakeLists.txt",
    "examples/espidf_basic/main/CMakeLists.txt",
    "examples/espidf_basic/main/main.cpp",
]
MANDATORY_COMMANDS = [
    "help",
    "version",
    "scan",
    "probe",
    "recover",
    "drv",
    "read",
    "cfg",
    "stress",
    "stress_mix",
    "selftest",
    "contrast",
    "invert",
    "flipx",
    "flipy",
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
    "health",
    "monitor",
    "reset",
]


def fail(msg: str) -> None:
    print(f"IDF example contract FAILED: {msg}")
    raise SystemExit(1)


def require_token(text: str, token: str, label: str) -> None:
    if token not in text:
        fail(f"{label} missing token '{token}'")


def command_has_dispatch(cli: str, command: str) -> bool:
    patterns = [
        rf'cmd::match\(cmdBuf,\s*"{re.escape(command)}"\)',
        rf'strcmp\(cmdBuf,\s*"{re.escape(command)}"\)\s*==\s*0',
        rf'strncmp\(cmdBuf,\s*"{re.escape(command)}\s',
        rf'cmd::parseInt\(cmdBuf,\s*"{re.escape(command)}"',
        rf'strncasecmp\(cmdBuf,\s*"{re.escape(command)}\s',
    ]
    return any(re.search(pattern, cli) for pattern in patterns)


def main() -> int:
    for rel in REQUIRED_FILES:
        if not (ROOT / rel).exists():
            fail(f"missing {rel}")

    if (ROOT / "examples" / "espidf_basic" / "main" / "ssd1315_idf_i2c.cpp").exists():
        fail("old ESP-IDF I2C adapter remains in espidf_basic/main")

    idf_main = (ROOT / "examples" / "espidf_basic" / "main" / "main.cpp").read_text(
        encoding="utf-8", errors="replace"
    )
    for token in (
        f"#define {IDF_EXAMPLE_MACRO} 1",
        '#include "examples/common/IdfArduinoCompat.h"',
        CLI_SOURCE_INCLUDE,
        'extern "C" void app_main(void)',
        "setup();",
        "loop();",
    ):
        require_token(idf_main, token, "ESP-IDF main")

    cmake = (ROOT / "examples" / "espidf_basic" / "main" / "CMakeLists.txt").read_text(
        encoding="utf-8", errors="replace"
    )
    for component in REQUIRED_COMPONENTS:
        if re.search(rf"\b{re.escape(component)}\b", cmake) is None:
            fail(f"ESP-IDF CMake missing required component '{component}'")

    compat = (ROOT / "examples" / "common" / "IdfArduinoCompat.h").read_text(
        encoding="utf-8", errors="replace"
    )
    for token in ("class IdfConsole", "esp_timer_get_time", "esp_rom_delay_us", "fcntl"):
        require_token(compat, token, "IdfArduinoCompat.h")

    transport = (ROOT / "examples" / "common" / "IdfI2cTransport.cpp").read_text(
        encoding="utf-8", errors="replace"
    )
    for token in ("i2c_new_master_bus", "i2c_master_transmit", "i2c_master_transmit_receive"):
        require_token(transport, token, "IdfI2cTransport.cpp")

    cli = (ROOT / "examples" / "01_basic_bringup_cli" / "main.cpp").read_text(
        encoding="utf-8", errors="replace"
    )
    require_token(cli, f"defined({IDF_EXAMPLE_MACRO})", "shared CLI")
    require_token(cli, "configureDisplayConfig", "shared CLI")
    require_token(cli, "i2c_scanner::scanDefault", "shared CLI")
    for command in MANDATORY_COMMANDS:
        if f'printHelpItem("{command}' not in cli:
            fail(f"CLI missing help item '{command}'")
        if not command_has_dispatch(cli, command):
            fail(f"CLI missing dispatch '{command}'")

    manifest = (ROOT / "idf_component.yml").read_text(encoding="utf-8", errors="replace")
    for token in ("esp32s2", "esp32s3", 'idf: ">=6.0.1"'):
        require_token(manifest, token, "idf_component.yml")

    print("IDF example contract PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
