# SSD1315 software follow-up prompts

Prepared 2026-10-25 from a fresh source review at
`a7e9f1fb5c43192c9614fc30f3d7a5a873183c21` on `main`.
These are maintainer implementation instructions; execution evidence is recorded
below and does not establish new hardware support claims. Read [the retained-branch audit](../RETAINED_BRANCH_AUDIT.md)
and [AGENTS.md](https://github.com/janhavelka/SSD1315/blob/main/AGENTS.md) before executing.

| Order | Prompt | Audit finding | Scope | Status |
| --- | --- | --- | --- | --- |
| 01 | [Make IDF timeout ownership explicit](01-idf-timeout-contract.md) | P2-T; historical M2 | Small IDF example API cleanup; preserve current per-callback budgets | RESOLVED in [d99b1ae](https://github.com/janhavelka/SSD1315/commit/d99b1ae273156f9d88c5a88cffe51dd061894c2c); local checks and 4/4 native IDF builds PASS; [evidence and limits](../RETAINED_BRANCH_AUDIT.md#p2-t-make-the-init-time-timeout-arguments-meaning-explicit) |

Implemented on 2026-10-29 using the four explicit arguments
`initIdfI2c(sda, scl, freq, address)`. The sole native caller retains its intended
address; the unused initialization timeout and old native name are removed.
`Config::i2cTimeoutMs` still supplies the callback budget. Callback behavior,
Arduino initialization and core APIs are unchanged. All seven requested local
commands passed, including **157/157** host tests. Native SDKs were unavailable
locally; [CI run 40897869218](https://github.com/janhavelka/SSD1315/actions/runs/40897869218)
passed all four S2/S3 builds on IDF v5.3.5/v5.5.5 for the implementation commit.
Its Linux Doxygen failure was fixed separately in
[2a37436](https://github.com/janhavelka/SSD1315/commit/2a37436b12f159121c3a28c038da03d98c6dba90);
the corresponding validation job in
[CI run 40898300547](https://github.com/janhavelka/SSD1315/actions/runs/40898300547)
passed. The audit records exact commands, results and coverage limits. Q1
remains open.

For future follow-up, use the existing development branch. Re-audit first because the
baseline above is a locator, not permission to overwrite newer work. Complete
the implementation, relevant verification, documentation, commit and upstream
synchronization in that task. Prefer deleting the misleading argument and
migrating the example to retaining a compatibility wrapper with an ignored
parameter. Reuse existing transport/error/lifecycle owners; do not introduce
another bus abstraction or alter the core display operation model.

The hardware qualification finding Q1 is intentionally not an implementation
prompt. Keep that evidence queue open. A successful code change, host test or
native-IDF build does not qualify panel identity, electrical behavior or visuals.

After further implementation, update this row and the audit with the resolution commit,
actual checks, and any remaining limitations. Keep the useful contract in the
public/example documentation. Do not mark this item complete merely because
the prompt was written, a checker passed, or an old CI run was green.
