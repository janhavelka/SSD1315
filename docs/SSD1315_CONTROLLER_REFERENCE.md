# SSD1315 Controller Reference

Controller and module facts the driver relies on. Driver usage, configuration
fields, and error codes are documented in `README.md`; this file records only
the hardware contract behind them.

## 1. Sources

| Source | File | Notes |
| --- | --- | --- |
| Solomon Systech SSD1315 datasheet, Rev 1.1, Apr 2017 | `docs/SSD1315_datasheet.pdf` | Appendix IV (command tables and command descriptions) carries Rev 1.0, Dec 2016 page headers |
| Wisevision `X096-2864KSWPG01-H30` module specification, Ver A, 2022-03-28 | `docs/Wisevision_X096-2864KSWPG01-H30_module_spec.pdf` | Reference module; lists Driver IC: SSD1315 |

Text extractions of both PDFs are in `docs/pdf-extracted-md/`. Page references
below are to the `## Page N` headings of those extracted markdown files, not to
the printed page numbers. Where a value exists only in a figure or image that
the extraction did not capture, it is marked "not stated in the extraction".

## 2. I2C protocol

Datasheet section 6.1.5 (extraction pages 15-16), Table 1-2 and section 1.1
(page 46), section 1.2.22 (page 56), Table 9-6 (page 33).

### Addressing and framing

| Item | Value |
| --- | --- |
| 7-bit slave address | `0111 10 SA0` = `0x3C` (SA0 = 0) or `0x3D` (SA0 = 1) |
| SA0 source | The D/C# pin acts as SA0 in I2C mode; board wiring selects the address |
| 8-bit forms | `0x78` / `0x7A` are the address-plus-R/W# byte, not the 7-bit address |
| Pin roles (I2C mode) | D2 = SDAOUT, D1 = SDAIN (tied together as SDA), D0 = SCL; other D pins tied LOW; RES# still required |
| Control byte | `Co D/C# 0 0 0 0 0 0` |
| `0x00` | Co = 0, D/C# = 0: all following bytes are commands |
| `0x40` | Co = 0, D/C# = 1: all following bytes are GDDRAM data |
| `0x80` / `0xC0` | Co = 1 variants: another control byte follows the next byte (command / data) |
| GDDRAM pointer | Column address pointer increments automatically after each data byte |

The driver sends every transaction as `START, address+W, one control byte
(0x00 or 0x40), N bytes, STOP`. It never emits `0x80` / `0xC0`; each bounded
command list or data chunk is its own transaction with a single control byte.

### Acknowledge caveat

- The controller ACKs after the address byte and after each control or data
  byte.
- SDAIN must be connected; SDAOUT may be disconnected. When SDAOUT is
  disconnected the acknowledge is ignored on the bus, so a module can be
  functional while never driving ACK.
- ITO track resistance in series with the SDA pull-up forms a divider, so the
  ACK low level may not reach a valid logic 0.

Consequence: an address ACK proves only that something acknowledged the
address; a missing ACK does not by itself prove the controller is absent.

### Write-only over I2C

- Table 1-2 (Status Register Read): "No status read is provided for serial
  mode."
- Section 1.1 (Data Read/Write): "No data read is provided in serial mode
  operation."

Status read and GDDRAM readback exist only on the parallel interfaces. Over
I2C the controller is write-only; there is no documented way to read back
controller state or RAM.

### Timing (datasheet Table 9-6, I2C Interface Timing Characteristics)

| Symbol | Parameter | Min | Max | Unit |
| --- | --- | --- | --- | --- |
| tcycle | Clock cycle time | 2.5 | - | us |
| tHSTART | Start condition hold time | 0.6 | - | us |
| tHD | Data hold time, SDAOUT pin | 0 | - | ns |
| tHD | Data hold time, SDAIN pin | 300 | - | ns |
| tSD | Data setup time | 100 | - | ns |
| tSSTART | Start condition setup time (repeated start only) | 0.6 | - | us |
| tSSTOP | Stop condition setup time | 0.6 | - | us |
| tR | Rise time, data and clock | - | 300 | ns |
| tF | Fall time, data and clock | - | 300 | ns |
| tIDLE | Idle time before a new transmission | 1.3 | - | us |

tcycle >= 2.5 us gives a maximum SCL of 400 kHz. This table is the
controller datasheet's; the module specification's I2C timing table
(section 3.3.5.1) is an image and its values are not stated in the
extraction.

## 3. Commands used by the driver

Datasheet Appendix IV, Table 1-1 and the charge-pump, scrolling, and advance
graphic tables (extraction pages 37-45), with constraints from the command
descriptions (pages 49-62). "POR" is the value after RES#. Only patterns listed
in the command tables may be sent; the datasheet prohibits other patterns.

### Fundamental commands

| Hex | Command | Parameter bytes | Valid range | POR | Constraints |
| --- | --- | --- | --- | --- | --- |
| `0x20` | Set memory addressing mode | A: `0x00` horizontal, `0x01` vertical, `0x02` page | `0x00`..`0x02`; `0x03` invalid | `0x02` (page) | `0x21`/`0x22` are only for horizontal or vertical mode |
| `0x21` | Set column address | A start, B end | 0..127 each | A = 0, B = 127 | Horizontal/vertical mode only |
| `0x22` | Set page address | A start, B end | 0..7 each | A = 0, B = 7 | Horizontal/vertical mode only |
| `0x40`..`0x7F` | Set display start line | In low 6 bits | 0..63 | 0 | Must be < scroll-area rows B of `0xA3` |
| `0x81` | Set contrast | A | `0x01`..`0xFF` | `0x7F` | `0x00` is outside the valid range |
| `0xA0` / `0xA1` | Segment remap | none | - | `0xA0` (column 0 -> SEG0) | `0xA1` maps column 127 -> SEG0. Affects only subsequent data input; data already in GDDRAM is not changed |
| `0xA4` / `0xA5` | Resume RAM / entire display ON | none | - | `0xA4` | `0xA5` ignores RAM content |
| `0xA6` / `0xA7` | Normal / inverse | none | - | `0xA6` | - |
| `0xA8` | Set multiplex ratio | A[5:0] = N, MUX = N + 1 | 15..63 (16..64 MUX); 0..14 invalid | 63 (64 MUX) | - |
| `0xAD` | IREF selection (SSD1315) | A: `0x00` external, `0x10` internal 19 uA (ISEG max 150 uA), `0x30` internal 30 uA (ISEG max 240 uA) | `0x00`, `0x10`, `0x30` | `0x00` (external, 19 uA setting) | Internal IREF is enabled only during display ON; when internal IREF is used the IREF pin is left NC |
| `0xAE` / `0xAF` | Display OFF / ON | none | - | `0xAE` | OFF: SEG outputs at VSS, COM outputs high impedance |
| `0xC0` / `0xC8` | COM scan direction | none | - | `0xC0` (COM0 -> COM[N-1]) | `0xC8` scans COM[N-1] -> COM0. Takes effect immediately on displayed content |
| `0xD3` | Set display offset | A[5:0] | 0..63 | 0 | Vertical shift by COM |
| `0xD5` | Clock divide / oscillator | A[3:0] = divide ratio - 1, A[7:4] = FOSC trim | divide 1..16, trim 0..15 | `0x80` (divide 1, trim `0x8`) | FFRM = FOSC / (D x K x 64); FOSC 620..756 kHz (typ 688 kHz at VDD 2.8 V, default trim); K = 103 at POR |
| `0xD9` | Set pre-charge period | A[3:0] phase 1, A[7:4] phase 2 | each nibble 1..15; 0 invalid | `0x22` | Command table: code N = 2N DCLK ("up to 30 DCLK, i.e. 2, 4, ... 30"); see unit note below |
| `0xDA` | COM pins configuration | A: bit 4 = alternative, bit 5 = left/right remap, bits [1] fixed 1 | `0x02`, `0x12`, `0x22`, `0x32` | `0x12` (alternative, no remap) | - |
| `0xDB` | VCOMH deselect level | A: `0x00` ~0.65 VCC, `0x10` ~0.71 VCC, `0x20` ~0.77 VCC, `0x30` ~0.83 VCC | those four | `0x20` | - |
| `0xE3` | NOP | none | - | - | - |

### Charge pump

| Hex | Command | Parameter | Valid values | POR | Constraints |
| --- | --- | --- | --- | --- | --- |
| `0x8D` | Charge pump setting | A: bit 2 enable, bits 7/0 output mode | `0x10` disabled, `0x14` 7.5 V, `0x94` 8.5 V, `0x95` 9.0 V | disabled (`0x10`); mode bits POR = 7.5 V | Must be enabled as `0x8D` followed by `0x14`, `0x94`, or `0x95`, then (after any other setup) `0xAF`; pump is active only during display ON. Disable sequence is `0xAE` then `0x8D 0x10` |

### Scrolling

| Hex | Command | Byte layout (after the command byte) | Constraints |
| --- | --- | --- | --- |
| `0x26` / `0x27` | Continuous right / left horizontal scroll setup (7 bytes total) | A dummy `0x00`; B start page 0..7; C interval code; D end page 0..7; E start column 0..127 (POR 0); F end column 0..127 (POR 127) | D >= B; F >= E; scroll must be deactivated (`0x2E`) before setup or RAM may be corrupted; designed for 128-column scrolling |
| `0x29` / `0x2A` | Continuous vertical + right / left horizontal scroll setup (8 bytes total) | A[0] horizontal column offset 0/1; B start page; C interval code; D end page; E vertical offset 0..63; F start column; G end column | D >= B; G >= F; E < scroll-area rows B of `0xA3`; scroll must be deactivated before setup |
| `0x2C` / `0x2D` | Content scroll right / left by one column (7 bytes total) | A dummy `0x00`; B start page; C dummy `0x01`; D end page; E start column; F end column | D >= B; F > E; a 2-frame delay is required between consecutive `0x2C` / `0x2D` |
| `0x2E` | Deactivate scroll | none | After `0x2E` the RAM data needs to be rewritten |
| `0x2F` | Activate scroll | none | Valid only after `0x26`, `0x27`, `0x29`, or `0x2A`; the last setup sent wins. After activation, RAM access (write or read) and changing the horizontal scroll parameters are prohibited |
| `0xA3` | Set vertical scroll area | A top fixed rows 0..63 (POR 0); B scroll rows (POR 64) | A + B <= MUX; B <= MUX; vertical offset of `0x29`/`0x2A` < B; display start line < B |

Interval code C[2:0] for `0x26`/`0x27`/`0x29`/`0x2A`:

| Code | `0x00` | `0x01` | `0x02` | `0x03` | `0x04` | `0x05` | `0x06` | `0x07` |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Frames per step | 6 | 32 | 64 | 128 | 3 | 4 | 5 | 2 |

### Advance graphic

| Hex | Command | Parameter | Valid values | POR | Constraints |
| --- | --- | --- | --- | --- | --- |
| `0x23` | Fade out / blinking | A[5:4]: `00` off, `10` fade out, `11` blink; A[3:0] step interval = 8 x (code + 1) frames (8..128) | `0x00`, `0x20`..`0x2F`, `0x30`..`0x3F` | `0x00` | Output follows RAM when fade is disabled |
| `0xD6` | Zoom in | A[0] | `0x00` off, `0x01` on | `0x00` | Requires alternative COM pin configuration (`0xDA` bit 4 = 1); rows 0..31 are expanded to fill 64 rows |

### Pre-charge unit inconsistency

The datasheet is internally inconsistent about `0xD9` units:

- Command table (page 39) and section 1.2.18 (page 53): each nibble code N is
  2N DCLK ("up to 30 DCLK, i.e. 2, 4, 6 ... 30"; "RESET equals 4 DCLKs").
- Section 6.3 (page 18): "K = Phase 1 period + Phase 2 period + K0 = 2 + 2 + 99
  = 103 at power on reset", counting the POR code 2 as 2 DCLK.
- Section 6.6 (page 20): "The period of phase 2 can be programmed in length from
  1 to 16 DCLKs."

The driver follows the command table: `prechargePhase1/2` are register codes
1..15 and code N encodes 2N DCLK.

## 4. Power, reset, and POR state

Datasheet section 6.9 (pages 23-24), section 6.5 (page 19), Table 9-1
(page 28).

### Power ON / OFF with external VCC (6.9.1)

Power ON:

1. Power ON VDD.
2. After VDD is stable, wait at least 20 ms (t0); set RES# LOW for at least
   3 us (t1), then HIGH.
3. After setting RES# LOW, wait at least 3 us (t2). Then power ON VCC.
4. After VCC is stable, send `0xAF`. "SEG/COM will be ON after 100ms (tAF)."

Power OFF:

1. Send `0xAE`.
2. Power OFF VCC.
3. Power OFF VDD after tOFF (minimum 0 ms, typical 100 ms).

### Power ON / OFF with internal charge pump (6.9.2)

Power ON:

1. Power ON VDD.
2. Wait tON (minimum 0 ms), power ON VBAT.
3. After VDD is stable, wait at least 20 ms (t0); set RES# LOW for at least
   3 us (t1), then HIGH.
4. After setting RES# LOW, wait at least 3 us (t2). Then send `0x8D` (enable
   charge pump) and `0xAF`.
5. "SEG/COM will be ON after 100ms (tAF)."

Power OFF:

1. Send `0xAE`.
2. Send `0x8D 0x10` to disable the charge pump.
3. Power OFF VBAT after tOFF (typical 100 ms).
4. Power OFF VDD after tOFF2 (minimum 0 ms, typical 5 ms).

Source erratum: on physical PDF page 24, Figure 6-19 labels the shutdown
command `8Dh 00h`, while section 6.9.2 prose specifies `8Dh 10h`. The command
table also requires `0x10` for charge-pump disable. The driver uses `0x8D 0x10`;
the figure is inconsistent even though its label is absent from text extraction.

Notes common to both sequences: VCC / VBAT must be left floating (disabled)
when off; VDD, VCC, and VBAT must never be pulled to ground; VDD must not be
powered off before VCC / VBAT; register values are reset after t1.

### tAF and the current software display-on guard

tAF = 100 ms is the time after `0xAF` until the SEG/COM outputs turn on. This
describes output latency; the cited sequence does not explicitly prohibit
host commands or GDDRAM writes during that interval.

The current driver additionally gates GDDRAM transfers until the display-on
guard completes: `startFlush()` rejects admission in `PanelPowerState::STARTING`,
and a pending compatibility flush waits in `_pollFlushInternal()`.
`Config::displayOnDelayMs` (default 100 ms) controls the modeled transition
`STARTING -> ON`. A cooperative wake retains operation ownership during
`DISPLAY_ON_DELAY`, so another operation cannot be admitted until its terminal
result is consumed. The wait itself does not hold the I2C bus.

### Reset

| Item | Value | Source |
| --- | --- | --- |
| VDD stable before RES# | >= 20 ms (t0) | 6.9 |
| RES# low pulse width | >= 3 us (t1) | Table 9-1, 6.9 |
| Wait after RES# low before VCC / commands | >= 3 us (t2) | 6.9 |
| Register reset | Registers are reset after t1 | 6.9 note |

### State after RES# (6.5)

1. Display OFF.
2. 128 x 64 display mode (64 MUX).
3. Normal segment and COM mapping (column 0 -> SEG0, row 0 -> COM0).
4. Serial-interface shift register cleared.
5. Display start line 0.
6. Column address counter 0.
7. Normal COM scan direction.
8. Contrast `0x7F`.
9. Normal display mode (equivalent to `0xA4`).

Together with the command-table POR values: page addressing mode, display
offset 0, clock `0x80`, pre-charge `0x22`, COM pins `0x12`, VCOMH `0x20`,
charge pump disabled, external IREF, no scroll, fade off, zoom off, vertical
scroll area 0 / 64.

## 5. GDDRAM layout

Datasheet section 6.7 (page 21), sections 1.2.1-1.2.5 (pages 47-49), and
section 1.2.8, Set Segment Re-map (physical page 49).

- 128 x 64 bits organised as 8 pages (PAGE0..PAGE7) of 128 columns; one byte
  per column per page.
- Within a byte, D0 (LSB) is the top row of the page and D7 (MSB) the bottom
  row. Writing one byte fills all 8 rows of that column in the current page.
- Horizontal addressing mode (`0x20 0x00`): after each data byte the column
  pointer increments; at the column end address it wraps to the column start
  address and the page pointer increments; at page end it wraps to the page
  start. `0x21` and `0x22` bound the window.
- The driver sets horizontal addressing once at initialisation and, for each
  page it flushes, sends `0x21 minCol maxCol`, `0x22 page page`, then the data
  bytes under control byte `0x40`. Each page gets its own window, so the flush
  does not depend on end-of-window wrap.

Effect of mapping commands on a 128 x 64 panel:

| Command | Effect |
| --- | --- |
| `0xA0` | Column address 0 -> SEG0 (POR) |
| `0xA1` | Column address 127 -> SEG0; applies to data written afterwards only |
| `0xC0` | PAGE0 -> COM0..COM7 ... PAGE7 -> COM56..COM63 (POR) |
| `0xC8` | PAGE0 -> COM63..COM56 ... PAGE7 -> COM7..COM0; applied immediately to displayed content |
| `0xDA 0x02` | Sequential COM: odd/even rows driven from the same side |
| `0xDA 0x12` | Alternative COM (POR): the arrangement used by common 128 x 64 modules and required for zoom |
| `0xDA 0x22` / `0x32` | Same as above with COM left/right remap |

For a changed `setFlipX()` value, the driver invalidates the synchronized
baseline and marks the active RAM buffer dirty. In full-buffer mode the caller
must admit a complete rewrite; in page-buffer mode the caller must render and
flush every window through a new iteration. A changed remap during an active
page iteration returns `BUSY` before I2C, so `nextPage()` cannot certify a frame
that spans remap configurations. Successful same-value calls preserve the
baseline and dirty state, subject to normal direct-command admission.

If a full-buffer flush building a missing baseline fails, times out or is
cancelled through the cooperative `cancelOperation()` API, the entire
framebuffer is marked dirty again. Only the application admits a later retry.
A successful complete rewrite retains any concurrent mutation as dirty data;
synchronization and wake still require those changes to be flushed.

`invalidatePanelState()` also invalidates cached controls and power. Its
cancellation of a legacy flush preserves the existing dirty mask and requires
full-buffer resync/recovery before normal flushing resumes; it does not
perform that recovery or rewrite data itself.

## 6. Wisevision X096-2864KSWPG01-H30 module

Module specification: identity (extraction pages 1 and 5), 3.2 DC characteristics (page 9), 3.3.5.1 (page 18), 4.2 and 4.3 (page 20), 4.4.1 (pages 21-24), 4.4.2 (pages 25-26).

### Identity

| Item | Value |
| --- | --- |
| Part | X096-2864KSWPG01-H30, Ver A, 2022-03-28 |
| Driver IC | SSD1315 |
| Panel | 128 x 64 monochrome white, COG, 1/64 duty |
| Interfaces | 8-bit 68xx/80xx parallel, 3-/4-wire SPI, I2C (BS[2:0] = 010 selects I2C) |

### Electrical limits (section 3.2, DC characteristics)

| Rail | Min | Typ | Max | Unit |
| --- | --- | --- | --- | --- |
| VDD (logic) | 1.65 | 2.8 | 3.3 | V |
| VCC supplied externally (internal DC/DC disabled) | 8.5 | 9.0 | 9.5 | V |
| VBAT (internal DC/DC enabled) | 3.5 | - | 4.2 | V |
| VCC generated by internal DC/DC | 7.0 | - | 7.5 | V |
| IDD operating | - | 160 | 220 | uA |
| ICC operating, external VCC | - | 9 | 15 | mA |
| IBAT operating, internal DC/DC | - | 27.0 | 32.0 | mA |
| IDD / ICC sleep | - | - | 10 | uA |

VDD should equal the MCU I/O voltage. VBAT must be connected to VDD when the
internal converter is not used.

### Module power sequences (section 4.2)

Power up: 1) power up VDD / VBAT; 2) send display off; 3) initialisation;
4) clear screen; 5) power up VCC; 6) delay 100 ms (VCC stable); 7) send
display on.

Power down: 1) send display off; 2) power down VCC / VBAT; 3) delay 100 ms
(VCC / VBAT at 0, panel discharged); 4) power down VDD. The internal-DC/DC
flowchart (4.4.2) additionally sends `0x8D 0x10` after `0xAE` and before
powering down VBAT.

The module notes repeat the controller rules: VCC / VBAT float when off; never
pull VDD, VCC, or VBAT to ground; do not power down VDD before VCC / VBAT. The
module reset state list (4.3) is identical to datasheet section 6.5.

### Module initialisation sequence (section 4.4, verbatim byte order)

From the `External setting` / `Internal setting(Charge pump)` C listings and
the 4.4.1 / 4.4.2 flowcharts. The listings toggle RES# high-low-high first.

| Bytes | Meaning |
| --- | --- |
| `0xAE` | Display off |
| `0x00` | Lower column start address 0 (page mode) |
| `0x10` | Higher column start address 0 (page mode) |
| `0x40` | Display start line 0 |
| `0xB0` | Page start address 0 (page mode) |
| `0x81 0xB0` | Contrast `0xB0` |
| `0xA1` | Segment remap (column 127 -> SEG0) |
| `0xA4` | Resume to RAM content |
| `0xA6` | Normal display |
| `0xA8 0x3F` | 64 MUX |
| `0xC8` | COM scan COM63 -> COM0 |
| `0xD3 0x00` | Display offset 0 |
| `0xD5 0x90` | Divide ratio 1, oscillator trim 9 |
| `0xD9 0x22` | Pre-charge phase 1 = 2, phase 2 = 2 |
| `0xDA 0x12` | Alternative COM pins, no left/right remap |
| `0xDB 0x30` | VCOMH ~0.83 VCC |
| `0x8D 0x10` (external VCC) or `0x8D 0x14` (internal DC/DC) | Charge pump off / 7.5 V |
| `0xAF` | Display on |

The flowcharts add "Clear Screen" before `0xAF` and, for external VCC, power
up VCC after the configuration commands and before `0xAF`.

The module never sends `0xAD` or `0x20`: it uses the POR external IREF and the
POR page addressing mode, positioning the RAM pointer with `0x00`, `0x10`, and
`0xB0`.

### IREF

The module uses an external IREF resistor from pin 26 (IREF) to VSS
(pin-definition table, physical PDF page 6). That table specifies a maximum
of 12.5 uA; the raw extraction drops the micro symbol. Application circuits give
R1 = 910 kohm for the 12 V external-VCC examples and R1 = 620 kohm for the
9 V examples, from R1 = (voltage at IREF - VSS) / IREF. The datasheet
(section 6.8) states the IREF pin voltage is VCC - 3 V and recommends
IREF = 30 +/- 2 uA for ISEG = 240 uA at contrast 255. That controller-level
recommendation does not replace the module's stated current limit.

### Mapping to `applyPanelProfile()` Wisevision profiles

`PanelProfile::WISEVISION_X096_2864KSWPG01_H30_INTERNAL_DC_DC` and
`PanelProfile::WISEVISION_X096_2864KSWPG01_H30_EXTERNAL_VCC`
(`include/ssd1315/Config.h`):

| Module bytes | Config field | Value |
| --- | --- | --- |
| `0xA8 0x3F` | `width`, `height` | 128, 64 |
| `0x81 0xB0` | `contrast` | `0xB0` |
| `0xA1` | `flipX` | `true` |
| `0xC8` | `flipY` | `true` |
| `0xA6` | `invert` | `false` |
| `0xD3 0x00` | `displayOffset` | 0 |
| `0x40` | `startLine` | 0 |
| `0xD5 0x90` | `clockDivide`, `oscFrequency` | 1, 9 |
| `0xD9 0x22` | `prechargePhase1`, `prechargePhase2` | 2, 2 |
| `0xDA 0x12` | `comPins` | `ALTERNATIVE_NO_REMAP` |
| `0xDB 0x30` | `vcomh` | `V_083_VCC` |
| `0x8D 0x14` | `chargePumpVoltage` (INTERNAL_DC_DC profile) | `V7_5` |
| `0x8D 0x10` | `chargePumpVoltage` (EXTERNAL_VCC profile) | `OFF` |
| (no `0xAD`) | `iref` | `IREF_EXTERNAL` |

## 7. Driver-side policy

### Initialisation sequence

`_sendInitStep()` in `src/SSD1315.cpp` sends 17 transactions carrying 20
commands, in this order, and leaves the display off:

| Step | Bytes | Source of value |
| --- | --- | --- |
| 0 | `0xAE` | fixed |
| 1 | `0x20 0x00` | fixed (horizontal addressing) |
| 2 | `0x40` OR `startLine` (low 6 bits) | `startLine` |
| 3 | `0xA0` / `0xA1` | `flipX` |
| 4 | `0xA8 (height-1)` | `height` |
| 5 | `0xC0` / `0xC8` | `flipY` |
| 6 | `0xD3 displayOffset` | `displayOffset` |
| 7 | `0xDA comPins` | `comPins` |
| 8 | `0xD5` with `oscFrequency` in bits 7:4 and `clockDivide - 1` in bits 3:0 | `oscFrequency`, `clockDivide` |
| 9 | `0xD9` with `prechargePhase2` in bits 7:4 and `prechargePhase1` in bits 3:0 | `prechargePhase1/2` |
| 10 | `0xDB vcomh` | `vcomh` |
| 11 | `0x81 contrast` | `contrast` |
| 12 | `0xAD iref` | `iref` |
| 13 | `0x8D chargePumpVoltage, 0x2E` | `chargePumpVoltage`; scroll deactivate |
| 14 | `0xA4, 0x23 0x00` | fixed (RAM content, fade off) |
| 15 | `0xA6` / `0xA7`, `0xD6 0x00` | `invert`; zoom off |
| 16 | `0xA3 0x00 height` | full-height vertical scroll area |

`0xAF` is sent separately after GDDRAM has been written (full-buffer resync)
or by an explicit wake.

Resync/recover resets fade to OFF, zoom to disabled, and vertical scroll area
to full height. Values set through the runtime effect/scroll-area setters are
not retained as configuration for replay; applications must explicitly reapply
them after a successful resync if required.

### Validated before I2C

`width` 1..128; `height` a multiple of 8 within the 16..64 MUX range;
`i2cAddress` `0x3C`/`0x3D`; `contrast` `0x01`..`0xFF`; `prechargePhase1/2`
1..15; `clockDivide` 1..16; `oscFrequency` 0..15; `displayOffset` 0..63;
`startLine` < `height`; enum-typed `comPins`, `chargePumpVoltage`, `iref`,
`vcomh`; scroll pages `0 <= start <= end < height/8`; vertical scroll offset 0..63
and less than the cached scroll-area rows; `0xA3` arguments
`topFixedRows + scrollRows <= height`, `scrollRows > 0`, and
`startLine < scrollRows`. Hardware scroll helpers require a 128-column
configuration because they send columns `0x00`..`0x7F`. Raw passthrough
(`sendCommand*`) is not validated against the command table.

### Deviations from the module code

- The driver sends `0x20 0x00` (horizontal addressing) and bounds every flush
  with `0x21`/`0x22`; the module code stays in POR page mode and uses `0x00`,
  `0x10`, `0xB0`.
- The driver sends `0xAD 0x00` for external IREF, which equals the POR value
  the module code relies on implicitly.
- The driver additionally sends `0x2E`, `0x23 0x00`, `0xD6 0x00`, and
  `0xA3 0x00 height` so that scroll, fade, zoom, and scroll-area state are
  known after initialisation regardless of prior controller state.
- The driver never sends the page-mode pointer commands `0x00`..`0x0F`,
  `0x10`..`0x17`, `0xB0`..`0xB7`; they are exposed as constants only.
- The driver does not drive RES# or any power rail; the reset and power
  sequences in sections 4 and 6 are the application's responsibility.
