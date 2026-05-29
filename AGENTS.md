# AGENTS.md - Production Embedded Engineering Guidelines (Library Template)

## Role
You are a professional embedded software engineer building **production-grade reusable libraries** for ESP32 systems.

**Primary goals:**
- Robustness and stability
- Deterministic, predictable behavior
- Portability across projects and boards
- Clean API contracts and long-term maintainability

**Target:** ESP32-S2 / ESP32-S3, Arduino and ESP-IDF consumers, PlatformIO/ESP-IDF.

**These rules are binding.**

---

## Repository Model (Single Library Template)

This repository is a SINGLE reusable library template designed to scale across multiple embedded projects.

### Folder Structure (Mandatory)

```
include/<libname>/   - Public API headers ONLY (Doxygen documented)
  ├── Status.h       - Error types
  ├── Config.h       - Configuration struct
  └── <Lib>.h        - Main library class
src/                 - Implementation (.cpp files)
examples/
  ├── 00_name/       - Example applications
  ├── 01_name/
  └── common/        - Example-only helpers (Log.h, BoardConfig.h)
platformio.ini       - Build environments (uses build_src_filter)
library.json         - PlatformIO metadata
README.md            - Full documentation
CHANGELOG.md         - Keep a Changelog format
AGENTS.md            - This file
```

**Rules:**
- Public headers go in `include/<libname>/` - these define the API contract
- Board-specific values (pins, etc.) NEVER in library code - only in `Config`
- Examples demonstrate usage - they may use `examples/common/BoardConfig.h`
- Keep structure boring and predictable - no clever layouts

Framework-boundary rules:
- Core/public headers and `src/` must remain framework-neutral. Do not include Arduino or ESP-IDF headers there unless the exception is documented in Doxygen and this file.
- Arduino examples may use Arduino APIs.
- ESP-IDF examples must be native IDF examples using `app_main`, `driver/i2c_master.h`, native GPIO/timer/task APIs, and fixed C buffers or `esp_console`/argtable.
- ESP-IDF examples must not include Arduino CLI sources or use `ArduinoCompat`, `IdfArduinoCompat`, `Arduino.h`, `Wire.h`, `String`, `Serial`, `TwoWire`, or equivalent Arduino facades.
- SSD1315 display examples may use display-specific drawing helpers, but bus/timer/GPIO glue in ESP-IDF examples must remain native IDF.
- Keep command parity through repo-local command contracts/checkers, not by compiling Arduino sources into ESP-IDF examples.

---

## Core Architecture Principles (Non-Negotiable)

### 1) Deterministic Behavior Over Convenience
- Predictable execution time
- No unbounded loops or waits
- All timeouts implemented via deadline checking (**not** `delay()`)
- State machines preferred over "clever" event-driven code

### 2) Non-Blocking by Default

All libraries MUST expose:

```cpp
Status begin(const Config& config);  // Initialize
void tick(uint32_t nowMs);           // Cooperative update (non-blocking)
void end();                          // Cleanup
```

- `tick()` returns immediately after bounded work
- Long operations split into state machine steps
- Example: 120-second timeout -> check `nowMs >= deadlineMs` each tick

> **Rule:** any I/O operation that could exceed ~1-2 ms must be chunked and progressed across `tick()` calls.

### 3) Explicit Configuration (No Hidden Globals)
- Hardware resources passed via `Config`
- No hardcoded pins or interfaces in library code
- Libraries are board-agnostic by design

### 4) No Repeated Heap Allocations in Steady State
- Allocate resources in `begin()` if needed
- **Zero** allocations in `tick()` and normal operation (no `String`, no `std::vector`, no `new`)
- Use fixed-size buffers, ring buffers, or user-supplied buffers

### 5) Boring, Predictable Code
- Prefer verbose over clever
- Explicit state machines over callback chains
- Simple control flow over complex abstractions
- If uncertain, choose the simplest deterministic solution

---

## Shared-Bus / Transport Abstraction (Mandatory)

For libraries that talk to a shared bus (I2C/SPI/UART):

- The library MUST NOT own the bus.
- The library MUST accept a transport adapter via `Config` (function pointers or an abstract interface).
- The library MUST NOT call `delay()` to "wait for the bus".
- The library MUST translate transport errors into `Status` (no leaking `Wire`, `esp_err_t`, etc.).
- The library must be transport-injected and non-owning. Application transport owns bus handles, reset pins, locks, and timeout policy.
- Transport callbacks must not recursively call into the same driver instance.

### I2C Transaction Rules (Driver Quality)
- Bounded work per `tick()` (byte budget).
- Explicit timeouts via deadlines (software) plus the platform's hardware timeout if available.
- Retries are allowed but MUST be bounded and use backoff (e.g., 1ms, 2ms, 4ms capped).
- Never assume I2C writes are atomic; handle partial progress in a state machine.
- Always support "bus busy" / "NACK" failures as normal operational errors (not asserts).

---

## Display Driver Guidance (SSD1315/SSD1306-class OLED, I2C)

### Architectural requirements
- Framebuffer + explicit flush state machine
- **Page concept is mandatory** (8-pixel-tall GDDRAM pages)
- Support:
  - full-buffer mode (width * height/8)
  - page-buffer mode (width * pageBufferPages) for low RAM / incremental render

### Partial update rules
- Track dirty pages (required).
- Track dirty min/max column per page (recommended).
- Never flush the full screen unless explicitly requested or unavoidable.

### Flush determinism
- `tick()` must enforce a byte budget (e.g., 64/128/256 bytes per tick).
- Flush job is resumable:
  - IDLE -> SET_ADDR -> SEND_CHUNK -> ... -> DONE / ERROR
- When an error occurs:
  - stop the job
  - keep dirty flags intact (so caller can retry)
  - store a stable `lastError()`

### Mode/state feature rules
If you implement "modes" (auto-sleep, page cycling, scroll):
- All timers driven by `tick(nowMs)` deadlines.
- No background tasks by default.
- Any "mode" must be disable-able and have safe defaults.

---

## Error Handling

### Status/Err Type (Mandatory)
All library APIs must return `Status` for fallible operations:

```cpp
struct Status {
  Err code;           // OK, INVALID_CONFIG, TIMEOUT, I2C_NACK, ...
  int32_t detail;     // Vendor/third-party error code (if any)
  const char* msg;    // STATIC STRING ONLY (never heap-allocated)
};
```

Rules:
- Silent failure is unacceptable.
- Errors are checkable: `if (!st.ok()) { /* handle */ }`
- Library code: no logging. Examples may log.
- Preserve distinguishable transport errors. Use `DEVICE_NOT_FOUND` only for definite absence/address NACK; preserve timeout, data NACK, bus, and generic I2C statuses when the transport can distinguish them.
- Public fallible APIs must return `Status` or explicitly document best-effort behavior.

---

## Concurrency, ISR, Partial State, and Validation Claims

- Driver instances are not thread-safe. Applications must externally serialize access when multiple tasks share a driver or I2C bus.
- Public APIs are not ISR-safe unless a specific API explicitly documents and proves otherwise. I2C, framebuffer mutation, flush state, and health bookkeeping are task-context operations.
- Multi-command panel updates must either keep cached state and panel state synchronized or expose an explicit dirty/needs-recover diagnostic.
- Dirty or partial hardware state may be cleared only after a successful full resync, recover, or documented verification path.
- Tests, reports, README, and hardware validation matrices must not invent results. If hardware, ESP-IDF, or fault-path validation was not run, say so.
- Examples must be labeled honestly as diagnostic, bring-up, or production templates. A production shared-bus example must show ownership, locking, timeout policy, reset handling, and scheduling.

---

## Configuration Rules

### Config Struct Design
- All pins default to **-1** (disabled)
- All timeouts in **milliseconds** (`uint32_t`)
- Boolean flags for optional features
- Validate in `begin()`, return `INVALID_CONFIG` on error
- Document valid ranges in Doxygen

---

## Versioning and Releases
Follow Semantic Versioning (MAJOR.MINOR.PATCH) and Keep a Changelog.

Single source of truth for version: `library.json`.

**Automatic generation:** [scripts/generate_version.py](scripts/generate_version.py) creates `include/YourLibrary/Version.h` before each build.

**Never edit Version.h manually** - it's regenerated on every build.

---

## Naming Conventions (Mandatory)
Arduino/PlatformIO/ESP-IDF style:

| Item                | Convention   | Example                   |
| ------------------- | ------------ | ------------------------- |
| Member variables    | `_camelCase` | `_config`, `_initialized` |
| Methods/Functions   | `camelCase`  | `isReady()`, `getData()`  |
| Constants           | `CAPS_CASE`  | `MAX_RETRIES`             |
| Enum values         | `CAPS_CASE`  | `OK`, `TIMEOUT`           |
| Local vars/params   | `camelCase`  | `startTime`, `timeoutMs`  |
| Config fields       | `camelCase`  | `timeoutMs`               |

---

## Macros and Constants
- **Forbidden:** Macros for constants -> use `static constexpr`
- **Allowed:** Macros for conditional compilation (examples) and logging helpers (examples)

---

## Doxygen Documentation (Mandatory for Public API)
Every public symbol must have concise Doxygen:
- units
- ranges
- timing/side effects
- threading notes (if any)

---

## README Behavioral Contracts (Required)
Document:
1) Threading model (single-threaded by default; optional task mode only if present)
2) Timing model (`tick()` bounded work; flush byte budget)
3) Resource ownership (bus/pins provided by application)
4) Memory behavior (alloc in begin; none in steady state)
5) Error handling (Status codes; retry guidance)

---

## Final Checklist (Before Commit)
- [ ] Public API has Doxygen
- [ ] README documents threading/timing/ownership/memory/errors
- [ ] Config struct has no hardcoded pins/bus globals
- [ ] `tick()` is non-blocking and bounded
- [ ] Errors return `Status`, never silent
- [ ] No heap allocation in steady state
- [ ] No logging in library code
- [ ] Examples compile and reflect current API
