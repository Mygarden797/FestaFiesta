param([ValidateSet('Verify')][string]$Mode = 'Verify')
$ErrorActionPreference = 'Stop'
$staffEffectProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
    throw '열린 에디터가 있어 일회용 실행을 시작하지 않습니다.'
}
$staffEffectOutput = Join-Path $staffEffectProjectRoot 'Saved/StaffEffect'
$staffEffectBackup = Join-Path $staffEffectOutput ('SessionBefore-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$staffEffectGroups = @(
    @{relative='Saved/Config/WindowsEditor'; names=@('EditorPerProjectUserSettings.ini','GameUserSettings.ini')},
    @{relative='Saved/SaveGames'; names=@()}
)
$staffEffectManifest = @()
foreach ($group in $staffEffectGroups) {
    $folder = Join-Path $staffEffectProjectRoot $group.relative
    if ($group.relative -eq 'Saved/SaveGames' -and (Test-Path -LiteralPath $folder)) {
        $group.names = @(Get-ChildItem -LiteralPath $folder -File | ForEach-Object Name)
    }
    foreach ($name in $group.names) {
        $relative = $group.relative + '/' + $name
        $source = Join-Path $staffEffectProjectRoot $relative
        $exists = Test-Path -LiteralPath $source
        $entry = [ordered]@{relative=$relative; existed=$exists; sha256=$null}
        if ($exists) {
            $destination = Join-Path $staffEffectBackup $relative
            New-Item -ItemType Directory -Force -Path (Split-Path $destination) | Out-Null
            Copy-Item -LiteralPath $source -Destination $destination
            $entry.sha256 = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
        }
        $staffEffectManifest += [pscustomobject]$entry
    }
}
New-Item -ItemType Directory -Force -Path $staffEffectBackup | Out-Null
$staffEffectManifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $staffEffectBackup 'manifest.json') -Encoding utf8
try {
    $staffEffectStarted = [DateTime]::UtcNow
    $scriptName = 'verify.py'
    & (Join-Path $staffEffectProjectRoot 'Tools/ui/blender/bright_globe/run_editor.ps1') `
        -Script (Join-Path $PSScriptRoot $scriptName) -Label ('staff_effect_' + $Mode.ToLower()) -NullRHI
}
finally {
    # 프로세스 종료 보고와 OS 프로세스 목록 정리 사이의 짧은 차이를 기다린다.
    for ($attempt = 0; $attempt -lt 20; $attempt++) {
        if (-not (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue)) { break }
        Start-Sleep -Milliseconds 500
    }
    if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
        throw ('에디터가 남아 복원을 보류합니다. 백업: ' + $staffEffectBackup)
    }
    # 테스트가 새로 만든 개인 파일은 삭제하지 않고 검증 폴더에 보관한다.
    $newFiles = @()
    foreach ($entry in $staffEffectManifest) {
        $destination = Join-Path $staffEffectProjectRoot $entry.relative
        if ($entry.existed) {
            Copy-Item -LiteralPath (Join-Path $staffEffectBackup $entry.relative) -Destination $destination
            if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $entry.sha256) {
                throw ('개인 파일 복원 해시 불일치: ' + $entry.relative)
            }
        } elseif (Test-Path -LiteralPath $destination) { $newFiles += Get-Item -LiteralPath $destination }
    }
    $staffEffectSaveFolder = Join-Path $staffEffectProjectRoot 'Saved/SaveGames'
    if (Test-Path -LiteralPath $staffEffectSaveFolder) {
        $newFiles += @(Get-ChildItem -LiteralPath $staffEffectSaveFolder -File | Where-Object {$_.Name -notin $staffEffectGroups[1].names})
    }
    foreach ($file in $newFiles) {
        $sourcePath = [IO.Path]::GetFullPath($file.FullName)
        $allowedFolder = [IO.Path]::GetFullPath((Join-Path $staffEffectProjectRoot 'Saved')) + '\'
        if (-not $sourcePath.StartsWith($allowedFolder, [StringComparison]::OrdinalIgnoreCase)) {
            throw '프로젝트 Saved 밖의 파일 이동을 차단했습니다.'
        }
        $relative = $sourcePath.Substring($allowedFolder.Length)
        $destination = [IO.Path]::GetFullPath((Join-Path (Join-Path $staffEffectBackup 'CreatedDuringRun') $relative))
        if (-not $destination.StartsWith($staffEffectBackup + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw '백업 폴더 밖의 파일 이동을 차단했습니다.'
        }
        New-Item -ItemType Directory -Force -Path (Split-Path $destination) | Out-Null
        Move-Item -LiteralPath $sourcePath -Destination $destination
    }
    @{restored=$true; mode=$Mode; backup=$staffEffectBackup; files=$staffEffectManifest} | ConvertTo-Json -Depth 6 |
        Set-Content -LiteralPath (Join-Path $staffEffectOutput ($Mode.ToLower() + '-session-restored.json')) -Encoding utf8
}
$staffEffectReportFile = Join-Path $staffEffectOutput ($Mode.ToLower() + '-report.json')
if (-not (Test-Path -LiteralPath $staffEffectReportFile) -or
    (Get-Item -LiteralPath $staffEffectReportFile).LastWriteTimeUtc -lt $staffEffectStarted) {
    throw '이번 실행의 스태프 숙련도 검증 보고서가 생성되지 않았습니다.'
}
$staffEffectReport = Get-Content -LiteralPath $staffEffectReportFile -Raw | ConvertFrom-Json
if ($staffEffectReport.status -ne 'passed') { throw ('스태프 숙련도 검증 ' + $Mode + ' 실패. Saved/StaffEffect 보고서를 확인하세요.') }
Write-Output ('STAFF_EFFECT_' + $Mode.ToUpper() + '_PASSED')
