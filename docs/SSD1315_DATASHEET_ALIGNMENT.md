# SSD1315 Datasheet Alignment

Date: 2026-05-31

This note records the controller and panel-profile contracts used by the
driver. Sources are the local `docs/SSD1315_datasheet.pdf`, the extracted
notes under `docs/extracted-md/`, and the local Wisevision
`X096-2864KSWPG01-H30` module specification.

## Supported Controller Contract

- Supported controller profile: `ControllerProfile::SSD1315` only.
- Default panel assumption: 128x64 monochrome SSD1315 I2C OLED.
- SSD1306 compatibility is not claimed. The SSD1315 profile can send
  SSD1315-specific commands such as `SET_IREF` (`0xAD`), and a future
  SSD1306-compatible profile would need guarded init bytes, tests, and real
  hardware validation.
- Serial/I2C mode is write-oriented for common modules. The driver does not
  implement GDDRAM readback or readback-based identity checks.

## I2C Address And Control Bytes

- Valid SSD1315 I2C addresses are 7-bit `0x3C` and `0x3D`, selected by the
  SA0 / D-C# pin. Do not pass 8-bit address forms `0x78` or `0x7A`.
- `probe()` is ACK-only. It can show that something acknowledged the address;
  it cannot prove SSD1315 controller identity. Modules that do not connect
  `SDAOUT`/ACK make address-NACK policy board-specific.
- Command streams use control byte `0x00`.
- GDDRAM data streams use control byte `0x40`.
- Datasheet continuation control bytes `0x80` (command continuation) and
  `0xC0` (data continuation) are valid SSD1315 I2C framing values. The driver
  does not use them in normal transactions because it sends one control byte per
  bounded command/data transaction.
- In I2C mode, the SSD1315 maps D2/D1 as SDA out/in and D0 as SCL. Practical
  modules usually expose only SDA, SCL, RES#, and power pins; bus ownership and
  pullups belong to the application board.

## Electrical And Reset Limits

These are source constraints from the controller datasheet and Wisevision module
spec, not software defaults:

| Fact | Source contract |
| --- | --- |
| Controller logic VDD | 1.65 V to 3.5 V |
| Wisevision module logic VDD | 1.65 V to 3.3 V, typical 2.8 V |
| Controller charge-pump VBAT | 3.0 V to 4.5 V |
| Wisevision internal DC/DC VBAT | 3.5 V to 4.2 V |
| I2C clock | 2.5 us minimum cycle, equivalent to 400 kHz maximum |
| I2C timing | 0.6 us start-hold/repeated-start/stop setup, 100 ns data setup, 300 ns maximum rise/fall, 1.3 us idle before a new transaction |
| Reset defaults | Display off, 128x64 mode, normal mapping, column counter 0, contrast `0x7F` |
| Power-off caution | Do not pull VDD, VCC, or VBAT to ground as a power-off method |

External-VCC and internal-charge-pump circuits use different VDD/VCC/VBAT
ordering. Board firmware, not this bus-only library, owns those power rails and
reset timing.

## Init And Power Policy

The init sequence keeps display off during configuration, sets horizontal
addressing, panel geometry, COM/SEG mapping, analog/timing values, IREF,
charge-pump mode, RAM/invert mode, and scroll deactivation, then optionally
clears GDDRAM before final `DISPLAY_ON`.

`DISPLAY_ON` (`0xAF`) is intentionally sent only after the init sequence and
the selected `clearOnBegin` / `clearOnRecover` policy have succeeded. If a
begin/recover path fails before that point, the driver does not claim cache or
panel state is synchronized.

Internal charge-pump profiles send `0x8D` with `0x14`, `0x94`, or `0x95`
before display-on. External-VCC profiles use charge pump `OFF` (`0x10`).
`end()` sends display-off and, for internal charge-pump profiles, a best-effort
charge-pump disable sequence through the raw untracked transport path. This is
intentional so an `OFFLINE` latch does not by itself prevent a final physical
shutdown attempt.

## Panel Profiles

The repository exposes narrow panel/electrical presets through
`applyPanelProfile()`. These are not controller compatibility profiles.

| Profile | Charge pump | IREF | Orientation | Analog defaults |
| --- | --- | --- | --- | --- |
| `GENERIC_128X64_INTERNAL_CHARGE_PUMP` | Internal 7.5 V | Internal 19 uA | `A0` / `C0` | contrast `0x7F`, clock `0x80`, precharge `0x22`, VCOMH `0x20` |
| `WISEVISION_X096_2864KSWPG01_H30_INTERNAL_DC_DC` | Internal 7.5 V | External resistor | `A1` / `C8` | contrast `0xB0`, clock `0x90`, precharge `0x22`, VCOMH `0x30` |
| `WISEVISION_X096_2864KSWPG01_H30_EXTERNAL_VCC` | Off / external VCC | External resistor | `A1` / `C8` | contrast `0xB0`, clock `0x90`, precharge `0x22`, VCOMH `0x30` |

The Wisevision module spec examples use 128x64, alternative COM pins `0x12`,
segment remap `A1`, COM scan `C8`, contrast `0xB0`, clock `0x90`, precharge
`0x22`, VCOMH `0x30`, and external IREF circuitry. Hardware validation must
select the profile that matches the actual module power wiring.

## GDDRAM And Flush Policy

SSD1315 GDDRAM is 8 pages by 128 columns for 128x64 panels. Page rows use D0 at
the top row of the page and D7 at the bottom row. Full-frame flush writes
1024 data bytes under data control byte `0x40` with column/page address windows.

Dirty-page state is retained after failed flushes so later retries resend the
affected framebuffer bytes.

## Scroll Policy

- Valid page range is `0..7` and `startPage <= endPage`.
- Vertical scroll offset is `0..63` and must be less than the currently cached
  vertical scroll area row count.
- Driver scroll setup uses full-width columns `0x00..0x7F`, so hardware scroll
  is currently supported only for 128-column configurations. Non-128-wide
  panels may still draw/flush with configured-width address windows, but scroll
  setup returns `UNSUPPORTED` until that geometry is validated.
- Scroll speed raw values follow SSD1315 order: `0x00`=6 frames, `0x01`=32,
  `0x02`=64, `0x03`=128, `0x04`=3, `0x05`=4, `0x06`=5, `0x07`=2.
- The driver deactivates prior scroll before reconfiguration.
- While scroll is active, framebuffer flush requests return `STATE_ERROR` and
  keep dirty data. After `stopScroll()`, all framebuffer pages are marked dirty
  so the application can redraw/flush controller RAM.
- The driver exposes raw constants/passthrough for content-scroll one-column
  commands (`0x2C`/`0x2D`) but no high-level helper. Callers that use them must
  enforce the datasheet's two-frame delay requirement for consecutive use.

## Reset Ownership

The core driver does not own GPIOs and does not toggle `RES#`. `recover()` is a
software-only re-probe and reinit/resync path. Production board firmware must
satisfy the module power sequence, VDD stability, reset-low timing, and reset
release timing before calling `begin()` or `recover()`. After a hardware reset,
the application should reinitialize and redraw/flush display content.

## Future Compatibility Conditions

A future SSD1306-compatible mode would need a separate controller profile that
removes or guards SSD1315-only commands, defines analog defaults explicitly,
keeps probe wording ACK-only, and passes both host transaction tests and real
hardware validation on representative SSD1306 panels.
