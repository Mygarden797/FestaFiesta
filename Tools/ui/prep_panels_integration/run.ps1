param([switch]$ActionsOnly)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
    throw '열려 있는 에디터가 있습니다. 저장 후 닫고 실행하세요.'
}
$output = Join-Path $projectRoot 'Saved/PrepPanelsIntegration'
$backupRoot = Join-Path $output ('SessionBefore-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$groups = @(
    @{relative='Saved/Config/WindowsEditor'; names=@('EditorPerProjectUserSettings.ini','GameUserSettings.ini')},
    @{relative='Saved/SaveGames'; names=@()}
)
$manifest = @()
foreach ($group in $groups) {
    $sourceFolder = Join-Path $projectRoot $group.relative
    if ($group.relative -eq 'Saved/SaveGames' -and (Test-Path -LiteralPath $sourceFolder)) {
        $group.names = @(Get-ChildItem -LiteralPath $sourceFolder -File | ForEach-Object Name)
    }
    foreach ($name in $group.names) {
        $relative = $group.relative + '/' + $name
        $source = Join-Path $projectRoot $relative
        if (Test-Path -LiteralPath $source) {
            $target = Join-Path $backupRoot $relative
            New-Item -ItemType Directory -Force -Path (Split-Path $target) | Out-Null
            Copy-Item -LiteralPath $source -Destination $target
            $manifest += [pscustomobject]@{relative=$relative; sha256=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash}
        }
    }
}
New-Item -ItemType Directory -Force -Path $backupRoot | Out-Null
$manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $backupRoot 'manifest.json') -Encoding utf8
$previousMode = [Environment]::GetEnvironmentVariable('FF_PREP_INTEGRATION_ACTIONS_ONLY', 'Process')
try {
    [Environment]::SetEnvironmentVariable('FF_PREP_INTEGRATION_ACTIONS_ONLY', $(if($ActionsOnly){'1'}else{'0'}), 'Process')
    & (Join-Path $projectRoot 'Tools/ui/blender/bright_globe/run_editor.ps1') `
        -Script (Join-Path $PSScriptRoot 'verify.py') -Label 'prep_panels_integration'
}
finally {
    [Environment]::SetEnvironmentVariable('FF_PREP_INTEGRATION_ACTIONS_ONLY', $previousMode, 'Process')
    if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
        throw ('에디터가 남아 있어 설정 복원을 보류합니다. 백업: ' + $backupRoot)
    }
    # 새 테스트 저장은 삭제하지 않고 검증 폴더로 옮긴다. 기존 저장은 바이트 단위로 복원한다.
    $saveFolder = Join-Path $projectRoot 'Saved/SaveGames'
    $originalSaves = @($groups[1].names)
    if (Test-Path -LiteralPath $saveFolder) {
        foreach ($file in Get-ChildItem -LiteralPath $saveFolder -File) {
            if ($file.Name -notin $originalSaves) {
                $sourcePath = [IO.Path]::GetFullPath($file.FullName)
                $destinationFolder = Join-Path $backupRoot 'CreatedDuringTest'
                if (-not $sourcePath.StartsWith([IO.Path]::GetFullPath($saveFolder) + '\', [StringComparison]::OrdinalIgnoreCase)) {
                    throw '검증 저장 폴더 밖의 파일 이동을 차단했습니다.'
                }
                New-Item -ItemType Directory -Force -Path $destinationFolder | Out-Null
                Move-Item -LiteralPath $sourcePath -Destination (Join-Path $destinationFolder $file.Name)
            }
        }
    }
    foreach ($item in $manifest) {
        $destination = Join-Path $projectRoot $item.relative
        Copy-Item -LiteralPath (Join-Path $backupRoot $item.relative) -Destination $destination
        if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $item.sha256) {
            throw ('복원 해시 불일치: ' + $item.relative)
        }
    }
    @{restored=$true; backup=$backupRoot; files=$manifest} | ConvertTo-Json -Depth 5 |
        Set-Content -LiteralPath (Join-Path $output 'session-restored.json') -Encoding utf8
}
$report = Get-Content -LiteralPath (Join-Path $output 'report.json') -Raw | ConvertFrom-Json
if ($report.status -ne 'passed') { throw '통합 확인 실패. Saved/PrepPanelsIntegration/report.json을 확인하세요.' }
Write-Output 'PREP_PANELS_INTEGRATION_PASSED'
