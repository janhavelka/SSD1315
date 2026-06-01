# SSD1315 Industrial-Grade Exploration Report

Date: 2026-05-31

Superseded note: this report records the audit state before the industrial gap
closure pass. The code-actionable gaps called out here are tracked in
`docs/SSD1315_INDUSTRIAL_GAP_CLOSURE_REPORT.md`; use that newer report for
current fixed/deferred status.

## 1. Repository State

- Current branch: `main`
- Current commit: `b240ed04b8d8f1014f5de4f9dc197c27810e10d4`
- Recent hardening branch head referenced by prior evidence:
  `7b69d96a94d48ec116b6da5e15aaa3227ec6b2b1`
- Worktree at audit start: clean
- Note: the requested hardening branch has already been merged into `main` in
  this checkout. This report audits the merged state.

## 2. Executive Verdict

- Merge-ready: yes for software-contract hardening. In this checkout the work
  is already merged to `main`. If judging the equivalent pull request, no core
  blocker was found for merging after CI confirmation.
- Release-ready: not yet. Public API and behavior changed while package
  metadata remains `1.2.0`, changelog entries are still under `[Unreleased]`,
  and the release checklist has not been completed.
- Field or industrial-grade ready: not yet. Serial HIL evidence exists, but
  visual hardware validation, photos/video, fault/recovery checks, reset or
  unplug/replug evidence, and long-duration soak/retention evidence are still
  incomplete.

The repository is credible as SSD1315 software-contract hardening. It is not
ready to be marketed as field-grade SSD1315 hardware validation.

This audit also found production-relevant code risks that should be fixed or
explicitly accepted before an industrial release: offline shutdown may skip
physical display-off/charge-pump-off writes, `tick(0)` can bypass the
display-on delay guard, vertical-scroll offset is not validated against a
custom scroll area, and non-128-wide panel support is accepted more broadly
than it is documented or validated.

## 3. What Is Strong

- The core stays framework-neutral. Core headers and `src/` do not own Arduino
  `Wire`, ESP-IDF bus handles, reset GPIOs, locks, or platform logging.
- I2C is injected and non-owning. Address, timeout, bus ownership, and recovery
  policy are application or adapter concerns.
- SSD1315 scope is honest in current public docs: SSD1306 compatibility is not
  claimed, and `probe()` is documented as ACK-only rather than identity.
- `ControllerProfile::SSD1315` and `PanelProfile` separate controller support
  from panel/electrical presets.
- SSD1315-specific commands such as `SET_IREF` are named and tested.
- `begin()` and `recover()` are documented as bounded blocking lifecycle calls.
  `clearOnBegin` and `clearOnRecover` let production callers avoid synchronous
  full GDDRAM clear when they will redraw and flush afterward.
- Failed multi-command control paths can set `controlStateDirty()` and expose
  `controlStateError()`.
- Dirty-page flush behavior is robust. Native tests cover failed flush retry,
  clear after fill, mutation during active flush, and retry from current
  framebuffer bytes.
- Hardware scroll blocks normal framebuffer flushes until stopped, and stopping
  scroll marks framebuffer data dirty for redraw.
- Arduino and native ESP-IDF validation CLIs share the same guarded HIL command
  sequence.
- The HIL runner records serial transcripts, summaries, commit/worktree
  metadata, and marks visual commands as operator checks rather than automatic
  hardware passes.

## 4. What Is Still Weak Or Risky

- The hardware-validation story is not fully reconciled. The hardware matrix
  still says no physical validation was run, while the COM16 report records a
  successful serial HIL run. This should be stated consistently as:
  "serial HIL command evidence exists; visual, fault, reset, and soak evidence
  remain incomplete."
- `end()` documents a best-effort power-off sequence, but when the driver is
  already `OFFLINE`, the tracked write path can return the offline latch before
  issuing display-off or charge-pump-disable bytes.
- The first `tick(0)` after display-on can set the display-on delay timestamp
  to `1`, then unsigned-subtract `0 - 1`, making the panel immediately ready.
  This weakens the documented `displayOnDelayMs` guard in zero-time test or
  adapter setups.
- The version remains `1.2.0` in `library.json`, `idf_component.yml`, Doxyfile
  project metadata, and generated `Version.h`. The public API additions imply a
  likely next release of `1.3.0`, but this should be done only in a release
  metadata pass.
- GitHub CI could not be queried locally because `gh` is not on `PATH`.
- Pure local ESP-IDF builds could not be run because `idf.py` is not on `PATH`.
  CI is configured to build `examples/espidf_basic`, but actual run status was
  not confirmed in this audit.
- Visual OLED degradation or retention was observed by the operator after long
  static/high-contrast testing. Current software evidence does not prove a
  live GDDRAM bug, but field readiness requires display-off, safe power-cycle,
  and preferably second-panel evidence.
- Some historical process reports remain tracked under `docs/`. They are useful
  evidence, but Doxygen includes `docs` recursively, so historical reports can
  be surfaced beside current operator docs unless excluded or clearly linked as
  historical evidence.
- The active docs mention removed historical chunk reports, but tracked
  evidence reports still exist. This is not a code issue, but reviewers can
  misread the documentation state.

## 5. Ranked Findings

### Blocker

No blocker was found for keeping the software-contract hardening merged.

Release is blocked as-versioned:

- Public changes remain under `[Unreleased]`.
- `library.json`, `idf_component.yml`, Doxyfile metadata, and generated
  `Version.h` still show `1.2.0`.
- A release should bump SemVer, likely to `1.3.0` if no breaking API changes are
  introduced, then move changelog entries out of `[Unreleased]`.

Field/industrial-grade release is blocked:

- Serial HIL evidence exists, but the committed hardware matrix has not been
  filled with complete visual/fault/soak results.
- OLED retention/burn-in isolation has not been completed with photos/video,
  display-off observation, power-cycle observation, or second-panel evidence.

### High

- Documentation status mismatch: README/readiness/matrix language still says no
  physical validation was run, while `docs/SSD1315_HIL_COM16_AUDITOR_REPORT.md`
  records successful serial HIL evidence. The correct release-facing statement
  is serial HIL passed, visual/fault/soak validation incomplete.
- CI status is not confirmed from this environment. Workflow structure looks
  correct, but `gh` was unavailable.
- Local pure ESP-IDF build status is still unconfirmed because `idf.py` is
  unavailable locally.
- Hardware evidence is not enough for field readiness. Missing-display,
  unplug/replug, reset-pin behavior, forced failure, and bounded moving-pattern
  soak remain open.
- `end()` while `OFFLINE` may not send the SSD1315 power-off sequence. Fixing
  this likely means using a carefully bounded raw best-effort write path during
  `end()` or documenting that `end()` is software cleanup only after OFFLINE.

### Medium

- `tickPowerOn(0)` can bypass the configured display-on delay. This should have
  a focused regression test and a sentinel-safe timestamp fix.
- Vertical scroll offset validation only checks `0..63`; it does not validate
  the offset against a configured vertical scroll area. If applications use
  `setVerticalScrollArea()`, invalid datasheet combinations can still be sent.
- Non-128-wide display widths are accepted by `Config`, but hardware scroll
  always uses the full 128-column scroll window and validation/docs focus on
  128x64 panels. Non-128-wide panel variants should be documented as limited or
  tested/guarded.
- The native ESP-IDF `cfg` command reports address, geometry, timeouts,
  contrast, dirty state, and orientation, but omits several matrix-required
  analog/profile fields that the Arduino CLI prints, such as COM pins, charge
  pump, IREF, VCOMH, clock, and precharge.
- Doxygen currently includes `docs` recursively. Historical reports can appear
  in generated docs unless Doxyfile excludes them or the docs index clearly
  separates current operator docs from historical evidence.
- `AGENTS.md` still contains a stale generated-header example path:
  `include/YourLibrary/Version.h` instead of `include/ssd1315/Version.h`.
- The public shim header `include/SSD1315.h` is intentionally tiny but lacks
  Doxygen context.
- Native tests cover full-buffer clear/fill and page-buffer iteration state, but
  a direct page-buffer clear/fill semantics test would reduce operator
  confusion.
- HIL runner classification is important but not unit-tested as a Python test.
  The previous false-positive fix deserves a small parser regression test.

### Low

- Local ESP32-S3 PlatformIO build initially failed inside cached Arduino
  framework object compilation without a project-source diagnostic. Cleaning
  `.pio/build/esp32s3dev` and rerunning passed. This looks like a stale local
  build artifact, not a source failure, but it should be mentioned in audit
  evidence.
- `library.json` still uses marketing wording like "Hardened" and
  "non-blocking". The README now qualifies lifecycle blocking behavior, but
  release packaging should keep wording precise.
- The historical changelog includes the original "SSD1315/SSD1306" description.
  This is acceptable history, but current release notes should explicitly say
  broad SSD1306 compatibility is not claimed.

### Future Work

- Add a controller-profile compatibility path only if an SSD1306 profile guards
  SSD1315-only commands and passes host plus hardware validation.
- Add more documented panel profiles only when real module wiring and analog
  defaults are known.
- Add logic-analyzer evidence for command control byte `0x00`, data control
  byte `0x40`, page/column windows, and clear/fill payload shape on real
  hardware.
- Add product-level OLED lifetime policy examples: inactivity blanking, lower
  contrast defaults, and moving-content soak patterns.

## 6. Readiness Categories

### Software-Contract Readiness

Strong. The public contracts now state the real SSD1315 target, injected I2C
model, ACK-only probe limitation, reset ownership, lifecycle blocking, dirty
control-state behavior, and tick-budgeted flushing.

### Serial HIL Readiness

Strong. `tools/run_ssd1315_hil.py` has a deterministic command list, transcript
capture, summary generation, non-overwriting log directories, `pyserial`
failure guidance, and operator-check classification for visual commands.

The COM16 report records a successful serial HIL run against SSD1315 firmware.
That is command-surface evidence, not visual correctness evidence.

### Visual Hardware Validation

Incomplete. The matrix still has `Not run` rows. The operator still needs
photos/video or direct recorded observations for checkerboard, clear, fill,
contrast levels, flip states, scroll behavior, recover behavior, and final
blank display state.

### Long-Duration Field Readiness

Incomplete. Long static/high-contrast OLED operation already showed retention
or aging symptoms. Field readiness needs bounded moving/alternating soak,
brightness policy, display-off or sleep policy, and fault/recovery evidence.

## 7. SSD1315-Specific Findings

### Controller Profile

The repo exposes only `ControllerProfile::SSD1315`. This is correct for the
current implementation. No broad SSD1306 compatibility is claimed.

### Panel Profiles

`PanelProfile` documents three narrow 128x64 electrical presets:

- generic internal charge pump;
- Wisevision internal DC/DC;
- Wisevision external VCC.

These are panel/electrical presets, not controller compatibility profiles. The
current documentation is defensible, but hardware validation must record which
profile was used and why it matches the physical module.

### IREF

`SET_IREF` (`0xAD`) is named as SSD1315-specific in code and docs. This is the
main reason SSD1306 compatibility must not be implied.

### Charge Pump

Internal charge-pump profiles send charge-pump enable before display-on.
External-VCC profile uses charge pump off. `end()` sends display-off and a
best-effort charge-pump disable when the active configuration used the internal
charge pump in normal states.

Risk: if the driver is already `OFFLINE`, the tracked command path can refuse
I2C before sending the shutdown bytes. This conflicts with the best-effort
shutdown wording and should be fixed or documented before release.

### VCOMH, Precharge, COM/SEG Mapping

The defaults are exposed in `Config` and overridden by documented panel
profiles. `docs/SSD1315_DATASHEET_ALIGNMENT.md` maps the Wisevision profile to
segment remap `A1`, COM scan `C8`, COM pins `0x12`, contrast `0xB0`, clock
`0x90`, precharge `0x22`, and VCOMH `0x30`.

A future docs polish pass should add a compact command-by-command init table
that maps every init command to its source and profile field.

### Scroll

Scroll arguments are range-checked. Prior scroll is deactivated before
reconfiguration. Flushes are blocked while scroll is active. `stopScroll()`
marks framebuffer pages dirty so redraw/flush can resync GDDRAM.

Gap: vertical-scroll offset is validated as `0..63`, but not against a custom
vertical scroll area set through `setVerticalScrollArea()`. The safe production
contract should either cache and validate the scroll area or clearly document
that callers must keep those parameters datasheet-valid.

### Clear/Fill And Flush

Native tests cover all key software-byte paths:

- `clear()` after `fill()` sends zero payload;
- failed flush preserves dirty state;
- retry sends current framebuffer data;
- framebuffer mutation during active flush keeps the affected page dirty;
- full-frame flush transaction count and chunking are covered.

This strongly reduces the chance that observed ghosting is caused by stale
software bytes, but actual panel behavior still needs visual evidence.

### Sleep And Display-Off

Display off/on aliases exist in both validation CLIs through `display off` and
`display on`. Docs correctly use `display off` as a retention-isolation step.
Auto-sleep exists and has a wraparound native test.

### OLED Retention

Docs now warn that OLED panels can retain static content or age unevenly. The
operator should distinguish physical retention from live GDDRAM corruption with:

- `recover`
- `scroll stop`
- `invert 0`
- `clear`
- `display off`
- safe power cycle
- second panel if available
- photos/video

Observed long static/high-contrast degradation should be treated as known OLED
physical behavior until transaction or logic-analyzer evidence proves stale
bytes or wrong commands.

## 8. CI, Build, And Package Findings

The workflow `.github/workflows/ci.yml` includes:

- Arduino PlatformIO builds for `esp32s3dev` and `esp32s2dev`;
- native PlatformIO tests;
- core timing guard;
- CLI contract guard;
- ESP-IDF example contract guard;
- `pio pkg pack`;
- pure ESP-IDF builds for `esp32s3` and `esp32s2` using
  `examples/espidf_basic` and `release-v5.3`.

`idf_component.yml` requires `idf: ">=5.3.0"` and declares targets `esp32s2`
and `esp32s3`, which matches the workflow target matrix.

The ESP-IDF example is native and contract-guarded, but its `cfg` output is
less complete than the Arduino CLI for panel analog/profile evidence. Before
using the ESP-IDF CLI as the only hardware-validation firmware, add those
fields or record them through another documented evidence path.

Local package metadata is coherent but not release-final:

- `library.json`: `1.2.0`
- `idf_component.yml`: `1.2.0`
- generated `Version.h`: `1.2.0`
- Doxyfile `PROJECT_NUMBER`: `1.2.0`

The package version should not be tagged until the release metadata pass is
complete.

## 9. HIL Evidence Findings

Existing evidence:

- `docs/SSD1315_HIL_COM16_AUDITOR_REPORT.md` records a final COM16 serial HIL
  run with exit status `0`.
- The serial run used SSD1315 firmware at commit `a15bea3`, address `0x3C`,
  Arduino framework, 128x64 geometry, and final clean `cfg` state.
- Serial command results showed `OK` or clean counters for the HIL sequence.

Missing evidence:

- operator visual results;
- photos/video;
- completed hardware matrix;
- display-off ghosting isolation;
- safe power-cycle persistence test;
- missing-display behavior;
- unplug/replug behavior;
- reset-pin behavior;
- forced transport failure, if practical;
- long moving/alternating soak result;
- logic-analyzer capture, if available.

Minimum extra evidence before a field-grade claim:

1. Complete `docs/SSD1315_HARDWARE_VALIDATION.md` from a fresh HIL run.
2. Attach or reference the generated serial transcript and summary.
3. Record photos/video for checkerboard, clear, fill, contrast, flip, and
   scroll behavior.
4. Run display-off and power-cycle retention isolation if ghosting appears.
5. Record at least one bounded moving/alternating soak with final `cfg`.
6. Record reset and missing-display behavior, or explicitly mark them not run
   with reason.

## 10. Documentation Honesty Findings

Good:

- SSD1315-only scope is clear.
- SSD1306 compatibility is not claimed.
- `probe()` is ACK-only.
- Reset GPIO ownership is platform/application policy.
- `begin()` and `recover()` are bounded blocking.
- OLED retention warnings exist.
- The HIL runner does not claim visual pass automatically.

Needs cleanup before release:

- Reconcile "no hardware validation was run" with the COM16 serial HIL report.
- Keep the hardware matrix as the final source of visual/fault/soak truth.
- Move historical reports into a clearly named evidence section or exclude them
  from generated Doxygen if they should not be treated as current operator
  instructions.
- Fix the stale `AGENTS.md` generated-header path.
- Add Doxygen context to `include/SSD1315.h`.
- Add or document a production-safe `end()` path when the driver is `OFFLINE`.
- Add a focused note for `tick(0)` / missing timebase behavior if the code is
  not fixed immediately.

## 11. Release And Versioning Findings

The current version remains `1.2.0`. That is appropriate while this work is not
released, but not appropriate for a release tag that includes the public API and
behavior changes.

Required before tag:

1. Decide the release version, likely `1.3.0`.
2. Update `library.json`.
3. Regenerate `include/ssd1315/Version.h` using `scripts/generate_version.py`.
4. Update `idf_component.yml`.
5. Update Doxyfile `PROJECT_NUMBER`.
6. Move `[Unreleased]` changelog entries to the release section.
7. Confirm GitHub CI.
8. Decide whether the release is software-contract only or hardware-validated.

Do not claim field-grade behavior unless the hardware matrix is complete.

## 12. Test Coverage Gaps

Recommended concrete tests:

- `test_page_buffer_clear_fill_affect_current_window_only`
  - Configure `pageBufferPages = 1`.
  - Assert `clear()` and `fill()` affect only the current buffer window and
    that full-display behavior requires `firstPage()` / `nextPage()`.

- `test_page_buffer_full_iteration_clear_flushes_all_pages`
  - Iterate all pages and assert all page windows are addressed and flushed.

- `test_end_offline_still_attempts_display_off_if_transport_available`
  - Force the driver offline, call `end()`, and assert the intended shutdown
    contract. If the intended contract remains software-only after offline,
    update docs instead.

- `test_tick_power_on_at_zero_does_not_bypass_display_on_delay`
  - Begin or wake with `displayOnDelayMs = 100`, call `tick(0)`, and assert the
    panel is not marked ready until enough monotonic time passes.

- `test_vertical_scroll_offset_respects_configured_scroll_area`
  - Set a smaller vertical scroll area, call vertical scroll with an invalid
    offset, and assert no I2C bytes are sent.

- `test_non_128_width_scroll_contract`
  - Configure a smaller valid width and assert scroll is rejected, documented,
    or sends a width-correct sequence depending on the chosen contract.

- `test_hil_runner_ignores_zero_fail_counters`
  - Feed `fail=0`, `FAIL:0`, ANSI-colored counters, and `Failures: 0`.
  - Assert they do not classify as failure.

- `test_hil_runner_detects_real_failure_tokens`
  - Feed `fail=1`, `I2C_TIMEOUT`, `DEVICE_NOT_FOUND`, and `STATE_ERROR`.
  - Assert serial result is `FAIL`.

- `test_hil_runner_visual_commands_require_operator_result`
  - Assert visual commands classify as serial review/operator check even when
    command text contains an OK token.

- `test_idf_transport_maps_error_classes`
  - Host-test or compile-test the ESP-IDF adapter mapping for address NACK,
    data NACK, timeout, bus error, and generic failures.

- `test_idf_cfg_reports_panel_profile_fields`
  - Guard the native ESP-IDF CLI evidence surface for COM pins, charge pump,
    IREF, VCOMH, clock, precharge, and panel profile.

- `test_sleep_display_off_clear_recover_sequence`
  - Verify `display off`, `display on`, `recover`, and `clear` command order
    and control-state behavior.

- `test_raw_diagnostic_command_requires_recover_before_validation`
  - If raw command helpers remain exposed, document/test the recommended
    resync path after raw mode-changing commands.

## 13. Validation Commands Run

| Command | Result | Notes |
| --- | --- | --- |
| `git status --short` | PASS | Clean at audit start. |
| `python tools/check_core_timing_guard.py` | PASS | `Core timing guard PASSED`. |
| `python tools/check_cli_contract.py` | PASS | `CLI contract PASSED`. |
| `python tools/check_idf_example_contract.py` | PASS | `IDF example contract PASSED`. |
| `python scripts/generate_version.py check` | PASS | `Version.h` up to date. |
| `python -m py_compile tools/run_ssd1315_hil.py tools/check_cli_contract.py` | PASS | No output, exit 0. |
| `python tools/run_ssd1315_hil.py --dry-run` | PASS | Printed the expected ordered HIL sequence. |
| `python -m platformio test -e native` | PASS | 57/57 native tests passed. |
| `python -m platformio run -e esp32s3dev` | PASS after clean | First attempt failed inside cached Arduino framework objects with no project-source diagnostic. `python -m platformio run -e esp32s3dev -t clean` succeeded, then rerun succeeded. |
| `python -m platformio run -e esp32s2dev` | PASS | Build succeeded. |
| `git diff --check` | PASS | No whitespace errors before report creation. |
| `idf.py --version` | NOT RUN | `idf.py` was not on `PATH`; pure local ESP-IDF builds were not run. |
| `gh --version` | NOT RUN | `gh` was not on `PATH`; PR/CI status was not queried locally. |

## 14. Recommended Next Prompts

### Release Metadata And Changelog

```text
Prepare SSD1315 release metadata for the merged hardening work. Verify current
version from library.json, choose the next SemVer version, update library.json,
idf_component.yml, Doxyfile PROJECT_NUMBER, generated Version.h via
scripts/generate_version.py, and move CHANGELOG.md entries from [Unreleased].
Do not claim field-grade hardware validation unless the completed matrix exists.
Run all local checks and commit the release metadata changes.
```

### Optional Documentation Polish

```text
Reconcile SSD1315 validation docs after COM16 serial HIL. Update README,
SSD1315_READINESS_SUMMARY.md, SSD1315_HARDWARE_VALIDATION.md, and Doxyfile so
they consistently say serial HIL command evidence exists but visual/fault/soak
validation is incomplete. Fix stale AGENTS.md Version.h path and add a short
Doxygen comment to include/SSD1315.h. Do not change driver behavior.
```

### Hardware Soak And Retention Validation

```text
Run SSD1315 hardware validation with tools/run_ssd1315_hil.py on the target
panel. Fill docs/SSD1315_HARDWARE_VALIDATION.md from serial logs, operator
visual observations, photos/video, display-off ghosting isolation, safe
power-cycle behavior, reset or unplug/replug behavior where safe, and a bounded
moving/alternating soak. Do not claim field-grade readiness unless every row is
complete or explicitly marked not applicable with reason.
```

### Required Code Fixes If Found

```text
Add focused fixes/tests only: offline end() shutdown behavior, tick(0)
display-on delay handling, vertical-scroll area validation, HIL runner parser
tests, page-buffer clear/fill semantics tests, sleep/display-off recovery tests,
and IDF adapter/error-surface tests. Keep the core framework-neutral and do not
add SSD1306 compatibility.
```

## 15. Final Operator Recommendation

- Safe to merge now: yes for software-contract hardening, subject to CI. This
  checkout is already on `main` at the merge commit.
- Safe to tag release now: no.
- Safe to call industrial-grade or field-ready now: no.

Exact remaining gates:

1. Confirm GitHub CI for the merged commit.
2. Complete the release metadata and changelog pass.
3. Run or confirm pure ESP-IDF builds, either locally or in CI.
4. Reconcile serial HIL evidence wording across docs.
5. Complete hardware matrix with visual evidence.
6. Complete OLED retention isolation if ghosting appears.
7. Complete fault/recovery/reset/unplug evidence where safe.
8. Complete bounded moving/alternating soak evidence.
9. Resolve the offline shutdown and `tick(0)` display-on-delay findings before
   claiming industrial-grade lifecycle behavior.
