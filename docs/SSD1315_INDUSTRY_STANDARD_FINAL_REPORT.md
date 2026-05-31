# SSD1315 Industry-Standard Final Report

Date: 2026-05-31  
Branch: `hardening/ssd1315-industry-readiness`  
Evidence baseline commit before this report: `f9f46a5`  
Final report commit: this commit; use `git log -1 --oneline` after commit for the immutable hash.

## 1. Executive Summary

The SSD1315 driver has been materially hardened as an SSD1315-specific
software-contract library. The core remains framework-neutral and
transport-injected; lifecycle blocking, probe limitations, reset ownership,
panel-control dirty state, flush retry behavior, and ESP-IDF example boundaries
are now documented and tested.

This branch is **not field-release complete**. Current-branch GitHub CI
execution is unconfirmed from this environment, local pure `idf.py` builds were
not run because `idf.py` is unavailable, and representative physical SSD1315
hardware validation was not run.

Verdict: **do not merge until CI confirms the branch/PR checks. After CI
passes, this is mergeable as SSD1315 software-contract hardening, not as a
field-release-complete hardware validation.**

## 2. Scope

Included:

- SSD1315-only controller honesty and compatibility wording.
- Golden init/control-byte/flush/scroll/fault tests on host fake transport.
- Lifecycle blocking, reset ownership, probe, dirty-state, and flush contracts.
- Arduino and native ESP-IDF example contract hardening.
- CI workflow configuration for Arduino, native tests, package validation, and
  pure ESP-IDF example builds.
- Hardware-validation matrix and blocked-hardware report.

Excluded:

- Broad SSD1306 compatibility profile.
- Real hardware pass/fail evidence.
- Release version bump, tag, or package publication.
- Core reset GPIO ownership or platform bus ownership.

## 3. What Changed Across Chunks

Chunk 1 froze the production-grade baseline and aligned CI/example paths:

- Confirmed actual ESP-IDF path: `examples/espidf_basic`.
- Verified package metadata and docs are SSD1315-first.
- Recorded local `idf.py` unavailability.

Chunk 2 made the validation command set executable and guarded:

- Standardized `version`, `scan`, `probe`, `cfg`, `selftest`, pattern, clear,
  fill, invert, contrast, flip, scroll, recover, stress, and monitor command
  contracts.
- Removed `contrast 0` from executable validation.
- Strengthened `tools/check_cli_contract.py`.

Chunk 3 aligned datasheet and panel contracts:

- Added `PanelProfile` and `applyPanelProfile()`.
- Restricted SSD1315 addresses to `0x3C`/`0x3D`.
- Rejected contrast `0`.
- Corrected SSD1315 scroll speed labels and scroll setup bytes.
- Blocked framebuffer flush while hardware scroll is active.
- Added best-effort internal charge-pump disable in `end()`.
- Added extensive native golden/fault tests.

Chunk 4 attempted hardware validation but correctly recorded it as blocked:

- `COM5` did not respond to the SSD1315 CLI.
- `COM18` ran unrelated `tunnelmonitor 0.3.0` firmware and was not overwritten.
- No SSD1315 validation firmware was flashed and no visual observations were
  available.

Chunk 5 adds this final gate report and release-note cleanup.

## 4. Public API Changes

- `ControllerProfile::SSD1315` documents the supported controller command
  profile.
- `PanelProfile` and `applyPanelProfile(Config&, PanelProfile)` provide
  documented 128x64 SSD1315 electrical/panel presets.
- `Config::clearOnBegin` and `Config::clearOnRecover` allow production users to
  skip blocking GDDRAM clears and resync through dirty flushing.
- `controlStateDirty()` and `controlStateError()` expose uncertain physical
  panel-control state after failed control sequences.
- `SettingsSnapshot` includes control-dirty and lifecycle clear fields.
- `setContrast(0)` now returns `INVALID_CONFIG`; valid range is `1..255`.
- `Config::i2cAddress` is now validated as SSD1315 `0x3C` or `0x3D`.
- `ScrollSpeed` labels now match SSD1315 raw values; old raw-value aliases
  remain for source compatibility.

## 5. Behavior Changes And Migration Notes

- `begin()` and `recover()` are bounded blocking lifecycle calls. By default
  they run init and clear 1024 GDDRAM bytes on a 128x64 panel before display-on.
- Set `clearOnBegin=false` or `clearOnRecover=false` only when the application
  will redraw/flush afterward.
- `probe()` is ACK-only. It does not identify SSD1315 silicon.
- `recover()` is software-only. Hardware `RES#` and power sequencing remain
  application/platform policy.
- Flush failure retains dirty framebuffer state for retry.
- Flush requests while hardware scroll is active return `STATE_ERROR` and keep
  data dirty. Stop scroll and redraw/flush before judging framebuffer alignment.
- Code that previously used address values other than `0x3C`/`0x3D` or
  contrast `0` must update configuration/diagnostics.

## 6. SSD1315 Controller Policy

Supported controller target: **SSD1315 only**.

SSD1306-like panels may share many command bytes, but compatibility is not
claimed. A future SSD1306-compatible profile must remove or guard SSD1315-only
commands such as `SET_IREF`, define analog defaults explicitly, pass host
transaction tests, and pass real hardware validation.

## 7. Unsupported Or Deferred

- SSD1306 compatibility profile.
- I2C/GDDRAM readback or readback identity.
- Core-owned GPIO reset callbacks.
- Core-owned I2C bus creation, locking, pins, bus speed, or bus recovery.
- ISR-safe or internally thread-safe public API.
- Field-grade hardware readiness without matrix results.

## 8. CI And Build Status

CI workflow configuration exists in `.github/workflows/ci.yml`:

- Arduino PlatformIO matrix: `esp32s3dev`, `esp32s2dev`.
- Native tests and core timing guard.
- Package validation and CLI/IDF contract guards.
- Pure ESP-IDF matrix: `esp32s3`, `esp32s2` using
  `espressif/esp-idf-ci-action@v1`, `release-v5.3`, path
  `examples/espidf_basic`.

Current-branch CI execution is **unconfirmed**:

- `gh` is not installed in this environment.
- CI triggers only on `push` to `main` and `pull_request` targeting `main`;
  direct pushes to `hardening/ssd1315-industry-readiness` do not run CI.
- The CI/IDF subagent found no current branch workflow runs or check-runs for
  HEAD during this session.

## 9. Local Checks Run

Final local verification in this chunk:

| Command | Result |
| --- | --- |
| `python tools/check_core_timing_guard.py` | Pass |
| `python tools/check_cli_contract.py` | Pass |
| `python tools/check_idf_example_contract.py` | Pass |
| `python scripts/generate_version.py check` | Pass |
| `python -m platformio test -e native` | Pass, 54 tests |
| `python -m platformio run -e esp32s3dev` | Pass |
| `python -m platformio run -e esp32s2dev` | Pass |
| `python -m platformio pkg pack` | Pass; generated tarball removed |

No generated package tarballs are intentionally tracked.

## 10. Pure ESP-IDF Build Status

Local pure ESP-IDF builds were **not run** because `idf.py` is not on `PATH`.
PowerShell reported:

```text
idf.py : The term 'idf.py' is not recognized as the name of a cmdlet, function, script file, or operable program.
```

The workflow is configured to build `examples/espidf_basic` for `esp32s3` and
`esp32s2`; actual CI confirmation remains pending.

## 11. Hardware Validation Results

Physical SSD1315 hardware validation was **not run**.

The hardware matrix remains unfilled in
`docs/SSD1315_HARDWARE_VALIDATION.md`. Chunk 4 recorded:

- `COM5`: Espressif USB serial device, no response to `version` at 115200 baud.
- `COM18`: Espressif USB serial device running unrelated `tunnelmonitor 0.3.0`
  firmware; not overwritten.
- No SSD1315 validation target, panel wiring, supply, reset connection, or
  visual observation was confirmed.
- No fault, reset, unplug/replug, or soak validation was run.

## 12. Known Remaining Risks

- GitHub CI has not confirmed this branch/PR.
- Pure ESP-IDF builds are configured but not locally run.
- Real SSD1315 panel behavior is unvalidated.
- Display analog defaults still need board-specific confirmation: charge pump,
  VCOMH, precharge, contrast, IREF, COM pins, SEG remap, and COM scan direction.
- Reset and power sequencing are documented but not physically validated.
- Missing-display, unplug/replug, forced-failure, and soak behavior are not
  physically validated.
- Version metadata remains `1.2.0`; publishing requires a new version because
  public APIs were added.

## 13. Industry-Standard Readiness Verdict

Merge verdict: **do not merge until GitHub CI confirms the branch/PR checks.
After CI passes, mergeable as SSD1315 software-contract hardening.**

Release verdict: **not field-release complete. Do not tag or publish until
version metadata is bumped, changelog release notes are finalized, pure ESP-IDF
CI has passed, and representative SSD1315 hardware/fault/reset/soak validation
is recorded.**

This branch is a strong software hardening candidate. It is not a completed
industry-standard release gate because CI and hardware gates remain open.

## 14. Merge Checklist

- [x] Worktree clean at chunk starts.
- [x] Chunk commits created and pushed.
- [x] Core remains framework-neutral.
- [x] SSD1315-only controller policy documented.
- [x] Probe documented as ACK-only.
- [x] Reset ownership documented as platform/application policy.
- [x] Lifecycle blocking table present.
- [x] CLI validation command list guarded.
- [x] ESP-IDF example path consistent.
- [x] Local PlatformIO/native/package checks pass.
- [ ] GitHub CI confirms current branch/PR.

## 15. Release Checklist

- [ ] Bump `library.json`, `idf_component.yml`, and generated `Version.h` to a
  new SemVer, likely `1.3.0`.
- [ ] Move `[Unreleased]` changelog entries to the release version/date.
- [ ] Confirm CI on the release commit.
- [ ] Run and record pure ESP-IDF builds.
- [ ] Run and record the full hardware matrix on representative SSD1315 panels.
- [ ] Record reset, missing-display, unplug/replug, forced-failure, and soak
  results.
- [ ] Confirm no generated tarballs or build artifacts are tracked.
- [ ] Tag only after the above are complete.

## 16. Future-Work Backlog

- Add hardware-profile selection to examples if the validation target is a
  Wisevision module that requires external IREF or external VCC.
- Add a supplemental partial-update visual command to the hardware smoke
  sequence.
- Add optional platform-owned reset callback examples without moving GPIO
  ownership into core.
- Add a future SSD1306-compatible controller profile only with guarded commands
  and hardware validation.
- Add CI visibility/reporting for branch checks before merge.
- Run long soak with OLED-safe moving/alternating content and capture serial
  logs plus visual evidence.
