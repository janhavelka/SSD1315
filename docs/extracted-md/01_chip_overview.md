# Chip Overview

SSD1315 is a single-chip CMOS OLED/PLED segment/common driver with controller for 128 x 64 dot-matrix displays. It has internal 128 x 64-bit GDDRAM, 256-step contrast control, command decoder, oscillator/display timing generator, reset circuit, and selectable MCU interfaces including I2C. Source: SSD1315 datasheet, pp. 6, 15, 18-21.

The Wisevision X096-2864KSWPG01-H30 module is a 128 x 64 OLED module using SSD1315-class command behavior and exposes an interface selectable for I2C among other modes. Source: module spec, pp. 1, 5-7.

Key documented facts:

| Item | Fact | Source |
|---|---|---|
| Resolution | 128 columns x 64 rows | SSD1315 datasheet, p. 6 |
| GDDRAM | 128 x 64 bits, arranged as 8 pages of 8-pixel-high rows | SSD1315 datasheet, p. 21 |
| Interfaces | I2C, 3-wire/4-wire SPI, 6800/8080 parallel | SSD1315 datasheet, pp. 11-16 |
| I2C addresses | `0x3C` or `0x3D` selected by SA0 | SSD1315 datasheet, p. 15 |
| Command/data distinction | I2C control byte uses D/C# bit; display data increments the GDDRAM column pointer | SSD1315 datasheet, p. 16 |
| Display power | Supports external VCC or internal charge-pump application | SSD1315 datasheet, pp. 23-25, 40 |

The I2C-visible interface is command writes plus GDDRAM data writes. In SSD1315 I2C mode `SDAIN` must be connected for SDA; `SDAOUT` may be disconnected, and when it is disconnected the acknowledgement signal is ignored on the I2C bus. Source: SSD1315 datasheet, p. 15; module spec, pp. 6-7.
