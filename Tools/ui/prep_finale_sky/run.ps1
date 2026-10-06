$Mode = 'Apply'
$ErrorActionPreference = 'Stop'
$finaleProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
    throw '열린 에디터가 있어 일회용 실행을 시작하지 않습니다.'
}
$finaleOutput = Join-Path $finaleProjectRoot 'Saved/PrepFinaleSky'
$finaleBackup = Join-Path $finaleOutput ('SessionBefore-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$finaleGroups = @(
    @{relative='Saved/Config/WindowsEditor'; names=@('EditorPerProjectUserSettings.ini','GameUserSettings.ini')},
    @{relative='Saved/SaveGames'; names=@()}
)
$finaleManifest = @()
foreach ($group in $finaleGroups) {
    $folder = Join-Path $finaleProjectRoot $group.relative
    if ($group.relative -eq 'Saved/SaveGames' -and (Test-Path -LiteralPath $folder)) {
        $group.names = @(Get-ChildItem -LiteralPath $folder -File | ForEach-Object Name)
    }
    foreach ($name in $group.names) {
        $relative = $group.relative + '/' + $name
        $source = Join-Path $finaleProjectRoot $relative
        $exists = Test-Path -LiteralPath $source
        $entry = [ordered]@{relative=$relative; existed=$exists; sha256=$null}
        if ($exists) {
            $destination = Join-Path $finaleBackup $relative
            New-Item -ItemType Directory -Force -Path (Split-Path $destination) | Out-Null
            Copy-Item -LiteralPath $source -Destination $destination
            $entry.sha256 = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
        }
        $finaleManifest += [pscustomobject]$entry
    }
}
New-Item -ItemType Directory -Force -Path $finaleBackup | Out-Null
$finaleManifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $finaleBackup 'manifest.json') -Encoding utf8
$previousFinaleMode = [Environment]::GetEnvironmentVariable('FF_PREP_FINALE_SKY_MODE', 'Process')
try {
    [Environment]::SetEnvironmentVariable('FF_PREP_FINALE_SKY_MODE', $Mode, 'Process')
    $scriptName = 'apply.py'
    & (Join-Path $finaleProjectRoot 'Tools/ui/blender/bright_globe/run_editor.ps1') `
        -Script (Join-Path $PSScriptRoot $scriptName) -Label ('prep_finale_sky_' + $Mode.ToLower()) -NullRHI
}
finally {
    [Environment]::SetEnvironmentVariable('FF_PREP_FINALE_SKY_MODE', $previousFinaleMode, 'Process')
    # 프로세스 종료 보고와 OS 프로세스 목록 정리 사이의 짧은 차이를 기다린다.
    for ($attempt = 0; $attempt -lt 20; $attempt++) {
        if (-not (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue)) { break }
        Start-Sleep -Milliseconds 500
    }
    if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
        throw ('에디터가 남아 복원을 보류합니다. 백업: ' + $finaleBackup)
    }
    # 테스트가 새로 만든 개인 파일은 삭제하지 않고 검증 폴더에 보관한다.
    $newFiles = @()
    foreach ($entry in $finaleManifest) {
        $destination = Join-Path $finaleProjectRoot $entry.relative
        if ($entry.existed) {
            Copy-Item -LiteralPath (Join-Path $finaleBackup $entry.relative) -Destination $destination
            if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $entry.sha256) {
                throw ('개인 파일 복원 해시 불일치: ' + $entry.relative)
            }
        } elseif (Test-Path -LiteralPath $destination) { $newFiles += Get-Item -LiteralPath $destination }
    }
    $finaleSaveFolder = Join-Path $finaleProjectRoot 'Saved/SaveGames'
    if (Test-Path -LiteralPath $finaleSaveFolder) {
        $newFiles += @(Get-ChildItem -LiteralPath $finaleSaveFolder -File | Where-Object {$_.Name -notin $finaleGroups[1].names})
    }
    foreach ($file in $newFiles) {
        $sourcePath = [IO.Path]::GetFullPath($file.FullName)
        $allowedFolder = [IO.Path]::GetFullPath((Join-Path $finaleProjectRoot 'Saved')) + '\'
        if (-not $sourcePath.StartsWith($allowedFolder, [StringComparison]::OrdinalIgnoreCase)) {
            throw '프로젝트 Saved 밖의 파일 이동을 차단했습니다.'
        }
        $relative = $sourcePath.Substring($allowedFolder.Length)
        $destination = [IO.Path]::GetFullPath((Join-Path (Join-Path $finaleBackup 'CreatedDuringRun') $relative))
        if (-not $destination.StartsWith($finaleBackup + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw '백업 폴더 밖의 파일 이동을 차단했습니다.'
        }
        New-Item -ItemType Directory -Force -Path (Split-Path $destination) | Out-Null
        Move-Item -LiteralPath $sourcePath -Destination $destination
    }
    @{restored=$true; mode=$Mode; backup=$finaleBackup; files=$finaleManifest} | ConvertTo-Json -Depth 6 |
        Set-Content -LiteralPath (Join-Path $finaleOutput ($Mode.ToLower() + '-session-restored.json')) -Encoding utf8
}
$finaleReport = Get-Content -LiteralPath (Join-Path $finaleOutput ($Mode.ToLower() + '-report.json')) -Raw | ConvertFrom-Json
if ($finaleReport.status -ne 'passed') { throw ('피날레 P1 ' + $Mode + ' 실패. Saved/PrepFinaleSky 보고서를 확인하세요.') }
Write-Output ('PREP_FINALE_SKY_' + $Mode.ToUpper() + '_PASSED')



