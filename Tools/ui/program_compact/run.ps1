param()
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
    throw '에디터가 실행 중입니다. 저장 후 종료해야 프로그램 에셋에 적용할 수 있습니다.'
}
$output = Join-Path $root ('Saved/ProgramCompact/Run-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $output -Force | Out-Null
$log = Join-Path $output 'apply.log'
$editorArgs = @(
    ('"' + (Join-Path $root 'FestaFiesta.uproject') + '"'), '/Engine/Maps/Entry',
    ('-ExecutePythonScript="' + (Join-Path $PSScriptRoot 'apply.py') + '"'),
    ('-UserDir="' + (Join-Path $output 'User') + '"'),
    '-DisablePlugins=RiderLink', '-unattended', '-nosplash', '-nosound',
    '-NoSourceControl', '-NullRHI', '-ddc=FestaFiestaBuildDDC',
    ('-abslog="' + $log + '"'))
$started = Get-Date
$process = Start-Process -FilePath 'C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $editorArgs -WindowStyle Hidden -PassThru
Write-Output ('PROGRAM_COMPACT_STARTED pid=' + $process.Id + ' log=' + $log)
$process.WaitForExit()
$reportPath = Join-Path $root 'Saved/ProgramCompact/apply-report.json'
if ($process.ExitCode -ne 0) { throw ('에디터 실패: ' + $log) }
if (!(Test-Path -LiteralPath $reportPath) -or (Get-Item -LiteralPath $reportPath).LastWriteTime -lt $started) {
    throw ('새 적용 결과가 없습니다: ' + $log)
}
$report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
if ($report.status -ne 'passed') { throw ('프로그램 간소화 적용 실패: ' + $reportPath) }
Write-Output ('PROGRAM_COMPACT_PASSED report=' + $reportPath)
