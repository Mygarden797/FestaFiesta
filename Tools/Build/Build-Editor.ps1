# Build UBT directly: UE 5.7 Build.bat enables delayed expansion and removes
# exclamation marks from project paths, even when those paths are quoted.
[CmdletBinding()]
param(
    [string] $EngineRoot,
    [ValidateSet('Build', 'Rebuild', 'Clean')]
    [string] $Action = 'Build'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'FestaFiesta.Build.psm1') -Force
$projectFile = Get-FFProjectFile
$projectRoot = Get-FFProjectRoot
$resolvedEngine = Resolve-FFEngineRoot -EngineRoot $EngineRoot
$ubt = Join-Path $resolvedEngine 'Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll'
$dotnetSetup = Get-Content -LiteralPath (Join-Path $resolvedEngine 'Engine\Build\BatchFiles\GetDotnetPath.bat') -Raw
$versionMatch = [regex]::Match($dotnetSetup, '(?m)^set UE_DOTNET_VERSION=(\d+\.\d+\.\d+)\s*$')
if (-not $versionMatch.Success) {
    throw 'Cannot determine the engine-bundled .NET version from GetDotnetPath.bat.'
}
$architecture = if ($env:PROCESSOR_ARCHITECTURE -eq 'ARM64' -or ${env:ProgramFiles(Arm)}) { 'win-arm64' } else { 'win-x64' }
$dotnet = Join-Path $resolvedEngine "Engine\Binaries\ThirdParty\DotNet\$($versionMatch.Groups[1].Value)\$architecture\dotnet.exe"
foreach ($required in @($dotnet, $ubt)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required Unreal build tool was not found: $required"
    }
}

$diagnostics = Join-Path $projectRoot 'Saved\BuildDiagnostics'
[System.IO.Directory]::CreateDirectory($diagnostics) | Out-Null

$arguments = @($ubt, 'FestaFiestaEditor', 'Win64', 'Development', "-Project=$projectFile", '-WaitMutex', '-NoHotReloadFromIDE', "-Log=$(Join-Path $diagnostics 'EditorBuild.log')")
if ($Action -eq 'Rebuild') { $arguments += '-Rebuild' }
if ($Action -eq 'Clean') { $arguments += '-Clean' }
# UBT sets its own Engine/Source working directory. No cmd.exe/batch forwarding.
& $dotnet @arguments
if ($LASTEXITCODE -ne 0) {
    throw "Editor $Action failed with exit code $LASTEXITCODE. See Saved/BuildDiagnostics/EditorBuild.log."
}
