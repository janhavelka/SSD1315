# SSD1315 software follow-up prompts

Prepared 2026-10-25 from a fresh source review at
`a7e9f1fb5c43192c9614fc30f3d7a5a873183c21` on `main`.
These are maintainer implementation instructions, not completed fixes or new
support claims. Read [the retained-branch audit](../RETAINED_BRANCH_AUDIT.md)
and [AGENTS.md](https://github.com/janhavelka/SSD1315/blob/main/AGENTS.md) before executing.

| Order | Prompt | Audit finding | Scope | Status |
| --- | --- | --- | --- | --- |
| 01 | [Make IDF timeout ownership explicit](01-idf-timeout-contract.md) | P2-T; historical M2 | Small IDF example API cleanup; preserve current per-callback budgets | NOT IMPLEMENTED |

Execute 01 on the existing development branch. Re-audit first because the
baseline above is a locator, not permission to overwrite newer work. Complete
the implementation, relevant verification, documentation, commit and upstream
synchronization in that task. Prefer deleting the misleading argument and
migrating the example to retaining a compatibility wrapper with an ignored
parameter. Reuse existing transport/error/lifecycle owners; do not introduce
another bus abstraction or alter the core display operation model.

The hardware qualification finding Q1 is intentionally not an implementation
prompt. Keep that evidence queue open. A successful code change, host test or
native-IDF build does not qualify panel identity, electrical behavior or visuals.

After implementation, update this row and the audit with the resolution commit,
actual checks, and any remaining limitations. Keep the useful contract in the
public/example documentation. Do not mark this item complete merely because
the prompt was written, a checker passed, or an old CI run was green.
