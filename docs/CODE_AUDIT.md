# SSD1315 Audit — 2026-08-27

Scope: full-repository documentation cleanup plus a correctness audit of the
driver against the retained SSD1315 controller datasheet and the Wisevision
module specification.

**This file is a working document, not maintained documentation.** Delete it
once its open items are actioned or moved into issues. `docs/DOCUMENTATION.md`
is the authority for what belongs in `docs/`.

---

## 1. What was already applied

Everything in this section is committed in the working tree, verified by the
125-test native suite plus the purpose-built harnesses in section 4, and
recorded in `CHANGELOG.md` under `[Unreleased]`. It is listed here only so the
report is a complete record; no action is required.

### Functional defects fixed (14)

| # | Defect | Effect before the fix |
|---|---|---|
| 1 | Legacy flush blocked while panel modeled `OFF` | `begin(clearOnBegin=false)` → `requestFlush()` → `tick()` issued **zero** transactions and timed out after `flushTimeoutMs`, charging a false failure to health. Only the cooperative path worked. |
| 2 | `pollOperation()` never advanced the display-on guard | After `setSleep(false)`, a cooperative-only owner stayed at `PanelPowerState::STARTING` **forever**; `startFlush()` returned `PANEL_NOT_READY` indefinitely. |
| 3 | `waitFlush()` consumed the page-iteration flush result | `firstPage()`/`nextPage()` driven by `waitFlush()` never advanced the window — an infinite loop. |
| 4 | `waitFlush()` waited in states it could not advance | After `invalidatePanelState()` it burned the whole timeout and returned `TIMEOUT` with nothing flushing. |
| 5 | `waitFlush()` stall guard counted iterations, not progress | Could abort a healthy, progressing flush; verdicts were issued before re-checking the exit condition. |
| 6 | `DriverState::DEGRADED` reachable only from `READY` | First counted failure after a successful init left `state()` at `UNINIT` and `isOnline()` **false**. |
| 7 | Scroll deactivation kept `_gddramSynchronized` | Page-buffer configs reported a complete baseline for pages the controller had scrolled. |
| 8 | `FlushStatus::lastError` returned the driver-wide error | A failed `setContrast()` surfaced as a flush error. |
| 9 | `_flushCol` stale until `SET_PAGE_ADDR` | Progress snapshots reported the previous flush's end column. |
| 10 | `begin()` destroyed the binding before a zero-I2C rejection | A rejected config freed the framebuffer, cleared `initialized`, and left the driver bound to the **rejected** config. |
| 11 | `setSleep(false)` wake guard applied when already awake | ESP-IDF `demo` after `scroll stop` aborted with `STATE_ERROR`. |
| 12 | `drawRect()` clipped before choosing edges | A partially off-screen box drew a **phantom border** pinned to the panel edge. |
| 13 | `drawCircle()`/`fillCircle()` clamped the radius | `drawCircle(-100,32,220)` drew at x=89..92 instead of x=118..120, silently. |
| 14 | `getTextWidthN()` vs `drawTextN()` disagreed on `'\r'` | `getTextWidth("AB\rC")` returned 18 where the glyphs occupy 12 — mis-sized centred/right-aligned layout. |

Plus, in examples and tooling: the Arduino `Wire` adapter leaked the
arduino-esp32 bus lock on a short write (and a native test asserted that
leak); the ESP-IDF adapter's ceil-rounded mutex wait failed every 1 ms budget
before reaching the bus; the ESP-IDF `text` command mis-parsed runs of spaces;
the Arduino `dirty mark ` prefix compared 10 characters against an 11-character
literal; `HealthMonitor` starved its own periodic report.

### Refactors

Duplication removed rather than patched around, so the fixes above each live in
one place: one terminal-flush settling helper (`_consumeTerminalFlush`, was 6
copies), one flush-failure path (`_failFlush`, was 3), one panel-power gate (was
2 ad-hoc booleans), one rectangle clip (`_clipRect`), one page-buffer row
translation shared by all 7 drawing primitives (`_bufferRow`, was 10 copies),
one test-pattern renderer (was 3 near-identical loops), and
`requestFlushRect()` delegating to `markDirtyRect()`. Net **−52 lines** across
the change set while fixing 14 defects.

### Documentation cleanup

Leftover agent scaffolding and contradictory template rules removed from
`AGENTS.md`; release status corrected to the published 4.0.2 (tag `v4.0.2` →
`fabc3a2`, CI runs 31031345188 and 31031638286 both green, GitHub Release
published); `RELEASING.md` de-hard-coded from that one release; dead
`docs/_pdf_extracts` reference removed from `Doxyfile`; broken `v0.1.0`
changelog links repointed; HIL `benchmark` mode documented and the real scope of
`--mode all` stated; the I2C command reference's wrong error taxonomy corrected
and its "recommended/suggested" design-proposal sections rewritten as the
shipped behaviour; ~90 lines of unreachable TunnelMonitor dependency-pin code
deleted from `scripts/generate_version.py`.

**The two highest-impact documentation defects** were both empirically
reproduced before correction:

- **Page-buffer coordinates.** README and two Doxygen examples instructed
  `drawText(0, display.pageBufferYOffset(), ...)`. Drawing APIs take absolute
  panel coordinates and translate internally, so following the documentation
  renders the scene **once per window** — 8 stacked copies at
  `pageBufferPages = 1`. Verified: rows `0-6` correct vs
  `0-6, 8-14, 16-22, … 56-62` when documented.
- **Hardware-scroll sample.** The README sample admitted a cooperative scroll
  then called `stopScroll()` immediately. Verified: `stopScroll()` returns
  `BUSY` and does nothing, and the scroll was never polled onto the wire — the
  sample neither started nor stopped scrolling.

---

## 2. Open items — proposals, not applied

These need a decision, carry a behaviour or API-compatibility question, or
conflict with a deliberately tested design. Each has a concrete proposal.

### 2.1 Deadline expiry during `DISPLAY_ON_DELAY` discards confirmed state

**Severity:** medium. **Files:** `src/SSD1315.cpp:1168` (deadline check),
`_terminateOperation`.

An owner runs `startResync()` with `useDeadline`. All 17 init commands, all
1024 GDDRAM bytes, and `DISPLAY_ON` are confirmed — 42 callbacks, everything
succeeded. The operation enters `DISPLAY_ON_DELAY`, a **zero-I2C** wait. If the
next poll arrives after `deadlineMs`, `_terminateOperation()` marks
`controlStateDirty`, sets `PanelPowerState::UNKNOWN`, and clears
`_gddramSynchronized` — forcing a full 42-transaction resync to recover state
that was fully confirmed moments earlier.

**Why it is not applied.** This is deliberate and pinned by two tests:
`test_resync_deadline_during_flush_and_after_display_on_has_no_late_i2c`
(`test/native/test_basic.cpp:4447-4472`) asserts exactly this outcome, and
`test_owner_safe_power_admission_and_wake_cancellation_dirty_state` depends on
it. I implemented the fix, saw those two tests fail, and reverted rather than
rewrite a tested contract unilaterally.

**The tension.** A deadline is documented as bounding *transport attempts*
("Absolute monotonic deadline no more than INT32_MAX ms ahead", and
`TIMED_OUT` = "Caller-supplied deadline expired before another transaction").
`DISPLAY_ON_DELAY` issues no transaction, so nothing the deadline governs is
outstanding. Discarding confirmed state there is conservative but expensive.

**Proposal (pick one):**

- **A — narrow the deadline to transacting phases.** Guard the check with
  `_operation.phase != OperationPhase::DISPLAY_ON_DELAY`. The operation then
  completes as `SUCCEEDED` once the guard elapses. Update the two tests to
  assert the new outcome and add one asserting no I2C is issued past the
  deadline (the property the test name actually claims). Most faithful to the
  documented meaning of the deadline.
- **B — keep the timeout, stop discarding state.** Still report `TIMED_OUT`,
  but in `_terminateOperation()` skip `_controlStateDirty` /
  `_invalidateModeledPanelPower()` / `_gddramSynchronized = false` when the
  phase is `DISPLAY_ON_DELAY` and `_operation.effect == CONFIRMED`. The owner
  learns the deadline was missed without paying for a resync. Smaller test
  churn.
- **C — document the current behaviour explicitly** on `OperationOptions` and
  `startResync()`: a deadline that can expire during the display-on guard
  forces a resync, so size deadlines as
  `expected transactions × i2cTimeoutMs + displayOnDelayMs`.

**Recommendation: B**, then C's wording. It preserves the owner's timeout
signal — the real point of a deadline — without throwing away information the
driver actually has.

### 2.2 `drawLine()` clipping displaces clipped lines by up to ~2 px

**Severity:** low (cosmetic). **File:** `src/SSD1315.cpp`, Cohen–Sutherland
intersections in `drawLine()`.

Intersections use integer division that truncates toward zero, and Bresenham is
then restarted from the rounded endpoint with a fresh error term, so a clipped
line's pixels can deviate from the true line.

**Measured** (20 000 random lines with at least one endpoint off-panel):
mean per-line maximum deviation **0.162 px**, worst observed **2.056 px**.
Fully in-bounds lines are exact — 0 differences in 1 244 samples against an
unclipped Bresenham reference.

**Proposal.** Round half-away-from-zero instead of truncating. Add next to
`outCode()`:

```cpp
int32_t divRound(int32_t num, int32_t den) {
  const int32_t half = den / 2;
  return ((num < 0) == (den < 0)) ? (num + half) / den : (num - half) / den;
}
```

and use it for all four intersection computations. This roughly halves the
error. Eliminating it entirely needs the Bresenham error term carried through
the clip, which is not worth the complexity at 128×64. Either way, add a
`@note` to `drawLine()` stating that lines clipped from off-screen endpoints may
differ from the ideal raster by about a pixel.

### 2.3 Public API surface that exists but does nothing

**Severity:** low. Removing any of these is a **breaking change**, so they are
listed for a decision at the next major version rather than changed now.

| Symbol | Status |
|---|---|
| `Config::clearOnRecover`, `inactivitySleepMs`, `pageCycleMs` | Write-only storage; read back only into `SettingsSnapshot`. |
| `setAutoSleep()`, `setPageCycleInterval()` | Write into the fields above and nothing else. |
| `touch()` | Empty body. |
| `setUserPageCount()`, `setActiveUserPage()`, `getUserPageCount()`, `getActiveUserPage()` | `_userPageCount` / `_activeUserPage` form a closed loop only their own getters read. |
| `Err::DRIVER_OFFLINE` | Produced at zero sites; a `switch` arm for it is unreachable. |
| `ScrollSpeed::FRAMES_256`, `FRAMES_25` | Zero uses; actively misleading (they mean 128 and 5 frames). |
| `Version.h: VERSION_INT` | Byte-identical duplicate of `VERSION_CODE`; zero uses. |
| `Status.h: Error(Err, const char*, int32_t)` | Reversed-argument twin of the overload the codebase uses; zero callers. |
| `markDirtyRect()` | Now called by `requestFlushRect()` after this audit's refactor, so no longer dead. |

**Proposal.** Keep them all for 4.x. For 5.0, delete the deprecated
timer/page-cycle group, `DRIVER_OFFLINE`, the two `ScrollSpeed` aliases,
`VERSION_INT`, and the reversed `Error` overload. Until then, tighten the
Doxygen on each to say plainly that it stores a value or is never returned, and
drop the 10 `userpages`/`pagecycle`/`autosleep`/`touch` round-trips from
`ARDUINO_EXTENDED_COMMANDS` in `tools/run_ssd1315_hil.py` — they spend hardware
time proving that no-ops echo their own stored values.

### 2.4 `SSD1315_VERSION_STRING` override desynchronises the version API

**Severity:** low. **Files:** `include/ssd1315/Version.h:17-70`,
`scripts/generate_version.py`.

`SSD1315_VERSION_STRING` is `#ifndef`-guarded as a build-time override, but only
`VERSION` follows it. `VERSION_MAJOR/MINOR/PATCH`, `VERSION_CODE` and
`VERSION_INT` are emitted as literals from `library.json`. A downstream
`-DSSD1315_VERSION_STRING="9.9.9"` yields `VERSION == "9.9.9"` while
`VERSION_MAJOR == 4`.

**Proposal.** The semantic version has exactly one source of truth
(`library.json`), so it should not have an override hook at all. Emit it
unguarded in the generator template and keep the `#ifndef` hooks only for genuine
build metadata (`SSD1315_GIT_COMMIT`, `SSD1315_BUILD_TIMESTAMP`,
`SSD1315_GIT_STATUS`). Trivial change, but it touches the generated header, so
run `python scripts/generate_version.py check` after.

### 2.5 `waitFlush()` treats `nowMs == 0` as "no timestamp supplied"

**Severity:** low. **File:** `src/SSD1315.cpp`, `waitFlush()` prologue.

`uint32_t start = nowMs; if (start == 0) start = _nowMs();` — zero is a valid
monotonic timestamp (notably the first millisecond after boot, exactly when
bring-up code runs). A caller passing a genuine `0` silently gets the hook's
value instead.

**Proposal.** Match the pattern already used for the flush start time
(`_flushStarted`): take the caller's timestamp as given and document that
`waitFlush()` uses `Config::nowMs` only when no hook-free timestamp is
meaningful. If the "0 means use the hook" behaviour is intentional, say so in
the Doxygen — it is currently undocumented.

### 2.6 `CODEOWNERS` catch-all is an unusable bare email

**Severity:** low. **File:** `CODEOWNERS:5` — `* info@thymos.cz`.

GitHub honours an email in `CODEOWNERS` only when it is a verified address on an
account with write access; otherwise the rule is silently dropped. Every path
not matched by the two `@janhavelka` rules therefore has no code owner. The
domain is also unrelated to this repository.

**Proposal.** Replace with the GitHub handle: `* @janhavelka`. Owner decision —
not changed here because it affects review routing.

### 2.7 Stray files in the working tree

- `cmd.txt` (untracked, 293 B) — a personal Windows `robocopy` drive-backup
  command with no relation to this library. Not covered by `.gitignore`, so it is
  one `git add .` from being committed. **Left in place deliberately**: it is
  your file and deleting it is unrecoverable. Recommend `rm cmd.txt`.
- `SSD1315-4.0.2.tar.gz` (191 KB, gitignored) — the generated package archive.
  `README.md` and `docs/RELEASING.md` both say to delete it after validation.
  Left in place because `tools/check_package_contents.py` consumes it and you may
  be mid-release-check; remove when finished.

---

## 3. Findings raised during the audit and rejected

Recorded so they are not re-investigated. Each was checked against the source and
found not to be a defect.

| Claim | Why it does not hold |
|---|---|
| `sendCommandList()` duplicates `_sendCommandList()`'s bounds checks with a conflicting code | The public wrapper's checks run before the helper's, so the public `BUFFER_OVERFLOW` is always what a caller observes. No contradiction. |
| `startVerticalScroll*()` hardcoding `A[0]=1` is a bug | 29h/2Ah is defined by the datasheet as *vertical **and** horizontal* scroll; the driver's fixed one-column offset is a documented API choice, not a violation. |
| `pollOperation(maxTransactions > 8)` should clamp instead of erroring | `test/native/test_basic.cpp:3864-3872` exercises `maxTransactions = 9` and asserts the current rejection on purpose. |
| Health counters are documented per-callback but incremented per-operation | The premise was wrong about the legacy path; the wording was still imprecise and has been corrected in `Status.h`. |
| `EffectState::NONE` on `I2C_NACK_ADDR` contradicts the control-state model | `EffectState` and control-state confidence are deliberately orthogonal; `SSD1315.h:87-88` says so. |
| `pageBufferPages` default 8 is wrong for `height = 32` | Validation rejects it with a clear code before any I2C; only the message wording was worth improving. |
| `attach()` aliasing check uses the wrong extent | `byteRangesOverlap()` is asked exactly the right question — the region the driver is about to write. |
| Wire adapter's 128-byte cap should derive from `Config::maxWriteBytes` | The adapter cap is a property of the arduino-esp32 buffer, not of driver config; they are independent by design. |
| Example's `byteBudgetPerTick = 256` is a bug | The budget is also consumed by `pollFlush()`, which the example uses; only `tick()` is limited to one instruction. |

---

## 4. Verification performed

The vendored PlatformIO Core venv emptied itself mid-session
(`%USERPROFILE%\.platformio\penv\Scripts` lost `pio.exe`), so per `AGENTS.md` no
second Core was installed. The native suite was instead built with the project's
own sources against a local Unity-compatible shim kept **outside** the
repository. That harness was mutation-tested — a one-byte fault injected into
the init sequence produced 2 failures — so it is not passing vacuously.

**Repair the global PlatformIO installation and re-run `.\scripts\pio.cmd test
-e native` plus the Arduino/ESP-IDF builds before merging.** The ESP32 and
ESP-IDF example builds could not be compiled locally at all.

| Check | Result |
|---|---|
| Native suite (125 tests) | **125/125 pass**, before and after every change |
| Doxygen (`WARN_AS_ERROR = FAIL_ON_WARNINGS`) | **0 warnings** |
| `check_core_timing_guard.py`, `check_cli_contract.py`, `check_idf_example_contract.py`, `generate_version.py check`, `test_hil_runner_parser.py`, HIL `--mode all` dry-run, `check_package_contents.py` | all pass |
| New CLI guard mutation test | correctly **fails** on an injected regression (the old one could not) |

Purpose-built harnesses (all re-run against the final code):

| Harness | Result |
|---|---|
| Drawing fuzz — 300 000 primitive calls, extreme/overflowing coordinates, guard bytes around an external framebuffer | **0** out-of-bounds writes, **0** dirty-page misses |
| Flush differential — 200 000 draw+flush cycles, GDDRAM model vs framebuffer, `maxWriteBytes=65`/budget 64 | **0** mismatches |
| Same at minimum capacity (`maxWriteBytes=4`, budget 1) | **0** mismatches in 20 000 |
| Fault injection — 15 % random transport failures, 16 137 injected failures over 108 165 writes | **0** unrecovered mismatches; the dirty-retention/retry contract holds |
| Operation-machine fuzz — 200 000 random start/poll/cancel/take sequences, up to 30 % failures | **0** stuck-ACTIVE, **0** lost terminal results, **0** zero-I2C contract violations |
| Page-buffer equivalence vs full buffer, `pageBufferPages` 1–7 | byte-identical panel image for every value |
| Init sequence dumped byte-for-byte against the datasheet | exact match (see below) |

### Datasheet conformance confirmed

Every command the driver emits was checked against
`docs/pdf-extracted-md/SSD1315_datasheet.md`. All encodings are correct:
charge pump `A[7] 0 0 1 0 A2 0 A0` (`0x10`/`0x14`/`0x94`/`0x95`), IREF `0xAD`
argument `0 0 A5 A4 0 0 0 0` (`0x00`/`0x10`/`0x30`), clock `0xD5` as
`(osc<<4)|(div-1)`, precharge `0xD9` as `(phase2<<4)|phase1` with 0 rejected,
COM pins `0xDA` fixed pattern, multiplex `0xA8` with the 16..64 range enforced
by `height >= 16`, vertical scroll area `0xA3` with `startLine < scrollRows`,
horizontal scroll `0x26/0x27` as 7 bytes and vertical `0x29/0x2A` as 8 —
correctly including the SSD1315-only trailing start/end column bytes that
SSD1306 lacks. `0x10-0x17` (not `0x10-0x1F`) for the higher column nibble is
right for SSD1315.

Init dump (defaults): `AE / 20 00 / 40 / A0 / A8 3F / C0 / D3 00 / DA 12 /
D5 80 / D9 22 / DB 20 / 81 7F / AD 10 / 8D 14 2E / A4 23 00 / A6 D6 00 /
A3 00 40` — 17 callbacks carrying 20 commands, exactly as documented.

All README transaction-count claims reproduce exactly: 17 init-off, 42 resync at
capacity 129, 50 at the default 65, 1058 at the `P=1` worst case.

---

## 5. Suggested order of work

1. Decide §2.1 (deadline during `DISPLAY_ON_DELAY`) — the only open item with a
   real behavioural consequence.
2. Apply §2.2 `divRound` and §2.4 version-macro cleanup — both small and
   self-contained.
3. Repair PlatformIO, then run the full validation list in `CONTRIBUTING.md`
   including the three Arduino builds and both ESP-IDF targets.
4. Resolve §2.6 `CODEOWNERS` and §2.7 stray files.
5. Schedule §2.3 (dead public API) for 5.0.
6. Delete this file.
