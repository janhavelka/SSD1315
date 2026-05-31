# SSD1315 Chunk 03 Datasheet and Panel Alignment Report

Date: 2026-05-31

## Branch and Starting State

- Branch: `hardening/ssd1315-industry-readiness`
- Starting commit: `53f9a22 feat: add SSD1315 validation CLI harness`
- Starting worktree: clean at chunk start
- Chunk scope: align SSD1315 command/init/panel assumptions with local
  datasheet and Wisevision module documentation before hardware validation.

## Datasheet Facts Checked

| Fact | Result |
| --- | --- |
| SSD1315 has 128x64 internal GDDRAM | Documented; default/profile geometry remains 128x64. |
| I2C addresses are SA0-selected `0x3C`/`0x3D` | Code now rejects other configured addresses. Probe remains ACK-only. |
| I2C control bytes are Co/D-C# command/data selectors | Golden tests assert command control byte `0x00` and data control byte `0x40`. |
| Init keeps display off until configuration and optional clear complete | Golden tests assert `DISPLAY_OFF` first and final `DISPLAY_ON` after init/clear. |
| Charge pump must be enabled before display-on for internal-pump profiles | Tests assert pump command precedes display-on; `end()` disables pump after display-off. |
| External VCC/charge-pump-off path exists | Added panel profile and tests for charge pump `OFF` with external IREF. |
| `SET_IREF` (`0xAD`) is SSD1315-specific | Kept SSD1315-only policy; Wisevision profiles use external IREF value. |
| Contrast command range is `0x01..0xFF` | `begin()`, `setContrast()`, CLI help, docs, and tests reject `0`. |
| Scroll speed raw-value mapping is SSD1315-specific | Corrected enum labels and docs; old labels remain aliases to preserve source compatibility. |
| Horizontal scroll uses full-width end column `0x7F` | Fixed code/tests from `0xFF` to `0x7F`. |
| Vertical+horizontal scroll sequence includes offset and column range | Fixed code/tests to emit the full SSD1315 sequence. |
| Scroll deactivation requires GDDRAM rewrite | Flushes are blocked while scroll is active; successful stop marks all framebuffer pages dirty. |
| Serial/I2C mode does not provide useful identity/readback | No identity or GDDRAM readback claims were added. |

## Module Spec Facts Checked

The local Wisevision `X096-2864KSWPG01-H30` spec was used as the target module
source. Relevant facts recorded in docs/tests:

- Module is 128x64 monochrome and lists SSD1315 as the driver IC.
- I2C mode uses SA0/D-C# for `0x3C`/`0x3D`.
- Module example orientation uses segment remap `A1` and COM scan `C8`.
- COM pins use alternative configuration `0x12`.
- Module examples use contrast `0xB0`, clock `0x90`, precharge `0x22`, and
  VCOMH `0x30`.
- Module examples include both internal DC/DC / charge-pump and external-VCC
  electrical paths.
- Module IREF examples use an external resistor, so Wisevision profiles select
  external IREF instead of internal SSD1315 IREF current.

## Code Changed

- Added `PanelProfile` and `applyPanelProfile()` for documented 128x64
  SSD1315 panel/electrical presets:
  `GENERIC_128X64_INTERNAL_CHARGE_PUMP`,
  `WISEVISION_X096_2864KSWPG01_H30_INTERNAL_DC_DC`, and
  `WISEVISION_X096_2864KSWPG01_H30_EXTERNAL_VCC`.
- Restricted configured SSD1315 I2C addresses to `0x3C` or `0x3D`.
- Rejected contrast value `0` in `begin()` and `setContrast()`.
- Corrected SSD1315 scroll speed enum labels, keeping old labels as raw-value
  aliases for source compatibility.
- Corrected horizontal and vertical scroll command byte sequences.
- Added `_scrollActive` tracking so framebuffer flush is blocked while hardware
  scroll is active and dirty data is preserved for redraw after scroll stop.
- Added best-effort internal charge-pump disable in `end()`.
- Updated Arduino and ESP-IDF CLI help/validation text for `contrast 1..255`
  and software-only reset/recover wording.

## Tests Added or Extended

- Panel profile application and invalid profile handling.
- Address and contrast validation before any bus write.
- Wisevision internal and external profile init command values.
- Semantic golden init ordering, command control bytes, data control bytes,
  charge pump/IREF ordering, and final display-on ordering.
- `clearOnBegin=false` dirty redraw path without blocking GDDRAM clear.
- Default `recover()` clear-before-display-on behavior.
- External-VCC charge-pump-off init.
- Scroll horizontal/vertical byte sequences, boundary pages, vertical offset,
  failure dirty-state behavior, no activate after setup failure, active-scroll
  flush blocking, and dirty redraw after stop.
- Full-frame flush address windows, chunking, and data payload.
- `end()` display-off plus internal charge-pump disable ordering.

## Profile Decision

A narrow SSD1315 panel-profile helper was implemented because it fits the
existing `Config` model without adding bus/reset ownership or a broad
controller compatibility abstraction. No SSD1306 profile was added. SSD1306
work remains future scope and must remove/guard SSD1315-only commands plus
pass hardware validation.

## Validation Commands Changed

- Executable validation commands remain the chunk 2 set.
- `contrast 0` remains absent from hardware validation.
- Help and CLI validation now advertise `contrast [1-255]` / `contrast <1..255>`.
- Arduino `stress`, `burst`, and `stress_mix` diagnostics now cycle contrast
  through `1..255`, not `0..255`, so validation stress does not intentionally
  send an invalid SSD1315 contrast value.
- `reset`/`recover` help now says software-only and does not imply `RES#`
  ownership.

## Integration Review

The integration-review subagent found and this chunk fixed:

- Arduino validation stress paths could still send contrast `0`.
- `applyPanelProfile()` mutated `Config` before returning `INVALID_CONFIG` for
  an unsupported profile; it is now atomic for invalid profiles and covered by
  native tests.
- Command-reference scroll wording and README lifecycle wording needed minor
  consistency updates.

No framework leakage, unsupported SSD1306 claims, or field-grade claims were
reported.

## Checks Run

| Command | Result |
| --- | --- |
| `git diff --check` | Pass |
| `python tools/check_core_timing_guard.py` | Pass |
| `python tools/check_cli_contract.py` | Pass |
| `python tools/check_idf_example_contract.py` | Pass |
| `python scripts/generate_version.py check` | Pass |
| `python -m platformio test -e native` | Pass, 54 tests |
| `python -m platformio run -e esp32s3dev` | Pass |
| `python -m platformio run -e esp32s2dev` | One earlier non-verbose run failed while compiling Arduino framework object `esp32-hal-spi.c.o` without a compiler diagnostic; immediate non-verbose rerun passed, and the final post-review rerun passed. |
| `python -m platformio pkg pack` | Pass; generated `SSD1315-1.2.0.tar.gz`, then removed the generated tarball from the worktree. |
| `idf.py --version` | Not available: PowerShell reported `idf.py` was not recognized as a cmdlet, function, script file, or operable program. |

Pure ESP-IDF `idf.py` builds were not run locally because `idf.py` is not on
`PATH`. CI remains responsible for confirming native ESP-IDF builds for
ESP32-S2 and ESP32-S3.

## Hardware Validation Status

Not run in this chunk. No panel was connected through this environment, and no
visual, reset-pin, unplug/replug, soak, screenshot, or logic-analyzer result is
claimed.

## Remaining Hardware-Validation Risks For Chunk 04

- Actual module wiring must select the matching panel profile, especially
  external VCC versus internal DC/DC and external versus internal IREF.
- Reset-low pulse width, VDD/VBAT/VCC stability, and board power sequencing
  remain application/fixture responsibilities.
- Contrast, VCOMH, precharge, COM/SEG mapping, and charge-pump voltage still
  need visual and electrical confirmation on the real module.
- CI must still confirm pure ESP-IDF jobs outside this local environment.
- SSD1306-like panels remain out of scope and unvalidated.
