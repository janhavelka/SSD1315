# Modes, Interrupts, Status, And Faults

SSD1315 has no interrupt output in the typical I2C display module use case. The command table includes a parallel-interface status read bit for display ON/OFF, but the datasheet states that no data read is provided in serial mode operation; common I2C module wiring is therefore command/write oriented. Source: SSD1315 datasheet, p. 46.

Modes and state:

| Area | Options / behavior | Source |
|---|---|---|
| Addressing mode | Page, horizontal, and vertical addressing controlled by `0x20`; column/page windows by `0x21`/`0x22`. | SSD1315 datasheet, pp. 47-49 |
| Display mode | Normal/inverse, entire-display-on, display off/on. | SSD1315 datasheet, pp. 38, 50 |
| Remap/orientation | Segment remap and COM scan direction commands affect how GDDRAM maps to glass. | SSD1315 datasheet, pp. 38-39, 49-50 |
| Charge pump | `0x8D` controls internal charge pump; valid enable values depend on output mode. | SSD1315 datasheet, pp. 40, 56 |
| Reset defaults | Display off, 128 x 64 mode, normal SEG/COM mapping, start line 0, column counter 0, contrast `0x7F`. | SSD1315 datasheet, p. 19 |

Fault handling is mostly transport-level:

- NACK on address/control/data is an I2C transport failure.
- If SDAOUT is not connected on a module, the acknowledgement signal is ignored in the I2C bus per the datasheet. Source: SSD1315 datasheet, p. 15.
- Visual faults such as blank display usually come from power sequencing, charge-pump settings, contrast/precharge/VCOMH, orientation/window setup, or framebuffer data rather than readable status bits.
