# Variants And Open Questions

Controller versus module source split:

| Topic | SSD1315 datasheet | Wisevision module spec |
|---|---|---|
| Controller commands and GDDRAM | Primary source | Uses controller command behavior in examples |
| I2C address/control byte | Primary source | Confirms I2C-mode pin roles |
| Power rails and charge pump theory | Primary source | Board/module-specific voltage and sequence examples |
| Pin mapping | Controller-level pin descriptions | Module connector pinout and tie-off guidance |

Not documented in PDFs / repository policy choices:

- The PDFs do not define a repository default initialization profile; they provide controller commands and Wisevision module examples.
- The PDFs document page, horizontal, and vertical addressing modes; they do not choose one software abstraction for framebuffer flushes.
- The SSD1315 datasheet says `SDAOUT` may be disconnected and ACK ignored; it does not define host error policy for missing ACK in that wiring.
- The PDFs document segment remap and COM scan commands, but do not define first-class rotation API names.
- The PDFs document external-VCC and internal-charge-pump application circuits; they do not define automatic charge-pump selection policy for software.

Verified source erratum/open question:

- On SSD1315 controller PDF physical page 24, the charge-pump shutdown prose
  in section 6.9.2 specifies `0x8D,0x10`, but Figure 6-19 labels the command
  `8Dh 00h`. The figure discrepancy is visible in the rendered PDF even though
  the text extraction omits it. The command table fixes D4=1 and A2=0 for
  disabled, which encodes `0x10`; the driver follows the prose and command
  table with `0x8D,0x10`.

Raw extraction remains in `docs/pdf-extracted-md` for verification.
