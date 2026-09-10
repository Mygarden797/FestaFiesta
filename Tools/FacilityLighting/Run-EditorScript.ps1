param(
    [Parameter(Mandatory=$true)][string]$Script,
    [string]$Label = 'lighting'
)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$scriptPath = (Resolve-Path -LiteralPath $Script).Path
if (-not $scriptPath.StartsWith($projectRoot + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Script must be in this project' }
$qaRoot = Join-Path $projectRoot 'Saved\FacilityLighting_20260910'
New-Item -ItemType Directory -Path $qaRoot -Force | Out-Null
[Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', (Join-Path $qaRoot '_cache'), 'Process')
$log = Join-Path $qaRoot ($Label + '.log')
$argsForEditor = @(
    ('"' + (Join-Path $projectRoot 'FestaFiesta.uproject') + '"'), '/Engine/Maps/Entry',
    ('-ExecutePythonScript="' + $scriptPath + '"'),
    '-DisablePlugins=RiderLink', '-unattended', '-nosplash', '-nosound', '-NoSourceControl', '-RenderOffscreen',
    '-ddc=InstalledNoZenLocalFallback', '-noxgeshadercompile',
    ('-ShaderWorkingDir="' + (Join-Path $qaRoot '_shaderwork') + '"'),
    ('-abslog="' + $log + '"')
)
# GPU RHI is intentional: this project exercises movable spot-light components.
$process = Start-Process -FilePath 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList $argsForEditor -WindowStyle Hidden -PassThru
Write-Output ('UNREAL_STARTED pid=' + $process.Id + ' log=' + $log)
$process.WaitForExit()
Write-Output ('UNREAL_FINISHED exit=' + $process.ExitCode)
if ($process.ExitCode -ne 0) { throw ('Unreal exited with ' + $process.ExitCode + '; log=' + $log) }
$scriptErrors = @(Select-String -LiteralPath $log -Pattern 'LogPython: Error:|LogEditorPythonExecuter: Error:|FF_FACILITY_.*_FAIL:')
if ($scriptErrors.Count -gt 0) { throw ('Editor script reported errors; log=' + $log) }
