# SSD1315 Pre-HIL Operator Final Check

Date: 2026-05-31

Branch: `hardening/ssd1315-industry-readiness`

Commit at start of pass: `8fb2b2bd13de8db877d16b4a7562fdb9a1492cf7`

Final commit for this pass: recorded in the operator final response after
commit creation.

## Scope

This was an operator-polish pass only. No physical HIL validation was performed,
no hardware matrix results were filled, no release tag was created, and no
version bump was made.

## Files Changed

- `README.md`
- `docs/SSD1315_HARDWARE_VALIDATION.md`
- `docs/SSD1315_HIL_RUNBOOK.md`
- `docs/SSD1315_HIL_TARGET_TEMPLATE.md`
- `docs/SSD1315_PRE_HIL_OPERATOR_FINAL_CHECK.md`
- `tools/check_cli_contract.py`

## HIL Command Sequence

The README, hardware validation matrix, HIL runbook, HIL runner, and CLI
contract guard use the same executable sequence:

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

## PR / CI Status

`gh` was unavailable in this environment, so PR existence and CI status were not
queried. PowerShell reported:

```text
gh : The term 'gh' is not recognized as the name of a cmdlet, function, script file, or operable program.
```

Manual GitHub UI steps:

1. Open the repository on GitHub.
2. Go to the Pull requests tab.
3. Filter for source branch `hardening/ssd1315-industry-readiness` and base
   branch `main`.
4. Open the PR, if present.
5. Confirm the latest commit matches the final commit from this pass.
6. Confirm all required GitHub Actions jobs completed successfully.

No CI pass/fail status is claimed by this report.

## Checks Run

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

## Operator HIL Command

After building and uploading the selected firmware, run:

```bash
python tools/run_ssd1315_hil.py --port <serial-port> --baud 115200 --out hil_logs
```

Fill `docs/SSD1315_HIL_TARGET_TEMPLATE.md` before running the command and attach
the generated `serial_transcript.txt`, `summary.md`, photos/video, and optional
logic analyzer captures to the hardware validation record.

## Hardware Validation Statement

No physical HIL validation was performed in this operator-polish pass. The next
step is an actual HIL run on representative SSD1315 hardware.
