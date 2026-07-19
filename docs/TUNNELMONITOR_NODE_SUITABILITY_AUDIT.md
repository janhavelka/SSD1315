# TunnelMonitor-node Suitability Audit

## SSD1315 OLED Display Library

Disposition updated: 2026-07-19

Result: **the v4 library-side architecture gaps H-02 through H-11 are
resolved on the current branch; integration is still deferred**.

H-01 remains an external blocker: the exact TunnelMonitor OLED module,
controller, electrical profile, IREF mode, orientation, and reset arrangement
have not been established by authoritative hardware evidence. The current
library profile sends SSD1315-specific `SET_IREF`; address ACK cannot prove
controller identity or SSD1306 compatibility.

TunnelMonitor must not adopt the library until H-01, the firmware-side owner
contract gates, immutable dependency selection, exact builds, and representative
hardware validation are completed.

## Evidence Basis

Published tag `v3.0.0` points to `1744a3720f30f0b2b19b5d7673b4b1dfccc52af4`.
The original suitability audit was then added on clean `main` at
`5d49925f31c154ac90a6f576e6c343db784b839e`. The v4
acceptance re-audit began from clean branch revision
`ac62f5e71528d4a8e22005fb63824f09a29ed072`; the acceptance implementation
fixes are committed at `3d97f2af489912a8056748e91b2381feaf296134`.
A published v4 release revision does not exist yet. The final
native rerun after the independent-review fixes passed 118 of 118 tests. That
is local host evidence, not CI, ESP-IDF, or hardware qualification.

TunnelMonitor was inspected read-only at:

| Revision | State | Relevance |
| --- | --- | --- |
| `0897f12c1a1369367747d1063936906005391580` | Clean `develop` | Display/I2C owner, scheduling, result, and dependency contracts audited |
| `322a7b2b130da658d9c86ee35afa874b10617939` | Clean `docs/mb85rc-suitability-contract-facts` | Only three FRAM documentation facts differ; no cited display/I2C source or authority changed |
| `b708f511964db6c51e949e99c67820476f00f9c7` | Clean final recheck of the same branch | Reverts the three FRAM fact edits; `322a7b2..b708f51` changes no display/I2C authority file |

The authoritative dependency policy still records SSD1315 integration as
deferred (`docs/guidelines/dependency_policy.md:23-35,95-110`). No
TunnelMonitor source, dependency, or documentation was changed by this work.

## Current V4 Library Contract

- `attach()` validates, binds, and optionally allocates once without I2C.
  Rejected candidates do not destroy an existing valid binding.
- `detach()`, `end()`, and the destructor release local state with zero I2C.
  Physical shutdown is an explicit `startShutdown()` operation.
- One fixed operation model covers initialize, flush, sleep, wake, resync,
  shutdown, and three-phase horizontal/vertical scroll setup.
  `OperationOptions` supplies nonzero request identity and an optional
  absolute wrap-safe deadline; `OperationProgress` and consume-once
  `OperationResult` expose phase, terminal state, transport-outcome effect
  certainty, command-confirmed modeled power, byte/chunk progress, and callback
  count. These are not controller readback or visual/electrical verification.
- Operation admission and `cancelOperation()` perform zero I2C. Cancellation
  retains any unconfirmed framebuffer data as dirty.
- `pollOperation()` accepts a maximum of eight callback slots per call. A
  normal external owner uses one, guaranteeing at most one callback and at
  most one physical transaction. Deadline-bearing work is limited to one
  attempt per poll so a later attempt never reuses stale caller time.
- The transport callback returns a fixed `TransportResult` with only terminal
  outcomes. Each invocation permits at most one physical transaction and must
  not retry, recover the bus, delay/back off, or replay an ambiguous write. The core does
  none of those things and owns no lock or bus. A callback already executing in
  TunnelMonitor's sole owner must not recursively acquire that owner lock.
- `Config::maxWriteBytes` is the complete transport write capacity, including
  the one-byte control prefix, and is validated in `[4..129]` before I2C.
- A 128x64 initialize-off operation takes 17 callbacks. A full-buffer resync
  at capacity 129 and payload budget 128 takes 42 callbacks: 17 init, eight
  repetitions of column address + page address + one data write, then
  `DISPLAY_ON`. The default capacity 65 takes 50 callbacks. The subsequent
  display-on interval performs no I2C.
- Initialization explicitly restores full-height scroll area, fade-off,
  zoom-off, and scroll deactivation. Wake rejects dirty or incompletely
  populated GDDRAM and never hides a flush.
- Drawing, `drawTextN()`, bounded text-width helpers, `markDirtyRect()`, dirty
  helpers, and activity bookkeeping are memory-only and never wake the panel.
- Successful raw-command passthrough invalidates modeled controller and power
  state. A full resync is required before state-dependent work resumes.
- `OFFLINE` is diagnostic only. It never owns admission, retry, or recovery
  policy.
- Auto-sleep and page-cycle fields/accessors remain deprecated compatibility
  storage. Core `tick()` never admits those policies.
- Page-buffer mode initializes off and does not support full-buffer resync. The
  owner iterates and flushes each page while the panel remains off, then admits
  an explicit wake after the complete frame is written.
- SSD1315 exposes no NVM programming, calibration storage, endurance-limited
  write, commissioning, or readback procedure. Rare/one-time maintenance
  operation requirements are not applicable; raw passthrough remains bounded
  advanced access.
- `begin()` and `recover()` are bounded blocking compatibility facades over the
  same cooperative state machine. A shared-bus owner must use
  attach/start/poll/result instead.

## Finding Dispositions

The named native tests are passing traceability points in
`test/native/test_basic.cpp`; the aggregate local result is 118 of 118.

| Finding | V4 disposition | Traceability |
| --- | --- | --- |
| H-01: exact TunnelMonitor panel profile unknown | **Open external blocker.** The library remains SSD1315-only and cannot infer controller identity from ACK. | Datasheet/profile tests remain necessary, but closure requires BOM/schematic/module evidence and real hardware. |
| H-02: active blocking lifecycle | **Resolved in the owner API.** Passive attach plus cooperative initialize/resync are primary; blocking begin/recover are compatibility-only. | `test_attach_is_zero_i2c_atomic_and_retains_binding_after_init_failure`, `test_all_cooperative_operations_respect_one_transaction_and_byte_budget`, `test_begin_does_not_probe_and_end_is_zero_i2c` |
| H-03: display-on before a complete first frame | **Resolved.** Initialize leaves the panel off; full resync transfers the complete frame before display-on; wake is zero-I2C rejected until a complete GDDRAM baseline exists and all dirty data is flushed, including every page-buffer window. | `test_resync_with_129_byte_capacity_sends_eight_full_chunks_before_display_on`, `test_page_buffer_attach_is_safe_and_owner_flushes_while_off_before_wake`, `test_wake_requires_complete_gddram_baseline_not_only_clean_dirty_bits` |
| H-04: hidden I2C from drawing, tick policy, destruction | **Resolved.** Drawing/activity are memory-only; tick does not admit sleep/page policy; detach/end/destruction are zero-I2C. | `test_draw_text_n_and_touch_are_fixed_length_memory_only_and_never_wake`, `test_detach_and_destructor_cancel_local_state_with_zero_i2c`, `test_begin_does_not_probe_and_end_is_zero_i2c` |
| H-05: core-owned OFFLINE admission/recovery | **Resolved.** Health remains observable but does not gate an admitted operation. | `test_offline_health_is_diagnostic_and_resync_still_attempts_i2c`, `test_repeated_initialize_shutdown_and_rebind_are_explicit` |
| H-06: fixed 64-byte private flush cap | **Resolved.** Validated total capacity `[4..129]` controls data payload; opaque command streams are never split, and undersized transports fail before I2C. | `test_attach_rejects_invalid_write_capacities_before_i2c`, `test_small_write_capacity_rejects_unsplittable_commands_before_i2c`, `test_resync_with_129_byte_capacity_sends_eight_full_chunks_before_display_on` |
| H-07: callback retry ambiguity | **Resolved at the library boundary.** `TransportResult` is terminal; each callback permits at most one physical transaction and no core/callback retry or recovery exists. | `test_v4_types_are_fixed_trivial_and_noexcept`, `test_ambiguous_flush_timeout_retains_dirty_and_retry_readdresses`, transport callback contract in `Config.h` |
| H-08: incomplete deadline/cancellation ownership | **Resolved.** Request identity, absolute deadlines, one-attempt deadline polls, timeout clipping, zero-I2C cancellation, effects, cooperative scroll setup, and consume-once results are explicit. | `test_cancellation_is_zero_i2c_during_init_and_each_flush_phase`, `test_cooperative_scroll_cancellation_covers_each_boundary`, `test_deadlines_are_exact_and_wrap_safe_without_late_i2c`, `test_deadline_clips_each_transport_attempt_timeout_including_wrap`, `test_request_ids_are_busy_until_result_is_consumed_and_take_is_once` |
| H-09: raw success silently leaves cache trusted | **Resolved.** Successful raw passthrough marks control, power, and GDDRAM certainty unknown until resync. | `test_raw_success_invalidates_control_and_resync_restores_flush_admission`, `test_raw_overloads_tick_policy_helpers_and_minimum_capacity` |
| H-10: overly broad transport status and borrowed stored messages | **Resolved.** Transport has a fixed terminal value type with numeric detail; operation state is separate and persistent results use library-owned static status messages. | `test_v4_types_are_fixed_trivial_and_noexcept`, `test_request_ids_are_busy_until_result_is_consumed_and_take_is_once` |
| H-11: fixed input text requires NUL scan/copy | **Resolved.** `drawTextN()` and `getTextWidthN()` accept pointer plus explicit length without over-read. | `test_draw_text_n_and_touch_are_fixed_length_memory_only_and_never_wake` |

## Acceptance Re-Audit Corrections

The 2026-07-19 independent acceptance pass re-read the original report and
found additional gaps in the first v4 implementation. All library-side items
below are closed on the current branch:

| Gap | Resolution and evidence |
| --- | --- |
| Rebind could discard active/unconsumed provenance | `attach()` now returns zero-I2C `BUSY` until completion/result consumption; `detach()` is the explicit discard. `test_rebind_preserves_active_and_terminal_operation_provenance`. |
| Result-pending shutdown returned lifecycle errors before `BUSY` | Direct and cooperative-scroll admission now gives result provenance precedence. Covered by the same rebind/provenance test. |
| Zero-I2C timeout/empty flush changed health | Health publication requires at least one transport callback. `test_no_i2c_terminal_outcomes_do_not_change_transport_health`. |
| Cooperative flush/resync could publish health both in the nested flush and at operation completion | Nested flush accounting is suppressed while an owner operation is active; the terminal operation publishes exactly once. `test_cooperative_operations_publish_health_once_at_terminal_result`. |
| Blocking clock stall bypassed uncertainty policy | Deadline, cancellation, and stalled-clock termination share one state/effect policy. `test_blocking_clock_stall_marks_command_state_uncertain`. |
| `firstPage()` could erase active retry state | It now returns `Status` and refuses operation/result/flush ownership conflicts. `test_pure_flush_cancellation_preserves_retry_and_first_page_is_busy`. |
| Failed page iteration and fresh-window coverage were under-specified | A failed window remains selected and blocks reset until retry; every fresh window is fully dirty even after partial drawing. `test_page_buffer_tick_preserves_error_for_next_page_retry`, `test_partial_page_render_flushes_full_windows_before_wake`. |
| Page/result helpers could bypass consume-once ownership | `nextPage()` does not advance, and `clearDirtyIfIdle()` returns `BUSY`, while cooperative work/result provenance is retained. `test_active_or_unconsumed_operation_blocks_direct_i2c_and_legacy_flush_paths`, `test_partial_page_render_flushes_full_windows_before_wake`. |
| Ambiguous raw failure left modeled state trusted; direct wake ignored dirty controls | Address NACK retains definite-no-effect state; data NACK/timeout/bus error invalidates control, power, and GDDRAM. Direct wake requires clean controls. `test_raw_failure_certainty_and_direct_wake_control_gate`. |
| Legacy scroll duplicated the three-command sequence | Blocking compatibility scroll now admits and runs the cooperative operation state machine. Golden command and phase-fault tests cover both entry points. |
| Write-only cache was described as hardware-verified | Public state is explicitly command-confirmed/modeled; cached booleans are qualified by dirty/power state. No SSD1315 readback is claimed. |
| Vertical-scroll A3 constraints/state were incomplete | Config enforces `startLine < height`; area changes enforce `startLine < scrollRows`; init/resync physically restores A3 `(0,height)`, fade-off, zoom-off, and scroll-off in the 17-callback sequence. Golden init and invalid-input tests cover it. |
| Wake could expose stale/incomplete GDDRAM | A fixed baseline invariant requires a complete full-buffer transfer or completed page iteration plus no dirty data. `test_wake_requires_complete_gddram_baseline_not_only_clean_dirty_bits`. |
| Framebuffer mutation after wake admission or during resync could precede stale display-on | The clean-baseline invariant is rechecked immediately before `DISPLAY_ON`; local rejection performs no display-on and does not degrade transport health. `test_display_on_rechecks_gddram_after_admission_and_resync_flush`. |
| Page iteration advanced after a generation-changing mutation during transfer | `nextPage()` retains the same window and dirty data until a retry succeeds. `test_page_iteration_retries_mutation_before_advancing_window`. |
| Byte-budget test could overflow its fake log without checking payload | The fake log is sized for the 1,058-callback worst case and every poll asserts callback and actual data-payload budgets. Minimum-capacity and exact 42/50/378 callback cases are covered. |
| CI/example did not compile the primary owner path | Native IDF flush uses start/poll/result with one callback per poll; its contract guard checks those calls. ESP-IDF CI is pinned to exact `v5.3.5`. |

The original non-library cleanup observations remain explicit: stale Power-page
row naming is already corrected in TunnelMonitor's authoritative operator UI
documentation. `OptionalJobStep::DisplayFrameOffCommand` remains unreachable
and is intentionally deferred for removal when the direct TunnelMonitor OLED
protocol is replaced; changing it now would be unrelated application churn.

## TunnelMonitor Contracts That Already Fit

- `I2cTask` is the sole I2C owner
  (`docs/guidelines/i2c_peripherals.md:28-35,100-135`).
- Hardware revision 2.0 uses ESP32-S3, SDA GPIO8, SCL GPIO9, 400 kHz, OLED
  address `0x3C`, and no declared OLED reset pin
  (`include/TunnelMonitor/BoardPins.h:17-30,71-87` and
  `include/TunnelMonitor/i2c/I2cConfig.h:9`).
- The display contract is fixed-capacity: 128x64, 1024 visible bytes, eight
  pages, and 128-byte page data
  (`include/TunnelMonitor/contracts/EnvPowerDisplay.h:50-78,147-160,193-204`).
- Result identity uses exact slot/token ownership and one-shot take/reclaim
  (`include/TunnelMonitor/i2c/I2cDiagnostics.h:183-246` and
  `src/i2c/I2cDiagnostics.cpp:240-365`).
- Page selection, text formatting, button handling, priority, deadlines,
  inactivity sleep, refresh cadence, device health, and logging remain
  TunnelMonitor policy. SSD1315 types must stay behind a private owner adapter.

## Remaining TunnelMonitor Integration Gates

These are firmware-side gates; the SSD1315 v4 refactor does not resolve them:

1. Display work has a 2500 ms operation deadline, while protected result slots
   are reclaimed at 1250 ms
   (`EnvPowerDisplay.h:54`, `I2cDiagnostics.h:16`, and
   `DisplayService.cpp:642-727`). Reclaim does not cancel owner work, and a late
   completion is dropped (`I2cDiagnostics.cpp:299-365`). Reconcile these
   contracts before adapting the library.
2. Current direct OLED transfers opt into the generic retry/recovery path,
   including the 129-byte page write (`I2cTask.cpp:2487-2860,3088-3146`). The
   future private adapter must permit at most one physical transaction per
   callback and must not replay an ambiguous display write.
3. Select and immutable-pin a released SSD1315 revision only after library CI,
   package checks, and the exact Arduino/ESP-IDF targets pass.
4. Measure the production static-RAM change for a private 1024-byte external
   framebuffer. Keep allocation, ownership, and lifetime inside `I2cTask`.
5. Map library operation identity/result/effect/power state into existing
   private owner state without changing public TunnelMonitor command/result
   layouts.

Do not use blocking `begin()` or `recover()` from `I2cTask`, raise the health
threshold to evade owner policy, hide retry/recovery in the callback, copy
protocol bytes into application code, or expose SSD1315 types publicly.

## Required Validation Before Integration

Library release validation must include the native suite and guard scripts,
Arduino ESP32-S2/S3 builds, native ESP-IDF ESP32-S2/S3 builds, package/version
checks, and Doxygen review. Record exact results only after those commands run on
the release candidate.

TunnelMonitor integration validation must prove:

- one owner poll with budget one causes at most one backend attempt;
- the original 64-bit admission deadline includes queue wait and is never
  renewed by the adapter;
- per-attempt timeout is clipped to remaining owner time;
- cancellation and result reclamation cannot leave owner work executing under a
  reused identity;
- absent OLED remains optional and RTC/FRAM/other I2C work continues;
- every ambiguous failure terminates without blind replay and a later request
  starts from known addressing;
- the exact production firmware build, queue stress, and mixed-device traffic
  pass with the immutable dependency revision.

Physical HIL on hardware revision 2.0 must record the exact module/controller,
supply and level compatibility, pull-ups, reset wiring, IREF/power profile,
orientation, four text rows and spacer rows, initial blank-to-frame transition,
sleep/wake, absence/reconnect, safe fault cases, mixed 400 kHz bus load, watchdog
liveness, and soak duration. Retained COM29 serial evidence used an ESP32-S2
Arduino target with unknown panel/electrical/reset facts and skipped visual and
fault checks; it does not qualify the v4 library or a new TunnelMonitor adapter.

## Final Assessment

The v4 core now provides the passive, bounded chip-level mechanism needed by a
sole external bus owner. H-02 through H-11 have implementation and test
traceability, subject to final release-candidate validation.

Integration nevertheless remains deferred. H-01, the 2500/1250 ms
deadline/result-lifetime conflict, retry-capable current display writes,
dependency pinning, exact production builds, and representative hardware HIL
must be resolved before the direct TunnelMonitor renderer is replaced.
