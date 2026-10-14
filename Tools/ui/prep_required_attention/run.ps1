param()
$ErrorActionPreference = 'Stop'

$attentionProject = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$attentionProjectFile = Join-Path $attentionProject 'FestaFiesta.uproject'
if (-not (Test-Path -LiteralPath $attentionProjectFile)) { throw ('프로젝트 파일 없음: ' + $attentionProjectFile) }
if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
    throw '열린 에디터가 있어 점검 안내 적용을 시작하지 않습니다. 저장 후 종료하세요.'
}
$attentionOutput = Join-Path $attentionProject 'Saved/PrepRequiredAttention'
$attentionRun = Join-Path $attentionOutput ('Run-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + [Guid]::NewGuid().ToString('N'))
$attentionBackup = Join-Path $attentionRun 'SessionBefore'
New-Item -ItemType Directory -Path $attentionBackup | Out-Null

# live_staff_compact runner의 개인 설정/저장 백업·복원 방식을 재사용한다.
# 하위 폴더와 추가 설정 파일도 포함하고 실행 도중 생성된 파일은 백업으로 옮긴다.
$attentionProtectedFolders = @('Saved/Config/WindowsEditor', 'Saved/SaveGames')
$attentionRelativeFiles = @(
    'Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini',
    'Saved/Config/WindowsEditor/GameUserSettings.ini'
)
foreach ($attentionRelativeFolder in $attentionProtectedFolders) {
    $attentionFolder = Join-Path $attentionProject $attentionRelativeFolder
    if (Test-Path -LiteralPath $attentionFolder) {
        $attentionRelativeFiles += @(Get-ChildItem -LiteralPath $attentionFolder -File -Recurse | ForEach-Object {
            $_.FullName.Substring($attentionProject.Length + 1).Replace('\','/')
        })
    }
}
$attentionManifest = @()
foreach ($attentionRelative in $attentionRelativeFiles | Sort-Object -Unique) {
    $attentionSource = Join-Path $attentionProject $attentionRelative
    $attentionEntry = [ordered]@{relative=$attentionRelative; existed=(Test-Path -LiteralPath $attentionSource); sha256=$null}
    if ($attentionEntry.existed) {
        $attentionDestination = Join-Path $attentionBackup $attentionRelative
        New-Item -ItemType Directory -Force -Path (Split-Path $attentionDestination) | Out-Null
        Copy-Item -LiteralPath $attentionSource -Destination $attentionDestination
        $attentionEntry.sha256 = (Get-FileHash -LiteralPath $attentionSource -Algorithm SHA256).Hash
    }
    $attentionManifest += [pscustomobject]$attentionEntry
}
$attentionManifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $attentionBackup 'manifest.json') -Encoding utf8
$attentionStartedUtc = [DateTime]::UtcNow
$attentionExitCode = $null
$attentionRunError = $null
try {
    $attentionArgs = @(
        ('"' + $attentionProjectFile + '"'), '/Engine/Maps/Entry',
        ('-ExecutePythonScript="' + (Join-Path $PSScriptRoot 'apply.py') + '"'),
        ('-PrepRequiredAttentionOutput="' + $attentionRun + '"'),
        ('-UserDir="' + (Join-Path $attentionRun 'User') + '"'),
        '-DisablePlugins=RiderLink', '-unattended', '-nosplash', '-nosound', '-NoSourceControl',
        '-NullRHI', '-ddc=FestaFiestaBuildDDC',
        ('-abslog="' + (Join-Path $attentionRun 'apply.log') + '"')
    )
    Write-Output ('PREP_REQUIRED_ATTENTION_START output=' + $attentionRun)
    $attentionProcess = Start-Process -FilePath 'C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $attentionArgs -WindowStyle Hidden -PassThru
    Write-Output ('PREP_REQUIRED_ATTENTION_PID=' + $attentionProcess.Id)
    $null = $attentionProcess.Handle
    $attentionProcess.WaitForExit()
    $attentionExitCode = $attentionProcess.ExitCode
} catch {
    $attentionRunError = $_.Exception.Message
} finally {
    for ($attentionAttempt = 0; $attentionAttempt -lt 20; $attentionAttempt++) {
        if (-not (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue)) { break }
        Start-Sleep -Milliseconds 500
    }
    if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
        throw ('에디터가 남아 개인 파일 복원을 보류합니다. 백업: ' + $attentionBackup)
    }
    $attentionOriginalFiles = @($attentionManifest | Where-Object existed | ForEach-Object relative)
    foreach ($attentionEntry in $attentionManifest | Where-Object existed) {
        $attentionDestination = Join-Path $attentionProject $attentionEntry.relative
        New-Item -ItemType Directory -Force -Path (Split-Path $attentionDestination) | Out-Null
        Copy-Item -LiteralPath (Join-Path $attentionBackup $attentionEntry.relative) -Destination $attentionDestination -Force
        if ((Get-FileHash -LiteralPath $attentionDestination -Algorithm SHA256).Hash -ne $attentionEntry.sha256) {
            throw ('개인 파일 복원 해시 불일치: ' + $attentionEntry.relative)
        }
    }
    $attentionMovedFiles = @()
    foreach ($attentionRelativeFolder in $attentionProtectedFolders) {
        $attentionFolder = Join-Path $attentionProject $attentionRelativeFolder
        if (-not (Test-Path -LiteralPath $attentionFolder)) { continue }
        $attentionAllowedFolder = [IO.Path]::GetFullPath($attentionFolder).TrimEnd('\') + '\'
        foreach ($attentionFile in @(Get-ChildItem -LiteralPath $attentionFolder -File -Recurse)) {
            $attentionSourcePath = [IO.Path]::GetFullPath($attentionFile.FullName)
            $attentionRelative = $attentionSourcePath.Substring($attentionProject.Length + 1).Replace('\','/')
            if ($attentionRelative -in $attentionOriginalFiles) { continue }
            if (-not $attentionSourcePath.StartsWith($attentionAllowedFolder, [StringComparison]::OrdinalIgnoreCase)) {
                throw '보호 대상 폴더 밖의 파일 이동을 차단했습니다.'
            }
            $attentionDestination = [IO.Path]::GetFullPath((Join-Path (Join-Path $attentionBackup 'CreatedDuringRun') $attentionRelative))
            if (-not $attentionDestination.StartsWith($attentionBackup + '\', [StringComparison]::OrdinalIgnoreCase)) {
                throw '백업 폴더 밖의 파일 이동을 차단했습니다.'
            }
            New-Item -ItemType Directory -Force -Path (Split-Path $attentionDestination) | Out-Null
            Move-Item -LiteralPath $attentionSourcePath -Destination $attentionDestination
            $attentionMovedFiles += $attentionRelative
        }
    }
    @{restored=$true; backup=$attentionBackup; files=$attentionManifest; created_during_run=$attentionMovedFiles} |
        ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $attentionRun 'session-restored.json') -Encoding utf8
}

if ($attentionRunError) { throw $attentionRunError }
if ($null -eq $attentionExitCode -or $attentionExitCode -ne 0) { throw ('에디터 종료 코드: ' + $attentionExitCode + '. 로그: ' + $attentionRun) }
# 실행별 GUID 폴더에 새로 생성한 결과만 판정한다.
$attentionReportPath = Join-Path $attentionRun 'apply-report.json'
if (-not (Test-Path -LiteralPath $attentionReportPath) -or (Get-Item -LiteralPath $attentionReportPath).LastWriteTimeUtc -lt $attentionStartedUtc) {
    throw ('새 적용 결과 없음: ' + $attentionRun)
}
$attentionReport = Get-Content -LiteralPath $attentionReportPath -Raw | ConvertFrom-Json
if ($attentionReport.status -ne 'passed' -or -not $attentionReport.protected_unchanged -or -not $attentionReport.repeat_unchanged) {
    throw ('점검 안내 적용/보존 검사 실패: ' + $attentionReportPath)
}
Write-Output ('PREP_REQUIRED_ATTENTION_PASSED report=' + $attentionReportPath)
