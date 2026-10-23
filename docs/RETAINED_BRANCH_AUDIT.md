# Retained audit and port branch review

Reviewed on 2026-10-23 against `main` at
`62561b163c6005900797c097fa92d5bf723c83b4`, after fetching `origin` and
confirming `main == origin/main`. Main is the newest branch tip by commit
date. The worktree was clean before this documentation change. Current-source
line references below identify that baseline, not the old report.

**Decision:** the unique audit commit contains only a historical report; no
driver implementation is stranded on it. Its important software findings
are addressed or superseded by the current cooperative operation model.
Physical qualification remains open, and the old timeout-parameter concern
has a small residual cleanup item. Archive the unique report tip and retire
it after this reconciliation is accepted. Port/hardening refs listed below
are already merged. No branch deletion, merge, or code fix was performed.

## Exact retained-ref inventory

Counts use `git rev-list --left-right --count main...origin/<branch>`:
main-only / branch-only commits. This inventory includes the older audit and
supporting development branches so their existence is not mistaken for
unfinished integration.

| Branch | Exact tip | Counts | Disposition |
| --- | --- | --- | --- |
| `audit/ssd1315-idf-merged-industry-readiness` | `62e475bb87e1a1c3cdfed9a07cd254487257cc9f` | 78 / 1 | Unique historical report; archive then retire after review acceptance. |
| `feature/ssd1315-idf-port` | `19d98b7ab2c194a6f5732c9af347523d6de6d55c` | 79 / 0 | Already merged. |
| `hardening/ssd1315-industry-readiness` | `7b69d96a94d48ec116b6da5e15aaa3227ec6b2b1` | 64 / 0 | Already merged. |
| `fix/ssd1315-industrial-gap-closure` | `ccb5f87417d1212d6b3fa73e6ff5451854562a71` | 50 / 0 | Already merged. |
| `hardening/tunnelmonitor-suitability-reaudit` | `8eb9c3d62f948d23f3e427d300e0ee4d15fc06a2` | 31 / 0 | Already merged. |
| `new_audit` | `9e924789a16049daa48dda615ac115e3245b94c1` | 116 / 0 | Already merged. |
| `agent/pioarduino-55-03-311` | `1eeda91a8a71e3c92a4dd515a64e428aa907dc88` | 19 / 0 | Already merged. |
| `nonblocking-manager` | `bf5e800f9fe03665c08d1c63d570f37462350aef` | 128 / 0 | Already merged. |
| `example_tuning` | `55f9bc463f314a2ed5d1b3abdf86cbc80b22659e` | 139 / 0 | Already merged. |
| `general_agents_check` | `dbaf040c971941d20891d2a83a0bcaa1d8361d81` | 148 / 0 | Already merged. |

The only branch-only commit is
`62e475bb87e1a1c3cdfed9a07cd254487257cc9f` (`feat: add SSD1315 IDF-Merged
Industry-Readiness Audit documentation`). It adds
`docs/SSD1315_IDF_MERGED_INDUSTRY_READINESS_AUDIT.md`, 392 lines, with no code
changes. `git cherry main origin/audit/ssd1315-idf-merged-industry-readiness`
returns `+` because the historical report itself was not patch-equivalently
merged. The report already acknowledges that the IDF port was merged at
`1c7e300b716caaa78b374a568adf050a026bbd8b`; its presence does not indicate a
missing port.

## Historical findings reconciled with current implementation

H/M/L identifiers match the retained report. Implementation evidence is
source and test coverage at the reviewed baseline. It is not a fresh
hardware result or a claim that an older CI run verifies today's commit.

| Historical finding | Current disposition and evidence |
| --- | --- |
| H1: no native IDF build validation in CI | Implemented CI coverage. `.github/workflows/ci.yml` has an `esp-idf-build` matrix for ESP32-S2/S3 at IDF v5.3.5 and v5.5.5, plus a direct native-example contract check. Component manifest supports `>=5.3.0,<6.0.0`; the example has a stable local component wrapper. A local native IDF build was not run during this review. |
| H2: blocking begin monopolizes bus | Superseded by the v4 owner-controlled model. `src/SSD1315.cpp:751` implements passive `attach()`, 930 admits initialization, and 1200 progresses explicit operations under transaction/byte/deadline budgets. `begin()` and `recover()` remain documented bounded compatibility facades over the same state machine. Current bounds are in `README.md:392` and public header at `include/ssd1315/SSD1315.h:479`; the old estimate of roughly 53 writes is not the current contract. |
| M1: display-on before GDDRAM clear | Implemented. The cooperative resync orders init/flush before `DISPLAY_ON` (`src/SSD1315.cpp:1309` and 1350); page-buffer initialization stays off until the owner has rendered/flushed all windows. `test/native/test_basic.cpp:583` asserts the golden command sequence, 1024 zero data bytes and last data before display-on; tests at 2945 and 3066 cover lifecycle failure stages. |
| M2: ignored initWire timeout argument | Substantially addressed transfer semantics, residual P2-T below. IDF `initWire()` now rejects zero, but otherwise does not apply/store its positive argument (`examples/common/IdfI2cTransport.cpp:61`). Actual transfers use the callback timeout, subtract mutex wait and bound the one transmit attempt (`:134`, 153-170). `docs/IDF_PORT.md:34` describes the total callback budget. The parameter remains a naming/contract cleanup opportunity; it does not make transaction timeouts unbounded. |
| M3: dynamic framebuffer default | Addressed by explicit ownership policy, not by removing an intentional convenience. `README.md:186` recommends caller-owned `externalBuffer` for deterministic production memory and gives a static-buffer example. `attach()` validates/binds storage and handles allocation failure; tests at `test/native/test_basic.cpp:812`, 838 and 855 cover ownership, size and page-window sizing. Examples are labeled bring-up diagnostics, so a new production framework or allocator injection is not required by this old suggestion. |
| L1: overly broad addresses and identity claim | Implemented strict SSD1315 address policy: `0x3C` or `0x3D`, tested by `test_begin_rejects_non_ssd1315_i2c_address_and_contrast_zero` at `test/native/test_basic.cpp:499`. Probe is explicitly ACK-only; it does not identify the controller. A speculative custom-address mode from the report is not part of the current scope. |
| L2: duplicate native tests | Closed. The authoritative suite is `test/native/`; root `test/test_basic.cpp` is now a five-line collection shim including `native/test_basic.cpp`, not a second implementation. Current PlatformIO test collection and instructions point at that maintained suite. |
| L3: implicit move suppression | Implemented. Copy/move operations are all explicitly deleted (`include/ssd1315/SSD1315.h:234`); static assertions at `test/native/test_basic.cpp:449` cover copy/move construction and assignment. |
| Missing partial-state/fault contracts | Implemented in the current operation/result and dirty/control-state models. Tests at `test/native/test_basic.cpp:3066`, 3345 and 3384 cover physical-stage failures, ambiguous flush failure and cancellation; failed flush retains dirty data and partial/raw controls require resync. |
| Missing timing/transaction-count tests | Implemented. Full-frame transaction/chunk counts at `test/native/test_basic.cpp:2276`; cooperative transaction/byte budget, exact deadline/wraparound and cancellation tests are in the same suite. These host tests do not replace bus measurements. |
| Missing hardware validation | Partially closed by dated serial HIL records; Q1 below remains. The old blanket claim of no hardware evidence is obsolete, but current ledgers explicitly leave visual, electrical, physical-fault and cooperative-owner qualification open. |

The old P2 request for an optional nonblocking initialization engine is
already implemented by the cooperative API. The suggestion to create a
production shared-bus example is now best treated as an owner-fixture task
under Q1; bus management still belongs to the application. Do not restore old
allocation, hidden scheduling, or compatibility behavior to match the report.

## Remaining actionable work

### Q1: qualify the actual panel, framework and cooperative bus owner

Priority: **required before field-grade or representative hardware claims**.
This is an evidence gap, not a missing patch in the retained report branch.

The [hardware ledger](SSD1315_HARDWARE_VALIDATION.md) records partial serial
HIL on ESP32-S3/COM21, including 97,000 operations over 3,612.265 seconds on
2026-07-31, and older S2 evidence. Its exact source/setup limitations matter:
the latest recorded fixture had unknown panel/controller identity and no
operator visual record; it used Arduino blocking compatibility diagnostics,
not a native IDF or cooperative owner fixture. It also predates the reviewed
main and cannot automatically qualify later code.

Acceptance, using [the HIL runbook](SSD1315_HIL_RUNBOOK.md):

1. Record exact source/build, module/controller authority, board/framework,
   supply/pull-ups, reset/IREF/pump/COM/remap configuration and instruments.
2. Capture visual cold boot, clear/fill/pattern/orientation/scroll,
   full/partial/page-window flush, sleep/wake and retention observations.
3. Use a safe fixture for absence, reconnect, NACK/timeout/bus errors, reset,
   recovery and dirty-data retention; distinguish unrun rows from pass.
4. Exercise `attach/start/poll/result` ownership on representative S2/S3
   Arduino and IDF targets, including one-attempt callbacks, deadlines,
   cancellation/result lifetime and mixed-device scheduling. Capture bus traces.
5. Extend representative multi-unit/thermal soak and attach logs/visual
   records to a validation or release record, updating the maintained ledger.

No hardware was operated in this review. Serial success alone does not prove
controller identity, display appearance, electrical behavior or field reliability.

### P2-T: make the init-time timeout argument's meaning explicit

Priority: **low, example-adapter cleanup**. Positive `initWire(timeoutMs)`
values are only validated as nonzero; the real transfer timeout comes from
`Config::i2cTimeoutMs` passed to each callback. The public example helper
declaration does not explain this distinction. The old report's exact
`(void)timeoutMs` implementation is gone, but its clarity concern is not fully
closed.

Acceptance: remove/rename the unused policy-looking argument and update its
callers, or document its validation-only role and the actual callback-owned
budget explicitly. Preserve one total mutex-plus-transmit timeout, add the
appropriate focused contract/adapter check, run native/CLI checks and build
the native S2/S3 example. Do not add another bus timeout owner or claim an
init parameter sets a transfer timeout when it does not. No change to this
helper was made during this documentation review.

## Retain/retire workflow and reproduction

Read-only commands from this checkout (substitute the recorded baseline SHA
for `main` to reproduce original counts after future commits):

```powershell
git status --short --branch
git for-each-ref --sort=-committerdate --format='%(refname:short) %(objectname)' refs/remotes/origin
git rev-list --left-right --count main...origin/audit/ssd1315-idf-merged-industry-readiness
git log --format='%H %s' main..origin/audit/ssd1315-idf-merged-industry-readiness
git cherry main origin/audit/ssd1315-idf-merged-industry-readiness
git diff --stat main...origin/audit/ssd1315-idf-merged-industry-readiness
git show 62e475bb87e1a1c3cdfed9a07cd254487257cc9f:docs/SSD1315_IDF_MERGED_INDUSTRY_READINESS_AUDIT.md
git merge-base --is-ancestor origin/feature/ssd1315-idf-port main
git merge-base --is-ancestor origin/hardening/ssd1315-industry-readiness main
git merge-base --is-ancestor origin/fix/ssd1315-industrial-gap-closure main
git merge-base --is-ancestor origin/hardening/tunnelmonitor-suitability-reaudit main
```

1. Keep Q1 in the maintained evidence ledger/runbook and track P2-T against
   current code. This document is a current disposition record, not a request
   to restore the old report as an unresolved roadmap.
2. Archive the unique report tip using an agreed archival ref or verified Git
   bundle before deleting its last reference. Writing its SHA here does not
   prevent unreachable-object garbage collection.
3. Once this reconciliation/archive is accepted, retire the report branch.
   The zero-divergence refs are eligible for routine merged-branch cleanup.
4. For future material behavior/build changes, run relevant local gates and
   verify remote CI for the exact final implementation commit. Do not reuse
   a historical green run as validation of new code.

## CI and validation

The existing CI was inspected before editing. It uses PlatformIO 6.1.19,
Arduino S2/S3 plus `compat_pioarduino_54_s3`, native tests, timing/CLI/IDF
guards, generated version check, HIL parser/dry-run plans, Doxygen/Graphviz,
package checks, and native IDF S2/S3 at v5.3.5 and v5.5.5.

Focused local commands for this documentation review:

```powershell
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python scripts/generate_version.py check
.\scripts\pio.cmd test -e native
doxygen Doxyfile
git diff --check
```

Local results on 2026-10-23: timing, CLI and IDF guards PASS; generated
version check PASS; native tests **157/157 PASS**; Doxygen PASS;
`git diff --check` and documentation-link checks PASS. The first scripted
PlatformIO invocation used a forward-slash batch-file path and failed before
starting PlatformIO; rerunning the repository wrapper with its resolved
Windows path produced the successful native result above.

Arduino/native IDF builds, packaging, hardware and remote CI dispatch were
not performed for this documentation-only change. Validation of a future
implementation remains separate from this branch review.
