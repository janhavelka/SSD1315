# Electrical And Timing

| Parameter | Value | Source |
|---|---:|---|
| Controller logic VDD | 1.65 V to 3.5 V | SSD1315 datasheet, p. 6 |
| Module logic VDD | 1.65 V to 3.3 V, typical 2.8 V | module spec, p. 9 |
| Controller charge-pump VBAT | 3.0 V to 4.5 V | SSD1315 datasheet, pp. 6, 62 |
| Module internal DC/DC VBAT | 3.5 V to 4.2 V | module spec, p. 9 |
| I2C address choices | `0x3C` or `0x3D` | SSD1315 datasheet, p. 15 |
| I2C clock cycle | 2.5 us min, equivalent to 400 kHz max | SSD1315 datasheet, p. 33 |
| I2C start hold / repeated-start setup / stop setup | 0.6 us min each | SSD1315 datasheet, p. 33 |
| I2C SDAOUT hold / SDAIN hold | 0 ns min / 300 ns min | SSD1315 datasheet, p. 33 |
| I2C data setup | 100 ns min | SSD1315 datasheet, p. 33 |
| I2C rise/fall time | 300 ns max each | SSD1315 datasheet, p. 33 |
| I2C idle before new transmission | 1.3 us min | SSD1315 datasheet, p. 33 |
| Reset state | Display off, 128 x 64 mode, normal mapping, column counter 0, contrast `0x7F` | SSD1315 datasheet, p. 19 |

Power sequencing matters:

- For external VCC, the datasheet gives a power-on/off sequence with VDD and VCC ordered to avoid stressing internal protection structures. Source: SSD1315 datasheet, p. 23.
- For charge-pump applications, the datasheet and module spec provide separate power-on/off flows. Source: SSD1315 datasheet, p. 24; module spec, pp. 20-26.
- Module spec cautions that power pins such as VDD/VCC/VBAT must not be pulled to ground as part of power-off handling. Source: module spec, p. 20.

Power sequencing notes are board and module facts, not generic timing advice: external-VCC and internal-charge-pump circuits use different VDD/VCC/VBAT ordering, and the module spec warns that VDD, VCC, and VBAT must not be pulled to ground as a power-off method. Source: SSD1315 datasheet, pp. 23-25; module spec, p. 20.
