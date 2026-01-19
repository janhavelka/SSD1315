# Post-Implementation Audit Report
## SSD1315 Managed Synchronous Driver Upgrade

**Date:** 2026-01-18  
**Auditor:** GitHub Copilot  
**Branch:** `nonblocking-manager`  
**Build Status:** ✅ All environments passing

---

## 1. Summary (What Changed)

### Implemented Features

- **DriverState health indicator** — 4-state enum (`UNINIT`, `READY`, `DEGRADED`, `OFFLINE`) tracking I2C transaction outcomes
- **Health counters** — `_consecutiveFailures`, `_totalFailures`, `_totalSuccess`, timestamps (`_lastOkMs`, `_lastErrorMs`)
- **Centralized tracking** — All I2C results flow through `_updateHealth()` with consistent semantics
- **`probe()` API** — Diagnostic-only device presence check (no health tracking)
- **`recover()` API** — Manual recovery mechanism for OFFLINE/DEGRADED states
- **`offlineThreshold` config** — Configurable failure threshold before OFFLINE (default: 3, min: 1)
- **Health getters** — `state()`, `isOnline()`, `lastOkMs()`, `lastErrorMs()`, `lastError()`, `consecutiveFailures()`, `totalFailures()`, `totalSuccess()`
- **New error codes** — `DEVICE_NOT_FOUND`, `IN_PROGRESS` added to `Err` enum
- **Flush tracking** — Per-flush-attempt health tracking (not per-chunk)

### What Was NOT Changed

| Aspect | Status |
|--------|--------|
| Async flush state machine | **Unchanged** — states IDLE/SET_ADDR/SEND_DATA/DONE/ERROR preserved |
| Page buffer iteration | **Unchanged** — `firstPage()`/`nextPage()` flow preserved |
| Blocking nature of command APIs | **Unchanged** — all `sendCommand*`, `setContrast()`, etc. remain blocking |
| No auto-recovery in tick | **Unchanged** — `tick()` does not call `recover()` automatically |
| No background health checks | **Unchanged** — state only changes on actual I2C operations |
| Drawing primitives | **Unchanged** — all drawing APIs unmodified |
| Power-on timing guard | **Unchanged** — `PowerState` FSM preserved |
| Auto-sleep/page cycling | **Unchanged** |

---

## 2. Files & Symbols Touched

| File | New/Modified Symbols | Notes |
|------|----------------------|-------|
| `include/ssd1315/Status.h` | `DriverState` (enum), `Err::DEVICE_NOT_FOUND`, `Err::IN_PROGRESS` | New enum and error codes added at end of file |
| `include/ssd1315/Config.h` | `Config::offlineThreshold` | New field, default 3 |
| `include/ssd1315/Ssd1315.h` | `probe()`, `recover()`, `state()`, `isOnline()`, `lastOkMs()`, `lastErrorMs()`, `lastError()`, `consecutiveFailures()`, `totalFailures()`, `totalSuccess()` | New public API |
| `include/ssd1315/Ssd1315.h` | `_updateHealth()`, `_i2cWriteRaw()`, `_i2cWriteTracked()`, `_applyConfig()` | New private helpers |
| `include/ssd1315/Ssd1315.h` | `_driverState`, `_lastOkMs`, `_lastErrorMs`, `_consecutiveFailures`, `_totalFailures`, `_totalSuccess`, `_flushError` | New private fields |
| `src/Ssd1315.cpp` | `begin()` | **Modified** — health reset, probe, state transitions |
| `src/Ssd1315.cpp` | `end()` | **Modified** — sets `_driverState = UNINIT`, preserves counters |
| `src/Ssd1315.cpp` | `sendCommand()`, `sendCommand2()`, `sendCommand3()`, `sendCommandList()` | **Modified** — now use `_i2cWriteTracked()` |
| `src/Ssd1315.cpp` | `sendData()` | **Modified** — now uses `_i2cWriteRaw()` (flush-only) |
| `src/Ssd1315.cpp` | `clearGddram()` | **Modified** — now uses `_i2cWriteTracked()` |
| `src/Ssd1315.cpp` | `tickFlush()` | **Modified** — uses raw writes, tracks health once at DONE/ERROR |

### Removed/Renamed Symbols

| Symbol | Change |
|--------|--------|
| Duplicate `lastError()` at line 759 | **Removed** — consolidated with new health getter |

---

## 3. DriverState & Health Tracking: Behavioral Contract

### DriverState Values

| State | Meaning | When Set |
|-------|---------|----------|
| `UNINIT` | Driver not initialized or init in progress | Default, after `end()`, or during `begin()` before first I2C result |
| `READY` | Last I2C transaction succeeded | After any successful tracked I2C op (from any state) |
| `DEGRADED` | 1 to (threshold-1) consecutive failures | First failure from READY or UNINIT |
| `OFFLINE` | ≥threshold consecutive failures | When `_consecutiveFailures >= offlineThreshold` |

### State Transition Rules (As Implemented)

```
_updateHealth() in Ssd1315.cpp lines 142-180:
```

| From | Event | To | Condition |
|------|-------|-----|-----------|
| UNINIT | I2C success | READY | `_initialized == true` |
| UNINIT | I2C failure (first) | DEGRADED | `_initialized == true` |
| UNINIT | I2C failure (threshold) | OFFLINE | `_initialized == true && _consecutiveFailures >= threshold` |
| READY | I2C success | READY | (no change) |
| READY | I2C failure (first) | DEGRADED | `_consecutiveFailures == 1` |
| DEGRADED | I2C success | READY | Always |
| DEGRADED | I2C failure | DEGRADED | `_consecutiveFailures < threshold` |
| DEGRADED | I2C failure | OFFLINE | `_consecutiveFailures >= threshold` |
| OFFLINE | I2C success | READY | **Auto-recovery** |
| OFFLINE | I2C failure | OFFLINE | (no change) |
| Any | `end()` called | UNINIT | Forced directly |

### Critical Invariants

1. **`_initialized == false` ⇒ `_driverState == UNINIT`** (always enforced)
2. **State transitions only occur when `_initialized == true`** (line 161)
3. **Counters update regardless of `_initialized`** (lines 148-159)

### Success Classification in `_updateHealth()`

```cpp
// Line 144-145 in Ssd1315.cpp
bool isSuccess = st.ok() || st.code == Err::IN_PROGRESS;
```

| Status | Classification |
|--------|----------------|
| `Err::OK` | SUCCESS |
| `Err::IN_PROGRESS` | SUCCESS (pattern consistency) |
| All other codes | FAILURE |

---

## 4. begin()/end()/recover()/probe(): Detailed Behavior Checks

### begin(config)

**Location:** `Ssd1315.cpp` lines 268-401

| Checklist Item | Status | Code Reference |
|----------------|--------|----------------|
| Config validation errors do NOT call `_updateHealth()` | ✅ | Lines 291-329 return directly |
| Health counters reset in begin() | ✅ | Lines 277-284 |
| `offlineThreshold` clamped to min 1 | ✅ | Lines 287-289 |
| `probe()` called before `_initialized = true` | ✅ | Line 332 (probe), line 369 (`_initialized = true`) |
| `_initialized` set true BEFORE `_applyConfig()` | ✅ | Line 369 before line 377 |
| On `_applyConfig()` failure: rollback `_initialized` | ✅ | Line 385 |
| On `_applyConfig()` failure: force `_driverState = UNINIT` | ✅ | Line 386 |
| On `_applyConfig()` failure: counters remain updated | ✅ | No reset in failure path |
| On success: `_driverState` is READY | ✅ | Via `_updateHealth()` in tracked I2C ops |
| No double-tracking of final success | ✅ | `_applyConfig()` returns `Ok()`, not a new tracking call |

**Probe failure tracking (lines 332-338):**
```cpp
Status st = probe();
if (!st.ok()) {
  // Probe failed before init; track failure but stay UNINIT
  _updateHealth(st);  // Counters update, state stays UNINIT (_initialized still false)
  return st;
}
```

### end()

**Location:** `Ssd1315.cpp` lines 418-437

| Checklist Item | Status | Code Reference |
|----------------|--------|----------------|
| Does NOT reset counters/timestamps | ✅ | Lines 435-436 comment, no reset code |
| Forces `_initialized = false` | ✅ | Line 431 |
| Forces `_driverState = UNINIT` | ✅ | Line 432 |
| DISPLAY_OFF uses tracked wrapper | ✅ | Line 423 `sendCommand()` → `_i2cWriteTracked()` |

### probe()

**Location:** `Ssd1315.cpp` lines 221-237

| Checklist Item | Status | Code Reference |
|----------------|--------|----------------|
| Uses RAW I2C path | ✅ | Line 230 `_i2cWriteRaw()` |
| Does NOT touch counters/state | ✅ | No `_updateHealth()` call |
| Maps I2C errors to `DEVICE_NOT_FOUND` | ✅ | Lines 232-234 |

**Error mapping logic:**
```cpp
if (!st.ok() && (st.code == Err::I2C_NACK_ADDR || st.code == Err::I2C_NACK_DATA ||
                 st.code == Err::I2C_TIMEOUT || st.code == Err::TIMEOUT)) {
  return Error(Err::DEVICE_NOT_FOUND, st.detail, "Device not responding");
}
```

**⚠️ Gap identified:** Other I2C errors (e.g., `I2C_BUS_ERROR`) pass through unchanged, not mapped to `DEVICE_NOT_FOUND`.

### recover()

**Location:** `Ssd1315.cpp` lines 239-262

| Checklist Item | Status | Code Reference |
|----------------|--------|----------------|
| Requires `_initialized == true` | ✅ | Lines 241-243 |
| Calls `probe()` first (diagnostic) | ✅ | Line 246 |
| Explicitly calls `_updateHealth()` on probe failure | ✅ | Line 249 |
| Calls `_applyConfig()` without double-tracking | ✅ | Line 254, no extra `_updateHealth()` |
| On success: marks framebuffer dirty | ✅ | Line 258 `markAllDirty()` |

**Note:** `recover()` does NOT call `requestFlush()` — only marks dirty. Application must request flush.

---

## 5. I2C Wrappers & Tracking Rules

### Wrapper Implementations

| Wrapper | Implementation | Tracking |
|---------|----------------|----------|
| `_i2cWriteRaw()` | Direct call to `_config.i2cWrite()` | NO |
| `_i2cWriteTracked()` | `_i2cWriteRaw()` + `_updateHealth()` | YES |

### Usage Table

| Call Site | Wrapper Used | Tracked? | Health Granularity |
|-----------|--------------|----------|-------------------|
| `probe()` | `_i2cWriteRaw()` | ❌ | N/A (diagnostic) |
| `sendCommand()` | `_i2cWriteTracked()` | ✅ | Per transaction |
| `sendCommand2()` | `_i2cWriteTracked()` | ✅ | Per transaction |
| `sendCommand3()` | `_i2cWriteTracked()` | ✅ | Per transaction |
| `sendCommandList()` | `_i2cWriteTracked()` (loop) | ✅ | Per chunk (N transactions) |
| `sendData()` | `_i2cWriteRaw()` (loop) | ❌ | N/A (flush path) |
| `clearGddram()` | `_i2cWriteTracked()` (loop) | ✅ | Per chunk |
| `tickFlush() SET_ADDR` | `_i2cWriteRaw()` | ❌ | N/A (flush path) |
| `tickFlush() SEND_DATA` | `sendData()` → `_i2cWriteRaw()` | ❌ | N/A (flush path) |
| `tickFlush() DONE` | `_updateHealth(Ok())` | ✅ | Once per flush |
| `tickFlush() ERROR` | `_updateHealth(_flushError)` | ✅ | Once per flush |

### Public API Tracking Granularity

| API | Tracking |
|-----|----------|
| `setContrast()` | 1 health update (via `sendCommand2`) |
| `setInvert()` | 1 health update (via `sendCommand`) |
| `setFlipX()` | 1 health update (via `sendCommand`) |
| `setFlipY()` | 1 health update (via `sendCommand`) |
| `setSleep()` | 1 health update (via `sendCommand`) |
| `setAllPixelsOn()` | 1 health update (via `sendCommand`) |
| `requestFlush()` | 0 immediate; 1 at completion via `tick()` |

---

## 6. Flush FSM: "One Logical Operation" Tracking

### Flush State Machine

```
                    requestFlush()
                         │
                         ▼
    ┌───────────────────────────────────────┐
    │                 IDLE                  │
    └───────────────────────────────────────┘
                         │
                         ▼
    ┌───────────────────────────────────────┐
    │              SET_ADDR                 │◀─────────┐
    │  _i2cWriteRaw() for column/page addr  │          │
    └───────────────────────────────────────┘          │
          │ success              │ fail               │
          ▼                      ▼                    │
    ┌────────────┐        ┌────────────┐              │
    │ SEND_DATA  │        │   ERROR    │              │
    │ _i2cWriteRaw│        │ accumulate │              │
    │  per chunk  │        │ _flushError│              │
    └────────────┘        └────────────┘              │
          │                      │                    │
          │ page done            │                    │
          ▼                      │                    │
    ┌────────────┐               │                    │
    │ next page? │───yes─────────┼────────────────────┘
    └────────────┘               │
          │ no                   │
          ▼                      │
    ┌────────────┐               │
    │    DONE    │               │
    └────────────┘               │
          │                      │
          ▼                      ▼
    ┌───────────────────────────────────────┐
    │            tickFlush() entry          │
    │  DONE: _updateHealth(Ok())            │
    │  ERROR: _updateHealth(_flushError)    │
    │  then → IDLE                          │
    └───────────────────────────────────────┘
```

### Verification Points

| Point | Status | Code Reference |
|-------|--------|----------------|
| Flush uses RAW writes per chunk | ✅ | `sendData()` line 620 uses `_i2cWriteRaw()` |
| SET_ADDR uses RAW writes | ✅ | Lines 815-831 use `_i2cWriteRaw()` |
| First failure accumulated to `_flushError` | ✅ | Lines 820, 828, 858 |
| `_updateHealth()` called exactly once at DONE | ✅ | Line 768 |
| `_updateHealth()` called exactly once at ERROR | ✅ | Line 775 |
| Byte budgeting unchanged | ✅ | Lines 841-845 |
| Timeout handling unchanged | ✅ | Lines 796-803 |

### Flush Error Accumulation

```cpp
// Line 792 - reset at flush start
_flushError = Ok();

// Lines 820, 828, 858 - accumulate first error
_flushError = st;
_lastError = st;
_flushState = FlushState::ERROR;
```

---

## 7. Counter Semantics & Invariants

### Health Fields

| Field | Type | Reset in begin() | Updated when |
|-------|------|------------------|--------------|
| `_lastOkMs` | `uint32_t` | ✅ → 0 | On success |
| `_lastErrorMs` | `uint32_t` | ✅ → 0 | On failure |
| `_lastError` | `Status` | ✅ → `Ok()` | On failure |
| `_consecutiveFailures` | `uint8_t` | ✅ → 0 | +1 on fail, reset on success |
| `_totalFailures` | `uint32_t` | ✅ → 0 | +1 on fail |
| `_totalSuccess` | `uint32_t` | ✅ → 0 | +1 on success |
| `_flushError` | `Status` | ✅ → `Ok()` | On flush failure |

### Counter Update Rules

From `_updateHealth()` lines 147-159:

```cpp
// Health COUNTERS are always updated (regardless of _initialized)
if (isSuccess) {
  _lastOkMs = millis();
  _consecutiveFailures = 0;
  _totalSuccess++;
} else {
  _lastError = st;
  _lastErrorMs = millis();
  _consecutiveFailures++;
  _totalFailures++;
}
```

### Key Invariant Confirmations

| Invariant | Confirmed |
|-----------|-----------|
| Counters update even when `_initialized == false` | ✅ Lines 147-159 unconditional |
| State transitions only when `_initialized == true` | ✅ Line 161 guard |
| `probe()` standalone does NOT update counters | ✅ No `_updateHealth()` in `probe()` |
| `probe()` inside `recover()` DOES update counters | ✅ Line 249 explicitly calls `_updateHealth(st)` |

### Counter Behavior During Init

When `begin()` calls `probe()` and it fails (line 335):
- `_initialized` is still `false`
- `_updateHealth(st)` is called
- Counters update: `_totalFailures++`, `_lastError = st`, etc.
- State stays `UNINIT` (guard at line 161)

---

## 8. Error Mapping & Status Codes

### Transport Error Propagation

| Source | Raw Status | Returned Status | Notes |
|--------|------------|-----------------|-------|
| `probe()` success | `Ok()` | `Ok()` | Pass-through |
| `probe()` NACK addr | `I2C_NACK_ADDR` | `DEVICE_NOT_FOUND` | Explicit mapping |
| `probe()` NACK data | `I2C_NACK_DATA` | `DEVICE_NOT_FOUND` | Explicit mapping |
| `probe()` timeout | `I2C_TIMEOUT` or `TIMEOUT` | `DEVICE_NOT_FOUND` | Explicit mapping |
| `probe()` bus error | `I2C_BUS_ERROR` | `I2C_BUS_ERROR` | **Pass-through** (not mapped) |
| `sendCommand*` any | any | any | Pass-through after tracking |
| Flush timeout | N/A | `TIMEOUT` | Generated internally |
| Flush I2C error | any | any | Accumulated to `_flushError` |

### Error Codes Added

| Code | Value | Purpose |
|------|-------|---------|
| `DEVICE_NOT_FOUND` | (new) | Probe failure mapping |
| `IN_PROGRESS` | (new) | Treated as success for pattern consistency |

### Health Tracking Error Classification

```cpp
// _updateHealth() line 144-145
bool isSuccess = st.ok() || st.code == Err::IN_PROGRESS;
```

This means:
- `IN_PROGRESS` resets `_consecutiveFailures` and increments `_totalSuccess`
- Any other non-OK status increments failure counters

---

## 9. Risk Review (Edge Cases + Regressions)

### Edge Case Analysis

| Scenario | Expected Behavior | Implementation Status |
|----------|-------------------|----------------------|
| `offlineThreshold = 1` | First failure → OFFLINE immediately | ✅ Line 176: `_consecutiveFailures >= _config.offlineThreshold` |
| First tracked I2C failure during init | UNINIT → DEGRADED (or OFFLINE if threshold=1) | ✅ Lines 169-172 check `_driverState == UNINIT` |
| Init succeeds, later flush fails repeatedly | READY → DEGRADED → OFFLINE (one update per flush) | ✅ Flush tracks once at DONE/ERROR |
| Device disappears mid-flush | Partial transfer, flush → ERROR, _updateHealth once | ✅ Error accumulated, tracked at ERROR state |
| Device returns spontaneously | Any I2C success from OFFLINE → READY | ✅ Line 164: `if (_driverState != DriverState::READY)` |
| Calling public APIs while OFFLINE | Still attempts I2C, may recover | ✅ No OFFLINE guard on `sendCommand*` |
| Calling `probe()` repeatedly | Counters unchanged, state unchanged | ✅ No `_updateHealth()` in `probe()` |
| Calling `end()` while OFFLINE | DISPLAY_OFF sent (may fail), state → UNINIT | ✅ Lines 423, 432 |
| Calling `recover()` repeatedly with device absent | Counters climb, stays OFFLINE | ✅ Each probe fail + `_applyConfig` tracked |
| Calling `recover()` while UNINIT | Returns `NOT_INITIALIZED` | ✅ Line 241-243 |

### Regression Risks

| Risk | Mitigation |
|------|------------|
| Flush now uses raw writes; setAddressWindow unused | ✅ `setAddressWindow()` still exists for non-flush use |
| `sendData()` changed to raw | ✅ Only used by flush; intentional |
| `clearGddram()` now tracked | ✅ Correct — part of init, should update health |
| Double `_lastError` field? | ✅ No — duplicate getter removed, field still `_lastError` |

### Potential Issues Identified

1. **⚠️ `probe()` bus error pass-through**: `I2C_BUS_ERROR` from probe is NOT mapped to `DEVICE_NOT_FOUND`
2. **⚠️ `end()` tracking during shutdown**: DISPLAY_OFF is tracked even though we immediately set `_initialized = false`. This updates counters but state transition is moot.
3. **⚠️ `_flushError` not reset on new requestFlush()**: Reset happens in `tickFlush()` when `_flushStartMs == 0`, which is correct, but relies on `_flushStartMs` being reset in `requestFlush()`.

---

## 10. Test Plan

### Unit Tests (Proposed)

| Test | Description |
|------|-------------|
| `test_DriverState_initial` | After default construction: `state() == UNINIT`, all counters 0 |
| `test_begin_success` | After `begin()` with valid config: `state() == READY`, `totalSuccess() > 0` |
| `test_begin_probe_fail` | Mock NACK: `begin()` fails, `state() == UNINIT`, `totalFailures() > 0` |
| `test_begin_init_fail` | Mock first I2C success, then fail: `state() == UNINIT` (rolled back) |
| `test_threshold_1` | With `offlineThreshold=1`: first fail → OFFLINE |
| `test_ready_to_degraded` | After `begin()`, mock fail: READY → DEGRADED |
| `test_degraded_to_offline` | threshold=3, 3 failures: DEGRADED → OFFLINE |
| `test_offline_auto_recovery` | In OFFLINE, mock success: OFFLINE → READY |
| `test_probe_no_tracking` | Multiple `probe()` calls: counters unchanged |
| `test_recover_success` | In OFFLINE, `recover()` with mocked success: → READY |
| `test_recover_fail` | In OFFLINE, `recover()` with mocked fail: stays OFFLINE, counters++ |
| `test_flush_single_tracking` | Flush 8 pages: `totalSuccess` increments by 1 (not 8) |
| `test_flush_fail_single_tracking` | Flush fails mid-page: `totalFailures` increments by 1 |
| `test_end_preserves_counters` | After `end()`: counters preserved, `state() == UNINIT` |

### Manual Test Steps

#### Setup
```cpp
#include <Wire.h>
#include <ssd1315/Ssd1315.h>

ssd1315::Ssd1315 display;
ssd1315::Config config;

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  
  config.width = 128;
  config.height = 64;
  config.i2cAddress = 0x3C;
  config.i2cWrite = myI2cWrite;
  config.offlineThreshold = 3;
}

void printHealth() {
  Serial.printf("State: %d, Online: %d, ConsecFail: %d, TotalFail: %lu, TotalOK: %lu\n",
    (int)display.state(),
    display.isOnline(),
    display.consecutiveFailures(),
    display.totalFailures(),
    display.totalSuccess()
  );
  Serial.printf("LastOkMs: %lu, LastErrMs: %lu\n",
    display.lastOkMs(), display.lastErrorMs());
}
```

#### Test Scenarios

**Scenario 1: Normal Operation**
1. `begin()` with display connected
2. Call `printHealth()` — expect READY, success count > 0
3. Draw and flush
4. Call `printHealth()` — expect success count increased by 1

**Scenario 2: Device Disconnected**
1. `begin()` with display connected → READY
2. Physically disconnect display
3. Call `setContrast(128)` 3 times
4. Call `printHealth()` — expect OFFLINE, `consecutiveFailures() == 3`

**Scenario 3: Auto-Recovery**
1. Reach OFFLINE state (as above)
2. Reconnect display
3. Call `setContrast(128)` — should succeed
4. Call `printHealth()` — expect READY, `consecutiveFailures() == 0`

**Scenario 4: Flush Counts as One Failure**
1. `begin()` with display connected
2. Note `totalFailures()`
3. Disconnect display
4. Call `requestFlush()` + run `tick()` until flush completes
5. Call `printHealth()` — expect `totalFailures` increased by exactly 1

**Scenario 5: Probe Independence**
1. Record `totalSuccess()` and `totalFailures()`
2. Call `probe()` 10 times
3. Verify counters unchanged

**Scenario 6: Recover Flow**
1. Reach OFFLINE
2. Call `recover()` with device disconnected — should fail, counters++
3. Reconnect display
4. Call `recover()` — should succeed, state → READY

---

## 11. Open Questions / TODOs

### Known Limitations

1. **No chip identity verification** — SSD1315 has no WHOAMI register; `probe()` only confirms ACK
2. **No bus-level recovery** — Driver does not attempt SDA/SCL unsticking
3. **No rate limiting** — Application must implement retry backoff

### Implementation TODOs

| Item | Severity | Notes |
|------|----------|-------|
| `probe()` I2C_BUS_ERROR not mapped | Low | Could map to DEVICE_NOT_FOUND for consistency |
| No example demonstrating health API | Medium | Should add to examples/ |
| `_flushError` initialization timing | Low | Currently correct but relies on `_flushStartMs` |

### Documentation TODOs

| Item | Status |
|------|--------|
| Update README with health API | ❌ Not done |
| Update CHANGELOG | ❌ Not done |
| Add Doxygen for new public APIs | ✅ Done in header |

### Code Quality Notes

- All new code follows AGENTS.md naming conventions (`_camelCase` members, `camelCase` methods)
- No heap allocations in steady state (counters are stack/member variables)
- All new errors use static string literals

---

## Appendix: Key Code Excerpts

### _updateHealth() Core Logic

```cpp
Status Ssd1315::_updateHealth(const Status& st) {
  bool isSuccess = st.ok() || st.code == Err::IN_PROGRESS;

  // Counters always update
  if (isSuccess) {
    _lastOkMs = millis();
    _consecutiveFailures = 0;
    _totalSuccess++;
  } else {
    _lastError = st;
    _lastErrorMs = millis();
    _consecutiveFailures++;
    _totalFailures++;
  }

  // State transitions only when _initialized
  if (_initialized) {
    if (isSuccess) {
      if (_driverState != DriverState::READY) {
        _driverState = DriverState::READY;
      }
    } else {
      if (_consecutiveFailures == 1 &&
          (_driverState == DriverState::READY || _driverState == DriverState::UNINIT)) {
        _driverState = DriverState::DEGRADED;
      }
      if (_consecutiveFailures >= _config.offlineThreshold) {
        _driverState = DriverState::OFFLINE;
      }
    }
  }
  return st;
}
```

### Flush Health Tracking Entry

```cpp
void Ssd1315::tickFlush(uint32_t nowMs) {
  if (_flushState == FlushState::DONE) {
    _updateHealth(Ok());  // Track once
    _flushState = FlushState::IDLE;
    return;
  }

  if (_flushState == FlushState::ERROR) {
    _updateHealth(_flushError);  // Track once
    _flushState = FlushState::IDLE;
    return;
  }
  // ... rest of FSM
}
```

---

**End of Audit Report**
