# SSD1315 driver code audit (2026-09-09)

Scope: `include/ssd1315/*.h`, `src/SSD1315.cpp`, example transports and CLI
callers, native tests, and the changed documentation/package checks. Sources:
SSD1315 datasheet Rev 1.1 and Wisevision X096-2864KSWPG01-H30 module spec
(`docs/pdf-extracted-md/`), with rendered controller pages checked where text
extraction omits figures.

This report incorporates the independent follow-up review. Its original
proposals were not all bug fixes and must not be applied wholesale. Function
names below identify code paths without relying on line numbers that move as
repairs are applied. Proposed policy/API redesigns remain separate work.

Consumer inspection establishes local usage only. The nearby manifests pin
CO2Control to v1.2.0, RadarNode to v1.1.3, and PistonNode/TLP300Node to v1.1.0;
they do not establish four v1.2.0 consumers. TunnelMonitor-node uses the
cooperative API and explicitly configures its timeout and reference policy.

## 0. Summary

The reviewed command encodings match the datasheet, but that does not prove
correctness of state transitions or complete-frame synchronization. The
follow-up review reproduced two synchronization defects after the initial
`setFlipX()` change and found missing flush admission in both CLI callers.
It also confirmed a pre-existing stranded legacy flush after panel invalidation.

The patch repairs remap/page-iteration admission, failed or cancelled baseline
rewrite handling, explicit CLI flushing, legacy flush termination, release
evidence guards, and the documentation corrections below. The expanded review
also closes related paths that could reuse stale page-iteration coverage and
preserves health-accounting order when ownership changes.
The datasheet supports the remap rewrite requirement in section 1.2.8, physical
page 49. Marking dirty pages alone does not start a framebuffer transfer.

The total flush timeout and deadline-clipped transport timeout implement
existing documented policies. They are not certain bugs. Raw command success
must retain conservative panel-model invalidation. Scroll geometry, reference
selection, and API simplification need their own explicit scope and validation.
There is already one physical flush state machine, `_pollFlushInternal()`,
with several admission/result/accounting owners around it.

## 1. Implemented repairs from the follow-up review

| Item | Implemented behavior and end-to-end path |
| --- | --- |
| F1 / R1 | `setFlipX()` returns `BUSY` before I2C for a changed remap during page iteration, including after the last window transfer before `nextPage()` finishes. Successful same-value calls preserve dirty/synchronization state and still follow ordinary direct-command admission. A later changed remap invalidates the baseline and requires a complete new frame before wake. |
| F1 / R2 | `_failFlush()` and cancellation/deadline termination through `_failOperation()` re-dirty the whole full framebuffer when its baseline is incomplete. A later caller-admitted flush rewrites every byte and restores wake admission. A successful complete rewrite establishes a baseline even when concurrent mutations leave dirty generations; those remaining changes still block synchronization/wake until flushed. No transport retry was added. |
| F1 / R3 | Arduino flip commands use `setFlipXAndFlush()` / `flushBlocking()`; native IDF uses `requestAndWaitFlush()`. Both surface the final flush failure. The Arduino self-test follows the same path, and the HIL flip sequence uses an asymmetric image so mirroring can be observed. |
| A7 | `invalidatePanelState()` now terminates an active legacy flush with observable `CANCELLED` state and no I2C, even with `flushTimeoutMs=0`. It retains RAM/dirty data, ends page iteration, releases the stranded flush ownership and allows recovery. Local cancellation does not count as a transport-health failure. |
| R5 | `validate_release_evidence()` again rejects `IN_PROGRESS_DO_NOT_RELEASE` and `SOAK_PENDING_REPLACE` and requires the dated COM21 report's platform, operation-count and duration tokens. In-memory regression cases exercise valid and rejected source/archive evidence; CI runs them. |
| R6 | Restored physical PDF page 24 Figure 6-19's `8Dh 00h` discrepancy with the `8Dh 10h` prose/command table; implementation retains `0x10`. Remap cites section 1.2.8. The controller reference distinguishes actual STARTING transfer gates from the proposed relaxation and documents resync resets of fade, zoom and scroll area. |

Related page-iteration coverage is also invalidated when initialization first
attempts I2C, including first/later failures and cancellation after a command.
Cancellation/deadline expiry before any attempt preserves the existing
iteration. Successful raw passthrough and ambiguous raw failures terminate
iteration; a definite address NACK retains its no-effect distinction. Discarding
dirty window data through `clearDirty()`/`clearDirtyIfIdle()` ends the iteration.
Starting hardware scroll during iteration returns `BUSY` before I2C, and
deactivating active scroll ends any iteration that could reuse pre-scroll pages.
Paths that end the iteration require a fresh complete render before certifying
a new baseline.

When a page-window result is pending, the older outcome is published once before
a direct command, new cooperative operation, invalidation or dirty discard
changes ownership. A prior success can no longer erase a newer command's
consecutive failure. Cooperative terminal results remain caller-owned and
consume-once; neither compatibility polling nor accounting consumes them.

The initial audit ran 145 native tests successfully before these follow-up
repairs. Those tests missed R1 and R2. Historical validation of the initial
repair tree (`db6490f`, before the 2026-09-10 diagnostic follow-up) on Windows:

| Check | Result |
| --- | --- |
| Native Unity suite | 156 passed, including 11 new regression functions covering synchronization, transport failure, cancellation, deadlines, ownership and eventual wake |
| HIL parser tests | 41 passed |
| Package evidence regressions | 4 tests passed, including release-blocker rejection despite otherwise complete evidence |
| Repository guards | Core timing, CLI, native-IDF example, generated version and source/archive package checks passed |
| Doxygen | Completed without warnings or errors |
| HIL dry runs | All seven named modes and the default invocation passed; no serial device accessed |
| Arduino builds | Current 55.03.311 S3 and S2, plus compatibility 54.03.20 S3 passed against final source |
| Native ESP-IDF and physical panel | Not run; asymmetric mirroring remains a hardware observation to perform |

Local logs are under the ignored `.pio/audit-20260909/` directory, including
`native-fixes.log`, `arduino-current-final.log`, `arduino-compat-final.log`,
`doxygen-fixes.log`, the `hil-*-fixes.log` files and `package-fixes.log`.
The earlier S2 toolchain blocker was resolved by restoring the verified existing
compiler extraction; no new PlatformIO Core or global registry change was used.
These are host/build results, not new hardware or visual validation evidence.

### 2026-09-10 follow-up: cancellation wording and diagnostic evidence

The full-frame re-dirty rule covers flush failure/timeout and cooperative
`cancelOperation()` cancellation. A legacy flush cancelled by
`invalidatePanelState()` instead retains its existing dirty mask and requires
full-buffer resync/recovery before normal flushing resumes. README, the
controller reference and changelog now state that distinction. The public
cooperative API notes were already scoped correctly.

An isolated native probe reproduced the distinction: after a remap and one
completed legacy-flush page, invalidation retains dirty mask `0xFE`, exposes
`CANCELLED`, rejects ordinary flush admission with `CONTROL_STATE_UNKNOWN`,
and successful recovery rewrites all 1024 bytes. This is a documentation
correction; the tested core behavior is unchanged.

The native ESP-IDF CLI now uses `SSD1315::toString(st.code)` in `printStatus()`
instead of its incomplete local error-name switch. `CANCELLED`,
`CONTROL_STATE_UNKNOWN` and `RESULT_NOT_AVAILABLE` therefore retain their
canonical names alongside the existing numeric code, detail and message.
The native-IDF example contract checker requires this shared conversion.

The HIL runner changes follow the actual parsing, classification and response
completion paths:

| Surface | Follow-up behavior |
| --- | --- |
| `cfg` / `settings` evidence | Require exactly one valid `initialized`, `dirty`, `flush`/`flushing`, `controlDirty` and `scrollActive` state value. Missing, invalid or repeated fields return `FAIL` in both final and intermediate modes. Final snapshots require initialized=true and the other states clean; intermediate mode relaxes cleanliness only. |
| Terminal status and completion | Current-command, known Arduino labels such as `setContrast`/`text+flush`, and native-IDF `Status:` error records, including `TIMEOUT`, return `FAIL`. Historical `Last error: TIMEOUT` alone does not become a new failure. Native-IDF `probe`/`recover` recognize `Status: OK` as completion; required configuration/status/counter tokens must be delimited before they establish a response boundary. |
| Counted diagnostics | Self-test/workload success requires an explicitly parsed zero-failure counter. Text workload summaries require explicit `Successes`/`Failures`, without falling back to unrelated operation or self-test counts. A requested workload count mismatch returns `FAIL`; incomplete, malformed or repeated required counters cannot produce `PASS` and remain `REVIEW_REQUIRED` when no explicit failure is present. |
| Functional scroll sequence | Insert the existing `flush` command after `scroll stop` and before display off/on. Stopping scroll invalidates the GDDRAM baseline; the rewrite restores the wake prerequisite. A failed rewrite remains observable and blocks wake. |

The post-scroll sequence was traced through both CLI helpers with isolated host
probes: omitting the flush transferred zero framebuffer bytes and wake returned
`STATE_ERROR`; inserting it transferred 1024 bytes and allowed sleep/wake;
an injected transfer timeout retained its detail and blocked wake until an
explicit successful retry. The local evidence is
`.pio/reaudit-20260910/hil_scroll_sequence_probe.log`.
These fixes concern diagnostic evidence and command ordering; they do not
change core driver operation or establish new hardware results.

Follow-up validation passed: 156 native tests, 52 HIL parser tests, four package
evidence regressions, CLI/IDF/core/version guards, all eight HIL dry-run
invocations, and Doxygen without warnings or errors. Current pioarduino
55.03.311 S3/S2 and compatibility 54.03.20 S3 builds passed against the final
source. Independent host probes passed 25 exact IDF status-printer cases and
the six post-scroll cases described above. Their logs, including
`parser-final-fixes.log`, `native-final-fixes.log`, `idf_status_print_probe.log`
and the final build logs, are local under ignored `.pio/reaudit-20260910/`.

Fresh final-package generation and source/archive validation also passed:
`.pio/reaudit-20260910/final-package/SSD1315-4.0.2.tar.gz`, with its local
`package-final-fixes.log` recording the check.
Native ESP-IDF firmware compilation and physical hardware/visual validation
were not run; the IDF printer and CLI-sequence probes execute on the host.

## 2. Reviewed functional and design proposals

### A1. Total flush timeout: existing policy, not a demonstrated defect

`Config::flushTimeoutMs` defaults to 1000 ms and `_pollFlushInternal()` applies
it to the whole job, including caller scheduling gaps. The Doxygen explicitly
defines that contract and documents zero for an external deadline owner.
With default transfer capacity a full 128x64 frame requires 32 callbacks;
the audit reproduced success at a 32 ms polling interval and failure at 33 ms.
That is a consequence of the configured total-job bound.

TunnelMonitor's `flushTimeoutMs=0` validation is an explicit application
contract, not proof that the default is wrong. Per-transaction `i2cTimeoutMs`
and whole-job deadlines bound different things. Retain the current behavior
and deadline tests. Converting the field into a `waitFlush()`-only timeout or
changing its default is a separate policy/API change requiring documentation
and consumer review.

### A2. Display-on guard: relaxation is an unvalidated policy proposal

`_startOperation()` rejects flush admission in `PanelPowerState::STARTING`;
`_pollFlushInternal()` defers transfer until ready or command-confirmed OFF.
Datasheet section 6.9 describes the 100 ms SEG/COM output latency after `0xAF`
without an explicit host-write prohibition. This permits considering a less
restrictive software policy; it does not prove the new behavior on a panel.

Removing those gates alone would still leave cooperative WAKE owning
`DISPLAY_ON_DELAY`. Review operation ownership, result consumption, visibility
diagnostics and compatibility timing together before changing this policy.
The current wait releases the bus but still gates GDDRAM transfer. No guard
relaxation or hardware validation is implied by this report.

### A3. Deadline clipping: existing attempt policy with precise failure effects

`pollOperation()` and `waitFlush()` clip the transport timeout to the remaining
caller deadline. This keeps the callback budget within that deadline and can
allow a short transaction to finish even when less than `i2cTimeoutMs` remains.
Requiring the entire configured maximum before every attempt would reject such
transactions and is a different admission policy.

A transport timeout can leave an indeterminate effect. However, a pure flush
failure through `_failFlush()` does not automatically invalidate control/power
state or force resync. The audit reproduced a clipped 1 ms transport timeout
followed by a successful caller-admitted flush retry without resync. Expiry
after a confirmed control prefix is PARTIAL, not NONE. Preserve those
classifications and the current timeout policy; do not delete the associated
state and tests based on the original overstated claim.

### A4. Narrow geometry and scrolling require a concrete panel profile

The driver accepts width 1..128 and begins its flush column range at zero.
There is no configurable column offset. This limits modules whose physical
SEG wiring needs an offset; it does not prove all sub-128 widths invalid.

A column-offset feature needs a concrete supported module, address validation,
remap semantics, and tests for all address-emission paths. Retain the current
128-column hardware-scroll restriction: datasheet section 1.3.1 explicitly
describes a 128-column design. A flush offset alone does not validate narrow
hardware scrolling. Do not widen scroll support or reject all narrow widths
as part of these fixes.

### A5. IREF selection depends on actual module wiring

Generic `Config` selects `INTERNAL_19UA`. Internal reference operation requires
appropriate IREF wiring; the datasheet says to leave that pin unconnected.
The Wisevision profiles already select `IREF_EXTERNAL` consistently with their
module specification. Neither reference mode is safe to assume for every
unidentified module: switching the generic default to external can break a
module with IREF left NC.

Keep explicit profile/board selection. TunnelMonitor's explicit internal
selection establishes software intent, not electrical qualification. Consult
the exact populated board schematic/module before claiming either mode correct;
a reference application circuit alone does not identify populated components.

### A6. Keep conservative invalidation after raw commands

`sendCommand*()` invalidates modeled panel state after arbitrary successful
raw passthrough. A raw addressing, scroll, power or remap command can invalidate
the assumptions used by later typed APIs and flushes. Success confirms the
write, not preservation of those assumptions. A warning to callers cannot make
an unchanged cached model accurate.

Retain this public contract and require resync before modeled operation resumes.
A narrowly defined, proven no-effect command could receive a separate exception
if needed; do not exempt arbitrary raw commands. `probe()` already provides an
ACK-only diagnostic without discarding the model.

### A7. Terminate a legacy flush on panel invalidation

This is a functional bug, not cosmetic cleanup. `invalidatePanelState()` must
terminate both cooperative and compatibility-owned work locally. The repair
must preserve dirty data, publish a stable cancellation/failure status, release
ownership for recovery, and account for the failed job once. Include active
page iteration and `flushTimeoutMs=0` in coverage. This repair does not require
a flush-facade redesign.

### A8. Document which settings resync resets

`_sendInitStep()` resets fade, zoom and vertical scroll area (steps 14-16).
Their setters do not retain all those values as resync configuration, unlike
contrast/invert/remap. Document that reset behavior. If retention is ever
chosen instead, initialization must replay the cached values; merely caching
them cannot implement retention.

### B1. Consolidating flush ownership must preserve consume-once results

`requestFlush()`/`tick()`/`pollFlush()`/`waitFlush()` and the cooperative API
already share `_pollFlushInternal()`. Multiple ownership and accounting layers
around it are a maintenance concern, not two physical flush engines.

A future compatibility facade may reduce that layering. Any automatic terminal
result consumption in `tick()` must apply only to compatibility-owned work.
Caller-admitted operations must keep their result available exactly once to
`takeOperationResult()`. Preserve page iteration, cancellation, deadlines,
dirty generations and exactly-once health accounting; those tests specify
required behavior and cannot be deleted just because implementation changes.

### B2. Power-state consolidation must preserve uncertainty

`_sleeping` caches successful command state; UNKNOWN modeled power is a
separate concept. A replacement storage model must preserve `isSleeping()`
and snapshot contracts, uncertainty after failed callbacks, and guard timing
latched after successful callbacks. The proposed expression treating every
state except STARTING/ON as sleeping would silently change those semantics.
No power-model refactor is required for the confirmed fixes.

### B3. Page-iteration API redesign is a compatibility change

The current boolean `nextPage()` combines progress and error signaling and
implicitly admits window flushes. Changing its return type or removing that
admission breaks callers. C++ cannot overload zero-argument `nextPage()` by
return type alone; a compatibility wrapper needs a differently named new
operation or a different signature. Any design must certify synchronization
only after complete coverage of every window under one remap configuration.
Local consumer searches do not establish that external callers omit this API.

### B4. Request IDs are a public contract

Nonzero IDs that differ from the preceding admitted request are documented.
Auto-assignment could be an intentional extension, but must define the returned
identity, wraparound and consume-once result ownership. No runtime defect has
been demonstrated by the existing restriction.

### B5. Blocking helper consolidation must retain stalled-clock handling

`waitFlush()` and `_runBlockingAdmittedOperation()` have bounded progress and
stall checks. A future shared loop must handle both a missing clock hook and a
present hook that stops advancing, while allowing real operation progress.
A guard restricted to a missing hook would weaken the current guarantee.

### C. Public API removals are optional major-version work

Stored-only policy fields, aliases, reversed `Status::Error` overloads,
namespace aliases and version aliases remain public contracts even if a local
search finds few callers. Removing them requires an explicit major-release
scope and migration documentation. Do not infer authorization for a 5.0.0
removal batch from this audit. Version changes must go through `library.json`
and its generator; never edit generated `Version.h` manually.

### D. Readability and transfer optimizations need semantic constraints

Some drawing bounds checks overlap, but `bufferIndex()` modulo arithmetic
maps pages into the active RAM window; it is not a third bounds check. Preserve
optimized line/rectangle primitives and mutation-generation tracking if helpers
are consolidated.

A multi-page flush fast path must respect caller byte/transaction budgets,
per-page generation snapshots, partial progress, retry addressing and baseline
coverage. Failed attempts count as callbacks, so `transactionCount` alone cannot
determine `EffectState`; address NACK and ambiguous errors have different
effects. Public API documentation must retain those distinctions and caller
ownership requirements even if its wording is shortened.

## 3. Recommended order

1. Repair and test the demonstrated synchronization, CLI admission and legacy
   invalidation defects (R1-R3 and A7), following each path through terminal
   results, dirty state, wake/recovery admission and health accounting.
2. Restore the release-evidence guard and correct source/documentation claims
   (R4-R6). Validate accepted evidence and rejected markers/missing tokens in
   memory, for both source and packaged evidence labels.
3. Run applicable host, contract, package and example-build checks. Record
   unrun hardware/native-IDF checks explicitly; previous results do not qualify
   a later implementation.
4. Defer A1-A5 policy/geometry/default changes, the B-series facade redesign,
   and C/D cleanup to separately scoped work. Retain raw invalidation (A6).
   Document resync reset behavior (A8) without adding speculative state.

## 4. Verified correct against the datasheet

The initial audit checked the following encodings and paths against the source
documents. These checks do not establish complete driver correctness or physical
panel behavior:

- I2C: 7-bit 0x3C/0x3D only; control bytes 0x00/0x40; one control byte per
  transaction; write-only (no status or GDDRAM read exists over I2C, so
  `probe()` correctly claims ACK only).
- Init sequence (`_sendInitStep`): `0xAE`, `0x20 0x00`, `0x40|line`,
  `0xA0/0xA1`, `0xA8 (height-1)` with height 16..64 (MUX 15..63 valid),
  `0xC0/0xC8`, `0xD3`, `0xDA` (0x02/0x12/0x22/0x32), `0xD5` (`osc<<4 |
  div-1`), `0xD9` (`phase2<<4 | phase1`, codes 1..15), `0xDB`, `0x81`
  (0x01..0xFF; 0x00 is invalid per the command table), `0xAD`
  (0x00/0x10/0x30), `0x8D` (0x10/0x14/0x94/0x95), `0x2E`, `0xA4`, `0x23 0x00`,
  `0xA6/0xA7`, `0xD6 0x00`, `0xA3 0 height`. Charge pump is enabled before
  `0xAF`, as required.
- Wisevision profile bytes equal the module spec's init code (`0xB0` contrast,
  `0xD5 0x90`, `0xDB 0x30`, `0xDA 0x12`, `0xD9 0x22`, `0xA1`, `0xC8`,
  external IREF, `0x8D 0x14` or `0x10`).
- Scroll: deactivate before setup; 7-byte `0x26/0x27` and 8-byte `0x29/0x2A`
  layouts; interval codes 6/32/64/128/3/4/5/2 frames; vertical offset <
  scroll-area rows; start line < scroll-area rows; GDDRAM writes refused while
  scroll is active; full-frame rewrite scheduled after `0x2E`.
- Shutdown order `0xAE` then `0x8D 0x10` matches datasheet 6.9.2.
- Zoom requires alternative COM pins (`0xDA` bit 4).
- Flush addressing: horizontal mode with per-page `0x21`/`0x22` windows;
  data chunk bounded by `maxWriteBytes - 1`; dirty column windows per page;
  `_dirtyGeneration` correctly keeps a page dirty when it is mutated during
  its own transfer.
- Example transports: Arduino `endTransmission()` codes 2/3/5 map to address
  NACK, data NACK, timeout; the ESP-IDF adapter maps `ESP_ERR_TIMEOUT` and
  treats the rest as bus errors (IDF does not distinguish address vs data
  NACK), which is the honest mapping.
- Timing paths use unsigned subtraction / wrap-safe deadline comparisons;
  native deadline tests remain required when changing these paths.
