# Run-RoadEditor.ps1
# 역할: 도로·타일 제작 원본과 납품 작업에서 공통 실행 환경에서 해당 제작·검증 작업을 실행한다.

param(
    [ValidateSet('Import', 'Verify', 'Capture', 'Cleanup')][string]$Mode = 'Import',
    [string]$EngineRoot
)
$ErrorActionPreference = 'Stop'
$roadRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$roadQA = Join-Path $PSScriptRoot 'qa'
New-Item -ItemType Directory -Path $roadQA -Force | Out-Null
$roadScripts = @{ Import = 'import_roads.py'; Verify = 'verify_roads.py'; Capture = 'capture_roads.py'; Cleanup = 'cleanup_attempts.py' }
$roadScript = Join-Path $PSScriptRoot $roadScripts[$Mode]
if (-not (Test-Path -LiteralPath $roadScript)) { throw ('Missing script: ' + $roadScript) }
Import-Module (Join-Path $PSScriptRoot '../Build/FestaFiesta.Build.psm1') -Force
$roadEditor = Join-Path (Resolve-FFEngineRoot -EngineRoot $EngineRoot) 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$roadLog = Join-Path $roadQA ($Mode.ToLowerInvariant() + '.log')
[Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', (Join-Path $roadQA '_cache'), 'Process')
$roadArgs = @(
    ('"' + (Join-Path $roadRoot 'FestaFiesta.uproject') + '"'), '/Engine/Maps/Entry',
    '-unattended', '-nosplash', '-nosound', '-NoSourceControl', '-DisablePlugins=RiderLink',
    '-ddc=InstalledNoZenLocalFallback', '-noxgeshadercompile',
    ('-ShaderWorkingDir="' + (Join-Path $roadQA '_shaderwork') + '"'),
    ('-abslog="' + $roadLog + '"')
)
if ($Mode -eq 'Capture') {
    $roadArgs += @('-RenderOffscreen', ('-ExecutePythonScript="' + $roadScript + '"'))
} else {
    # Full editor initialization exposes StaticMeshEditorSubsystem for authored LODs.
    $roadArgs += @('-NullRHI', ('-ExecutePythonScript="' + $roadScript + '"'))
}
$roadProcess = Start-Process -FilePath $roadEditor -ArgumentList $roadArgs -WindowStyle Hidden -PassThru
Write-Output ('UNREAL_ROADS_STARTED mode=' + $Mode + ' pid=' + $roadProcess.Id + ' log=' + $roadLog)
$roadProcess.WaitForExit()
Write-Output ('UNREAL_ROADS_FINISHED exit=' + $roadProcess.ExitCode)
if ($roadProcess.ExitCode -ne 0) { throw ('Unreal exited with ' + $roadProcess.ExitCode + '; log=' + $roadLog) }
$roadErrors = @(Select-String -LiteralPath $roadLog -Pattern 'LogPython: Error:|LogEditorPythonExecuter: Error:|ROAD_.*_FAIL:')
if ($roadErrors.Count -gt 0) { throw ('Road script reported errors; log=' + $roadLog) }
