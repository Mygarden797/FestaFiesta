$Mode = 'Apply'
$ErrorActionPreference = 'Stop'
$dockProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
    throw '열린 에디터가 있어 일회용 실행을 시작하지 않습니다.'
}
$dockOutput = Join-Path $dockProjectRoot 'Saved/LiveUtilityDock'
$dockBackup = Join-Path $dockOutput ('SessionBefore-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$dockGroups = @(
    @{relative='Saved/Config/WindowsEditor'; names=@('EditorPerProjectUserSettings.ini','GameUserSettings.ini')},
    @{relative='Saved/SaveGames'; names=@()}
)
$dockManifest = @()
foreach ($group in $dockGroups) {
    $folder = Join-Path $dockProjectRoot $group.relative
    if ($group.relative -eq 'Saved/SaveGames' -and (Test-Path -LiteralPath $folder)) {
        $group.names = @(Get-ChildItem -LiteralPath $folder -File | ForEach-Object Name)
    }
    foreach ($name in $group.names) {
        $relative = $group.relative + '/' + $name
        $source = Join-Path $dockProjectRoot $relative
        $exists = Test-Path -LiteralPath $source
        $entry = [ordered]@{relative=$relative; existed=$exists; sha256=$null}
        if ($exists) {
            $destination = Join-Path $dockBackup $relative
            New-Item -ItemType Directory -Force -Path (Split-Path $destination) | Out-Null
            Copy-Item -LiteralPath $source -Destination $destination
            $entry.sha256 = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
        }
        $dockManifest += [pscustomobject]$entry
    }
}
New-Item -ItemType Directory -Force -Path $dockBackup | Out-Null
$dockManifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $dockBackup 'manifest.json') -Encoding utf8
$previousDockMode = [Environment]::GetEnvironmentVariable('FF_LIVE_UTILITY_DOCK_MODE', 'Process')
try {
    [Environment]::SetEnvironmentVariable('FF_LIVE_UTILITY_DOCK_MODE', $Mode, 'Process')
    $scriptName = 'apply.py'
    & (Join-Path $dockProjectRoot 'Tools/ui/blender/bright_globe/run_editor.ps1') `
        -Script (Join-Path $PSScriptRoot $scriptName) -Label ('live_utility_dock_' + $Mode.ToLower()) -NullRHI
}
finally {
    [Environment]::SetEnvironmentVariable('FF_LIVE_UTILITY_DOCK_MODE', $previousDockMode, 'Process')
    # 프로세스 종료 보고와 OS 프로세스 목록 정리 사이의 짧은 차이를 기다린다.
    for ($attempt = 0; $attempt -lt 20; $attempt++) {
        if (-not (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue)) { break }
        Start-Sleep -Milliseconds 500
    }
    if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
        throw ('에디터가 남아 복원을 보류합니다. 백업: ' + $dockBackup)
    }
    # 테스트가 새로 만든 개인 파일은 삭제하지 않고 검증 폴더에 보관한다.
    $newFiles = @()
    foreach ($entry in $dockManifest) {
        $destination = Join-Path $dockProjectRoot $entry.relative
        if ($entry.existed) {
            Copy-Item -LiteralPath (Join-Path $dockBackup $entry.relative) -Destination $destination
            if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $entry.sha256) {
                throw ('개인 파일 복원 해시 불일치: ' + $entry.relative)
            }
        } elseif (Test-Path -LiteralPath $destination) { $newFiles += Get-Item -LiteralPath $destination }
    }
    $dockSaveFolder = Join-Path $dockProjectRoot 'Saved/SaveGames'
    if (Test-Path -LiteralPath $dockSaveFolder) {
        $newFiles += @(Get-ChildItem -LiteralPath $dockSaveFolder -File | Where-Object {$_.Name -notin $dockGroups[1].names})
    }
    foreach ($file in $newFiles) {
        $sourcePath = [IO.Path]::GetFullPath($file.FullName)
        $allowedFolder = [IO.Path]::GetFullPath((Join-Path $dockProjectRoot 'Saved')) + '\'
        if (-not $sourcePath.StartsWith($allowedFolder, [StringComparison]::OrdinalIgnoreCase)) {
            throw '프로젝트 Saved 밖의 파일 이동을 차단했습니다.'
        }
        $relative = $sourcePath.Substring($allowedFolder.Length)
        $destination = [IO.Path]::GetFullPath((Join-Path (Join-Path $dockBackup 'CreatedDuringRun') $relative))
        if (-not $destination.StartsWith($dockBackup + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw '백업 폴더 밖의 파일 이동을 차단했습니다.'
        }
        New-Item -ItemType Directory -Force -Path (Split-Path $destination) | Out-Null
        Move-Item -LiteralPath $sourcePath -Destination $destination
    }
    @{restored=$true; mode=$Mode; backup=$dockBackup; files=$dockManifest} | ConvertTo-Json -Depth 6 |
        Set-Content -LiteralPath (Join-Path $dockOutput ($Mode.ToLower() + '-session-restored.json')) -Encoding utf8
}
$dockReport = Get-Content -LiteralPath (Join-Path $dockOutput ($Mode.ToLower() + '-report.json')) -Raw | ConvertFrom-Json
if ($dockReport.status -ne 'passed') { throw ('SNS·음악 P2 ' + $Mode + ' 실패. Saved/LiveUtilityDock 보고서를 확인하세요.') }
Write-Output ('LIVE_UTILITY_DOCK_' + $Mode.ToUpper() + '_PASSED')




