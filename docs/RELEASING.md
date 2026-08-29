# Releasing SSD1315

`library.json` is the version source of truth. Never move or reuse an existing
tag, and never tag a commit until that exact commit has a successful `CI` run.
On Windows, use the repository PlatformIO wrapper shown below.

## Prepare And Validate

Start from a clean, synchronized `main` branch. A dirty, divergent, or
conflicted tree must be resolved before release preparation continues.

```powershell
git switch main
git fetch --prune origin
git status --short --branch
git pull --ff-only origin main

$releaseVersion = (Get-Content -Raw library.json | ConvertFrom-Json).version
$releaseTag = "v$releaseVersion"
git show-ref --verify --quiet "refs/tags/$releaseTag"
if ($LASTEXITCODE -eq 0) { throw "Tag $releaseTag already exists" }
$remoteTag = git ls-remote --tags origin "refs/tags/$releaseTag"
if ($remoteTag) { throw "Remote tag $releaseTag already exists" }

python scripts/generate_version.py check
python -B tools/check_core_timing_guard.py
python -B tools/check_cli_contract.py
python -B tools/check_idf_example_contract.py
python -B tools/test_hil_runner_parser.py
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
claims.

## Commit And Verify CI

Review the diff before committing, then commit the prepared release. The message
follows Conventional Commits and names the version from `library.json`:

```powershell
git add -A
git commit -m "chore: prepare SSD1315 $releaseVersion release"
git push origin main

$releaseSha = git rev-parse HEAD
$runId = $null
for ($attempt = 0; $attempt -lt 24 -and -not $runId; $attempt++) {
  $runId = gh run list --workflow CI --branch main --commit $releaseSha `
    --limit 1 --json databaseId --jq '.[0].databaseId'
  if (-not $runId) { Start-Sleep -Seconds 5 }
}
if (-not $runId) { throw "CI run for $releaseSha did not appear within two minutes" }
gh run watch $runId --exit-status

$releaseSha = git rev-parse HEAD
$run = gh run list --workflow CI --branch main --commit $releaseSha --limit 1 `
  --json headSha,status,conclusion,url | ConvertFrom-Json | Select-Object -First 1
if ($null -eq $run -or $run.headSha -ne $releaseSha -or
    $run.status -ne "completed" -or $run.conclusion -ne "success") {
  throw "The exact release commit does not have a successful CI run"
}
$run | Format-List
```

## Tag And Publish

Only after the exact-commit check above succeeds:

```powershell
$releaseVersion = (Get-Content -Raw library.json | ConvertFrom-Json).version
$releaseTag = "v$releaseVersion"
git tag -a $releaseTag -m "Release $releaseTag"
git push origin $releaseTag

$tagRunId = $null
for ($attempt = 0; $attempt -lt 24 -and -not $tagRunId; $attempt++) {
  $tagRunId = gh run list --workflow CI --branch $releaseTag --event push `
    --limit 1 --json databaseId --jq '.[0].databaseId'
  if (-not $tagRunId) { Start-Sleep -Seconds 5 }
}
if (-not $tagRunId) { throw "Tag CI run for $releaseTag did not appear within two minutes" }
gh run watch $tagRunId --exit-status

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
default: some older tags (`v4.0.1`, `v1.x`) exist without a published GitHub
Release, so the automatic starting point can silently skip commits.

```powershell
$previousTag = git describe --tags --abbrev=0 "$releaseTag^"
gh release create $releaseTag --verify-tag --title $releaseTag `
  --notes-start-tag $previousTag --generate-notes
```
