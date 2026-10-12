param([ValidateSet('Apply')][string]$Mode = 'Apply')
$ErrorActionPreference = 'Stop'
$reportPhase3ProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
    throw '열린 에디터가 있어 일회용 실행을 시작하지 않습니다.'
}
$reportPhase3Output = Join-Path $reportPhase3ProjectRoot 'Saved/ReportPhase3'
$reportPhase3Backup = Join-Path $reportPhase3Output ('SessionBefore-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$reportPhase3Groups = @(
    @{relative='Saved/Config/WindowsEditor'; names=@('EditorPerProjectUserSettings.ini','GameUserSettings.ini')},
    @{relative='Saved/SaveGames'; names=@()}
)
$reportPhase3Manifest = @()
foreach ($group in $reportPhase3Groups) {
    $folder = Join-Path $reportPhase3ProjectRoot $group.relative
    if ($group.relative -eq 'Saved/SaveGames' -and (Test-Path -LiteralPath $folder)) {
        $group.names = @(Get-ChildItem -LiteralPath $folder -File | ForEach-Object Name)
    }
    foreach ($name in $group.names) {
        $relative = $group.relative + '/' + $name
        $source = Join-Path $reportPhase3ProjectRoot $relative
        $exists = Test-Path -LiteralPath $source
        $entry = [ordered]@{relative=$relative; existed=$exists; sha256=$null}
        if ($exists) {
            $destination = Join-Path $reportPhase3Backup $relative
            New-Item -ItemType Directory -Force -Path (Split-Path $destination) | Out-Null
            Copy-Item -LiteralPath $source -Destination $destination
            $entry.sha256 = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
        }
        $reportPhase3Manifest += [pscustomobject]$entry
    }
}
New-Item -ItemType Directory -Force -Path $reportPhase3Backup | Out-Null
$reportPhase3Manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $reportPhase3Backup 'manifest.json') -Encoding utf8
try {
    $reportPhase3Started = [DateTime]::UtcNow
    $scriptName = 'apply.py'
    & (Join-Path $reportPhase3ProjectRoot 'Tools/ui/blender/bright_globe/run_editor.ps1') `
        -Script (Join-Path $PSScriptRoot $scriptName) -Label ('report_phase3_' + $Mode.ToLower()) -NullRHI
}
finally {
    # 프로세스 종료 보고와 OS 프로세스 목록 정리 사이의 짧은 차이를 기다린다.
    for ($attempt = 0; $attempt -lt 20; $attempt++) {
        if (-not (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue)) { break }
        Start-Sleep -Milliseconds 500
    }
    if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
        throw ('에디터가 남아 복원을 보류합니다. 백업: ' + $reportPhase3Backup)
    }
    # 테스트가 새로 만든 개인 파일은 삭제하지 않고 검증 폴더에 보관한다.
    $newFiles = @()
    foreach ($entry in $reportPhase3Manifest) {
        $destination = Join-Path $reportPhase3ProjectRoot $entry.relative
        if ($entry.existed) {
            Copy-Item -LiteralPath (Join-Path $reportPhase3Backup $entry.relative) -Destination $destination
            if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $entry.sha256) {
                throw ('개인 파일 복원 해시 불일치: ' + $entry.relative)
            }
        } elseif (Test-Path -LiteralPath $destination) { $newFiles += Get-Item -LiteralPath $destination }
    }
    $reportPhase3SaveFolder = Join-Path $reportPhase3ProjectRoot 'Saved/SaveGames'
    if (Test-Path -LiteralPath $reportPhase3SaveFolder) {
        $newFiles += @(Get-ChildItem -LiteralPath $reportPhase3SaveFolder -File | Where-Object {$_.Name -notin $reportPhase3Groups[1].names})
    }
    foreach ($file in $newFiles) {
        $sourcePath = [IO.Path]::GetFullPath($file.FullName)
        $allowedFolder = [IO.Path]::GetFullPath((Join-Path $reportPhase3ProjectRoot 'Saved')) + '\'
        if (-not $sourcePath.StartsWith($allowedFolder, [StringComparison]::OrdinalIgnoreCase)) {
            throw '프로젝트 Saved 밖의 파일 이동을 차단했습니다.'
        }
        $relative = $sourcePath.Substring($allowedFolder.Length)
        $destination = [IO.Path]::GetFullPath((Join-Path (Join-Path $reportPhase3Backup 'CreatedDuringRun') $relative))
        if (-not $destination.StartsWith($reportPhase3Backup + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw '백업 폴더 밖의 파일 이동을 차단했습니다.'
        }
        New-Item -ItemType Directory -Force -Path (Split-Path $destination) | Out-Null
        Move-Item -LiteralPath $sourcePath -Destination $destination
    }
    @{restored=$true; mode=$Mode; backup=$reportPhase3Backup; files=$reportPhase3Manifest} | ConvertTo-Json -Depth 6 |
        Set-Content -LiteralPath (Join-Path $reportPhase3Output ($Mode.ToLower() + '-session-restored.json')) -Encoding utf8
}
$reportPhase3ReportFile = Join-Path $reportPhase3Output ($Mode.ToLower() + '-report.json')
if (-not (Test-Path -LiteralPath $reportPhase3ReportFile) -or
    (Get-Item -LiteralPath $reportPhase3ReportFile).LastWriteTimeUtc -lt $reportPhase3Started) {
    throw '이번 실행의 결산 적용 보고서가 생성되지 않았습니다.'
}
$reportPhase3Report = Get-Content -LiteralPath $reportPhase3ReportFile -Raw | ConvertFrom-Json
if ($reportPhase3Report.status -ne 'passed') { throw ('결산 만족도 열 적용 ' + $Mode + ' 실패. Saved/ReportPhase3 보고서를 확인하세요.') }
Write-Output ('REPORT_PHASE3_' + $Mode.ToUpper() + '_PASSED')
