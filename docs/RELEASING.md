# Releasing SSD1315

`library.json` is the version source of truth. Never move or reuse an existing
tag, and never tag a commit until that exact commit has a successful `CI` run.
On Windows, use the repository PlatformIO wrapper shown below. Install Doxygen
and Graphviz for the strict API documentation build. Use an authenticated GitHub
CLI (`gh`) to inspect CI. Run these commands from the repository root and stop
on any failed check; Windows PowerShell does not automatically stop when an
external command returns a nonzero exit code.

## Prepare And Validate

Start from a clean, synchronized `main` branch. A dirty, divergent, or
conflicted tree must be resolved before release preparation continues. Preserve
unrelated work; do not discard or stage it as part of release preparation.

```powershell
git switch main
git fetch --prune origin
git status --short --branch
git pull --ff-only origin main
```

Choose the next version using Semantic Versioning. Use the generator's
`bump patch`, `bump minor`, `bump major`, or `set X.Y.Z` command to update
`library.json` and synchronize `include/ssd1315/Version.h`, `idf_component.yml`
and the Doxyfile version. Do not edit the generated header by hand. Move the
completed `[Unreleased]` entries into the matching dated changelog section and
leave an empty `[Unreleased]` section for subsequent work. If the intended
version is already prepared, check it without bumping it again.

Then check tag availability and validate the prepared tree:

```powershell
$releaseVersion = (Get-Content -Raw library.json | ConvertFrom-Json).version
$releaseTag = "v$releaseVersion"
git show-ref --verify --quiet "refs/tags/$releaseTag"
if ($LASTEXITCODE -eq 0) { throw "Tag $releaseTag already exists" }
$remoteTag = git ls-remote --tags origin "refs/tags/$releaseTag"
if ($LASTEXITCODE -ne 0) { throw "Remote tag lookup failed" }
if ($remoteTag) { throw "Remote tag $releaseTag already exists" }

python scripts/generate_version.py check
python -B tools/check_core_timing_guard.py
python -B tools/check_cli_contract.py
python -B tools/check_idf_example_contract.py
python -B tools/test_hil_runner_parser.py
python -B tools/test_package_contents.py
python -B tools/run_ssd1315_hil.py --dry-run
foreach ($mode in 'smoke','functional','retention','benchmark','arduino-extended','soak','all') {
  python -B tools/run_ssd1315_hil.py --dry-run --mode $mode --soak-ops 10
}
.\scripts\pio.cmd test -e native
.\scripts\pio.cmd run -e esp32s3dev
.\scripts\pio.cmd run -e esp32s2dev
.\scripts\pio.cmd run -e compat_pioarduino_54_s3
doxygen Doxyfile
.\scripts\pio.cmd pkg pack
python -B tools/check_package_contents.py
tar -tf "SSD1315-$releaseVersion.tar.gz"
git diff --check
git status --short --branch
```

Physical HIL is a separate qualification activity. Do not convert host, CI, or
serial-only results into visual, electrical, reset, fault, or field-readiness
claims. Record evidence in the [hardware ledger](SSD1315_HARDWARE_VALIDATION.md).
The native IDF matrix runs in CI for S2/S3 on v5.3.5 and v5.5.5; local Arduino
builds do not substitute for it. Local native builds use the
[IDF guide](IDF_PORT.md).

## Commit And Verify CI

Review the diff before committing, then stage only the prepared release files
or hunks. The example below lists the version/changelog files; add other owned
release changes explicitly if needed. The message follows Conventional Commits
and names the version from `library.json`:

```powershell
git add -- library.json include/ssd1315/Version.h idf_component.yml Doxyfile CHANGELOG.md
if ($LASTEXITCODE -ne 0) { throw "Release staging failed" }
git diff --cached --check
if ($LASTEXITCODE -ne 0) { throw "Staged release whitespace check failed" }
git diff --cached
git commit -m "chore: prepare SSD1315 $releaseVersion release"
if ($LASTEXITCODE -ne 0) { throw "Release commit failed" }
git push origin main
if ($LASTEXITCODE -ne 0) { throw "Release push failed" }

$releaseSha = git rev-parse HEAD
$runId = $null
for ($attempt = 0; $attempt -lt 24 -and -not $runId; $attempt++) {
  $runId = gh run list --workflow CI --branch main --commit $releaseSha `
    --limit 1 --json databaseId --jq '.[0].databaseId'
  if (-not $runId) { Start-Sleep -Seconds 5 }
}
if (-not $runId) { throw "CI run for $releaseSha did not appear within two minutes" }
gh run watch $runId --exit-status
if ($LASTEXITCODE -ne 0) { throw "Release CI did not pass" }

$releaseSha = git rev-parse HEAD
$run = gh run list --workflow CI --branch main --commit $releaseSha --limit 1 `
  --json headSha,status,conclusion,url | ConvertFrom-Json | Select-Object -First 1
if ($null -eq $run -or $run.headSha -ne $releaseSha -or
    $run.status -ne "completed" -or $run.conclusion -ne "success") {
  throw "The exact release commit does not have a successful CI run"
}
$run | Format-List
```

If CI fails, fix the confirmed failure and repeat validation for the new commit.
Do not weaken checks or reuse an older passing run. A missing automatic run can
be dispatched through the existing `CI` workflow, then verified by its exact
`headSha` as above. The workflow does not publish a release automatically.

## Tag And Publish

Only after the exact-commit check above succeeds:

```powershell
$releaseVersion = (Get-Content -Raw library.json | ConvertFrom-Json).version
$releaseTag = "v$releaseVersion"
if ((git rev-parse HEAD) -ne $releaseSha) { throw "HEAD changed after CI verification" }
if (git status --porcelain) { throw "Worktree changed after CI verification" }
git tag -a $releaseTag $releaseSha -m "Release $releaseTag"
if ($LASTEXITCODE -ne 0) { throw "Tag creation failed" }
git push origin $releaseTag
if ($LASTEXITCODE -ne 0) { throw "Tag push failed" }

$tagRunId = $null
for ($attempt = 0; $attempt -lt 24 -and -not $tagRunId; $attempt++) {
  $tagRunId = gh run list --workflow CI --branch $releaseTag --event push `
    --limit 1 --json databaseId --jq '.[0].databaseId'
  if (-not $tagRunId) { Start-Sleep -Seconds 5 }
}
if (-not $tagRunId) { throw "Tag CI run for $releaseTag did not appear within two minutes" }
gh run watch $tagRunId --exit-status
if ($LASTEXITCODE -ne 0) { throw "Tag CI did not pass" }

Start-Process "https://github.com/janhavelka/SSD1315/releases/new?tag=$releaseTag"
```

On GitHub, select the existing tag, use the tag as the release title, paste the
matching changelog section, mark the release as Latest, and publish it. The
validated `SSD1315-<version>.tar.gz` package may be attached if a PlatformIO
package artifact is desired; otherwise GitHub's source archives are sufficient.
Delete local generated archives after publication when they are no longer
needed.

If GitHub-generated commit notes are preferred to the curated changelog text,
pass `--notes-start-tag` explicitly with the previous tag. Do not rely on the
default: a tag can exist without a published GitHub Release, so an automatic
starting point can skip the intended interval. Use the CLI publication command
below instead of the browser publication step when choosing generated notes.

```powershell
$previousTag = git describe --tags --abbrev=0 "$releaseTag^"
gh release create $releaseTag --verify-tag --title $releaseTag `
  --notes-start-tag $previousTag --generate-notes
```
