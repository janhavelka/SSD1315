# SSD1315 HIL Validation Report - COM21 - 2026-07-22

Status: accepted as partial v4 serial HIL for the configured 128x64 device at
address `0x3C`, including a measured one-hour soak. This report does not claim
controller identity, visual correctness, electrical qualification, fault-
injection coverage, reset-pin coverage, or production TunnelMonitor
integration.

## Run Context

| Field | Value |
| --- | --- |
| Repository branch | `main` |
| Earlier suite firmware | `5c84e3496d9f0274689940636bf4efc7935100f8` (`5c84e34 clean`) |
| Final soak firmware | `074463ed4baddf56d031e58e178430ee023d35ed` (`074463e clean`) |
| Firmware package version | `4.0.0`; the pending 4.0.1 change contains release metadata, documentation, CI/tooling/tests, and no later core-driver or example-firmware change |
| Host checkout during final soak | `main` at `074463ed4baddf56d031e58e178430ee023d35ed`, dirty with 4.0.1 release metadata and documentation outside the flashed clean worktree |
| Framework / build | Arduino, PlatformIO `esp32s3dev` |
| PlatformIO board definition | ESP32-S3-DevKitC-1-N8, 8 MB flash layout, PSRAM disabled in the diagnostic build |
| Physical MCU/flash detected during upload | ESP32-S3 QFN56 rev 0.2, embedded 8 MB PSRAM, 16 MB flash, USB Serial/JTAG |
| Serial target | `COM21`, 115200 baud |
| Board context | TunnelMonitor HW2.00; diagnostic example used SDA GPIO8, SCL GPIO9, 400 kHz |
| Display configuration | 128x64, `0x3C`, `example-default-128x64-internal-charge-pump` |
| Panel/controller identity | Unknown; address ACK and successful writes do not identify the controller |
| Supply / pull-ups | Unknown / unknown |
| Reset | Not wired to the diagnostic; no reset-pin test |
| Operator evidence | Serial-only; visual rows were skipped rather than promoted to pass |

The shared-bus scan returned ACKs at `0x3C`, `0x41`, `0x50`, and `0x51`.
Only `0x3C` was addressed by the display diagnostic. No device identity is
inferred from those ACKs.

## Accepted Evidence

Raw `hil_logs` directories are local and untracked. This report is the durable
summary. Each normally completed directory contains the exact argv, serial
transcript, per-command JSON/CSV, parsed configuration, health delta, failure
analysis, and run statistics.

| Run | Artifact directory | Result | Coverage |
| --- | --- | --- | --- |
| Smoke | `hil_logs\ssd1315_20260722_145929` | PASS: 7/7 serial rows | Version, telemetry, exact scan parsing, ACK-only probe, config, self-test 20/20, final clean config |
| Functional | `hil_logs\ssd1315_20260722_145945` | PASS serial: 10 PASS, 20 operator-required, 0 fail | Draw/control/scroll/recover, stress 100, mixed stress 100, final clean config |
| Retention | `hil_logs\ssd1315_20260722_150012` | PASS serial: 8 PASS, 18 operator-required, 0 fail | Ghosting-isolation command sequence and retention demo 12/12; no visual observation |
| Benchmark | `hil_logs\ssd1315_20260722_150039` | PASS serial: 5 PASS, 3 operator-required, 0 fail | Stress 500, mixed stress 100, flush stress 100, burst 500 |
| Extended Arduino, initial | `hil_logs\ssd1315_20260722_150857` | PASS serial: 30 PASS, 25 operator-required, 0 fail | Original 55-command plan; found and verified the page-iteration fix |
| Response-boundary smoke | `hil_logs\ssd1315_20260722_175629` | PASS: 7/7 serial rows | Intermediate clean `62397d2` firmware identity and baseline health |
| Compact soak qualification | `hil_logs\ssd1315_20260722_175637` | PASS: soak COMPLETE | Intermediate one-batch qualification before the second boundary defect was found |
| Final one-packet smoke | `hil_logs\ssd1315_20260722_181638` | PASS: 7/7 serial rows | Runner 2.7, exact clean `074463e` firmware, baseline health, zero retries |
| Final one-packet soak qualification | `hil_logs\ssd1315_20260722_181646` | PASS: soak COMPLETE | One 500-operation batch in 21.859 seconds, exact compact record, clean final config, zero retries |
| Replacement one-hour soak | `hil_logs\ssd1315_20260722_181812` | PASS: soak COMPLETE | 3,641.156 soak seconds; 193 exact batches / 96,500 operations; 0 failures/retries; stable heap/reset; clean final config |
| Post-soak smoke | `hil_logs\ssd1315_20260722_191948` | PASS: 7/7 serial rows | Exact clean `074463e` identity, baseline health, clean config, zero retries |
| Post-soak extended Arduino | `hil_logs\ssd1315_20260722_191955` | PASS serial: 52 PASS, 25 operator-required, 0 fail | All 77 safe commands; clean final config; zero retries; no raw passthrough |

The benchmark reported 500/500 basic stress operations, 100/100 mixed
operations, 100/100 flushes, and 500/500 burst commands. The initial corrected
extended run completed in 28.25 seconds and ended initialized, awake, not
flushing, with no dirty framebuffer, no dirty control state, and no active
scroll.

The accepted duration run took 3,645.219 host seconds overall and produced
1,162 accepted command rows. Its 387 telemetry samples had strictly increasing
uptime and loop heartbeat, constant reset reason 11, 344,848-byte free heap,
342,888-byte minimum free heap, and zero driver failures. The transcript
contained one firmware-identity command, two config commands, 193/193 exact
compact records, no host-read event, and one final-cleanup section.

## Hardware-Found Defects And Fixes

The first extended run exposed a diagnostic lifecycle defect. In full-buffer
mode, `pageiter` called only one flush tick and reported completion. That could
leave GDDRAM dirty or incompletely synchronized, after which `sleep 0`
correctly rejected wake with `STATE_ERROR` because a complete clean GDDRAM
baseline was unavailable.

The example was refactored to one page-iteration owner that waits for a complete
full-buffer flush and, in page-buffer mode, completes every selected window
before advancing. The corrected sequence passed on COM21, and the native
page-buffer/flush tests remained green. This was an example-owner bug; the core
wake safety gate behaved correctly.

Hardware use also verified fixes for exact CLI command matching (`ver` versus
`verbose`, `fill` versus `fillrect`, and `flush` versus `flushrect`) and the
advertised `?` alias.

The first compact one-hour attempt completed its device work but exposed a
USB-CDC command-response boundary defect: nine final compact records were held
until the next request. The runner refused to attribute those late records and
therefore rejected the run. Flushing at the Arduino command boundary removed
that cross-command delay but was not sufficient: a subsequent duration run
received only the first 128 bytes of a compact result and timed out without
sending another request. That run was also rejected.

The shared Arduino/ESP-IDF machine record was then reduced to a newline-
terminated line shorter than one 64-byte USB CDC packet. Runner 2.7 parses the
exact count, driver delta/state, and error code from that line and stops after
any unrecovered timeout, including partial output, so late data cannot be
misattributed. Clean `074463e` smoke and 500-operation qualification runs
completed with no retry or spillover before the replacement duration run.

## Rejected Preliminary Soak Attempts

Rejected directories remain local failure evidence and are not counted as the
one-hour pass:

- `hil_logs\ssd1315_20260722_151047` stopped after 954.703 soak seconds when a
  `version` response was truncated. It completed 45 batches / 22,500 mixed
  operations with zero driver failures, stable heap, and monotonic telemetry,
  but failed the duration gate.
- `hil_logs\ssd1315_20260722_153031` exercised a close/reopen recovery design.
  Reopening USB serial reset the MCU, visible through decreased uptime and
  reset counters. The run was terminated and the recovery design removed.
- `hil_logs\ssd1315_20260722_154031` reached 3,573.797 soak seconds and 73,500
  completed mixed operations with zero driver failures. Four missing/truncated
  host reads and verbose output prevented a complete one-hour verdict, so it
  was rejected rather than rounded up.
- `hil_logs\ssd1315_20260722_164412` was intentionally superseded after the
  compact `soakstep` evidence contract was added; it is not duration evidence.
- `hil_logs\ssd1315_20260722_165248` met the 3,600-second duration and the
  device produced 124 exact 500-operation records (62,000 operations), stable
  heap, unchanged reset reason, and clean final config. Runner 2.4 rejected
  nine records whose USB-CDC delivery crossed the following command boundary.
  This run found the response-boundary defect and is not accepted as the pass.
- `hil_logs\ssd1315_20260722_175711` stopped after 659.781 soak seconds. It
  accepted 25 complete batches / 12,500 operations with zero driver failures,
  stable heap, unchanged reset reason, and no host retry, then received only
  the first 128 bytes of cycle 26's compact record. Runner 2.6 timed out and
  correctly sent no later request. This run proved command-boundary flushing
  alone was insufficient and is not accepted as the pass.

Same-handle read retries remain limited to idempotent `version`, `telemetry`,
and `cfg` commands. They never reopen the port, renew the soak deadline, or
retry writes, stress work, serial exceptions, or explicit device/I2C failures.
Every retry remains visible in the artifacts.

## TunnelMonitor-node Assessment

The COM21 scan provides limited evidence that the standalone diagnostic can
address the display while three other devices ACK on the physical bus. It does
not qualify a production TunnelMonitor adapter or mixed-device scheduling.

TunnelMonitor-node was inspected read-only at `prompt-45-platformization`
revision `4d7555a2306b38032d7f6cbb15ccb29674fcecca` after Prompt 45F. Its exact
committed state passed 1,109/1,109 native tests in a clean detached worktree.
The committed changes after the earlier tested `710d3ac` baseline concern
Cloud/profile projection, not the I2C/display contract. The live checkout then
contained uncommitted Prompt-45G I2C-owner work; those user changes were not
built or modified. The tested display code is the existing direct-I2C
implementation, not the future SSD1315 module.

Adoption remains gated by:

- authoritative module/controller and electrical-profile identification,
  because the SSD1315 profile sends `SET_IREF`;
- reconciling the 2,500 ms display operation deadline with the 1,250 ms
  protected-result cutoff so identity cannot be reclaimed while work can still
  finish;
- a private module callback that performs one physical attempt with no hidden
  retry, recovery, or ambiguous-write replay;
- immutable dependency selection and remote fetchability;
- production owner-fixture tests for deadlines, cancellation,
  absence/reconnect, and mixed-device scheduling.

The planned private `Ssd1315Module` should own the SSD1315 instance, external
1,024-byte framebuffer, and presentation state. Generic `I2cTask` should remain
free of display-protocol state and own only generic bus transactions. No
TunnelMonitor source or dependency was changed by this validation.

## Validation Limits

- No operator visual pass/fail, photo, or video was captured.
- No panel/controller marking, supply voltage, pull-up value, or IREF wiring was
  established.
- No display reset GPIO was connected to the diagnostic.
- No unplug/replug, missing-display, bus-fault, power-cycle, thermal, or logic-
  analyzer test was attempted because the fixture safety and wiring were not
  specified.
- Arbitrary raw `cmd*` passthrough was intentionally excluded. Sending every
  possible raw controller byte sequence is unsafe and can invalidate modeled
  panel state; host tests cover raw-command invalidation and resync contracts.
- The Arduino diagnostic uses bounded blocking compatibility calls; hardware
  validation of v4 `attach/start/poll/result`, one-callback polling, owner
  cancellation, and result lifetime requires a dedicated production fixture.
- Serial success is not a field-grade or SSD1306-compatibility claim.
