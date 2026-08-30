# Code Audit Resolution — 2026-08-30

## Outcome

The repository was synchronized on `main` before edits. The local branch,
`origin/main`, and `HEAD` all pointed to `3a5b2cd` (`Add SSD1315 audit
documentation for correctness and cleanup`). Every finding in the temporary
`docs/CODE_AUDIT.md` was then checked against that exact source tree, its public
contracts, tests, examples, generated files, and repository history.

The audit was useful and most of its functional analysis was correct. Its 14
already-applied defect fixes were retained and all seven open items were
resolved. A second, fresh review of the original audit, the actual
`3a5b2cd..8026e85` diff, and the resulting source found residual lifecycle,
clipping, adapter-timeout, documentation, and test-evidence gaps. Those gaps
were corrected without adding a parallel abstraction. The temporary audit file
was removed after its durable conclusions were folded into code, public
documentation, the changelog, and this report.

No physical hardware validation was performed. This report makes no new visual,
electrical, reset, fault-injection, retention, or soak claim.

## Fresh Independent Re-audit

The original audit was re-read in full from Git history, not from this report or
the earlier completion summary. Before that review, remotes were fetched and a
clean `main`, `origin/main`, and `HEAD` were confirmed at `8026e85`. Three
parallel read-only reviews covered:

- original-requirement and disposition coverage, scope, and report accuracy;
- lifecycle/operation state, deadlines, flush ownership, and adapter timeouts;
- graphics/API arithmetic, edge cases, code simplicity, and durable tests.

Their findings were then reproduced or traced against the current source and
diff before changes were accepted. The confirmed gaps and final dispositions
were:

| Area | Confirmed gap | Resolution |
|---|---|---|
| Cooperative wake timing | The guard started from the timestamp sampled before the synchronous `DISPLAY_ON` callback, so callback time could consume the post-command interval. | The first post-callback owner poll now latches the guard start. A fake transport that advances time inside `DISPLAY_ON` pins the behavior. |
| Legacy wait deadline | `waitFlush()` called `tick()` before checking the helper deadline, and its per-attempt timeout was not clipped to the remaining total wait budget. | Deadline checking now precedes each tick; the attempt timeout is clipped to the remaining budget; dirty/error ownership remains intact. |
| Invalid legacy poll | `pollFlush(maxInstructions > 0, byteBudget = 0)` advanced the local power guard before rejecting the call. | Validation now precedes `tickPowerOn()`; zero-instruction/zero-budget queries remain valid and zero-I2C. |
| Flush failure simplicity | The internal flush-timeout branch duplicated `_failFlush()` bookkeeping. | It now uses the single existing failure helper. |
| Resync/flush ownership | A resync failure during initialization or after a completed flush could overwrite an unrelated or successful `FlushStatus` with `ERROR`. | Only failures while the current operation is in a flush phase mutate the flush result; a completed resync flush remains `DONE` and is accounted once. |
| Already-awake reassertion | Successful `setSleep(false)` on an already modeled-`ON` panel still downgraded it to `STARTING`. | `ON`/ready is preserved; only an actual off/starting wake starts a guard. |
| Line clipping | Successive corner intersections were calculated from already rounded endpoints, compounding error and rejecting a visible bottom-left crossing. | Every intersection now uses immutable original-line geometry with the existing 64-bit nearest rounding. |
| Glyph arithmetic | `drawChar()` narrowed `x + col` and `y + row` before clipping. | Coordinates stay 32-bit until on-panel bounds prove narrowing safe. |
| ESP-IDF mutex budget | Flooring a real sub-millisecond mutex wait could grant another full millisecond to transmit. | An uncontended zero-tick fast path retains the full budget; only real contention is ceil-rounded and subtracted. |
| CLI accuracy | Arduino help described `touch()` as an activity timestamp and did not qualify page-policy commands as compatibility storage. | Help/output and its static contract now state no-op/stored-only behavior. |
| Durable evidence | The original external page-buffer equivalence harness was unavailable and the repository had no equivalent regression. | The native suite now compares full-buffer output with all eight one-page windows for lines, rectangles, circles, checked bitmap, character, and text rendering. |

The re-audit also corrected report-only inaccuracies: native Arduino compilation
does not compile the guarded ESP-IDF adapter, the reversed `Error` overload has
one compatibility-test caller (but no production caller), and the 127-byte
Arduino example setting is a configuration-clarity correction rather than a
functional transport fix.

## Previously Applied Findings

The 14 functional findings in section 1 were valid. The baseline implementation
already contained the intended fix for each one; this review added direct
regression coverage where the repository suite did not pin the behavior and
closed residual edge cases noted below.

| # | Verified finding | Resolution status |
|---:|---|---|
| 1 | Legacy flush could not progress while the modeled panel was `OFF`. | Existing fix retained; direct legacy-path test added. |
| 2 | Cooperative polling did not advance the display-on guard. | Existing advancement fix retained; the guard's post-callback start was corrected and covered. |
| 3 | `waitFlush()` consumed page-iteration results. | Existing success-path fix retained; timeout ownership fixed too. |
| 4 | `waitFlush()` waited in a state it could not advance. | Existing immediate-return behavior pinned by a test. |
| 5 | The stalled-clock guard counted iterations instead of progress. | Existing progress-based guard retained. |
| 6 | `DEGRADED` was reachable only from `READY`. | Existing health transition retained and verified in the suite. |
| 7 | Stopping scroll retained a false synchronized-GDDRAM baseline. | Existing invalidation retained; direct test added. |
| 8 | Flush diagnostics exposed the driver-wide last error. | Existing independent flush error retained; direct test added. |
| 9 | Flush progress could report a stale column. | Initial reset existed; reset on every later dirty page was added. |
| 10 | Rejected `begin()` configuration destroyed the prior binding. | Existing validate-before-rebind fix retained. |
| 11 | `setSleep(false)` unnecessarily guarded an already-awake panel. | Existing wake-precondition fix retained; re-audit also preserved modeled `ON`/ready on reassertion. |
| 12 | Pre-clipped rectangles painted phantom panel-edge borders. | Existing edge-selection fix retained; direct clipping test added. |
| 13 | Radius clamping silently relocated off-panel circles. | Existing clamp removal retained; remaining narrowing wrap fixed. |
| 14 | Carriage return contributed to measured but not drawn text width. | Existing width fix retained; direct width assertion added. |

The five section 1 example/tooling findings were also checked explicitly:

| Finding | Resolution status |
|---|---|
| Arduino `Wire` short write leaked its open transmission/bus lock. | Existing `endTransmission()` cleanup retained and covered by the native adapter test. |
| ESP-IDF ceil-rounded every mutex acquisition and rejected uncontended 1 ms calls. | The earlier fix was refined during re-audit: zero-tick uncontended acquisition plus ceil subtraction only after real contention. |
| ESP-IDF `text` mis-parsed runs of spaces. | Existing `strtok_r` remainder parsing retained and guarded by the CLI contract checker. |
| Arduino `dirty mark ` compared the wrong prefix length. | Existing literal-length fix retained and guarded by the CLI contract checker. |
| `HealthMonitor` starved its periodic report. | Existing schedule update only after an emitted report retained. |

The section 1 refactors were also checked. The consolidated validation,
transport-error mapping, terminal-flush settling, dirty-rectangle handling,
configuration messages, glyph constants, and generator simplification remain
appropriate and simpler than parallel implementations. The fresh audit removed
the last manual flush-timeout bookkeeping branch, so terminal flush failures now
consistently use `_failFlush()`.

The documentation-cleanup findings were checked as well: page-buffer drawing
uses absolute panel coordinates; the hardware-scroll sample polls and consumes
cooperative results before stopping; obsolete agent scaffolding and release-
specific `RELEASING.md` instructions remain removed; the dead Doxyfile extract
path is absent; changelog links, HIL `benchmark`/`all` mode descriptions, and the
I2C error/behavior reference are corrected; and the unreachable generator
dependency-pin code remains deleted. The two coordinate/sample corrections are
now backed by maintained native/contract tests rather than only the unavailable
external audit harness.

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
The fresh re-audit found that calculating a second corner intersection from an
already rounded endpoint could still reject a visible line; intersections now
use immutable original-line geometry. The approximate one-pixel clipped-raster
limitation is documented.

### 2.3 — Public no-op/deprecated API

Kept for 4.x source compatibility. The stored timer/page policy fields and
accessors, `touch()`, the two legacy `ScrollSpeed` names, `DRIVER_OFFLINE`,
`VERSION_INT`, and the reversed `Error` overload are now described plainly as
stored-only, no-op, aliases, never-returned, or deprecated compatibility
surface. `markDirtyRect()` remains live through `requestFlushRect()`. The
original audit's zero-caller claim was too broad: the reversed overload has no
production caller, but one native compatibility test calls it. Removal remains
a 5.0 decision.

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
exactly once. The helper now checks its total deadline before every tick and
clips each callback timeout to the remaining wait budget.

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

The rejection rationale for `byteBudgetPerTick = 256` was incorrect because the
Arduino example does not call `pollFlush()`. It was not, however, a functional
transport overflow: the normal `tick()` path already clamps payload to
`maxWriteBytes - 1`, which is 127 bytes for that example. Changing the configured
value to 127 makes its intent and documentation exact without changing emitted
transactions.

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
  had consumed the entire callback timeout. The initial fix rejected exhausted
  budgets, and the fresh audit completed it with an uncontended fast path plus
  ceil subtraction for real mutex contention.
- A cooperative display-on guard counted synchronous callback time as panel
  delay; the guard now starts from the first post-callback owner timestamp.
- `waitFlush()` could start I2C at its exact deadline and grant a callback more
  time than remained. Both the admission check and attempt timeout are bounded
  by the helper deadline.
- Invalid zero-byte legacy polls could advance power state before rejection;
  validation now precedes all state mutation.
- Resync failures outside the current flush phases could overwrite completed or
  stale flush results; operation-phase ownership is now explicit.
- Repeated `DISPLAY_ON` on an already awake panel no longer invents a new
  `STARTING` interval.
- Corner-clipped lines now use original geometry for every intersection, and
  glyph coordinates are widened before bounds checks.

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
| Native Unity suite | 145/145 passed after the fresh re-audit |
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
compilation and physical HIL were not run locally. Arduino builds do not compile
the ESP-IDF adapter: `IdfI2cTransport.cpp` is guarded by
`ESP_PLATFORM && !ARDUINO`, so the earlier report claim that Arduino compilation
covered that source was false. Static contract checks are not a substitute for
native compilation or hardware validation.

GitHub Actions run `33320133491` validated the first audit-resolution commit in
all nine jobs: the native suite, current ESP32-S3/S2 Arduino builds, the
compatibility Arduino build, metadata validation, and native ESP-IDF builds for
S2/S3 on both ESP-IDF 5.3.5 and 5.5.5. A new run for the fresh re-audit commit is
recorded below once the final commit is pushed.
