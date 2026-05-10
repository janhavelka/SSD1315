# Pinout And Signals

Controller pins are multiplexed by selected bus mode. In I2C mode the relevant SSD1315 and module signals are below.

| Signal | I2C-mode role | Notes | Source |
|---|---|---|---|
| `SDA` / `SDAIN` / `SDAOUT` | I2C data | SDA and SCL require pull-ups. SDAIN must be connected for SDA; SDAOUT may be disconnected, but then ACK is ignored. | SSD1315 datasheet, p. 15 |
| `SCL` / `D0` | I2C clock | Clock for I2C transfers. | SSD1315 datasheet, p. 15; module spec, pp. 6-7 |
| `D/C#` / `SA0` | I2C address select | In I2C mode this pin acts as SA0, selecting 7-bit address `0x3C` when low or `0x3D` when high. | SSD1315 datasheet, p. 15; module spec, p. 6 |
| `RES#` | Reset input | Used to initialize the device. Low resets controller state. | SSD1315 datasheet, pp. 15, 19 |
| `CS#`, `R/W#`, `E/RD#` | Non-I2C controls | Module spec ties unused serial/I2C-mode controls to defined levels; follow module wiring table. | module spec, pp. 6-7 |
| `VDD` | Logic supply | Module spec lists 1.65 V to 3.3 V logic supply, typical 2.8 V. | module spec, p. 9 |
| `VBAT`, `VCC`, charge pump pins | OLED/charge-pump power | Internal charge-pump applications use VBAT and flying capacitors; external VCC applications wire power differently. | SSD1315 datasheet, pp. 23-25; module spec, pp. 9, 20 |
| `IREF` | Segment current reference | Module spec states external resistor/current setting requirements. | module spec, p. 6 |

The module pinout includes many parallel/SPI data pins that are tied or repurposed in I2C mode. Do not infer I2C wiring only from controller pin names; use the module spec for board wiring. Source: module spec, pp. 5-7.
