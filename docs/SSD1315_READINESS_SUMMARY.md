# SSD1315 Readiness Summary

Status: SSD1315 software-contract hardening is present on `main`. Committed
COM29 serial HIL evidence exists for an ESP32-S2 Arduino/PlatformIO target,
including functional, retention, benchmark, 8-hour serial soak, and post-soak
serial cleanup. Complete field validation is still open because visual checks,
fault/recovery checks, reset behavior, logic-analyzer evidence, and exact
representative hardware matrix coverage are not fully recorded.

This document is the reviewer and operator summary. It replaces the temporary
audit, chunk, HIL attempt, ghosting diagnostic, and exploration reports that
were produced while the hardening work was being built.

## Active Documentation Set

- `README.md`: public usage, API, build, validation, and release-gate notes.
- `CHANGELOG.md`: release-facing summary of public changes.
- `AGENTS.md`: repository engineering rules for future changes.
- `docs/README.md`: map of maintained docs and evidence policy.
- `docs/SSD1315_DATASHEET_ALIGNMENT.md`: SSD1315 controller and panel-profile
  facts used by the driver.
- `docs/SSD1315_I2C_Command_Reference.md`: command-level reference notes.
- `docs/IDF_PORT.md`: native ESP-IDF example and component notes.
- `docs/SSD1315_HIL_RUNBOOK.md`: procedure for a repeatable hardware run.
- `docs/SSD1315_HIL_TARGET_TEMPLATE.md`: per-target setup and evidence form.
- `docs/SSD1315_HARDWARE_VALIDATION.md`: committed hardware validation ledger.
- Vendor PDFs and extracted markdown under `docs/`: source evidence only.

## What Changed

- The driver is explicitly SSD1315-only through `ControllerProfile::SSD1315`.
- SSD1306 compatibility claims were removed. A future SSD1306 profile must
  guard SSD1315-only commands such as `SET_IREF` and pass hardware validation.
- `PanelProfile` and `applyPanelProfile()` document supported 128x64 SSD1315
  panel and power presets.
- `clearOnBegin` and `clearOnRecover` let applications skip the blocking full
  GDDRAM clear when they will redraw and flush afterward.
- `controlStateDirty()` and `controlStateError()` expose failed multi-command
  panel-control operations that may leave controller state uncertain.
- `probe()` remains ACK-only. It does not prove SSD1315 identity.
- Contrast value `0` is rejected. Validation uses `1`, `127`, and `255`.
- Scroll command sequences, scroll-active flush blocking, and scroll recovery
  documentation were aligned with SSD1315 behavior.
- Dirty framebuffer data is preserved after flush failures for retry.
- `end()` attempts best-effort `DISPLAY_OFF` and internal charge-pump disable
  through a raw shutdown path even if the normal operation state is `OFFLINE`.
- Display-on delay and flush timing are safe when the first tick timestamp is
  `0`.
- Hardware scroll is explicitly supported only for 128-column panels, and
  vertical-scroll offset validation uses the active vertical scroll area.
- Page-buffer clear/fill window semantics are covered by native tests.
- The native ESP-IDF example uses `app_main`, `driver/i2c_master.h`, fixed
  buffers, nonblocking stdin polling, and a mutex-owned example transport.
- Arduino and ESP-IDF validation CLIs expose the same HIL smoke command set.
- `tools/run_ssd1315_hil.py` runs smoke, functional, retention, soak, or all
  command plans, captures serial logs, writes JSON/CSV/Markdown evidence, and
  marks visual commands as operator checks rather than automatic passes.
- `Config::externalBufferSizeBytes` makes caller-owned framebuffer size
  ownership explicit and rejects undersized storage before I2C.
- `Err::DRIVER_OFFLINE` distinguishes a latched offline driver fault from
  transient `BUSY` operation conflicts.
- Checked `drawBitmap(..., bitmapSizeBytes, ...)` validates caller bitmap
  source length before reading.

## Public API And Behavior Notes

- `begin()` and `recover()` are bounded blocking lifecycle calls. They send the
  init sequence synchronously and, by default, clear controller GDDRAM.
- `tick()` remains the normal bounded flush path and sends at most the configured
  byte budget per call.
- `recover()` is software-only. The core driver does not own or toggle `RES#`.
- The I2C bus, pins, locks, bus recovery, reset GPIO, and timeout policy are
  application or platform-adapter responsibilities.
- Driver instances are not internally thread-safe and public APIs are not
  ISR-safe.
- Failed panel-control writes require `recover()` or another full control-state
  resync before cached control settings should be trusted.

## Validation Status

- Host/native tests, guard scripts, HIL dry-runs, package packing, and
  PlatformIO ESP32-S2/ESP32-S3 builds are part of the release validation set.
- Pure local `idf.py` builds are not claimed unless they are run in the current
  environment. In this checkout, `idf.py` was not on `PATH`. CI must build
  `examples/espidf_basic` for ESP32-S2 and ESP32-S3.
- COM29 serial HIL on 2026-06-23 recorded ESP32-S2, PlatformIO `esp32s2dev`,
  Arduino framework, 128x64, address `0x3C`, SDA GPIO8, SCL GPIO9, 400 kHz,
  serial functional/retention/benchmark passes, and an 8-hour serial soak with
  755500 mixed operations and 0 serial failures. The panel model, supply,
  pullups, reset wiring, photos/video, physical fault injection, and logic
  analyzer evidence were not recorded.
- The 3.0.0 closeout work was also rerun on COM29 with serial-only smoke,
  functional, retention, and short soak (`--soak-ops 100`) HIL. Those local
  dirty-worktree artifacts are listed in `SSD1315_HARDWARE_VALIDATION.md`.
- That serial evidence is not complete field validation. Operator visual
  checks, photos/video, fault injection, reset-pin behavior, display-off
  ghosting isolation, sanitizer runtime coverage, and full hardware matrix
  evidence remain incomplete.
- OLED image retention or burn-in-like artifacts must be handled as a hardware
  observation until transaction logs prove stale bytes or wrong commands. Use
  the clear/ghosting sequence in `SSD1315_HIL_RUNBOOK.md`.

## Release Gate

Release `2.1.0` has been published. The current closeout changes are follow-up
work prepared for the next `3.0.0` release line and must not mutate the
already-pushed `v2.1.0` tag.

This code is suitable to review as SSD1315 software-contract hardening after CI
passes. It is not field-release complete until representative hardware
validation, fault/recovery checks, reset evidence, visual evidence, and soak
evidence are recorded in `SSD1315_HARDWARE_VALIDATION.md`.

## Removed Historical Files

The old per-chunk reports, production follow-up reports, COM16/COM17 attempt
reports, clear/ghosting diagnostic report, gap-closure implementation report,
and industrial exploration report were removed from the active docs set. They
repeated stale branch state and made the operator docs harder to follow. The
active docs listed above are the source of truth going forward.
