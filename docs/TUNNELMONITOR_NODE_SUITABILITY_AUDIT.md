# TunnelMonitor-node suitability audit

## SSD1315 OLED display library

Date: 2026-07-18

Audit result: **good base, not suitable unchanged**

SSD1315 v3.0.0 has a solid protocol core. It is framework-neutral, supports a
caller-owned framebuffer, has bounded dirty tracking, and its explicit flush
poll can perform one I2C transaction at a time. Those parts are worth keeping.

It is not a drop-in fit for TunnelMonitor. Initialization, recovery, some draw
calls, automatic sleep, and destruction can perform hidden or multi-transaction
I2C work. The library also owns an `OFFLINE` policy that conflicts with
TunnelMonitor's sole I2C owner. Its fixed 64-byte data chunks do not match the
current screened 128-byte page writes.

The recommended path is a focused library refactor followed by a private
`I2cTask` adapter. Do not build a TunnelMonitor adapter that works around the
current lifecycle one special case at a time. Until the refactor and hardware
qualification are complete, the current direct display implementation is the
safer production path.

## Audit basis

The audit used these exact revisions:

| Repository | Revision | Notes |
| --- | --- | --- |
| TunnelMonitor-node | `fff99fe17e60b9287ec4d8d3eca5b3230ae44223` | Branch `prompt-44b-sequence`; current architecture and direct OLED path |
| SSD1315 | `1744a3720f30f0b2b19b5d7673b4b1dfccc52af4` | `main`, `origin/main`, and tag `v3.0.0` all resolved to this commit during the audit |

Unless stated otherwise, SSD1315 source references below mean v3.0.0 at the
commit above. TunnelMonitor references mean the revision above.

The chip facts were checked against:

- `docs/SSD1315_datasheet.pdf`, especially PDF pages 15, 16, 24, and 33;
- `docs/Wisevision_X096-2864KSWPG01-H30_module_spec.pdf`, PDF pages 19 to 21,
  only as an example module specification; and
- the library's extracted command, datasheet-alignment, readiness, and HIL
  documents.

This was a suitability audit. It did not change firmware or library code,
select a dependency revision, or run new physical hardware tests.

## Latest branch revalidation

Revalidated after `git fetch origin --prune --tags` on 2026-07-18:

- GitHub reports `main` as the remote default branch.
- `origin/main@1744a372` is the newest remote branch tip by commit date. The
  next newest branch is
  `origin/fix/ssd1315-industrial-gap-closure@ccb5f87` from 2026-06-01.
- The local checkout was already `main@1744a372`, exactly aligned with
  `origin/main` with ahead/behind `0/0`. Only this audit report was untracked.
- The audited revision and final checkout are identical, but the public API,
  core implementation, tests, profiles, and lifecycle documentation were
  re-read at the final HEAD. This was not only a commit-label comparison.

| Finding | Recheck against final `main` |
| --- | --- |
| H-01 | Confirmed: the library still sends the SSD1315-specific IREF command and its profiles do not establish the actual TunnelMonitor module or legacy `0x40` VCOMH setting. |
| H-02 | Confirmed: `begin()` and `recover()` still execute synchronous probe, initialization, optional clear, and display-on transactions. |
| H-03 | Confirmed: disabling synchronous clear still turns the display on before a later cooperative first-frame flush. |
| H-04 | Confirmed: framebuffer mutations can wake synchronously, `tick()` can combine flush and auto-sleep I2C, and destruction calls active `end()`. |
| H-05 | Confirmed: the core still owns `DEGRADED`/`OFFLINE` admission and library recovery policy. |
| H-06 | Confirmed: flush data remains capped by private `FLUSH_DATA_CHUNK_BYTES = 64`, independent of the public byte budget. |
| H-07 | Confirmed: the synchronous write callback still lacks a terminal, one-physical-attempt/no-retry contract. |
| H-08 | Confirmed: flush timing still starts during polling and there is no zero-I2C cancellation API. |
| H-09 | Confirmed: successful raw command calls still leave cached modeled control state marked clean. |
| H-10 | Confirmed: transport callbacks still return general `Status`, including `IN_PROGRESS`, and persistent snapshots still copy borrowed message pointers. |
| H-11 | Confirmed: `drawText()` still accepts only a null-terminated string and scans to an internal bound rather than accepting caller length. |

The native suite was re-run on the final HEAD: 97 of 97 tests passed.

## Decision summary

### Use after a focused refactor

The following are release gates for a TunnelMonitor integration:

1. Confirm the exact OLED module, controller, supply arrangement, IREF mode,
   orientation, and reset wiring on hardware revision 2.0.0. Add a named panel
   profile for that exact hardware.
2. Split passive configuration from I2C work. Binding a transport, profile, and
   caller-owned buffer must perform zero I2C transactions.
3. Replace blocking initialization and recovery with fixed cooperative jobs.
   One owner poll with budget one must cause at most one transport callback.
4. Make drawing and dirty tracking memory-only. Make sleep, wake, display-on,
   initialization, frame transfer, and shutdown explicit owner-scheduled jobs.
5. Remove the mandatory library-owned `OFFLINE` admission policy and recovery
   policy from the core.
6. Make detach and destruction perform no I2C.
7. Support and validate the selected transport capacity. TunnelMonitor's
   current shape is 128 data bytes plus the `0x40` control byte, or 129 total
   bytes per page-data write.
8. Define a terminal, single-attempt transport contract. A callback must not
   retry a possibly partial OLED data write.
9. Add explicit cancellation that preserves dirty data. TunnelMonitor keeps the
   original 64-bit admission deadline; the library must not renew it.
10. Prove the refactored result in native tests, the exact TunnelMonitor
    ESP32-S3 build, visual HIL on the real module, and mixed-device bus stress.

### Do not solve this with configuration tricks

These are not acceptable long-term fixes:

- setting `clearOnBegin=false` and accepting the remaining blocking init
  sequence;
- setting `offlineThreshold` very high;
- repeatedly calling `begin()` after absence or failure;
- calling blocking `recover()` from an `I2cTask` poll;
- letting the transport callback perform bus recovery and replay a timed-out
  data write;
- silently changing a 128-byte page write into two 64-byte writes;
- copying the current TunnelMonitor init bytes into a profile called
  `Generic` without confirming the module;
- using raw command escape hatches to repair cached library state;
- using heap allocation for the TunnelMonitor framebuffer; or
- exposing SSD1315 types in TunnelMonitor public contracts.

## TunnelMonitor requirements

The library must fit the existing resource-owner model. The firmware should not
weaken that model to fit the library.

| Requirement | Current authority or evidence | Consequence for SSD1315 |
| --- | --- | --- |
| One I2C owner | `docs/guidelines/i2c_peripherals.md:28-34`; `docs/guidelines/target_architecture.md:299-312` | Only `I2cTask` may call the library transport. The library must not initialize the bus, lock it, recover it, retry it, or own device health. |
| Passive dependency | `docs/guidelines/dependency_policy.md:27-40,71-90` | Binding/configuration must perform no I2C. Optional display absence must leave the object usable for a later attempt. |
| Fixed bus and address | `include/TunnelMonitor/BoardPins.h:26-30,71-82`; `include/TunnelMonitor/i2c/I2cConfig.h:9` | ESP-IDF I2C, SDA GPIO8, SCL GPIO9, 400 kHz, address `0x3C`, no wired OLED reset pin. No `Wire` dependency. |
| Optional device | `docs/guidelines/operator_ui.md:24-41,189-195`; `src/i2c/I2cDiagnostics.cpp:45-54` | OLED absence/failure must not fail aggregate I2C health, block boot, or stop RTC/FRAM work. |
| Fixed display contract | `include/TunnelMonitor/contracts/EnvPowerDisplay.h:54-78,147-160,193-204` | 128x64, eight pages, 1024 visible bytes, four fixed 15-byte text lines, a 64-byte public render command, and an eight-data-chunk result. |
| Full visible refresh | `docs/guidelines/cli.md:337-341`; `docs/guidelines/operator_ui.md:62-69` | Each visible render rewrites all eight pages. Partial dirty optimization is optional internally and must not change the visible contract. |
| Stepped transfers | `docs/guidelines/i2c_peripherals.md:100-138,454-476` | Normal work advances by one backend call per owner poll. Each transfer has a 20 ms timeout and the whole admitted display operation has a 2500 ms deadline. |
| Current screened transfer shape | `src/i2c/I2cTask.cpp:3017-3159`; `docs/reports/hil-testing/condensed/i2c_400khz_queue8_display128_stress_20260705.md:88-97` | Per page: one address write, then one 129-byte transaction containing control `0x40` and 128 data bytes. Initial render is 18 transactions; later render is 17. |
| External UI policy | `src/display/DisplayService.cpp:563-666`; `docs/guidelines/operator_ui.md:33-41` | Page selection, text, refresh cadence, inactivity sleep, button handling, priority, status, and deadlines remain in `DisplayService` and `I2cTask`. |
| Owner-private dependency | `docs/guidelines/reference/dependencies.md:328-353` | Library types and errors are mapped at the private adapter. They do not enter public commands, results, CLI, web, or service contracts. |

The current renderer generates bytes on demand and uses a 129-byte stack
transfer buffer (`src/i2c/I2cTask.cpp:793-808,3087-3109`). A library integration
therefore needs an explicit memory decision; it is not a zero-cost replacement.

## What already fits

These v3 properties should be preserved:

- Core headers and source are independent of Arduino, ESP-IDF, and `Wire`.
- Bus access is injected through a non-owning synchronous callback.
- Copy and move are deleted (`include/ssd1315/SSD1315.h:120-124`).
- An external framebuffer can be supplied and size-checked. With that path,
  steady-state operation does not need heap allocation.
- Full-buffer and page-buffer modes are present.
- Dirty pages and dirty column ranges are fixed-capacity. Generation tracking
  avoids clearing new dirty state when a flush overlaps framebuffer changes.
- A failed or partial flush retains dirty data for a later request.
- A restarted flush reissues its column and page setup before data.
- `pollFlush(now, 1, budget)` performs at most one command or data callback.
- Flush progress and terminal state are observable through `FlushStatus`.
- Fixed stack buffers are used; no dynamic STL container was found in core
  steady paths.
- The configured controller profile is explicit. The library does not claim it
  can identify an SSD1315 over I2C.
- Probe documentation is honest that an ACK is presence evidence, not product
  identity.
- Native tests cover geometry, dirty tracking, partial failures, retries,
  profiles, control-state uncertainty, and cooperative flush behavior.

## Hard findings

### H-01: the exact panel profile is not confirmed

Priority: integration blocker

TunnelMonitor calls the display "SSD1315/SSD1306-compatible" and uses a fixed
128x64 sequence at `src/i2c/I2cTask.cpp:2846-2851`. That sequence selects page
addressing (`0x20 0x02`), segment/com orientation `A0/C0`, precharge `0xF1`,
VCOMH `0x40`, and does not send SSD1315 `SET_IREF`.

The library is intentionally SSD1315-only. Its init path uses horizontal
addressing (`0x20 0x00`), sends `SET_IREF 0xAD`, defaults to precharge `0x22`,
and its current VCOMH enum does not express the legacy `0x40` value
(`include/ssd1315/Config.h:271-302`; `src/SSD1315.cpp:877-955`). The Wisevision
profile also uses different orientation and electrical values.

These differences are not proof that either sequence is wrong. They show that
the physical module and its power circuit must be identified before choosing a
profile. Address ACK cannot identify the controller. The controller datasheet
also allows a circuit where SDAOUT is not connected, in which case ACK is not
available; mandatory ACK probing is not universal hardware proof.

Required action:

- Record the module manufacturer and ordering code in the hardware authority.
- Confirm controller, internal/external charge pump, IREF topology, orientation,
  address strapping, reset wiring, and supply sequencing from that module's
  specification and schematic.
- Add one named profile for the confirmed module. Keep raw register values out
  of a vague `Generic` profile.
- Validate orientation, contrast, full-frame alignment, power-on, and sleep/wake
  on actual hardware.
- Do not send SSD1315-only commands to hardware that is only assumed to be
  SSD1306-compatible.

### H-02: `begin()` and `recover()` are blocking, active lifecycle calls

Priority: architecture blocker

`begin()` validates and stores configuration, probes the device, allocates or
attaches a framebuffer, sends the init sequence, optionally clears GDDRAM, and
turns the display on (`src/SSD1315.cpp:644-794,877-1009`). The README documents
53 I2C transactions and 1112 bytes for the default 128x64 begin. Disabling the
clear still leaves about 19 synchronous transactions.

`recover()` repeats probe/configuration work synchronously
(`src/SSD1315.cpp:601-637`). On a failed begin, runtime configuration and the
buffer binding are cleared. An absent optional display therefore cannot remain
passively configured for a later owner-directed initialization attempt.

TunnelMonitor permits one normal display backend call per owner poll. It also
starts the display service in an asleep/passive state and explicitly requests
sleep from `App` (`src/app/App.cpp:287-298`). Active `begin()` cannot be called
as ordinary construction or setup.

Required refactor:

- Add zero-I/O `attach()` or `configure()` that validates the profile,
  transport, limits, and caller-owned buffer.
- Keep the binding after NACK, timeout, cancellation, and failed init.
- Add a fixed cooperative initialize/resync job. With a budget of one, each
  poll performs at most one callback.
- Make probe an explicit optional operation, not a hidden precondition for
  passive configuration.
- Keep a blocking compatibility facade only outside the passive core, if
  standalone users still need it.

### H-03: first-frame presentation cannot be both cooperative and clean

Priority: integration blocker

With `clearOnBegin=true`, the library clears GDDRAM synchronously, which violates
the owner budget. With `clearOnBegin=false`, initialization turns the display on
before the later cooperative flush. Old or random GDDRAM can therefore become
visible before TunnelMonitor's first full frame. `displayOnDelayMs` delays later
work but does not replace a staged first-frame sequence.

The current TunnelMonitor path is safer: it keeps the display off, sends all
eight page writes, then sends `DISPLAY_ON` (`0xAF`). The SSD1315 datasheet also
documents a 100 ms display-on timing interval.

Required refactor:

- Let initialization finish in a stable `ReadyOff` state.
- Flush or clear the complete first frame cooperatively while the panel remains
  off.
- Send `DISPLAY_ON` as its own scheduled transfer after the frame is complete.
- Represent the 100 ms interval as a non-blocking phase.
- On wake after an error or uncertain control state, use the same
  initialize-off, full-frame, display-on sequence.

A simple target sequence is:

```text
Attached -> InitializingOff -> ReadyOff -> FullFrameFlush -> DisplayOnDelay -> Ready
```

### H-04: drawing, `tick()`, and destruction can perform hidden I2C

Priority: architecture blocker

Framebuffer drawing is not purely memory work. `clear`, `fill`, and drawing
operations can call `wakeIfSleeping()`, which writes to I2C synchronously
(`src/SSD1315.cpp:1195-1215,1938-1948,2026+`). Several draw APIs return `void`,
so a wake failure cannot be returned directly.

`tick()` can advance one flush transfer and then also perform automatic sleep
work in the same call (`src/SSD1315.cpp:797-815,1259-1294`). This breaks the
one-callback owner budget. A failed wake can also be attempted repeatedly from
one bounded text draw, especially when no time hook is available. Drawing or
library timers must not change power state when TunnelMonitor's
`DisplayService` owns inactivity policy.

The destructor calls `end()`. `end()` sends display-off and possibly pump-off
writes before releasing the buffer (`src/SSD1315.cpp:295-297,818-874`). This can
touch a bus after the owner or callback context has already been destroyed, and
the destructor cannot report failure.

Required refactor:

- Make all framebuffer mutation and dirty tracking memory-only.
- Remove automatic wake from drawing.
- Separate flush and control/power progress. One poll budget covers all jobs,
  not one transaction per internal subsystem.
- Disable or remove core-owned auto-sleep and page-cycling policy. If retained
  for standalone use, put it in an optional facade above the passive core.
- Make destructor and detach zero-I/O.
- Provide an explicit cooperative sleep/shutdown job that the owner can run
  before teardown.

### H-05: library-owned `OFFLINE` and recovery conflict with `I2cTask`

Priority: architecture blocker

The library counts failures, enters `DEGRADED`/`OFFLINE`, blocks normal work
after `offlineThreshold`, and requires library `recover()` to resume
(`include/ssd1315/Status.h:176+`; `src/SSD1315.cpp:303-351,436-459`).

TunnelMonitor already owns device presence, health, recovery admission, bus
recovery, deadlines, and optional-device reporting. A second latch can refuse a
valid owner request after the physical bus has been recovered. It can also make
an absent display influence behavior independently of the project's health
model.

Required refactor:

- Remove health-based transport gating from the passive core.
- Do not let a previous failure block a later explicit owner operation.
- Keep counters only as diagnostics if they never change admission.
- If standalone applications need managed health, place that policy in a
  separate optional facade.

### H-06: the flush payload cap does not match the screened page write

Priority: required compatibility decision

The public byte budget defaults to 128, but private
`FLUSH_DATA_CHUNK_BYTES` caps actual framebuffer data at 64 bytes
(`src/SSD1315.cpp:261,1435-1448`). A full 128x64 library flush therefore uses 16
data transactions plus address commands. The current TunnelMonitor transfer is
eight data transactions, each 129 bytes on the wire.

Sixty-four-byte chunks are bounded and have passed older TunnelMonitor direct
driver HIL. They are technically viable, but silently changing the shape would
invalidate the current transfer-count, latency, result, and HIL evidence. A
byte budget that cannot reach its documented value is also misleading.

Required refactor:

- Add explicit transport limits, including maximum total write size.
- Derive maximum data payload as total capacity minus the one-byte control
  prefix.
- Support at least 129 total bytes for the selected TunnelMonitor profile, or
  make a deliberate project-level decision to change the firmware contract and
  requalify it.
- Reject incompatible profile/buffer/transport combinations before I2C.
- Make progress report data bytes and data chunks separately from total I2C
  transactions.

### H-07: callback retry semantics are unsafe for OLED data

Priority: correctness blocker

`I2cWriteFn` describes a synchronous transaction, but it does not state that
the callback is exactly one physical attempt with no hidden retry. Current
TunnelMonitor display phases call a retry-capable owner helper. On a timeout,
the controller may already have accepted part or all of the payload and
advanced its GDDRAM column pointer. Replaying the same data bytes without first
restoring the address window can shift or corrupt the frame.

The library's dirty retention and address reissue on a new flush are good. They
only remain safe if the callback does not replay the ambiguous transaction.

Required refactor and integration rule:

- Define the callback as exactly one completed physical attempt.
- Forbid internal bus recovery, retry, delay, or a second transfer.
- Require a terminal result. `IN_PROGRESS` is not a valid callback result.
- On any ambiguous data failure, stop the job and keep the affected range
  dirty.
- Let `I2cTask` recover the bus. A later new flush must reissue address setup
  before sending data.
- Add fake-transport tests for a callback that accepted a prefix before
  returning timeout.

### H-08: flush cancellation and deadline ownership are incomplete

Priority: integration blocker

The flush timer starts at the first `pollFlush()` call, not at command
admission, and there is no public cancel operation
(`src/SSD1315.cpp:1295-1676`). TunnelMonitor's 2500 ms deadline includes queue
wait and uses wrap-safe 64-bit time. If that deadline expires first, the library
can remain busy with an operation that the owner has already terminated.

Required refactor:

- Add `cancel()`/`abort()` with no I2C side effect.
- Preserve or re-mark all not-confirmed dirty data.
- Return a terminal cancelled/deadline code and stable progress snapshot.
- Allow the passive core's internal timeout to be disabled when an external
  owner supplies the deadline.
- Never restart or renew the owner's deadline when an internal phase changes.

There is also an existing TunnelMonitor issue outside the library: display
commands declare a 2500 ms operation deadline, but `DisplayService` reclaims a
protected foreground result after the generic 1250 ms cutoff
(`include/TunnelMonitor/i2c/I2cDiagnostics.h:16`;
`src/display/DisplayService.cpp:321-338,563-648`). Before integration, reconcile
those values or prove and document a smaller admitted completion bound. The
library must not compensate by inventing another deadline.

### H-09: successful raw commands can invalidate cached state silently

Priority: required API cleanup

`sendCommand*` and `sendCommandList` mark `controlStateDirty` when a transfer
fails, but a successful raw command is allowed without invalidating cached
addressing, sleep, scroll, or control assumptions (`src/SSD1315.cpp:1028-1086`).
A caller can therefore change memory mode or power state while the driver still
believes its cached state is valid.

Required refactor:

- Keep normal chip controls as typed operations.
- Remove raw command methods from the safe core surface, or clearly mark them
  unsafe and set all affected control state to unknown after every unmodeled
  command, including successful ones.
- Require a full cooperative resync before the next normal flush when control
  state is unknown.

### H-10: the transport status type is too broad

Priority: required API cleanup

The callback returns the library's general `Status`. That type includes
`IN_PROGRESS`, although the callback must complete synchronously. The library
can therefore receive a nonterminal result in a terminal transport position.

`Status` also stores `const char* msg` and the library caches callback status in
last-error, flush, control, and settings snapshots. Static lifetime is only a
documented convention. An adapter can accidentally return a stack or transient
message pointer that later becomes invalid (`include/ssd1315/Status.h:84-168`).

Required refactor:

- Use a small terminal transport result with typed code and optional numeric
  detail.
- Store values, not borrowed message pointers, in persistent state.
- Provide `toString()` for library-owned enum values.
- Map transport codes to TunnelMonitor status only at the private adapter.

### H-11: the current text API is not a clean match for fixed command lines

Priority: nice-to-have compatibility helper

TunnelMonitor lines are fixed 15-byte fields. The library's C-string text API
scans for a terminator, with a large internal bound. An adapter would need to
copy and terminate every line before drawing. That is safe if done carefully,
but it is unnecessary work and an easy boundary mistake.

Recommended helper:

- Add a bounded `drawTextN(x, y, data, length)` or equivalent pointer-plus-size
  overload.
- Never read beyond the supplied length.
- Keep font rasterization separate from page names, status, and application
  formatting.

## Recommended narrow design

Do not add a generic task framework or device manager. Extend the existing core
with one fixed panel operation state machine.

### Passive core boundary

The core should provide operations with these semantics:

```cpp
Status attach(const Config& config);       // validation and binding, zero I2C
Status startInitialize();                  // queue only, zero I2C
Status startFlush();                       // queue only, zero I2C
Status startSleep();                       // queue only, zero I2C
Status startResync();                      // queue only, zero I2C
Status poll(uint32_t nowMs, uint8_t maxInstructions);
Status cancel(CancelReason reason);        // zero I2C, preserve dirty data
void detach();                             // zero I2C
```

Names may differ. The behavior is the important part:

- `maxInstructions=1` means at most one transport callback across all phases;
- configuration and request methods perform no transport work;
- NACK or absence does not erase the binding;
- all jobs are fixed-state and allocation-free;
- no operation retries or recovers the bus;
- the owner may cancel at any phase; and
- progress remains inspectable after terminal completion.

### Small useful types

Add only types that make ownership and diagnostics clearer:

- `TransportResult { TransportCode code; uint32_t detail; }`
- `TransportLimits { uint16_t maxWriteBytes; }`
- `OperationKind { None, Initialize, Flush, Sleep, Resync }`
- `OperationPhase` for the fixed chip-level phases
- `PanelPowerState { Unknown, Off, Starting, On }`
- `OperationProgress { kind, phase, page, bytesCompleted,
  dataChunkCount, transactionCount, terminalCode }`
- `CancelReason` or a typed cancellation status
- one explicit `PanelProfile` entry for the confirmed TunnelMonitor module

Extend the existing `FlushStatus` and profile types where practical. Do not add
a generic scheduler, queue, service registry, widget tree, or application page
model.

### Small useful helpers

Pure helpers that would reduce adapter mistakes are reasonable:

- `requiredFramebufferBytes(width, height)`;
- `maxDataBytesForWriteCapacity(totalBytes)`, accounting for the control byte;
- `validateConfig()` with no I2C;
- `drawTextN()`;
- `markDirtyRect()` and existing `markAllDirty()`;
- `toString()` for error, operation, phase, power, and profile enums; and
- a profile inspection helper that returns geometry and electrical settings by
  value.

The existing unused `I2cWriteReadFn` hook is described as present for API
uniformity but is not used by the SSD1315 core (`include/ssd1315/Config.h:41-48`).
Remove it in the next breaking refactor. A chip library should not carry a
speculative transport hook.

### Buffer recommendation for TunnelMonitor

Use a caller-owned static 1024-byte full framebuffer inside the private I2C
owner. This costs 1 KiB of fixed RAM but is the simplest integration:

- no heap or allocation failure;
- straightforward rendering of four lines at rows 0, 16, 32, and 48;
- explicit clearing of spacer pages;
- simple full-frame retry after an ambiguous failure; and
- no lifetime callback for on-demand pixels during a long owner job.

Measure and record the RAM change in the production build. If 1 KiB is rejected
after measurement, use the existing fixed page-buffer mode or add a bounded
pixel-source callback. Do not implement both integration modes initially.

## What must stay in TunnelMonitor

The library should own SSD1315 protocol, framebuffer state, and fixed chip-level
operation phases. It should not own:

- the I2C bus, bus lock, bus recovery, retries, or transfer timeout policy;
- device presence, aggregate health, watchdog liveness, or logging;
- command queues, priorities, admission deadlines, or result-slot lifetime;
- System/Network/Storage/Uptime page selection and text formatting;
- button debounce, page cycling, refresh cadence, or inactivity sleep policy;
- public `DisplayRenderCommand`, `DisplayRenderResult`, `PublicStatus`, CLI, web,
  or service types; or
- startup sequencing across RTC, FRAM, environment sensor, power monitor, and
  display.

## Validation evidence and gaps

### Results available now

| Check | Result | Scope |
| --- | --- | --- |
| SSD1315 native suite | PASS, 97/97 | Exact v3.0.0 commit audited |
| Arduino ESP32-S3 build | PASS | Resolved Espressif32 platform 54.3.20, Arduino 3.2.0, IDF libraries 5.4.0 |
| Arduino ESP32-S2 build | PASS | Same resolved platform release |
| Core/CLI/IDF guards, version/package checks | PASS | Local scripted checks |
| HIL parser suite | PASS, 21 tests | Parser only |
| ESP-IDF CI | PASS in [exact-commit workflow run 28381102468](https://github.com/janhavelka/SSD1315/actions/runs/28381102468) | Local `idf.py` was unavailable; the exact commit's six-job CI workflow was successful |
| Retained SSD1315 HIL | 8 hours, 755,500 mixed operations, zero reported serial failures | ESP32-S2 Arduino, COM29, older pre-v3 commits |
| Retained TunnelMonitor display transfer stress | PASS | Current direct renderer, not a library adapter |

The SSD1315 HIL report is useful endurance evidence, but it does not close this
integration. It records an unknown panel model, supply, pull-ups, and reset
wiring. Operator visual validation was skipped. It did not test missing or
hot-plugged display, display power/reset faults, physical shared-bus faults, or
the exact v3 release.

The library's `platformio.ini` uses an unpinned `platform = espressif32`. The
resolved version was recorded above, but a release candidate should exact-pin
the platform/toolchain used for qualification, consistent with TunnelMonitor's
dependency policy.

### Required native tests after refactor

At minimum, add tests that prove:

- passive attach performs zero callbacks;
- absent/NACK initialization leaves the object attached and a later init works;
- request methods and cancellation perform zero callbacks;
- every initialize, resync, flush, sleep, wake, and shutdown poll performs at
  most one callback with budget one;
- draw, clear, text, dirty marking, detach, and destruction perform zero I2C;
- no code path performs an internal retry or bus recovery;
- cancellation works at every phase and preserves not-confirmed dirty data;
- callback `IN_PROGRESS` is rejected by type or validation;
- transient message pointers cannot enter stored state;
- transport-capacity validation accepts 129 bytes and rejects too-small values
  before I2C;
- the confirmed panel profile emits the exact reviewed bytes;
- the first visible display-on command occurs only after a complete frame;
- a full frame uses exactly eight 128-byte data chunks when configured for the
  TunnelMonitor transport;
- NACK, timeout, and ambiguous partial acceptance at each address/data/control
  phase terminate cleanly;
- a later retry reissues address setup before data;
- fixed 15-byte text fields never over-read; and
- unrelated fake RTC/FRAM work remains schedulable during display progress.

### Required TunnelMonitor integration tests

After the library refactor:

- keep SSD1315 behind the private `I2cTask` boundary;
- assert the original 64-bit 2500 ms deadline, including queue wait;
- reconcile the current 1250 ms result-slot lifetime mismatch;
- verify the 20 ms per-callback timeout is clipped to remaining owner time;
- prove eight 129-byte page-data writes and the expected init/address/on/off
  transactions, unless the architecture authority deliberately changes;
- verify missing display maps to optional/absent without degrading other I2C
  devices;
- inject failures at every phase and prove no blind replay;
- verify wake after sleep/error performs the required reinit/full frame/on
  sequence; and
- rerun native tests, the production WiFi firmware build, queue stress, and
  mixed RTC/FRAM/environment/power/display traffic.

### Required physical HIL

On the actual hardware revision 2.0.0 and confirmed OLED module:

- confirm controller/module/electrical profile against the BOM and schematic;
- verify correct left/right and top/bottom orientation;
- verify all four text rows and blank spacer rows with no stale pixels;
- verify contrast and no visible random/stale frame during start or wake;
- verify the display does not appear asleep after a normal visible render;
- verify sleep and wake, repeated init, and power cycling;
- verify GPIO14 active-low button behavior and 30 ms debounce;
- verify OLED absence and reconnect behavior;
- run display traffic with RTC, FRAM, environment sensor, and power monitor on
  the shared 400 kHz bus;
- inject NACK/timeout/bus-recovery cases where practical; and
- verify other I2C work and watchdog-safe liveness continue during display
  failure.

Do not claim the retained COM29 serial soak or the current direct-renderer HIL
qualifies a new adapter.

## TunnelMonitor cleanup discovered by this audit

These are firmware documentation or integration issues, not SSD1315 features:

1. Reconcile the 2500 ms display operation deadline with the 1250 ms protected
   result cutoff before changing the implementation.
2. Remove retry-enabled display writes. After an ambiguous data timeout,
   terminate the render and let a later complete render restart from known
   addressing.
3. Current authority and implementation use System/Network/Storage/Uptime, but
   older guideline rows still say Power. Clean the stale rows before visual
   HIL. Keep page content out of the chip library.
4. `OptionalJobStep::DisplayFrameOffCommand` is declared and implemented but no
   normal transition reaches it. Remove the dead phase when replacing the
   direct protocol; do not add off/on flicker to every refresh.

## Recommended implementation order

1. Confirm the physical module and publish its electrical/profile facts in the
   TunnelMonitor hardware authority.
2. Refactor SSD1315 lifecycle into passive attach plus cooperative operations.
3. Remove hidden I2C, core-owned health gating/recovery, and destructor I2C.
4. Tighten the terminal transport contract, cancellation, stored status, and
   raw-command state handling.
5. Add configurable validated write capacity and a 128-byte data path.
6. Add the bounded text overload and focused native tests.
7. Release and immutable-pin a new SSD1315 revision.
8. Reconcile TunnelMonitor's display deadline/result lifetime and disable blind
   display retries.
9. Integrate through one private adapter using a static external framebuffer.
10. Run exact production builds, native integration tests, visual/button HIL,
    and mixed-bus stress before replacing the direct renderer.

## Final assessment

SSD1315 v3.0.0 is a credible library to refactor. Its transport injection,
external buffer, dirty tracking, and bounded flush poll are better foundations
than rewriting the controller protocol in TunnelMonitor.

The current lifecycle and policy layers are the mismatch. They perform work at
times the resource owner cannot schedule, duplicate health/recovery decisions,
and do not preserve the current 128-byte page-transfer contract. The right fix
is to make the SSD1315 core passive and cooperative, then keep TunnelMonitor's
application and bus policy outside it.

After the focused refactor, a named module profile, exact pinning, and hardware
qualification, the library should be suitable for platformized TunnelMonitor
firmware. It should not replace the current direct OLED path before those gates
are complete.
