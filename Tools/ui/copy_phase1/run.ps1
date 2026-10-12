param([ValidateSet('Apply')][string]$Mode = 'Apply')
$ErrorActionPreference = 'Stop'
$copyPhase1ProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
    throw '열린 에디터가 있어 일회용 실행을 시작하지 않습니다.'
}
$copyPhase1Output = Join-Path $copyPhase1ProjectRoot 'Saved/UiCopyPhase1'
$copyPhase1Backup = Join-Path $copyPhase1Output ('SessionBefore-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$copyPhase1Groups = @(
    @{relative='Saved/Config/WindowsEditor'; names=@('EditorPerProjectUserSettings.ini','GameUserSettings.ini')},
    @{relative='Saved/SaveGames'; names=@()}
)
$copyPhase1Manifest = @()
foreach ($group in $copyPhase1Groups) {
    $folder = Join-Path $copyPhase1ProjectRoot $group.relative
    if ($group.relative -eq 'Saved/SaveGames' -and (Test-Path -LiteralPath $folder)) {
        $group.names = @(Get-ChildItem -LiteralPath $folder -File | ForEach-Object Name)
    }
    foreach ($name in $group.names) {
        $relative = $group.relative + '/' + $name
        $source = Join-Path $copyPhase1ProjectRoot $relative
        $exists = Test-Path -LiteralPath $source
        $entry = [ordered]@{relative=$relative; existed=$exists; sha256=$null}
        if ($exists) {
            $destination = Join-Path $copyPhase1Backup $relative
            New-Item -ItemType Directory -Force -Path (Split-Path $destination) | Out-Null
            Copy-Item -LiteralPath $source -Destination $destination
            $entry.sha256 = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
        }
        $copyPhase1Manifest += [pscustomobject]$entry
    }
}
New-Item -ItemType Directory -Force -Path $copyPhase1Backup | Out-Null
$copyPhase1Manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $copyPhase1Backup 'manifest.json') -Encoding utf8
try {
    $scriptName = 'apply.py'
    & (Join-Path $copyPhase1ProjectRoot 'Tools/ui/blender/bright_globe/run_editor.ps1') `
        -Script (Join-Path $PSScriptRoot $scriptName) -Label ('copy_phase1_' + $Mode.ToLower()) -NullRHI
}
finally {
    # 프로세스 종료 보고와 OS 프로세스 목록 정리 사이의 짧은 차이를 기다린다.
    for ($attempt = 0; $attempt -lt 20; $attempt++) {
        if (-not (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue)) { break }
        Start-Sleep -Milliseconds 500
    }
    if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
        throw ('에디터가 남아 복원을 보류합니다. 백업: ' + $copyPhase1Backup)
    }
    # 테스트가 새로 만든 개인 파일은 삭제하지 않고 검증 폴더에 보관한다.
    $newFiles = @()
    foreach ($entry in $copyPhase1Manifest) {
        $destination = Join-Path $copyPhase1ProjectRoot $entry.relative
        if ($entry.existed) {
            Copy-Item -LiteralPath (Join-Path $copyPhase1Backup $entry.relative) -Destination $destination
            if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $entry.sha256) {
                throw ('개인 파일 복원 해시 불일치: ' + $entry.relative)
            }
        } elseif (Test-Path -LiteralPath $destination) { $newFiles += Get-Item -LiteralPath $destination }
    }
    $copyPhase1SaveFolder = Join-Path $copyPhase1ProjectRoot 'Saved/SaveGames'
    if (Test-Path -LiteralPath $copyPhase1SaveFolder) {
        $newFiles += @(Get-ChildItem -LiteralPath $copyPhase1SaveFolder -File | Where-Object {$_.Name -notin $copyPhase1Groups[1].names})
    }
    foreach ($file in $newFiles) {
        $sourcePath = [IO.Path]::GetFullPath($file.FullName)
        $allowedFolder = [IO.Path]::GetFullPath((Join-Path $copyPhase1ProjectRoot 'Saved')) + '\'
        if (-not $sourcePath.StartsWith($allowedFolder, [StringComparison]::OrdinalIgnoreCase)) {
            throw '프로젝트 Saved 밖의 파일 이동을 차단했습니다.'
        }
        $relative = $sourcePath.Substring($allowedFolder.Length)
        $destination = [IO.Path]::GetFullPath((Join-Path (Join-Path $copyPhase1Backup 'CreatedDuringRun') $relative))
        if (-not $destination.StartsWith($copyPhase1Backup + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw '백업 폴더 밖의 파일 이동을 차단했습니다.'
        }
        New-Item -ItemType Directory -Force -Path (Split-Path $destination) | Out-Null
        Move-Item -LiteralPath $sourcePath -Destination $destination
    }
    @{restored=$true; mode=$Mode; backup=$copyPhase1Backup; files=$copyPhase1Manifest} | ConvertTo-Json -Depth 6 |
        Set-Content -LiteralPath (Join-Path $copyPhase1Output ($Mode.ToLower() + '-session-restored.json')) -Encoding utf8
}
$copyPhase1Report = Get-Content -LiteralPath (Join-Path $copyPhase1Output ($Mode.ToLower() + '-report.json')) -Raw | ConvertFrom-Json
if ($copyPhase1Report.status -ne 'passed') { throw ('UI 1단계 문구 수정 ' + $Mode + ' 실패. Saved/UiCopyPhase1 보고서를 확인하세요.') }
Write-Output ('UI_COPY_PHASE1_' + $Mode.ToUpper() + '_PASSED')
