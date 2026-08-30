# SSD1315 Documentation Map

This directory contains the maintained supporting documentation for the
SSD1315 library. Keep operator procedures, release gates, and hardware evidence
here. Fold durable conclusions into these documents instead of retaining task
prompts, implementation diaries, or one-off investigation logs.

## Maintained Documents

- `../README.md`: public API, usage, build, validation, and release-gate notes.
- `../CHANGELOG.md`: release-facing summary of public changes.
- `../CONTRIBUTING.md`: architecture, documentation, and validation expectations.
- `../SECURITY.md`: supported-release, private-reporting, and trust-boundary policy.
- `../AGENTS.md`: repository engineering rules for future changes.
- `RELEASING.md`: release preparation, exact-commit CI, tagging, and GitHub
  publication procedure.
- `IDF_PORT.md`: ESP-IDF component and native example notes.
- `SSD1315_DATASHEET_ALIGNMENT.md`: controller and panel-profile facts used by
  the driver.
- `SSD1315_I2C_Command_Reference.md`: SSD1315 command reference notes.
- `SSD1315_HIL_RUNBOOK.md`: hardware-in-the-loop procedure.
- `SSD1315_HIL_TARGET_TEMPLATE.md`: per-target setup and evidence form.
- `SSD1315_HARDWARE_VALIDATION.md`: committed hardware validation ledger.
- `SSD1315_READINESS_SUMMARY.md`: current reviewer/operator readiness summary.

## Dated Evidence Records

- `reports/code-audit-resolution-20260830.md`: finding-by-finding disposition
  of the 2026-08-27 code audit, resulting fixes, corrections, and validation.
- `reports/hil-validation-COM21-20260731.md`: current pioarduino 55.03.311
  ESP32-S3 N16R8 migration audit and partial serial HIL, including functional,
  benchmark, extended-command, and 97,000-operation one-hour soak coverage.

Dated reports preserve exactly what was run at that time. Superseded reports
are consolidated into the hardware ledger and remain recoverable from Git
history instead of accumulating prompt-specific assessments in the active
documentation. Current status belongs in `SSD1315_READINESS_SUMMARY.md` and
`SSD1315_HARDWARE_VALIDATION.md`.

## Source Evidence

- `SSD1315_datasheet.pdf`: local controller datasheet.
- `Wisevision_X096-2864KSWPG01-H30_module_spec.pdf`: local module reference.
- `extracted-md/`: compact notes extracted from the PDFs. These files are
  source evidence and must remain in release packages:
  - `00_document_inventory.md`
  - `01_chip_overview.md`
  - `02_pinout_and_signals.md`
  - `03_electrical_and_timing.md`
  - `04_protocol_commands_and_transactions.md`
  - `05_register_map.md`
  - `06_modes_interrupts_status_and_faults.md`
  - `07_initialization_reset_and_operational_notes.md`
  - `08_variant_differences_and_open_questions.md`
- `pdf-extracted-md/`: full extracted PDF text for repository-local search and
  review. These duplicate the source PDFs and are excluded from release
  packages.

The extracted markdown is source material, not user documentation. Use it when
changing command behavior, panel profile defaults, timing, reset, or power
contracts.

## Documentation Maintenance

- Public symbol contracts live beside declarations in `include/ssd1315/`.
- README is the usage and behavior overview; avoid copying detailed contracts
  into multiple supporting documents.
- CHANGELOG records release-facing changes; dated reports remain immutable
  evidence snapshots.
- Hardware claims belong in dated evidence reports and are summarized by the
  validation ledger; both must name exact setup, revision, command coverage,
  fault cases, and duration.
- `doxygen Doxyfile` is a warning-as-error completeness check. Its configuration
  excludes generated output, historical HIL evidence, and extracted source
  material.
- Generated HTML under `docs/doxygen/` is local output and is not committed.

## Hardware Evidence Policy

Serial HIL command evidence can prove that firmware, CLI, I2C transport, and
driver commands completed. It does not prove visual correctness, fault recovery,
OLED retention behavior, reset wiring, or long-duration field behavior.

Use `reports/` for the current representative immutable run and
`SSD1315_HARDWARE_VALIDATION.md` for the cross-run ledger. Keep raw runner
artifacts under ignored `hil_logs/`; do not commit serial transcripts. Use
`Not run` for anything not executed and `unknown` for setup facts the operator
could not verify. Do not claim field-grade readiness until serial, visual,
fault/recovery, reset, and soak evidence are recorded.

## Local Artifacts

The following are local outputs and should not be committed unless a release or
validation package explicitly requires them:

- `docs/doxygen/`
- `hil_logs/`
- `.pio/`
- generated `SSD1315-*.tar.gz` archives
