# Windows entry point; preserve arguments containing spaces.
# Example: .\build.ps1 --jobs 8 --debug trace "Dark Forest"
param([Parameter(ValueFromRemainingArguments=$true)][string[]]$CliArgs)
$ErrorActionPreference = 'Stop'
Push-Location -LiteralPath $PSScriptRoot
try {
    & python (Join-Path $PSScriptRoot 'scripts/build.py') @CliArgs
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} finally {
    Pop-Location
}
