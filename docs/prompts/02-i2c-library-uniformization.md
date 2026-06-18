# SSD1315 I2C Uniformization Prompt

Repository: `SSD1315`

Absolute path: `C:\Users\Honza\Documents\Projects\SSD1315`

## Execution Rules

You are working inside this single repository. Implement this prompt directly;
do not repeat the cross-repository audit.

You may spawn subagents for read-only inspection of APIs, tests, I2C
transactions, docs, and diagnostics. Keep final judgment, edits, and
verification in the main agent.

Prefer simple, robust, readable code. Before adding code, inspect whether
existing code can be simplified, reused, tightened, or deleted.

Preserve dirty user changes. Do not commit unless explicitly asked.

## Common Uniformization Target

Apply this shared I2C library contract: injected non-owning transport, `Status` returns, cache-only `getSettings(SettingsSnapshot&) const`, active `probe()`/diagnostics named explicitly, `DriverState` with `state()` and `driverState()`, `isOnline()`, `lastOkMs()`, `lastErrorMs()`, `lastError()`, `consecutiveFailures()`, `totalFailures()`, and `totalSuccess()`.

Keep the common `Err` vocabulary append-only where missing: `OK`, `NOT_INITIALIZED`, `INVALID_CONFIG`, `INVALID_PARAM`, `I2C_ERROR`, `I2C_NACK_ADDR`, `I2C_NACK_DATA`, `I2C_TIMEOUT`, `I2C_BUS`, `DEVICE_NOT_FOUND`, `TIMEOUT`, `BUSY`, and `IN_PROGRESS`. Preserve SSD1315-specific geometry, page-buffer, panel, framebuffer, flush, and visual-HIL behavior.

Uniformization is not a new base class or framework. Make only local, source-compatible additions and tests.

## Current State

- Public health state is in `include\ssd1315\Status.h`: `DriverState` at line 222 and `SettingsSnapshot` at lines 271-325.
- Public lifecycle and health accessors are in `include\ssd1315\SSD1315.h`: `begin()` at line 153, `getSettings(SettingsSnapshot&)` at line 203, `probe()` at line 244, `recover()` at line 263, `driverState()` at line 275, `isOnline()` at line 281, and counters at lines 290-338.
- The flush state machine is explicit: `FlushState` at `include\ssd1315\SSD1315.h:1057`, `tickFlush()` at line 1080, implementation at `src\SSD1315.cpp:1296-1472`, and `waitFlush()` at `src\SSD1315.cpp:1724-1781`.
- Framebuffer allocation happens in `begin()` at `src\SSD1315.cpp:728-733`; steady flush paths are chunked.
- HIL runner and parser tests exist: `tools\run_ssd1315_hil.py` and `tools\test_hil_runner_parser.py`.
- The repository currently has a dirty worktree; preserve it.

## Best Sources To Adapt

- Keep SSD1315 as the source pattern for chunked/bounded long I2C work.
- Keep BME280/SHT3x as the source pattern for non-display health naming where SSD1315 already matches.
- Keep SSD1315's HIL parser test style for visual/operator-required classification.

## Implementation Tasks

1. Preserve the flush state machine and byte-budgeted I2C model. Do not replace it with blocking full-frame writes.
2. Confirm README/Doxygen explicitly state memory behavior: optional allocation in `begin()`, no repeated allocation in `tick()`/flush steady paths, and user-supplied buffer option if present.
3. Keep `probe()` ACK-only and diagnostic-only. Document that ACK does not prove panel geometry or visual correctness.
4. Keep HIL visual commands classified as operator-required evidence. Serial "OK" is not visual PASS.
5. Add/update HIL parser tests if new commands or output formats have been added since `tools\test_hil_runner_parser.py`.
6. If the existing HIL runner remains named `tools\run_ssd1315_hil.py`, document that exception or add a thin `tools\run_i2c_hil.py` wrapper. Either path must cover the common minimum `version`, `scan`, `probe`, `settings`, `health`, failure-token classification, and dry-run/parser test contract where the CLI supports it.

## API Changes Required

- None expected.

## Simplifications Before Adding Code

- Do not add a second flush scheduler or background task. Improve the existing `FlushState` path if needed.

## Tests To Add Or Update

- Parser tests for any new HIL output.
- Native tests for any flush timeout, dirty-page, page-buffer, or buffer allocation behavior changed.

## Commands To Run

- `pio test -e native`
- `pio run -e esp32s3dev`
- `python tools\test_hil_runner_parser.py`
- Live HIL only with `python tools\run_ssd1315_hil.py --port <PORT>` and operator visual review where required.

## Constraints And Non-Goals

- Do not add hidden full-screen blocking writes in normal steady operation.
- Do not claim visual correctness from serial output alone.
- Do not add bus/pin/reset ownership to the core driver.
- Preserve distinct timeout, address NACK/device-not-found, data NACK, bus, geometry, page-buffer, panel, buffer, and flush statuses. Do not collapse them into generic `I2C_ERROR` or use `DEVICE_NOT_FOUND` for timeout/data/bus failures.

## Risks And Open Questions

- Open: whether the default HIL suite should require operator visual confirmation for every drawing command or only for selected representative patterns.
