param([string]$Label = 'apply')

$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$scriptPath = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot 'apply.py'))
$editorPath = 'C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$output = Join-Path $projectRoot 'Saved/SnsDensity'
$reportPath = Join-Path $output 'apply-report.json'
if ($Label -notmatch '^[A-Za-z0-9_-]+$') { throw 'Label은 영문·숫자·밑줄·하이픈만 허용한다.' }
if (-not (Test-Path -LiteralPath $editorPath -PathType Leaf)) { throw 'UE 5.7 에디터를 찾을 수 없다.' }
if (-not (Test-Path -LiteralPath $scriptPath -PathType Leaf)) { throw 'apply.py를 찾을 수 없다.' }
if (Get-Process -Name UnrealEditor, UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
    throw 'UE 에디터가 이미 실행 중이다. 다른 세션의 에셋 작업 보호를 위해 중단했다.'
}
if (Test-Path -LiteralPath $reportPath -PathType Leaf) {
    $priorReport = Get-Content -LiteralPath $reportPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($priorReport.status -eq 'passed') {
        throw '이미 적용했다. 디자이너 편집 보존을 위해 재실행하지 않는다.'
    }
}

New-Item -ItemType Directory -Path $output -Force | Out-Null
$runStamp = (Get-Date).ToUniversalTime().ToString('yyyyMMdd-HHmmss-fff')
$log = Join-Path $output ($Label + '-' + $runStamp + '.log')
$processReport = Join-Path $output ($Label + '-' + $runStamp + '.process.json')
$previousCache = [Environment]::GetEnvironmentVariable('UE-LocalDataCachePath', 'Process')
[Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', (Join-Path $output '_cache'), 'Process')
$startedAt = (Get-Date).ToUniversalTime()
$editorArgs = @(
    ('"' + (Join-Path $projectRoot 'FestaFiesta.uproject') + '"'), '/Engine/Maps/Entry',
    ('-ExecutePythonScript="' + $scriptPath + '"'),
    '-NullRHI', '-DisablePlugins=RiderLink', '-unattended', '-nosplash', '-nosound', '-NoSourceControl',
    '-ddc=InstalledNoZenLocalFallback', '-noxgeshadercompile',
    ('-ShaderWorkingDir="' + (Join-Path $output '_shaderwork') + '"'), ('-abslog="' + $log + '"')
)

try {
    $editorProcess = Start-Process -FilePath $editorPath -ArgumentList $editorArgs -WindowStyle Hidden -PassThru
    Write-Output ('SNS_DENSITY_EDITOR_STARTED pid=' + $editorProcess.Id + ' log=' + $log)
    $editorProcess.WaitForExit()
    $editorProcess.Refresh()
    $exitCode = $editorProcess.ExitCode
    Write-Output ('SNS_DENSITY_EDITOR_FINISHED exit=' + $exitCode)
    if (-not (Test-Path -LiteralPath $log -PathType Leaf)) { throw '에디터 실행 로그가 없다.' }
    $scriptErrors = @(Select-String -LiteralPath $log -Pattern 'LogPython: Error:|FF_SNS_DENSITY_APPLY_FAIL|FF_LIVE_SOCIAL_PANELS_FAIL|LogBlueprint: Error:|Fatal error:|Assertion failed:')
    $applyReport = $null
    $freshReport = $false
    if (Test-Path -LiteralPath $reportPath -PathType Leaf) {
        $applyReport = Get-Content -LiteralPath $reportPath -Raw -Encoding UTF8 | ConvertFrom-Json
        $reportFile = Get-Item -LiteralPath $reportPath
        $freshReport = $reportFile.LastWriteTimeUtc -ge $startedAt
    }
    [pscustomobject]@{
        script = $scriptPath
        pid = $editorProcess.Id
        exit_code = $exitCode
        null_rhi = $true
        script_error_count = $scriptErrors.Count
        report_status = if ($applyReport) { $applyReport.status } else { 'missing' }
        fresh_report = $freshReport
        log = $log
        started_at = $startedAt.ToString('o')
        finished_at = (Get-Date).ToUniversalTime().ToString('o')
    } | ConvertTo-Json | Set-Content -LiteralPath $processReport -Encoding UTF8
    if ($exitCode -ne 0) { throw ('Unreal 실행 실패: ' + $log) }
    if ($scriptErrors.Count -gt 0) { throw ('Python/위젯 생성 또는 컴파일 실패: ' + $log) }
    if (-not $freshReport -or -not $applyReport -or $applyReport.status -ne 'passed') {
        throw ('이번 실행의 성공 보고서가 없다: ' + $reportPath)
    }
    if (-not $applyReport.protected_assets_unchanged) { throw '보호 에셋 해시 검증에 실패했다.' }
    Write-Output ('SNS_DENSITY_APPLY_PASSED report=' + $reportPath)
}
finally {
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', $previousCache, 'Process')
}
