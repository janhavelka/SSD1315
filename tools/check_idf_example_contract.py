#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
IDF_PROJECT = ROOT / "examples" / "espidf_basic"
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
    "soakstep",
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
    "elapsedUs >= timeoutUs",
    "fcntl(",
    "O_NONBLOCK",
    "getchar()",
    "char input[",
    "Results:",
    "Total ops:",
    "Successes:",
    "Failures:",
    "SOAK n=",
    " df=",
    "Usage: soakstep <count 1-10000>",
    "Count must be 1-10000",
    "text[0] == '-'",
    "display.requestFlushRect",
    "display.waitFlush(transport::nowMs(nullptr), 1000)",
    "display.startFlush(options)",
    "display.pollOperation(transport::nowMs(nullptr), 1, 128)",
    "display.takeOperationResult(result)",
]

REQUIRED_CFG_TOKENS = [
    "Config: controllerProfile=",
    "panelProfile=",
    "Config: addr=0x%02X geometry=%ux%u",
    "pageBuffer=",
    "budget=",
    "clearOnBegin=",
    "clearOnRecover=",
    "scrollActive=",
    "comPins=0x%02X",
    "chargePump=0x%02X",
    "iref=0x%02X",
    "vcomh=0x%02X",
    "clockDivide=",
    "oscFrequency=",
    "prechargePhase1=",
    "prechargePhase2=",
    "state=%s online=%s",
    "healthOk=",
    "healthFail=",
    "consec=",
    "controlDirty=",
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
    project_cmake_text = read(IDF_PROJECT / "CMakeLists.txt", "ESP-IDF project CMake")
    cmake_text = read(IDF_MAIN / "CMakeLists.txt", "ESP-IDF example CMake")
    wrapper_cmake_text = read(
        IDF_PROJECT / "components" / "SSD1315" / "CMakeLists.txt",
        "stable local SSD1315 component wrapper",
    )
    transport_text = read(ROOT / "examples" / "common" / "IdfI2cTransport.cpp", "native I2C adapter")
    combined = "\n".join([main_text, project_cmake_text, cmake_text, wrapper_cmake_text, transport_text])

    for token in REQUIRED_IDF_TOKENS:
        if token not in combined:
            fail(f"required native ESP-IDF token missing: {token}")

    for component in ("SSD1315", "esp_driver_i2c", "esp_driver_gpio", "esp_timer", "freertos"):
        if re.search(rf"\b{re.escape(component)}\b", cmake_text) is None:
            fail(f"ESP-IDF CMake missing required component '{component}'")

    if "EXTRA_COMPONENT_DIRS" in project_cmake_text:
        fail("ESP-IDF example must not depend on repository-root component directory name")
    for token in ("src/SSD1315.cpp", "include"):
        if token not in wrapper_cmake_text:
            fail(f"stable SSD1315 component wrapper missing '{token}'")

    for pattern in FORBIDDEN_PATTERNS:
        if re.search(pattern, combined):
            fail(f"forbidden Arduino/legacy token present: {pattern}")

    if (ROOT / "examples" / "common" / "IdfArduinoCompat.h").exists():
        fail("stale IdfArduinoCompat.h remains")

    for command in MANDATORY_COMMANDS:
        if re.search(rf'"{re.escape(command)}"', main_text) is None:
            fail(f"native CLI missing command '{command}'")

    for token in REQUIRED_CFG_TOKENS:
        if token not in main_text:
            fail(f"native cfg output missing evidence token '{token}'")

    manifest = read(ROOT / "idf_component.yml", "ESP-IDF component manifest")
    for token in ("esp32s2", "esp32s3"):
        if token not in manifest:
            fail(f"idf_component.yml missing '{token}'")
    if re.search(r"(?m)^\s*idf:\s*['\"]?>=5\.3\.0,<6\.0\.0['\"]?\s*$", manifest) is None:
        fail("idf_component.yml must declare tested IDF range >=5.3.0,<6.0.0")

    workflow = read(ROOT / ".github" / "workflows" / "ci.yml", "CI workflow")
    for token in ("actions/checkout@v7", "actions/setup-python@v7",
                  "actions/cache@v6", "workflow_dispatch:", "tags: ['v*']"):
        if token not in workflow:
            fail(f"CI workflow missing maintained release token: {token}")
    for version in ("v5.3.5", "v5.5.5"):
        if version not in workflow:
            fail(f"CI native ESP-IDF matrix missing exact tag {version}")
    if "esp_idf_version: ${{ matrix.idf-version }}" not in workflow:
        fail("CI native ESP-IDF job must select the pinned matrix version")

    print("IDF example contract PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
