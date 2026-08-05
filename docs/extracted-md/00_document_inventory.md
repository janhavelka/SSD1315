# Document Inventory

These compact notes summarize SSD1315 controller and Wisevision
X096-2864KSWPG01-H30 module facts. The repository retains the source PDFs and
raw extraction under `docs/pdf-extracted-md`; release packages contain only
these curated notes.

| Source PDF | Raw extract | Pages used | Notes |
|---|---|---:|---|
| `docs/SSD1315_datasheet.pdf` | `docs/pdf-extracted-md/SSD1315_datasheet.md` | 1-62 | Primary controller source. Physical pages 1-36 are Rev 1.1 controller material; appended pages 37-62 contain Rev 1.0 command-table material. Cite the physical PDF page and visible revision/page label when ambiguity matters. |
| `docs/Wisevision_X096-2864KSWPG01-H30_module_spec.pdf` | `docs/pdf-extracted-md/Wisevision_X096-2864KSWPG01-H30_module_spec.md` | 1-39 | Module source: 128x64 OLED module, pin assignment, I2C mode wiring, electrical/application notes. The raw machine extraction has doubled glyphs and drops some symbols; rendered PDF page 6 says `12.5 µA maximum`, not the extracted `12.5A maximum`. |

Compact note set:

| File | Purpose |
|---|---|
| `01_chip_overview.md` | Controller/module overview. |
| `02_pinout_and_signals.md` | I2C pins, SA0/D-C#, reset, charge-pump and module wiring notes. |
| `03_electrical_and_timing.md` | Supply ranges, I2C timing, reset and power sequencing. |
| `04_protocol_commands_and_transactions.md` | I2C address/control byte and command/data transactions. |
| `05_register_map.md` | GDDRAM layout and command groups. |
| `06_modes_interrupts_status_and_faults.md` | Addressing modes, display modes, reset defaults, no interrupt/status model. |
| `07_initialization_reset_and_operational_notes.md` | Typical init and operational notes. |
| `08_variant_differences_and_open_questions.md` | Module/controller differences and unresolved choices. |

Repository files under `pdf-extracted-md/` are deliberately retained as raw
search transcripts. Do not silently repair OCR in those files; record verified
corrections in these curated notes and check the rendered PDF. They and the
vendor PDFs are excluded from release packages.
