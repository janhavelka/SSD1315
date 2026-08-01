# Initialization, Reset, And Operational Notes

The complete bring-up shape combines application-owned rail/reset sequencing
with the driver's SSD1315 command sequence:

1. Apply board-specific power sequencing for VDD and VBAT/VCC.
2. Assert and release `RES#` according to the module's reset timing.
3. Send `Display Off` (`0xAE`).
4. Configure display clock (`0xD5`), multiplex (`0xA8`), display offset (`0xD3`), start line, segment remap, COM scan direction, COM pins (`0xDA`), contrast (`0x81`), precharge (`0xD9`), VCOMH (`0xDB`), SSD1315 IREF (`0xAD`), and charge pump (`0x8D`) as required by the module.
5. Select horizontal memory addressing and clear or upload the framebuffer
   window. Horizontal mode/windowing is repository policy; the Wisevision
   examples use reset-default page addressing instead.
6. Send normal-display / RAM-display commands as desired, then `Display On` (`0xAF`).

Sources: SSD1315 datasheet, pp. 19, 23-24, 38-40, 47-56; module spec, pp. 20-26.

External-VCC sequencing powers VCC only after the VDD/reset timing and powers
VCC off before VDD. Charge-pump sequencing powers VBAT after VDD, enables the
pump with `0x8D` before `0xAF`, and shuts down with `0xAE`, then `0x8D,0x10`,
before VBAT/VDD rail removal. Rails and RES# remain application-owned.

Operational notes:

- Keep a software framebuffer if the API exposes pixels, text, or partial updates; the controller is write-oriented in common I2C module wiring.
- Use `0x21` and `0x22` to bound partial updates in horizontal or vertical addressing mode.
- For page-mode writes, set page `0xB0-0xB7` plus lower and higher column nibble commands before sending data.
- Module power design and glass mapping determine contrast, charge-pump, COM pins, and remap settings; the SSD1315 controller datasheet and Wisevision module spec are separate sources for those values.
