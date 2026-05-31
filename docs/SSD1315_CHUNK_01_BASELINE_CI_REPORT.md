# SSD1315 Chunk 01 Baseline / CI Report

Date: 2026-05-31
Branch: `hardening/ssd1315-industry-readiness`

## 1. Commit-Before State

Start checks:

- `git branch --show-current`: `hardening/ssd1315-industry-readiness`
- `git status --short`: clean
- `git log --oneline -8`: `eda23aa` at `HEAD`, followed by `59046d8`, `1c7e300`, `19d98b7`, `b9d58d1`, `822f3d1`, `5082c38`, `0808f63`

The previous report's statement that production-grade follow-up changes were
staged local edits was stale. The work was already committed and pushed at
`eda23aa41ede4e12b9ea9bde58d7ae76cfc91c79` before this chunk began. Both
`git diff` and `git diff --cached` were empty.

## 2. Baseline Verification

The committed baseline contains the expected production follow-up items:

- `ControllerProfile::SSD1315` and SSD1315-only public compatibility policy.
- `Config::clearOnBegin` and `Config::clearOnRecover`.
- `controlStateDirty()` / `controlStateError()` plus snapshot fields.
- Final `DISPLAY_ON` moved after init and optional clear policy.
- Dirty panel-control diagnostics and native tests.
- ESP-IDF mutex and nonblocking stdin hardening.
- CI ESP-IDF matrix for `esp32s2` and `esp32s3`.
- `idf_component.yml` aligned to `idf: ">=5.3.0"`.
- Transaction-logging host tests.
- Hardware validation matrix document.
- README, Doxygen, AGENTS, and report updates for SSD1315-only contracts.

## 3. Path Consistency

Actual ESP-IDF example path: `examples/espidf_basic`.

Searches for `examples/esp`, `espidf_basic`, `esp_idf`, and `esp-idf` found
consistent current references to `examples/espidf_basic` in README, docs,
`.github/workflows/ci.yml`, and guard scripts. No stale
`examples/esp_idf/basic` reference was found.

Regression coverage:

- `tools/check_idf_example_contract.py` hardcodes and verifies
  `examples/espidf_basic/main`.
- `tools/check_cli_contract.py` checks CLI parity against
  `examples/espidf_basic/main/main.cpp`.
- `.github/workflows/ci.yml` builds `examples/espidf_basic`.

## 4. CI Workflow Status

Inspected `.github/workflows/ci.yml`:

- Native tests run in the `native-tests` job.
- Arduino PlatformIO builds run for `esp32s3dev` and `esp32s2dev`.
- Guard scripts run: core timing guard, CLI contract, IDF example contract.
- Package validation runs through `pio pkg pack`.
- Pure ESP-IDF CI builds use `espressif/esp-idf-ci-action@v1` with
  `esp_idf_version: release-v5.3`, target matrix `esp32s3` and `esp32s2`,
  and path `examples/espidf_basic`.

The manifest requires `idf: ">=5.3.0"`, so the CI IDF version satisfies the
component metadata. Local GitHub Actions execution was not run; actual CI
execution remains pending on GitHub.

## 5. Public Metadata

Checked `library.json`, `idf_component.yml`, `CMakeLists.txt`, README, docs,
and generated `include/ssd1315/Version.h`.

- Public metadata describes this repository as SSD1315-first / SSD1315-only.
- Strong SSD1306 compatibility claims are absent from current package metadata.
- README states SSD1306-like panels are unvalidated and require a future
  guarded profile plus hardware validation.
- `probe()` is documented as ACK-only, not controller identity.
- Hardware validation remains explicitly not run.
- No field-grade release claim is made.
- `Version.h` is generated and matches `library.json` version `1.2.0`.

## 6. Checks Run

Passed:

- `python tools/check_core_timing_guard.py`
- `python tools/check_cli_contract.py`
- `python tools/check_idf_example_contract.py`
- `python scripts/generate_version.py check`
- `python -m platformio test -e native` (44 tests passed)
- `python -m platformio run -e esp32s3dev`
- `python -m platformio run -e esp32s2dev`
- `python -m platformio pkg pack`

Notes:

- The first non-verbose `esp32s3dev` PlatformIO run failed while compiling an
  Arduino framework object without surfacing a compiler diagnostic in the
  captured output. An immediate verbose rerun and a subsequent normal rerun
  both passed without source changes.
- `python -m platformio pkg pack` created `SSD1315-1.2.0.tar.gz`; it was
  removed after validation because the tarball is a generated artifact.

Not run:

- `idf.py -C examples/espidf_basic set-target esp32s3`
- `idf.py -C examples/espidf_basic build`
- `idf.py -C examples/espidf_basic fullclean`
- `idf.py -C examples/espidf_basic set-target esp32s2`
- `idf.py -C examples/espidf_basic build`

Reason:

```text
idf.py : The term 'idf.py' is not recognized as the name of a cmdlet,
function, script file, or operable program.
```

## 7. Subagent Findings

- `ci-idf-agent`: no blockers; CI and guard scripts use the actual
  `examples/espidf_basic` path, build S2/S3 IDF targets with release-v5.3, and
  match `idf_component.yml` minimum IDF `>=5.3.0`. It noted CI proves IDF 5.3,
  not every newer IDF release.
- `git-release-hygiene-agent`: no branch, artifact, duplicate-test, or
  generated-version blockers. It confirmed `test/test_basic.cpp` is an
  intentional shim into `test/native/test_basic.cpp`, and
  `scripts/generate_version.py check` passes. It flagged release metadata
  (`library.json`, `idf_component.yml`, `Version.h`, and CHANGELOG) as stale
  for a future release/tag because the current work remains under
  `Unreleased`; that release-gate work is deferred to chunk 05. It also
  flagged a stale `.gitignore` generated-version path, fixed in this chunk.

## 8. Changes Made In This Chunk

- Corrected stale commit-state wording in
  `docs/SSD1315_PRODUCTION_GRADE_FOLLOWUP_REPORT.md`.
- Corrected malformed local IDF command bullets in that same report.
- Added this chunk-specific baseline/CI report.
- Updated `.gitignore` to reference the actual generated-version path
  `include/ssd1315/Version.h`.

No code, workflow, metadata, or example path changes were required.

## 9. Remaining Risks For Next Chunk

- Pure ESP-IDF build still needs CI confirmation or a local environment with
  `idf.py` installed.
- Hardware validation has not been run.
- CLI hardware-validation command set still needs chunk 02 verification.
- Version bump / CHANGELOG release-section work remains for the final release
  gate. Current metadata is internally synchronized at `1.2.0`, but the
  unreleased API/build/docs work should not be tagged as `1.2.0`.
- SSD1306 compatibility remains intentionally unimplemented and unvalidated.
- Reset-pin behavior and panel analog defaults still require real hardware
  validation.
