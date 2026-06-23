# AGENTS.md - SSD1315 Production Embedded Guidelines

## Role
You are a professional embedded software engineer maintaining a hardened SSD1315
I2C OLED display driver for ESP32 systems.

**Primary goals:**
- Robustness and stability
- Deterministic, predictable behavior
- Portability across projects and boards
- Clean API contracts and long-term maintainability

**Target:** ESP32-S2 / ESP32-S3, Arduino and ESP-IDF consumers, PlatformIO/ESP-IDF.

**These rules are binding.**

---

# SSD1315 production-hardening rules

- Core library code in `include/` and `src/` must remain framework-neutral: no Arduino, Wire, ESP-IDF, FreeRTOS, heap-heavy UI helpers, or platform logging in core.
- The core driver must not own the I2C bus. Bus creation, pins, clock rate, locking, reset GPIOs, and bus recovery belong to the platform adapter/application.
- Public fallible APIs must return `Status` or the repository-standard equivalent; do not introduce exceptions or hidden dynamic allocation.
- Public APIs are not ISR-safe and the driver instance is not internally thread-safe unless explicitly changed and tested.
- Probe/scan must not claim SSD1315 identity. On I2C, this driver can usually prove address ACK only, not controller identity.
- SSD1306 compatibility must be profile-based or clearly qualified. Do not send SSD1315-specific commands such as `SET_IREF` while advertising generic SSD1306 compatibility unless documented as an SSD1315 profile.
- Multi-command panel-control operations must either be individually recoverable or mark panel control state as possibly dirty and provide a resync path.
- GDDRAM/flush dirty-page behavior must preserve dirty data after failed flushes.
- `begin()` and `recover()` are bounded blocking lifecycle calls when they issue init sequences or clear GDDRAM synchronously. Do not call them nonblocking.
- Hardware validation claims must name exact panel/module, MCU, framework, bus speed, reset wiring, command coverage, failure scenarios, and soak duration. Do not claim field-grade without representative hardware matrix results.

## SSD1315 subagent roles

- `ssd1315-spec-agent`: inspect datasheet/docs/current init sequence, verify SSD1315-specific commands, identify SSD1306-incompatible assumptions, and propose controller profile policy.
- `core-contracts-agent`: audit lifecycle, reset, probe, move/copy, panel dirty state, flush semantics, and transaction/latency contracts.
- `idf-ci-agent`: audit ESP-IDF component metadata, native IDF example, app-owned bus model, locking, tick scheduling, and CI jobs.
- `tests-fault-agent`: add or extend host fake-transport tests for init sequence, command/data control bytes, flush chunking, failure retention, probe mapping, panel dirty state, and reset/recover behavior.
- `docs-hw-agent`: update README/Doxygen/docs/hardware validation matrix with exact SSD1315 validation commands and honest compatibility wording.
- `integration-review-agent`: review final diff for framework leakage, accidental broad refactor, unsupported claims, stale docs, and missing tests.

Each subagent must report factual findings before implementation choices are finalized.

---

## Repository Model (Single Library)

This repository is a single reusable SSD1315 display library.

### Folder Structure (Mandatory)

```
include/ssd1315/     - Public API headers ONLY (Doxygen documented)
  ├── Status.h       - Error types and diagnostics
  ├── Config.h       - Configuration, transport, and panel profiles
  └── SSD1315.h      - Main display driver class
src/                 - Implementation (.cpp files)
examples/
  ├── 01_basic_bringup_cli/
  ├── espidf_basic/
  └── common/        - Example-only helpers and adapters
platformio.ini       - Build environments (uses build_src_filter)
library.json         - PlatformIO metadata
README.md            - Full documentation
CHANGELOG.md         - Keep a Changelog format
AGENTS.md            - This file
```

**Rules:**
- Public headers go in `include/ssd1315/` and define the API contract.
- Board-specific pins, buses, reset GPIOs, and locks never belong in core code.
- Examples demonstrate usage and may use helpers under `examples/common/`.
- Keep the layout boring and predictable.

Framework-boundary rules:
- Core/public headers and `src/` must remain framework-neutral. Do not include Arduino or ESP-IDF headers there unless the exception is documented in Doxygen and this file.
- Arduino examples may use Arduino APIs.
- ESP-IDF examples must be native IDF examples using `app_main`, `driver/i2c_master.h`, native GPIO/timer/task APIs, and fixed C buffers or `esp_console`/argtable.
- ESP-IDF examples must not include Arduino CLI sources or use `ArduinoCompat`, `IdfArduinoCompat`, `Arduino.h`, `Wire.h`, `String`, `Serial`, `TwoWire`, or equivalent Arduino facades.
- SSD1315 display examples may use display-specific drawing helpers, but bus/timer/GPIO glue in ESP-IDF examples must remain native IDF.
- Keep command parity through repo-local command contracts/checkers, not by compiling Arduino sources into ESP-IDF examples.

---

## Core Architecture Principles (Non-Negotiable)

### 0) Scope and Simplification First
- Prefer simplicity, clarity, correctness, robustness, safety, and readability over clever abstractions or speculative flexibility.
- Before coding, inspect whether existing code can be simplified, reused, or deleted.
- Prefer deleting unnecessary code over adding new code.
- Keep changes tightly scoped to the user's request and the current module boundary.
- Prefer extending existing owners, modules, APIs, and contracts over creating parallel abstractions.
- Before adding a service, class, file, interface, or abstraction, check whether an existing owner/module is the correct home.
- Add abstractions only for a concrete current need with a clear caller or test.
- Do not add placeholder classes, future stubs, empty managers, broad frameworks, plugin systems, service registries, or speculative extension points.
- Preserve dirty user changes. Never revert unrelated work unless the user explicitly asks for that revert.

### 1) Deterministic Behavior Over Convenience
- Predictable execution time
- No unbounded waits, retries, loops, allocations, queues, or buffers in steady paths
- All timeouts implemented via deadline checking (**not** `delay()`)
- State machines preferred over "clever" event-driven code
- Every hardware operation that can block must have a timeout and an observable failure path.
- Recovery logic must be bounded, deterministic, and testable.
- Do not hide hardware failures behind silent retries or fake success.

### 2) Cooperative Runtime, Bounded Lifecycle

SSD1315 exposes:

```cpp
Status begin(const Config& config);  // Bounded blocking init
void tick(uint32_t nowMs);           // Cooperative update (non-blocking)
void end();                          // Cleanup
```

- `tick()` returns immediately after bounded flush/power/sleep work
- Normal framebuffer I/O is split into state machine steps
- `begin()` and `recover()` may synchronously issue the SSD1315 init sequence
  and optional GDDRAM clear; their transaction counts and timeout bounds must
  stay documented in README/Doxygen.
- Example runtime timeout: 120-second timeout -> check `nowMs >= deadlineMs`
  each tick

> **Rule:** steady-state display I/O that could exceed ~1-2 ms must be chunked
> and progressed across `tick()` calls. Lifecycle calls may be bounded blocking
> only when the public contract documents the transaction and timeout budget.

### 3) Explicit Configuration (No Hidden Globals)
- Hardware resources passed via `Config`
- No hardcoded pins or interfaces in library code
- Libraries are board-agnostic by design
- Prefer explicit state, explicit ownership, and small local helpers over hidden global state.

### 4) No Repeated Heap Allocations in Steady State
- Allocate resources in `begin()` if needed
- **Zero** allocations in `tick()` and normal operation (no `String`, no `std::vector`, no `new`)
- Use fixed-size buffers, ring buffers, or user-supplied buffers
- Avoid dynamic allocation in steady embedded paths unless it is already an accepted local pattern and the bound is clear.

### 5) Boring, Predictable Code
- Prefer verbose over clever
- Explicit state machines over callback chains
- Simple control flow over complex abstractions
- If uncertain, choose the simplest deterministic solution
- Prefer small local helpers over broad generic layers.

---

## Shared-Bus / Transport Abstraction (Mandatory)

For libraries that talk to a shared bus (I2C/SPI/UART):

- The I2C bus must have one clear owner.
- The library MUST NOT own the bus.
- Device drivers must not directly own or reconfigure a shared bus unless this repository's architecture explicitly says so.
- The library MUST accept a transport adapter via `Config` (function pointers or an abstract interface).
- The library MUST NOT call `delay()` to "wait for the bus".
- The library MUST translate transport errors into `Status` (no leaking `Wire`, `esp_err_t`, etc.).
- The library must be transport-injected and non-owning. Application transport owns bus handles, reset pins, locks, and timeout policy.
- Transport callbacks must not recursively call into the same driver instance.
- Keep chip-level protocol code inside the driver/wrapper. Keep application policy outside the chip driver.
- Do not add fake devices, simulated buses, or test doubles to production paths.

### I2C Transaction Rules (Driver Quality)
- Bounded work per `tick()` (byte budget).
- Explicit timeouts via deadlines (software) plus the platform's hardware timeout if available.
- I2C transactions must be timeout-bounded and report errors clearly.
- Retries are allowed but MUST be bounded and use backoff (e.g., 1ms, 2ms, 4ms capped).
- Never assume I2C writes are atomic; handle partial progress in a state machine.
- Always support "bus busy" / "NACK" failures as normal operational errors (not asserts).
- Do not hide I2C failures behind silent retries or success statuses.
- Do not implement chip protocols manually outside the existing hardened driver/wrapper when it already provides the needed timeout, recovery, and testability behavior.

---

## Display Driver Guidance (SSD1315-class OLED, I2C)

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

**Automatic generation:** [scripts/generate_version.py](scripts/generate_version.py) creates `include/ssd1315/Version.h` before each build.

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
- **Exception:** generated `include/ssd1315/Version.h` may define
  `SSD1315_*` compile-time override macros for package/build metadata. The
  supported public API remains the namespace `static constexpr` values; do not
  introduce new hand-written constant macros elsewhere.

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
