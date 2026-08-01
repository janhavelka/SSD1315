# SSD1315 I2C Driver Reference (robust, implementation-ready)

This file is a **practical extraction/paraphrase** from:
- `SSD1315_datasheet.pdf` (Solomon Systech SSD1315 datasheet, Appendix IV Command Tables + timing/power notes)
- `Wisevision_X096-2864KSWPG01-H30_module_spec.pdf` (one referenced example module specification)

Goal: give a coding agent enough detail to implement a **stable SSD1315 I2C-only driver** without reading PDFs.

---

## 0) Reference-module controller identification (Wisevision spec)

The Wisevision X096 reference specification lists **Driver IC: SSD1315**. This
does not identify an uninspected TunnelMonitor module; its controller and
electrical profile remain external facts.

---

## 1) I2C essentials

### 1.1 7-bit slave address
SSD1315 uses the 7-bit base `0b011110x`.

- SA0 = 0 -> **0x3C**
- SA0 = 1 -> **0x3D**

In I2C mode, SSD1315 uses the **D/C# pin as SA0** (board wiring decides 0x3C vs 0x3D).

> Note: many docs mention "0x78/0x7A" which are the **8-bit write addresses** (0x3C<<1 / 0x3D<<1).

### 1.2 Control byte format (I2C framing)
After the slave address (write), send a **control byte** then payload bytes.

Control byte bits:
- **Co** (bit7): continuation; if 1, another control byte follows
- **D/C#** (bit6): 0 = following bytes are **commands**, 1 = following bytes are **GDDRAM data**
- bits[5:0] = 0

Common values:
- **0x00**: command stream
- **0x40**: data stream
- **0x80**: command stream with continuation control byte expected
- **0xC0**: data stream with continuation control byte expected

The driver intentionally uses `0x00` and `0x40` in normal transactions and
splits bounded command/data transactions at the transport layer, so continuation
control bytes are documented here as datasheet framing values rather than normal
driver output. `Config::maxWriteBytes` includes this control byte and is valid in
`[4..129]`; therefore a capacity of 129 can carry one control byte plus a full
128-column page payload.

### 1.3 I2C timing (module spec)
From the module's I2C timing table:
- `tCYCLE` (clock cycle time) min **2.5 us** -> implies **400 kHz** maximum clock
- `tHSTART` min **0.6 us**
- SDA hold time: **0 ns** for SDA_OUT, **300 ns** for SDA_IN
- `tSD` (data setup) min **100 ns**
- `tSSTART` min **0.6 us** (repeated start setup)
- `tSSTOP` min **0.6 us**
- Rise/fall time max **300 ns**
- `tIDLE` min **1.3 us**

Practical implication: default to **400 kHz** I2C; for long buses or weak pullups, drop to 100 kHz.

### 1.4 Electrical robustness warning (datasheet)
The SSD1315 datasheet warns that **trace resistance + pull-up resistance** can form a divider and prevent SDA from reaching a valid LOW during ACK.
Mitigations:
- use sane pullups (e.g., 2.2k-10k depending on bus capacitance)
- shorten lines where possible
- reduce I2C speed if ACK becomes flaky

---

## 2) Power / reset sequencing (SSD1315 datasheet)

### 2.1 External-VCC controller sequence

Power on:

1. When **VDD** is stable, wait **t0 >= 20 ms**
2. Toggle **RES#**:
   - LOW for **t1 >= 3 us**
   - then HIGH
3. After RES# LOW, wait **t2 >= 3 us**, then power ON **VCC**
4. After VCC is stable, send **Display ON (0xAF)**
   - SEG/COM outputs become active after **tAF ≈ 100 ms**

Power off:

1. Send **Display OFF (0xAE)**
2. Power OFF VCC
3. Power OFF VDD after **tOFF** (min 0 ms, typical 100 ms)

### 2.2 Internal-charge-pump controller sequence

Power on:

1. Power ON **VDD**.
2. After **tON** (minimum 0 ms), power ON **VBAT**.
3. After VDD is stable, wait **t0 >= 20 ms**, pulse **RES#** LOW for
   **t1 >= 3 us**, then HIGH.
4. At least **t2 >= 3 us** after driving RES# LOW, enable the selected internal
   charge-pump output with **0x8D** and then send **Display ON (0xAF)**.
5. SEG/COM outputs become active after **tAF ~= 100 ms**.

Power off:

1. Send **Display OFF (0xAE)**.
2. Send **0x8D, 0x10** to disable the charge pump.
3. Power OFF VBAT after **tOFF** (typical 100 ms), then power OFF VDD after
   **tOFF2** (minimum 0 ms, typical 5 ms).

The page-24 shutdown figure labels the disable argument as `0x00`, but the
prose and command table specify `0x10`; this repository follows the command
table. See `docs/extracted-md/08_variant_differences_and_open_questions.md`.

### 2.3 Ownership and safety warnings

- These are controller sequences, not actions performed by the driver. The
  application owns rails and RES#; the core sends only configured commands.
- Keep VCC/VBAT floating or disabled when OFF, as applicable.
- Never pull VDD, VCC, or VBAT to ground as a power-off method.

### 2.4 Module "special tip" (Wisevision)
The module spec recommends adding an **electronic switch** on Vin; otherwise leakage current may occur.

---

## 3) Addressing modes + "page" concept

### 3.1 GDDRAM page
SSD1315 GDDRAM is organized into **pages**:
- 1 page = **8 vertical pixels**
- 128x64 panel -> 8 pages (0..7)

### 3.2 Memory addressing modes (Set with 0x20)
Command: **0x20, mode**
- mode selects one of:
  - **Horizontal addressing**
  - **Vertical addressing**
  - **Page addressing** (datasheet describes Page mode with A[1:0] = 10b)

### 3.3 Range setting for partial updates (recommended)
Use these to bound writes to a rectangle/page range:
- **0x21, colStart, colEnd** (Set Column Address)
- **0x22, pageStart, pageEnd** (Set Page Address)

Recommended driver approach for partial update:
- switch to **Horizontal addressing**
- set column + page ranges
- stream only the dirty bytes using data control byte **0x40**

---

## 4) SSD1315 command-table reference (I2C write scope)

This section lists the supported write-command constants drawn from these
SSD1315 command tables:
- Table 1-1 Fundamental Command Table
- Internal Charge Pump Command Table
- Scrolling Command Table
- Advance Graphic Command Table

> Note: "Read commands" exist for parallel interfaces only; **no status/data reads in serial (I2C) mode**.

### 4.1 Fundamental commands (Table 1-1)

#### Addressing / pointers
- **0x00-0x0F**: Set Lower Column Start Address (Page addressing mode)
- **0x10-0x17**: Set Higher Column Start Address (Page addressing mode)
- **0x20, mode**: Set Memory Addressing Mode
- **0x21, start, end**: Set Column Address range
- **0x22, start, end**: Set Page Address range
- **0xB0-0xB7**: Set Page Start Address (Page addressing mode)
- **0x40-0x7F**: Set Display Start Line (lower 6 bits)

#### Display enable / global modes
- **0xAE**: Display OFF
- **0xAF**: Display ON
- **0xA4**: Resume to RAM content
- **0xA5**: Entire Display ON (ignore RAM)
- **0xA6**: Normal display
- **0xA7**: Inverse display

#### Orientation
- **0xA0 / 0xA1**: Segment remap (flip X)
- **0xC0 / 0xC8**: COM scan direction (flip Y)

#### Geometry / layout
- **0xA8, muxMinus1**: Multiplex ratio (height-1)
- **0xD3, offset**: Display offset
- **0xDA, cfg**: COM pins hardware configuration
  - bits: A[4] selects sequential(0)/alternative(1)
  - A[5] COM left/right remap disable(0)/enable(1)
  - lower bits include a fixed pattern (...0010b) per command table
  - Practical values you will use:
    - **0x02**: sequential, no remap
    - **0x12**: alternative, no remap (common for 128x64)
    - **0x22**: sequential, remap
    - **0x32**: alternative, remap

#### Analog / timing / quality knobs
- **0x81, contrast**: Set Contrast Control (0x01-0xFF). The driver rejects
  `0x00` instead of sending an undefined validation value.
- **0xD5, clk**: Display clock divide ratio / oscillator frequency (packed bits)
- **0xD9, precharge**: Pre-charge period (packed bits; RESET gives 4 DCLKs)
- **0xDB, vcomh**: Set VCOMH deselect level
  - common parameter codes (per table):
    - **0x00** ~ 0.65xVCC
    - **0x10** ~ 0.71xVCC
    - **0x20** ~ 0.77xVCC (RESET)
    - **0x30** ~ 0.83xVCC

#### Misc
- **0xE3**: NOP

#### SSD1315-specific (IREF)
- **0xAD, irefCfg**: External or internal IREF selection
  - A[4]=0: external IREF (reset)
  - A[4]=1: enable internal IREF during display ON
  - A[5]=0: internal IREF ~19 uA (reset), max segment current ~150 uA
  - A[5]=1: internal IREF ~30 uA, max segment current ~240 uA

### 4.2 Internal charge pump commands (Charge Pump Command Table)
- **0x8D, pumpCfg**: Charge pump enable/disable and voltage setting
  - A[2]=0: disable charge pump (RESET)
  - A[2]=1: enable charge pump during display ON
  - Common pump voltage parameter codes:
    - **0x10**: disabled (RESET/off sequence)
    - **0x14**: 7.5 V
    - **0x94**: 8.5 V
    - **0x95**: 9.0 V
  - Datasheet note: enable using sequence:
    - `0x8D; 0x14/0x94/0x95; ...; 0xAF`
  - Internal-pump power-down sequence used by the driver:
    - `0xAE; 0x8D; 0x10`

### 4.3 Scrolling commands (Scrolling Command Table)

#### Horizontal scroll setup (continuous)
- **0x26**: Right Horizontal Scroll setup (7-byte setup sequence total)
- **0x27**: Left Horizontal Scroll setup
- Driver setup form: command, dummy `0x00`, start page, speed, end page,
  start column `0x00`, end column `0x7F`, then `0x2F` activate.
- The public helper supports this hardware-scroll form only on 128-column
  configurations. Non-128-wide configs return `UNSUPPORTED`.

#### Vertical + horizontal scroll setup (continuous)
- **0x29**: Vertical and Right Horizontal Scroll setup
- **0x2A**: Vertical and Left Horizontal Scroll setup
- Driver setup form: command, horizontal offset `0x01`, start page, speed,
  end page, vertical offset, start column `0x00`, end column `0x7F`, then
  `0x2F` activate.
- The public helper supports this hardware-scroll form only on 128-column
  configurations. Non-128-wide configs return `UNSUPPORTED`.
- The driver validates `0 <= startPage <= endPage <= 7`, vertical offset
  `0..63`, and vertical offset less than the active vertical scroll area rows.

#### Scroll frame interval argument

| Raw value | SSD1315 interval |
| --- | --- |
| `0x00` | 6 frames |
| `0x01` | 32 frames |
| `0x02` | 64 frames |
| `0x03` | 128 frames |
| `0x04` | 3 frames |
| `0x05` | 4 frames |
| `0x06` | 5 frames |
| `0x07` | 2 frames |

#### Content scroll (one-column steps)
- **0x2C**: Right Horizontal Scroll by one column (content scroll setup)
- **0x2D**: Left Horizontal Scroll by one column (content scroll setup)
- Datasheet note: if sending 0x2C/0x2D consecutively, insert a delay of **2 frame periods**.

#### Activate / deactivate
- **0x2E**: Deactivate scroll
  - Datasheet note: after deactivating, RAM data needs to be rewritten
- **0x2F**: Activate scroll
  - Valid sequences: {26h,2Fh}, {27h,2Fh}, {29h,2Fh}, {2Ah,2Fh}
  - If multiple setup commands are sent, the last valid setup governs behavior.

#### Vertical scroll area
- **0xA3, topFixedRows, scrollRows**: Set Vertical Scroll Area
- Datasheet constraints include `topFixedRows + scrollRows <= MUX` and
  `displayStartLine < scrollRows`.
- Initialization/resync physically commands `(0, configured height)` and also
  restores fade-off, zoom-off, and scroll deactivation before clearing modeled
  control uncertainty. The helper caches only a successful explicit update.

### 4.4 Advance graphic commands (Advance Graphic Command Table)
- **0x23, cfg**: Set Fade Out and Blinking
  - A[5:4]=00: disable (RESET)
  - A[5:4]=10: Fade-out mode (contrast decreases to all OFF)
  - A[5:4]=11: Blinking mode (fade out then fade in loop)
  - A[3:0]: time interval per fade step is 8, 16, ..., 128 frames;
    `frames = 8 * (code + 1)` for code 0..15
- **0xD6, enable**: Set Zoom In
  - A[0]=0: disable (RESET)
  - A[0]=1: enable zoom-in
  - Datasheet note: panel must be in **alternative COM pin configuration** (`0xDA` with A[4]=1)

---

## 5) Partial update strategy (recommended, robust)

Minimum viable:
- dirty page bitset (pages = height/8)

Recommended:
- dirty page bitset + `dirtyMinCol[page]` / `dirtyMaxCol[page]`

Flush algorithm (deterministic):
1. Switch to Horizontal addressing (`0x20, horizontal`)
2. For each dirty page:
   - set column range (`0x21`)
   - set page range (`0x22`)
   - stream only dirty bytes (control byte `0x40`)
3. Chunk data writes by the smaller of the owner byte budget and
   `maxWriteBytes - 1`; resume where the confirmed prefix ended
4. On NACK/timeout: stop flush; keep dirty flags; store `lastError`

The cooperative owner admits flush/resync work with a request identity and
optional absolute deadline, then calls `pollOperation()`. One normal owner poll
uses transaction budget one. One callback permits at most one physical bus
transaction; OK confirms its completion. Neither callback nor core retries or
performs bus recovery.

Panel-control note:
- Multi-command control sequences such as scroll setup can leave physical
  controller state uncertain when an I2C failure occurs mid-sequence. The
  driver exposes `controlStateDirty()` / `controlStateError()` for that case.
  Successful arbitrary raw passthrough also invalidates modeled control/power
  state. Clear the driver's modeled uncertainty only through a complete
  successful initialize/resync sequence; this is not hardware readback.

---

## 6) Suggested driver error taxonomy (stable API)
- `INVALID_CONFIG` (bad width/height, null transport, bad pageBufferPages)
- `I2C_NACK_ADDR` / `I2C_NACK_DATA`
- `I2C_TIMEOUT`
- `PANEL_NOT_READY` (cooperative flush admission while modeled power is neither
  confirmed ON nor confirmed OFF; legacy timing waits may report `IN_PROGRESS`)
- `STATE_ERROR` (bad call order)
- `UNSUPPORTED` (attempted serial read, etc.)

---

## 7) "Nice to have later" features that the command set enables
- hardware scrolling helpers (wrap setup + activate/deactivate)
- fade/blink mode helpers
- zoom-in mode helper (with safety checks)
- contrast presets (dim/normal/bright)
- invert and flip toggles
- a simple built-in self-test (checkerboard/fill/bars) for manufacturing
