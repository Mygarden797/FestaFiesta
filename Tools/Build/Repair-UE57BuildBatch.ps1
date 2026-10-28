# Repair the local UE 5.7 entry point used by Rider, including after project regeneration.
[CmdletBinding()]
param(
    [string] $EngineRoot,
    [switch] $Apply
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'FestaFiesta.Build.psm1') -Force
$resolvedEngine = Resolve-FFEngineRoot -EngineRoot $EngineRoot
$version = Get-FFEngineVersion -EngineRoot $resolvedEngine
if ($version -notlike '5.7.*') { throw "This repair supports UE 5.7 only; found $version." }
$batch = Join-Path $resolvedEngine 'Engine\Build\BatchFiles\Build.bat'
$original = [System.IO.File]::ReadAllText($batch)
$marker = 'rem FestaFiesta: preserve literal ! in project paths.'
if ($original.Contains($marker)) {
    Write-Host "Build.bat already preserves exclamation marks: $batch"
    return
}

# Percent expansion is safe on separate lines. The lock retry block originally
# needed delayed expansion because ExitCode was set inside the same parsed block.
# Move its exit-code check outside that block before disabling delayed expansion.
$lockPattern = '(?ms)^9>&2 2>nul \(call :LockAndRestoreStdErr %\* 9>"%LockFile%"\) \|\| \(\r?\n.*?^\)\r?\nexit /B !ExitCode!'
if ([regex]::Matches($original, $lockPattern).Count -ne 1 -or
    [regex]::Matches($original, '(?m)^setlocal enabledelayedexpansion\r?$').Count -ne 1) {
    throw 'Unexpected Build.bat format. No engine files were changed.'
}
$lockReplacement = @'
9>&2 2>nul (call :LockAndRestoreStdErr %* 9>"%LockFile%")
if defined ExitCode exit /B %ExitCode%
if "%WaitingForLock%" == "0" (
    set WaitingForLock=1
    echo %~nx0 is already running, waiting for existing script to terminate...
)
ping 127.0.0.1 -n 2 >nul
goto :Lock
'@
$candidate = [regex]::Replace($original, $lockPattern, $lockReplacement)
$candidate = $candidate.Replace('setlocal enabledelayedexpansion', "$marker`r`nsetlocal DisableDelayedExpansion")
$candidate = $candidate.Replace('!ExitCode!', '%ExitCode%').Replace('!ERRORLEVEL!', '%ERRORLEVEL%')
$candidate = [regex]::Replace($candidate, '\r?\n', "`r`n")
if ($candidate.Contains('!ExitCode!') -or $candidate.Contains('!ERRORLEVEL!')) {
    throw 'Unconverted delayed variables remain. No engine files were changed.'
}

$repairRoot = Join-Path (Get-FFProjectRoot) 'Saved\BuildDiagnostics\UE57BuildBatchFix'
[System.IO.Directory]::CreateDirectory($repairRoot) | Out-Null
$backup = Join-Path $repairRoot 'Build.bat.original'
$prepared = Join-Path $repairRoot 'Build.bat'
$utf8 = New-Object System.Text.UTF8Encoding($false)
if (Test-Path -LiteralPath $backup) {
    if ([System.IO.File]::ReadAllText($backup) -cne $original) {
        throw "A different original is already backed up at $backup. No engine files were changed."
    }
}
else {
    [System.IO.File]::WriteAllText($backup, $original, $utf8)
}
[System.IO.File]::WriteAllText($prepared, $candidate, $utf8)
Write-Host "Original backup: $backup"
Write-Host "Prepared repair: $prepared"
if ($Apply) {
    Copy-Item -LiteralPath $prepared -Destination $batch -Force
    if ((Get-FileHash -LiteralPath $prepared).Hash -cne (Get-FileHash -LiteralPath $batch).Hash) {
        throw 'Engine Build.bat verification failed.'
    }
    Write-Host "Repaired UE $version build entry point: $batch"
}
else {
    Write-Host 'Review the prepared repair; use -Apply to update the engine entry point.'
}
