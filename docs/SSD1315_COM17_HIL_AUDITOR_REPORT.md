# SSD1315 COM17 HIL Auditor Report

Date: 2026-06-01

## Scope

This report records the COM17 serial HIL run requested after firmware was
re-uploaded to the panel. It is serial/device evidence plus operator-observed
screen activity. It is not a full field-ready hardware validation because the
runner was not used in interactive visual mode and no photos, video, fault
injection, power-cycle, or completed visual checklist were attached.

## Repository State

- Branch: `fix/ssd1315-industrial-gap-closure`
- Firmware commit reported by device: `00cc378`
- Host commit recorded by runner: `00cc378fea1d8a13e0bb16763a72e2d50ce94c3c`
- Host worktree during final HIL run: `dirty`
- Reason for dirty host worktree: the HIL runner was patched during diagnosis
  to fix command-completion parsing and artifact formatting. The firmware on
  the device was not changed during this diagnosis.

## Initial Failure Diagnosis

The first `mode all` runs reported `Serial device test: FAIL` even though the
display visibly responded. The serial transcripts showed that the display and
CLI were working. The failures were caused by HIL runner classification issues:

- `stress_mix 500` was still running, but the runner treated a short idle gap
  after `Running 500 mixed operations` as command completion and sent `clear`
  too early.
- The first `pattern checker` command could finish slightly after the default
  8 second command timeout.
- `selftest` can leave framebuffer data dirty, so the next mode's initial
  `cfg` must be treated as an intermediate state check, not a final clean-state
  assertion.
- The hardware matrix fragment parser did not understand the CLI format
  `width=128 height=64`, so it printed `unknownxunknown`.

No evidence in the transcript showed I2C transport failure, controller failure,
or failed clear/fill/scroll/control commands.

## Runner Fixes Made Before Final Run

- Added command-aware serial completion checks.
- Required stress commands to wait for `Results`, `Successes`, and `Failures`.
- Allowed selected intermediate `cfg` commands to report dirty framebuffer
  state without failing the serial device test.
- Increased `pattern checker` timeout budget.
- Parsed split `width=128 height=64` fields.
- Formatted I2C address as `0x3C` in the hardware matrix fragment.

Regression command run:

```bash
python tools/test_hil_runner_parser.py
```

Result: PASS, 11/11 tests.

## Final HIL Command

```bash
python tools/run_ssd1315_hil.py --port COM17 --baud 115200 --mode all --soak-ops 500 --out hil_logs --expect-address 0x3C --expect-width 128 --expect-height 64 --expect-controller SSD1315
```

## Final HIL Artifacts

- Log directory: `hil_logs/ssd1315_20260601_085559`
- Summary: `hil_logs/ssd1315_20260601_085559/summary.md`
- Raw transcript: `hil_logs/ssd1315_20260601_085559/serial_transcript.txt`
- Machine results: `hil_logs/ssd1315_20260601_085559/results.json`
- CSV results: `hil_logs/ssd1315_20260601_085559/results.csv`
- Failure analysis: `hil_logs/ssd1315_20260601_085559/failure_analysis.md`
- Matrix fragment: `hil_logs/ssd1315_20260601_085559/hardware_matrix_fragment.md`

## Final Verdicts From Runner

- Serial device test: PASS
- Serial review required: false
- Visual operator checks: INCOMPLETE
- Retention isolation: INCOMPLETE
- Soak: COMPLETE
- Field-ready evidence: NO

The failure analysis file says:

```text
No serial failures or review-required command results were detected.
```

## Important Serial Results

- `version`: PASS; firmware identity parsed.
- `scan`: PASS; expected address `0x3C` present.
- `probe`: PASS; probe reported OK. This is ACK evidence, not controller
  identity proof.
- `cfg`: final PASS; initialized, not flushing, not dirty, controlDirty=false,
  scrollActive=false.
- `selftest`: PASS; `fail=0`.
- Visual command serial side: `pattern checker`, `clear`, `fill`, invert,
  contrast, flip, scroll, display off/on, and recover all returned serial OK,
  but still require visual evidence.
- `stress_mix 100`: PASS; counters matched `N=100`.
- `stress_mix 500`: PASS; counters matched `N=500`, elapsed about 16.44 s.
- Final cleanup: `clear` PASS and final `cfg` PASS.

## Visual Observation

The operator reported that things were happening on the OLED screen. This is
useful bring-up evidence, but it is not a completed visual validation record.
For auditor-grade visual evidence, rerun with interactive visual capture and
attach photos or video for checkerboard, clear, fill, contrast levels, flip,
scroll, display off/on, and post-recover state.

## Remaining Auditor Gaps

- Worktree was dirty because the HIL runner was patched during diagnosis.
- Board, panel module, pull-ups, supply voltage, reset wiring, and bus speed
  metadata were not supplied to the runner.
- Visual checklist was not completed.
- Retention isolation was not completed because visual answers were not
  recorded.
- Fault tests such as missing display, unplug/replug, reset-pin behavior, and
  induced control dirty state were not run.
- No photos, video, or logic analyzer captures are attached in this repository.

## Recommended Next Auditor Run

After committing or otherwise freezing the runner fixes, rebuild/re-upload only
if firmware changed. Then run with metadata and interactive visual checks:

```bash
python tools/run_ssd1315_hil.py --port COM17 --baud 115200 --mode all --soak-ops 500 --out hil_logs --expect-address 0x3C --expect-width 128 --expect-height 64 --expect-controller SSD1315 --interactive-visual --operator <name> --board <board> --panel <module> --supply-voltage <voltage> --pullups <values> --reset-wired yes|no|unknown --bus-speed <hz>
```

Until that evidence is captured, the correct conclusion is: serial/device HIL
passed on COM17, but field-ready hardware validation remains incomplete.
