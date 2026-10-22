# Windows entry point; preserve arguments containing spaces.
# Default is an incremental Godot build. Launch only with --run.
# Example: .\build.ps1 --run --jobs 8 --debug trace "Dark Forest"
# Explicit clean rebuild: .\build.ps1 --clean --jobs 8
param([Parameter(ValueFromRemainingArguments=$true)][string[]]$CliArgs)
$ErrorActionPreference = 'Stop'
Push-Location -LiteralPath $PSScriptRoot
try {
    & python (Join-Path $PSScriptRoot 'scripts/build.py') @CliArgs
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} finally {
    Pop-Location
}
