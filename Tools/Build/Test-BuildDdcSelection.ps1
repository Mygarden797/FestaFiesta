# Exercise the production DDC evidence gate without starting Unreal or touching cache entries.
[CmdletBinding()]
param([string] $OutputRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'FestaFiesta.Build.psm1') -Force

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path ([System.IO.Path]::GetTempPath()) (
        'FF-BuildDdcSelection-{0}' -f [Guid]::NewGuid().ToString('N'))
}
$OutputRoot = [System.IO.Path]::GetFullPath($OutputRoot)
if (Test-Path -LiteralPath $OutputRoot) { throw "OutputRoot already exists: $OutputRoot" }
[System.IO.Directory]::CreateDirectory($OutputRoot) | Out-Null
$encoding = New-Object System.Text.UTF8Encoding($false)

# Preserve the actual UE 5.7.4 log shape, including its engine-relative path.
# An explicit -ddc command line alone is insufficient: UE can fall back to its
# default graph after accepting the argument. A generic Zen process startup is
# also not proof that the DDC hierarchy uses the Zen HTTP transport.
$expectedPath = '../../../../../FFProj/DerivedDataCache/BuildFileSystem'
$writableLine = "LogDerivedDataCache: FFBuildLocal: Using data cache path ${expectedPath}: Writable"
$cases = @(
    @{
        name = 'actual-relative-writable-path'
        text = $writableLine
        error = ''
    },
    @{
        name = 'unrelated-zen-startup-is-not-an-active-cache'
        text = $writableLine + "`nLogZenServiceInstance: Zen server is ready" +
            "`nLogDerivedDataCache: ZenShared: Disabled because no host is configured."
        error = ''
    },
    @{
        name = 'fallback-cannot-pass-despite-a-writable-line'
        text = $writableLine + "`nLogDerivedDataCache: Warning: Unable to create or use cache graph 'FestaFiestaBuildDDC'. Reverting to the default graph."
        error = 'fell back or activated a Zen HTTP cache'
    },
    @{
        name = 'active-zen-cache-cannot-pass-despite-a-writable-line'
        text = $writableLine + "`nLogDerivedDataCache: ZenLocal: Using ZenServer HTTP service at http://[::1]:8558 with namespace ue.ddc status: OK."
        error = 'fell back or activated a Zen HTTP cache'
    },
    @{
        name = 'requested-graph-without-selection-evidence'
        text = 'LogInit: Command Line: -ddc=FestaFiestaBuildDDC -unattended'
        error = 'did not report its writable filesystem cache'
    },
    @{
        name = 'read-only-cache-cannot-satisfy-the-build'
        text = $writableLine.Replace(': Writable', ': ReadOnly')
        error = 'did not report its writable filesystem cache'
    },
    @{
        name = 'unexpected-path-cannot-satisfy-the-build'
        text = $writableLine.Replace('DerivedDataCache/BuildFileSystem', 'OtherCache')
        error = 'selected an unexpected filesystem path'
    }
)

$results = @()
foreach ($case in $cases) {
    $logPath = Join-Path $OutputRoot ($case.name + '.log')
    [System.IO.File]::WriteAllText($logPath, $case.text + [Environment]::NewLine, $encoding)
    $failure = ''
    $selectedPath = $null
    try { $selectedPath = Assert-FFBuildDdcSelection -LogPath $logPath }
    catch { $failure = $_.Exception.Message }

    if ($case.error -eq '') {
        if ($failure -ne '' -or $selectedPath -cne $expectedPath) {
            throw "$($case.name) should preserve the observed writable path; actual path='$selectedPath', error='$failure'"
        }
    }
    elseif ($failure -notlike "*$($case.error)*") {
        throw "$($case.name) should fail with '$($case.error)'; actual error: $failure"
    }
    $results += [ordered]@{
        name = $case.name
        passed = $true
        selectedPath = $selectedPath
        observedError = $failure
    }
}

$summary = [ordered]@{
    schemaVersion = 1
    status = 'passed'
    checkedAtUtc = [DateTime]::UtcNow.ToString('o')
    powerShellVersion = $PSVersionTable.PSVersion.ToString()
    moduleSha256 = (Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'FestaFiesta.Build.psm1') -Algorithm SHA256).Hash.ToLowerInvariant()
    checkCount = $results.Count
    checks = $results
    fixtures = $OutputRoot
}
$summaryPath = Join-Path $OutputRoot 'summary.json'
[System.IO.File]::WriteAllText($summaryPath, ($summary | ConvertTo-Json -Depth 5) + [Environment]::NewLine, $encoding)
Write-Output $summaryPath
