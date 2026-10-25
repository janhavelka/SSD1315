# SSD1315 Documentation Map

## Maintained Documents

- `../README.md`: public API, usage, build, and validation commands.
- `../CHANGELOG.md`: release-facing summary of public changes.
- `../CONTRIBUTING.md`: architecture, documentation, and validation expectations.
- `../SECURITY.md`: supported-release and reporting policy.
- `../AGENTS.md`: repository engineering rules.
- `RELEASING.md`: release preparation, exact-commit CI, tagging, and publication.
- `IDF_PORT.md`: ESP-IDF component and native example notes.
- [RETAINED_BRANCH_AUDIT.md](RETAINED_BRANCH_AUDIT.md): current retained-branch
  disposition, historical-finding reconciliation and remaining acceptance work.
- [Software follow-up prompt](https://github.com/janhavelka/SSD1315/blob/main/docs/prompts/README.md):
  active implementation instructions for P2-T; separate from the API contract.
- `SSD1315_CONTROLLER_REFERENCE.md`: controller command, timing, power-sequence,
  GDDRAM, and Wisevision module facts the driver relies on.
- `SSD1315_HIL_RUNBOOK.md`: hardware-in-the-loop procedure, runner options, and
  the per-target evidence form.
- `SSD1315_HARDWARE_VALIDATION.md`: dated hardware evidence, setup and artifact
  identity, measured outcomes, limitations, and remaining qualification work.

## Source Evidence

- `SSD1315_datasheet.pdf`: controller datasheet.
- `Wisevision_X096-2864KSWPG01-H30_module_spec.pdf`: module reference.
- `extracted-md/`: compact notes extracted from the PDFs; shipped in release
  packages.
- `pdf-extracted-md/`: full extracted PDF text for repository-local search;
  excluded from release packages together with the PDFs.

## Rules

- Public symbol contracts live beside declarations in `include/ssd1315/`.
  README summarizes usage and behavior; supporting documents do not repeat it.
- Hardware claims name the exact setup, revision, command coverage, fault
  cases, and duration. Serial HIL evidence proves that commands completed over
  I2C; it does not prove visual correctness, fault recovery, retention, reset
  wiring, or long-duration field behavior. Use `Not run` for anything not
  executed.
- Completed task prompts, audit diaries, build transcripts and implementation
  plans belong in Git history or local artifacts. Preserve their useful API
  contracts in public headers/guides and measured hardware evidence in the
  validation ledger before removing standalone work reports.
- `doxygen Doxyfile` checks public headers and maintained Markdown with warnings
  treated as errors. Install Doxygen and Graphviz; source extracts are retained
  for reference and excluded from generated API pages.
- Generated output (`docs/doxygen/`, `.pio/`, `hil_logs/`, package archives)
  is never committed.
