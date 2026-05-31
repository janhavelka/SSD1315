# SSD1315 Clear/Ghosting Diagnostic Report

Status: software diagnostic complete; physical panel retention/burn-in is still
unverified.

## 1. Branch And Commit

- Branch: `hardening/ssd1315-industry-readiness`
- Diagnostic base commit: `adba5c610f569f5017358a4a7f810376e1ed06b8`
- Worktree at start: clean

## 2. Observed Symptoms

The operator reported that commands reach the SSD1315 panel, but visual output
does not look clean:

- stale content can remain visible during stress tests;
- `clear` can appear not to blank the panel;
- `fill` visibly fills the screen, then a later `clear` can leave old or bright
  content visible;
- `demo 1` can show old content underneath new graphics;
- OLED image retention or burn-in is suspected but not yet proven.

## 3. Root-Cause Status

Software bug found and fixed for one credible stale-GDDRAM path.

The driver could previously clear a page dirty bit after an in-progress flush
even when the framebuffer had been changed during that page transfer. Example:
`fill()`, start a budgeted flush, send the first half of page 0 as `0xFF`, then
call `clear()`. The second half of page 0 would be sent as `0x00`, but page 0
could still be marked clean at completion. That can leave earlier `0xFF` bytes
in GDDRAM without a pending retry.

This does not prove or disprove panel image retention. Hardware retention is
still undetermined because no display-off, power-cycle, photo/video, or
second-panel test was run in this environment.

## 4. Evidence Used

Code inspection found:

- `clear()` and `fill()` overwrite the framebuffer and mark pages dirty.
- `tickFlush()` reads data from the current framebuffer at send time.
- Before this fix, page completion cleared dirty state unconditionally for the
  page being flushed.
- Command/data control bytes remain correct: command streams use `0x00`; data
  streams use `0x40`.
- Address-window writes precede flush data.
- Hardware scroll blocks flush through `STATE_ERROR` until stopped.

Native tests added now prove:

- `fill()` flush sends all `0xFF` GDDRAM bytes.
- `clear()` after a completed `fill()` flush sends all `0x00` GDDRAM bytes.
- mutating the framebuffer during an active partial flush keeps the affected
  page dirty for retry;
- the retry sends current framebuffer bytes, not stale bytes from the previous
  flush attempt;
- a failed partial flush followed by `clear()` retries with zero bytes;
- display-off/display-on and display-all-on/display-RAM commands use the
  expected SSD1315 command bytes.

No serial transcript, logic-analyzer capture, photos, videos, display-off
persistence result, power-cycle result, or second-panel result was available.

## 5. Code Changes

- Added dirty-generation tracking per page. A page dirty bit is cleared after a
  page transfer only if no framebuffer mutator marked that page dirty during the
  transfer.
- Added native regression tests for clear/fill byte streams, active-flush
  framebuffer mutation, failed-flush retry with current framebuffer data, and
  display-off/all-on command bytes.
- Added `display <off|on>` diagnostic CLI aliases in Arduino and native ESP-IDF
  validation CLIs. These call the existing sleep/display-off control path.
- Made the Arduino and native ESP-IDF `demo` paths establish a baseline before
  drawing: stop scroll, disable all-pixels-on, disable invert, wake display,
  clear, and abort if the baseline clear/flush fails.
- Updated CLI contract checks for the new aliases.
- Added runbook and hardware-matrix guidance for distinguishing live GDDRAM
  corruption from OLED retention/burn-in.

## 6. Tests And Local Checks

Passed locally:

- `python tools/check_core_timing_guard.py`
- `python tools/check_cli_contract.py`
- `python tools/check_idf_example_contract.py`
- `python scripts/generate_version.py check`
- `python -m py_compile tools/run_ssd1315_hil.py`
- `python tools/run_ssd1315_hil.py --dry-run`
- `python -m platformio test -e native`
- `python -m platformio run -e esp32s3dev`
- `python -m platformio run -e esp32s2dev`
- `git diff --check`

## 7. Hardware Sequence

Hardware was not run in this diagnostic pass. The next operator run should use:

```text
version
cfg
recover
scroll stop
invert 0
clear
fill
clear
pattern checker
clear
demo 1
clear
cfg
clear
contrast 1
clear
contrast 127
clear
fill
clear
display off
display on
recover
clear
cfg
```

Record whether any ghost remains while `display off` is active and whether it
persists across a safe power cycle before any drawing. If it does, that points
strongly to panel retention, optical residue, or burn-in.

## 8. HIL Resume Verdict

HIL should not treat previous ghosting observations as panel-retention evidence
until the new firmware/tests are used. Real HIL may resume after CI/local builds
pass, but the first hardware run should include the clear/ghosting isolation
sequence above.

## 9. Remaining Risks

- Physical OLED retention or burn-in may still be present and requires
  display-off and power-cycle evidence.
- Raw diagnostic commands can still put the controller into unusual modes; use
  `recover`, `scroll stop`, `invert 0`, `display on`, and `clear` before judging
  visual output.
- A logic-analyzer capture is still useful for confirming command control byte
  `0x00`, data control byte `0x40`, page/column windows, and zero payloads on
  the actual board.
