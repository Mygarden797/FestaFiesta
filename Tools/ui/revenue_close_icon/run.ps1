param()
$ErrorActionPreference = 'Stop'

$closeIconProject = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$closeIconProjectFile = Join-Path $closeIconProject 'FestaFiesta.uproject'
if (-not (Test-Path -LiteralPath $closeIconProjectFile)) { throw ('프로젝트 파일 없음: ' + $closeIconProjectFile) }
if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
    throw '열린 에디터가 있어 매출 순위 닫기 아이콘 적용을 시작하지 않습니다. 저장 후 종료하세요.'
}
$closeIconOutput = Join-Path $closeIconProject 'Saved/RevenueCloseIcon'
$closeIconRun = Join-Path $closeIconOutput ('Run-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + [Guid]::NewGuid().ToString('N'))
$closeIconBackup = Join-Path $closeIconRun 'SessionBefore'
New-Item -ItemType Directory -Path $closeIconBackup | Out-Null

# live_staff_compact runner의 개인 설정/저장 백업·복원 방식을 재사용한다.
# 하위 폴더와 추가 설정 파일도 포함하고 실행 도중 생성된 파일은 백업으로 옮긴다.
$closeIconProtectedFolders = @('Saved/Config/WindowsEditor', 'Saved/SaveGames')
$closeIconRelativeFiles = @(
    'Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini',
    'Saved/Config/WindowsEditor/GameUserSettings.ini'
)
foreach ($closeIconRelativeFolder in $closeIconProtectedFolders) {
    $closeIconFolder = Join-Path $closeIconProject $closeIconRelativeFolder
    if (Test-Path -LiteralPath $closeIconFolder) {
        $closeIconRelativeFiles += @(Get-ChildItem -LiteralPath $closeIconFolder -File -Recurse | ForEach-Object {
            $_.FullName.Substring($closeIconProject.Length + 1).Replace('\','/')
        })
    }
}
$closeIconManifest = @()
foreach ($closeIconRelative in $closeIconRelativeFiles | Sort-Object -Unique) {
    $closeIconSource = Join-Path $closeIconProject $closeIconRelative
    $closeIconEntry = [ordered]@{relative=$closeIconRelative; existed=(Test-Path -LiteralPath $closeIconSource); sha256=$null}
    if ($closeIconEntry.existed) {
        $closeIconDestination = Join-Path $closeIconBackup $closeIconRelative
        New-Item -ItemType Directory -Force -Path (Split-Path $closeIconDestination) | Out-Null
        Copy-Item -LiteralPath $closeIconSource -Destination $closeIconDestination
        $closeIconEntry.sha256 = (Get-FileHash -LiteralPath $closeIconSource -Algorithm SHA256).Hash
    }
    $closeIconManifest += [pscustomobject]$closeIconEntry
}
$closeIconManifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $closeIconBackup 'manifest.json') -Encoding utf8
$closeIconStartedUtc = [DateTime]::UtcNow
$closeIconExitCode = $null
$closeIconRunError = $null
try {
    $closeIconArgs = @(
        ('"' + $closeIconProjectFile + '"'), '/Engine/Maps/Entry',
        ('-ExecutePythonScript="' + (Join-Path $PSScriptRoot 'apply.py') + '"'),
        ('-UserDir="' + (Join-Path $closeIconRun 'User') + '"'),
        '-DisablePlugins=RiderLink', '-unattended', '-nosplash', '-nosound', '-NoSourceControl',
        '-NullRHI', '-ddc=FestaFiestaBuildDDC',
        ('-abslog="' + (Join-Path $closeIconRun 'apply.log') + '"')
    )
    Write-Output ('REVENUE_CLOSE_ICON_START output=' + $closeIconRun)
    $closeIconProcess = Start-Process -FilePath 'C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $closeIconArgs -WindowStyle Hidden -PassThru
    Write-Output ('REVENUE_CLOSE_ICON_PID=' + $closeIconProcess.Id)
    $null = $closeIconProcess.Handle
    $closeIconProcess.WaitForExit()
    $closeIconExitCode = $closeIconProcess.ExitCode
} catch {
    $closeIconRunError = $_.Exception.Message
} finally {
    for ($closeIconAttempt = 0; $closeIconAttempt -lt 20; $closeIconAttempt++) {
        if (-not (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue)) { break }
        Start-Sleep -Milliseconds 500
    }
    if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
        throw ('에디터가 남아 개인 파일 복원을 보류합니다. 백업: ' + $closeIconBackup)
    }
    $closeIconOriginalFiles = @($closeIconManifest | Where-Object existed | ForEach-Object relative)
    foreach ($closeIconEntry in $closeIconManifest | Where-Object existed) {
        $closeIconDestination = Join-Path $closeIconProject $closeIconEntry.relative
        New-Item -ItemType Directory -Force -Path (Split-Path $closeIconDestination) | Out-Null
        Copy-Item -LiteralPath (Join-Path $closeIconBackup $closeIconEntry.relative) -Destination $closeIconDestination -Force
        if ((Get-FileHash -LiteralPath $closeIconDestination -Algorithm SHA256).Hash -ne $closeIconEntry.sha256) {
            throw ('개인 파일 복원 해시 불일치: ' + $closeIconEntry.relative)
        }
    }
    $closeIconMovedFiles = @()
    foreach ($closeIconRelativeFolder in $closeIconProtectedFolders) {
        $closeIconFolder = Join-Path $closeIconProject $closeIconRelativeFolder
        if (-not (Test-Path -LiteralPath $closeIconFolder)) { continue }
        $closeIconAllowedFolder = [IO.Path]::GetFullPath($closeIconFolder).TrimEnd('\') + '\'
        foreach ($closeIconFile in @(Get-ChildItem -LiteralPath $closeIconFolder -File -Recurse)) {
            $closeIconSourcePath = [IO.Path]::GetFullPath($closeIconFile.FullName)
            $closeIconRelative = $closeIconSourcePath.Substring($closeIconProject.Length + 1).Replace('\','/')
            if ($closeIconRelative -in $closeIconOriginalFiles) { continue }
            if (-not $closeIconSourcePath.StartsWith($closeIconAllowedFolder, [StringComparison]::OrdinalIgnoreCase)) {
                throw '보호 대상 폴더 밖의 파일 이동을 차단했습니다.'
            }
            $closeIconDestination = [IO.Path]::GetFullPath((Join-Path (Join-Path $closeIconBackup 'CreatedDuringRun') $closeIconRelative))
            if (-not $closeIconDestination.StartsWith($closeIconBackup + '\', [StringComparison]::OrdinalIgnoreCase)) {
                throw '백업 폴더 밖의 파일 이동을 차단했습니다.'
            }
            New-Item -ItemType Directory -Force -Path (Split-Path $closeIconDestination) | Out-Null
            Move-Item -LiteralPath $closeIconSourcePath -Destination $closeIconDestination
            $closeIconMovedFiles += $closeIconRelative
        }
    }
    @{restored=$true; backup=$closeIconBackup; files=$closeIconManifest; created_during_run=$closeIconMovedFiles} |
        ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $closeIconRun 'session-restored.json') -Encoding utf8
}

if ($closeIconRunError) { throw $closeIconRunError }
if ($null -eq $closeIconExitCode -or $closeIconExitCode -ne 0) { throw ('에디터 종료 코드: ' + $closeIconExitCode + '. 로그: ' + $closeIconRun) }
# 이번 실행 이후 수정된 결과만 판정하고 실행별 폴더에도 보관한다.
$closeIconReportPath = Join-Path $closeIconOutput 'apply-report.json'
if (-not (Test-Path -LiteralPath $closeIconReportPath) -or (Get-Item -LiteralPath $closeIconReportPath).LastWriteTimeUtc -lt $closeIconStartedUtc) {
    throw ('새 적용 결과 없음: ' + $closeIconRun)
}
$closeIconReport = Get-Content -LiteralPath $closeIconReportPath -Raw | ConvertFrom-Json
if ($closeIconReport.status -ne 'passed' -or -not $closeIconReport.protected_unchanged) {
    throw ('매출 순위 닫기 아이콘 적용/보존 검사 실패: ' + $closeIconReportPath)
}
Copy-Item -LiteralPath $closeIconReportPath -Destination (Join-Path $closeIconRun 'apply-report.json')
Write-Output ('REVENUE_CLOSE_ICON_PASSED report=' + $closeIconReportPath)
