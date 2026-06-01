# SSD1315 Readiness Summary

Status: software-contract hardening and serial-HIL preparation are complete on
this branch. Earlier COM16 and COM17 runs provide serial command evidence,
including a final COM17 serial/device pass after runner parser fixes. The
representative visual/fault/reset/soak hardware matrix is still incomplete.

This document replaces the intermediate audit, chunk, and final-gate reports
that were produced while the branch was being hardened. It is the current
human-readable summary for reviewers and operators.

## Active Documentation Set

- `README.md`: public usage, API, build, validation, and release-gate notes.
- `CHANGELOG.md`: release-facing summary of public changes.
- `AGENTS.md`: repository engineering rules for future changes.
- `docs/README.md`: map of maintained docs, source evidence, and validation
  claim policy.
- `docs/SSD1315_DATASHEET_ALIGNMENT.md`: SSD1315 controller and panel-profile
  facts used by the driver.
- `docs/SSD1315_INDUSTRIAL_GAP_CLOSURE_REPORT.md`: latest code-actionable
  gap closure report and local validation results.
- `docs/SSD1315_I2C_Command_Reference.md`: command-level reference notes.
- `docs/IDF_PORT.md`: native ESP-IDF example and component notes.
- `docs/SSD1315_HIL_RUNBOOK.md`: procedure for a repeatable hardware run.
- `docs/SSD1315_HIL_TARGET_TEMPLATE.md`: per-target setup and evidence form.
- `docs/SSD1315_HARDWARE_VALIDATION.md`: final matrix to fill with real
  hardware results.
- Vendor PDFs and extracted markdown under `docs/`: source evidence only.

## What Changed On This Branch

- The driver now exposes an SSD1315-only `ControllerProfile::SSD1315`.
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
  PlatformIO ESP32-S2/ESP32-S3 builds passed in the docs cleanup pass on
  2026-05-31.
- The ESP32-S2 and ESP32-S3 PlatformIO builds should be run sequentially in this
  Windows environment. Running both at the same time caused transient framework
  object build failures that passed when rerun one target at a time.
- Pure local `idf.py` builds have not been claimed unless they are run in the
  current environment. In this checkout, `idf.py` was not on `PATH`. CI must
  build `examples/espidf_basic` for ESP32-S2 and ESP32-S3.
- GitHub PR/CI status was not queried in this checkout because `gh` was not on
  `PATH`. Check the branch or PR in the GitHub UI before merge.
- Serial HIL command evidence exists from earlier COM16 and COM17 runs. The
  COM17 run reached a serial/device pass after HIL parser fixes, but visual
  operator checks, fault/recovery records, reset behavior where wired, and
  representative bounded-soak evidence are still missing from the committed
  matrix. The retained serial-only facts are summarized in
  `docs/SSD1315_HARDWARE_VALIDATION.md`. Fill the matrix result rows only with
  observed target evidence.

## Release Gate

This branch may be merged as SSD1315 software-contract hardening after CI passes.
It is not field-release complete until representative hardware validation,
fault/recovery checks, and soak evidence are recorded.

The current package version is still sourced from `library.json`. Because public
API and behavior were added, publishing this work requires a SemVer bump and
moving the changelog entries out of `[Unreleased]`.

## Removed Historical Files

The old per-chunk reports, production follow-up reports, final pre-HIL operator
reports, one-off COM16/COM17 auditor reports, clear/ghosting diagnostic report,
and superseded exploration report were deleted from the active docs set. They
were useful process logs, but they repeated stale branch state and made the
operator docs harder to follow.

Persistent conclusions from those reports were folded into this summary and
`docs/SSD1315_HARDWARE_VALIDATION.md`: serial command evidence exists, visual
and fault evidence is still incomplete, OLED image retention must be separated
from live GDDRAM faults, and field-ready claims remain blocked on the completed
matrix.
