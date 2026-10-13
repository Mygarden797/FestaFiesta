param()
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
    throw '에디터가 실행 중입니다. 저장 후 종료해야 점검 에셋에 적용할 수 있습니다.'
}
$output = Join-Path $projectRoot ('Saved/CheckBalanced/Run-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $output -Force | Out-Null
$editorArgs = @(
    ('"' + (Join-Path $projectRoot 'FestaFiesta.uproject') + '"'), '/Engine/Maps/Entry',
    ('-ExecutePythonScript="' + (Join-Path $PSScriptRoot 'apply.py') + '"'),
    ('-UserDir="' + (Join-Path $output 'User') + '"'),
    '-DisablePlugins=RiderLink', '-unattended', '-nosplash', '-nosound', '-NoSourceControl',
    '-NullRHI', '-ddc=FestaFiestaBuildDDC', ('-abslog="' + (Join-Path $output 'apply.log') + '"'))
$started = Get-Date
$process = Start-Process -FilePath 'C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $editorArgs -WindowStyle Hidden -PassThru
Write-Output ('CHECK_BALANCED_STARTED pid=' + $process.Id + ' output=' + $output)
$process.WaitForExit()
$reportPath = Join-Path $projectRoot 'Saved/CheckBalanced/apply-report.json'
if ($process.ExitCode -ne 0) { throw ('에디터 실패: ' + $output) }
if (!(Test-Path -LiteralPath $reportPath) -or (Get-Item -LiteralPath $reportPath).LastWriteTime -lt $started) {
    throw ('새 적용 결과 없음: ' + $output)
}
$report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
if ($report.status -ne 'passed') { throw ('적용 실패: ' + $reportPath) }
Write-Output ('CHECK_BALANCED_PASSED report=' + $reportPath)
