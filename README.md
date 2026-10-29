# SSD1315 I2C OLED Display Driver

[![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32-orange)](https://platformio.org)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

Hardened I2C driver library for SSD1315 OLED displays on ESP32. The core is framework-neutral and works with Arduino/PlatformIO or ESP-IDF through application-owned I2C callbacks.

This repository targets SSD1315. SSD1306-like panels may work because many commands overlap, but compatibility is not guaranteed unless a separate controller profile and hardware validation are added. `probe()` can prove only address ACK, not controller identity, and ACK-less module wiring makes address-NACK policy board-specific.

Quick Start covers bring-up; Timing and Operation Model describes shared-bus
integration. The [documentation map](docs/DOCUMENTATION.md) links the platform,
release, controller-reference and hardware-validation guides.

## Features

- **Owner-budgeted operations** - one cooperative state machine for initialize,
  flush, sleep, wake, resync, and shutdown
- **Partial updates** - dirty tracking with column-level granularity
- **Page buffer mode** - u8g2-style iteration; a one-page 128x64 buffer uses
  128 bytes instead of a 1,024-byte full frame
- **Hardware scroll** - horizontal and vertical scroll support on 128-column
  SSD1315 panels
- **Command access** - supported SSD1315 write-command constants, helpers, and
  raw write passthrough for the driver-supported command surface
- **Deterministic memory option** - use a caller-supplied framebuffer for
  production ownership; internal allocation remains a bring-up convenience
- **Robust error handling** - stable `Status`/`Err` contracts on primary
  fallible operations and retained error state for legacy iteration
- **Transport abstraction** - no Wire dependency; inject your own I2C callback
- **ESP-IDF component support** - root CMake metadata and a native `i2c_master` example

## Quick Start

This Arduino bring-up example uses bounded blocking `begin()` followed by a
cooperative legacy flush. For a shared bus, use the explicit start/poll/result
owner flow described below.

```cpp
#include <Arduino.h>
#include <Wire.h>
#include "SSD1315.h"

// One terminal callback result; OK confirms one complete I2C transaction.
static SSD1315::TransportResult mapWireError(uint8_t result) {
  switch (result) {
    case 0: return SSD1315::TransportResult::Ok();
    case 2: return SSD1315::TransportResult::NackAddress(result);
    case 3: return SSD1315::TransportResult::NackData(result);
    case 5: return SSD1315::TransportResult::Timeout(result);
    default: return SSD1315::TransportResult::BusError(result);
  }
}

SSD1315::TransportResult myI2cWrite(uint8_t addr, const uint8_t* data,
                                   size_t len, uint32_t timeoutMs, void* user) {
  TwoWire* wire = static_cast<TwoWire*>(user);
  if (wire == nullptr) {
    return SSD1315::TransportResult::BusError(-1);
  }
  if (data == nullptr || len == 0) {
    return SSD1315::TransportResult::BusError(-2);
  }
#if defined(ARDUINO_ARCH_ESP32)
  wire->setTimeOut(static_cast<uint16_t>(timeoutMs > 65535U ? 65535U : timeoutMs));
#endif
  wire->beginTransmission(addr);
  size_t written = wire->write(data, len);
  if (written != len) {
    (void)wire->endTransmission(true); // Close the transaction and release Wire's lock
    return SSD1315::TransportResult::BusError(static_cast<int32_t>(written));
  }
  uint8_t result = wire->endTransmission(true);
  return mapWireError(result);
}

uint32_t myNowMs(void*) {
  return millis();
}

void myCooperativeYield(void*) {
  yield();
}

SSD1315::SSD1315 display;
bool flushPending = false;

void setup() {
  Serial.begin(115200);
  if (!Wire.begin(8, 9, 400000)) {  // SDA, SCL, frequency
    Serial.println("I2C initialization failed");
    return;
  }
  Wire.setTimeOut(25);

  SSD1315::Config cfg;
  cfg.width = 128;
  cfg.height = 64;
  cfg.i2cAddress = 0x3C;
  cfg.i2cWrite = myI2cWrite;
  cfg.i2cUser = &Wire;
  cfg.maxWriteBytes = 128;  // Actual Wire transaction capacity, control included
  cfg.nowMs = myNowMs;
  cfg.cooperativeYield = myCooperativeYield;
  cfg.i2cTimeoutMs = 25;
  cfg.pageBufferPages = 8;  // Full buffer mode

  SSD1315::Status st = display.begin(cfg);
  if (!st.ok()) {
    Serial.printf("Error: %s\n", st.msg);
    return;
  }

  display.clear();
  display.drawText(0, 0, "Hello World!");
  st = display.requestFlush();
  if (!st.ok()) {
    Serial.printf("Flush admission failed: %s\n", st.msg);
    return;
  }
  flushPending = true;
}

void loop() {
  if (!flushPending) return;
  display.tick(millis());  // At most one flush instruction
  if (!display.isFlushing()) {
    flushPending = false;
    const SSD1315::Status st = display.getFlushStatus().lastError;
    if (!st.ok()) Serial.printf("Flush failed: %s\n", st.msg);
    // A retry, if wanted, must be explicitly admitted by the application.
  }
}
```

## Configuration Reference

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `controllerProfile` | enum | SSD1315 | Controller profile. Only SSD1315 is currently supported; includes SSD1315 `SET_IREF` |
| `width` | uint8_t | 128 | Display width in pixels (`1..128`); GDDRAM columns start at zero, with no column-offset setting |
| `height` | uint8_t | 64 | Display height in pixels (`16..64`, every multiple of 8) |
| `i2cAddress` | uint8_t | 0x3C | SSD1315 7-bit I2C address (`0x3C` or `0x3D`; do not pass 8-bit forms `0x78`/`0x7A`) |
| `i2cWrite` | function | nullptr | **Required.** I2C write callback |
| `i2cUser` | void* | nullptr | User context for callback |
| `maxWriteBytes` | uint16_t | 65 | Total callback write capacity, including the control byte (`4..129`) |
| `nowMs` | function | `nullptr` | Monotonic millisecond clock for blocking waits and health timestamps; required by blocking lifecycle waits for the display-on guard when `displayOnDelayMs > 0`. Cooperative progress uses the timestamp supplied to `pollOperation()` |
| `cooperativeYield` | function | `nullptr` | Optional yield hook for bounded wait helpers |
| `timeUser` | void* | `nullptr` | User context for `nowMs` / `cooperativeYield` |
| `pageBufferPages` | uint8_t | 8 | Pages in RAM buffer (1 to height/8) |
| `byteBudgetPerTick` | uint16_t | 128 | Max data bytes for one `tick()` data instruction; must be greater than zero |
| `i2cTimeoutMs` | uint32_t | 25 | Positive total transport-callback timeout in ms, including adapter-owned lock waiting; an operation or `waitFlush()` deadline may reduce it |
| `flushTimeoutMs` | uint32_t | 1000 | Total flush timeout in ms, including time between polls; `0` disables this bound |
| `displayOnDelayMs` | uint32_t | 100 | Post-`DISPLAY_ON` guard; values below 100 ms waive conservative tAF |
| `clearOnBegin` | bool | true | Blocking compatibility choice: full resync and wake, or initialize off |
| `clearOnRecover` | bool | true | Deprecated compatibility storage; `recover()` always performs full resync and requires full-buffer mode |
| `inactivitySleepMs` | uint32_t | 0 | Deprecated compatibility storage; core never admits sleep policy |
| `pageCycleMs` | uint32_t | 0 | Deprecated compatibility storage; core never admits page policy |
| `flipX` | bool | false | Flip horizontally (segment remap) |
| `flipY` | bool | false | Flip vertically (COM scan) |
| `invert` | bool | false | Invert display colors |
| `contrast` | uint8_t | 0x7F | Initial contrast (`1..255`; `0` is rejected) |
| `comPins` | enum | ALTERNATIVE_NO_REMAP | COM pin configuration |
| `chargePumpVoltage` | enum | V7_5 | Charge pump mode/voltage (`OFF`, `V7_5`, `V8_5`, `V9_0`) |
| `iref` | enum | INTERNAL_19UA | SSD1315 reference-current source; select for the module's IREF wiring |
| `vcomh` | enum | V_077_VCC | VCOMH deselect level |
| `clockDivide` | uint8_t | 1 | Display clock divide ratio (`1..16`) |
| `oscFrequency` | uint8_t | 8 | Oscillator frequency trim (`0..15`) |
| `prechargePhase1` | uint8_t | 2 | Phase 1 code `1..15`; code N encodes 2*N DCLKs |
| `prechargePhase2` | uint8_t | 2 | Phase 2 code `1..15`; code N encodes 2*N DCLKs |
| `displayOffset` | uint8_t | 0 | Vertical display offset (`0..63`, command `0xD3`); not a horizontal column offset |
| `startLine` | uint8_t | 0 | Display start line (`0..height-1`) |
| `offlineThreshold` | uint8_t | 3 | Diagnostic threshold for `OFFLINE`; zero is clamped to 1 on attach. Never gates I2C admission |
| `externalBuffer` | uint8_t* | nullptr | External framebuffer (optional) |
| `externalBufferSizeBytes` | size_t | 0 | Required external framebuffer length |

Use `SSD1315::applyPanelProfile(cfg, profile)` before `attach()`/`begin()` when targeting
a documented 128x64 panel preset. Current presets are:
`GENERIC_128X64_INTERNAL_CHARGE_PUMP`,
`WISEVISION_X096_2864KSWPG01_H30_INTERNAL_DC_DC`, and
`WISEVISION_X096_2864KSWPG01_H30_EXTERNAL_VCC`. These are electrical/panel
presets, not SSD1306 compatibility profiles; transport, address, reset GPIO,
bus speed, and buffering remain application-owned.
The generic preset selects internal 19 uA IREF; both Wisevision presets select
external IREF. Match the populated module: internal reference requires IREF
left unconnected, while external reference requires the specified resistor.
The driver cannot detect that wiring over I2C.

## Memory Modes

By default, the driver allocates its framebuffer during `attach()`/`begin()`. That
convenience mode is acceptable for bring-up and simple applications, but
production firmware that requires deterministic memory ownership should provide
`externalBuffer` in `Config`. The external buffer must remain valid until
`detach()`, `end()`, destruction, or a successful rebind and must be at least
`width * pageBufferPages` bytes; set
`pageBufferPages` to `height / 8` for full-buffer mode.

Example full-buffer ownership for a 128x64 panel:

```cpp
namespace {
constexpr uint8_t OLED_WIDTH = 128;
constexpr uint8_t OLED_HEIGHT = 64;
constexpr uint8_t OLED_PAGES = OLED_HEIGHT / 8;
static uint8_t oledFramebuffer[static_cast<size_t>(OLED_WIDTH) * OLED_PAGES] = {};
}

SSD1315::Config cfg;
cfg.width = OLED_WIDTH;
cfg.height = OLED_HEIGHT;
cfg.pageBufferPages = OLED_PAGES;
cfg.externalBuffer = oledFramebuffer;
cfg.externalBufferSizeBytes = sizeof(oledFramebuffer);
```

The driver never takes ownership of `externalBuffer` and will not free it.
Every successful `attach()`/`begin()` clears the complete selected buffer,
including caller-owned storage; rejected candidates leave both bindings intact.
Keep it in static storage or another region whose lifetime exceeds the display
binding. Alignment beyond normal `uint8_t` alignment is not required by the
driver. Internal RAM is the most predictable placement on ESP32; PSRAM can be
used when the application accepts its latency and availability policy.

### Full Buffer Mode

Set `pageBufferPages` equal to `height/8` (e.g., 8 for 128x64).

- RAM usage: width x height / 8 bytes (1024 bytes for 128x64)
- Draw anywhere in the buffer
- Dirty tracking enables efficient partial updates
- Best for dynamic content with random access

```cpp
cfg.pageBufferPages = 8;  // Full buffer for 128x64
```

### Page Buffer Mode

Set `pageBufferPages` to 1 or 2 for minimal RAM usage.

- RAM usage: width x pageBufferPages bytes (128-256 bytes)
- Use firstPage()/nextPage() to select each RAM window
- Initialization leaves the panel off; `startResync()` is unsupported
- The owner iterates and flushes every page while off, then explicitly wakes
- Drawing coordinates are always ABSOLUTE panel coordinates, in every window.
  Render the same scene each time; the driver clips it to the active RAM window
  and translates the row itself. Do NOT add `pageBufferYOffset()` to a drawing
  Y coordinate - that draws the scene once per window, stacked down the panel.
  `pageBufferYOffset()` reports the first absolute row of the active window; use
  it to skip work that cannot be visible, not to shift coordinates
- `firstPage()` and each advance mark the complete fresh RAM window dirty, so
  even a partial draw transfers every byte needed for a known first frame
- `startWake()` never flushes implicitly and rejects dirty/incomplete GDDRAM;
  every page window must complete successfully before first presentation
- The clean-baseline invariant is rechecked immediately before `DISPLAY_ON`;
  post-admission or in-flush mutation terminates without presenting stale data
- After a cooperative page flush succeeds, `nextPage()` advances the RAM window
  without I2C. If drawing changed that window during transfer, `nextPage()`
  keeps it selected for a retry. Calling it earlier queues the legacy flush path
- Changing `setFlipX()` or starting hardware scroll returns `BUSY` during
  iteration, including a completed window awaiting `nextPage()`. Finish the
  iteration before changing the remap, then restart and rewrite every window
- Panel invalidation, the first attempted initialization command, stopping an
  active scroll, or discarding pending dirty data ends the iteration. Earlier
  windows cannot certify a complete frame after those transitions
- Renders entire screen each frame, but only page buffer in RAM
- Best for static or slowly-changing content
- clear()/fill() affect only the current buffer window; use firstPage()/nextPage() to cover the full display

```cpp
cfg.pageBufferPages = 1;  // Minimal RAM

SSD1315::Status st = display.begin(cfg); // Initializes page-buffer mode, panel off
if (st.ok()) st = display.firstPage();
uint32_t requestId = 1;
while (st.ok() && display.isPageIterating()) {
  display.clear();
  display.drawText(0, 0, "Title");   // absolute panel coordinates, every window
  display.drawText(0, 40, "Status"); // clipped automatically when out of window
  SSD1315::OperationOptions flush;
  flush.requestId = requestId++;
  st = display.startFlush(flush);    // zero-I2C admission
  if (!st.ok()) break;
  while (st.ok() || st.inProgress()) {
    st = display.pollOperation(millis(), 1, 128);
    if (!st.inProgress()) break;
    yield();
  }
  SSD1315::OperationResult flushed;
  const SSD1315::Status take = display.takeOperationResult(flushed);
  st = take.ok() ? flushed.status : take;
  if (!st.ok()) break;
  if (!display.nextPage()) st = display.lastError(); // memory-only advance
}

if (st.ok()) {
  SSD1315::OperationOptions wake;
  wake.requestId = requestId++;
  st = display.startWake(wake);      // requires all windows successfully flushed
  const bool wakeAdmitted = st.ok();
  while (wakeAdmitted && (st.ok() || st.inProgress())) {
    st = display.pollOperation(millis(), 1);
    if (!st.inProgress()) break;
    yield();
  }
  if (wakeAdmitted) {
    SSD1315::OperationResult wakeResult;
    const SSD1315::Status take = display.takeOperationResult(wakeResult);
    st = take.ok() ? wakeResult.status : take;
  }
}
// Report st: retain an earlier render/flush failure instead of masking it with wake.
```

## Timing And Operation Model

The v4 API has one fixed, allocation-free operation state machine. An external
owner admits work with `startInitialize()`, `startFlush()`, `startSleep()`,
`startWake()`, `startResync()`, `startShutdown()`, or the cooperative scroll
setup APIs, then calls
`pollOperation(nowMs, maxTransactions, byteBudget)`. Admission is zero-I2C.
`OperationOptions` carries a nonzero request ID that differs from the preceding
admitted ID, plus an optional absolute, wrap-safe deadline.
`OperationProgress` exposes phase, transport-outcome effect
certainty, command-confirmed modeled power, bytes, chunks, and callback count.
The deadline bounds transport attempts: after a confirmed `DISPLAY_ON`, the
final zero-I2C guard completes normally even if the deadline passes. Owners keep
polling that guard to completion or cancel it explicitly.
SSD1315 I2C has no controller-status or GDDRAM readback, so these values do not
prove controller identity, electrical state, or visible panel state. One terminal
`OperationResult` is retrieved exactly once with `takeOperationResult()`.
`cancelOperation()` performs no I2C and preserves unconfirmed dirty data.
Until that terminal result is consumed, the driver retains exclusive operation
provenance: direct commands and legacy flush paths return `BUSY` without I2C.

Typical external-owner flow:

```cpp
SSD1315::Status st = display.attach(cfg);  // zero I2C
bool admitted = false;
if (st.ok()) {
  SSD1315::OperationOptions op;
  op.requestId = requestId;                // nonzero, owner-issued identity
  op.useDeadline = true;
  op.deadlineMs = absoluteDeadlineMs;
  st = display.startResync(op);             // zero I2C admission
  admitted = st.ok();
}

// In the sole I2C owner's scheduled poll:
if (admitted && (st.ok() || st.inProgress())) {
  st = display.pollOperation(nowMs, 1, 128); // at most one callback
}

if (admitted && !st.inProgress()) {
  SSD1315::OperationResult result;
  const SSD1315::Status take = display.takeOperationResult(result);
  if (take.ok()) {
    admitted = false;                      // This result has been consumed once
    st = result.status;                   // Retain success/failure/cancel/timeout
    // Match result.requestId to the request and handle st before new admission.
  } else {
    st = take;                            // Report the owner-state error
  }
}
```

`pollOperation()` accepts at most eight callback slots per call. A normal shared-
bus owner should pass `maxTransactions = 1`, guaranteeing at most one transport
callback and at most one physical bus transaction. An adapter may fail before
bus access (for example while taking its application-owned lock); OK alone
confirms a complete bus transaction. Deadline-bearing operations are limited
to one attempt per poll even if a larger maximum is supplied, so later attempts
cannot reuse stale caller time. Data is additionally limited by the explicit
byte budget and `maxWriteBytes - 1`; the control byte is included in the
transport capacity but excluded from the payload budget.

For a 128x64 full buffer:

| Operation | Capacity / budget | Maximum transport callbacks |
| --- | --- | ---: |
| Initialize off | Any valid capacity | 17 |
| Full resync | `maxWriteBytes=129`, payload budget 128 | 42: 17 init + 8 x (column, page, data) + display-on |
| Full resync | Default `maxWriteBytes=65` | 50 |
| Full resync | One transaction per poll; `P=min(byteBudget,maxWriteBytes-1)`, `N=height/8` | `18 + N*(2 + ceil(width/P))` |
| Sleep | Any valid capacity | 1 |
| Wake | Clean, completely populated GDDRAM | 1 plus zero-I2C configured guard |
| Shutdown | Pump already OFF / internal pump | 1 / 2 |
| Horizontal scroll setup | `maxWriteBytes>=8` | 3: deactivate + setup + activate |
| Vertical scroll setup | `maxWriteBytes>=9` | 3: deactivate + setup + activate |

The display-on timing interval is a zero-I2C phase after the final command. Its
cooperative timer is latched by the first owner poll after the synchronous
transport callback returns, so time spent inside that callback cannot consume
the post-command guard.
When a poll allows multiple transactions, its data budget is shared across
those transactions, so exact data chunking can depend on poll boundaries. The
safe all-configurations bound is `18 + N*(2 + width)` callbacks.
Per-attempt timeout is clipped to an operation deadline. No callback is started
at or after that deadline; the final zero-I2C display-on guard is exempt. The
core performs no retry, bus recovery, lock acquisition, backoff, or bus
initialization.
At the supported 128x64 worst case `P=1`, full resync is 1,058 callbacks.
Blocking `begin(clearOnBegin=true)`/`recover()` are bounded by that callback
count times `i2cTimeoutMs`, plus `displayOnDelayMs` and bounded local overhead,
provided the application callback honors its timeout.

`begin()` and `recover()` remain bounded blocking compatibility facades over
this same state machine. Shared-bus owners should use passive `attach()` and
the cooperative start/poll/result API instead.

Blocking lifecycle waits for the display-on guard require `Config::nowMs` when
`displayOnDelayMs` is nonzero; otherwise they return `INVALID_CONFIG`. This
includes default full-buffer `begin()` and `recover()`. Inject a monotonic clock
and a bounded platform yield hook for these waits. Cooperative owners supply
time directly to `pollOperation()` and do not need the clock hook for progress.

### Poll And Byte Budgets

The `byteBudgetPerTick` setting controls the maximum data payload for the one
legacy flush instruction that `tick()` may issue. Command instructions do not
consume the byte budget. A data transfer is limited by the smaller of the
budget, remaining dirty bytes, and `maxWriteBytes - 1`.

| Setting | Behavior | Use Case |
|---------|----------|----------|
| Capacity 65, budget 64+ | Up to 64 payload bytes | Default, conservative adapter |
| Capacity 129, budget 128 | Up to 128 payload bytes | One full 128-column page data transfer |

`byteBudgetPerTick` must be greater than zero. `pollFlush()` also requires a
nonzero `byteBudget`; pass `maxInstructions = 0` when the owner wants to query
progress without issuing I2C. Use `waitFlush()` only when a bounded synchronous
wait is acceptable for the calling task.

Cooperative progress bounds the number and size of attempts; each synchronous
transport callback may still block for its supplied timeout. `tick()` can also
advance an admitted cooperative operation by at most one callback, but never
consumes its result.

For latency-sensitive systems, keep `byteBudgetPerTick` small and prefer
`startFlush()` with owner-scheduled `pollOperation()` calls. Compatibility
callers can use `requestFlush()` with `tick()` or `pollFlush()` instead of
blocking in `waitFlush()`.

`flushTimeoutMs` runs from the first non-query flush poll and includes time
between subsequent polls, on both cooperative and compatibility paths. It is
independent of the per-callback `i2cTimeoutMs`, the optional operation deadline,
and a `waitFlush()` deadline. Choose it for the owner's scheduling cadence; setting
it to zero disables only this flush-wide bound. The core does not retry when
any of these bounds expires.

### Power-On Timing

The SSD1315 specifies an approximately 100 ms tAF interval after display ON.
The default `displayOnDelayMs=100` applies that guard non-blocking; during the
configured interval, legacy flush work is deferred and cooperative flush
admission requires command-confirmed `OFF` or `ON`, excluding `STARTING`.
An active cooperative wake also retains operation ownership until its guard
completes and the caller consumes the result. These are current software
gates; the datasheet's SEG/COM output latency does not itself prohibit host
GDDRAM writes during that interval. A value below 100 ms,
including zero, is an explicit application-owned diagnostic or qualified timing
waiver. It only changes when modeled power becomes `ON` and cannot establish
physical panel readiness. Cooperative paths latch the first owner timestamp
after the successful `DISPLAY_ON` callback; delayed paths are safe when that
timestamp is `0`.

### Sleep And Page Policy

Auto-sleep and page-cycle configuration/accessors are deprecated compatibility
storage. Drawing, `touch()`, and `tick()` never admit hidden power or page work.
Application policy explicitly schedules `startSleep()`/`startWake()` and owns
UI page selection and cadence.

## Partial Updates

The driver tracks dirty regions at page and column granularity:

```cpp
// Draw in a specific area
display.fillRect(10, 20, 30, 15);

// Flush only the dirty region
display.requestFlush();  // Automatically flushes only dirty pages

// Or explicitly flush a rectangle
display.requestFlushRect(10, 20, 30, 15);
```

Dirty tracking:
- Per-page dirty flags (bitmask)
- Per-page min/max dirty column range
- Horizontal addressing mode for efficient rectangle updates
- Failed flushes preserve dirty state for unsent or partially sent pages. A
  later `requestFlush()` retries the affected bytes from the framebuffer.
  An ordinary flush failure, including an ambiguous transport timeout, does
  not itself invalidate cached panel controls or power. Cooperative callers
  consume the failed result before explicitly admitting a dirty retry.
- If a complete baseline is missing (for example after horizontal remap), a
  failed or timed-out full-buffer flush, or cooperative cancellation through
  `cancelOperation()`, marks the entire frame dirty again. An explicitly
  admitted full rewrite rebuilds that baseline. After a successful rewrite,
  mutations made during transfer remain dirty and must be flushed before wake.
  The core never retries itself.
- `invalidatePanelState()` also invalidates cached controls and power. When
  cancelling a legacy flush it preserves the existing dirty mask; full-buffer
  callers must resync/recover before normal flushing resumes.

Changing `setFlipX()` sends the remap command and marks the framebuffer dirty;
it does not start a transfer. Explicitly flush the full buffer, or restart and
flush every page window, to display the mirrored image and permit wake. The
diagnostic CLI `flipx` commands perform the bounded flush and report its result.

## Hardware Scrolling

Hardware scroll moves pixels on the display without CPU involvement:

```cpp
SSD1315::OperationOptions scroll;
scroll.requestId = nextRequestId();
scroll.useDeadline = true;
scroll.deadlineMs = scrollDeadlineMs;

// Owner-safe zero-I2C admission; poll/result uses the common operation model.
SSD1315::Status st = display.startHorizontalScrollOperation(
    scroll, false, 0, 7, SSD1315::ScrollSpeed::FRAMES_5);

// Admission alone sends nothing. Poll admitted work, then consume its result.
if (st.ok()) {
  do {
    st = display.pollOperation(nowMs(), 1);
  } while (st.inProgress());
  SSD1315::OperationResult result;
  const SSD1315::Status take = display.takeOperationResult(result);
  st = take.ok() ? result.status : take;
}
// Report st before issuing further control commands.

// Later: stop scrolling. Direct control APIs return BUSY until the cooperative
// result above has been consumed. Controller RAM must be rewritten afterwards.
st = display.stopScroll();         // Check st, then explicitly render/flush
```

While SSD1315 hardware scroll is active, the driver blocks framebuffer flushes
with `STATE_ERROR` so normal RAM writes are not issued during scroll mode. On a
successful `stopScroll()`, the framebuffer is marked dirty; redraw if needed
and flush to restore controller GDDRAM alignment. Starting scroll is rejected
during page iteration. Stopping active scroll ends any iteration; restart it
to rewrite every window in page-buffer mode.

Hardware scroll is currently supported only for 128-column SSD1315
configurations. Non-128-wide panels may still draw and flush with their
configured width, but scroll setup returns `UNSUPPORTED` until that geometry is
tested. `startVerticalScroll()` validates its vertical offset against the
current vertical scroll area configured by `setVerticalScrollArea()`.
Initialization/resync explicitly commands a full-height area and resets fade,
zoom, and hardware scroll. `setVerticalScrollArea()` rejects zero rows, an area
beyond panel height, and `startLine >= scrollRows` before I2C.

The legacy `startHorizontalScroll()` and `startVerticalScroll()` overloads are
bounded blocking advanced compatibility calls. They perform up to three
sequential callback invocations with per-callback `i2cTimeoutMs`, but have no
request ID, operation deadline, or cancellation. Shared-bus owners should use the
cooperative `...ScrollOperation()` APIs.

### Panel Control Dirty State

Panel-control operations change controller registers rather than framebuffer
RAM. If an I2C failure occurs during a multi-command control sequence, cached
settings may no longer match the physical controller. The driver sets
`controlStateDirty()` and stores `controlStateError()` for failures in init,
recover, scroll setup, display mode, orientation, contrast, fade, zoom, and
sleep/all-on controls.

The dirty control-state flag is cleared only after a complete successful
initialize/resync command sequence, which explicitly restores the modeled
scroll area, fade-off, and zoom-off controls. This is not hardware readback.
Initialization replays the current configured contrast, inversion and X/Y
remap, but resets all-pixels-on, fade/blink, zoom and active scrolling to off,
and the scroll area to the full panel height. Reapply those effects after
successful recovery if the application needs them; they are not retained
settings.
For a full-buffer external owner, use
`startResync()` and consume its terminal result.
`recover()` is the full-buffer blocking compatibility path:

```cpp
if (display.controlStateDirty()) {
  SSD1315::Status st = display.recover();
  // On success, init, full framebuffer transfer, display-on, and the timing
  // interval are already complete.
}
```

`invalidatePanelState()` performs no I2C. It cancels an active cooperative
operation while retaining its consume-once result, or terminates an active
legacy flush with `CANCELLED` in `getFlushStatus()`/`lastError()` without
counting a transport failure. It also ends page iteration and retains the
current RAM window and dirty retry data. A completed legacy window's outcome
is still counted exactly once.

Page-buffer mode cannot perform full-buffer resync. Reinitialize off and
consume the result. If a failed/cancelled window remains, explicitly flush it
before `firstPage()`; that API protects pending retry data from being discarded.
Restart `firstPage()`, render and flush every page window, then explicitly wake.

## Command Passthrough

All driver-supported, I2C-applicable SSD1315 write commands are accessible:

```cpp
// Single command
display.sendCommand(SSD1315::cmd::DISPLAY_ALL_ON);

// Command with argument
display.sendCommand2(SSD1315::cmd::SET_CONTRAST, 0xFF);

// Command list
uint8_t cmds[] = {0xA6, 0xAF};
display.sendCommandList(cmds, sizeof(cmds));
```

`sendCommandList()` is a bounded blocking convenience API. It accepts at most
32 command bytes and invokes the callback once, permitting at most one physical
bus transaction. The complete opaque
command/argument stream plus control byte must fit `maxWriteBytes`; the driver
never splits it at an unknown argument boundary.

See [CommandTable.h](include/ssd1315/CommandTable.h) for supported write-command
definitions. Parallel-interface status/read commands are not available through
this write-only I2C driver.
Raw command APIs do not validate arbitrary command/argument patterns. Callers
must use documented SSD1315 command encodings and avoid unsupported bit
patterns. Every successful raw command invalidates the modeled panel-control
and power cache because the core cannot infer arbitrary command effects. Run a
full resync before using operations that require trustworthy modeled panel state.
Address NACK proves the raw command had no effect and retains the model. Data
NACK, timeout, or bus error is ambiguous and invalidates modeled control, power,
and the complete-GDDRAM baseline. Direct wake is rejected until resync.
`SCROLL_RIGHT_ONE_COL` (`0x2C`) and `SCROLL_LEFT_ONE_COL` (`0x2D`)
are exposed as raw constants only; no high-level helper enforces the datasheet's
two-frame delay requirement for consecutive content-scroll use.

### Rare / One-Time Procedures

Not applicable: SSD1315 exposes no nonvolatile programming, calibration
storage, write-cycle endurance, commissioning, or readback/reconciliation
procedure through this write-only driver. Raw command passthrough is bounded
advanced access; it does not create a maintenance or NVM API.

## Error Handling

Primary fallible APIs return `Status`:

```cpp
SSD1315::Status st = display.begin(cfg);
if (!st.ok()) {
  Serial.printf("Error: %s (code=%d, detail=%d)\n",
                st.msg, (int)st.code, st.detail);
}
```

The legacy `nextPage()` compatibility iterator returns `bool`; `false` means
either completion or failure, so callers must inspect `lastError()` before
starting another iteration. Memory-only drawing helpers clip or ignore invalid
coordinates as documented in their Doxygen contracts.

Error codes:
- `OK` - Success
- `INVALID_CONFIG` - Bad configuration parameter
- `INVALID_DIMENSIONS` - Unsupported width/height combination
- `INVALID_PAGE_COUNT` - `pageBufferPages` is outside the valid range
- `NOT_INITIALIZED` - No attached binding, or controller initialization required
- `STATE_ERROR` - Operation not valid in the current state
- `BUSY` - Transient conflict such as an active flush/operation or an
  unconsumed cooperative result
- `PANEL_NOT_READY` - Cooperative flush admission requires modeled power ON/OFF
- `CANCELLED` - Active operation was explicitly cancelled
- `CONTROL_STATE_UNKNOWN` - Full resynchronization is required
- `RESULT_NOT_AVAILABLE` - No unconsumed terminal operation result exists
- `I2C_NACK_ADDR` - Device not responding
- `I2C_NACK_DATA` - Data transmission failed
- `I2C_TIMEOUT` - I2C timeout
- `I2C_BUS_ERROR` - Arbitration/stuck-bus/other bus-level failure
- `TIMEOUT` - Operation timeout
- `BUFFER_OVERFLOW` - Buffer or transfer size exceeded supported bounds
- `BUFFER_TOO_SMALL` - Caller-provided buffer is smaller than the documented size
- `UNSUPPORTED` - Requested operation is not supported in this mode/backend
- `INTERNAL_ERROR` - Internal invariant failure or impossible callback contract violation
- `DEVICE_NOT_FOUND` - Definite address NACK from `probe()` after `attach()`
- `IN_PROGRESS` - Async operation in progress (not an error)
- `DRIVER_OFFLINE` - Legacy compatibility code; `OFFLINE` is diagnostic-only

## Health Tracking

The driver tracks communication health for diagnostics. It does not use health
state to admit, retry, or recover I2C work; the application bus owner owns that
policy.

### DriverState

```cpp
enum class DriverState : uint8_t {
  UNINIT,    // Controller lifecycle is uninitialized; counters may record init failure
  READY,     // Last counted operation/callback succeeded
  DEGRADED,  // 1 to (N-1) consecutive failures
  OFFLINE    // N+ consecutive failures (threshold reached)
};
```

`DriverState` is a communication diagnostic, not the lifecycle or proof that a
panel is physically online. State transitions occur only after callback-backed
tracked work. A cooperative operation publishes health exactly once at its
terminal result, not from its nested flush. Zero-I2C cancellation, deadline,
empty-flush completion, and local display-on invariant rejection do not change
health:

- Once initialized, a counted success -> `READY`
- Once initialized, 1 to `offlineThreshold - 1` consecutive failures -> `DEGRADED`
- Once initialized, failures >= `offlineThreshold` -> `OFFLINE` (including the
  first failure when the threshold is 1)
- `detach()` or `end()` -> `UNINIT`

A completed page-window outcome is accounted before a later tracked command
or operation takes ownership. An older success cannot erase a newer failure
when `nextPage()` eventually observes that window's result.

### Health API

```cpp
// Device presence check (diagnostic only, requires attach()/configured transport)
Status probe();

// Re-initialize after failure
Status recover();

// State queries
DriverState state() const;
bool isOnline() const;  // true if READY or DEGRADED

// Diagnostics
uint32_t lastOkMs() const;         // Timestamp of last success
uint32_t lastErrorMs() const;      // Timestamp of last error
Status lastError() const;          // Most recent error
uint8_t consecutiveFailures() const;
uint32_t totalFailures() const;
uint32_t totalSuccess() const;
```

### Configuration

```cpp
cfg.offlineThreshold = 3;  // Failures before OFFLINE (default: 3, min: 1)
```

### Owner Recovery Pattern

Use the `admitted` and `st` state from the owner flow above. Consume the failed
operation result first. The application sets `recoveryDue` only when its own
bounded retry/backoff policy permits one recovery attempt; `OFFLINE` alone
does not schedule work. Full resync requires full-buffer mode.

```cpp
if (recoveryDue && !admitted &&
    display.state() == SSD1315::DriverState::OFFLINE) {
  recoveryDue = false;                     // Consume one owner-policy decision
  SSD1315::OperationOptions options;
  options.requestId = nextRequestId();
  options.useDeadline = true;
  options.deadlineMs = recoveryDeadlineMs;
  st = display.startResync(options);
  admitted = st.ok();
  // On rejection, report st. On admission, use the same poll/result flow.
}
```

### Notes

- `probe()` is diagnostic-only: does not affect health counters or state
- `probe()` sends a NOP and checks ACK only. SSD1315 has no useful I2C identity
  register, so ACK does not prove controller type.
- `OFFLINE` is diagnostic-only and does not suppress an owner-admitted attempt.
- `recover()` requires an attached/begun binding and is a blocking compatibility
  facade over the cooperative resync state machine.
- `detach()`, `end()`, and the destructor perform zero I2C. Schedule and consume
  `startShutdown()` first when the physical panel must be shut down.
- Health counters reset when a binding is detached or replaced. Capture a
  settings/health snapshot before detach when post-mortem data is required.
- Parameter/configuration errors are rejected before I2C and do not update health
- Success/failure counters saturate at `UINT32_MAX` instead of wrapping

## Examples

| Example | Description |
|---------|-------------|
| [01_basic_bringup_cli](examples/01_basic_bringup_cli/) | Arduino bring-up diagnostic CLI with stress and feature commands |
| [espidf_basic](examples/espidf_basic/) | Native ESP-IDF bring-up diagnostic CLI with fixed buffers and `i2c_master` transport |

The unified `01_basic_bringup_cli` example includes:
- common bringup commands (`help`, `version`, `telemetry`, `scan`, `probe`, `recover`, `drv`, `read`, `cfg/settings`, `verbose`, `stress`)
- feature controls (`contrast`, `invert`, `flipx`, `flipy`, `display off/on`, `sleep`, `allon`, `zoom`, `fade`, scroll commands)
- graphics commands (`text`, `pattern`, `line`, `rect`, `fillrect`, `circle`, `fillcircle`, `flush`, `flushrect`)
- validation helpers (`stress_mix`, compact runner-owned `soakstep`,
  `selftest`/`featuretest`, `flushstress`, `burst`, `monitor`)

These examples are diagnostic/bring-up applications, not production shared-bus
templates. Their local adapters may own a bus/mutex and run blocking CLI
commands for test convenience. Production firmware should bind the driver to
its existing sole bus owner and use the cooperative operation API.

The Arduino example's `initWire()` is startup-only, before Wire owns the pins.
It uses open-drain bus-clear pulses and bounded SCL waits, and fails if either
line remains LOW. Runtime recovery must first detach the existing bus owner;
the CLI's display `reset`/`recover` commands do not call this startup helper.

The ESP-IDF example intentionally does not compile the Arduino CLI source. It
implements the main display bring-up, diagnostics, graphics, flush, scroll,
and stress paths natively. Both CLIs expose the executable smoke commands in
the HIL runbook, including `pattern checker`,
`scrollh right 0 7`, `scrollv left 0 7 1`, and `scroll stop`. Full hardware
validation still requires an operator to observe the display and record the
target results. Use `tools/run_ssd1315_hil.py` and
`docs/SSD1315_HIL_RUNBOOK.md` for repeatable HIL device-test logging.

The HIL runner classifies serial responses, records machine-readable evidence,
and checks firmware/MCU identity, configuration and health counters. Visual
observations remain operator-owned. Its retries are bounded to selected
idempotent reads during a duration soak; display writes, explicit device
failures, resets, and final cleanup failures remain terminal.

For a reset-and-capture run, pass `--require-startup --startup-wait 0` to
validate the complete original startup before the first command. The runner
retains bounded live output and rejects incomplete health/workload records,
crashes and ambiguous completion before sending another command. A successful
serial result does not establish visual correctness.

```powershell
python tools/run_ssd1315_hil.py --dry-run --mode all --soak-ops 10
python tools/run_ssd1315_hil.py --mode smoke --port <serial-port> `
  --baud 115200 --out hil_logs --expect-address 0x3C
```

See [the HIL runbook](docs/SSD1315_HIL_RUNBOOK.md) for command plans, soak
options, retry boundaries, visual checklists, artifact handling, and failure
interpretation. Raw transcripts remain ignored under `hil_logs/`; only the
maintained evidence summary is committed.

### Example Helpers (`examples/common/`)

Not part of the library. These implement diagnostic project-level glue and
keep the examples self-contained:

| File | Purpose |
|------|---------|
| `BoardConfig.h` | Example-only pin, address, geometry, and bus-speed constants |
| `BuildConfig.h` | Compile-time `LOG_LEVEL` configuration |
| `Log.h` | Serial logging macros (`LOGE`/`LOGW`/`LOGI`/`LOGD`/`LOGT`/`LOGV`) |
| `I2cTransport.h` | Arduino Wire adapter for the bring-up example |
| `IdfI2cTransport.*` | ESP-IDF `driver/i2c_master.h` adapter for the native example |
| `I2cScanner.h` | I2C bus scanner with table output |
| `CliStyle.h` | Help-table formatting for the Arduino CLI |
| `CommandHandler.h` | Command parsing helpers (`readLine`, `match`, `parseInt`) |
| `HealthDiag.h` | Verbose health diagnostics with color, snapshots, and `HealthMonitor` |

## API Reference

This is a compact public-surface index. The generated Doxygen pages from
`include/ssd1315/` are authoritative for ranges, timing, side effects, return
codes, ownership, and threading restrictions. `Status.h`, `Config.h`, and
`CommandTable.h` also expose the documented status, transport, profile, command,
scroll, and operation enums plus their `toString()` helpers.

### Lifecycle

```cpp
Status attach(const Config& config); // Validate/bind/allocate; zero I2C
void detach();                       // Release local state; zero I2C
bool isAttached() const;
Status validateConfig(const Config& config) const;

Status startInitialize(const OperationOptions& options);
Status startFlush(const OperationOptions& options);
Status startSleep(const OperationOptions& options);
Status startWake(const OperationOptions& options);
Status startResync(const OperationOptions& options);
Status startShutdown(const OperationOptions& options);
Status startHorizontalScrollOperation(const OperationOptions& options,
                                      bool left, uint8_t startPage,
                                      uint8_t endPage, ScrollSpeed speed);
Status startVerticalScrollOperation(const OperationOptions& options,
                                    bool left, uint8_t startPage,
                                    uint8_t endPage, ScrollSpeed speed,
                                    uint8_t verticalOffset);
Status pollOperation(uint32_t nowMs, uint8_t maxTransactions,
                     uint16_t byteBudget = 0);
Status cancelOperation();            // Zero I2C
OperationProgress getOperationProgress() const;
Status takeOperationResult(OperationResult& out); // Consume once
PanelPowerState panelPowerState() const;
void invalidatePanelState();         // Zero I2C

Status begin(const Config& config);  // Blocking compatibility facade
Status recover();                    // Blocking compatibility facade
void tick(uint32_t nowMs);           // At most one callback; does not consume results
void end();                          // detach() alias; zero I2C
bool isInitialized() const;
const Config& getConfig() const;
Status getSettings(SettingsSnapshot& out) const; // Cached config and runtime state (no I2C)
SettingsSnapshot getSettings() const;            // Convenience snapshot
Status probe();                      // Raw presence check, no health tracking
DriverState state() const;
DriverState driverState() const;     // Compatibility alias for state()
bool isOnline() const;
bool controlStateDirty() const;
Status controlStateError() const;
```

### Drawing

```cpp
void clear();
void fill();
void setPixel(int16_t x, int16_t y, bool on = true);
bool getPixel(int16_t x, int16_t y) const;
void drawHLine(int16_t x, int16_t y, int16_t w, bool on = true);
void drawVLine(int16_t x, int16_t y, int16_t h, bool on = true);
void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, bool on = true);
void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, bool on = true);
void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, bool on = true);
void drawCircle(int16_t cx, int16_t cy, int16_t r, bool on = true);
void fillCircle(int16_t cx, int16_t cy, int16_t r, bool on = true);
Status drawBitmap(int16_t x, int16_t y, const uint8_t* bmp, int16_t w, int16_t h, size_t bitmapSizeBytes, bool on = true);
void drawBitmap(int16_t x, int16_t y, const uint8_t* bmp, int16_t w, int16_t h, bool on = true);
void drawChar(int16_t x, int16_t y, char c, bool on = true);
int16_t drawText(int16_t x, int16_t y, const char* str, bool on = true);
int16_t drawTextN(int16_t x, int16_t y, const char* data, size_t length,
                  bool on = true);
static int16_t getTextWidth(const char* str);
static int16_t getTextWidthN(const char* data, size_t length);
void fillCheckerboard(uint8_t size = 1);
void fillVerticalStripes(uint8_t width = 1);
void fillHorizontalStripes(uint8_t height = 1);
```

### Display Control

```cpp
Status setContrast(uint8_t contrast);      // 1..255; 0 is invalid
Status setBrightness(uint8_t brightness);  // Alias for setContrast()
Status setInvert(bool invert);
Status setFlipX(bool flip);
Status setFlipY(bool flip);
Status setSleep(bool sleep);
bool isSleeping() const;
Status setAllPixelsOn(bool allOn);
Status startHorizontalScroll(bool left, uint8_t startPage, uint8_t endPage,
                             ScrollSpeed speed = ScrollSpeed::FRAMES_5);
Status startVerticalScroll(bool left, uint8_t startPage, uint8_t endPage,
                           ScrollSpeed speed, uint8_t verticalOffset);
Status stopScroll();
Status setVerticalScrollArea(uint8_t topFixedRows, uint8_t scrollRows);
Status setFadeMode(FadeMode mode, uint8_t interval = 0);
Status setZoom(bool enable);
```

The blocking scroll helpers are advanced compatibility calls. Shared-bus
owners use the cooperative scroll-operation admissions listed under Lifecycle.

### Raw Command Access

```cpp
Status sendCommand(uint8_t command);
Status sendCommand2(uint8_t command, uint8_t argument);
Status sendCommand3(uint8_t command, uint8_t argument1, uint8_t argument2);
Status sendCommandList(const uint8_t* commands, size_t length); // 1..32 bytes
```

Raw commands use at most one callback, never split opaque command streams, and
invalidate modeled panel state as described under Command Passthrough.

### Deprecated Compatibility Storage And Helpers

```cpp
void setAutoSleep(uint32_t inactivityMs);
void touch();
void setUserPageCount(uint8_t count);
void setActiveUserPage(uint8_t page);
uint8_t getActiveUserPage() const;
uint8_t getUserPageCount() const;
void setPageCycleInterval(uint32_t intervalMs);
```

The timer/page methods remain for source compatibility, but they store values
only. They never cause drawing, `touch()`, or `tick()` to admit I2C work.

### Flush Control

```cpp
Status requestFlush();
Status requestFlushRect(int16_t x, int16_t y, int16_t w, int16_t h);
bool isFlushing() const;
Status pollFlush(uint32_t nowMs, uint8_t maxInstructions,
                 uint16_t byteBudget);
FlushStatus getFlushStatus() const;
Status lastError() const;
void clearLastError();
void clearError();                    // Compatibility alias
Status waitFlush(uint32_t nowMs, uint32_t timeoutMs = 0);
```

`waitFlush()` uses its `nowMs` argument as the exact wait start; zero is a valid
timestamp, not a sentinel. When configured, `Config::nowMs` supplies subsequent
samples on the same monotonic timebase. The helper is bounded even if that clock
stops advancing: it yields cooperatively between polls and returns `TIMEOUT` if
the time source stalls. It checks the total timeout before each tick and clips
each transport attempt to the remaining wait budget, so no callback starts at
or after the wait deadline.

### Page Buffer Mode

```cpp
bool isPageBufferMode() const;
Status firstPage();
bool nextPage();
bool isPageIterating() const;
uint8_t currentPageIndex() const;
uint8_t totalPages() const;
int16_t pageBufferYOffset() const;
```

`currentPageIndex()` is a RAM-window index, not always a physical GDDRAM page.
The first physical page is `currentPageIndex() * pageBufferPages`, and
`pageBufferYOffset()` is that page's first absolute pixel row. Drawing calls take
absolute panel coordinates in every window; never add the offset to them.

### Framebuffer And Dirty Tracking

```cpp
uint8_t* getBuffer();
const uint8_t* getBuffer() const;
size_t getBufferSize() const;
void markDirty(uint8_t page, uint8_t minCol = 0, uint8_t maxCol = 255);
void markAllDirty();
void markDirtyRect(int16_t x, int16_t y, int16_t w, int16_t h);
void clearDirty();                    // Force-clear escape hatch
Status clearDirtyIfIdle();            // Preserves active/failed retry state
bool isDirty() const;
```

Direct mutable-buffer access does not mark data dirty automatically. Call a
dirty-marking helper after mutation; use `clearDirtyIfIdle()` unless explicitly
discarding retained retry data.

## Threading Model

The driver is not internally thread-safe. Use one owner task, or externally
serialize every access to an instance. Public APIs are task-context only and
must not be called from ISRs. Transport callbacks must not re-enter the same
driver instance.

## Resource Ownership

- **I2C bus**: Application owns the bus; library uses callback only
- **Framebuffer**: Library may allocate once in `attach()`/`begin()`, or use a
  caller-owned external buffer; steady operation allocates nothing
- **Pins**: Application configures; library has no pin knowledge

## ESP-IDF Usage

The driver can be consumed as an ESP-IDF component. Applications own the
`i2c_master_bus_handle_t` and `i2c_master_dev_handle_t`, then provide callbacks
through `Config::i2cWrite`, `Config::nowMs`, and `Config::cooperativeYield`.
`examples/espidf_basic` is a native ESP-IDF bring-up CLI built on
`driver/i2c_master.h`. Its `transport::initIdfI2c(sda, scl, freq, address)`
allocates the example's bus/device and mutex; it does not set transfer deadlines.
`Config::i2cTimeoutMs` supplies the total mutex-wait plus transmit callback
budget, possibly reduced by an operation deadline. Old native `initWire` calls
must use the new name and remove the unused timeout argument. Arduino's
separate `initWire` timeout remains meaningful and unchanged. See
[docs/IDF_PORT.md](docs/IDF_PORT.md) for resource lifetime, migration, and the
diagnostic scan's separate timeout bounds.

## Building

Arduino environments are pinned to immutable pioarduino `55.03.311`, which
provides Arduino-ESP32 3.3.11 and ESP-IDF libraries 5.5.5. The ESP32-S3
environment uses the exact `esp32-s3-devkitc1-n16r8` board definition (16 MB
flash, 8 MB octal PSRAM). `compat_pioarduino_54_s3` is a build-only regression
environment for the previously qualified `54.03.20` stack; it is not used for
normal builds or HIL. The ESP-IDF component manifest supports `>=5.3.0,<6.0.0`;
CI defines native-example builds for S2/S3 with v5.3.5 and v5.5.5.
An Arduino build uses bundled ESP-IDF libraries; it does not validate the
native ESP-IDF example. That example needs its own `idf.py` build below.

On Windows, use `scripts/pio.cmd`. It resolves the current user's
VS Code-managed PlatformIO Core under `%USERPROFILE%` and never installs a
second Core. If that installation or one of its toolchains is incomplete, the
wrapper fails visibly; repair the global installation instead of adding a
compiler-path workaround to this library. On other operating systems, replace
`.\scripts\pio.cmd` below with an existing `pio` command.

```powershell
# Build default example
.\scripts\pio.cmd run

# Build specific environment
.\scripts\pio.cmd run -e esp32s3dev
.\scripts\pio.cmd run -e esp32s2dev
.\scripts\pio.cmd run -e compat_pioarduino_54_s3

# Run host/native tests (the native environment is a test target)
.\scripts\pio.cmd test -e native

# Generate public API documentation (Doxygen + Graphviz); warnings fail
doxygen Doxyfile

# In an activated ESP-IDF environment, build one selected target
idf.py -C examples/espidf_basic set-target esp32s3
idf.py -C examples/espidf_basic build

# Upload
.\scripts\pio.cmd run -t upload -e esp32s3dev
```

Use `esp32s2` instead of `esp32s3` for the other supported native target.
For the complete CI matrix, repeat in clean ESP-IDF v5.3.5 and v5.5.5
environments with separate build/configuration state for each SDK/target pair.
See [the IDF guide](docs/IDF_PORT.md) for the native adapter contract.

## Validation

```powershell
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python scripts/generate_version.py check
python tools/test_hil_runner_parser.py
python tools/test_package_contents.py
python tools/run_ssd1315_hil.py --dry-run --mode all --soak-ops 10
.\scripts\pio.cmd test -e native
.\scripts\pio.cmd run -e esp32s3dev
.\scripts\pio.cmd run -e esp32s2dev
.\scripts\pio.cmd run -e compat_pioarduino_54_s3
.\scripts\pio.cmd pkg pack
python tools/check_package_contents.py
doxygen Doxyfile
tar -tf SSD1315-<version>.tar.gz
```

Remove the generated package tarball after local validation unless you are
preparing a release artifact.

## Production Readiness Notes

- Core code is framework-neutral and transport-injected; Arduino `Wire`, ESP-IDF bus handles, reset GPIO, locks, and timeout policy belong to application adapters/examples.
- `attach()`, `detach()`, `end()`, and destruction are zero-I2C. Production bus
  owners schedule explicit initialize/flush/sleep/wake/resync/shutdown jobs and
  normally allow one transaction per poll. `begin()` and `recover()` are
  blocking compatibility facades over that same state machine.
- Each transport callback returns one terminal `TransportResult`, permits at
  most one physical transaction, and never retries. `OK` confirms the complete
  physical transaction. The core never locks. A callback may apply application-
  owned serialization within the supplied timeout, but must not recursively
  reacquire a lock already held by its caller; neither core nor callback may
  retry, recover, or replay an ambiguous OLED write.
- Controller command, timing, power-sequence, and module facts used by the
  driver are kept in
  [docs/SSD1315_CONTROLLER_REFERENCE.md](docs/SSD1315_CONTROLLER_REFERENCE.md).
- Hardware `RES#` sequencing is board-owned; `recover()` is software
  reinitialization only.
- Failed multi-command controls and successful raw passthrough invalidate cached
  panel state; use a full resync before relying on modeled controls/power.
- Failed framebuffer flushes preserve dirty GDDRAM data for retry.
- Driver instances are not thread-safe and public APIs are not ISR-safe. Shared-bus users must serialize access externally.
- OLED panels can retain static content or age unevenly. For production UI, avoid long-lived high-contrast static screens, dim inactive displays, and use sleep/blanking where the product allows it.

## Hardware Compatibility

The primary target is a 128x64 SSD1315 I2C panel with a matching electrical
profile. The following limits are part of the current API:

| Area | Supported scope and limitation |
| --- | --- |
| Controller | SSD1315 only. The default profile sends SSD1315-specific `SET_IREF`; no SSD1306 compatibility profile is implemented. |
| Geometry | Software accepts widths `1..128` and heights `16..64` in steps of 8. Flushes use GDDRAM columns `0..width-1`; modules requiring a nonzero column offset need a separate implementation and validation. Accepted geometry does not establish module compatibility. |
| Hardware scroll | Requires width 128; narrower configurations return `UNSUPPORTED`. |
| Page-buffer recovery | `startResync()` and `recover()` require a full framebuffer. With a page buffer, initialize off, render/flush every window, then explicitly wake. |
| Module wiring | Select power, COM pins, remap and IREF for the actual module. Profiles do not discover wiring or controller identity. |

Total flush timeouts, the display-on guard, and conservative invalidation after
raw commands are intentional current policies. Changing them or consolidating
compatibility APIs is separate design work, not a prerequisite to use the
implemented driver.

Serial hardware-in-the-loop evidence (ESP32-S3, address `0x3C`) is recorded in
[docs/SSD1315_HARDWARE_VALIDATION.md](docs/SSD1315_HARDWARE_VALIDATION.md).
Those dated Arduino diagnostic runs record serial-reported command completion
for their exact source and setup; they do not qualify later revisions, native
ESP-IDF hardware, or cooperative bus-owner scheduling. Panel identity, visual,
electrical, fault/recovery, reset-pin, and representative multi-unit soak results
remain open before field-grade qualification. Use
[docs/SSD1315_HIL_RUNBOOK.md](docs/SSD1315_HIL_RUNBOOK.md) to record them.

## Documentation

- [CHANGELOG.md](CHANGELOG.md) - release history
- [CONTRIBUTING.md](CONTRIBUTING.md) - contribution and validation requirements
- [SECURITY.md](SECURITY.md) - supported-release and security reporting policy
- `AGENTS.md` - repository engineering rules
- [docs/DOCUMENTATION.md](docs/DOCUMENTATION.md) - documentation map and evidence policy
- [docs/RETAINED_BRANCH_AUDIT.md](docs/RETAINED_BRANCH_AUDIT.md) - retained branch decisions, resolved software findings and remaining hardware qualification
- [docs/RELEASING.md](docs/RELEASING.md) - release procedure
- [docs/IDF_PORT.md](docs/IDF_PORT.md) - ESP-IDF component and example notes
- [docs/SSD1315_CONTROLLER_REFERENCE.md](docs/SSD1315_CONTROLLER_REFERENCE.md) - controller and module facts the driver relies on
- [docs/SSD1315_HIL_RUNBOOK.md](docs/SSD1315_HIL_RUNBOOK.md) - hardware validation procedure and per-target evidence form
- [docs/SSD1315_HARDWARE_VALIDATION.md](docs/SSD1315_HARDWARE_VALIDATION.md) - hardware evidence ledger

## License

MIT License. See [LICENSE](LICENSE) for details.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.
