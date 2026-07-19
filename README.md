# SSD1315 I2C OLED Display Driver

[![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32-orange)](https://platformio.org)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

Hardened I2C driver library for SSD1315 OLED displays on ESP32. The core is framework-neutral and works with Arduino/PlatformIO or ESP-IDF through application-owned I2C callbacks.

This repository targets SSD1315. SSD1306-like panels may work because many commands overlap, but compatibility is not guaranteed unless a separate controller profile and hardware validation are added. `probe()` can prove only address ACK, not controller identity, and ACK-less module wiring makes address-NACK policy board-specific.

## Features

- **Owner-budgeted operations** - one cooperative state machine for initialize,
  flush, sleep, wake, resync, and shutdown
- **Partial updates** - dirty tracking with column-level granularity  
- **Page buffer mode** - u8g2-style iteration for low RAM usage (128 bytes vs 1KB)
- **Hardware scroll** - horizontal and vertical scroll support on 128-column
  SSD1315 panels
- **Command access** - supported SSD1315 write-command constants, helpers, and
  raw write passthrough for the driver-supported command surface
- **Deterministic memory option** - use a caller-supplied framebuffer for
  production ownership; internal allocation remains a bring-up convenience
- **Robust error handling** - Status return type on all fallible operations
- **Transport abstraction** - no Wire dependency; inject your own I2C callback
- **ESP-IDF component support** - root CMake metadata and a native `i2c_master` example

## Quick Start

```cpp
#include <Arduino.h>
#include <Wire.h>
#include "SSD1315.h"

// One terminal result from exactly one physical I2C attempt.
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

void setup() {
  Wire.begin(8, 9);  // SDA, SCL
  Wire.setClock(400000);
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
  display.requestFlush();
}

void loop() {
  display.tick(millis());  // Drive state machine
}
```

## Configuration Reference

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `controllerProfile` | enum | SSD1315 | Controller profile. Only SSD1315 is currently supported; includes SSD1315 `SET_IREF` |
| `width` | uint8_t | 128 | Display width in pixels (1-128) |
| `height` | uint8_t | 64 | Display height in pixels (16, 32, 64; multiple of 8) |
| `i2cAddress` | uint8_t | 0x3C | SSD1315 7-bit I2C address (`0x3C` or `0x3D`; do not pass 8-bit forms `0x78`/`0x7A`) |
| `i2cWrite` | function | nullptr | **Required.** I2C write callback |
| `i2cUser` | void* | nullptr | User context for callback |
| `maxWriteBytes` | uint16_t | 65 | Total callback write capacity, including the control byte (`4..129`) |
| `nowMs` | function | `nullptr` | Optional monotonic clock source; examples should inject the platform timer |
| `cooperativeYield` | function | `nullptr` | Optional yield hook for bounded wait helpers |
| `timeUser` | void* | `nullptr` | User context for `nowMs` / `cooperativeYield` |
| `pageBufferPages` | uint8_t | 8 | Pages in RAM buffer (1 to height/8) |
| `byteBudgetPerTick` | uint16_t | 128 | Max data bytes for one `tick()` data instruction; must be greater than zero |
| `i2cTimeoutMs` | uint32_t | 25 | I2C transaction timeout |
| `flushTimeoutMs` | uint32_t | 1000 | Total flush timeout (0=none) |
| `displayOnDelayMs` | uint32_t | 100 | Power-on timing guard |
| `clearOnBegin` | bool | true | Blocking compatibility choice: full resync and wake, or initialize off |
| `clearOnRecover` | bool | true | Deprecated compatibility storage; `recover()` always performs full resync |
| `inactivitySleepMs` | uint32_t | 0 | Deprecated compatibility storage; core never admits sleep policy |
| `pageCycleMs` | uint32_t | 0 | Deprecated compatibility storage; core never admits page policy |
| `flipX` | bool | false | Flip horizontally (segment remap) |
| `flipY` | bool | false | Flip vertically (COM scan) |
| `invert` | bool | false | Invert display colors |
| `contrast` | uint8_t | 0x7F | Initial contrast (`1..255`; `0` is rejected) |
| `comPins` | enum | ALTERNATIVE_NO_REMAP | COM pin configuration |
| `chargePumpVoltage` | enum | V7_5 | Charge pump mode/voltage (`OFF`, `V7_5`, `V8_5`, `V9_0`) |
| `iref` | enum | INTERNAL_19UA | IREF selection (SSD1315) |
| `vcomh` | enum | V_077_VCC | VCOMH deselect level |
| `clockDivide` | uint8_t | 1 | Display clock divide ratio |
| `oscFrequency` | uint8_t | 8 | Oscillator frequency trim |
| `prechargePhase1` | uint8_t | 2 | Pre-charge phase 1 DCLK count |
| `prechargePhase2` | uint8_t | 2 | Pre-charge phase 2 DCLK count |
| `displayOffset` | uint8_t | 0 | Vertical display offset (`0xD3`) |
| `startLine` | uint8_t | 0 | Display start line (`0x40..0x7F`) |
| `offlineThreshold` | uint8_t | 3 | Diagnostic threshold for `OFFLINE`; never gates I2C admission |
| `externalBuffer` | uint8_t* | nullptr | External framebuffer (optional) |
| `externalBufferSizeBytes` | size_t | 0 | Required external framebuffer length |

Use `SSD1315::applyPanelProfile(cfg, profile)` before `begin()` when targeting
a documented 128x64 panel preset. Current presets are:
`GENERIC_128X64_INTERNAL_CHARGE_PUMP`,
`WISEVISION_X096_2864KSWPG01_H30_INTERNAL_DC_DC`, and
`WISEVISION_X096_2864KSWPG01_H30_EXTERNAL_VCC`. These are electrical/panel
presets, not SSD1306 compatibility profiles; transport, address, reset GPIO,
bus speed, and buffering remain application-owned.

## Memory Modes

By default, the driver allocates its framebuffer during `attach()`/`begin()`. That
convenience mode is acceptable for bring-up and simple applications, but
production firmware that requires deterministic memory ownership should provide
`externalBuffer` in `Config`. The external buffer must remain valid until
`end()` and must be at least `width * pageBufferPages` bytes; set
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
- Must use firstPage()/nextPage() iteration
- Initialization leaves the panel off; `startResync()` is unsupported
- The owner iterates and flushes every page while off, then explicitly wakes
- `nextPage()` is non-blocking; `tick()` or the owner operation poll advances I2C
- Renders entire screen each frame, but only page buffer in RAM
- Best for static or slowly-changing content
- clear()/fill() affect only the current buffer window; use firstPage()/nextPage() to cover the full display

```cpp
cfg.pageBufferPages = 1;  // Minimal RAM

// begin(cfg) initializes the controller and leaves the panel off.
display.firstPage();
while (display.isPageIterating()) {
  display.tick(millis());
  if (!display.isFlushing()) {
    display.clear();
    display.drawText(0, 0, "Title"); // clipped to the current page window
    display.fillCircle(64, 32, 10);
    display.nextPage();              // queues this window, never blocks
  }
}

SSD1315::OperationOptions wake;
wake.requestId = 1;
display.startWake(wake);             // explicitly present the complete frame
while (display.pollOperation(millis(), 1).inProgress()) {
  yield();
}
SSD1315::OperationResult wakeResult;
display.takeOperationResult(wakeResult);
```

## Timing And Operation Model

The v4 API has one fixed, allocation-free operation state machine. An external
owner admits work with `startInitialize()`, `startFlush()`, `startSleep()`,
`startWake()`, `startResync()`, or `startShutdown()`, then calls
`pollOperation(nowMs, maxTransactions, byteBudget)`. Admission is zero-I2C.
`OperationOptions` carries a nonzero request ID and an optional absolute,
wrap-safe deadline. `OperationProgress` exposes phase, effect certainty,
verified power state, bytes, chunks, and attempted transactions. One terminal
`OperationResult` is retrieved exactly once with `takeOperationResult()`.
`cancelOperation()` performs no I2C and preserves unconfirmed dirty data.

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
  SSD1315::Status take = display.takeOperationResult(result);
  // Match result.requestId and consume success/failure/cancel/timeout once.
}
```

`pollOperation()` accepts at most eight transactions per call. A normal shared-
bus owner should pass `maxTransactions = 1`, guaranteeing at most one transport
callback across all operation phases. Data is additionally limited by the
explicit byte budget and `maxWriteBytes - 1`; the control byte is included in
the transport capacity but excluded from the payload budget.

For a 128x64 full buffer:

| Operation | Capacity / budget | Physical I2C attempts |
| --- | --- | ---: |
| Initialize off | Any valid capacity | 17 |
| Full resync | `maxWriteBytes=129`, payload budget 128 | 42: 17 init + 8 x (column, page, data) + display-on |
| Full resync | Default `maxWriteBytes=65` | 50 |

The display-on timing interval is a zero-I2C phase after the final command.
Per-attempt timeout is clipped to an operation deadline. The core performs no
retry, bus recovery, lock acquisition, backoff, or bus initialization.

`begin()` and `recover()` remain bounded blocking compatibility facades over
this same state machine. Shared-bus owners should use passive `attach()` and
the cooperative start/poll/result API instead.

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

For latency-sensitive systems, keep `byteBudgetPerTick` small and prefer
`requestFlush()` with `tick()` or explicit `pollFlush()` budgets over
`waitFlush()`.

### Power-On Timing

The SSD1315 requires settling time after display ON before the panel is fully
active. The driver enforces this non-blocking via `displayOnDelayMs`. During
this period, flush operations are deferred. A zero delay is immediate; delayed
paths are safe even when the first caller timestamp is `0`.

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

## Hardware Scrolling

Hardware scroll moves pixels on the display without CPU involvement:

```cpp
// Horizontal scroll
display.startHorizontalScroll(false, 0, 7, SSD1315::ScrollSpeed::FRAMES_5);

// Vertical + horizontal scroll
display.startVerticalScroll(true, 0, 7, SSD1315::ScrollSpeed::FRAMES_4, 1);

// Stop scrolling; controller RAM must be rewritten after scroll
display.stopScroll();
```

While SSD1315 hardware scroll is active, the driver blocks framebuffer flushes
with `STATE_ERROR` so normal RAM writes are not issued during scroll mode. On a
successful `stopScroll()`, the framebuffer is marked dirty; redraw if needed
and flush to restore controller GDDRAM alignment.

Hardware scroll is currently supported only for 128-column SSD1315
configurations. Non-128-wide panels may still draw and flush with their
configured width, but scroll setup returns `UNSUPPORTED` until that geometry is
tested. `startVerticalScroll()` validates its vertical offset against the
current vertical scroll area configured by `setVerticalScrollArea()`.

### Panel Control Dirty State

Panel-control operations change controller registers rather than framebuffer
RAM. If an I2C failure occurs during a multi-command control sequence, cached
settings may no longer match the physical controller. The driver sets
`controlStateDirty()` and stores `controlStateError()` for failures in init,
recover, scroll setup, display mode, orientation, contrast, fade, zoom, and
sleep/all-on controls.

The dirty control-state flag is cleared only after a successful verified
initialize/resync sequence. For a full-buffer external owner, use
`startResync()` and consume its terminal result.
`recover()` is the full-buffer blocking compatibility path:

```cpp
if (display.controlStateDirty()) {
  SSD1315::Status st = display.recover();
  // On success, init, full framebuffer transfer, display-on, and the timing
  // interval are already complete.
}
```

Page-buffer mode cannot perform full-buffer resync. Reinitialize off, render and
flush every page window, then explicitly wake.

## Command Passthrough

All SSD1315 commands are accessible:

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
32 command bytes per call; use explicit library operations or separate bounded
calls for longer setup sequences.

See [CommandTable.h](include/ssd1315/CommandTable.h) for all command definitions.
Raw command APIs do not validate arbitrary command/argument patterns. Callers
must use documented SSD1315 command encodings and avoid unsupported bit
patterns. Every successful raw command invalidates the modeled panel-control
and power cache because the core cannot infer arbitrary command effects. Run a
full resync before using operations that require verified panel state.
`SCROLL_RIGHT_ONE_COL` (`0x2C`) and `SCROLL_LEFT_ONE_COL` (`0x2D`)
are exposed as raw constants only; no high-level helper enforces the datasheet's
two-frame delay requirement for consecutive content-scroll use.

## Error Handling

All fallible operations return `Status`:

```cpp
SSD1315::Status st = display.begin(cfg);
if (!st.ok()) {
  Serial.printf("Error: %s (code=%d, detail=%d)\n", 
                st.msg, (int)st.code, st.detail);
}
```

Error codes:
- `OK` - Success
- `INVALID_CONFIG` - Bad configuration parameter
- `INVALID_DIMENSIONS` - Unsupported width/height combination
- `INVALID_PAGE_COUNT` - `pageBufferPages` is outside the valid range
- `NOT_INITIALIZED` - No attached binding, or controller initialization required
- `STATE_ERROR` - Operation not valid in the current state
- `BUSY` - Transient operation conflict such as active flush
- `PANEL_NOT_READY` - Requested work requires a confirmed panel power state
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
  UNINIT,    // Not initialized
  READY,     // Last I2C transaction succeeded
  DEGRADED,  // 1 to (N-1) consecutive failures
  OFFLINE    // N+ consecutive failures (threshold reached)
};
```

State transitions occur based on tracked I2C results:
- Success from `READY`/`DEGRADED`/`OFFLINE` -> `READY`
- First failure -> `DEGRADED`
- Failures >= `offlineThreshold` -> `OFFLINE`
- `end()` -> `UNINIT`

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

```cpp
if (display.state() == SSD1315::DriverState::OFFLINE &&
    !display.getOperationProgress().status.inProgress()) {
  SSD1315::OperationOptions options;
  options.requestId = nextRequestId();
  options.useDeadline = true;
  options.deadlineMs = recoveryDeadlineMs;
  display.startResync(options);
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
- Health counters persist across detach/end for post-mortem analysis and reset
  on the next successful attachment.
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
- validation helpers (`stress_mix`, `selftest`/`featuretest`, `flushstress`, `burst`, `monitor`)

These examples are diagnostic/bring-up applications, not production shared-bus
templates. Their local adapters may own a bus/mutex and run blocking CLI
commands for test convenience. Production firmware should bind the driver to
its existing sole bus owner and use the cooperative operation API.

The ESP-IDF example intentionally does not compile the Arduino CLI source. It
implements the main display bring-up, diagnostics, graphics, flush, scroll,
and stress paths natively. Both CLIs expose the executable smoke commands used
by `docs/SSD1315_HARDWARE_VALIDATION.md`, including `pattern checker`,
`scrollh right 0 7`, `scrollv left 0 7 1`, and `scroll stop`. Full hardware
validation still requires an operator to observe the display and record the
matrix results. Use `tools/run_ssd1315_hil.py` and
`docs/SSD1315_HIL_RUNBOOK.md` for repeatable HIL device-test logging.

The HIL runner is a serial device tester and evidence collector. It can
classify command responses, parse `version`/`telemetry`/`cfg`/stress counters,
and write machine-readable artifacts, but it does not claim visual pass automatically.
Visual commands are recorded as operator-required unless `--interactive-visual`
is used and the operator enters pass/fail observations.

Pre-HIL smoke sequence used by the runbook, hardware matrix, and runner:

```text
version
telemetry
scan
probe
cfg
selftest
pattern checker
clear
fill
invert 1
invert 0
contrast 1
contrast 127
contrast 255
flipx 1
flipx 0
flipy 1
flipy 0
scrollh right 0 7
scrollv left 0 7 1
scroll stop
recover
stress 100
stress_mix 100
monitor 1000
monitor 0
telemetry
contrast 127
clear
cfg
```

Runner modes:

```bash
python tools/run_ssd1315_hil.py --dry-run --mode functional
python tools/run_ssd1315_hil.py --mode smoke --port <serial-port> --baud 115200 --out hil_logs --expect-address 0x3C
python tools/run_ssd1315_hil.py --mode functional --port <serial-port> --baud 115200 --out hil_logs --interactive-visual
python tools/run_ssd1315_hil.py --mode retention --port <serial-port> --baud 115200 --out hil_logs --interactive-visual
python tools/run_ssd1315_hil.py --mode soak --port <serial-port> --baud 115200 --out hil_logs --soak-ops 1000
```

Each real run creates a timestamped directory with `serial_transcript.txt`,
`summary.md`, `results.json`, `results.csv`, `metadata.json`,
`operator_visual_checklist.md`, `hardware_matrix_fragment.md`, parsed cfg
snapshots, health delta, failure analysis, and the command plan. The runner is
burn-in cautious by default: it warns around full-on/high-contrast static
commands and restores `contrast 127`, `invert 0`, `scroll stop`, and `clear` in
the standard plans.

### Example Helpers (`examples/common/`)

Not part of the library. These simulate project-level glue and keep examples self-contained:

| File | Purpose |
|------|---------|
| `BoardConfig.h` | Pin definitions and Wire init for supported boards |
| `BuildConfig.h` | Compile-time `LOG_LEVEL` configuration |
| `Log.h` | Serial logging macros (`LOGE`/`LOGW`/`LOGI`/`LOGD`/`LOGT`/`LOGV`) |
| `I2cTransport.h` | Arduino Wire adapter or ESP-IDF adapter selector for examples |
| `IdfI2cTransport.*` | ESP-IDF `driver/i2c_master.h` adapter for the native example |
| `I2cScanner.h` | I2C bus scanner with table output |
| `BusDiag.h` | Bus diagnostics wrapper (scan + probe) |
| `CliShell.h` | Serial command-line shell with line editing |
| `CommandHandler.h` | Command parsing helpers (`readLine`, `match`, `parseInt`) |
| `HealthView.h` | Compact health status display |
| `HealthDiag.h` | Verbose health diagnostics with color, snapshots, and `HealthMonitor` |
| `TransportAdapter.h` | Transport function pointer adapter |

## API Reference

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
Status pollOperation(uint32_t nowMs, uint8_t maxTransactions,
                     uint16_t byteBudget = 0);
Status cancelOperation();            // Zero I2C
OperationProgress getOperationProgress() const;
Status takeOperationResult(OperationResult& out); // Consume once
PanelPowerState panelPowerState() const;
void invalidatePanelState();         // Zero I2C

Status begin(const Config& config);  // Blocking compatibility facade
Status recover();                    // Blocking compatibility facade
void tick(uint32_t nowMs);           // Legacy one-instruction progress
void end();                          // detach() alias; zero I2C
bool isInitialized() const;
const Config& getConfig() const;
Status getSettings(SettingsSnapshot& out) const; // Cached config and runtime state (no I2C)
Status probe();                      // Raw presence check, no health tracking
DriverState state() const;
bool isOnline() const;
bool controlStateDirty() const;
Status controlStateError() const;
```

### Drawing

```cpp
void clear();
void fill();
void setPixel(int16_t x, int16_t y, bool on = true);
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
static int16_t getTextWidthN(const char* data, size_t length);
void markDirtyRect(int16_t x, int16_t y, int16_t w, int16_t h);
```

### Display Control

```cpp
Status setContrast(uint8_t contrast);      // 1..255; 0 is invalid
Status setBrightness(uint8_t brightness);  // Alias for setContrast()
Status setInvert(bool invert);
Status setFlipX(bool flip);
Status setFlipY(bool flip);
Status setSleep(bool sleep);
Status setAllPixelsOn(bool allOn);
```

### Deprecated Compatibility Storage And Helpers

```cpp
void setAutoSleep(uint32_t inactivityMs);
void touch();
void setUserPageCount(uint8_t count);
void setActiveUserPage(uint8_t page);
uint8_t getActiveUserPage() const;
uint8_t getUserPageCount() const;
void setPageCycleInterval(uint32_t intervalMs);
Status requestFlushRect(int16_t x, int16_t y, int16_t w, int16_t h);
```

The timer/page methods remain for source compatibility, but they store values
only. They never cause drawing, `touch()`, or `tick()` to admit I2C work.

Notes:

- `probe()` is diagnostic-only and does not affect health counters.
- `recover()` performs a full-buffer resync through the same state machine; it
  does not toggle RES#. It is unsupported in page-buffer mode.

### Flush Control

```cpp
Status requestFlush();
Status requestFlushRect(int16_t x, int16_t y, int16_t w, int16_t h);
bool isFlushing() const;
Status lastError() const;
Status waitFlush(uint32_t nowMs, uint32_t timeoutMs = 0);
```

`waitFlush()` is bounded even with an injected clock that stops advancing: it yields
cooperatively between polls and returns `TIMEOUT` if the time source stalls.

### Page Buffer Mode

```cpp
bool isPageBufferMode() const;
void firstPage();
bool nextPage();
uint8_t currentPageIndex() const;
int16_t pageBufferYOffset() const;
```

## Threading Model

**Single-threaded only.** Call all methods from the same task (typically `loop()`). The driver is not thread-safe and should not be called from ISRs.

## Resource Ownership

- **I2C bus**: Application owns the bus; library uses callback only
- **Framebuffer**: Library may allocate once in `attach()`/`begin()`, or use a
  caller-owned external buffer; steady operation allocates nothing
- **Pins**: Application configures; library has no pin knowledge

## ESP-IDF Usage

The driver can be consumed as an ESP-IDF component. Applications own the
`i2c_master_bus_handle_t` and `i2c_master_dev_handle_t`, then provide callbacks
through `Config::i2cWrite`, `Config::nowMs`, and
`Config::cooperativeYield`. The example under `examples/espidf_basic` is a
native ESP-IDF CLI using `app_main()`, fixed C buffers, and the bounded
`driver/i2c_master.h` adapter. The example transport owns a mutex to demonstrate
example-local serialization, and stdin is configured nonblocking so display
`tick()` continues while the CLI is idle. It does not include Arduino CLI
sources or Arduino compatibility facades. It remains a bring-up diagnostic, not
a production shared-bus ownership template.

## Building

```bash
# Build default example
pio run

# Build specific environment
pio run -e esp32s3dev
pio run -e esp32s2dev

# Run host/native tests (the native environment is a test target)
pio test -e native

# Build the ESP-IDF example from examples/espidf_basic when idf.py is available
idf.py -C examples/espidf_basic set-target esp32s3
idf.py -C examples/espidf_basic build
idf.py -C examples/espidf_basic fullclean
idf.py -C examples/espidf_basic set-target esp32s2
idf.py -C examples/espidf_basic build

# Upload
pio run -t upload -e esp32s3dev
```

## Validation

```bash
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python scripts/generate_version.py check
python -m py_compile tools/run_ssd1315_hil.py tools/check_cli_contract.py
python tools/run_ssd1315_hil.py --dry-run
python tools/test_hil_runner_parser.py
python tools/run_ssd1315_hil.py --dry-run --mode smoke
python tools/run_ssd1315_hil.py --dry-run --mode functional
python tools/run_ssd1315_hil.py --dry-run --mode retention
python tools/run_ssd1315_hil.py --dry-run --mode soak --soak-ops 10
python tools/run_ssd1315_hil.py --dry-run --mode all --soak-ops 10
python -m platformio test -e native
python -m platformio run -e esp32s3dev
python -m platformio run -e esp32s2dev
python -m platformio pkg pack
python tools/check_package_contents.py
tar -tf SSD1315-<version>.tar.gz
```

Remove the generated package tarball after local validation unless you are
preparing a release artifact.

Pure ESP-IDF builds require `idf.py`:

```bash
idf.py -C examples/espidf_basic set-target esp32s3
idf.py -C examples/espidf_basic build
idf.py -C examples/espidf_basic fullclean
idf.py -C examples/espidf_basic set-target esp32s2
idf.py -C examples/espidf_basic build
```

## Production Readiness Notes

- Core code is framework-neutral and transport-injected; Arduino `Wire`, ESP-IDF bus handles, reset GPIO, locks, and timeout policy belong to application adapters/examples.
- `attach()`, `detach()`, `end()`, and destruction are zero-I2C. Production bus
  owners schedule explicit initialize/flush/sleep/wake/resync/shutdown jobs and
  normally allow one transaction per poll. `begin()` and `recover()` are
  blocking compatibility facades over that same state machine.
- Each transport callback returns a terminal `TransportResult` for exactly one
  physical attempt. The callback and core must not retry, recover, lock, or
  replay an ambiguous OLED write.
- Electrical and reset limits from the chip/module source documents are kept in
  [SSD1315_DATASHEET_ALIGNMENT.md](docs/SSD1315_DATASHEET_ALIGNMENT.md).
- `probe()` is diagnostic-only and preserves timeout, bus, data-NACK, and generic I2C errors. `DEVICE_NOT_FOUND` is reserved for definite address NACK when the module wires `SDAOUT`/ACK. ACK is not SSD1315 identity.
- `recover()` is software reinitialization only. Hardware `RES#` sequencing is board-owned and must be handled by the application if the panel requires it.
- Failed multi-command controls and successful raw passthrough invalidate cached
  panel state; use a full resync before relying on modeled controls/power.
- Failed framebuffer flushes preserve dirty GDDRAM data for retry.
- Driver instances are not thread-safe and public APIs are not ISR-safe. Shared-bus users must serialize access externally.
- `OFFLINE` is an observable diagnostic threshold only; it does not own
  admission or recovery policy.
- This repository targets SSD1315. SSD1306 compatibility is not claimed because the default profile sends SSD1315-specific commands such as `SET_IREF`.
- OLED panels can retain static content or age unevenly. For production UI, avoid long-lived high-contrast static screens, dim inactive displays, and use sleep/blanking where the product allows it.
- If `clear` appears to leave old content, separate software state from panel
  retention before accepting HIL evidence: run `recover`, `scroll stop`,
  `invert 0`, `clear`, then `display off`. A ghost that remains visible while
  the display is off or across a safe power cycle points to physical image
  retention, optical residue, or panel aging rather than live GDDRAM bytes.

## Hardware Compatibility

Target controller/profile:
- SSD1315 128x64 I2C panels, including Wisevision-style modules when their
  power, reset, COM pin, remap, contrast, and IREF requirements match the
  configured profile.

SSD1306-like panels may work, but compatibility is not guaranteed unless a
future `ControllerProfile::SSD1306_COMPAT` (or equivalent) removes/guards
SSD1315-specific commands and is hardware-validated.

Committed serial HIL evidence exists for a COM29 ESP32-S2 Arduino/PlatformIO
run at address `0x3C`, including functional, retention, benchmark, an 8-hour
serial soak, and post-soak serial cleanup. It is recorded in
[docs/reports/hil-validation-COM29-20260623.md](docs/reports/hil-validation-COM29-20260623.md).
That is useful serial/device evidence, but it is not complete field validation:
visual pass/fail evidence, photos/video, safe physical fault injection,
reset-pin behavior, and logic-analyzer captures remain incomplete in the
maintained matrix. Use
[docs/SSD1315_HARDWARE_VALIDATION.md](docs/SSD1315_HARDWARE_VALIDATION.md)
and [docs/SSD1315_HIL_RUNBOOK.md](docs/SSD1315_HIL_RUNBOOK.md) to record
representative visual, fault/recovery, reset, and soak results before claiming
field-grade readiness.

This work is software-contract hardening after CI passes. It is not
field-release complete until representative hardware validation, fault/recovery
checks, and soak evidence are recorded.

## Documentation

- [CHANGELOG.md](CHANGELOG.md) - full release history
- `AGENTS.md` - repository engineering rules for future changes
- [docs/README.md](docs/README.md) - maintained documentation map and evidence policy
- [docs/SSD1315_READINESS_SUMMARY.md](docs/SSD1315_READINESS_SUMMARY.md) - current readiness summary
- [docs/IDF_PORT.md](docs/IDF_PORT.md) - ESP-IDF portability guidance
- [docs/SSD1315_DATASHEET_ALIGNMENT.md](docs/SSD1315_DATASHEET_ALIGNMENT.md) - controller and panel-profile contract
- [docs/SSD1315_HIL_RUNBOOK.md](docs/SSD1315_HIL_RUNBOOK.md) - repeatable hardware validation procedure
- [docs/SSD1315_HIL_TARGET_TEMPLATE.md](docs/SSD1315_HIL_TARGET_TEMPLATE.md) - target-specific operator template
- [docs/SSD1315_HARDWARE_VALIDATION.md](docs/SSD1315_HARDWARE_VALIDATION.md) - matrix for real hardware results
- [docs/SSD1315_I2C_Command_Reference.md](docs/SSD1315_I2C_Command_Reference.md) - command reference notes
- [docs/SSD1315_datasheet.pdf](docs/SSD1315_datasheet.pdf) - device reference material
- [docs/Wisevision_X096-2864KSWPG01-H30_module_spec.pdf](docs/Wisevision_X096-2864KSWPG01-H30_module_spec.pdf) - display module reference sheet

## License

MIT License. See [LICENSE](LICENSE) for details.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.
