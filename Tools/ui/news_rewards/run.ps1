param([ValidateSet('Apply','Verify')][string]$Mode = 'Verify', [string]$TestName = '')
$ErrorActionPreference = 'Stop'
if ($TestName -and $Mode -ne 'Verify') { throw 'TestName은 Verify 모드에서만 사용할 수 있습니다.' }
$newsRewardsProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
    throw '열린 에디터가 있어 일회용 실행을 시작하지 않습니다.'
}
$newsRewardsOutput = Join-Path $newsRewardsProjectRoot 'Saved/NewsRewards'
$newsRewardsBackup = Join-Path $newsRewardsOutput ('SessionBefore-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$newsRewardsGroups = @(
    @{relative='Saved/Config/WindowsEditor'; names=@('EditorPerProjectUserSettings.ini','GameUserSettings.ini')},
    @{relative='Saved/SaveGames'; names=@()}
)
$newsRewardsManifest = @()
foreach ($group in $newsRewardsGroups) {
    $folder = Join-Path $newsRewardsProjectRoot $group.relative
    if ($group.relative -eq 'Saved/SaveGames' -and (Test-Path -LiteralPath $folder)) {
        $group.names = @(Get-ChildItem -LiteralPath $folder -File | ForEach-Object Name)
    }
    foreach ($name in $group.names) {
        $relative = $group.relative + '/' + $name
        $source = Join-Path $newsRewardsProjectRoot $relative
        $exists = Test-Path -LiteralPath $source
        $entry = [ordered]@{relative=$relative; existed=$exists; sha256=$null}
        if ($exists) {
            $destination = Join-Path $newsRewardsBackup $relative
            New-Item -ItemType Directory -Force -Path (Split-Path $destination) | Out-Null
            Copy-Item -LiteralPath $source -Destination $destination
            $entry.sha256 = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
        }
        $newsRewardsManifest += [pscustomobject]$entry
    }
}
New-Item -ItemType Directory -Force -Path $newsRewardsBackup | Out-Null
$newsRewardsManifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $newsRewardsBackup 'manifest.json') -Encoding utf8
$newsRewardsPreviousTest = [Environment]::GetEnvironmentVariable('FF_NEWS_REWARDS_TEST')
try {
    [Environment]::SetEnvironmentVariable('FF_NEWS_REWARDS_TEST', $TestName)
    $newsRewardsStarted = [DateTime]::UtcNow
    $scriptName = if ($Mode -eq 'Apply') { 'apply.py' } else { 'verify.py' }
    & (Join-Path $newsRewardsProjectRoot 'Tools/ui/blender/bright_globe/run_editor.ps1') `
        -Script (Join-Path $PSScriptRoot $scriptName) -Label ('news_rewards_' + $Mode.ToLower()) -NullRHI
}
finally {
    [Environment]::SetEnvironmentVariable('FF_NEWS_REWARDS_TEST', $newsRewardsPreviousTest)
    # 프로세스 종료 보고와 OS 프로세스 목록 정리 사이의 짧은 차이를 기다린다.
    for ($attempt = 0; $attempt -lt 20; $attempt++) {
        if (-not (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue)) { break }
        Start-Sleep -Milliseconds 500
    }
    if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
        throw ('에디터가 남아 복원을 보류합니다. 백업: ' + $newsRewardsBackup)
    }
    # 테스트가 새로 만든 개인 파일은 삭제하지 않고 검증 폴더에 보관한다.
    $newFiles = @()
    foreach ($entry in $newsRewardsManifest) {
        $destination = Join-Path $newsRewardsProjectRoot $entry.relative
        if ($entry.existed) {
            Copy-Item -LiteralPath (Join-Path $newsRewardsBackup $entry.relative) -Destination $destination
            if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $entry.sha256) {
                throw ('개인 파일 복원 해시 불일치: ' + $entry.relative)
            }
        } elseif (Test-Path -LiteralPath $destination) { $newFiles += Get-Item -LiteralPath $destination }
    }
    $newsRewardsSaveFolder = Join-Path $newsRewardsProjectRoot 'Saved/SaveGames'
    if (Test-Path -LiteralPath $newsRewardsSaveFolder) {
        $newFiles += @(Get-ChildItem -LiteralPath $newsRewardsSaveFolder -File | Where-Object {$_.Name -notin $newsRewardsGroups[1].names})
    }
    foreach ($file in $newFiles) {
        $sourcePath = [IO.Path]::GetFullPath($file.FullName)
        $allowedFolder = [IO.Path]::GetFullPath((Join-Path $newsRewardsProjectRoot 'Saved')) + '\'
        if (-not $sourcePath.StartsWith($allowedFolder, [StringComparison]::OrdinalIgnoreCase)) {
            throw '프로젝트 Saved 밖의 파일 이동을 차단했습니다.'
        }
        $relative = $sourcePath.Substring($allowedFolder.Length)
        $destination = [IO.Path]::GetFullPath((Join-Path (Join-Path $newsRewardsBackup 'CreatedDuringRun') $relative))
        if (-not $destination.StartsWith($newsRewardsBackup + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw '백업 폴더 밖의 파일 이동을 차단했습니다.'
        }
        New-Item -ItemType Directory -Force -Path (Split-Path $destination) | Out-Null
        Move-Item -LiteralPath $sourcePath -Destination $destination
    }
    @{restored=$true; mode=$Mode; backup=$newsRewardsBackup; files=$newsRewardsManifest} | ConvertTo-Json -Depth 6 |
        Set-Content -LiteralPath (Join-Path $newsRewardsOutput ($Mode.ToLower() + '-session-restored.json')) -Encoding utf8
}
$newsRewardsReportFile = Join-Path $newsRewardsOutput ($Mode.ToLower() + '-report.json')
if (-not (Test-Path -LiteralPath $newsRewardsReportFile) -or
    (Get-Item -LiteralPath $newsRewardsReportFile).LastWriteTimeUtc -lt $newsRewardsStarted) {
    throw '이번 실행의 SNS 공감·사건 보상 보고서가 생성되지 않았습니다.'
}
$newsRewardsReport = Get-Content -LiteralPath $newsRewardsReportFile -Raw | ConvertFrom-Json
if ($newsRewardsReport.status -ne 'passed') { throw ('SNS 공감·사건 보상 ' + $Mode + ' 실패. Saved/NewsRewards 보고서를 확인하세요.') }
Write-Output ('NEWS_REWARDS_' + $Mode.ToUpper() + '_PASSED')
