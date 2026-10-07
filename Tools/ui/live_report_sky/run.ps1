$Mode = 'Apply'
$ErrorActionPreference = 'Stop'
$reportProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
    throw '열린 에디터가 있어 일회용 실행을 시작하지 않습니다.'
}
$reportOutput = Join-Path $reportProjectRoot 'Saved/LiveReportSky'
$reportBackup = Join-Path $reportOutput ('SessionBefore-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$reportGroups = @(
    @{relative='Saved/Config/WindowsEditor'; names=@('EditorPerProjectUserSettings.ini','GameUserSettings.ini')},
    @{relative='Saved/SaveGames'; names=@()}
)
$reportManifest = @()
foreach ($group in $reportGroups) {
    $folder = Join-Path $reportProjectRoot $group.relative
    if ($group.relative -eq 'Saved/SaveGames' -and (Test-Path -LiteralPath $folder)) {
        $group.names = @(Get-ChildItem -LiteralPath $folder -File | ForEach-Object Name)
    }
    foreach ($name in $group.names) {
        $relative = $group.relative + '/' + $name
        $source = Join-Path $reportProjectRoot $relative
        $exists = Test-Path -LiteralPath $source
        $entry = [ordered]@{relative=$relative; existed=$exists; sha256=$null}
        if ($exists) {
            $destination = Join-Path $reportBackup $relative
            New-Item -ItemType Directory -Force -Path (Split-Path $destination) | Out-Null
            Copy-Item -LiteralPath $source -Destination $destination
            $entry.sha256 = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
        }
        $reportManifest += [pscustomobject]$entry
    }
}
New-Item -ItemType Directory -Force -Path $reportBackup | Out-Null
$reportManifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $reportBackup 'manifest.json') -Encoding utf8
$previousReportMode = [Environment]::GetEnvironmentVariable('FF_LIVE_REPORT_SKY_MODE', 'Process')
try {
    [Environment]::SetEnvironmentVariable('FF_LIVE_REPORT_SKY_MODE', $Mode, 'Process')
    $scriptName = 'apply.py'
    & (Join-Path $reportProjectRoot 'Tools/ui/blender/bright_globe/run_editor.ps1') `
        -Script (Join-Path $PSScriptRoot $scriptName) -Label ('live_report_sky_' + $Mode.ToLower()) -NullRHI
}
finally {
    [Environment]::SetEnvironmentVariable('FF_LIVE_REPORT_SKY_MODE', $previousReportMode, 'Process')
    # 프로세스 종료 보고와 OS 프로세스 목록 정리 사이의 짧은 차이를 기다린다.
    for ($attempt = 0; $attempt -lt 20; $attempt++) {
        if (-not (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue)) { break }
        Start-Sleep -Milliseconds 500
    }
    if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
        throw ('에디터가 남아 복원을 보류합니다. 백업: ' + $reportBackup)
    }
    # 테스트가 새로 만든 개인 파일은 삭제하지 않고 검증 폴더에 보관한다.
    $newFiles = @()
    foreach ($entry in $reportManifest) {
        $destination = Join-Path $reportProjectRoot $entry.relative
        if ($entry.existed) {
            Copy-Item -LiteralPath (Join-Path $reportBackup $entry.relative) -Destination $destination
            if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $entry.sha256) {
                throw ('개인 파일 복원 해시 불일치: ' + $entry.relative)
            }
        } elseif (Test-Path -LiteralPath $destination) { $newFiles += Get-Item -LiteralPath $destination }
    }
    $reportSaveFolder = Join-Path $reportProjectRoot 'Saved/SaveGames'
    if (Test-Path -LiteralPath $reportSaveFolder) {
        $newFiles += @(Get-ChildItem -LiteralPath $reportSaveFolder -File | Where-Object {$_.Name -notin $reportGroups[1].names})
    }
    foreach ($file in $newFiles) {
        $sourcePath = [IO.Path]::GetFullPath($file.FullName)
        $allowedFolder = [IO.Path]::GetFullPath((Join-Path $reportProjectRoot 'Saved')) + '\'
        if (-not $sourcePath.StartsWith($allowedFolder, [StringComparison]::OrdinalIgnoreCase)) {
            throw '프로젝트 Saved 밖의 파일 이동을 차단했습니다.'
        }
        $relative = $sourcePath.Substring($allowedFolder.Length)
        $destination = [IO.Path]::GetFullPath((Join-Path (Join-Path $reportBackup 'CreatedDuringRun') $relative))
        if (-not $destination.StartsWith($reportBackup + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw '백업 폴더 밖의 파일 이동을 차단했습니다.'
        }
        New-Item -ItemType Directory -Force -Path (Split-Path $destination) | Out-Null
        Move-Item -LiteralPath $sourcePath -Destination $destination
    }
    @{restored=$true; mode=$Mode; backup=$reportBackup; files=$reportManifest} | ConvertTo-Json -Depth 6 |
        Set-Content -LiteralPath (Join-Path $reportOutput ($Mode.ToLower() + '-session-restored.json')) -Encoding utf8
}
$reportReport = Get-Content -LiteralPath (Join-Path $reportOutput ($Mode.ToLower() + '-report.json')) -Raw | ConvertFrom-Json
if ($reportReport.status -ne 'passed') { throw ('일일 결산 P4 ' + $Mode + ' 실패. Saved/LiveReportSky 보고서를 확인하세요.') }
Write-Output ('LIVE_REPORT_SKY_' + $Mode.ToUpper() + '_PASSED')
