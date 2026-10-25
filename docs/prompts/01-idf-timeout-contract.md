# 01 — Make the native IDF adapter's timeout ownership explicit

Implement the SSD1315 audit finding **P2-T**, after checking that it still exists
on the current branch. Read [the prompt index](README.md),
[AGENTS.md](https://github.com/janhavelka/SSD1315/blob/main/AGENTS.md), and
[the finding](../RETAINED_BRANCH_AUDIT.md#p2-t-make-the-init-time-timeout-arguments-meaning-explicit).
Deliver the change and evidence; do not stop at another plan.

## Provenance and current behavior

This prompt was source-reviewed on 2026-10-25 at
`a7e9f1fb5c43192c9614fc30f3d7a5a873183c21` (`main`). Historical M2 is in
`62e475bb87e1a1c3cdfed9a07cd254487257cc9f`,
`docs/SSD1315_IDF_MERGED_INDUSTRY_READINESS_AUDIT.md`. That branch contains an
old report, not a missing driver patch. Read it through `git show` if needed;
do not merge or restore its old implementation assumptions.

The present IDF helper declares:

```cpp
bool initWire(int sda, int scl, uint32_t freq = 400000U,
              uint16_t timeoutMs = 50U, uint8_t address = 0x3CU);
```

`examples/common/IdfI2cTransport.cpp` calls `deinitWire()`, rejects a zero
timeout, and creates the mutex/bus/device. A positive timeout is neither stored
nor used to configure the bus. The name and call imply a policy that is absent.

Actual transfer policy is already explicit and bounded: `wireWrite()` receives
its timeout from the driver callback, includes mutex contention in the same
budget, rounds elapsed wait conservatively, and calls `i2c_master_transmit()`
once with the remaining milliseconds. The native example also supplies
`I2C_TIMEOUT_MS` to `Config::i2cTimeoutMs`; that is the real driver setting.
This finding is API clarity, not evidence of unbounded current transfers.

## Read and audit before editing

Inspect these actual owners and their callers, rather than relying on line
numbers from the old report:

- `examples/common/IdfI2cTransport.h` and `.cpp`: `Ssd1315IdfI2c`, `initWire`,
  `deinitWire`, `lastInitError`, `configUser`, `wireWrite`, `timeoutArg`, and
  `mapEspError`.
- `examples/espidf_basic/main/main.cpp`: `I2C_TIMEOUT_MS`, driver configuration,
  `app_main` initialization, and the separately timed diagnostic scan path.
- `examples/espidf_basic/main/CMakeLists.txt` and the local component wrapper:
  the actual native adapter source registration.
- `examples/common/I2cTransport.h`: the different Arduino helper. Its
  `initWire` timeout actually configures Wire and bounds its GPIO bus-clear
  path. Do not apply the IDF removal blindly to it.
- `test/native/test_i2c_transport.cpp`: these are **Arduino Wire** tests using
  `test/stubs/Wire.h`, not behavioral tests of the native IDF adapter.
- `tools/check_idf_example_contract.py`: current guard scans IDF sources for
  structural native-boundary and lock/budget tokens. It does not prove transfer
  attempt counts or deadline behavior; audit those in the callback itself.
- `docs/IDF_PORT.md`, `README.md`, `.github/workflows/ci.yml`, and `Doxyfile`.

Useful audit commands from the repository root:

```powershell
git status --short --branch
git fetch origin
git log -1 --format='%H %s'
rg -n 'initWire|I2C_TIMEOUT_MS|i2cTimeoutMs' examples test tools docs README.md
rg -n 'wireWrite|remainingMs|elapsedUs|xSemaphoreTake|i2c_master_transmit' examples/common/IdfI2cTransport.cpp
```

Confirm every initializer call, the distinction from Arduino, and whether any
new consumer makes compatibility necessary. Preserve unrelated work and
fast-forward only if safe. Keep the task on its existing branch.

## Preferred implementation

1. Remove the policy-looking timeout from the IDF initialization interface and
   its native caller. Prefer a clear IDF-specific initializer name, such as
   `initIdfI2c`, with explicit pins/frequency/address. This avoids a dangerous
   positional migration: simply deleting parameter four while keeping the old
   name could silently reinterpret an old four-argument timeout as an address.
   Select the smallest clear signature after inspecting actual callers.
2. Retain `Config::i2cTimeoutMs` and per-call `wireWrite` timeout as the transfer
   policy. Do not introduce a stored fallback timeout, second timeout owner,
   second mutex, retry loop, generic transport wrapper or hidden bus recovery.
3. Document the initialization/resource contract and callback budget beside
   the helper declarations. Update the native example, IDF guide and relevant
   release note together. Initialization allocates resources; it does not set
   transfer deadlines. Application ownership remains unchanged.
4. Preserve current `wireWrite` behavior: one backend attempt, shared
   mutex-plus-transfer budget, raw `esp_err_t` detail, no invented address/data
   NACK distinction, and lock release on all existing return paths. Do not
   refactor timing arithmetic simply because the initializer is changing.
5. If a live external dependency requires compatibility, document that concrete
   evidence and choose an explicit deprecated overload without ambiguous
   argument binding. Do not preserve a meaningless parameter speculatively.
   Avoid a lasting adapter layer whose sole purpose is hiding the old name.

Do not change the core `attach/start/poll/result` API, framebuffer ownership,
transaction count, display sequencing, reset policy or supported IDF range for
this cleanup. If auditing reveals a separate functional defect, record it with
evidence and isolate its regression/fix; do not silently enlarge this task.

## Acceptance and meaningful verification

- No native initializer implies that it configures transaction timeout. All
  native callers pass the intended address after migration. A stale old call
  cannot silently bind its timeout as the address.
- Changing `Config::i2cTimeoutMs` still controls the callback budget. Example
  docs identify the same single owner. Arduino initialization retains its real
  timeout behavior and existing tests.
- Native builds compile the real `IdfI2cTransport.cpp`, not a substitute or
  Arduino facade. Both supported target and SDK combinations still build.
- Preserve existing callback-budget/error contracts. If their implementation
  is changed, add a focused native-adapter harness for no contention, contention
  consuming part/all of a budget, backend timeout/error, and exactly-once mutex
  release. The existing Wire fake does not cover these cases. If the change is
  signature/documentation-only, do not create a large fake SDK or a test that
  merely repeats the declaration: actual native compilation and the existing
  focused guards are proportionate.

Run relevant existing commands on Windows using the installed wrapper:

```powershell
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python scripts/generate_version.py check
.\scripts\pio.cmd test -e native
doxygen Doxyfile
git diff --check
```

For actual native compilation, reuse `.github/workflows/ci.yml`'s
`esp-idf-build`: `examples/espidf_basic`, ESP32-S2/S3 and IDF v5.3.5/v5.5.5.
In each corresponding clean IDF environment use
`idf.py -C examples/espidf_basic set-target esp32s2` (or `esp32s3`) and
`idf.py -C examples/espidf_basic build`. Use isolated build state when switching
SDK/target. Arduino PlatformIO builds do not substitute for those checks.

Follow the shared CI policy: focused local checks for an isolated signature/
documentation cleanup; exact-final-commit terminal CI for any substantial
behavior/build/API change. Do not repeatedly dispatch unchanged suites or edit
triggers to avoid them. Report unavailable SDK/build checks as not run, and
never present a static token check as an IDF compilation result.

## Closeout

Update P2-T in `docs/RETAINED_BRANCH_AUDIT.md` and this index row with the chosen
API migration, resolution commit, actual commands/results and limitations.
Keep Q1 hardware qualification open. Stage only owned changes, commit, push
without force and verify local/upstream synchronization under `AGENTS.md`.
Summarize what was simplified and the verification actually performed. A
version/release is not required just to execute these prompts; if one is
otherwise requested, use the generator rather than editing `Version.h`.
