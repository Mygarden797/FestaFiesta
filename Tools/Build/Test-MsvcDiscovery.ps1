# Exercises toolchain preflight without starting Visual Studio or Unreal.
[CmdletBinding()]
param([string] $OutputRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'FestaFiesta.Build.psm1') -Force

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path ([System.IO.Path]::GetTempPath()) (
        'FF-MsvcDiscovery-{0}' -f [Guid]::NewGuid().ToString('N'))
}
$OutputRoot = [System.IO.Path]::GetFullPath($OutputRoot)
if (Test-Path -LiteralPath $OutputRoot) { throw "OutputRoot already exists: $OutputRoot" }
[System.IO.Directory]::CreateDirectory($OutputRoot) | Out-Null
$encoding = New-Object System.Text.UTF8Encoding($false)
$checks = New-Object System.Collections.Generic.List[string]
$version = '14.44.35207'
$missingLocator = Join-Path $OutputRoot 'absent-vswhere.exe'
$missingStandardRoot = Join-Path $OutputRoot 'absent-standard-root'

function Assert-Test {
    param([bool] $Condition, [string] $Message)
    if (-not $Condition) { throw $Message }
    $checks.Add($Message)
}

function New-ToolchainFixture {
    param([string] $Root, [string] $Version, [string[]] $Tools = @('cl.exe', 'link.exe'))
    $toolchainRoot = Join-Path $Root "VC\Tools\MSVC\$Version"
    $binaryRoot = Join-Path $toolchainRoot 'bin\Hostx64\x64'
    [System.IO.Directory]::CreateDirectory($binaryRoot) | Out-Null
    foreach ($tool in $Tools) {
        [System.IO.File]::WriteAllText((Join-Path $binaryRoot $tool), 'fixture, never execute', $encoding)
    }
    return $toolchainRoot
}

function New-LocatorFixture {
    param([string] $Name, [AllowEmptyCollection()][string[]] $Roots, [int] $ExitCode = 0)
    $locator = Join-Path $OutputRoot "$Name.ps1"
    $argumentsFile = Join-Path $OutputRoot "$Name-arguments.txt"
    $lines = @(
        "[System.IO.File]::WriteAllLines('$($argumentsFile.Replace("'", "''"))', [string[]]`$args)"
        foreach ($root in $Roots) { "Write-Output '$($root.Replace("'", "''"))'" }
        "`$global:LASTEXITCODE = $ExitCode"
    )
    [System.IO.File]::WriteAllLines($locator, [string[]]$lines, $encoding)
    return $locator
}

function Assert-DiscoveryRejected {
    param([hashtable] $Arguments, [string] $Message)
    $rejected = $false
    try { $null = Resolve-FFMsvcInstallation @Arguments }
    catch { $rejected = $true }
    Assert-Test -Condition $rejected -Message $Message
}

$customRoot = Join-Path $OutputRoot 'Custom Build Tools'
$expected = New-ToolchainFixture -Root $customRoot -Version $version
$customLocator = New-LocatorFixture -Name 'custom-vswhere' -Roots @($customRoot)
$actual = Resolve-FFMsvcInstallation -InstallVersion $version -VsWherePath $customLocator `
    -VisualStudio2022Root $missingStandardRoot
Assert-Test -Condition ($actual -ceq $expected) -Message 'Custom-location Build Tools resolves the exact locked MSVC install.'
$locatorArguments = @([System.IO.File]::ReadAllLines((Join-Path $OutputRoot 'custom-vswhere-arguments.txt')))
Assert-Test -Condition (($locatorArguments -join '|') -ceq (
    '-products|*|-version|[17.0,18.0)|-property|installationPath')) `
    -Message 'Discovery includes Build Tools and all complete VS 2022 instances without requiring the latest VC component.'

$otherRoot = Join-Path $OutputRoot 'Newer installation'
$null = New-ToolchainFixture -Root $otherRoot -Version '14.45.00000'
$multipleLocator = New-LocatorFixture -Name 'multiple-vswhere' -Roots @($otherRoot, $customRoot)
$actual = Resolve-FFMsvcInstallation -InstallVersion $version -VsWherePath $multipleLocator `
    -VisualStudio2022Root $missingStandardRoot
Assert-Test -Condition ($actual -ceq $expected) -Message 'A newer installation does not hide another instance containing the locked compiler.'

$standardRoot = Join-Path $OutputRoot 'Standard VS 2022'
$standardExpected = New-ToolchainFixture -Root (Join-Path $standardRoot 'Community') -Version $version
$actual = Resolve-FFMsvcInstallation -InstallVersion $version -VsWherePath $missingLocator `
    -VisualStudio2022Root $standardRoot
Assert-Test -Condition ($actual -ceq $standardExpected) -Message 'Standard-location discovery works when vswhere is absent.'
$emptyLocator = New-LocatorFixture -Name 'empty-vswhere' -Roots @()
$actual = Resolve-FFMsvcInstallation -InstallVersion $version -VsWherePath $emptyLocator `
    -VisualStudio2022Root $standardRoot
Assert-Test -Condition ($actual -ceq $standardExpected) -Message 'Standard-location discovery also works when vswhere returns no instances.'

$wrongVersionLocator = New-LocatorFixture -Name 'wrong-version-vswhere' -Roots @($otherRoot)
Assert-DiscoveryRejected -Arguments @{
    InstallVersion = $version; VsWherePath = $wrongVersionLocator; VisualStudio2022Root = $missingStandardRoot
} -Message 'An installed but unlocked MSVC version cannot satisfy preflight.'

foreach ($missingTool in @('cl.exe', 'link.exe')) {
    $incompleteRoot = Join-Path $OutputRoot "Without-$missingTool"
    $presentTool = if ($missingTool -eq 'cl.exe') { 'link.exe' } else { 'cl.exe' }
    $null = New-ToolchainFixture -Root $incompleteRoot -Version $version -Tools @($presentTool)
    $incompleteLocator = New-LocatorFixture -Name "incomplete-$missingTool-vswhere" -Roots @($incompleteRoot)
    Assert-DiscoveryRejected -Arguments @{
        InstallVersion = $version; VsWherePath = $incompleteLocator; VisualStudio2022Root = $missingStandardRoot
    } -Message "An exact-version directory missing Win64 $missingTool is rejected."
}

$failedLocator = New-LocatorFixture -Name 'failed-vswhere' -Roots @($customRoot) -ExitCode 7
Assert-DiscoveryRejected -Arguments @{
    InstallVersion = $version; VsWherePath = $failedLocator; VisualStudio2022Root = $standardRoot
} -Message 'A failed locator does not turn partial output into successful discovery.'
Assert-DiscoveryRejected -Arguments @{
    InstallVersion = $version; VsWherePath = $missingLocator; VisualStudio2022Root = $missingStandardRoot
} -Message 'A machine without discovered installations fails preflight.'
Assert-DiscoveryRejected -Arguments @{
    InstallVersion = '..\unlocked'; VsWherePath = $customLocator; VisualStudio2022Root = $missingStandardRoot
} -Message 'InstallVersion cannot escape the locked version directory.'

$summary = [ordered]@{
    schemaVersion = 1
    status = 'passed'
    checkedAtUtc = [DateTime]::UtcNow.ToString('o')
    moduleSha256 = (Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'FestaFiesta.Build.psm1') -Algorithm SHA256).Hash.ToLowerInvariant()
    checkCount = $checks.Count
    checks = @($checks)
    fixtures = $OutputRoot
}
$summaryPath = Join-Path $OutputRoot 'summary.json'
[System.IO.File]::WriteAllText($summaryPath, ($summary | ConvertTo-Json -Depth 4) + [Environment]::NewLine, $encoding)
Write-Output $summaryPath
