param([ValidateSet('Apply', 'Capture')][string]$Mode = 'Apply', [switch]$Rebuild)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
    throw '열린 에디터가 있어 일회용 실행을 시작하지 않습니다.'
}
# PIE 창 위치·그래픽 설정은 종료 때 개인 설정 파일에 저장된다. 실행 전 상태로 되돌린다.
$personal = @('Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini', 'Saved/Config/WindowsEditor/GameUserSettings.ini')
$backup = Join-Path $root ('Saved/TitleIntro/SessionBefore-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force -Path $backup | Out-Null
foreach ($relative in $personal) {
    $source = Join-Path $root $relative
    if (Test-Path -LiteralPath $source) { Copy-Item -LiteralPath $source -Destination (Join-Path $backup (Split-Path $relative -Leaf)) }
}
$previousRebuild = [Environment]::GetEnvironmentVariable('FF_TITLE_INTRO_REBUILD', 'Process')
try {
    # -Rebuild: 🔴 WBP 트리를 코드 골격으로 교체(FF.ApplyTitleIntroDesign). 없으면 없을 때만 만든다.
    [Environment]::SetEnvironmentVariable('FF_TITLE_INTRO_REBUILD', $(if ($Rebuild) {'1'} else {'0'}), 'Process')
    $script = if ($Mode -eq 'Capture') { 'capture.py' } else { 'apply.py' }
    & (Join-Path $root 'Tools/ui/blender/bright_globe/run_editor.ps1') `
        -Script (Join-Path $PSScriptRoot $script) -Label ('title_intro_' + $Mode.ToLower())
}
finally {
    [Environment]::SetEnvironmentVariable('FF_TITLE_INTRO_REBUILD', $previousRebuild, 'Process')
    for ($attempt = 0; $attempt -lt 20; $attempt++) {
        if (-not (Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue)) { break }
        Start-Sleep -Milliseconds 500
    }
    foreach ($relative in $personal) {
        $saved = Join-Path $backup (Split-Path $relative -Leaf)
        if (Test-Path -LiteralPath $saved) { Copy-Item -LiteralPath $saved -Destination (Join-Path $root $relative) -Force }
    }
}
Write-Output ('TITLE_INTRO_' + $Mode.ToUpper() + '_DONE')
