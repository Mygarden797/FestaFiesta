param(
    [string]$Script = 'Tools/facilities/import_expansion70.py',
    [string]$Label = 'import',
    [string]$Numbers = '',
    [switch]$ReplaceOwned
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
$engineInstall = ((Get-Content -LiteralPath 'C:/ProgramData/Epic/UnrealEngineLauncher/LauncherInstalled.dat' | ConvertFrom-Json).InstallationList | Where-Object AppName -EQ 'UE_5.7' | Select-Object -First 1).InstallLocation
$taskProcess = Start-Process -FilePath (Join-Path $engineInstall 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') -ArgumentList $taskArgs -WindowStyle Hidden -PassThru
Write-Output ('FF70_STARTED pid=' + $taskProcess.Id + ' log=' + $taskLog)
$taskProcess.WaitForExit()
if ($taskProcess.ExitCode -ne 0) { throw ('Unreal failed: ' + $taskProcess.ExitCode + '; ' + $taskLog) }
$taskErrors = @(Select-String -LiteralPath $taskLog -Pattern 'LogPython: Error:|FF70_.*FAIL')
if ($taskErrors.Count) { throw ('Script errors: ' + $taskLog) }
Write-Output ('FF70_FINISHED ' + $taskLog)
