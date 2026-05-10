# Document Inventory

These compact notes summarize SSD1315 controller and Wisevision X096-2864KSWPG01-H30 module facts. Raw extraction remains in `docs/pdf-extracted-md`; this directory contains curated notes only.

| Source PDF | Raw extract | Pages used | Notes |
|---|---|---:|---|
| `docs/SSD1315_datasheet.pdf` | `docs/pdf-extracted-md/SSD1315_datasheet.md` | 1-62 | Primary controller source: interfaces, GDDRAM, commands, timing, reset, power sequencing. |
| `docs/Wisevision_X096-2864KSWPG01-H30_module_spec.pdf` | `docs/pdf-extracted-md/Wisevision_X096-2864KSWPG01-H30_module_spec.md` | 1-39 | Module source: 128x64 OLED module, pin assignment, I2C mode wiring, electrical/application notes. |

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
