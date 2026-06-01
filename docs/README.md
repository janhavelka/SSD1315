# SSD1315 Documentation Map

This directory keeps the maintained engineering documents for the SSD1315
driver. Process logs and one-off audit reports are not kept here unless they
still contain active release or validation decisions.

## Maintained Documents

- `SSD1315_READINESS_SUMMARY.md`: current branch status, public behavior
  changes, validation state, and release gate.
- `SSD1315_DATASHEET_ALIGNMENT.md`: SSD1315 controller facts, panel-profile
  assumptions, and unsupported compatibility claims.
- `SSD1315_I2C_Command_Reference.md`: SSD1315 command-level reference notes.
- `IDF_PORT.md`: ESP-IDF component and example notes.
- `SSD1315_HIL_RUNBOOK.md`: operator procedure for repeatable HIL runs.
- `SSD1315_HIL_TARGET_TEMPLATE.md`: per-board and per-panel setup template.
- `SSD1315_HARDWARE_VALIDATION.md`: committed hardware validation matrix.
- `SSD1315_INDUSTRIAL_GAP_CLOSURE_REPORT.md`: latest implementation report for
  the industrial-gap closure branch.

Implementation reports are retained for review history and are intentionally
omitted from the generated Doxygen API site. Operator-facing procedures and API
contracts should live in the non-report documents above.

## Source Evidence

- `SSD1315_datasheet.pdf`: controller datasheet used for command and timing
  checks.
- `Wisevision_X096-2864KSWPG01-H30_module_spec.pdf`: target module reference
  sheet used for the default panel assumptions.
- `extracted-md/` and `pdf-extracted-md/`: extracted reference text. Treat this
  as source evidence, not operator procedure.

## Validation Claim Policy

Serial HIL logs are useful evidence, but they are not visual validation by
themselves. A field-ready claim requires the completed hardware matrix with
serial logs, visual evidence, fault/recovery notes, reset behavior where
applicable, and bounded soak results.

Use `unknown` instead of guessing hardware setup fields. Leave untested matrix
rows as `Not run`.

Do not claim SSD1306 compatibility from SSD1315 tests. The repository targets
SSD1315 only unless a separate compatibility profile is added and validated.

## Local Artifacts

Do not commit generated HIL logs, photos, package tarballs, PlatformIO build
directories, or Doxygen output unless a specific release process says to do so.
Store large media outside the repository and reference it from the validation
matrix.
