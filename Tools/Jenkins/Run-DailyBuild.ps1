[CmdletBinding()]
param(
    [string] $SettingsPath = 'C:\FestaFiestaCI\settings.json',
    [string] $BuildNumber = $env:BUILD_NUMBER,
    [switch] $Plan,
    [switch] $BuildOnly
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$settings = Get-Content -LiteralPath $SettingsPath -Raw | ConvertFrom-Json
$arguments = @{}
foreach ($name in @('SourceRepository', 'SourceBranch', 'CheckoutRoot', 'OutputRoot', 'StateRoot', 'EngineRoot', 'RcloneExe', 'RcloneConfig', 'DriveFolderId')) {
    $arguments[$name] = [string]$settings.$name
}
foreach ($name in @('UploadProvider', 'DuckExe', 'DuckConfig')) {
    if ($settings.PSObject.Properties.Name -contains $name) { $arguments[$name] = [string]$settings.$name }
}
$arguments.BuildNumber = if ($BuildNumber) { $BuildNumber } else { 'manual-' + [DateTimeOffset]::UtcNow.ToString('yyyyMMddHHmmss') }
if ($Plan) { $arguments.Plan = $true }
if ($BuildOnly) { $arguments.BuildOnly = $true }
& (Join-Path $PSScriptRoot 'Invoke-FestaFiestaDailyBuild.ps1') @arguments
