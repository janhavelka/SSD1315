# SSD1315 Documentation Map

This directory contains the maintained supporting documentation for the
SSD1315 library. Keep operator procedures, release gates, and hardware evidence
here. Do not add temporary prompt reports or one-off investigation logs unless
they are folded back into the maintained documents.

## Maintained Documents

- `../README.md`: public API, usage, build, validation, and release-gate notes.
- `../CHANGELOG.md`: release-facing summary of public changes.
- `../AGENTS.md`: repository engineering rules for future changes.
- `IDF_PORT.md`: ESP-IDF component and native example notes.
- `SSD1315_DATASHEET_ALIGNMENT.md`: controller and panel-profile facts used by
  the driver.
- `SSD1315_I2C_Command_Reference.md`: SSD1315 command reference notes.
- `SSD1315_HIL_RUNBOOK.md`: hardware-in-the-loop procedure.
- `SSD1315_HIL_TARGET_TEMPLATE.md`: per-target setup and evidence form.
- `SSD1315_HARDWARE_VALIDATION.md`: committed hardware validation ledger.
- `SSD1315_READINESS_SUMMARY.md`: current reviewer/operator readiness summary.

## Source Evidence

- `SSD1315_datasheet.pdf`: local controller datasheet.
- `Wisevision_X096-2864KSWPG01-H30_module_spec.pdf`: local module reference.
- `extracted-md/`: compact notes extracted from the PDFs.
- `pdf-extracted-md/`: full extracted PDF text for search and review.

The extracted markdown is source material, not user documentation. Use it when
changing command behavior, panel profile defaults, timing, reset, or power
contracts.

## Hardware Evidence Policy

Serial HIL command evidence can prove that firmware, CLI, I2C transport, and
driver commands completed. It does not prove visual correctness, fault recovery,
OLED retention behavior, reset wiring, or long-duration field behavior.

Use `SSD1315_HARDWARE_VALIDATION.md` for committed results. Use `Not run` for
anything not executed and `unknown` for setup facts the operator could not
verify. Do not claim field-grade readiness until serial, visual, fault/recovery,
reset, and soak evidence are recorded.

## Local Artifacts

The following are local outputs and should not be committed unless a release or
validation package explicitly requires them:

- `docs/doxygen/`
- `hil_logs/`
- `.pio/`
- generated `SSD1315-*.tar.gz` archives

## Removed Historical Reports

The old chunk reports, one-off HIL attempt reports, gap-closure implementation
reports, ghosting investigation report, and industrial exploration report were
removed from the active docs set. Their durable conclusions are now captured in
`SSD1315_READINESS_SUMMARY.md`, `SSD1315_HARDWARE_VALIDATION.md`, and
`SSD1315_HIL_RUNBOOK.md`.
