# SSD1315 Pre-HIL Readiness Report

Date: 2026-05-31

Branch: `hardening/ssd1315-industry-readiness`

Pre-pass starting commit: `15fd911a2c02e354fdd6231f26a6b3ccc91f7893`

Pre-HIL readiness commit: this commit; use `git log -1 --oneline` after commit
or the operator final response for the immutable hash.

## 1. Branch Handling

Work continued on the existing SSD1315 hardening branch. No new branch was
created because `hardening/ssd1315-industry-readiness` was still active and the
worktree was clean at the start of this pass.

## 2. What Changed

- Added `tools/run_ssd1315_hil.py`, a host-side serial HIL runner with dry-run,
  timestamped log directories, raw transcript capture, summary generation, and
  operator-check markers for visual commands.
- Added `docs/SSD1315_HIL_RUNBOOK.md` with preflight fields, build/flash
  instructions, serial runner usage, visual acceptance criteria, fault/recovery
  criteria, soak guidance, and evidence capture requirements.
- Updated `docs/SSD1315_HARDWARE_VALIDATION.md` to reference the runbook and use
  the bounded `monitor 1000` / `monitor 0` sequence.
- Updated README references so operators can find the HIL runner and runbook.
- Updated Arduino and ESP-IDF CLI `version` / `cfg` output to include more
  build, framework, target, controller/profile, address, geometry, clear-policy,
  flush/dirty, and control-dirty context.
- Added example-level scroll-active tracking to both validation CLIs so `cfg`
  can report the command-surface scroll state without adding a core API.
- Updated `tools/check_cli_contract.py` so the hardware matrix and HIL runner
  stay aligned on the executable command sequence.
- Added `hil_logs/` to `.gitignore`.

## 3. CLI Commands Verified

The expected HIL command surface is implemented by both Arduino and ESP-IDF
examples and guarded by `tools/check_cli_contract.py`:

```text
version
scan
probe
cfg
selftest
pattern checker
clear
fill
invert 1
invert 0
contrast 1
contrast 127
contrast 255
flipx 1
flipx 0
flipy 1
flipy 0
scrollh right 0 7
scrollv left 0 7 1
scroll stop
recover
stress 100
stress_mix 100
monitor 1000
monitor 0
contrast 127
clear
cfg
```

`contrast 0` remains intentionally excluded. `probe` remains ACK-only and does
not claim SSD1315 identity. `selftest` is serial/software evidence only; visual
correctness still requires operator observation.

## 4. HIL Runner Status

Status: added.

Dry-run command:

```bash
python tools/run_ssd1315_hil.py --dry-run
```

Actual HIL command:

```bash
python tools/run_ssd1315_hil.py --port <serial-port> --baud 115200 --out hil_logs
```

The runner writes:

- `hil_logs/ssd1315_<timestamp>/serial_transcript.txt`
- `hil_logs/ssd1315_<timestamp>/summary.md`

It does not flash firmware. If `pyserial` is missing, it reports this exact
install hint:

```bash
python -m pip install pyserial
```

## 5. Local Checks Run

| Command | Result |
| --- | --- |
| `python tools/check_core_timing_guard.py` | Pass |
| `python tools/check_cli_contract.py` | Pass |
| `python tools/check_idf_example_contract.py` | Pass |
| `python scripts/generate_version.py check` | Pass |
| `python -m py_compile tools/run_ssd1315_hil.py` | Pass |
| `python tools/run_ssd1315_hil.py --dry-run` | Pass |
| `python -m platformio test -e native` | Pass, 54 tests |
| `python -m platformio run -e esp32s3dev` | Pass |
| `python -m platformio run -e esp32s2dev` | Pass |
| `python -m platformio pkg pack` | Pass; generated tarball removed |
| `git diff --check` | Pass; only Git CRLF conversion warnings |

No generated package tarballs are intentionally tracked.

## 6. Pure ESP-IDF Status

Local pure ESP-IDF builds were not run because `idf.py` is unavailable on
`PATH`. PowerShell reported:

```text
idf.py : The term 'idf.py' is not recognized as the name of a cmdlet, function, script file, or operable program.
```

The CI workflow remains configured for `examples/espidf_basic` on `esp32s3` and
`esp32s2` with ESP-IDF `release-v5.3`, matching `idf_component.yml` requirement
`>=5.3.0`.

## 7. CI Status

`gh` is unavailable in this environment, so branch/PR CI status was not queried.
PowerShell reported:

```text
gh : The term 'gh' is not recognized as the name of a cmdlet, function, script file, or operable program.
```

Manual CI check instructions:

1. Open the PR for `hardening/ssd1315-industry-readiness` targeting `main`.
2. Confirm `.github/workflows/ci.yml` ran on the PR head commit.
3. Confirm the Arduino PlatformIO matrix, native tests, guard scripts, package
   validation, and ESP-IDF `esp32s2`/`esp32s3` jobs all passed.
4. If using GitHub CLI after installing it, run:

```bash
gh pr status
gh run list --branch hardening/ssd1315-industry-readiness --limit 10
```

Note: the workflow currently triggers on pushes to `main` and pull requests
targeting `main`; a direct push to this hardening branch does not by itself run
CI unless a PR exists.

## 8. Remaining Blockers Before HIL

- Select and connect the actual MCU board and SSD1315 panel.
- Record all preflight fields in `docs/SSD1315_HIL_RUNBOOK.md`.
- Build and upload the matching validation firmware to the selected board.
- Install `pyserial` if the runner reports it missing.
- Run the HIL command sequence and attach serial transcript, summary, photos or
  video, and optional logic analyzer captures.
- Fill `docs/SSD1315_HARDWARE_VALIDATION.md` with real observed results.
- Decide which fault tests are safe for the fixture before disconnecting,
  resetting, or inducing transport failures.

## 9. Explicit Non-Validation Statement

No physical SSD1315 hardware validation was performed in this pre-HIL readiness
pass. The hardware matrix remains unfilled until the actual HIL run records
serial logs, visual observations, and fault/soak evidence.
