# Exercise the production presentation-file gate without launching Unreal or a game.
[CmdletBinding()]
param([string] $OutputRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$tokens = $null
$parseErrors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile(
    (Join-Path $PSScriptRoot 'Verify-Package.ps1'), [ref]$tokens, [ref]$parseErrors)
if ($parseErrors.Count -gt 0) { throw 'Verify-Package.ps1 has syntax errors.' }
foreach ($name in @('Assert-Condition', 'Assert-PresentationRuntimeFiles')) {
    $functionAst = $ast.Find({
        param($node)
        $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name
    }, $true)
    if ($null -eq $functionAst) { throw "Production helper is missing: $name" }
    . ([scriptblock]::Create($functionAst.Extent.Text))
}

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    # The nested CEF fixtures exceed Windows PowerShell 5.1's path limit when
    # placed under a long checkout. Explicit output locations still take priority.
    $OutputRoot = Join-Path ([System.IO.Path]::GetTempPath()) (
        'FF-Presentation-{0}' -f [Guid]::NewGuid().ToString('N'))
}
$OutputRoot = [System.IO.Path]::GetFullPath($OutputRoot)
if (Test-Path -LiteralPath $OutputRoot) { throw "OutputRoot already exists: $OutputRoot" }
[System.IO.Directory]::CreateDirectory($OutputRoot) | Out-Null

$cefPrefix = 'Engine/Binaries/ThirdParty/CEF3/Win64/fixture-version'
$movie = 'FestaFiesta/Content/Movies/FF_Intro_SpaceFireworks.mp4'
$helper = 'Engine/Binaries/Win64/EpicWebHelper.exe'
# Mirrors the installed UE 5.7.4 runtime layout, with tiny stand-in file bytes.
# We test missing/truncated staging output, not binary execution or decoding.
$fixturePaths = @($movie, $helper) + @(
    'libcef.dll', 'chrome_elf.dll', 'd3dcompiler_47.dll', 'libEGL.dll', 'libGLESv2.dll',
    'dxcompiler.dll', 'dxil.dll', 'vk_swiftshader.dll', 'vulkan-1.dll', 'icudtl.dat',
    'chrome_100_percent.pak', 'chrome_200_percent.pak', 'resources.pak',
    'snapshot_blob.bin', 'v8_context_snapshot.bin',
    'Resources/locales/en-US.pak', 'Resources/locales/ko.pak'
) | ForEach-Object {
    if ($_ -eq $movie -or $_ -eq $helper) { $_ } else { "$cefPrefix/$_" }
}
$cases = @(
    @{ name = 'complete'; missing = ''; empty = ''; error = '' },
    @{ name = 'missing-movie'; missing = $movie; empty = ''; error = 'missing: FestaFiesta/Content/Movies/' },
    @{ name = 'empty-movie'; missing = ''; empty = $movie; error = 'empty: FestaFiesta/Content/Movies/' },
    @{ name = 'missing-browser-helper'; missing = $helper; empty = ''; error = 'missing: Engine/Binaries/Win64/EpicWebHelper.exe' },
    @{ name = 'missing-cef-library'; missing = "$cefPrefix/libcef.dll"; empty = ''; error = 'found 0 libcef.dll' },
    @{ name = 'empty-cef-library'; missing = ''; empty = "$cefPrefix/libcef.dll"; error = 'empty:' },
    @{ name = 'missing-cef-resources'; missing = "$cefPrefix/resources.pak"; empty = ''; error = 'missing:' },
    @{ name = 'missing-korean-locale'; missing = "$cefPrefix/Resources/locales/ko.pak"; empty = ''; error = 'Resources/locales/ko.pak' },
    @{ name = 'split-cef-companion'; missing = "$cefPrefix/icudtl.dat"; empty = ''; error = 'missing:' },
    @{ name = 'duplicate-cef-version'; missing = ''; empty = ''; error = 'found 2 libcef.dll' },
    @{ name = 'version-directory-changed'; missing = ''; empty = ''; error = '' }
)
$results = @()
foreach ($case in $cases) {
    $caseRoot = Join-Path $OutputRoot $case.name
    foreach ($relativePath in $fixturePaths) {
        if ($relativePath -eq $case.missing) { continue }
        $destination = $relativePath
        if ($case.name -eq 'version-directory-changed') {
            $destination = $destination.Replace('/fixture-version/', '/different-version/')
        }
        $path = Join-Path $caseRoot $destination
        [System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($path)) | Out-Null
        [byte[]]$bytes = @(1)
        if ($relativePath -eq $case.empty) { $bytes = [byte[]]@() }
        [System.IO.File]::WriteAllBytes($path, $bytes)
    }
    if ($case.name -eq 'split-cef-companion' -or $case.name -eq 'duplicate-cef-version') {
        $misplacedFile = if ($case.name -eq 'split-cef-companion') { 'icudtl.dat' } else { 'libcef.dll' }
        $otherPath = Join-Path $caseRoot "Engine/Binaries/ThirdParty/CEF3/Win64/other-version/$misplacedFile"
        [System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($otherPath)) | Out-Null
        [System.IO.File]::WriteAllBytes($otherPath, [byte[]]@(1))
    }
    $failure = ''
    $files = @()
    try { $files = @(Assert-PresentationRuntimeFiles -Root $caseRoot) }
    catch { $failure = $_.Exception.Message }
    if ($case.error -eq '') {
        if ($failure -ne '' -or $files.Count -ne 19) {
            throw "$($case.name) should pass with 19 recorded files; actual error: $failure"
        }
    }
    elseif ($failure -notlike "*$($case.error)*") {
        throw "$($case.name) should fail with '$($case.error)'; actual error: $failure"
    }
    $results += [ordered]@{ name = $case.name; passed = $true; observedError = $failure }
}
$report = [ordered]@{ passed = $true; caseCount = $results.Count; cases = $results }
$reportPath = Join-Path $OutputRoot 'results.json'
[System.IO.File]::WriteAllText($reportPath, ($report | ConvertTo-Json -Depth 5),
    (New-Object System.Text.UTF8Encoding($false)))
Write-Host "Presentation runtime gate: $($results.Count) cases passed. $reportPath"
