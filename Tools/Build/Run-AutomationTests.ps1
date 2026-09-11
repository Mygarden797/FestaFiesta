[CmdletBinding()]
param(
    [string] $EngineRoot,

    [string[]] $TestFilter = @('FestaFiesta', 'FF_ZoneSystem'),

    [string] $OutputRoot,

    [switch] $SkipDataValidation
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Import-Module (Join-Path $PSScriptRoot 'FestaFiesta.Build.psm1') -Force

$projectRoot = Get-FFProjectRoot
$projectFile = Get-FFProjectFile
$resolvedEngineRoot = Resolve-FFEngineRoot -EngineRoot $EngineRoot
$editorCommandlet = Join-Path $resolvedEngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$buildBatch = Join-Path $resolvedEngineRoot 'Engine\Build\BatchFiles\Build.bat'

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $timestamp = [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')
    $OutputRoot = Join-Path $projectRoot "Artifacts\Validation\$timestamp"
}
$OutputRoot = [System.IO.Path]::GetFullPath($OutputRoot)
[System.IO.Directory]::CreateDirectory($OutputRoot) | Out-Null

Assert-FFAssetsMaterialized

$editorBuildLog = Join-Path $OutputRoot 'EditorBuild.log'
$editorBuildArguments = @(
    'FestaFiestaEditor',
    'Win64',
    'Development',
    "-Project=$projectFile",
    '-WaitMutex',
    '-NoHotReloadFromIDE'
)

Write-Host 'Building the Development Editor target used by validation...'
$editorBuildExitCode = -1
$savedErrorActionPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
try {
    & $buildBatch @editorBuildArguments 2>&1 | Tee-Object -FilePath $editorBuildLog
    $editorBuildExitCode = $LASTEXITCODE
}
finally {
    $ErrorActionPreference = $savedErrorActionPreference
}
if ($editorBuildExitCode -ne 0) {
    throw "Development Editor build failed with exit code $editorBuildExitCode. See: $editorBuildLog"
}

$failedFilters = New-Object System.Collections.Generic.List[string]
foreach ($filter in $TestFilter) {
    if ([string]::IsNullOrWhiteSpace($filter)) {
        continue
    }

    $safeFilter = $filter -replace '[^A-Za-z0-9._-]', '_'
    $filterRoot = Join-Path $OutputRoot $safeFilter
    $reportRoot = Join-Path $filterRoot 'Report'
    $logPath = Join-Path $filterRoot 'Automation.log'
    [System.IO.Directory]::CreateDirectory($reportRoot) | Out-Null
    $reportIndex = Join-Path $reportRoot 'index.json'
    if (Test-Path -LiteralPath $reportIndex -PathType Leaf) {
        # A prior successful export must not validate a failed or empty current run.
        Remove-Item -LiteralPath $reportIndex -Force
    }

    $arguments = @(
        $projectFile,
        '-unattended',
        '-nop4',
        '-nosplash',
        '-nullrhi',
        '-nosound',
        '-stdout',
        '-FullStdOutLogOutput',
        "-ReportExportPath=$reportRoot",
        '-TestExit=Automation Test Queue Empty',
        "-ExecCmds=Automation RunTests $filter"
    )

    Write-Host "Running Unreal automation tests: $filter"
    $exitCode = -1
    $savedErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & $editorCommandlet @arguments 2>&1 | Tee-Object -FilePath $logPath
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $savedErrorActionPreference
    }

    $failureLines = @(Select-String -LiteralPath $logPath -Pattern @(
        'Result=\{Fail\}',
        'Automation Test Failed',
        'Automation Test Queue Empty.*[1-9][0-9]* test\(s\) failed',
        'Fatal error:',
        'LogClass: Error: .*not initialized properly'
    ) -ErrorAction SilentlyContinue)

    $reportPassed = $false
    try {
        $report = Get-Content -LiteralPath $reportIndex -Raw | ConvertFrom-Json
        $completed = [int]$report.succeeded + [int]$report.succeededWithWarnings
        $reportPassed = $completed -gt 0 -and [int]$report.failed -eq 0 -and
            [int]$report.notRun -eq 0 -and [int]$report.inProcess -eq 0 -and
            @($report.tests).Count -eq $completed -and
            @($report.tests | Where-Object { $_.state -ne 'Success' }).Count -eq 0
    }
    catch {
        Write-Warning "Automation did not export a valid current report: $reportIndex"
    }

    if ($exitCode -ne 0 -or $failureLines.Count -gt 0 -or -not $reportPassed) {
        $failedFilters.Add($filter)
        Write-Error "Automation test filter '$filter' failed. See: $logPath" -ErrorAction Continue
    }
    else {
        Write-Host "Automation tests passed: $filter" -ForegroundColor Green
    }
}

if ($failedFilters.Count -gt 0) {
    throw "Automation validation failed for: $($failedFilters -join ', ')"
}

if (-not $SkipDataValidation) {
    $dataValidationRoot = Join-Path $OutputRoot 'DataValidation'
    $dataValidationLog = Join-Path $dataValidationRoot 'DataValidation.log'
    [System.IO.Directory]::CreateDirectory($dataValidationRoot) | Out-Null

    $dataValidationArguments = @(
        $projectFile,
        '-run=DataValidation',
        '-ProjectOnly',
        '-IncludeOnlyOnDiskAssets',
        '-unattended',
        '-nop4',
        '-nullrhi',
        '-nosound',
        '-stdout',
        '-FullStdOutLogOutput'
    )

    Write-Host 'Running Unreal project data validation...'
    $dataValidationExitCode = -1
    $savedErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & $editorCommandlet @dataValidationArguments 2>&1 | Tee-Object -FilePath $dataValidationLog
        $dataValidationExitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $savedErrorActionPreference
    }
    $validationFailures = @(Select-String -LiteralPath $dataValidationLog -Pattern @(
        'LogContentValidation: Error:',
        'AssetCheck:\s+Error:',
        'Data validation FAILED',
        'Failure - [1-9][0-9]* error\(s\)',
        'Fatal error:'
    ) -ErrorAction SilentlyContinue)

    $validationSuccess = @(Select-String -LiteralPath $dataValidationLog `
        -Pattern 'Success - 0 error\(s\)' -ErrorAction SilentlyContinue)

    if ($dataValidationExitCode -ne 0 -or
        $validationFailures.Count -gt 0 -or
        $validationSuccess.Count -eq 0) {
        throw "Project data validation failed. See: $dataValidationLog"
    }

    Write-Host 'Project data validation passed.' -ForegroundColor Green
}

Write-Host "Validation completed successfully. Reports: $OutputRoot" -ForegroundColor Green
