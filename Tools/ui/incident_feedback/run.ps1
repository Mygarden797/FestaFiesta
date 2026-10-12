param([ValidateSet('Verify')][string]$Mode = 'Verify')
$ErrorActionPreference = 'Stop'
$incidentFeedbackProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
    throw '열린 에디터가 있어 일회용 실행을 시작하지 않습니다.'
}
$incidentFeedbackOutput = Join-Path $incidentFeedbackProjectRoot 'Saved/IncidentFeedback'
$incidentFeedbackBackup = Join-Path $incidentFeedbackOutput ('SessionBefore-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$incidentFeedbackGroups = @(
    @{relative='Saved/Config/WindowsEditor'; names=@('EditorPerProjectUserSettings.ini','GameUserSettings.ini')},
    @{relative='Saved/SaveGames'; names=@()}
)
$incidentFeedbackManifest = @()
foreach ($group in $incidentFeedbackGroups) {
    $folder = Join-Path $incidentFeedbackProjectRoot $group.relative
    if ($group.relative -eq 'Saved/SaveGames' -and (Test-Path -LiteralPath $folder)) {
        $group.names = @(Get-ChildItem -LiteralPath $folder -File | ForEach-Object Name)
    }
    foreach ($name in $group.names) {
        $relative = $group.relative + '/' + $name
        $source = Join-Path $incidentFeedbackProjectRoot $relative
        $exists = Test-Path -LiteralPath $source
        $entry = [ordered]@{relative=$relative; existed=$exists; sha256=$null}
        if ($exists) {
            $destination = Join-Path $incidentFeedbackBackup $relative
            New-Item -ItemType Directory -Force -Path (Split-Path $destination) | Out-Null
            Copy-Item -LiteralPath $source -Destination $destination
            $entry.sha256 = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
        }
        $incidentFeedbackManifest += [pscustomobject]$entry
    }
}
New-Item -ItemType Directory -Force -Path $incidentFeedbackBackup | Out-Null
$incidentFeedbackManifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $incidentFeedbackBackup 'manifest.json') -Encoding utf8
try {
    $incidentFeedbackStarted = [DateTime]::UtcNow
    $scriptName = 'verify.py'
    & (Join-Path $incidentFeedbackProjectRoot 'Tools/ui/blender/bright_globe/run_editor.ps1') `
        -Script (Join-Path $PSScriptRoot $scriptName) -Label ('incident_feedback_' + $Mode.ToLower()) -NullRHI
}
finally {
    # 프로세스 종료 보고와 OS 프로세스 목록 정리 사이의 짧은 차이를 기다린다.
    for ($attempt = 0; $attempt -lt 20; $attempt++) {
        if (-not (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue)) { break }
        Start-Sleep -Milliseconds 500
    }
    if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
        throw ('에디터가 남아 복원을 보류합니다. 백업: ' + $incidentFeedbackBackup)
    }
    # 테스트가 새로 만든 개인 파일은 삭제하지 않고 검증 폴더에 보관한다.
    $newFiles = @()
    foreach ($entry in $incidentFeedbackManifest) {
        $destination = Join-Path $incidentFeedbackProjectRoot $entry.relative
        if ($entry.existed) {
            Copy-Item -LiteralPath (Join-Path $incidentFeedbackBackup $entry.relative) -Destination $destination
            if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $entry.sha256) {
                throw ('개인 파일 복원 해시 불일치: ' + $entry.relative)
            }
        } elseif (Test-Path -LiteralPath $destination) { $newFiles += Get-Item -LiteralPath $destination }
    }
    $incidentFeedbackSaveFolder = Join-Path $incidentFeedbackProjectRoot 'Saved/SaveGames'
    if (Test-Path -LiteralPath $incidentFeedbackSaveFolder) {
        $newFiles += @(Get-ChildItem -LiteralPath $incidentFeedbackSaveFolder -File | Where-Object {$_.Name -notin $incidentFeedbackGroups[1].names})
    }
    foreach ($file in $newFiles) {
        $sourcePath = [IO.Path]::GetFullPath($file.FullName)
        $allowedFolder = [IO.Path]::GetFullPath((Join-Path $incidentFeedbackProjectRoot 'Saved')) + '\'
        if (-not $sourcePath.StartsWith($allowedFolder, [StringComparison]::OrdinalIgnoreCase)) {
            throw '프로젝트 Saved 밖의 파일 이동을 차단했습니다.'
        }
        $relative = $sourcePath.Substring($allowedFolder.Length)
        $destination = [IO.Path]::GetFullPath((Join-Path (Join-Path $incidentFeedbackBackup 'CreatedDuringRun') $relative))
        if (-not $destination.StartsWith($incidentFeedbackBackup + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw '백업 폴더 밖의 파일 이동을 차단했습니다.'
        }
        New-Item -ItemType Directory -Force -Path (Split-Path $destination) | Out-Null
        Move-Item -LiteralPath $sourcePath -Destination $destination
    }
    @{restored=$true; mode=$Mode; backup=$incidentFeedbackBackup; files=$incidentFeedbackManifest} | ConvertTo-Json -Depth 6 |
        Set-Content -LiteralPath (Join-Path $incidentFeedbackOutput ($Mode.ToLower() + '-session-restored.json')) -Encoding utf8
}
$incidentFeedbackReportFile = Join-Path $incidentFeedbackOutput ($Mode.ToLower() + '-report.json')
if (-not (Test-Path -LiteralPath $incidentFeedbackReportFile) -or
    (Get-Item -LiteralPath $incidentFeedbackReportFile).LastWriteTimeUtc -lt $incidentFeedbackStarted) {
    throw '이번 실행의 사건 종료 안내 검증 보고서가 생성되지 않았습니다.'
}
$incidentFeedbackReport = Get-Content -LiteralPath $incidentFeedbackReportFile -Raw | ConvertFrom-Json
if ($incidentFeedbackReport.status -ne 'passed') { throw ('사건 종료 안내 검증 ' + $Mode + ' 실패. Saved/IncidentFeedback 보고서를 확인하세요.') }
Write-Output ('INCIDENT_FEEDBACK_' + $Mode.ToUpper() + '_PASSED')
