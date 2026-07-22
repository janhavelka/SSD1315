# TunnelMonitor-node Integration Gates

Status reviewed: 2026-07-21

Integration remains deferred. The SSD1315 v4 library-side findings H-02 through
H-11 are resolved and covered by the current public contracts and native suite;
they are not repeated here. H-01 and the application-side ownership, timing,
dependency, build, and hardware gates below remain open.

The reviewed native suite passed 118 of 118 tests. These are local software
results, not native ESP-IDF, TunnelMonitor integration, or hardware
qualification. Current library behavior is maintained in the README, public
Doxygen, changelog, and tests rather than duplicated in this target-specific
gate document.

## Authoritative Target Evidence

TunnelMonitor was last inspected read-only at clean `develop` revision
`14844c18b6e239baf9865df0e2ffccb6d91dde49`. No TunnelMonitor source,
dependency, or documentation was changed by the library audit.

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
- Planned platformization Prompt 45N describes the future private adapter, but
  explicitly stops on unknown controller authority and retains the 2,500/1,250
  ms lifetime gate. It is a plan, not an implemented dependency decision.

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
3. **Remove retry ambiguity at the adapter boundary.** Current direct OLED
   writes use the generic retry/recovery path, including the 129-byte page
   write. The SSD1315 callback must permit at most one physical transaction and
   must never replay an ambiguous write.
4. **Select an immutable dependency.** The plan names reviewed full commit
   `6040c6cb51841ac268c9a7a50ceda4dbbf8072fb` or an identical immutable v4
   release, while the dependency policy still says deferred. Prove the selected
   revision is remotely fetchable and passes library CI, package checks, and
   exact Arduino and native ESP-IDF targets before implementation.
5. **Keep ownership private and deterministic.** Place the 1,024-byte external
   framebuffer and SSD1315 adapter state inside `I2cTask`, measure the production
   static-RAM change, and do not expose SSD1315 types through public
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
liveness, and soak duration. The historical COM29 run used ESP32-S2 with unknown
panel/electrical/reset facts and no visual or physical fault evidence; it does
not qualify v4 or a TunnelMonitor adapter.

## Decision

The v4 library now supplies the passive, bounded mechanism required by a sole
external bus owner. TunnelMonitor adoption remains blocked until every open gate
above is resolved and recorded with exact build and hardware evidence.
