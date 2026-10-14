param()
$ErrorActionPreference = 'Stop'

$assistantProject = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$assistantProjectFile = Join-Path $assistantProject 'FestaFiesta.uproject'
if (-not (Test-Path -LiteralPath $assistantProjectFile)) { throw ('프로젝트 파일 없음: ' + $assistantProjectFile) }
if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
    throw '열린 에디터가 있어 결산 비서 카드 적용을 시작하지 않습니다. 저장 후 종료하세요.'
}
$assistantOutput = Join-Path $assistantProject 'Saved/ReportAssistantFocus'
$assistantRun = Join-Path $assistantOutput ('Run-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + [Guid]::NewGuid().ToString('N'))
$assistantBackup = Join-Path $assistantRun 'SessionBefore'
New-Item -ItemType Directory -Path $assistantBackup | Out-Null

# live_staff_compact runner의 개인 설정/저장 백업·복원 방식을 재사용한다.
# 하위 폴더와 추가 설정 파일도 포함하고 실행 도중 생성된 파일은 백업으로 옮긴다.
$assistantProtectedFolders = @('Saved/Config/WindowsEditor', 'Saved/SaveGames')
$assistantRelativeFiles = @(
    'Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini',
    'Saved/Config/WindowsEditor/GameUserSettings.ini'
)
foreach ($assistantRelativeFolder in $assistantProtectedFolders) {
    $assistantFolder = Join-Path $assistantProject $assistantRelativeFolder
    if (Test-Path -LiteralPath $assistantFolder) {
        $assistantRelativeFiles += @(Get-ChildItem -LiteralPath $assistantFolder -File -Recurse | ForEach-Object {
            $_.FullName.Substring($assistantProject.Length + 1).Replace('\','/')
        })
    }
}
$assistantManifest = @()
foreach ($assistantRelative in $assistantRelativeFiles | Sort-Object -Unique) {
    $assistantSource = Join-Path $assistantProject $assistantRelative
    $assistantEntry = [ordered]@{relative=$assistantRelative; existed=(Test-Path -LiteralPath $assistantSource); sha256=$null}
    if ($assistantEntry.existed) {
        $assistantDestination = Join-Path $assistantBackup $assistantRelative
        New-Item -ItemType Directory -Force -Path (Split-Path $assistantDestination) | Out-Null
        Copy-Item -LiteralPath $assistantSource -Destination $assistantDestination
        $assistantEntry.sha256 = (Get-FileHash -LiteralPath $assistantSource -Algorithm SHA256).Hash
    }
    $assistantManifest += [pscustomobject]$assistantEntry
}
$assistantManifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $assistantBackup 'manifest.json') -Encoding utf8
$assistantStartedUtc = [DateTime]::UtcNow
$assistantExitCode = $null
$assistantRunError = $null
try {
    $assistantArgs = @(
        ('"' + $assistantProjectFile + '"'), '/Engine/Maps/Entry',
        ('-ExecutePythonScript="' + (Join-Path $PSScriptRoot 'apply.py') + '"'),
        ('-UserDir="' + (Join-Path $assistantRun 'User') + '"'),
        '-DisablePlugins=RiderLink', '-unattended', '-nosplash', '-nosound', '-NoSourceControl',
        '-NullRHI', '-ddc=FestaFiestaBuildDDC',
        ('-abslog="' + (Join-Path $assistantRun 'apply.log') + '"')
    )
    Write-Output ('REPORT_ASSISTANT_FOCUS_START output=' + $assistantRun)
    $assistantProcess = Start-Process -FilePath 'C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $assistantArgs -WindowStyle Hidden -PassThru
    Write-Output ('REPORT_ASSISTANT_FOCUS_PID=' + $assistantProcess.Id)
    $null = $assistantProcess.Handle
    $assistantProcess.WaitForExit()
    $assistantExitCode = $assistantProcess.ExitCode
} catch {
    $assistantRunError = $_.Exception.Message
} finally {
    for ($assistantAttempt = 0; $assistantAttempt -lt 20; $assistantAttempt++) {
        if (-not (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue)) { break }
        Start-Sleep -Milliseconds 500
    }
    if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
        throw ('에디터가 남아 개인 파일 복원을 보류합니다. 백업: ' + $assistantBackup)
    }
    $assistantOriginalFiles = @($assistantManifest | Where-Object existed | ForEach-Object relative)
    foreach ($assistantEntry in $assistantManifest | Where-Object existed) {
        $assistantDestination = Join-Path $assistantProject $assistantEntry.relative
        New-Item -ItemType Directory -Force -Path (Split-Path $assistantDestination) | Out-Null
        Copy-Item -LiteralPath (Join-Path $assistantBackup $assistantEntry.relative) -Destination $assistantDestination -Force
        if ((Get-FileHash -LiteralPath $assistantDestination -Algorithm SHA256).Hash -ne $assistantEntry.sha256) {
            throw ('개인 파일 복원 해시 불일치: ' + $assistantEntry.relative)
        }
    }
    $assistantMovedFiles = @()
    foreach ($assistantRelativeFolder in $assistantProtectedFolders) {
        $assistantFolder = Join-Path $assistantProject $assistantRelativeFolder
        if (-not (Test-Path -LiteralPath $assistantFolder)) { continue }
        $assistantAllowedFolder = [IO.Path]::GetFullPath($assistantFolder).TrimEnd('\') + '\'
        foreach ($assistantFile in @(Get-ChildItem -LiteralPath $assistantFolder -File -Recurse)) {
            $assistantSourcePath = [IO.Path]::GetFullPath($assistantFile.FullName)
            $assistantRelative = $assistantSourcePath.Substring($assistantProject.Length + 1).Replace('\','/')
            if ($assistantRelative -in $assistantOriginalFiles) { continue }
            if (-not $assistantSourcePath.StartsWith($assistantAllowedFolder, [StringComparison]::OrdinalIgnoreCase)) {
                throw '보호 대상 폴더 밖의 파일 이동을 차단했습니다.'
            }
            $assistantDestination = [IO.Path]::GetFullPath((Join-Path (Join-Path $assistantBackup 'CreatedDuringRun') $assistantRelative))
            if (-not $assistantDestination.StartsWith($assistantBackup + '\', [StringComparison]::OrdinalIgnoreCase)) {
                throw '백업 폴더 밖의 파일 이동을 차단했습니다.'
            }
            New-Item -ItemType Directory -Force -Path (Split-Path $assistantDestination) | Out-Null
            Move-Item -LiteralPath $assistantSourcePath -Destination $assistantDestination
            $assistantMovedFiles += $assistantRelative
        }
    }
    @{restored=$true; backup=$assistantBackup; files=$assistantManifest; created_during_run=$assistantMovedFiles} |
        ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $assistantRun 'session-restored.json') -Encoding utf8
}

if ($assistantRunError) { throw $assistantRunError }
if ($null -eq $assistantExitCode -or $assistantExitCode -ne 0) { throw ('에디터 종료 코드: ' + $assistantExitCode + '. 로그: ' + $assistantRun) }
# 이번 실행 이후 수정된 결과만 판정하고 실행별 폴더에도 보관한다.
$assistantReportPath = Join-Path $assistantOutput 'apply-report.json'
if (-not (Test-Path -LiteralPath $assistantReportPath) -or (Get-Item -LiteralPath $assistantReportPath).LastWriteTimeUtc -lt $assistantStartedUtc) {
    throw ('새 적용 결과 없음: ' + $assistantRun)
}
$assistantReport = Get-Content -LiteralPath $assistantReportPath -Raw | ConvertFrom-Json
if ($assistantReport.status -ne 'passed' -or -not $assistantReport.protected_unchanged) {
    throw ('결산 비서 카드 적용/보존 검사 실패: ' + $assistantReportPath)
}
Copy-Item -LiteralPath $assistantReportPath -Destination (Join-Path $assistantRun 'apply-report.json')
Write-Output ('REPORT_ASSISTANT_FOCUS_PASSED report=' + $assistantReportPath)
