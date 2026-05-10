# Register Map

SSD1315 is command/GDDRAM based, not a conventional register map device. Use command constants and GDDRAM addressing.

GDDRAM:

| Region | Layout | Source |
|---|---|---|
| Display memory | 128 columns x 64 rows | SSD1315 datasheet, pp. 6, 21 |
| Page addressing | 8 pages, each page is 8 vertical pixels high | SSD1315 datasheet, pp. 21, 47 |
| Horizontal/vertical addressing | Set column range with `0x21`, page range with `0x22` | SSD1315 datasheet, pp. 48-49 |

Core command groups:

| Command(s) | Purpose | Source |
|---|---|---|
| `0x00-0x0F`, `0x10-0x17` | Lower/higher column start address in page mode | SSD1315 datasheet, pp. 38, 47 |
| `0x20` | Memory addressing mode | SSD1315 datasheet, pp. 38, 47 |
| `0x21`, `0x22` | Column and page address ranges | SSD1315 datasheet, pp. 38, 48-49 |
| `0x40-0x7F` | Display start line | SSD1315 datasheet, pp. 38, 49 |
| `0x81` | Contrast control | SSD1315 datasheet, pp. 38, 49 |
| `0xA0/0xA1` | Segment remap | SSD1315 datasheet, pp. 38, 49 |
| `0xA4/0xA5` | Entire display follows RAM / all on | SSD1315 datasheet, p. 38 |
| `0xA6/0xA7` | Normal / inverse display | SSD1315 datasheet, pp. 38, 50 |
| `0xA8` | Multiplex ratio | SSD1315 datasheet, pp. 38, 50 |
| `0xAE/0xAF` | Display off / on | SSD1315 datasheet, pp. 38, 50 |
| `0xB0-0xB7` | Page start address in page mode | SSD1315 datasheet, pp. 38, 50 |
| `0xC0/0xC8` | COM scan direction | SSD1315 datasheet, pp. 39, 50 |
| `0xD3`, `0xD5`, `0xD9`, `0xDA`, `0xDB` | Offset, clock, precharge, COM pins, VCOMH | SSD1315 datasheet, pp. 39, 50-56 |
| `0x8D` | Charge pump setting | SSD1315 datasheet, pp. 40, 56 |
| `0x26/0x27/0x29/0x2A/0x2E/0x2F` | Scroll setup/deactivate/activate | SSD1315 datasheet, pp. 41-44, 57-60 |

Do not send undocumented command bit patterns; the command appendix warns that unsupported patterns are prohibited because behavior can be unexpected. Source: SSD1315 datasheet, p. 46.
