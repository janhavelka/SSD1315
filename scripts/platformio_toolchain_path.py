"""Expose pioarduino's nested Xtensa toolchain directory to SCons.

Some Windows installations retain the ESP-IDF archive's leading
``xtensa-esp-elf`` directory when PlatformIO registers the package. PlatformIO
then adds only the package-level ``bin`` directory, which does not exist. Keep
the normal package layout untouched and add the nested directory only when it
is present.
"""

from pathlib import Path

Import("env")  # type: ignore[name-defined]  # PlatformIO / SCons built-in


platform = env.PioPlatform()  # type: ignore[name-defined]
toolchain_dir = platform.get_package_dir("toolchain-xtensa-esp-elf")
if toolchain_dir is not None:
    nested_bin = Path(toolchain_dir) / "xtensa-esp-elf" / "bin"
    if nested_bin.is_dir():
        env.PrependENVPath("PATH", str(nested_bin))  # type: ignore[name-defined]
