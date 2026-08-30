# Code Audit Resolution — 2026-08-30

## Outcome

The repository was synchronized on `main` before edits. The local branch,
`origin/main`, and `HEAD` all pointed to `3a5b2cd` (`Add SSD1315 audit
documentation for correctness and cleanup`). Every finding in the temporary
`docs/CODE_AUDIT.md` was then checked against that exact source tree, its public
contracts, tests, examples, generated files, and repository history.

The audit was useful and most of its functional analysis was correct. Its 14
already-applied defect fixes were retained. All seven open items were resolved,
five additional edge defects exposed by the review were corrected, dedicated
regression coverage was added, and one previously rejected finding was reversed.
The temporary audit file was removed after its durable conclusions were folded
into code, public documentation, the changelog, and this report.

No physical hardware validation was performed. This report makes no new visual,
electrical, reset, fault-injection, retention, or soak claim.

## Previously Applied Findings

The 14 functional findings in section 1 were valid. The baseline implementation
already contained the intended fix for each one; this review added direct
regression coverage where the repository suite did not pin the behavior and
closed residual edge cases noted below.

| # | Verified finding | Resolution status |
|---:|---|---|
| 1 | Legacy flush could not progress while the modeled panel was `OFF`. | Existing fix retained; direct legacy-path test added. |
| 2 | Cooperative polling did not advance the display-on guard. | Existing fix retained; deadline/guard coverage expanded. |
| 3 | `waitFlush()` consumed page-iteration results. | Existing success-path fix retained; timeout ownership fixed too. |
| 4 | `waitFlush()` waited in a state it could not advance. | Existing immediate-return behavior pinned by a test. |
| 5 | The stalled-clock guard counted iterations instead of progress. | Existing progress-based guard retained. |
| 6 | `DEGRADED` was reachable only from `READY`. | Existing health transition retained and verified in the suite. |
| 7 | Stopping scroll retained a false synchronized-GDDRAM baseline. | Existing invalidation retained; direct test added. |
| 8 | Flush diagnostics exposed the driver-wide last error. | Existing independent flush error retained; direct test added. |
| 9 | Flush progress could report a stale column. | Initial reset existed; reset on every later dirty page was added. |
| 10 | Rejected `begin()` configuration destroyed the prior binding. | Existing validate-before-rebind fix retained. |
| 11 | `setSleep(false)` unnecessarily guarded an already-awake panel. | Existing fix retained; scroll-stop/reassertion test added. |
| 12 | Pre-clipped rectangles painted phantom panel-edge borders. | Existing edge-selection fix retained; direct clipping test added. |
| 13 | Radius clamping silently relocated off-panel circles. | Existing clamp removal retained; remaining narrowing wrap fixed. |
| 14 | Carriage return contributed to measured but not drawn text width. | Existing width fix retained; direct width assertion added. |

The section 1 refactors were also checked. The consolidated validation,
transport-error mapping, terminal-flush settling, dirty-rectangle handling,
configuration messages, glyph constants, and generator simplification remain
appropriate and simpler than parallel implementations.

## Open-Item Decisions

### 2.1 — Deadline during `DISPLAY_ON_DELAY`

Applied proposal A, not the audit's recommendation B. The public contract says
`TIMED_OUT` means the deadline expired before another transaction. The final
display-on guard performs no transaction, so it now completes normally after a
confirmed `DISPLAY_ON`, even if the transport deadline has passed. No callback
can start at or after the deadline.

Proposal B was not safe as written: the delay phase carries a partial operation
effect rather than the proposed `CONFIRMED` condition, could leave modeled power
stuck at `STARTING`, and would incorrectly couple operation-effect certainty to
control-state confidence. The new behavior is documented on
`OperationOptions`, `startWake()`, `startResync()`, `pollOperation()`, and in the
README. Tests cover both successful guard completion and the absence of late
I2C.

### 2.2 — Clipped-line displacement

Accepted with a safer implementation. Cohen–Sutherland intersections now round
to the nearest pixel, half away from zero, using a 64-bit product. This avoids
overflow in intermediate multiplication while keeping the rasterizer simple.
The approximate one-pixel clipped-raster limitation is documented.

### 2.3 — Public no-op/deprecated API

Kept for 4.x source compatibility. `DRIVER_OFFLINE`, `VERSION_INT`, and the
reversed `Error` overload are now described plainly as never-returned or
deprecated compatibility surface. Removal remains a 5.0 decision.

The extended Arduino HIL plan no longer spends hardware time on 13 round trips
through `touch`, user-page, page-cycle, and auto-sleep commands that only prove
no-op or stored-value behavior. Parser coverage prevents those command families
from returning to that plan.

### 2.4 — Version override desynchronization

Accepted and extended to the independently overrideable full-version macro.
Generated headers now undefine obsolete `SSD1315_VERSION_STRING` and
`SSD1315_VERSION_FULL` command-line definitions before emitting the canonical
semantic version from `library.json` and the canonical full-version composition.
Build metadata overrides remain supported. The generated header and generator
contract are tested.

### 2.5 — `waitFlush(nowMs == 0)`

Accepted. Zero is now used exactly as the caller's valid monotonic start time;
`Config::nowMs` is sampled only for subsequent times and must share that
timebase. Timeout and stalled-clock failures now use the common flush-failure
path, preserving page-iteration terminal-result ownership and counting health
exactly once.

### 2.6 — `CODEOWNERS`

Accepted. The catch-all bare email was replaced with `@janhavelka`, matching the
existing public-API and core-source ownership rules.

### 2.7 — Stray files

The conclusion was right but the audit's repository-state description was
stale. `cmd.txt` was tracked (introduced by `f0cde50`), not untracked, and was
deleted as an unrelated personal backup command; it remains recoverable from
Git history. The generated `SSD1315-4.0.2.tar.gz` was absent at review start.

## Rejected-Finding Review

The following section 3 rejections remain correct:

- public `sendCommandList()` validation deterministically precedes its helper;
- the vertical-scroll one-column offset is a documented SSD1315 API choice;
- `maxTransactions > 8` must be rejected rather than silently clamped;
- health counters are operation-level and their corrected wording is accurate;
- `EffectState` and panel-control confidence are intentionally independent;
- the default page count is valid for default geometry and incompatible changed
  geometry is rejected before I2C;
- the framebuffer alias check uses the extent the driver can write; and
- the Arduino adapter's total write cap is independent of driver configuration.

The rejection of `byteBudgetPerTick = 256` was incorrect. The example's normal
`tick()` path can send one payload per call, and the adapter accepts 128 total
bytes including the control byte, so the exact maximum payload is 127 bytes.
The example now uses 127.

## Additional Corrections Found During Verification

- `pollOperation()` used to advance the local display-on guard before rejecting
  an invalid `maxTransactions` value. Validation now occurs before any mutation.
- A multi-page dirty flush reset `_flushCol` for the first page but not each
  subsequent page, producing a stale progress snapshot before page addressing.
- Extreme circle coordinates and spans could still narrow through `int16_t` and
  wrap back onto the panel after the earlier radius-clamp fix. Coordinates and
  spans are now clipped in 32-bit space before narrowing.
- `waitFlush()` timeout paths bypassed shared terminal-flush handling. They now
  preserve dirty data, error ownership, and single health accounting.
- The native ESP-IDF example could grant another 1 ms transmit after mutex wait
  had consumed the entire callback timeout. It now releases the mutex and
  returns `TIMEOUT` before bus access when no budget remains.

## Audit-Document Corrections

Several statements were not used as evidence because they were stale or not
reproducible from the committed repository:

- full resync is 42 callbacks only at `maxWriteBytes = 129`; the default
  capacity produces 50 callbacks;
- the claimed net 52-line core reduction does not match the compared commits;
- rectangle clipping was not shared by every rectangle operation;
- the generator reduction was approximately 110 lines, not approximately 90;
- the external fuzz, differential, fault-injection, and mutation harnesses were
  not committed, so their numerical results cannot be independently rerun; and
- the literal carriage-return character in the audit/CHANGELOG prose broke
  warning-as-error Doxygen parsing and was replaced by `0x0D` wording.

The v4.0.2 tag/release evidence cited by the audit was checked separately and
was consistent with the published release. README's stale “prepared” wording
was corrected to “published.”

## Validation

Validation was run against the final source changes:

| Check | Result |
|---|---|
| Native Unity suite | 137/137 passed |
| ESP32-S3 Arduino build, pioarduino 55.03.311 | Passed |
| ESP32-S2 Arduino build, pioarduino 55.03.311 | Passed |
| ESP32-S3 compatibility build, pioarduino 54.03.20 | Passed in an isolated PlatformIO data directory |
| Core timing, Arduino CLI, and native ESP-IDF contract checkers | Passed |
| Generated-version consistency check | Passed |
| HIL runner parser suite | 38/38 passed |
| Benchmark and Arduino-extended HIL dry runs | Passed |
| Doxygen, warnings as errors | Passed |
| Package creation and content validation | Passed; generated archive removed afterward |

The first PlatformIO attempt exposed an incomplete generated `C:\pio\penv`.
It was preserved as `C:\pio\penv.broken.20260830_audit`, and the required
repository wrapper recreated the environment using the already installed VS
Code-managed PlatformIO Core 6.1.19. The old and current pioarduino platforms
also cannot safely share their identically named framework package directories,
so the older compatibility build was run in an isolated short data directory.
No second PlatformIO Core was installed. The generated compatibility data
directory `C:\pio\audit-compat-20260830` remains because the environment's
destructive-operation guard rejected its recursive removal after validation.

`idf.py` and physical SSD1315 hardware were unavailable, so native ESP-IDF
compilation and physical HIL were not run locally. The native ESP-IDF source was
covered by its static contract checker and Arduino compilation of the shared
adapter, but those checks are not substitutes for native IDF or hardware
validation.
