# Protocol Commands And Transactions

I2C address byte format is `011110 SA0 R/W`. The 7-bit addresses are `0x3C` when SA0 is low and `0x3D` when SA0 is high. Source: SSD1315 datasheet, p. 15.

After the address, the master sends a control byte followed by command or data bytes. Source: SSD1315 datasheet, p. 16.

| Control byte field | Meaning | SSD1315 behavior | Source |
|---|---|---|---|
| `Co` | Continuation bit | `0` means following bytes are data bytes only under that control context. | SSD1315 datasheet, p. 16 |
| `D/C#` | Data/command select | `0` means following byte is command; `1` means following byte is display data written to GDDRAM. | SSD1315 datasheet, p. 16 |
| lower six bits | Zero | Control byte lower bits are six zeros. | SSD1315 datasheet, p. 16 |

Common control bytes:

| Byte | Meaning |
|---:|---|
| `0x00` | Command stream, continuation clear. |
| `0x40` | Data stream, continuation clear. |
| `0x80` | Single command follows, continuation set for another control byte. |
| `0xC0` | Single data byte follows, continuation set for another control byte. |

GDDRAM write behavior: after a data byte is written, the GDDRAM column address pointer increments automatically. Source: SSD1315 datasheet, pp. 16, 46-49.

For a full 128 x 64 update in horizontal addressing mode, set `0x20,0x00`, column range `0x21,0x00,0x7F`, page range `0x22,0x00,0x07`, then send 1024 GDDRAM data bytes under control byte `0x40`. Source: SSD1315 datasheet, pp. 16, 21, 47-49.
