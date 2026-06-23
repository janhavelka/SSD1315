# SSD1315 Internal Stress Audit - 2026-06-23

Status: host-side stress and static audit focused on driver state-machine
contracts. This report does not add new hardware validation evidence.

## Scope

- No additional HIL soak was run for this pass.
- Added deterministic native stress coverage for `pollFlush()` instruction and
  byte budgets, zero-instruction polling, page-address failure recovery, hostile
  drawing coordinates, dirty bounds, and `requestFlushRect()` busy behavior.
- Kept testing through public contracts. Private driver internals were not made
  public or exposed to test-only hooks.

## New Permanent Tests

| Test | Coverage |
| --- | --- |
| `test_poll_flush_zero_instruction_queries_do_not_touch_i2c_or_advance` | Repeated `pollFlush(maxInstructions=0)` queries during an active flush must not emit I2C, must not advance phase/page/column, and must preserve dirty state. |
| `test_poll_flush_budget_stress_matrix_preserves_dirty_and_completes` | Sweeps instruction budgets `{0,1,2,3,4,7,255}` and byte budgets `{1,2,3,7,8,15,16,31,32,63,64,65,127,128,255}` over a mixed dirty framebuffer. Each poll is checked against its transaction and byte limits until completion. |
| `test_poll_flush_page_address_failure_preserves_dirty_and_retries` | Injects `I2C_TIMEOUT` on the page-address command, verifies dirty retention and health accounting, then retries with one instruction and one data byte per poll. |
| `test_hostile_drawing_and_flush_rect_stress_preserves_external_buffer_guards` | Runs 240 deterministic hostile drawing/flush-rect operations with negative and oversized coordinates/sizes, external buffer guards, invalid dirty pages/columns, and out-of-range `getPixel()` checks. |

## Commands Run

```powershell
python -m platformio test -e native
git diff --check
g++ -std=c++17 -g -O1 '-fsanitize=address,undefined' -fno-omit-frame-pointer -Iinclude -Iexamples -Itest/stubs -I.pio/libdeps/native/Unity/src src/SSD1315.cpp test/native/test_basic.cpp .pio/libdeps/native/Unity/src/unity.c -o .pio/build/native/ssd1315_asan_test.exe
cppcheck --enable=warning,performance,portability --std=c++17 -Iinclude -Iexamples -Itest/stubs --suppress=missingIncludeSystem src include test/native/test_basic.cpp
cppcheck --enable=warning,performance,portability --check-level=exhaustive --std=c++17 -Iinclude -Iexamples -Itest/stubs --suppress=missingIncludeSystem src include test/native/test_basic.cpp
```

## Results

- Native Unity: `91 test cases: 91 succeeded`.
- `git diff --check`: no whitespace errors; Git emitted CRLF conversion warnings.
- Cppcheck normal and exhaustive modes: no warnings, performance issues, or
  portability issues reported for the checked files.
- ASan/UBSan direct build could not link because this MinGW installation lacks
  `libasan` and `libubsan`; sanitizer execution was not performed.

## Findings

- No new driver crash, dirty-retention failure, byte-budget overrun,
  instruction-budget overrun, or external-buffer guard corruption was found by
  the new host stress tests.
- The first draft of the new tests exposed two test-harness assumptions, not
  production defects: expected page data byte was corrected from column mask to
  page bit value, and hostile `requestFlushRect()` handling was changed to drain
  legitimately started flush work before issuing more flush-rect requests.

## Residual Risk

- Host tests do not prove electrical behavior, panel visual correctness, reset
  wiring, bus recovery under physical faults, or field-grade soak stability.
- Sanitizer coverage remains pending until an ASan/UBSan-capable native toolchain
  is available.
