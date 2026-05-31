# SSD1315 Hardening Final Report

Branch: `hardening/ssd1315-industry-readiness`

Historical note: superseded by
`docs/SSD1315_PRODUCTION_GRADE_FOLLOWUP_REPORT.md` for the current production
follow-up. Items listed here as remaining work may have been addressed later.

## Summary

This pass tightened SSD1315 diagnostics and contracts without refactoring the display engine. The core was already framework-neutral; changes focused on explicit move deletion, probe error precision, CI coverage for ESP-IDF builds, and documentation honesty around blocking lifecycle calls and controller compatibility.

## Public API / Behavior Changes

- `SSD1315` move construction/assignment are explicitly deleted, matching existing non-copyable behavior.
- `probe()` maps only definite address NACK to `DEVICE_NOT_FOUND`; timeout, data NACK, bus, and generic I2C errors are preserved.
- Doxygen now describes repeated `begin()` behavior as runtime-state reset instead of claiming it calls `end()`.

## Code Changes

- Updated `src/SSD1315.cpp` probe mapping.
- Added active native tests in `test/test_basic.cpp` for move/copy prevention and timeout-preserving probe behavior.
- Mirrored the same test additions in the unused `test/native/test_basic.cpp` copy to keep the duplicate test source consistent.
- Added ESP-IDF example contract and ESP32-S2/S3 IDF build jobs to CI.
- Updated `AGENTS.md` and `README.md` with transport ownership, ISR/thread-safety, partial panel-state, reset ownership, bounded blocking lifecycle, and SSD1306 compatibility cautions.

## Tests And Checks Run

- `python tools/check_core_timing_guard.py`: pass
- `python tools/check_cli_contract.py`: pass
- `python tools/check_idf_example_contract.py`: pass
- `python scripts/generate_version.py check`: pass
- `python -m platformio test -e native`: pass, 32/32 test cases
- `python -m platformio run -e esp32s3dev`: pass
- `python -m platformio run -e esp32s2dev`: initial parallel run failed while compiling Arduino framework `HEXBuilder.cpp.o`; isolated rerun passed
- `python -m platformio pkg pack`: pass; generated tarball was removed after validation

## Not Run

- Local pure ESP-IDF build: not run because `idf.py` is not installed in this shell.
- Hardware validation: not run.

## Remaining Work

- Confirm new CI ESP-IDF jobs on GitHub.
- Add golden init-sequence tests and/or a controller profile before making strong SSD1306 compatibility claims.
- Add explicit panel-config dirty/needs-recover diagnostics for multi-command control sequences such as scroll setup.
- Record hardware validation for init, flush, missing-display, reset pin handling, and recovery behavior.

## Readiness Assessment

Closer to industry-grade and likely mergeable after CI confirms the new ESP-IDF jobs. Production deployment still needs hardware validation and controller-compatibility evidence.
