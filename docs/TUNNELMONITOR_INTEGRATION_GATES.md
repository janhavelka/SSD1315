# TunnelMonitor-node Integration Gates

Status reviewed: 2026-07-22

Integration remains deferred. The SSD1315 v4 library-side findings H-02 through
H-11 are resolved and covered by the current public contracts and native suite;
they are not repeated here. H-01 and the application-side ownership, timing,
dependency, build, and hardware gates below remain open.

The reviewed native suite passed 118 of 118 tests. These are local software
results, not native ESP-IDF, TunnelMonitor integration, or hardware
qualification. Current library behavior is maintained in the README, public
Doxygen, changelog, and tests rather than duplicated in this target-specific
gate document.

The clean TunnelMonitor `4d7555a` native baseline also passed 1,109 of 1,109
tests in a detached worktree. Those tests cover the existing direct display,
selected-device settings, and I2C owner contracts, not the future SSD1315
module.

## Authoritative Target Evidence

TunnelMonitor was last inspected read-only at `prompt-45-platformization`
revision `4d7555a2306b38032d7f6cbb15ccb29674fcecca` after Prompt 45F. Its exact
committed state was tested in a clean detached worktree. Changes after the
earlier `710d3ac` baseline were Cloud/profile work and did not alter the
I2C/display contract. The live checkout later contained uncommitted Prompt-45G
I2C-owner work; those user changes were neither built nor modified. No
TunnelMonitor source, dependency, or documentation was changed by the library
audit or COM21 test session.

The target contracts already establish:

- `I2cTask` is the sole I2C owner.
- Hardware revision 2.0 uses ESP32-S3, SDA GPIO8, SCL GPIO9, 400 kHz, OLED
  address `0x3C`, and no declared OLED reset pin.
- The display contract is fixed at 128x64, 1,024 visible bytes, eight pages,
  and 128 data bytes per page.
- Result identity uses fixed slot/token ownership with one-shot take/reclaim.
- Page selection, formatting, buttons, priority, deadlines, inactivity sleep,
  refresh cadence, health, and logging remain TunnelMonitor policy.
- The authoritative dependency policy still records SSD1315 integration as
  deferred.
- Planned platformization Prompt 45L owns the future private SSD1315 module.
  The preceding Prompt 45G through 45K owner/device migrations and controller
  authority are prerequisites; this is a plan, not an implemented dependency
  decision.

These facts do not establish the panel controller or electrical profile.
Address ACK proves only that a device acknowledged `0x3C`.

## Open Gates

1. **Identify the exact panel.** Record the module and controller, supply and
   level compatibility, pull-ups, reset wiring, IREF mode, charge-pump/external
   supply selection, COM pins, segment/COM orientation, and analog defaults.
   The library currently sends SSD1315-specific `SET_IREF`; SSD1306
   compatibility is not claimed.
2. **Reconcile result lifetime with operation lifetime.** TunnelMonitor permits
   a 2,500 ms display operation but reclaims protected result slots after
   1,250 ms. Reclaim does not cancel owner work, and late completion is dropped.
   Cancellation and identity reuse must be made safe before adaptation.
3. **Remove retry ambiguity at the private module callback boundary.** Current
   direct OLED writes use the generic retry/recovery path, including the 129-
   byte page write. The SSD1315 callback must permit at most one physical
   transaction and must never replay an ambiguous write.
4. **Select an immutable dependency.** The dependency policy still says
   deferred. Prompt 45L must select an immutable release/full commit, prove it
   is remotely fetchable, and record passing library CI, package checks, and
   exact Arduino and native ESP-IDF targets before implementation.
5. **Keep ownership private and deterministic.** Place the 1,024-byte external
   framebuffer and SSD1315 composition in the private `Ssd1315Module`; keep
   generic `I2cTask` free of display/device protocol state. The module callback
   may submit only one generic owner transaction per invocation. Measure the
   production static-RAM change and do not expose SSD1315 types through public
   TunnelMonitor command/result layouts.

Do not call blocking `begin()` or `recover()` from the owner task, hide a retry
or bus-recovery loop inside the callback, recursively acquire the owner lock,
renew the admission deadline, copy SSD1315 protocol bytes into application
code, or use the diagnostic `OFFLINE` threshold as owner policy.

## Validation Required Before Adoption

The immutable library candidate must pass its native suite, guard scripts,
Arduino ESP32-S2/S3 builds, native ESP-IDF ESP32-S2/S3 builds, package/version
checks, and warning-free Doxygen generation.

The TunnelMonitor integration must then prove:

- one owner poll with budget one causes at most one backend attempt;
- the original 64-bit deadline includes queue wait and is never renewed;
- per-attempt timeout is clipped to remaining owner time;
- cancellation/reclaim cannot leave work executing under a reused identity;
- absence or ambiguous OLED failure does not block RTC, FRAM, or other devices;
- a later request re-establishes known SSD1315 addressing without blind replay;
- exact production builds, queue stress, and mixed-device traffic pass.

Representative hardware revision 2.0 HIL must record the identified panel and
electrical setup, four text rows and spacer rows, blank-to-first-frame behavior,
sleep/wake, absence/reconnect, safe fault cases, mixed 400 kHz traffic, watchdog
liveness, and soak duration. The COM21 v4 serial run used TunnelMonitor HW2.00
ESP32-S3 hardware and observed four ACKing shared-bus addresses, but it used the
standalone blocking diagnostic with unknown panel/electrical facts and no visual
or physical fault evidence. It does not qualify a production TunnelMonitor
module. The COM29 run is historical pre-v4 evidence only.

## Decision

The v4 library now supplies the passive, bounded mechanism required by a sole
external bus owner. TunnelMonitor adoption remains blocked until every open gate
above is resolved and recorded with exact build and hardware evidence.
