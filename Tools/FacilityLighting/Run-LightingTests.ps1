# Run-LightingTests.ps1
# 역할: 시설 조명과 조명 제어 화면 작업에서 공통 실행 환경에서 해당 제작·검증 작업을 실행한다.

param(
    [string]$Filter = 'FF_ZoneSystem.Lighting+FestaFiesta.UI.FacilityInfo.Lighting',
    [string]$Label = 'automation'
)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$qaRoot = Join-Path $projectRoot 'Saved\FacilityLighting_20260910'
New-Item -ItemType Directory -Path $qaRoot -Force | Out-Null
[Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', (Join-Path $qaRoot '_cache'), 'Process')
$log = Join-Path $qaRoot ($Label + '.log')
$arguments = @(
    ('"' + (Join-Path $projectRoot 'FestaFiesta.uproject') + '"'), '/Engine/Maps/Entry',
    '-DisablePlugins=RiderLink', '-unattended', '-nosplash', '-nosound', '-NoSourceControl', '-RenderOffscreen',
    '-ini:Engine:[SystemSettings]:r.CEFGPUAcceleration=0',
    '-ddc=InstalledNoZenLocalFallback', '-noxgeshadercompile',
    ('-ShaderWorkingDir="' + (Join-Path $qaRoot '_shaderwork') + '"'),
    ('-abslog="' + $log + '"'),
    ('-ReportExportPath="' + (Join-Path $qaRoot ($Label + '_Report')) + '"'),
    '-TestExit="Automation Test Queue Empty"', ('-ExecCmds="Automation RunTests ' + $Filter + '"')
)
$process = Start-Process -FilePath 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList $arguments -WindowStyle Hidden -PassThru
Write-Output ('TESTS_STARTED pid=' + $process.Id + ' log=' + $log)
$process.WaitForExit()
$failures = @(Select-String -LiteralPath $log -Pattern 'Result=\{Fail\}|Fatal error:|Automation Test Failed|No automation tests matched')
$passes = @(Select-String -LiteralPath $log -Pattern 'Result=\{Success\}')
Write-Output ('TESTS_FINISHED exit=' + $process.ExitCode + ' passes=' + $passes.Count + ' failures=' + $failures.Count)
if ($process.ExitCode -ne 0 -or $failures.Count -gt 0 -or $passes.Count -eq 0) { throw ('Lighting tests failed; log=' + $log) }
