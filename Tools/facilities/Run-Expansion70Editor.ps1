# Run-Expansion70Editor.ps1
# 역할: 시설 모델·Data Asset·썸네일 작업에서 공통 실행 환경에서 해당 제작·검증 작업을 실행한다.

param(
    [string]$Script = 'Tools/facilities/import_expansion70.py',
    [string]$Label = 'import',
    [string]$Numbers = '',
    [switch]$ReplaceOwned,
    [string]$EngineRoot
)
$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$taskScript = (Resolve-Path -LiteralPath (Join-Path $taskRoot $Script)).Path
$taskOutput = Join-Path $taskRoot 'Saved/FacilityExpansion70'
New-Item -ItemType Directory -Path $taskOutput -Force | Out-Null
$env:FF70_NUMBERS = $Numbers
$env:FF70_REPLACE_OWNED = if ($ReplaceOwned) { '1' } else { '0' }
[Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', (Join-Path $taskOutput '_cache'), 'Process')
$taskLog = Join-Path $taskOutput ($Label + '.log')
$taskArgs = @(
    ('"' + (Join-Path $taskRoot 'FestaFiesta.uproject') + '"'),
    '-run=pythonscript', ('-script="' + $taskScript + '"'),
    '-unattended', '-nullrhi', '-nosound', '-nosplash', '-nop4',
    '-DisablePlugins=RiderLink', '-ddc=InstalledNoZenLocalFallback',
    ('-abslog="' + $taskLog + '"')
)
Import-Module (Join-Path $PSScriptRoot '../Build/FestaFiesta.Build.psm1') -Force
$engineInstall = Resolve-FFEngineRoot -EngineRoot $EngineRoot
$taskProcess = Start-Process -FilePath (Join-Path $engineInstall 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') -ArgumentList $taskArgs -WindowStyle Hidden -PassThru
Write-Output ('FF70_STARTED pid=' + $taskProcess.Id + ' log=' + $taskLog)
$taskProcess.WaitForExit()
if ($taskProcess.ExitCode -ne 0) { throw ('Unreal failed: ' + $taskProcess.ExitCode + '; ' + $taskLog) }
$taskErrors = @(Select-String -LiteralPath $taskLog -Pattern 'LogPython: Error:|FF70_.*FAIL')
if ($taskErrors.Count) { throw ('Script errors: ' + $taskLog) }
Write-Output ('FF70_FINISHED ' + $taskLog)
