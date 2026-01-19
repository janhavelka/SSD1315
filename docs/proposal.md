# Implementation Proposal: Managed Synchronous Device Driver Upgrade

## 1. Overview

This proposal upgrades the SSD1315 library from a "register wrapper with async flush" to a "managed synchronous device driver" with:
- Lightweight **health indicator** tracking (4 states)
- Health/diagnostic fields for failure tracking
- Device presence probing
- Blocking recovery mechanism

**Pattern:** Managed Synchronous Driver  
**Blocking:** Yes (all command and configuration APIs are blocking)  
**Health Tracking:** Centralized via `_updateHealth()`; per-transaction for commands, per-attempt for flush  
**Recovery:** Manual only (no automatic recovery in tick)  
**Device-Specific Async:** Flush and page iteration via explicit state machines (`tick()`)

**Key constraint:** Preserve all existing behavior, especially the async flush state machine.

### 1.1 Synchronous vs Device-Specific Async

All **direct I2C command/configuration calls are blocking**. Some device-specific workflows remain asynchronous by design and are driven cooperatively via `tick()`. These include:

- **Display flush** — Transfers framebuffer to GDDRAM in chunks
- **Page iteration** — Cycles through display pages over time

These async procedures:
- Do NOT introduce async I2C queues
- Do NOT imply a lifecycle FSM
- Do NOT perform automatic recovery
- Do NOT change the "managed synchronous driver" classification

Health tracking and OFFLINE detection remain synchronous and transaction-based. Recovery is always manual.

### 1.2 Design Philosophy

**DriverState is a health indicator, NOT a lifecycle state machine.**

This is a critical distinction:
- **Lifecycle FSM:** States represent operational phases (PROBING → INIT → READY → RECOVERING)
- **Health indicator:** States represent the result of recent I2C operations (READY / DEGRADED / OFFLINE)

This driver uses the **health indicator** model because:
1. All I2C operations remain synchronous and blocking
2. No background tasks or tick-driven state transitions exist
3. State changes occur ONLY as a direct result of I2C transaction outcomes
4. The application controls all retry and recovery timing

**Implication:** A device that stops responding will remain in OFFLINE until the application either:
- Calls `recover()` explicitly, OR
- Attempts another I2C operation that happens to succeed (automatic promotion to READY)

### 1.3 Dual Tracking: `_initialized` vs `DriverState`

| Field | Purpose | Values |
|-------|---------|--------|
| `_initialized` (bool) | Guards API calls | true/false |
| `_driverState` (enum) | Reflects device health | UNINIT/READY/DEGRADED/OFFLINE |

**Consistency Rules**:
- `_initialized == false` → `_driverState == UNINIT` (always)
- `_initialized == true` → `_driverState ∈ {UNINIT, READY, DEGRADED, OFFLINE}`
  - UNINIT is transient during init (between setting `_initialized=true` and first tracked I2C result)
  - First tracked I2C success during init → READY
  - First tracked I2C failure during init → DEGRADED (or OFFLINE if threshold is 1)
  - After successful `begin()`, state is READY
  - Normal operation: READY, DEGRADED, or OFFLINE

---

## 2. New Types and Enums

### 2.1 DriverState Enum

**Location:** `include/ssd1315/Status.h`

```cpp
/**
 * @brief Driver health indicator (NOT a lifecycle state machine).
 *
 * DriverState reflects the outcome of recent I2C transactions. It is NOT
 * a lifecycle FSM - there are no time-based transitions, no background
 * checks, and no automatic state changes without actual I2C activity.
 *
 * State changes occur ONLY when:
 * - An I2C transaction succeeds (→ READY)
 * - An I2C transaction fails (→ DEGRADED or OFFLINE based on threshold)
 * - begin() or end() is called (→ UNINIT or READY)
 *
 * IMPORTANT: A single successful I2C transaction from ANY state (including
 * OFFLINE) will automatically transition the driver to READY. This is
 * intentional - it allows spontaneous device recovery without requiring
 * an explicit recover() call.
 */
enum class DriverState : uint8_t {
  UNINIT,    ///< Driver not initialized or init in progress.
             ///< This is a structural state, not a health state.
             ///< During init (when _initialized==true): first I2C success → READY,
             ///< first I2C failure → DEGRADED (or OFFLINE if threshold is 1).
  
  READY,     ///< Last I2C transaction succeeded. Device is healthy.
             ///< This is the normal operating state.
  
  DEGRADED,  ///< 1 to (N-1) consecutive failures occurred.
             ///< Device may still respond - worth retrying.
             ///< Any success → READY. Nth failure → OFFLINE.
  
  OFFLINE    ///< N or more consecutive failures occurred.
             ///< Device assumed missing or unresponsive.
             ///< Application should call recover() or investigate.
             ///< NOTE: Any successful I2C op still → READY (auto-recovery).
};
```

**State Transition Rules:**

| From | Event | To |
|------|-------|-----|
| UNINIT | `begin()`: first tracked I2C succeeds | READY |
| UNINIT | `begin()`: first tracked I2C fails | DEGRADED |
| UNINIT | `begin()` failure (threshold reached) | OFFLINE |
| Any | `end()` | UNINIT |
| READY | I2C failure | DEGRADED |
| DEGRADED | I2C success | READY |
| DEGRADED | failures >= threshold | OFFLINE |
| OFFLINE | I2C success | READY |
| OFFLINE | `recover()` success | READY |

**Note:** "begin() success" means: probe succeeded, allocations succeeded, and `_applyConfig()` completed without error. The transition UNINIT → READY occurs via `_updateHealth()` on the first successful tracked I2C transaction inside `_applyConfig()`. If the first tracked I2C fails, state transitions to DEGRADED (or OFFLINE if `offlineThreshold == 1`).

### 2.2 Config Struct Addition

**Location:** `include/ssd1315/Config.h`

```cpp
struct Config {
  // ... existing fields ...
  
  /// Consecutive failure threshold before OFFLINE state.
  /// Default: 3. Clamped to minimum 1 in begin().
  uint8_t offlineThreshold = 3;
};
```

**Validation in begin():**

```cpp
if (_config.offlineThreshold < 1) {
  _config.offlineThreshold = 1;
}
```

---

## 3. Health Tracking Fields (Private)

**Location:** `include/ssd1315/Ssd1315.h` (private section)

**Note:** Flat members instead of nested struct for consistency with other drivers.

```cpp
private:
  // Driver state
  DriverState _driverState = DriverState::UNINIT;

  // Health counters
  uint32_t _lastOkMs = 0;              // Timestamp of last success (millis)
  uint32_t _lastErrorMs = 0;           // Timestamp of last error (millis)
  Status   _lastError = Status::Ok();  // Most recent error
  uint8_t  _consecutiveFailures = 0;   // Failures since last success
  uint32_t _totalFailures = 0;         // Lifetime failure count
  uint32_t _totalSuccess = 0;          // Lifetime success count
```

---

## 4. Public API — Diagnostics

### 4.1 `probe()`

```cpp
/**
 * @brief Check if device is present at configured I2C address.
 *
 * Sends a minimal I2C transaction to verify device responds with ACK.
 *
 * IMPORTANT LIMITATIONS:
 * - Does NOT initialize the device or change driver state
 * - Does NOT update health tracking (probe is diagnostic-only)
 * - Does NOT verify chip identity (SSD1315 has no WHOAMI register)
 * - ACK only confirms "something responds at this address"
 *
 * Can be called in ANY state (even UNINIT).
 * Useful for:
 * - Scanning for devices before init
 * - Checking if device is present without affecting health state
 *
 * @return Status Ok if device ACK'd, error otherwise.
 *         Returns DEVICE_NOT_FOUND on NACK or timeout.
 *
 * @pre i2cWrite callback must be configured.
 *
 * @note SSD1315 has no WHOAMI register. Probe sends a NOP command (0xE3)
 *       and checks for ACK. Does NOT call _updateHealth().
 */
Status probe();
```

**Key Properties:**
- Can be called in ANY state (even UNINIT)
- Does NOT modify `_driverState`
- Does NOT update health counters
- Uses `_i2cWriteRaw()` to bypass tracking

### 4.2 `recover()`

```cpp
/**
 * @brief Attempt to recover the device from OFFLINE or DEGRADED state.
 *
 * Blocking operation that:
 * 1. Probes device presence
 * 2. Re-sends full initialization sequence via _applyConfig()
 *
 * @return Status Ok on success, error on failure.
 *
 * @note On success: state → READY via _updateHealth().
 * @note On failure: state updated via _updateHealth().
 * @note Requires `_initialized == true`.
 */
Status recover();
```

**Implementation approach:**
1. If `!_initialized`: return `Error(NOT_INITIALIZED, "begin() not called")`
2. Call `probe()` — if fails: `_updateHealth(st)`, return error
3. Call `_applyConfig()` — health is tracked internally (do NOT call `_updateHealth()` again on result)
4. On success: mark framebuffer dirty for resync

---

## 5. Public API — Health Getters

| Function | Return Type | Description |
|----------|-------------|-------------|
| `state()` | `DriverState` | Current state (UNINIT/READY/DEGRADED/OFFLINE) |
| `isOnline()` | `bool` | `true` if READY or DEGRADED |
| `lastOkMs()` | `uint32_t` | Timestamp of last successful I2C op |
| `lastErrorMs()` | `uint32_t` | Timestamp of last failed I2C op |
| `lastError()` | `Status` | Most recent error status |
| `consecutiveFailures()` | `uint8_t` | Failures since last success |
| `totalFailures()` | `uint32_t` | Lifetime failure count |
| `totalSuccess()` | `uint32_t` | Lifetime success count |

```cpp
DriverState state() const { return _driverState; }

bool isOnline() const {
  return _driverState == DriverState::READY || 
         _driverState == DriverState::DEGRADED;
}

uint32_t lastOkMs() const { return _lastOkMs; }
uint32_t lastErrorMs() const { return _lastErrorMs; }
Status lastError() const { return _lastError; }
uint8_t consecutiveFailures() const { return _consecutiveFailures; }
uint32_t totalFailures() const { return _totalFailures; }
uint32_t totalSuccess() const { return _totalSuccess; }
```

---

## 6. Core Private Helpers

### 6.1 `_updateHealth(const Status& st)` — Central Health Tracker

**Purpose:** Single point for all health state updates.

```cpp
private:
  /**
   * @brief Central health state manager.
   *
   * All I2C transaction results flow through this function.
   * Updates counters and state based on success/failure.
   *
   * @param st The status from the I2C operation.
   * @return The same status (for chaining).
   */
  Status _updateHealth(const Status& st) {
    // Determine success: OK or IN_PROGRESS are both considered success
    // IN_PROGRESS is included for pattern consistency, even if SSD1315
    // does not currently return it from I2C operations
    bool isSuccess = st.ok() || st.code == Err::IN_PROGRESS;
    
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
    
    // DriverState transitions are ONLY allowed when _initialized == true
    // This preserves the invariant: _initialized == false ⇒ _driverState == UNINIT
    if (_initialized) {
      if (isSuccess) {
        // Any success → READY (from UNINIT, DEGRADED, or OFFLINE)
        if (_driverState != DriverState::READY) {
          _driverState = DriverState::READY;
        }
      } else {
        // Failure handling: UNINIT/READY → DEGRADED on first failure
        // Note: UNINIT can occur during init when _initialized is already true
        if (_consecutiveFailures == 1 && 
            (_driverState == DriverState::READY || _driverState == DriverState::UNINIT)) {
          _driverState = DriverState::DEGRADED;
        }
        // DEGRADED → OFFLINE when threshold reached
        if (_consecutiveFailures >= _config.offlineThreshold) {
          _driverState = DriverState::OFFLINE;
        }
      }
    }
    return st;
  }
```

**When to call `_updateHealth()`:**

| Scenario | Call `_updateHealth()`? |
|----------|------------------------|
| Real I2C transaction result | ✅ YES |
| Flush completion (DONE or ERROR) | ✅ YES (once per flush) |
| `INVALID_CONFIG` | ❌ NO |
| `INVALID_PARAM` | ❌ NO |
| `NOT_INITIALIZED` | ❌ NO |
| Standalone `probe()` result | ❌ NO (diagnostic only) |
| `probe()` failure inside `recover()` | ✅ YES (explicitly) |

**Success definition:**
- `Err::OK` → SUCCESS (resets `_consecutiveFailures`)
- `Err::IN_PROGRESS` → SUCCESS (for pattern consistency)
- `Err::I2C_ERROR`, `Err::TIMEOUT`, other `!st.ok()` → FAILURE

**State transition guard:**
- Health counters are updated regardless of `_initialized`
- DriverState transitions (READY/DEGRADED/OFFLINE) only occur when `_initialized == true`
- When `_initialized == false`, `_driverState` remains `UNINIT`

### 6.2 `_i2cWriteRaw()` — Raw I2C Write (No Tracking)

**Purpose:** Bypass health tracking for diagnostics.

```cpp
private:
  /**
   * @brief Raw I2C write without health tracking.
   *
   * Used by probe() and internal diagnostics.
   * Does NOT call _updateHealth().
   *
   * @param data Buffer to send
   * @param len Length of buffer
   * @return Status from transport
   */
  Status _i2cWriteRaw(const uint8_t* data, size_t len) {
    if (!_config.i2cWrite) {
      return Status::Error(Err::INVALID_CONFIG, "I2C write callback null");
    }
    return _config.i2cWrite(_config.i2cAddress, data, len,
                            _config.i2cTimeoutMs, _config.i2cUser);
  }
```

**Used by:** `probe()`

### 6.3 `_i2cWriteTracked()` — Tracked I2C Write

**Purpose:** Perform I2C write with automatic health tracking.

```cpp
private:
  /**
   * @brief Execute I2C write and update health tracking.
   *
   * All normal I2C operations should use this wrapper.
   *
   * @param data Buffer to send
   * @param len Length of buffer
   * @return Status from transport (after health update)
   */
  Status _i2cWriteTracked(const uint8_t* data, size_t len) {
    Status st = _i2cWriteRaw(data, len);
    return _updateHealth(st);
  }
```

**Used by:** `sendCommand()`, `sendCommand2()`, `sendCommand3()`, `sendCommandList()`

**Note on `sendData()`:** When called during flush, `sendData()` uses `_i2cWriteRaw()` instead. See Section 7.5.

### 6.4 Transaction Granularity for Non-Flush APIs

For command/config APIs, **each call to `_i2cWriteTracked()` counts as one health update**.

| Function | I2C Calls | Health Updates |
|----------|-----------|----------------|
| `sendCommand()` | 1 | 1 |
| `sendCommand2()` | 1 | 1 |
| `sendCommand3()` | 1 | 1 |
| `sendCommandList(N)` | N | N |
| `setContrast()` | 1 (via sendCommand2) | 1 |

Multi-step operations like `sendCommandList()` intentionally count each I2C transaction individually. This is acceptable because:
- Command sequences are short (typically 1-5 ops)
- Each is a distinct I2C transaction that can independently fail
- Failure mid-sequence is meaningful and should be tracked

### 6.5 `_applyConfig()` — Shared Configuration Application

**Purpose:** Apply stored config to device (shared by `begin()` and `recover()`).

```cpp
private:
  /**
   * @brief Apply stored configuration to device.
   *
   * Sends initialization sequence and runtime settings.
   * Used by both begin() and recover().
   *
   * Does call _updateHealth() on each I2C operation.
   *
   * @return Status Ok on success, error on first failure.
   */
  Status _applyConfig() {
    // Step 1: Run init sequence
    Status st = initDisplay();
    if (!st.ok()) return st;
    
    // Step 2: Clear GDDRAM
    st = clearGddram();
    if (!st.ok()) return st;
    
    // Step 3: Apply runtime settings (contrast, invert, flip, etc.)
    // These are stored in _config and need to be re-applied after reinit
    st = sendCommand2(cmd::SET_CONTRAST, _config.contrast);
    if (!st.ok()) return st;
    
    st = sendCommand(_config.invert ? cmd::INVERT_DISPLAY : cmd::NORMAL_DISPLAY);
    if (!st.ok()) return st;
    
    // Add other runtime settings as needed (flipX, flipY, etc.)
    
    return Status::Ok();
  }
```

---

## 7. Modifications to Existing Functions

### 7.1 Functions That Need I2C Tracking

All functions that call `_config.i2cWrite()` directly must switch to `_i2cWriteTracked()`:

| Function | Current Call | Change |
|----------|--------------|--------|
| `sendCommand()` | `i2cWrite()` | → `_i2cWriteTracked()` |
| `sendCommand2()` | `i2cWrite()` | → `_i2cWriteTracked()` |
| `sendCommand3()` | `i2cWrite()` | → `_i2cWriteTracked()` |
| `sendCommandList()` | `i2cWrite()` (loop) | → `_i2cWriteTracked()` |
| `sendData()` | `i2cWrite()` (loop) | → `_i2cWriteRaw()` (see 7.5) |

**Note:** `sendData()` is called from async flush (`tickFlush()`). Flush uses `_i2cWriteRaw()` internally and tracks health **once per flush attempt**, not per chunk. See Section 7.5 for details.

### 7.2 `begin()` Modifications

```cpp
Status Ssd1315::begin(const Config& config) {
  _config = config;
  _driverState = DriverState::UNINIT;
  _initialized = false;
  
  // Reset health tracking
  _lastOkMs = 0;
  _lastErrorMs = 0;
  _lastError = Status::Ok();
  _consecutiveFailures = 0;
  _totalFailures = 0;
  _totalSuccess = 0;
  
  // Clamp threshold
  if (_config.offlineThreshold < 1) _config.offlineThreshold = 1;
  
  // Validate config (no _updateHealth for config errors)
  if (!_config.i2cWrite) {
    return Status::Error(Err::INVALID_CONFIG, "I2C write callback null");
  }
  
  // Probe device (no health update - diagnostic only)
  Status st = probe();
  if (!st.ok()) {
    // Probe failed before init; track failure but stay UNINIT
    // Note: counters update, but _driverState stays UNINIT (not yet initialized)
    _updateHealth(st);
    return st;
  }
  
  // ... existing buffer allocation ...
  
  // Set _initialized = true BEFORE _applyConfig() so that _updateHealth()
  // can perform state transitions during init.
  // Keep _driverState = UNINIT; _updateHealth() will transition it:
  //   - First I2C success → READY
  //   - First I2C failure → DEGRADED (or OFFLINE if threshold is 1)
  _initialized = true;
  // Note: _driverState remains UNINIT here; _updateHealth() handles transitions
  
  // Apply config (includes init sequence)
  // _applyConfig uses tracked wrappers, so health is updated per I2C op.
  // First success: UNINIT → READY. First failure: UNINIT → DEGRADED.
  // Subsequent failures: DEGRADED → OFFLINE (based on threshold).
  st = _applyConfig();
  if (!st.ok()) {
    // Init failed - rollback to UNINIT
    // Note: _driverState may be DEGRADED or OFFLINE at this point
    _initialized = false;
    _driverState = DriverState::UNINIT;
    return st;
  }
  
  // Success: _driverState should already be READY from _updateHealth() calls.
  // Defensive assertion (optional):
  // assert(_driverState == DriverState::READY);
  return Status::Ok();
}
```

### 7.3 `end()` Modifications

```cpp
void Ssd1315::end() {
  if (!_initialized) {
    return;
  }
  
  sendCommand(cmd::DISPLAY_OFF);  // Will track via _i2cWriteTracked
  
  // ... existing cleanup ...
  
  _initialized = false;
  _driverState = DriverState::UNINIT;
  
  // Note: Health counters and timestamps are NOT reset.
  // They remain available for post-mortem diagnostics.
  // Counters will be reset on next begin() call.
}
```

### 7.4 Public API State Guards (Optional Enhancement)

Optionally, public API functions can check state before operating:

```cpp
Status Ssd1315::setContrast(uint8_t contrast) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "not initialized");
  }
  // Optional: warn if OFFLINE but still try
  // (device might have recovered on its own)
  return sendCommand2(cmd::SET_CONTRAST, contrast);
}
```

**Decision:** Keep existing behavior (check `_initialized` only). The health state is informational, not a hard gate. This allows:
- Application to attempt operations even in OFFLINE (might succeed)
- Single success automatically restores to READY

### 7.5 Flush Health Tracking (Device-Specific)

Flush is tracked as **one logical operation**, not per I2C chunk.

**Implementation in `tickFlush()`:**

```cpp
// Inside tickFlush() state machine:

case FlushState::SEND_DATA: {
  // Use _i2cWriteRaw() - NO per-chunk health tracking
  Status st = _i2cWriteRaw(chunkData, chunkLen);
  if (!st.ok()) {
    _flushError = st;  // Accumulate error
    _flushState = FlushState::ERROR;
    return;
  }
  // ... advance to next chunk or DONE ...
  break;
}

case FlushState::DONE:
  // Flush completed successfully - track ONCE
  _updateHealth(Status::Ok());
  _flushState = FlushState::IDLE;
  break;

case FlushState::ERROR:
  // Flush failed - track ONCE with accumulated error
  _updateHealth(_flushError);
  _flushState = FlushState::IDLE;
  break;
```

**Key points:**
- `sendData()` (called during flush) uses `_i2cWriteRaw()`, not `_i2cWriteTracked()`
- Chunk errors are accumulated in `_flushError`
- `_updateHealth()` is called exactly once: at DONE or ERROR
- This prevents a single bad flush from counting as many failures

---

## 8. `probe()` Implementation Details

```cpp
Status Ssd1315::probe() {
  // SSD1315 has no WHOAMI register. We send NOP (0xE3) and check ACK.
  // NOP command is safe and has no side effects.
  uint8_t buf[2] = {cmd::CTRL_COMMAND, cmd::NOP};
  
  Status st = _i2cWriteRaw(buf, 2);  // No health tracking!
  
  if (!st.ok() && (st.code == Err::I2C_ERROR || st.code == Err::TIMEOUT)) {
    return Status::Error(Err::DEVICE_NOT_FOUND, "Device not responding", st.detail);
  }
  
  // Note: probe() does NOT call _updateHealth() - diagnostic only
  return st;
}
```

**Why NOP command?**
- SSD1315 acknowledges NOP (0xE3) but does nothing
- Safe to send at any time, even before init
- Confirms device is present and responding

**Why `_i2cWriteRaw()`?**
- Probe is diagnostic-only, should not affect health state
- Allows repeated probing without polluting counters

---

## 9. `recover()` Implementation Details

```cpp
Status Ssd1315::recover() {
  // Can't recover if never initialized
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "begin() not called");
  }
  
  // Step 1: Probe device (probe itself is diagnostic-only)
  Status st = probe();
  if (!st.ok()) {
    // Explicitly track probe failure as a real failure
    // This is intentional: a failed recovery attempt is a transport-level event
    _updateHealth(st);
    return st;
  }
  
  // Step 2: Re-apply configuration (includes init sequence)
  // _applyConfig uses _i2cWriteTracked internally, so health is updated
  st = _applyConfig();
  // Note: _updateHealth already called by _applyConfig internals
  
  if (st.ok()) {
    // Step 3: Mark framebuffer dirty for resync
    markAllDirty();
  }
  
  return st;
}
```

**Key Properties:**
- Requires `_initialized == true`
- `probe()` itself does not update health (diagnostic-only)
- Probe failure inside `recover()` is **explicitly** passed to `_updateHealth()`
- On success: state → READY via `_updateHealth()` inside `_applyConfig()`
- On failure: counters incremented, state may transition to DEGRADED/OFFLINE

---

## 10. State Transition Diagram

```
                    ┌─────────────────────────────────────────┐
                    │                                         │
                    ▼                                         │
              ┌──────────┐                                    │
  begin() ──▶│  UNINIT  │◀── end()                           │
              └────┬─────┘                                    │
                   │                                          │
                   │ begin() success                          │
                   ▼                                          │
              ┌──────────┐                                    │
      ┌──────▶│  READY   │◀────────────────────┐              │
      │       └────┬─────┘                     │              │
      │            │                           │              │
      │            │ I2C failure               │              │
      │            ▼                           │              │
      │       ┌──────────┐                     │              │
      │       │ DEGRADED │─────────────────────┤              │
      │       └────┬─────┘  any success        │              │
      │            │                           │              │
      │            │ failures >= threshold     │              │
      │            ▼                           │              │
      │       ┌──────────┐  any success        │              │
      │       │ OFFLINE  │─────────────────────┘              │
      │       └────┬─────┘  (auto-recovery!)                  │
      │            │                                          │
      │            │ recover() success                        │
      └────────────┴──────────────────────────────────────────┘
```

**Transition rules:**

| From | Event | To | Condition | Action |
|------|-------|-----|-----------|--------|
| UNINIT | `begin()`: first tracked I2C succeeds | READY | `_initialized` | Counters reset at start; `_updateHealth()` promotes state |
| UNINIT | `begin()`: first tracked I2C fails | DEGRADED | `_initialized` | `_updateHealth()` transitions UNINIT→DEGRADED |
| UNINIT | `begin()` failure (threshold reached) | OFFLINE | `_initialized` | Failures >= threshold during init |
| UNINIT | `begin()` failure (rollback) | UNINIT | — | On `_applyConfig()` failure: rollback `_initialized=false`, reset state |
| Any | `end()` | UNINIT | — | Preserve counters, set `_initialized=false` |
| READY | I2C failure | DEGRADED | `_initialized` | Increment counters |
| DEGRADED | I2C success | READY | `_initialized` | Clear `_consecutiveFailures` |
| DEGRADED | failures >= threshold | OFFLINE | `_initialized` | Update state |
| **OFFLINE** | **I2C success** | **READY** | `_initialized` | **Auto-recovery** |
| OFFLINE | `recover()` success | READY | `_initialized` | Via internal `_updateHealth()` |
| OFFLINE | `recover()` failure | OFFLINE | `_initialized` | Update counters |
| UNINIT | I2C op (pre-init) | UNINIT | `!_initialized` | Counters updated, state unchanged |

**Note:** When `_initialized == false`, health counters are still updated, but `_driverState` remains `UNINIT`. State transitions only occur when `_initialized == true`.

---

## 11. Edge Cases Handled

### 11.1 First Failure After Long Success
- State: READY → DEGRADED
- `_consecutiveFailures`: 0 → 1
- `_lastOkMs`: unchanged (previous value still valid)
- Behavior: Application can check `state()` to detect degradation

### 11.2 Repeated Failures
- Each failure: `_consecutiveFailures++`
- At threshold: state → OFFLINE
- Behavior: Application notified via state change, can call `recover()`

### 11.3 Recovery Failure
- `recover()` returns error
- State updated via `_updateHealth()`
- `_consecutiveFailures` incremented
- Behavior: Application can retry `recover()` or give up

### 11.4 Spontaneous Recovery (Device Comes Back)
- If OFFLINE and next operation succeeds: state → READY automatically
- `_consecutiveFailures` reset to 0
- Behavior: Self-healing without explicit `recover()` call

### 11.5 Probe Before Begin
- `probe()` works without `begin()`
- Uses `_i2cWriteRaw()` to bypass tracking
- Does NOT change driver state
- Behavior: Application can scan for devices before deciding to init

### 11.6 End While OFFLINE
- `end()` attempts DISPLAY_OFF command (may fail)
- State → UNINIT regardless of I2C result
- Health counters **preserved** for post-mortem diagnostics
- Behavior: Counters available after `end()`, reset on next `begin()`

---

## 12. What Is NOT Handled (Explicit Non-Goals)

### 12.1 Non-Goal: Async State Transitions
- No tick-driven health checks
- No background monitoring threads
- State only changes on actual I2C operations
- **Rationale:** This is a synchronous driver; async behavior belongs in application layer

### 12.2 Non-Goal: Automatic Recovery
- No automatic `recover()` calls
- Application must explicitly call `recover()` if desired
- No background retry logic
- **Rationale:** Recovery timing is application-specific; driver shouldn't guess

### 12.3 Non-Goal: Rate Limiting
- No backoff between retries
- No cooldown after OFFLINE
- Application controls retry timing
- **Rationale:** Rate limiting policies vary by application

### 12.4 Non-Goal: Multiple Addresses / Devices
- Health tracking is for configured address only
- No multi-device management
- **Rationale:** Multi-device coordination belongs in application layer

### 12.5 Non-Goal: Chip Identity Verification
- SSD1315 has no ID register
- `probe()` only confirms ACK, not chip type
- **Rationale:** Physically impossible without chip support

### 12.6 Non-Goal: Bus-Level Logic
- No I2C bus arbitration
- No bus recovery (SDA/SCL unsticking)
- **Rationale:** Bus management belongs in transport layer

---

## 13. Files Modified

| File | Changes |
|------|---------|
| `include/ssd1315/Status.h` | Add `DriverState` enum |
| `include/ssd1315/Config.h` | Add `offlineThreshold` field |
| `include/ssd1315/Ssd1315.h` | Add health fields, new public methods, private helpers |
| `src/Ssd1315.cpp` | Implement new methods, modify I2C calls to use tracking |

### 13.1 Relationship to Async Flush State Machine

**The async flush FSM (`FlushState`) and `DriverState` are independent:**

```
┌─────────────────────────────────────────────────────────────┐
│                     Driver Health Layer                      │
│  DriverState: UNINIT / READY / DEGRADED / OFFLINE           │
│  (Commands: updated per I2C op; Flush: once at DONE/ERROR)  │
└─────────────────────────────────────────────────────────────┘
                              │
                              │ Flush result propagates UP
                              │ (once per flush attempt)
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    Flush State Machine                       │
│  FlushState: IDLE / SET_ADDR / SEND_DATA / DONE / ERROR     │
│  (Uses _i2cWriteRaw per chunk; tracks health once at end)   │
└─────────────────────────────────────────────────────────────┘
```

**Key points:**
- Flush uses `_i2cWriteRaw()` per chunk (no per-chunk health tracking)
- Health is updated **once** when flush reaches DONE or ERROR
- `DriverState` does NOT affect flush logic (flush continues regardless)
- `FlushState::ERROR` is a flush-level state, separate from `DriverState::OFFLINE`

### 13.2 Flush Health Granularity (SSD1315-Specific)

**Critical design decision:** Flush is tracked as **one logical health operation**, not per I2C chunk.

#### Rationale

A display flush may transfer 1024+ bytes across many I2C transactions (chunks). Tracking each chunk individually would:
- Cause rapid state oscillation (READY→DEGRADED→OFFLINE) on intermittent failures
- Generate misleading `totalFailures` counts (one bad flush = many "failures")
- Make `consecutiveFailures` threshold meaningless for flushes

#### Implementation

| API Type | Health Tracking Granularity |
|----------|-----------------------------|
| Command/config APIs (`sendCommand*`, `setContrast`, etc.) | Per I2C transaction |
| Flush (`tickFlush()`) | Per flush attempt (once) |

**Flush FSM behavior:**

1. Flush uses `_i2cWriteRaw()` internally (no per-chunk tracking)
2. Chunk-level I2C errors are accumulated inside the flush FSM
3. `_updateHealth()` is called **exactly once** per flush attempt:
   - When flush reaches `DONE` → `_updateHealth(Status::Ok())`
   - When flush reaches `ERROR` → `_updateHealth(accumulatedError)`

**Implications:**

| Metric | Meaning for Flush |
|--------|-------------------|
| `totalSuccess` | Counts successful flush completions (not chunks) |
| `totalFailures` | Counts failed flush attempts (not chunks) |
| `consecutiveFailures` | Sequential flush failures without intervening success |

**Example:**
- A flush sending 16 chunks where chunk #5 fails:
  - Flush FSM transitions to ERROR
  - `_updateHealth(error)` called once
  - `_consecutiveFailures` increments by 1 (not 12)

#### Application Guidance

> **Recovery after flush failure:**
>
> When a flush fails and the driver transitions to DEGRADED or OFFLINE:
> - The framebuffer remains intact
> - Call `recover()` to re-initialize the device
> - Call `requestFlush()` to resync display content
>
> Rate-limit recovery attempts:
> ```cpp
> if (!display.isOnline()) {
>   if (now - lastRecoveryAttempt > RECOVERY_COOLDOWN_MS) {
>     display.recover();
>     lastRecoveryAttempt = now;
>   }
> }
> ```

---

## 14. Example Usage

```cpp
#include <Wire.h>
#include <ssd1315/Ssd1315.h>

using namespace ssd1315;

Ssd1315 display;
Config config;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000);
  
  // Configure
  config.width = 128;
  config.height = 64;
  config.i2cAddress = 0x3C;
  config.i2cWrite = myI2cWrite;
  config.offlineThreshold = 3;
  
  // Optional: Probe before init
  if (!display.probe().ok()) {
    Serial.println("Display not found at 0x3C!");
    return;
  }
  Serial.println("Display found, initializing...");
  
  // Initialize
  Status st = display.begin(config);
  if (!st.ok()) {
    Serial.printf("Init failed: %s\n", st.msg);
    return;
  }
  
  Serial.printf("Display ready, state: %d\n", (int)display.state());
  
  // Draw something
  display.clear();
  display.drawText(0, 0, "Hello!");
  display.requestFlush();
}

void loop() {
  uint32_t now = millis();
  display.tick(now);
  
  // Periodic health check
  static uint32_t lastHealthCheck = 0;
  if (now - lastHealthCheck > 5000) {
    lastHealthCheck = now;
    
    DriverState st = display.state();
    Serial.printf("Health: state=%d, failures=%d/%lu, lastOk=%lu, lastErr=%lu\n",
                  (int)st,
                  display.consecutiveFailures(),
                  display.totalFailures(),
                  display.lastOkMs(),
                  display.lastErrorMs());
    
    // Attempt recovery if offline
    if (!display.isOnline()) {
      Serial.println("Attempting recovery...");
      Status rst = display.recover();
      if (rst.ok()) {
        Serial.println("Recovery succeeded!");
        display.requestFlush();  // Resync framebuffer
      } else {
        Serial.printf("Recovery failed: %s\n", rst.msg);
      }
    }
  }
  
  // Normal drawing logic
  // ...
}
```

---

## 15. Summary Tables

### 15.1 Private Members Added

| Name | Type | Purpose |
|------|------|---------|
| `_driverState` | `DriverState` | Current operational state |
| `_lastOkMs` | `uint32_t` | Last success timestamp |
| `_lastErrorMs` | `uint32_t` | Last error timestamp |
| `_lastError` | `Status` | Most recent error |
| `_consecutiveFailures` | `uint8_t` | Sequential failure count |
| `_totalFailures` | `uint32_t` | Lifetime failures |
| `_totalSuccess` | `uint32_t` | Lifetime successes |

### 15.2 Private Methods Added

| Name | Purpose |
|------|---------|
| `_updateHealth(const Status&)` | Central health state manager (Section 6.1) |
| `_i2cWriteRaw(data, len)` | Raw I2C write, no health tracking (Section 6.2) |
| `_i2cWriteTracked(data, len)` | Tracked I2C write (Section 6.3) |
| `_applyConfig()` | Apply stored config to device (Section 6.5) |

### 15.3 Public Methods Added

| Name | Returns | Purpose |
|------|---------|---------|
| `probe()` | `Status` | Check device presence (no state change) |
| `recover()` | `Status` | Manual recovery attempt |
| `state()` | `DriverState` | Get current state |
| `isOnline()` | `bool` | Check if operational (READY or DEGRADED) |
| `lastOkMs()` | `uint32_t` | Last success timestamp |
| `lastErrorMs()` | `uint32_t` | Last error timestamp |
| `lastError()` | `Status` | Most recent error |
| `consecutiveFailures()` | `uint8_t` | Sequential failures |
| `totalFailures()` | `uint32_t` | Lifetime failures |
| `totalSuccess()` | `uint32_t` | Lifetime successes |

### 15.4 Config Fields Added

| Name | Type | Default | Purpose |
|------|------|---------|---------|
| `offlineThreshold` | `uint8_t` | `3` | Failures before OFFLINE |

---

## 16. Key Design Principles

1. **Centralized health tracking** — All state changes flow through `_updateHealth()`
2. **No implicit recovery** — Application controls retry strategy
3. **Diagnostic isolation** — `probe()` never affects health counters (but callers may explicitly track probe failures)
4. **Transport-level tracking** — Only real I2C outcomes affect health; config/param errors do not
5. **Flush as logical operation** — Flush health is tracked once per attempt, not per chunk
6. **IN_PROGRESS is success** — For pattern consistency with other drivers
7. **Transport agnostic** — Driver never touches Wire/I2C directly
8. **Sync APIs, async workflows** — Commands are blocking; flush/iteration are tick-driven
9. **State guard by _initialized** — DriverState transitions only when `_initialized == true`

---

## 17. Naming Conventions

| Element | Convention | Example |
|---------|------------|---------|
| Private members | `_camelCase` | `_driverState`, `_lastOkMs` |
| Public methods | `camelCase` | `probe()`, `recover()`, `isOnline()` |
| Private helpers | `_camelCase` | `_updateHealth()`, `_applyConfig()` |
| Enum values | `CAPS_CASE` | `UNINIT`, `READY`, `OFFLINE` |
| Config fields | `camelCase` | `offlineThreshold`, `i2cTimeoutMs` |

---

**Ready for implementation.**

---

## Patch Notes (Revision 3)

### 1. Fixed UNINIT→DEGRADED transition during init (Section 6.1)
**Problem:** The `_updateHealth()` failure branch only transitioned from READY→DEGRADED, not UNINIT→DEGRADED. If the first tracked I2C operation during init failed, state would incorrectly remain UNINIT (or jump directly to OFFLINE if threshold was 1).

**Fix:** Updated the failure condition to: `(_driverState == READY || _driverState == UNINIT)` so that first failure during init correctly transitions to DEGRADED.

### 2. Updated Section 1.3 "Consistency Rules"
Added explicit documentation that first tracked I2C failure during init transitions UNINIT→DEGRADED (or OFFLINE if threshold is 1).

### 3. Updated Section 2.1 "State Transition Rules" table
- Added row: `UNINIT | begin(): first tracked I2C fails | DEGRADED`
- Added row: `UNINIT | begin() failure (threshold reached) | OFFLINE`
- Updated note to explain failure-during-init behavior.

### 4. Updated Section 7.2 `begin()` comments
Clarified that first I2C failure during init transitions to DEGRADED (not just success→READY), and that on `_applyConfig()` failure the state may already be DEGRADED/OFFLINE before rollback.

### 5. Updated Section 10 Transition table
- Added UNINIT→DEGRADED and UNINIT→OFFLINE rows for init failure cases
- Added UNINIT→UNINIT rollback row for `begin()` failure
- Removed redundant "UNINIT | I2C success (during init)" row (covered by first row).

### 6. Updated DriverState enum comment (Section 2.1)
Clarified UNINIT transitions: first I2C success → READY, first I2C failure → DEGRADED.
