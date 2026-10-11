param([ValidateSet('Apply')][string]$Mode = 'Apply')
$ErrorActionPreference = 'Stop'
$promotionEffectsProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
    throw '열린 에디터가 있어 일회용 실행을 시작하지 않습니다.'
}
$promotionEffectsOutput = Join-Path $promotionEffectsProjectRoot 'Saved/PromotionEffects'
$promotionEffectsBackup = Join-Path $promotionEffectsOutput ('SessionBefore-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$promotionEffectsGroups = @(
    @{relative='Saved/Config/WindowsEditor'; names=@('EditorPerProjectUserSettings.ini','GameUserSettings.ini')},
    @{relative='Saved/SaveGames'; names=@()}
)
$promotionEffectsManifest = @()
foreach ($group in $promotionEffectsGroups) {
    $folder = Join-Path $promotionEffectsProjectRoot $group.relative
    if ($group.relative -eq 'Saved/SaveGames' -and (Test-Path -LiteralPath $folder)) {
        $group.names = @(Get-ChildItem -LiteralPath $folder -File | ForEach-Object Name)
    }
    foreach ($name in $group.names) {
        $relative = $group.relative + '/' + $name
        $source = Join-Path $promotionEffectsProjectRoot $relative
        $exists = Test-Path -LiteralPath $source
        $entry = [ordered]@{relative=$relative; existed=$exists; sha256=$null}
        if ($exists) {
            $destination = Join-Path $promotionEffectsBackup $relative
            New-Item -ItemType Directory -Force -Path (Split-Path $destination) | Out-Null
            Copy-Item -LiteralPath $source -Destination $destination
            $entry.sha256 = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
        }
        $promotionEffectsManifest += [pscustomobject]$entry
    }
}
New-Item -ItemType Directory -Force -Path $promotionEffectsBackup | Out-Null
$promotionEffectsManifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $promotionEffectsBackup 'manifest.json') -Encoding utf8
try {
    $promotionEffectsStarted = [DateTime]::UtcNow
    $scriptName = 'apply.py'
    & (Join-Path $promotionEffectsProjectRoot 'Tools/ui/blender/bright_globe/run_editor.ps1') `
        -Script (Join-Path $PSScriptRoot $scriptName) -Label ('promotion_effects_' + $Mode.ToLower()) -NullRHI
}
finally {
    # 프로세스 종료 보고와 OS 프로세스 목록 정리 사이의 짧은 차이를 기다린다.
    for ($attempt = 0; $attempt -lt 20; $attempt++) {
        if (-not (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue)) { break }
        Start-Sleep -Milliseconds 500
    }
    if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
        throw ('에디터가 남아 복원을 보류합니다. 백업: ' + $promotionEffectsBackup)
    }
    # 테스트가 새로 만든 개인 파일은 삭제하지 않고 검증 폴더에 보관한다.
    $newFiles = @()
    foreach ($entry in $promotionEffectsManifest) {
        $destination = Join-Path $promotionEffectsProjectRoot $entry.relative
        if ($entry.existed) {
            Copy-Item -LiteralPath (Join-Path $promotionEffectsBackup $entry.relative) -Destination $destination
            if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $entry.sha256) {
                throw ('개인 파일 복원 해시 불일치: ' + $entry.relative)
            }
        } elseif (Test-Path -LiteralPath $destination) { $newFiles += Get-Item -LiteralPath $destination }
    }
    $promotionEffectsSaveFolder = Join-Path $promotionEffectsProjectRoot 'Saved/SaveGames'
    if (Test-Path -LiteralPath $promotionEffectsSaveFolder) {
        $newFiles += @(Get-ChildItem -LiteralPath $promotionEffectsSaveFolder -File | Where-Object {$_.Name -notin $promotionEffectsGroups[1].names})
    }
    foreach ($file in $newFiles) {
        $sourcePath = [IO.Path]::GetFullPath($file.FullName)
        $allowedFolder = [IO.Path]::GetFullPath((Join-Path $promotionEffectsProjectRoot 'Saved')) + '\'
        if (-not $sourcePath.StartsWith($allowedFolder, [StringComparison]::OrdinalIgnoreCase)) {
            throw '프로젝트 Saved 밖의 파일 이동을 차단했습니다.'
        }
        $relative = $sourcePath.Substring($allowedFolder.Length)
        $destination = [IO.Path]::GetFullPath((Join-Path (Join-Path $promotionEffectsBackup 'CreatedDuringRun') $relative))
        if (-not $destination.StartsWith($promotionEffectsBackup + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw '백업 폴더 밖의 파일 이동을 차단했습니다.'
        }
        New-Item -ItemType Directory -Force -Path (Split-Path $destination) | Out-Null
        Move-Item -LiteralPath $sourcePath -Destination $destination
    }
    @{restored=$true; mode=$Mode; backup=$promotionEffectsBackup; files=$promotionEffectsManifest} | ConvertTo-Json -Depth 6 |
        Set-Content -LiteralPath (Join-Path $promotionEffectsOutput ($Mode.ToLower() + '-session-restored.json')) -Encoding utf8
}
$promotionEffectsReportFile = Join-Path $promotionEffectsOutput ($Mode.ToLower() + '-report.json')
if (-not (Test-Path -LiteralPath $promotionEffectsReportFile) -or
    (Get-Item -LiteralPath $promotionEffectsReportFile).LastWriteTimeUtc -lt $promotionEffectsStarted) {
    throw '이번 실행의 홍보 카드 적용 보고서가 생성되지 않았습니다.'
}
$promotionEffectsReport = Get-Content -LiteralPath $promotionEffectsReportFile -Raw | ConvertFrom-Json
if ($promotionEffectsReport.status -ne 'passed') { throw ('홍보 카드 정보 행 적용 ' + $Mode + ' 실패. Saved/PromotionEffects 보고서를 확인하세요.') }
Write-Output ('PROMOTION_EFFECTS_' + $Mode.ToUpper() + '_PASSED')
