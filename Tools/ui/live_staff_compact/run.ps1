param()
$ErrorActionPreference = 'Stop'
$compactProject = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
    throw '열린 에디터가 있어 라이브 스태프 적용을 시작하지 않습니다. 저장 후 종료하세요.'
}
$compactOutput = Join-Path $compactProject 'Saved/LiveStaffCompact'
$compactRun = Join-Path $compactOutput ('Run-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$compactBackup = Join-Path $compactRun 'SessionBefore'
New-Item -ItemType Directory -Force -Path $compactBackup | Out-Null
$compactGroups = @(
    @{relative='Saved/Config/WindowsEditor'; names=@('EditorPerProjectUserSettings.ini','GameUserSettings.ini')},
    @{relative='Saved/SaveGames'; names=@()}
)
$compactManifest = @()
foreach ($group in $compactGroups) {
    $folder = Join-Path $compactProject $group.relative
    if ($group.relative -eq 'Saved/SaveGames' -and (Test-Path -LiteralPath $folder)) {
        $group.names = @(Get-ChildItem -LiteralPath $folder -File | ForEach-Object Name)
    }
    foreach ($name in $group.names) {
        $relative = $group.relative + '/' + $name
        $source = Join-Path $compactProject $relative
        $entry = [ordered]@{relative=$relative; existed=(Test-Path -LiteralPath $source); sha256=$null}
        if ($entry.existed) {
            $destination = Join-Path $compactBackup $relative
            New-Item -ItemType Directory -Force -Path (Split-Path $destination) | Out-Null
            Copy-Item -LiteralPath $source -Destination $destination
            $entry.sha256 = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
        }
        $compactManifest += [pscustomobject]$entry
    }
}
$compactManifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $compactBackup 'manifest.json') -Encoding utf8
$compactStarted = Get-Date
try {
    $compactArgs = @(
        ('"' + (Join-Path $compactProject 'FestaFiesta.uproject') + '"'), '/Engine/Maps/Entry',
        ('-ExecutePythonScript="' + (Join-Path $PSScriptRoot 'apply.py') + '"'),
        ('-UserDir="' + (Join-Path $compactRun 'User') + '"'),
        '-DisablePlugins=RiderLink', '-unattended', '-nosplash', '-nosound', '-NoSourceControl',
        '-NullRHI', '-ddc=FestaFiestaBuildDDC', ('-abslog="' + (Join-Path $compactRun 'apply.log') + '"'))
    $compactProcess = Start-Process -FilePath 'C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $compactArgs -WindowStyle Hidden -PassThru
    Write-Output ('LIVE_STAFF_COMPACT_STARTED pid=' + $compactProcess.Id + ' output=' + $compactRun)
    $compactProcess.WaitForExit()
    if ($compactProcess.ExitCode -ne 0) { throw ('에디터 실행 실패: ' + $compactRun) }
}
finally {
    for ($attempt = 0; $attempt -lt 20; $attempt++) {
        if (-not (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue)) { break }
        Start-Sleep -Milliseconds 500
    }
    if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
        throw ('에디터가 남아 개인 파일 복원을 보류합니다. 백업: ' + $compactBackup)
    }
    $newFiles = @()
    foreach ($entry in $compactManifest) {
        $destination = Join-Path $compactProject $entry.relative
        if ($entry.existed) {
            Copy-Item -LiteralPath (Join-Path $compactBackup $entry.relative) -Destination $destination
            if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $entry.sha256) {
                throw ('개인 파일 복원 해시 불일치: ' + $entry.relative)
            }
        } elseif (Test-Path -LiteralPath $destination) { $newFiles += Get-Item -LiteralPath $destination }
    }
    $compactSaveFolder = Join-Path $compactProject 'Saved/SaveGames'
    if (Test-Path -LiteralPath $compactSaveFolder) {
        $newFiles += @(Get-ChildItem -LiteralPath $compactSaveFolder -File | Where-Object {$_.Name -notin $compactGroups[1].names})
    }
    foreach ($file in $newFiles | Sort-Object FullName -Unique) {
        $sourcePath = [IO.Path]::GetFullPath($file.FullName)
        $allowedFolder = [IO.Path]::GetFullPath((Join-Path $compactProject 'Saved')) + '\'
        if (-not $sourcePath.StartsWith($allowedFolder, [StringComparison]::OrdinalIgnoreCase)) {
            throw '프로젝트 Saved 밖의 파일 이동을 차단했습니다.'
        }
        $relative = $sourcePath.Substring($allowedFolder.Length)
        $destination = [IO.Path]::GetFullPath((Join-Path (Join-Path $compactBackup 'CreatedDuringRun') $relative))
        if (-not $destination.StartsWith($compactBackup + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw '백업 폴더 밖의 파일 이동을 차단했습니다.'
        }
        New-Item -ItemType Directory -Force -Path (Split-Path $destination) | Out-Null
        Move-Item -LiteralPath $sourcePath -Destination $destination
    }
    @{restored=$true; backup=$compactBackup; files=$compactManifest} | ConvertTo-Json -Depth 6 |
        Set-Content -LiteralPath (Join-Path $compactOutput 'session-restored.json') -Encoding utf8
}
$compactReportPath = Join-Path $compactOutput 'apply-report.json'
if (!(Test-Path -LiteralPath $compactReportPath) -or (Get-Item -LiteralPath $compactReportPath).LastWriteTime -lt $compactStarted) {
    throw ('새 적용 결과 없음: ' + $compactRun)
}
$compactReport = Get-Content -LiteralPath $compactReportPath -Raw | ConvertFrom-Json
if ($compactReport.status -ne 'passed' -or -not $compactReport.protected_unchanged) {
    throw ('라이브 스태프 적용 실패: ' + $compactReportPath)
}
Write-Output ('LIVE_STAFF_COMPACT_PASSED report=' + $compactReportPath)
