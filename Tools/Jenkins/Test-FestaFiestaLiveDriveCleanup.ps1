# Offline cleanup fixtures. The upload adapter and CLI are replaced locally;
# no credentials, Jenkins request, or remote file operation is performed.
[CmdletBinding()]
param([string] $FixtureParent = [IO.Path]::GetTempPath())
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$script:Assertions = 0
function Assert-CleanupTest([bool] $Condition, [string] $Message) {
    if (-not $Condition) { throw ('Assertion failed: ' + $Message) }
    $script:Assertions++
}
function Import-Module { param([string] $Name, [switch] $Force) }
function Invoke-FFDuckUploadFile { param($Options, $BuildLabel, $File) }
$liveScript = Join-Path $PSScriptRoot 'Test-FestaFiestaLiveDrive.ps1'
$fixtureRoot = [IO.Path]::GetFullPath((Join-Path $FixtureParent ('ff-live-cleanup-' + [Guid]::NewGuid().ToString('N'))))
$encoding = New-Object Text.UTF8Encoding($false)
try {
    [IO.Directory]::CreateDirectory($fixtureRoot) | Out-Null
    $duckPath = Join-Path $fixtureRoot 'fixture-duck.ps1'
    $fixtureSource = @'
$harness = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'harness.json') -Raw | ConvertFrom-Json
$operationIndex = -1
for ($index = 0; $index -lt $args.Count; $index++) {
    if ($args[$index] -in @('--delete', '--list')) { $operationIndex = $index; break }
}
if ($operationIndex -lt 0) { throw 'Unexpected fixture operation.' }
$operation = [string]$args[$operationIndex]
$uri = [string]$args[$operationIndex + 1]
$line = @{operation=$operation;uri=$uri} | ConvertTo-Json -Compress
[IO.File]::AppendAllText($harness.callsPath, $line + [Environment]::NewLine)
$global:LASTEXITCODE = 0
if ($operation -eq '--list') {
    $global:LASTEXITCODE = [int]$harness.listExitCode
    Write-Output ([string]$harness.listing)
}
'@
    [IO.File]::WriteAllText($duckPath, $fixtureSource, $encoding)
    $escape = [string][char]27
    $cases = @(
        @{name='cursor-only';listing=($escape + '7' + $escape + '[0m' + "`r`n" + $escape + '8');code=0;folderRemoved=$true},
        @{name='listed-file';listing=($escape + '7' + 'keep-this-file.txt' + $escape + '8');code=0;folderRemoved=$false},
        @{name='unknown-output';listing=($escape + ']unknown-control');code=0;folderRemoved=$false},
        @{name='list-error';listing=($escape + '7' + $escape + '8');code=17;folderRemoved=$false}
    )
    foreach ($case in $cases) {
        $caseRoot = Join-Path $fixtureRoot $case.name
        [IO.Directory]::CreateDirectory($caseRoot) | Out-Null
        $configPath = Join-Path $caseRoot 'duck.json'
        $settingsPath = Join-Path $caseRoot 'settings.json'
        $callsPath = Join-Path $caseRoot 'calls.jsonl'
        $configuration = @{username='fixture@example.com';rootUri='googledrive:/My%20Drive/Fixture'}
        $settings = @{UploadProvider='Duck';DuckExe=$duckPath;DuckConfig=$configPath;StateRoot=$caseRoot;DriveFolderId='fixture_folder_12345'}
        [IO.File]::WriteAllText($configPath, ($configuration | ConvertTo-Json), $encoding)
        [IO.File]::WriteAllText($settingsPath, ($settings | ConvertTo-Json), $encoding)
        [IO.File]::WriteAllText((Join-Path $fixtureRoot 'harness.json'), (@{callsPath=$callsPath;listing=$case.listing;listExitCode=$case.code} | ConvertTo-Json), $encoding)
        $caught = $null
        try { & $liveScript -SettingsPath $settingsPath | Out-Null } catch { $caught = $_ }
        $calls = @(Get-Content -LiteralPath $callsPath | ForEach-Object { $_ | ConvertFrom-Json })
        $folderDeletes = @($calls | Where-Object { $_.operation -eq '--delete' -and $_.uri.EndsWith('/') })
        Assert-CleanupTest ($calls[0].operation -eq '--delete' -and $calls[0].uri.EndsWith('/connection-check.txt')) ($case.name + ': delete only the exact probe file first.')
        Assert-CleanupTest ($calls[1].operation -eq '--list') ($case.name + ': inspect folder before deciding to delete it.')
        Assert-CleanupTest ($folderDeletes.Count -eq [int]$case.folderRemoved) ($case.name + ': remove folder only after a successful empty listing.')
        $receiptPath = Join-Path $caseRoot 'jenkins-agent-drive-verification.json'
        if ($case.code -ne 0) {
            Assert-CleanupTest ($null -ne $caught -and $caught.Exception.Message -like '*exit 17*') 'A failed listing fails closed.'
            Assert-CleanupTest (-not [IO.File]::Exists($receiptPath)) 'A failed check cannot emit a success receipt.'
        } else {
            Assert-CleanupTest ($null -eq $caught) ($case.name + ': check completes.')
            $receipt = Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json
            Assert-CleanupTest ($receipt.probeFileMovedToTrash -and $receipt.probeFolderMovedToTrash -eq $case.folderRemoved) ($case.name + ': receipt distinguishes file and directory cleanup.')
            Assert-CleanupTest ($receipt.probeFolderLabel -match '^Jenkins-AgentCheck-[a-f0-9]{32}$') ($case.name + ': receipt identifies only the fresh UUID probe.')
        }
        if ($case.folderRemoved) {
            Assert-CleanupTest ($folderDeletes[0].uri -match '^googledrive:/My%20Drive/Fixture/Jenkins-AgentCheck-[a-f0-9]{32}/$') 'Directory deletion is confined to the fresh UUID probe, never the parent.'
        }
    }
    Write-Output ('PASS: {0} offline LiveDrive cleanup assertions.' -f $script:Assertions)
} finally {
    # The test owns this UUID root. Delete files and then empty directories,
    # checking containment first; never use recursive deletion.
    if ([IO.Directory]::Exists($fixtureRoot)) {
        foreach ($item in @(Get-ChildItem -LiteralPath $fixtureRoot -File -Recurse)) {
            if (-not $item.FullName.StartsWith($fixtureRoot + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Fixture cleanup escaped its UUID root.' }
            [IO.File]::Delete($item.FullName)
        }
        foreach ($directory in @(Get-ChildItem -LiteralPath $fixtureRoot -Directory -Recurse | Sort-Object { $_.FullName.Length } -Descending)) {
            if (-not $directory.FullName.StartsWith($fixtureRoot + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Fixture cleanup escaped its UUID root.' }
            [IO.Directory]::Delete($directory.FullName, $false)
        }
        [IO.Directory]::Delete($fixtureRoot, $false)
    }
}
