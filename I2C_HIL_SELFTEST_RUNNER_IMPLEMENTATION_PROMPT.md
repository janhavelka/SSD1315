# Prompt: Add A General I2C Hardware Self-Test / HIL Runner

You are working in an embedded I2C library repository. You will be given one
repository at a time. Do not assume the specific device type in advance. Audit
the actual code, examples, docs, and command surface before implementing.

This task is to add a focused host-side Python self-test / HIL runner and
operator documentation so the repository can be validated on real hardware and
produce auditor-ready evidence.

Do not do a broad refactor. Do not add unrelated features. Do not claim hardware
validation was performed unless real hardware was actually run.

---

## Core Goal

Add a Python script that can drive the repository's existing serial CLI or
diagnostic firmware on real hardware, capture a complete serial transcript,
summarize command results, mark manual checks clearly, and produce output that
can be sent to a technical auditor.

The script must use a complete repository-specific command set derived from the
actual CLI/docs/source. It must be honest:

- commands that exist may be run;
- commands that are missing must be documented as missing or added only if small
  and appropriate;
- commands that are unsafe must be opt-in or manual only;
- visual/manual validation must be marked `OPERATOR_CHECK_REQUIRED`, not PASS;
- address ACK must not be treated as chip identity unless the device has a real,
  documented identity register and that register is read successfully.

---

## Start Checks

Run these first:

```bash
git status --short
git branch --show-current
git rev-parse HEAD
rg -n "help|version|scan|probe|selftest|cfg|config|status|read|write|recover|reset|stress|monitor" examples src include docs tools || true
rg --files
```

If the worktree is dirty, inspect the diff and do not overwrite user work.

If the repository has branch rules in `AGENTS.md`, README, or docs, follow them.

---

## Use Subagents

Spawn subagents where available. Each subagent must report factual findings
before implementation choices are finalized.

Use at least these roles when the tooling supports subagents:

1. `repo-contract-agent`
   - Inspect public API contracts, docs, versioning, package metadata, and
     framework-boundary rules.
   - Report whether the core library must remain framework-neutral and whether
     the runner can be host-side only.

2. `cli-surface-agent`
   - Inspect Arduino, ESP-IDF, PlatformIO, native, or other diagnostic examples.
   - Inventory real serial commands, help output, parser syntax, and response
     tokens.
   - Identify command gaps and unsafe commands.

3. `device-semantics-agent`
   - Inspect datasheet notes, command/register docs, and existing driver code.
   - Determine whether the device is register-readable, write-mostly, sensor-like,
     display-like, memory-like, GPIO-like, RTC-like, ADC-like, etc.
   - Define what can and cannot be proven from I2C ACK, register reads, ID
     registers, measurements, or visual/operator evidence.

4. `hil-runner-agent`
   - Design the Python command model, timeout/completion logic, transcript
     capture, summary output, and dry-run behavior.
   - Check for false-positive failure patterns such as ordinary `timeout=50ms`
     text.

5. `docs-validation-agent`
   - Build an operator runbook and target template.
   - Ensure docs do not claim hardware validation unless evidence exists.
   - Ensure manual/visual checks and unsafe fault tests are clearly separated.

6. `ci-build-agent`
   - Inspect CI workflows, build targets, generated files, package artifacts, and
     local check commands.
   - Check whether `gh`, `platformio`, `idf.py`, or other tooling is available.
   - Do not invent CI status.

7. `integration-review-agent`
   - Review the final diff for scope, command/docs/runner consistency, false
     hardware claims, unsafe defaults, and generated artifacts.
   - Run or verify the contract guard before commit.

If subagents are unavailable, perform these audits yourself and document the
findings in the final report.

---

## Audit Existing Validation Surface

Before editing, inspect:

- README and docs;
- `AGENTS.md`;
- public headers and status/error model;
- Arduino examples;
- ESP-IDF examples, if present;
- PlatformIO environments;
- serial CLI handlers and help text;
- existing diagnostic scripts;
- existing hardware validation docs;
- CI workflows;
- generated version scripts;
- `.gitignore`.

Find the real command surface. Do not invent commands.

If no serial CLI exists, choose the smallest appropriate path:

- add a small diagnostic CLI only if it matches the repository style; or
- create the runner around the actual available interface and document what is
  missing.

---

## Required Script

Add:

```text
tools/run_i2c_hil.py
```

If the repository has a clear library/device name, it is acceptable to use a
specific filename such as:

```text
tools/run_<library>_hil.py
```

The script must:

- accept `--port`;
- accept `--baud`;
- accept `--out`;
- accept `--timeout`;
- accept `--dry-run`;
- optionally accept `--commands <file>` if a command-file mode fits the repo;
- create a timestamped log directory, for example:

```text
hil_logs/i2c_<timestamp>/
```

- never overwrite an existing log directory;
- save raw serial transcript to:

```text
serial_transcript.txt
```

- save Markdown summary to:

```text
summary.md
```

- save machine-readable summary to:

```text
summary.json
```

- optionally save:

```text
operator_checklist.md
```

- send commands one by one;
- wait for a CLI prompt, known completion token, serial idle interval, or timeout;
- classify serial-only commands as PASS/FAIL/REVIEW based on exact text where
  possible;
- mark manual/visual commands as `OPERATOR_CHECK_REQUIRED`;
- print the final operator checklist;
- fail gracefully if `pyserial` is missing, with this install hint:

```bash
python -m pip install pyserial
```

- never flash firmware automatically unless the repository already has an
  established flashing workflow and the operator explicitly passes an opt-in flag
  such as `--flash`.

---

## Command Model

Represent each command with metadata, not just strings.

Use a structure like this:

```python
CommandSpec(
    command="probe",
    purpose="ACK-only device presence check",
    expected=["Status: OK", "ACK", "OK"],
    failures=[
        "DEVICE_NOT_FOUND",
        "I2C_NACK_ADDR",
        "I2C_TIMEOUT",
        "I2C_BUS_ERROR",
    ],
    timeout_s=5.0,
    operator_check=False,
    destructive=False,
    requires_opt_in=None,
    recovery_command=None,
    notes=(
        "Probe proves address ACK only. It does not prove chip identity unless "
        "the device has a documented identity register and that register is read."
    ),
)
```

Each command should define:

- command string;
- purpose;
- expected serial tokens;
- failure tokens;
- timeout;
- whether operator observation is required;
- whether the command is destructive or modifies persistent state;
- opt-in flag required, if any;
- recovery command, if needed;
- notes for the auditor.

---

## Command Set Generation

Generate the repository-specific sequence from actual evidence.

Prefer this order when applicable:

1. `version` or equivalent build/version command.
2. `help` if useful for transcript context.
3. `scan` or bus scan.
4. `probe` or address ACK check.
5. `cfg`, `config`, `settings`, `status`, or equivalent.
6. Device identity check only if a real documented ID register exists.
7. Safe self-test.
8. Basic read operation.
9. Configuration readback.
10. Safe volatile configuration write and readback, if supported.
11. Data acquisition / measurement / transaction test.
12. Boundary validation commands.
13. Recovery command.
14. Stress command.
15. Optional soak command, gated behind an opt-in flag.
16. Final status/config read.
17. Restore-safe-state command.

Do not assume all I2C devices support every category.

Adapt based on device type:

- Register-readable sensors: read chip ID, configuration registers, measurement
  data, and status bits if available.
- Write-mostly displays/controllers: do not claim identity from ACK; use visual
  or operator checks where required.
- EEPROM/FRAM: avoid destructive writes by default; use a documented scratch
  address only with explicit opt-in.
- RTCs: avoid changing time by default; use read-only time/status checks unless
  explicitly authorized.
- GPIO expanders: avoid toggling pins connected to real loads unless explicitly
  authorized.
- ADCs/DACs: record expected physical input/output assumptions and mark them
  operator-required unless the fixture measures them.
- Power, relay, motor, or actuator boards: default to read-only diagnostics; all
  output-changing commands must be opt-in.

---

## Identity And Probe Honesty

Use these rules:

- I2C ACK proves only that something acknowledged the address.
- ACK is not chip identity.
- A scan is not a validation pass by itself.
- `DEVICE_NOT_FOUND` should be reserved for definite address NACK when the
  transport can distinguish it.
- Preserve timeout, data NACK, bus error, and generic transport errors when the
  CLI/library can distinguish them.
- Claim chip identity only when:
  - the datasheet defines a stable ID register or equivalent;
  - the script/firmware reads it;
  - the observed value matches documented expectations;
  - the transcript records the read and result.

---

## Safety Rules

The default command sequence must be safe.

Do not run commands by default that can:

- erase memory;
- overwrite calibration;
- permanently change configuration;
- write to EEPROM/flash;
- power-cycle hardware;
- hold outputs active;
- drive relays, motors, heaters, high-current loads, or external equipment;
- leave displays showing static high-brightness images;
- reset attached systems;
- hotplug or disconnect hardware;
- induce bus faults;
- require physical probing that could damage the board.

Gate these behind explicit flags such as:

```bash
--include-destructive
--include-soak
--include-fault-tests
--include-output-tests
```

If a command is unsafe and no opt-in flag is provided, write it to the operator
checklist as manual/future, not to the executable sequence.

---

## Serial Completion And Classification

The runner should handle both prompt-based and no-prompt CLIs.

Completion can be:

- prompt seen;
- known completion token seen;
- serial idle gap after output;
- timeout.

Avoid false failures. For example, do not treat ordinary text such as
`timeout=50ms` as a failed command. Use precise failure patterns:

- `Status: TIMEOUT`;
- `I2C_TIMEOUT`;
- `I2C_NACK_ADDR`;
- `I2C_NACK_DATA`;
- `I2C_BUS_ERROR`;
- `DEVICE_NOT_FOUND`;
- `FAILED`;
- `ERROR`, only if the repository does not print it in benign contexts;
- known repository-specific error tokens.

If output is ambiguous, classify as `REVIEW_REQUIRED`, not PASS.

Suggested result categories:

- `PASS`;
- `FAIL`;
- `SERIAL_OK_OR_REVIEW`;
- `REVIEW_REQUIRED`;
- `OPERATOR_CHECK_REQUIRED`;
- `SKIPPED_UNSAFE`;
- `TIMEOUT`;
- `NOT_IMPLEMENTED`.

---

## Output Requirements

`summary.md` must include:

- date/time;
- branch;
- commit hash if available;
- dirty/clean worktree state if available;
- serial port;
- baud rate;
- firmware/framework if reported;
- I2C address;
- device/module info if reported;
- detected or configured command sequence;
- per-command table:
  - command;
  - purpose;
  - serial result;
  - operator result;
  - wait/completion reason;
  - elapsed time;
  - notes;
- final verdict:
  - `PASS`;
  - `FAIL`;
  - `OPERATOR_REVIEW_REQUIRED`;
  - `INCOMPLETE`;
- paths to logs and artifacts.

`summary.json` must contain equivalent machine-readable data.

`serial_transcript.txt` must include:

- timestamp;
- port and baud;
- command boundaries;
- raw serial output.

---

## Auditor-Facing Documentation

Create or update:

```text
docs/I2C_HIL_RUNBOOK.md
docs/I2C_HIL_TARGET_TEMPLATE.md
docs/I2C_HIL_SELFTEST_REPORT.md
```

If the repository uses device-specific naming, use names like:

```text
docs/<DEVICE>_HIL_RUNBOOK.md
docs/<DEVICE>_HIL_TARGET_TEMPLATE.md
docs/<DEVICE>_HIL_SELFTEST_REPORT.md
```

The runbook must include:

- hardware preflight fields:
  - operator;
  - date/time;
  - branch;
  - commit hash;
  - dirty/clean worktree state;
  - MCU board;
  - framework;
  - build target;
  - serial port;
  - baud rate;
  - I2C address;
  - bus speed;
  - supply voltage;
  - pull-up values;
  - reset wiring;
  - interrupt pin wiring, if relevant;
  - device/module model;
  - chip marking;
  - fixture details;
  - firmware version;
- build commands for supported frameworks;
- upload commands without guessing ports;
- monitor commands;
- exact runner command;
- exact executable command sequence;
- expected serial results;
- manual/operator checks;
- unsafe/fault/recovery tests with explicit safety preconditions;
- evidence capture:
  - serial transcript;
  - summary files;
  - photos/video if relevant;
  - logic analyzer capture;
  - scope/meter readings if relevant;
  - operator notes.

The target template must be copy-pasteable for each hardware target and include:

- MCU board;
- build environment;
- serial port;
- device/module;
- I2C address;
- supply voltage;
- pull-ups;
- reset/interrupt wiring;
- exact build command;
- exact upload command;
- exact HIL runner command;
- evidence checklist;
- logic analyzer checklist if available.

The self-test report must state whether hardware was actually run. If not run,
say so clearly.

---

## Contract Guard

If the repo already has guard scripts, update them. Otherwise add:

```text
tools/check_hil_contract.py
```

The guard should fail if:

- the documented command sequence differs from `run_i2c_hil.py`;
- the README, runbook, hardware matrix, and runner disagree;
- destructive or unsafe commands appear in the default executable sequence;
- docs claim hardware passed without evidence;
- required runbook/template/report files are missing;
- the serial runner is syntactically invalid;
- `hil_logs/` is not ignored;
- the script omits `--dry-run`;
- the script omits pyserial install guidance.

Run the guard in local validation.

---

## Gitignore

Ensure generated HIL logs are not accidentally committed:

```gitignore
hil_logs/
```

Do not remove existing ignore rules.

---

## Local Validation

Run what is available locally. At minimum:

```bash
python -m py_compile tools/run_i2c_hil.py
python tools/run_i2c_hil.py --dry-run
python tools/check_hil_contract.py
```

Also run repository-specific checks when present:

```bash
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python scripts/generate_version.py check
python -m platformio test -e native
python -m platformio run -e <target>
python -m platformio pkg pack
```

If ESP-IDF is supported and `idf.py` is available, run the relevant IDF builds.
If `idf.py` is unavailable, record the exact failure.

If `gh` is available, check PR/CI status. If not, document exact manual GitHub
UI steps and do not invent CI status.

Never invent results. Record exact pass/fail/not-run status.

Remove generated package tarballs or build artifacts unless the repository
intentionally tracks them.

---

## Hardware Run Rules

Do not perform real hardware actions unless explicitly requested and the
operator has provided:

- serial port;
- target board;
- power/wiring confirmation;
- safe handling instructions for fault tests.

If no hardware is available, run only dry-run and software checks. State clearly:

```text
No physical HIL validation was performed.
```

---

## Final Report

Create or update:

```text
docs/I2C_HIL_SELFTEST_REPORT.md
```

Include:

1. branch;
2. commit hash;
3. whether a new branch was created;
4. files changed;
5. command sequence;
6. safety exclusions;
7. checks run and pass/fail results;
8. dry-run result;
9. hardware run result, or explicit not-run statement;
10. PR/CI status, or why it was not checked;
11. exact command the operator should run for HIL;
12. remaining blockers;
13. auditor-facing verdict:
    - `ready for HIL`;
    - `software-prepared only`;
    - `blocked`;
    - never `hardware validated` unless hardware was actually run and evidence
      was captured.

---

## Final Commit

Before finishing:

```bash
git status --short
git add -A
git commit -m "Add I2C HIL self-test runner"
git push
```

If push is not possible, commit locally and report the exact reason push failed.

Final response must include:

- branch;
- commit hash;
- whether pushed;
- files changed;
- checks run and results;
- exact HIL command for the operator;
- whether hardware validation was performed;
- remaining blockers.

---

## Final Reminder

The Python runner does not magically prove hardware correctness. Its job is to
produce disciplined evidence:

- exact command sequence;
- exact serial transcript;
- clear software pass/fail classification where possible;
- explicit operator-review fields where physical behavior must be observed;
- no unsupported identity, compatibility, CI, or hardware-validation claims.
