# SSD1315 HIL Validation Report - COM29 - 2026-06-23

Status: serial HIL evidence and repository audit. This report does not claim
field readiness because visual inspection, photos/video, safe fault injection,
logic-analyzer capture, reset-pin validation, and representative hardware matrix
coverage were not completed.

## Run Context

| Field | Value |
| --- | --- |
| Repository path | `C:\Users\Honza\Documents\Projects\SSD1315` |
| Branch | `main` |
| Commit | `59759a80ebb474401ff3e09e17cfe42186ce3a97` |
| Initial worktree | Clean before HIL/tooling edits |
| Final worktree | Dirty: HIL runner and parser tests changed, report added |
| Report date/time | 2026-06-23, Europe/Prague (UTC+02:00) |
| OS | Microsoft Windows 11 Education |
| Python | 3.12.10 |
| PlatformIO | Core 6.1.18 |
| ESP-IDF CLI | `idf.py` not available on PATH |
| Serial port | `COM29` |
| Baud rate | 115200 |
| Target board/environment | ESP32-S2, PlatformIO `esp32s2dev`, Arduino framework |
| Firmware flashed | `examples/01_basic_bringup_cli` via `platformio.ini` |
| Board detected by upload | ESP32-S2FH4 rev v1.0, embedded 4 MB flash, MAC `dc:b4:d9:b5:a5:cc` |
| OLED config | 128x64, 7-bit I2C address `0x3C`, example panel profile `example-default-128x64-internal-charge-pump` |
| I2C pins / speed | Example defaults SDA GPIO8, SCL GPIO9, 400 kHz |
| Additional scanned address | `0x51`, not identified by this OLED driver |
| Reset wiring | Unknown; no hardware reset-pin test run |
| Supply / pullups / panel model | Unknown; not safe to infer from software |

`probe()` and scan results prove address ACK only. They do not identify SSD1315
silicon.

## Safety Scope

Only serial CLI commands already exposed by the repository examples were used.
No hotplug, disconnect, overcurrent, overtemperature, power cycling, reset GPIO,
or physical fault injection was attempted because the fixture safety was not
specified. Visual commands were run briefly and restored to safe states, but no
operator visual pass/fail evidence was collected.

## Exact Commands

Build and upload:

```powershell
python -m platformio run -e esp32s2dev
python -m platformio run -e esp32s2dev --target upload --upload-port COM29
```

Upload wrote and verified the image, then failed during post-upload hard reset
because the serial device disappeared. The flashed firmware subsequently
responded on `COM29`, so HIL continued and this anomaly is recorded as an upload
reset/port re-enumeration issue, not a firmware write failure.

Host checks and builds:

```powershell
python -m py_compile tools\run_ssd1315_hil.py tools\test_hil_runner_parser.py
python tools\run_ssd1315_hil.py --parser-self-test
python tools\test_hil_runner_parser.py
python tools\check_cli_contract.py
python tools\check_idf_example_contract.py
python tools\check_core_timing_guard.py
python scripts\generate_version.py check
python tools\check_package_contents.py
python -m platformio test -e native
python -m platformio run -e esp32s2dev
python -m platformio run -e esp32s3dev
```

HIL commands:

```powershell
python tools\run_ssd1315_hil.py --mode smoke --port COM29 --baud 115200 --out hil_logs --expect-address 0x3C --expect-width 128 --expect-height 64 --serial-only --timeout 5 --startup-wait 2
python tools\run_ssd1315_hil.py --mode functional --port COM29 --baud 115200 --out hil_logs --expect-address 0x3C --expect-width 128 --expect-height 64 --expect-controller SSD1315 --serial-only --timeout-s 8 --idle-timeout-s 0.35 --boot-settle-s 1 --reconnect-attempts 2 --verbose
python tools\run_ssd1315_hil.py --mode benchmark --port COM29 --baud 115200 --out hil_logs --expect-address 0x3C --expect-width 128 --expect-height 64 --expect-controller SSD1315 --serial-only --timeout-s 8 --idle-timeout-s 0.35 --boot-settle-s 1 --reconnect-attempts 2 --soak-ops 500 --verbose
python tools\run_ssd1315_hil.py --mode retention --port COM29 --baud 115200 --out hil_logs --expect-address 0x3C --expect-width 128 --expect-height 64 --expect-controller SSD1315 --serial-only --timeout-s 8 --idle-timeout-s 0.35 --boot-settle-s 1 --reconnect-attempts 2 --verbose
python tools\run_ssd1315_hil.py --mode soak --port COM29 --baud 115200 --out hil_logs --expect-address 0x3C --expect-width 128 --expect-height 64 --expect-controller SSD1315 --serial-only --timeout-s 8 --idle-timeout-s 0.35 --boot-settle-s 1 --reconnect-attempts 2 --soak-ops 500 --soak-duration-hours 8
python tools\run_ssd1315_hil.py --mode functional --port COM29 --baud 115200 --out hil_logs --expect-address 0x3C --expect-width 128 --expect-height 64 --expect-controller SSD1315 --serial-only --timeout-s 8 --idle-timeout-s 0.35 --boot-settle-s 1 --reconnect-attempts 2
```

## Artifact Locations

| Purpose | Path |
| --- | --- |
| Initial smoke serial run | `hil_logs\ssd1315_20260622_205431` |
| Functional serial run | `hil_logs\ssd1315_20260622_205859` |
| Benchmark serial run | `hil_logs\ssd1315_20260622_205926` |
| Retention serial run | `hil_logs\ssd1315_20260622_210001` |
| 8-hour soak serial run | `hil_logs\ssd1315_20260622_210029` |
| Soak stdout/stderr | `hil_logs\soak_8h_20260622_210029_stdout.txt`, `hil_logs\soak_8h_20260622_210029_stderr.txt` |
| Post-soak functional cleanup/verification | `hil_logs\ssd1315_20260623_051630` |

The listed `hil_logs` directories are local run artifacts, not committed source
evidence unless separately archived. Later HIL directories contain
`serial_transcript.txt`, `summary.md`, `results.json`, `results.csv`,
`metadata.json`, `run_stats.json`, `hardware_matrix_fragment.md`,
`failure_analysis.md`, and parsed config files where supported by the updated
runner. The initial smoke directory was produced by the older runner and did
not include every later artifact, such as `run_stats.json`.

## Summary

| Run | Result | Serial counts | Elapsed | Notes |
| --- | --- | --- | ---: | --- |
| Smoke | PASS serial | 6 PASS, 0 FAIL | Not recorded by old runner | Pre-fix metadata recorded clean worktree as `unknown` |
| Functional | PASS serial, visual incomplete | 8 PASS, 20 serial-pass/operator-required, 0 FAIL | 19.92 s | Exercised scan, probe, config, selftest, draw/control/scroll/recover/stress |
| Benchmark | PASS serial, visual incomplete | 5 PASS, 3 serial-pass/operator-required, 0 FAIL | 13.78 s | Captured bounded timing rates |
| Retention | PASS serial, visual incomplete | 9 PASS, 7 serial-pass/operator-required, 0 FAIL | 11.28 s | Serial-only subset of ghosting isolation; no visual evidence |
| 8-hour soak | PASS serial, visual incomplete | 6043 PASS, 4532 serial-pass/operator-required, 0 FAIL | 28808.375 s | 1511 `stress_mix 500` blocks, 755500 mixed operations |
| Post-soak functional | PASS serial, visual incomplete | 8 PASS, 20 serial-pass/operator-required, 0 FAIL | 19.95 s | Restored final clean `cfg` after soak stopped mid-cycle |

## Detailed Test Table

| ID | Area | Command or step | Expected | Observed | Elapsed | Result | Notes |
| --- | --- | --- | --- | --- | ---: | --- | --- |
| HIL-001 | Flash | `pio run -e esp32s2dev --target upload --upload-port COM29` | Image written and target reset | Image written and verified; reset/port configure failed after write | 20.33 s | PASS with anomaly | Firmware responded afterward on `COM29` |
| HIL-002 | Boot/identity | `version` | Version/config output | Firmware reported Arduino, ESP32-S2-Saola-1, SSD1315 profile, `0x3C`, 128x64 | 0.44 s typical | PASS | Firmware profile, not silicon identity |
| HIL-003 | Bus scan | `scan` | Expected address present | `0x3C` and `0x51` ACKed | 0.55 s | PASS | `0x51` not identified by this driver |
| HIL-004 | Probe | `probe` | ACK-only OK, no health side effects | Probe OK; health counters unchanged | 0.42-0.44 s | PASS | Does not prove controller type |
| HIL-005 | Config snapshot | `cfg` | Expected geometry/state, no dirty/control error for final checks | Final post-soak cfg: initialized yes, dirty no, flushing no, controlDirty no, scrollActive no | 0.44 s | PASS | Final snapshot from `hil_logs\ssd1315_20260623_051630` |
| HIL-006 | Self-test | `selftest` | Serial checks complete with fail=0 | pass=20 fail=0 skip=0 | 0.64-0.66 s | PASS serial | Visual correctness not proven |
| HIL-007 | Draw/pattern | `pattern checker`, `clear`, `fill` | Serial OK and visible operator confirmation | Serial OK; operator evidence skipped | 0.47-0.56 s each | PASS serial / VISUAL NOT RUN | No photos/video |
| HIL-008 | Display controls | `invert`, `contrast`, `flipx`, `flipy`, `allon`, `display off/on` | Serial OK and visible state changes | Serial OK; visual evidence skipped | 0.42-0.47 s each | PASS serial / VISUAL NOT RUN | High contrast restored |
| HIL-009 | Scroll | `scrollh`, `scrollv`, `scroll stop` | Serial OK and visible motion/stop | Serial OK; visual evidence skipped | 0.42-0.44 s each | PASS serial / VISUAL NOT RUN | Scroll stopped before cleanup |
| HIL-010 | Recovery | `recover` | Software recover OK and health state usable | Recover OK; health counter +52 in functional run | 0.47 s | PASS serial | No reset GPIO toggle |
| HIL-011 | Stress | `stress 100` | 100 successes, 0 failures | 100 successes, 0 failures | 0.45 s command wait | PASS | Functional run |
| HIL-012 | Mixed stress | `stress_mix 100` | 100 successes, 0 failures | 100 successes, 0 failures, 32 ops/sec | 3.55 s command wait | PASS serial | Visual evidence skipped |
| HIL-013 | Monitor | `monitor 1000`, `monitor 0` | Enable and disable acknowledged | `Health monitor: ON`, then OFF | 0.44 s each | PASS | Parser updated to accept Arduino wording |
| HIL-014 | Benchmark contrast | `stress 500` | 500 successes, 0 failures | 500 successes, 0 failures, 77 ms, 6493 ops/sec | 0.45 s command wait | PASS | Serial benchmark |
| HIL-015 | Benchmark mixed | `stress_mix 100` | 100 successes, 0 failures | 100 successes, 0 failures, 3102 ms, 32 ops/sec | 3.55 s command wait | PASS serial | Visual evidence skipped |
| HIL-016 | Benchmark flush | `flushstress 100` | 100 successes, 0 failures | 100 successes, 0 failures, 2869 ms, 34 flushes/sec, 28 ms average | 3.34 s command wait | PASS serial | Arduino CLI path |
| HIL-017 | Benchmark burst | `burst 500` | 500 successes, 0 failures | 500 successes, 0 failures, 68 ms, 7352 cmds/sec | 0.44 s command wait | PASS | Serial benchmark |
| HIL-018 | Retention serial subset | `recover`, `scroll stop`, `invert 0`, `allon 0`, `clear`, `display off/on`, `pattern checker` | Serial OK; visual operator records ghosting | Serial OK; no operator evidence | 11.28 s total | PASS serial / VISUAL NOT RUN | Display ended off in retention mode |
| HIL-019 | 8-hour soak | Repeated `version`, `cfg`, `contrast 127`, `clear`, `stress_mix 500`, `clear`, `cfg` until deadline | No serial failures, no persistent dirty/control state, bounded latency | 10575 commands, 755500 mixed ops, 0 FAIL, 0 review, max latency 16.485 s | 28808.375 s | PASS serial | Deadline hit after final `stress_mix 500`; post-soak functional run cleaned state |
| HIL-020 | Post-soak cleanup | Functional run ending `contrast 127`, `clear`, `cfg` | Clean final config | dirty no, flushing no, controlDirty no, scrollActive no, sleep no | 19.95 s | PASS serial | Final display not intentionally left in high-contrast static state |

Full per-command detail for the soak is in
`hil_logs\ssd1315_20260622_210029\results.csv` and `results.json`.

## Timing And Sampling Results

| Path | Count | Failures | Measured firmware timing | Effective rate |
| --- | ---: | ---: | ---: | ---: |
| `stress 500` contrast writes | 500 | 0 | 77 ms | 6493 ops/sec |
| `stress_mix 100` mixed operations | 100 | 0 | 3102 ms | 32 ops/sec |
| `flushstress 100` flushes | 100 | 0 | 2869 ms, avg 28 ms/flush | 34 flushes/sec |
| `burst 500` command writes | 500 | 0 | 68 ms | 7352 cmds/sec |
| 8-hour soak `stress_mix 500` blocks | 1511 blocks / 755500 ops | 0 | Max command latency 16.485 s | About 26.2 mixed ops/sec over full wall time |

The 8-hour soak command latency statistics from `run_stats.json` were:
min 0.421 s, mean 2.723 s, max 16.485 s.

## 8-Hour Soak Summary

| Field | Value |
| --- | --- |
| Start | 2026-06-22T21:00:29+02:00 |
| End | 2026-06-23T05:00:37+02:00 |
| Duration | 28808.375 s, about 8 h 0 m 8 s |
| Command count | 10575 |
| Command mix | `version` 1511, `cfg` 3021, `contrast 127` 1511, `clear` 3021, `stress_mix 500` 1511 |
| Mixed operation count | 755500 |
| Serial results | 6043 PASS, 4532 SERIAL_PASS_OPERATOR_REQUIRED, 0 FAIL, 0 REVIEW |
| Operator visual results | 4532 skipped serial-only, 0 completed |
| Reset/recovery count during soak | 0 |
| Serial reconnect count during soak | 0 observed |
| Worst command latency | 16.485 s (`stress_mix 500`) |
| Final soak command | `stress_mix 500` |
| Final cleanup | Post-soak functional serial run passed and ended with clean `cfg` |
| Final config after cleanup | initialized yes, dirty no, flushing no, controlDirty no, scrollActive no, sleep no |

The soak passed as serial evidence. It did not complete visual, thermal, reset,
or fault-injection evidence.

## Failures, Anomalies, And Limitations

- Upload anomaly: image write/hash verification passed, but esptool reported a
  serial configure error during hard reset. The firmware then responded on
  `COM29`.
- Initial smoke metadata anomaly: a clean worktree was recorded as `unknown`
  because the runner collapsed empty `git status --short` output. Fixed before
  later runs.
- Soak stopped at the duration deadline after `stress_mix 500`, before the
  planned final `clear` and `cfg`. A post-soak functional run restored and
  verified clean state.
- Visual validation was not run. All visual rows are serial evidence only.
- Missing-display, hotplug, power-cycle, reset-pin, and bus-fault tests were not
  run because the fixture safety and reset wiring were unknown.
- Logic analyzer capture was not run.
- Native ESP-IDF example build was not run locally because `idf.py` was not on
  PATH. CI configuration includes ESP-IDF builds, and the local static IDF
  contract check passed.

## Tooling Fixes Implemented

| Fix | Reason | Files | Verification |
| --- | --- | --- | --- |
| Preserve empty `git status --short` as clean metadata | Smoke run recorded clean worktree as `unknown` | `tools/run_ssd1315_hil.py` | `--parser-self-test`, parser tests, later metadata showed `dirty` correctly |
| Add aliases `--timeout-s`, `--idle-timeout-s`, `--boot-settle-s` | Prompt required these bounded controls | `tools/run_ssd1315_hil.py` | Parser tests and dry runs |
| Add `--parser-self-test` | Prompt required parser self-test mode | `tools/run_ssd1315_hil.py`, `tools/test_hil_runner_parser.py` | `python tools\run_ssd1315_hil.py --parser-self-test` |
| Add `run_stats.json` and summary run stats | Report needed elapsed/count/latency data | `tools/run_ssd1315_hil.py` | Generated in later HIL artifacts |
| Add bounded duration soak mode | Original soak was count-only, not an 8-hour deadline loop | `tools/run_ssd1315_hil.py` | 8-hour soak completed |
| Add benchmark mode | Needed bounded sample-rate evidence | `tools/run_ssd1315_hil.py`, parser tests | Benchmark HIL and tests passed |
| Accept Arduino `Health monitor:` wording | Functional monitor command otherwise needed review | `tools/run_ssd1315_hil.py`, parser tests | Functional HIL monitor rows PASS |

## Library And Repository Audit Findings

| Severity | Finding | Evidence | Risk | Simplest safe fix | Implemented |
| --- | --- | --- | --- | --- | --- |
| Medium | Duration soak and run-stat evidence were missing from the runner | Existing runner had count-only `soak_commands()` and no `run_stats.json` | Could not satisfy bounded 8-hour HIL evidence cleanly | Add duration loop, stats artifact, parser tests | Yes |
| Medium | Clean worktree was recorded as `unknown` in HIL metadata | Initial smoke artifact `host_worktree=unknown` despite clean pre-run status | Misleading evidence record | Preserve empty git status output distinctly | Yes |
| Low | Arduino monitor output was not accepted by classifier | Functional CLI prints `Health monitor: ON/OFF` | False review-required HIL rows | Accept both Arduino and IDF monitor wording | Yes |
| Low | `PANEL_NOT_READY` is documented but not used by the current flush gate | Static audit: flush delay returns `IN_PROGRESS` while waiting | Public docs may overstate observable status | Either document `IN_PROGRESS` for delay gate or return `PANEL_NOT_READY` from an explicit query path with tests | No |
| Low | README/API excerpt is narrower than public API | Static audit: public header has `pollFlush`, dirty helpers, scroll/fade/zoom helpers not in README excerpt | Users may miss supported diagnostics and bounded flush APIs | Label README section as a core excerpt or expand it | No |
| Low | "All commands" wording is too broad for write-only passthrough | Static audit: command reference excludes serial-mode read/status behavior | Overstates command coverage | Qualify as all supported SSD1315 write commands exposed as constants/passthrough | No |
| Medium | Void drawing/activity APIs can wake a sleeping panel and surface I2C failures only through diagnostics | Static audit; no HIL failure observed | Callers may miss wake failure if they do not inspect `lastError()` | Document best-effort behavior or add fallible alternatives after API decision | No |
| Low | Package/IDF static checks could cover more helper files semantically | Static audit: checker scans selected files and token patterns | Possible future drift in IDF helper headers | Add `IdfI2cTransport.h` to checker inputs and targeted lock/reset assertions | No |

No core driver fix was made from HIL evidence because the serial HIL run did not
show incorrect success after hardware failure, stale data, hangs, dirty-state
loss, or recovery failure. The unimplemented items are documentation/API or
test-coverage refinements rather than proven HIL regressions.

## Verification Results

| Check | Result |
| --- | --- |
| `python -m py_compile tools\run_ssd1315_hil.py tools\test_hil_runner_parser.py` | PASS |
| `python tools\run_ssd1315_hil.py --parser-self-test` | PASS |
| `python tools\test_hil_runner_parser.py` | PASS, 15 tests |
| `python tools\check_cli_contract.py` | PASS |
| `python tools\check_idf_example_contract.py` | PASS |
| `python tools\check_core_timing_guard.py` | PASS |
| `python scripts\generate_version.py check` | PASS |
| `python tools\check_package_contents.py` | PASS |
| `python -m platformio test -e native` | PASS, 87 tests |
| `python -m platformio run -e esp32s2dev` | PASS |
| `python -m platformio run -e esp32s3dev` | PASS |

`pio pkg pack` was not rerun in this pass to avoid modifying the tracked
`SSD1315-2.0.0.tar.gz` release artifact. Existing package content validation
passed.
