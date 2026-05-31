# SSD1315 Chunk 02 CLI Validation Harness Report

Date: 2026-05-31

## Branch and Starting State

- Branch: `hardening/ssd1315-industry-readiness`
- Starting commit: `9376470 chore: freeze SSD1315 production-grade baseline`
- Starting worktree: clean at chunk start
- Chunk scope: make the hardware-validation command set executable and contract-guarded without changing the core display engine.

## Command Inventory

| Command | Arduino CLI | ESP-IDF CLI | Documented | Contract-guarded | Action |
| --- | --- | --- | --- | --- | --- |
| `scan` | Yes | Yes | Yes | Yes | Kept. |
| `probe` | Yes | Yes | Yes | Yes | Help/docs now state ACK-only, not controller identity. |
| `cfg` | Yes | Yes | Yes | Yes | Kept. |
| `selftest` | Yes | Yes | Yes | Yes | Kept as executable smoke command. |
| `pattern checker` | Yes | Yes | Yes | Yes | IDF parser now rejects unknown patterns instead of falling through to checker. |
| `clear` | Yes | Yes | Yes | Yes | Kept. |
| `fill` | Yes | Yes | Yes | Yes | Kept with OLED static-image caution in docs/help. |
| `invert 1` | Yes | Yes | Yes | Yes | Kept. |
| `invert 0` | Yes | Yes | Yes | Yes | Kept. |
| `contrast 1` | Yes | Yes | Yes | Yes | Replaced unsafe validation use of `contrast 0`. |
| `contrast 127` | Yes | Yes | Yes | Yes | Kept and used as final restore value. |
| `contrast 255` | Yes | Yes | Yes | Yes | Kept. |
| `flipx 1` | Yes | Yes | Yes | Yes | Kept. |
| `flipx 0` | Yes | Yes | Yes | Yes | Kept. |
| `flipy 1` | Yes | Yes | Yes | Yes | Kept. |
| `flipy 0` | Yes | Yes | Yes | Yes | Kept. |
| `scrollh right 0 7` | Yes | Yes | Yes | Yes | Help/docs aligned to `left|right`; IDF parser validates direction, page range, and speed. |
| `scrollv left 0 7 1` | Yes | Yes | Yes | Yes | Help/docs aligned; IDF parser validates direction, page range, offset, and speed. |
| `scroll stop` | Yes | Yes | Yes | Yes | Added/standardized stop command; `scrollstop` remains as an alias. |
| `recover` | Yes | Yes | Yes | Yes | Kept. |
| `stress 100` | Yes | Yes | Yes | Yes | Kept as bounded diagnostic stress. |
| `stress_mix 100` | Yes | Yes | Yes | Yes | Kept as bounded mixed-path stress. |
| `monitor` | Yes | Yes | Yes | Yes | ESP-IDF monitor now keeps polling stdin; docs describe Arduino/IDF semantics. |

Manual/future validation rows remain outside the executable command sequence: missing-display behavior, unplug/replug behavior, reset-pin pulse behavior, long soak, and evidence capture references.

## Commands Added or Changed

- Added/standardized `scroll stop` in both CLIs, with `scrollstop` kept as a compatibility alias.
- Fixed Arduino scroll speed validation so out-of-range parsed values are rejected before casting, and aligned runtime usage strings to `left|right`.
- Tightened ESP-IDF CLI validation for `contrast`, `invert`, `flipx`, `flipy`, `scrollh`, `scrollv`, and `pattern`.
- Made ESP-IDF `monitor` nonblocking with respect to stdin polling, so it has a clean stop path.
- Updated Arduino and ESP-IDF help text so `probe` is described as ACK-only and not SSD1315 identity.
- Added `controlDirty` reporting to shared health diagnostics and CLI status output.
- Updated hardware-validation docs to use `contrast 1`, `contrast 127`, and `contrast 255`; `contrast 0` is no longer part of the executable validation sequence.
- Added operator cautions for visual validation, OLED static high-contrast images, and recovery/redraw when `controlStateDirty()` is true.

## Intentionally Not Implemented

- No CLI command was added for application-owned reset GPIO pulsing; reset-pin behavior remains hardware/application validation.
- No command was added for unplug/replug or missing-display simulation; those remain manual hardware-validation procedures.
- No full parser extraction was done for Arduino serial I/O. Host tests target the core driver command effects and fault behavior instead.
- No hardware smoke was run in this chunk because no serial target/hardware session was available in the environment.

## Contract Guard Changes

`tools/check_cli_contract.py` now verifies that the documented executable smoke command set is represented in CLI help/source and in `docs/SSD1315_HARDWARE_VALIDATION.md`. The guard covers:

`selftest`, `pattern checker`, `clear`, `fill`, `invert`, `contrast`, `flipx`, `flipy`, `scrollh`, `scrollv`, `scroll stop`, `recover`, `stress`, `stress_mix`, and `monitor`.

It also rejects reintroduction of `contrast 0` as a hardware-validation command and checks that ESP-IDF monitor mode keeps polling stdin.

After integration review, the guard also checks Arduino runtime scroll syntax/error text and the signed scroll-speed parser signature.

## Native Tests Added

Native fake-transport coverage now includes:

- display-control command bytes for invert, flipx, flipy, and contrast;
- control-state dirty behavior after failed invert, flipx, flipy, and contrast operations;
- successful `recover()` clearing control-state dirty diagnostics;
- horizontal and vertical scroll command byte sequences;
- scroll validation failures that do not send I2C;
- scroll transport failures marking `controlStateDirty()`.

## Checks Run

| Command | Result |
| --- | --- |
| `python tools/check_core_timing_guard.py` | Pass |
| `python tools/check_cli_contract.py` | Pass |
| `python tools/check_idf_example_contract.py` | Pass |
| `python scripts/generate_version.py check` | Pass |
| `python -m platformio test -e native` | Pass, 48 tests |
| `python -m platformio run -e esp32s3dev` | Pass |
| `python -m platformio run -e esp32s2dev` | First non-verbose attempt failed inside Arduino framework object generation without a compiler diagnostic; verbose rerun passed and exact non-verbose rerun passed. |
| `python -m platformio pkg pack` | Pass; produced `SSD1315-1.2.0.tar.gz`, then the generated tarball was removed from the worktree. |
| `idf.py --version` | Not available: PowerShell reported `idf.py` was not recognized as a cmdlet, function, script file, or operable program. |

Pure ESP-IDF `idf.py` builds were not run locally because `idf.py` is not on `PATH`; CI remains responsible for native ESP-IDF target confirmation.

## Hardware Smoke

Not run. No hardware/serial session was available. The hardware-validation document now separates executable CLI smoke commands from manual/future hardware procedures, and it requires operator visual confirmation for display output.

## Remaining Risks for Chunk 03

- SSD1315 analog/init defaults still need datasheet and panel-profile alignment.
- Actual ESP-IDF CI execution is pending outside this local environment.
- Hardware validation remains not run; no controller marking, panel behavior, reset-pin, unplug/replug, or soak evidence exists yet.
- Some CLI commands can only prove command dispatch locally; visual correctness still depends on observed panel behavior.
