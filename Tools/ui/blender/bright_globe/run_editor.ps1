param([Parameter(Mandatory=$true)][string]$Script, [string]$Label='baseline', [switch]$NullRHI)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../..'))
$scriptPath = (Resolve-Path -LiteralPath $Script).Path
if (-not $scriptPath.StartsWith($root + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Script outside project' }
if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) { throw 'Editor already running; avoid a second writer' }
$output = Join-Path $root 'Saved/BrightGlobe'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$log = Join-Path $output ($Label + '.log')
[Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', (Join-Path $output '_cache'), 'Process')
$editorArgs = @(
    ('"' + (Join-Path $root 'FestaFiesta.uproject') + '"'), '/Engine/Maps/Entry',
    ('-ExecutePythonScript="' + $scriptPath + '"'),
    '-DisablePlugins=RiderLink', '-unattended', '-nosplash', '-nosound', '-NoSourceControl', '-RenderOffscreen',
    '-ddc=InstalledNoZenLocalFallback', '-noxgeshadercompile', '-ResX=1600', '-ResY=900',
    ('-ShaderWorkingDir="' + (Join-Path $output '_shaderwork') + '"'), ('-abslog="' + $log + '"'))
if ($NullRHI) { $editorArgs += '-NullRHI' }
$process = Start-Process -FilePath 'C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $editorArgs -WindowStyle Hidden -PassThru
Write-Output ('BRIGHT_EDITOR_STARTED pid=' + $process.Id + ' log=' + $log)
$process.WaitForExit()
Write-Output ('BRIGHT_EDITOR_FINISHED exit=' + $process.ExitCode)
$scriptErrors = @(Select-String -LiteralPath $log -Pattern 'LogPython: Error:|BRIGHT_GLOBE_FAIL')
[pscustomobject]@{
    script = $scriptPath
    pid = $process.Id
    exit_code = $process.ExitCode
    null_rhi = [bool]$NullRHI
    script_error_count = $scriptErrors.Count
    log = $log
    finished_at = (Get-Date).ToString('o')
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output ($Label + '.process.json')) -Encoding utf8
if ($process.ExitCode -ne 0) { throw ('Unreal failed; see ' + $log) }
if ($scriptErrors.Count -gt 0) { throw ('Script failed; see ' + $log) }
