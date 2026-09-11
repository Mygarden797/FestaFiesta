<#
.SYNOPSIS
Runs the NPC table-lifetime regression in an existing Development package.
.DESCRIPTION
Launches the inner game executable with NullRHI. Success requires a fresh JSON
report containing exactly the requested successful test, as well as exit code 0.
Game user data and all diagnostics go to a new directory outside the payload.
This does not build, alter the package, or prove rendered gameplay.
.EXAMPLE
.\Tools\Build\Test-PackagedNpcArrival.ps1 -PayloadRoot 'C:\Builds\NpcFix\Package\Windows'
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string] $PayloadRoot,

    [string] $OutputRoot,

    [ValidateRange(30, 600)]
    [int] $TimeoutSeconds = 180
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$testName = 'FestaFiesta.Crowd.Arrival.RuntimeTablesSurviveFestivalGarbageCollection'
$resolvedPayload = Resolve-Path -LiteralPath $PayloadRoot
if ($resolvedPayload.Provider.Name -ne 'FileSystem' -or
    -not (Test-Path -LiteralPath $resolvedPayload.ProviderPath -PathType Container)) {
    throw 'PayloadRoot must be an existing filesystem directory containing the packaged game.'
}
$PayloadRoot = [System.IO.Path]::GetFullPath($resolvedPayload.ProviderPath)
$manifestPath = Join-Path $PayloadRoot 'build-manifest.json'
$gameExecutable = Join-Path $PayloadRoot 'FestaFiesta\Binaries\Win64\FestaFiesta.exe'
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw "Package manifest is missing: $manifestPath"
}
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($manifest.target.configuration -cne 'Development') {
    throw "This test requires a Development package. See: $manifestPath"
}
if (-not (Test-Path -LiteralPath $gameExecutable -PathType Leaf)) {
    throw "Development game executable is missing: $gameExecutable"
}

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
    $runId = '{0}-{1}' -f [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss'), [Guid]::NewGuid().ToString('N')
    $OutputRoot = Join-Path $projectRoot "Artifacts\Validation\NpcSpawn\Packaged-$runId"
}
$OutputRoot = [System.IO.Path]::GetFullPath($OutputRoot)
$payloadPrefix = $PayloadRoot.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
if ($OutputRoot.TrimEnd('\', '/').Equals($PayloadRoot.TrimEnd('\', '/'), [StringComparison]::OrdinalIgnoreCase) -or
    $OutputRoot.StartsWith($payloadPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'OutputRoot must be outside PayloadRoot so the package stays unchanged.'
}
$reportRoot = Join-Path $OutputRoot 'Report'
$reportIndex = Join-Path $reportRoot 'index.json'
if (Test-Path -LiteralPath $reportIndex) {
    throw "Refusing to reuse a previous automation report: $reportIndex"
}
if (Test-Path -LiteralPath $OutputRoot) {
    throw "OutputRoot already exists. Choose a new directory; no existing results are deleted: $OutputRoot"
}

New-Item -ItemType Directory -Path $OutputRoot -ErrorAction Stop | Out-Null
New-Item -ItemType Directory -Path $reportRoot -ErrorAction Stop | Out-Null
$userRoot = Join-Path $OutputRoot 'UserData'
New-Item -ItemType Directory -Path $userRoot -ErrorAction Stop | Out-Null
$gameLog = Join-Path $OutputRoot 'Game.log'
$stdoutLog = Join-Path $OutputRoot 'stdout.log'
$stderrLog = Join-Path $OutputRoot 'stderr.log'
$summaryPath = Join-Path $OutputRoot 'summary.json'

# Start-Process joins ArgumentList into a Windows command line. Quote each path
# within its option; Windows filesystem paths cannot contain a literal double quote.
$gameArguments = @(
    '-unattended',
    '-nop4',
    '-nullrhi',
    '-nosound',
    '-nosplash',
    '-stdout',
    '-FullStdOutLogOutput',
    ('-abslog="{0}"' -f $gameLog),
    ('-UserDir="{0}"' -f $userRoot),
    ('-ReportExportPath="{0}"' -f $reportRoot),
    '-TestExit="Automation Test Queue Empty"',
    ('-ExecCmds="Automation RunTests {0}"' -f $testName)
)
$startedAtUtc = [DateTime]::UtcNow
$summary = [ordered]@{
    schemaVersion = 1
    status = 'running'
    scope = 'one-packaged-runtime-automation-test-nullrhi'
    testName = $testName
    payloadRoot = $PayloadRoot
    manifest = $manifestPath
    configuration = [string]$manifest.target.configuration
    executable = $gameExecutable
    outputRoot = $OutputRoot
    startedAtUtc = $startedAtUtc.ToString('o')
    completedAtUtc = $null
    durationSeconds = $null
    timeoutSeconds = $TimeoutSeconds
    processId = $null
    exitCode = $null
    timedOut = $false
    report = $reportIndex
    counts = $null
    exportedTestPath = $null
    exportedTestState = $null
    gameLog = $gameLog
    stdout = $stdoutLog
    stderr = $stderrLog
    error = $null
}

function Write-RunSummary {
    $json = $summary | ConvertTo-Json -Depth 6
    [System.IO.File]::WriteAllText($summaryPath, $json + [Environment]::NewLine,
        (New-Object System.Text.UTF8Encoding($false)))
}

$gameProcess = $null
$failureMessage = $null
Write-RunSummary
try {
    # Reject a report created between directory allocation and process dispatch.
    if (Test-Path -LiteralPath $reportIndex) {
        throw 'A report appeared before this run started; refusing to reuse it.'
    }
    $gameProcess = Start-Process -FilePath $gameExecutable -ArgumentList $gameArguments `
        -WorkingDirectory $OutputRoot -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput $stdoutLog -RedirectStandardError $stderrLog
    $summary.processId = $gameProcess.Id
    if (-not $gameProcess.WaitForExit($TimeoutSeconds * 1000)) {
        $summary.timedOut = $true
        # This object owns the process started above. Never kill by executable name
        # or enumerate other games/editors that may be running on the machine.
        if (-not $gameProcess.HasExited) {
            $gameProcess.Kill()
            $gameProcess.WaitForExit(5000) | Out-Null
        }
        throw "Packaged NPC test exceeded $TimeoutSeconds seconds."
    }
    $gameProcess.WaitForExit()
    $summary.exitCode = $gameProcess.ExitCode
    if (-not (Test-Path -LiteralPath $reportIndex -PathType Leaf)) {
        throw 'The game did not export a current Report/index.json; exit code alone is insufficient.'
    }
    $report = Get-Content -LiteralPath $reportIndex -Raw | ConvertFrom-Json
    $counts = [ordered]@{}
    foreach ($field in @('succeeded', 'succeededWithWarnings', 'failed', 'notRun', 'inProcess')) {
        $property = $report.PSObject.Properties[$field]
        if ($null -eq $property -or $null -eq $property.Value -or
            [string]$property.Value -notmatch '^\d+$') {
            throw "Automation report has a missing or invalid '$field' count."
        }
        $counts[$field] = [int]$property.Value
    }
    $summary.counts = $counts
    $tests = @($report.tests)
    if ($tests.Count -ne 1) {
        throw "Expected exactly one exported test; received $($tests.Count)."
    }
    $summary.exportedTestPath = [string]$tests[0].fullTestPath
    $summary.exportedTestState = [string]$tests[0].state
    if ($summary.exportedTestPath -cne $testName -or $summary.exportedTestState -cne 'Success') {
        throw 'The exported test name or result does not match the requested successful NPC regression.'
    }
    if (($counts.succeeded + $counts.succeededWithWarnings) -ne 1 -or
        $counts.failed -ne 0 -or $counts.notRun -ne 0 -or $counts.inProcess -ne 0) {
        throw 'Automation report totals do not show exactly one completed successful test and zero unfinished/failed tests.'
    }
    if ($summary.exitCode -ne 0) {
        throw "The game exited with code $($summary.exitCode), despite exporting a report."
    }
    $summary.status = 'passed'
}
catch {
    $summary.status = 'failed'
    $summary.error = $_.Exception.Message
    $failureMessage = $summary.error
}
finally {
    if ($null -ne $gameProcess) {
        try {
            if (-not $gameProcess.HasExited) {
                $gameProcess.Kill()
                $gameProcess.WaitForExit(5000) | Out-Null
            }
            if ($gameProcess.HasExited) {
                $summary.exitCode = $gameProcess.ExitCode
            }
        }
        catch {
            $summary.status = 'failed'
            $summary.error = 'Could not finish cleanup of the process created by this run: ' + $_.Exception.Message
            $failureMessage = $summary.error
        }
        finally {
            $gameProcess.Dispose()
        }
    }
    $completedAtUtc = [DateTime]::UtcNow
    $summary.completedAtUtc = $completedAtUtc.ToString('o')
    $summary.durationSeconds = [Math]::Round(($completedAtUtc - $startedAtUtc).TotalSeconds, 3)
    Write-RunSummary
}

if ($null -ne $failureMessage) {
    throw "Packaged NPC test failed. Summary: $summaryPath"
}
Write-Output $summaryPath
