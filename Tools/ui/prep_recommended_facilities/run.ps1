param([ValidateSet('Apply','Test')][string]$Mode = 'Apply')
$ErrorActionPreference = 'Stop'
$recommendedProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
    throw '열린 에디터가 있어 일회용 실행을 시작하지 않습니다.'
}
$recommendedOutput = Join-Path $recommendedProjectRoot 'Saved/PrepRecommended'
$recommendedBackup = Join-Path $recommendedOutput ('SessionBefore-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$recommendedGroups = @(
    @{relative='Saved/Config/WindowsEditor'; names=@('EditorPerProjectUserSettings.ini','GameUserSettings.ini')},
    @{relative='Saved/SaveGames'; names=@()}
)
$recommendedManifest = @()
foreach ($group in $recommendedGroups) {
    $folder = Join-Path $recommendedProjectRoot $group.relative
    if ($group.relative -eq 'Saved/SaveGames' -and (Test-Path -LiteralPath $folder)) {
        $group.names = @(Get-ChildItem -LiteralPath $folder -File | ForEach-Object Name)
    }
    foreach ($name in $group.names) {
        $relative = $group.relative + '/' + $name
        $source = Join-Path $recommendedProjectRoot $relative
        $exists = Test-Path -LiteralPath $source
        $entry = [ordered]@{relative=$relative; existed=$exists; sha256=$null}
        if ($exists) {
            $destination = Join-Path $recommendedBackup $relative
            New-Item -ItemType Directory -Force -Path (Split-Path $destination) | Out-Null
            Copy-Item -LiteralPath $source -Destination $destination
            $entry.sha256 = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
        }
        $recommendedManifest += [pscustomobject]$entry
    }
}
New-Item -ItemType Directory -Force -Path $recommendedBackup | Out-Null
$recommendedManifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $recommendedBackup 'manifest.json') -Encoding utf8
$previousRecommendedMode = [Environment]::GetEnvironmentVariable('FF_PREP_RECOMMENDED_MODE', 'Process')
try {
    [Environment]::SetEnvironmentVariable('FF_PREP_RECOMMENDED_MODE', $Mode, 'Process')
    $scriptName = if ($Mode -eq 'Test') { 'test.py' } else { 'apply.py' }
    & (Join-Path $recommendedProjectRoot 'Tools/ui/blender/bright_globe/run_editor.ps1') `
        -Script (Join-Path $PSScriptRoot $scriptName) -Label ('prep_recommended_' + $Mode.ToLower()) -NullRHI
}
finally {
    [Environment]::SetEnvironmentVariable('FF_PREP_RECOMMENDED_MODE', $previousRecommendedMode, 'Process')
    # 프로세스 종료 보고와 OS 프로세스 목록 정리 사이의 짧은 차이를 기다린다.
    for ($attempt = 0; $attempt -lt 20; $attempt++) {
        if (-not (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue)) { break }
        Start-Sleep -Milliseconds 500
    }
    if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
        throw ('에디터가 남아 복원을 보류합니다. 백업: ' + $recommendedBackup)
    }
    # 테스트가 새로 만든 개인 파일은 삭제하지 않고 검증 폴더에 보관한다.
    $newFiles = @()
    foreach ($entry in $recommendedManifest) {
        $destination = Join-Path $recommendedProjectRoot $entry.relative
        if ($entry.existed) {
            Copy-Item -LiteralPath (Join-Path $recommendedBackup $entry.relative) -Destination $destination
            if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $entry.sha256) {
                throw ('개인 파일 복원 해시 불일치: ' + $entry.relative)
            }
        } elseif (Test-Path -LiteralPath $destination) { $newFiles += Get-Item -LiteralPath $destination }
    }
    $recommendedSaveFolder = Join-Path $recommendedProjectRoot 'Saved/SaveGames'
    if (Test-Path -LiteralPath $recommendedSaveFolder) {
        $newFiles += @(Get-ChildItem -LiteralPath $recommendedSaveFolder -File | Where-Object {$_.Name -notin $recommendedGroups[1].names})
    }
    foreach ($file in $newFiles) {
        $sourcePath = [IO.Path]::GetFullPath($file.FullName)
        $allowedFolder = [IO.Path]::GetFullPath((Join-Path $recommendedProjectRoot 'Saved')) + '\'
        if (-not $sourcePath.StartsWith($allowedFolder, [StringComparison]::OrdinalIgnoreCase)) {
            throw '프로젝트 Saved 밖의 파일 이동을 차단했습니다.'
        }
        $relative = $sourcePath.Substring($allowedFolder.Length)
        $destination = [IO.Path]::GetFullPath((Join-Path (Join-Path $recommendedBackup 'CreatedDuringRun') $relative))
        if (-not $destination.StartsWith($recommendedBackup + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw '백업 폴더 밖의 파일 이동을 차단했습니다.'
        }
        New-Item -ItemType Directory -Force -Path (Split-Path $destination) | Out-Null
        Move-Item -LiteralPath $sourcePath -Destination $destination
    }
    @{restored=$true; mode=$Mode; backup=$recommendedBackup; files=$recommendedManifest} | ConvertTo-Json -Depth 6 |
        Set-Content -LiteralPath (Join-Path $recommendedOutput ($Mode.ToLower() + '-session-restored.json')) -Encoding utf8
}
$recommendedReport = Get-Content -LiteralPath (Join-Path $recommendedOutput ($Mode.ToLower() + '-report.json')) -Raw | ConvertFrom-Json
if ($recommendedReport.status -ne 'passed') { throw ('권장 시설 ' + $Mode + ' 실패. Saved/PrepRecommended 보고서를 확인하세요.') }
Write-Output ('PREP_RECOMMENDED_' + $Mode.ToUpper() + '_PASSED')
