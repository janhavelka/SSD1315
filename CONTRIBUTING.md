# Contributing

Thank you for considering contributing to this project!

## Quick Start

1. Fork the repository.
2. Create a focused branch: `git switch -c feature/my-feature`.
3. Make the smallest coherent change and update public Doxygen/README contracts.
4. Run the validation appropriate to the change.
5. Add a Keep a Changelog entry under `[Unreleased]`.
6. Commit with a clear Conventional Commit message, then push and open a pull
   request.

## Guidelines

### Architecture And Code Style

- Keep `include/ssd1315/` and `src/` framework-neutral. Arduino, ESP-IDF,
  FreeRTOS, bus handles, pins, locks, reset, retry, and recovery policy belong
  in examples/adapters or the consuming application.
- Preserve the non-owning, synchronous, one-attempt transport contract.
- Return `Status` from new fallible public APIs; do not add exceptions, hidden
  allocation, logging, waits, retries, or background tasks to core.
- Keep `tick()` and cooperative polling bounded and allocation-free.
- Follow `.clang-format`, use `constexpr` instead of constant macros, and prefer
  explicit state and ownership over speculative abstractions.
- Do not edit `include/ssd1315/Version.h` manually. `library.json` is the
  version source of truth; regenerate with `scripts/generate_version.py`.

### Documentation

- Add concise Doxygen to every public symbol, including units, ranges, return
  codes, I2C/timing effects, ownership, and threading restrictions.
- Update README behavior/API summaries and the `[Unreleased]` changelog when a
  public contract changes.
- Keep hardware claims in the maintained validation matrix. Never infer visual,
  electrical, reset, fault, or soak success from host tests or serial ACK alone.
- Do not commit generated `docs/doxygen/`, `.pio/`, `hil_logs/`, or package
  archives.

### Validation

Run the smallest applicable set and record anything unavailable:

```sh
pio test -e native
pio run -e esp32s3dev
pio run -e esp32s2dev
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python scripts/generate_version.py check
doxygen Doxyfile
```

Changes to packaging should also run `python -m platformio pkg pack` followed
by `python tools/check_package_contents.py`; remove the generated archive after
validation. Native ESP-IDF and physical HIL results must be reported separately
and honestly when those environments are available.

### Commits

- Use [Conventional Commits](https://www.conventionalcommits.org/) format:
  - `feat:` new feature
  - `fix:` bug fix
  - `docs:` documentation only
  - `refactor:` code change that neither fixes a bug nor adds a feature
  - `test:` adding or updating tests
  - `chore:` maintenance tasks

### Pull Requests

- Keep the change focused and preserve unrelated work.
- Explain public API/behavior changes and compatibility impact.
- List exact tests/builds run and explicit gaps.
- Include representative hardware evidence only when it was actually captured.
- Ensure CI passes before merge.

### What We Accept

- Bug fixes
- Documentation improvements
- Performance improvements (with benchmarks)
- New examples (if they demonstrate a common use case)

### What We Probably Won't Accept

- Breaking API changes without discussion
- Heavy dependencies
- Platform-specific code in the library core
- Features that add heap allocations in steady state

## Questions?

Open a GitHub Discussion or Issue for questions.
