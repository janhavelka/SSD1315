# SSD1315 Readiness Summary

Status: SSD1315 software-contract hardening is present on `main`. Reported
local serial HIL command evidence exists for one COM16 run, but the raw logs are
not committed and complete hardware validation is still open because visual
checks, fault/recovery checks, reset behavior, and soak evidence were not fully
recorded.

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
- `docs/SSD1315_INDUSTRIAL_GAP_CLOSURE_REPORT.md`: latest code-actionable
  gap closure report and local validation results.
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

- Host/native tests, guard scripts, HIL dry-run, package packing, and
  PlatformIO ESP32-S2/ESP32-S3 builds passed in the release documentation pass
  on 2026-06-01.
- Pure local `idf.py` builds are not claimed unless they are run in the current
  environment. In this checkout, `idf.py` was not on `PATH`. CI must build
  `examples/espidf_basic` for ESP32-S2 and ESP32-S3.
- One local COM16 serial HIL command run was reported on 2026-05-31 using
  SSD1315 firmware at address `0x3C`. The serial command path passed after
  runner parser false positives were corrected, but the raw logs are not
  committed.
- That COM16 evidence is not complete field validation. Operator visual
  checks, photos/video, fault injection, reset-pin behavior, display-off
  ghosting isolation, and soak evidence were not recorded.
- OLED image retention or burn-in-like artifacts must be handled as a hardware
  observation until transaction logs prove stale bytes or wrong commands. Use
  the clear/ghosting sequence in `SSD1315_HIL_RUNBOOK.md`.

## Release Gate

Version metadata and changelog entries have been prepared for release `1.3.0`.
Publish only after CI passes for the release commit and tag.

This code is suitable to review as SSD1315 software-contract hardening after CI
passes. It is not field-release complete until representative hardware
validation, fault/recovery checks, reset evidence, visual evidence, and soak
evidence are recorded in `SSD1315_HARDWARE_VALIDATION.md`.

## Removed Historical Files

The old per-chunk reports, production follow-up reports, COM16 attempt report,
clear/ghosting diagnostic report, and industrial exploration report were
removed from the active docs set. They repeated stale branch state and made the
operator docs harder to follow. The active docs listed above are the source of
truth going forward.
