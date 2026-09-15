[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string] $Script,
    [ValidatePattern('^[A-Za-z0-9._-]+$')][string] $Label = 'road_editor',
    [string] $SuccessPattern = 'ROAD_(?:IMPORT|VERIFY)_PASS',
    [string] $EngineRoot,
    [ValidateRange(30, 14400)][int] $TimeoutSeconds = 1800
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'RoadFacilities.Runner.psm1') -Force
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$scriptPath = (Resolve-Path -LiteralPath $Script).Path
if (-not $scriptPath.StartsWith($projectRoot + '\', [StringComparison]::OrdinalIgnoreCase) -or
    [IO.Path]::GetExtension($scriptPath) -ne '.py') {
    throw 'Script must be a Python file inside this project.'
}
if ([string]::IsNullOrWhiteSpace($SuccessPattern)) {
    throw 'SuccessPattern must identify an explicit success marker written by the Python script.'
}
# Validate the pattern before launching a potentially long editor session.
$null = [regex]::new($SuccessPattern)
$run = Invoke-RoadEditorProcess -EngineRoot $EngineRoot -Label $Label -TimeoutSeconds $TimeoutSeconds `
    -ExtraArguments @(('-ExecutePythonScript="' + $scriptPath + '"'))
$errors = @(Select-String -LiteralPath $run.LogPath -Pattern @(
    'LogPython: Error:', 'LogEditorPythonExecuter: Error:', 'Fatal error:',
    '(?:FF_)?ROAD.*_FAIL', 'Assertion failed:', 'Unhandled Exception:'
))
$successes = @(Select-String -LiteralPath $run.LogPath -Pattern $SuccessPattern)
$passed = $errors.Count -eq 0 -and $successes.Count -gt 0
[ordered]@{
    status = $(if ($passed) { 'PASS' } else { 'FAIL' })
    script = $scriptPath
    exit_code = $run.ExitCode
    log = $run.LogPath
    success_pattern = $SuccessPattern
    success_markers = @($successes | ForEach-Object { $_.Line })
    errors = @($errors | ForEach-Object { $_.Line })
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $run.ValidationPath -Encoding UTF8
if (-not $passed) {
    throw ('Road editor script failed or did not emit its success marker; report=' + $run.ValidationPath)
}
Write-Output ('ROAD_EDITOR_SCRIPT_PASS report=' + $run.ValidationPath)
