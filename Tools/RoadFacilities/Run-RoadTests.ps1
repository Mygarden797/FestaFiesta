[CmdletBinding()]
param(
    [ValidatePattern('^[A-Za-z0-9_. +\-]+$')][string] $Filter = 'FF_ZoneSystem.Road+FestaFiesta.Building.Catalog.ProductionRoadAssetsAndEconomy+FestaFiesta.Building.FacilityNaming+FestaFiesta.UI.PrepBuild.CategoryContract+FestaFiesta.NPC.ZoneSystemBridge.ExcludesInfoAndSafety+FestaFiesta.Crowd.Road',
    [ValidatePattern('^[A-Za-z0-9._-]+$')][string] $Label = 'road_automation',
    [ValidateRange(1, 100000)][int] $MinimumPassCount = 1,
    [string] $EngineRoot,
    [ValidateRange(30, 14400)][int] $TimeoutSeconds = 1800
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'RoadFacilities.Runner.psm1') -Force
if ([string]::IsNullOrWhiteSpace($Filter)) { throw 'An automation test filter is required.' }
$run = Invoke-RoadEditorProcess -EngineRoot $EngineRoot -Label $Label -TimeoutSeconds $TimeoutSeconds `
    -ExtraArguments @(
        '-TestExit="Automation Test Queue Empty"',
        ('-ExecCmds="Automation RunTests ' + $Filter + '"')
    )
$failures = @(Select-String -LiteralPath $run.LogPath -Pattern @(
    'Result=\{Fail\}', 'Automation Test Failed', 'No automation tests matched',
    'No tests found', 'Fatal error:', 'Assertion failed:', 'Unhandled Exception:',
    'Automation Test Queue Empty.*[1-9][0-9]* test\(s\) failed'
))
$passes = @(Select-String -LiteralPath $run.LogPath -Pattern 'LogAutomationController:.*Test Completed\. Result=\{Success\}')
$queueCompleted = @(Select-String -LiteralPath $run.LogPath -Pattern 'LogAutomationCommandLine:.*Automation Test Queue Empty [1-9][0-9]* tests performed\.')
$missingFilters = @($Filter.Split('+') | ForEach-Object { $_.Trim() } | Where-Object {
    $selector = $_
    $matchingPasses = @($passes | Where-Object { $_.Line.IndexOf($selector, [StringComparison]::OrdinalIgnoreCase) -ge 0 })
    $matchingPasses.Count -eq 0
})
$passed = $failures.Count -eq 0 -and $passes.Count -ge $MinimumPassCount -and $queueCompleted.Count -gt 0 -and $missingFilters.Count -eq 0
[ordered]@{
    status = $(if ($passed) { 'PASS' } else { 'FAIL' })
    filter = $Filter
    exit_code = $run.ExitCode
    log = $run.LogPath
    report = $run.ReportPath
    minimum_pass_count = $MinimumPassCount
    pass_count = $passes.Count
    queue_completed = $queueCompleted.Count -gt 0
    filters_without_passing_tests = $missingFilters
    passed_tests = @($passes | ForEach-Object { $_.Line })
    failures = @($failures | ForEach-Object { $_.Line })
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $run.ValidationPath -Encoding UTF8
Write-Output ('ROAD_TESTS_FINISHED passes=' + $passes.Count + ' failures=' + $failures.Count + ' report=' + $run.ValidationPath)
if (-not $passed) {
    throw ('Road automation failed, did not finish, or did not run the expected tests; report=' + $run.ValidationPath)
}
