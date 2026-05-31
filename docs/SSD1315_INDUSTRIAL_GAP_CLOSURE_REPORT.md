# SSD1315 Industrial Gap Closure Report

Date: 2026-05-31

## 1. Branch And Commit

- Branch: `fix/ssd1315-industrial-gap-closure`
- Starting commit: `7ee619e61e899861c1ee59217ff71ac5f069ef37`
- Worktree at start: clean before this branch was created
- Final commit: see the commit that adds this report

## 2. Starting Repository State

The work started from the merged SSD1315 industry-readiness state. The previous
exploration report found the branch mergeable as software-contract hardening,
but not release- or field-ready because several lifecycle, scroll, HIL parser,
page-buffer, and evidence-tooling gaps remained.

No broad refactor, release tag, SSD1306 profile, or new hardware-validation
claim was made in this pass.

## 3. Subagents

- A lifecycle-focused subagent reviewed `end()`, display-on delay, sleep/display
  behavior, and offline-latch interactions. Its findings drove the `end()`
  raw shutdown path and the `tick(0)` display-on delay fix.
- Additional requested agents could not be spawned in this session because the
  multi-agent tool reported the agent thread limit was reached. The remaining
  HIL, test, ESP-IDF, docs, and final-review work was done locally.

## 4. Issues Fixed Or Deferred

Fixed:

- `end()` now attempts `DISPLAY_OFF` and internal charge-pump disable through a
  raw, untracked write path. An `OFFLINE` latch no longer prevents the final
  best-effort shutdown attempt.
- `end()` is idempotent and clears transient runtime state while preserving
  health counters for post-mortem diagnostics.
- Display-on delay no longer treats timestamp `0` as an elapsed interval.
  Delayed display readiness now uses explicit state instead of a timestamp
  sentinel.
- Flush timeout tracking no longer wraps when a flush starts at `nowMs == 0`.
- Vertical scroll offset validation now uses the active vertical scroll area.
- Hardware scroll now returns `UNSUPPORTED` for non-128-column configurations.
  Non-128-wide drawing and flushing continue to use configured-width address
  windows.
- ESP-IDF `cfg` output now exposes controller/profile, geometry, page buffer,
  budget, analog/timing values, sleep/all-on, scroll, dirty/control state, and
  health counters.
- Page-buffer clear/fill behavior has direct native regression coverage.
- Display-off/on, recover, clear, and control-dirty behavior have direct native
  regression coverage.
- HIL parser false positives for `fail=0`, `FAIL:0`, `Failures: 0`, and
  harmless `Last error: never` text are covered by Python tests.

Deferred:

- Release metadata/version bump. This pass intentionally does not tag or
  publish. Because public API/behavior changed, release should bump SemVer
  before publication.
- Broad SSD1306 compatibility. The repository remains SSD1315-only.
- New physical hardware validation. The runner is improved, but no new real
  HIL run was performed in this code pass.
- Full field-ready claim. Visual, fault/recovery, reset, and soak matrix
  evidence remains required.

## 5. Public API And Behavior Changes

- `SettingsSnapshot` now includes additional evidence fields:
  `comPins`, `chargePumpVoltage`, `iref`, `vcomh`, `clockDivide`,
  `oscFrequency`, `prechargePhase1`, `prechargePhase2`, and `scrollActive`.
- `end()` behavior is stronger but keeps the same `void end()` signature. It
  performs best-effort raw shutdown writes without updating health counters.
- Hardware scroll APIs now reject non-128-wide panel configurations with
  `UNSUPPORTED`.
- `startVerticalScroll()` rejects offsets outside the active vertical scroll
  area before sending I2C bytes.
- `setSleep(true)` remains display-off sleep. It does not disable the charge
  pump; full charge-pump-off remains part of `end()` for internal charge-pump
  profiles.

## 6. HIL Runner / Device Tester Changes

`tools/run_ssd1315_hil.py` is now a serial device tester and evidence collector.

Supported modes:

- `smoke`: quick version, scan/probe, cfg, selftest, final cfg.
- `functional`: the shared Arduino/ESP-IDF command sequence.
- `retention`: clear/display-off/display-on sequence for OLED retention
  investigation.
- `soak`: bounded mixed stress using alternating content.
- `all`: smoke, functional, retention, and soak.

New or strengthened options include expected address/geometry/controller/profile
checks, operator metadata, strict metadata checks, `--serial-only`,
`--interactive-visual`, `--soak-ops`, `--no-risky-visuals`, JSON/CSV artifact
flags, and matrix-fragment generation.

Artifacts generated for real serial runs:

- `serial_transcript.txt`
- `summary.md`
- `results.json`
- `results.csv`
- `metadata.json`
- `operator_visual_checklist.md`
- `hardware_matrix_fragment.md`
- `parsed_cfg_initial.json`
- `parsed_cfg_final.json`
- `health_delta.json`
- `failure_analysis.md`
- `command_plan.json`

The runner never claims visual pass automatically. Visual commands are marked
`OPERATOR_REQUIRED`, `SKIPPED_SERIAL_ONLY`, or an interactive operator result.
Field-ready evidence remains false unless a future process records all required
serial, visual, fault, reset, and soak evidence.

## 7. Tests Added

Native/fake-transport coverage added:

- best-effort `end()` shutdown while online and while `OFFLINE`
- idempotent `end()`
- display-on delay with `tick(0)`, elapsed release, zero delay, and wraparound
- vertical scroll offset validation against active scroll area
- invalid scroll parameter no-I2C behavior and preserved active scroll state
- non-128-width scroll rejection and configured-width flush window
- page-buffer clear/fill current-window semantics
- full page-buffer clear/fill iteration
- display off/on, recover, clear, and control-dirty behavior

Python HIL parser coverage added:

- zero failure counters are not failures
- nonzero failure counters are failures
- transport/status error tokens are failures
- visual OK responses still require operator evidence
- stress counters must match requested count
- harmless `Last error: never` in cfg does not fail
- cfg hex evidence fields parse as hex values

## 8. Documentation Updated

- README now documents the device-tester modes, artifacts, burn-in-safe runner
  behavior, serial-vs-visual evidence distinction, stronger `end()` contract,
  `tick(0)` timing behavior, and 128-column hardware-scroll scope.
- HIL runbook now explains smoke, functional, retention, soak, and all modes.
- Hardware validation matrix now distinguishes prior serial HIL evidence from
  incomplete visual/fault/reset/soak validation.
- HIL target template now includes exact mode-based runner commands and added
  evidence artifact fields.
- Datasheet alignment notes now describe raw best-effort shutdown and active
  vertical scroll area validation.
- Command reference notes now state that helper-level hardware scroll is limited
  to 128-column configs and that vertical offset is checked against the cached
  scroll area.
- Readiness summary now reflects the gap closure fixes and remaining gates.
- AGENTS.md generated-header path now points to `include/ssd1315/Version.h`.
- Top-level `include/SSD1315.h` shim now has a Doxygen file comment.
- The older exploration report now states it is superseded by this closure
  report for fixed/deferred status.

## 9. Validation Commands And Results

Passed:

- `python tools/check_core_timing_guard.py`
- `python tools/check_cli_contract.py`
- `python tools/check_idf_example_contract.py`
- `python scripts/generate_version.py check`
- `python -m py_compile tools/run_ssd1315_hil.py tools/check_cli_contract.py tools/check_idf_example_contract.py tools/test_hil_runner_parser.py`
- `python tools/test_hil_runner_parser.py`
- `python tools/run_ssd1315_hil.py --dry-run`
- `python tools/run_ssd1315_hil.py --dry-run --mode smoke`
- `python tools/run_ssd1315_hil.py --dry-run --mode functional`
- `python tools/run_ssd1315_hil.py --dry-run --mode retention`
- `python tools/run_ssd1315_hil.py --dry-run --mode soak --soak-ops 10`
- `python tools/run_ssd1315_hil.py --dry-run --mode all --soak-ops 10`
- `python -m platformio test -e native` (`77` tests passed)
- `python -m platformio run -e esp32s3dev`
- `python -m platformio run -e esp32s2dev`
- `git diff --check`

Not run:

- Local pure ESP-IDF builds. `idf.py --version` failed because `idf.py` is not
  on `PATH` in this environment.
- Real hardware serial smoke run. The operator did not request a new physical
  run for this implementation pass.

## 10. Remaining Hardware-Only Gaps

- Representative visual evidence for checkerboard, clear, fill, contrast,
  orientation, and scroll behavior.
- Fault/recovery evidence for missing display, unplug/replug if safe, and
  induced control-state failures where practical.
- Reset-pin behavior if `RES#` is wired.
- Bounded moving/alternating soak evidence with final clean cfg state.
- OLED retention isolation evidence with photos/video if ghosting is observed.
- Local or CI confirmation of pure ESP-IDF builds for `examples/espidf_basic`.

## 11. Verdict

- Merge-ready: yes, if CI passes. The code-actionable software gaps identified
  by the exploration report were addressed and local checks passed.
- Release-ready: not yet. Release metadata/version/changelog finalization and
  pure ESP-IDF CI confirmation are still required.
- Industrial/field-ready: not yet. The improved runner can collect evidence,
  but field-ready claims require the completed visual/fault/reset/soak hardware
  validation matrix.
