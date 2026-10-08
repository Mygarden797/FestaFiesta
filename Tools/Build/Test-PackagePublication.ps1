# Exercises the production publication/snapshot code without running Unreal.
[CmdletBinding()]
param([string] $OutputRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$packageScript = Join-Path $PSScriptRoot 'Package-Win64.ps1'
$tokens = $null
$parseErrors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile(
    $packageScript, [ref]$tokens, [ref]$parseErrors)
if ($parseErrors.Count -gt 0) { throw 'Package-Win64.ps1 has syntax errors.' }

# Load only the actual helpers; dot-sourcing the package entry point would build.
foreach ($name in @('Write-Utf8Json', 'Move-ReleaseDirectory', 'Get-GitSourcePaths', 'Write-SourceArchive')) {
    $functionAst = $ast.Find({
        param($node)
        $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name
    }, $true)
    if ($null -eq $functionAst) { throw "Production helper is missing: $name" }
    . ([scriptblock]::Create($functionAst.Extent.Text))
}
$snapshotAst = $ast.Find({
    param($node)
    $node -is [System.Management.Automation.Language.IfStatementAst] -and
        $node.Extent.Text.StartsWith('if ($git.Dirty)') -and $node.Extent.Text.Contains('source-dirty.patch')
}, $true)
if ($null -eq $snapshotAst) { throw 'Production dirty-source snapshot block is missing.' }

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $projectRootForResults = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
    $runId = '{0}-{1}' -f [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss'), [Guid]::NewGuid().ToString('N')
    $OutputRoot = Join-Path $projectRootForResults "Artifacts\Validation\PackagePublication-$runId"
}
$OutputRoot = [System.IO.Path]::GetFullPath($OutputRoot)
if (Test-Path -LiteralPath $OutputRoot) { throw "OutputRoot already exists: $OutputRoot" }
[System.IO.Directory]::CreateDirectory($OutputRoot) | Out-Null
$encoding = New-Object System.Text.UTF8Encoding($false)
$checks = New-Object System.Collections.Generic.List[string]

function Assert-Test {
    param([bool] $Condition, [string] $Message)
    if (-not $Condition) { throw $Message }
    $checks.Add($Message)
}

$latestPath = Join-Path $OutputRoot 'latest.json'
Write-Utf8Json -Value ([ordered]@{ buildLabel = 'old'; releasePath = 'old' }) -Path $latestPath -Atomic
Assert-Test -Condition ((Get-Content -LiteralPath $latestPath -Raw | ConvertFrom-Json).buildLabel -ceq 'old') `
    -Message 'Initial atomic publication creates complete JSON.'
$oldHash = (Get-FileHash -LiteralPath $latestPath -Algorithm SHA256).Hash

# Deny replacement to reproduce a real filesystem publication failure.
$lock = [System.IO.File]::Open($latestPath, [System.IO.FileMode]::Open,
    [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read)
$replacementFailed = $false
try {
    try {
        Write-Utf8Json -Value ([ordered]@{ buildLabel = 'failed'; releasePath = 'failed' }) -Path $latestPath -Atomic
    }
    catch { $replacementFailed = $true }
}
finally { $lock.Dispose() }
Assert-Test -Condition $replacementFailed -Message 'A locked destination rejects publication.'
Assert-Test -Condition ((Get-FileHash -LiteralPath $latestPath -Algorithm SHA256).Hash -ceq $oldHash) `
    -Message 'Failed publication preserves the previous latest.json bytes.'
Assert-Test -Condition (@(Get-ChildItem -LiteralPath $OutputRoot -File -Filter '.latest.json.*.tmp' -Force).Count -eq 0) `
    -Message 'Failed publication cleans its own temporary file.'

Write-Utf8Json -Value ([ordered]@{ buildLabel = 'new'; releasePath = 'new' }) -Path $latestPath -Atomic
$published = Get-Content -LiteralPath $latestPath -Raw | ConvertFrom-Json
Assert-Test -Condition ($published.buildLabel -ceq 'new' -and $published.releasePath -ceq 'new') `
    -Message 'Successful replacement publishes complete new JSON.'
$latestBytes = [System.IO.File]::ReadAllBytes($latestPath)
Assert-Test -Condition (-not ($latestBytes.Length -ge 3 -and $latestBytes[0] -eq 0xef -and
    $latestBytes[1] -eq 0xbb -and $latestBytes[2] -eq 0xbf)) -Message 'Published JSON is UTF-8 without BOM.'

# Hold an actual Windows directory handle without FILE_SHARE_DELETE. Only the
# delay is substituted, so lock failure and successful rename use real I/O.
if ($null -eq ('FFPackagePublicationDirectoryLock' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
public static class FFPackagePublicationDirectoryLock {
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern SafeFileHandle CreateFile(string name, uint access,
        uint share, IntPtr security, uint disposition, uint flags, IntPtr template);
    public static SafeFileHandle Open(string path) {
        SafeFileHandle handle = CreateFile(path, 0x80000000, 3, IntPtr.Zero, 3, 0x02000000, IntPtr.Zero);
        if (handle.IsInvalid) {
            int error = Marshal.GetLastWin32Error();
            handle.Dispose();
            throw new Win32Exception(error);
        }
        return handle;
    }
}
'@
}
$promotionRoot = Join-Path $OutputRoot 'Release promotion fixture'
$promotionStaging = Join-Path $promotionRoot '.staging'
[System.IO.Directory]::CreateDirectory($promotionStaging) | Out-Null
$promotionLatest = Join-Path $promotionRoot 'latest.json'
Write-Utf8Json -Value ([ordered]@{ buildLabel = 'previous'; releasePath = 'previous' }) -Path $promotionLatest -Atomic
$promotionLatestHash = (Get-FileHash -LiteralPath $promotionLatest -Algorithm SHA256).Hash

$transientSource = Join-Path $promotionStaging 'transient'
[System.IO.Directory]::CreateDirectory($transientSource) | Out-Null
[System.IO.File]::WriteAllText((Join-Path $transientSource 'payload.txt'), 'validated payload', $encoding)
$payloadHash = (Get-FileHash -LiteralPath (Join-Path $transientSource 'payload.txt') -Algorithm SHA256).Hash
$script:promotionTestLock = [FFPackagePublicationDirectoryLock]::Open($transientSource)
$script:promotionTestDelays = New-Object System.Collections.Generic.List[int]
function Start-Sleep {
    param([int] $Milliseconds)
    $script:promotionTestDelays.Add($Milliseconds)
    $script:promotionTestLock.Dispose()
}
try {
    Move-ReleaseDirectory -OutputRoot $promotionRoot -BuildLabel 'transient' -WarningAction SilentlyContinue
}
finally {
    $script:promotionTestLock.Dispose()
    Remove-Item -LiteralPath Function:Start-Sleep
}
Assert-Test -Condition ($script:promotionTestDelays.Count -eq 1 -and $script:promotionTestDelays[0] -eq 500) `
    -Message 'A real transient directory lock is retried and succeeds after release.'
Assert-Test -Condition (-not (Test-Path -LiteralPath $transientSource) -and
    (Get-FileHash -LiteralPath (Join-Path $promotionRoot 'transient\payload.txt') -Algorithm SHA256).Hash -ceq $payloadHash) `
    -Message 'Successful promotion moves the exact validated payload to the expected final directory.'

$persistentSource = Join-Path $promotionStaging 'persistent'
[System.IO.Directory]::CreateDirectory($persistentSource) | Out-Null
[System.IO.File]::WriteAllText((Join-Path $persistentSource 'payload.txt'), 'retained payload', $encoding)
$persistentHash = (Get-FileHash -LiteralPath (Join-Path $persistentSource 'payload.txt') -Algorithm SHA256).Hash
$script:promotionTestLock = [FFPackagePublicationDirectoryLock]::Open($persistentSource)
$script:promotionTestDelays.Clear()
function Start-Sleep {
    param([int] $Milliseconds)
    $script:promotionTestDelays.Add($Milliseconds)
}
$persistentError = $null
try {
    try { Move-ReleaseDirectory -OutputRoot $promotionRoot -BuildLabel 'persistent' -WarningAction SilentlyContinue }
    catch { $persistentError = $_ }
}
finally {
    $script:promotionTestLock.Dispose()
    Remove-Item -LiteralPath Function:Start-Sleep
}
Assert-Test -Condition ($null -ne $persistentError -and
    $persistentError.Exception.Data['ReleasePromotionAttempts'] -eq 8 -and
    ($script:promotionTestDelays -join ',') -ceq '500,1000,2000,4000,4000,4000,4000') `
    -Message 'Persistent locks stop after eight attempts and 19.5 seconds of bounded scheduled delays.'
Assert-Test -Condition ((Get-FileHash -LiteralPath (Join-Path $persistentSource 'payload.txt') -Algorithm SHA256).Hash -ceq $persistentHash -and
    -not (Test-Path -LiteralPath (Join-Path $promotionRoot 'persistent'))) `
    -Message 'Exhausted promotion preserves staging bytes and does not create a final release.'

# A destination appearing between attempts must stop retrying and retain both
# the existing destination and the FIRST lock error, not the later safety error.
$raceSource = Join-Path $promotionStaging 'appeared'
$script:promotionRaceDestination = Join-Path $promotionRoot 'appeared'
[System.IO.Directory]::CreateDirectory($raceSource) | Out-Null
$script:promotionTestLock = [FFPackagePublicationDirectoryLock]::Open($raceSource)
$script:promotionTestDelays.Clear()
function Start-Sleep {
    param([int] $Milliseconds)
    $script:promotionTestDelays.Add($Milliseconds)
    $script:promotionTestLock.Dispose()
    [System.IO.Directory]::CreateDirectory($script:promotionRaceDestination) | Out-Null
    [System.IO.File]::WriteAllText((Join-Path $script:promotionRaceDestination 'keep.txt'), 'existing release')
}
$raceError = $null
try {
    try { Move-ReleaseDirectory -OutputRoot $promotionRoot -BuildLabel 'appeared' -WarningAction SilentlyContinue }
    catch { $raceError = $_ }
}
finally {
    $script:promotionTestLock.Dispose()
    Remove-Item -LiteralPath Function:Start-Sleep
}
$originalMoveException = $raceError.Exception
while ($null -ne $originalMoveException.InnerException) { $originalMoveException = $originalMoveException.InnerException }
Assert-Test -Condition (($originalMoveException.HResult -band 0xffff) -in @(5, 32, 33) -and
    $raceError.Exception.Data['ReleasePromotionLastError'] -like '*destination already exists*' -and
    $script:promotionTestDelays.Count -eq 1) `
    -Message 'A destination race stops immediately while preserving the first native lock error.'
Assert-Test -Condition ((Test-Path -LiteralPath $raceSource -PathType Container) -and
    [System.IO.File]::ReadAllText((Join-Path $script:promotionRaceDestination 'keep.txt')) -ceq 'existing release') `
    -Message 'A destination race preserves both staging and the pre-existing destination.'

$invalidLabelError = $null
try { Move-ReleaseDirectory -OutputRoot $promotionRoot -BuildLabel '..\outside' }
catch { $invalidLabelError = $_ }
Assert-Test -Condition ($null -ne $invalidLabelError) -Message 'Promotion rejects a label that escapes the selected release root.'
$redirectedPayload = Join-Path $OutputRoot 'Outside staging payload'
[System.IO.Directory]::CreateDirectory($redirectedPayload) | Out-Null
[System.IO.File]::WriteAllText((Join-Path $redirectedPayload 'keep.txt'), 'do not move')
$redirectedSource = Join-Path $promotionStaging 'redirected'
New-Item -ItemType Junction -Path $redirectedSource -Target $redirectedPayload | Out-Null
$redirectedError = $null
try { Move-ReleaseDirectory -OutputRoot $promotionRoot -BuildLabel 'redirected' }
catch { $redirectedError = $_ }
Assert-Test -Condition ($null -ne $redirectedError -and
    $redirectedError.Exception.Data['ReleasePromotionAttempts'] -eq 1 -and
    -not (Test-Path -LiteralPath (Join-Path $promotionRoot 'redirected')) -and
    [System.IO.File]::ReadAllText((Join-Path $redirectedPayload 'keep.txt')) -ceq 'do not move') `
    -Message 'A staging junction is rejected before moving its outside payload or retrying.'
Assert-Test -Condition ((Get-FileHash -LiteralPath $promotionLatest -Algorithm SHA256).Hash -ceq $promotionLatestHash) `
    -Message 'Successful and failed directory promotion leave the previous latest.json byte-for-byte intact.'

$source = $ast.Extent.Text
$verificationWrite = $source.IndexOf('Write-Utf8Json -Value $verificationSummary -Path $finalVerificationPath', [StringComparison]::Ordinal)
$latestWrite = $source.IndexOf("Write-Utf8Json -Value `$latest -Path (Join-Path `$OutputRoot 'latest.json') -Atomic", [StringComparison]::Ordinal)
Assert-Test -Condition ($verificationWrite -ge 0 -and $latestWrite -gt $verificationWrite) `
    -Message 'Production latest publication follows final verification metadata.'
$promotionCall = $source.IndexOf('Move-ReleaseDirectory -OutputRoot $OutputRoot -BuildLabel $BuildLabel', [StringComparison]::Ordinal)
Assert-Test -Condition ($promotionCall -ge 0 -and $verificationWrite -gt $promotionCall) `
    -Message 'Production uses bounded promotion before final verification metadata and latest publication.'

$projectRoot = Join-Path $OutputRoot 'Source fixture'
$metadataRoot = Join-Path $OutputRoot 'Snapshot'
[System.IO.Directory]::CreateDirectory($projectRoot) | Out-Null
[System.IO.Directory]::CreateDirectory($metadataRoot) | Out-Null
& git -C $projectRoot init --quiet --initial-branch=main
if ($LASTEXITCODE -ne 0) { throw 'Fixture Git initialization failed.' }
# Build Korean names using code points so the fixture also runs in Windows
# PowerShell 5.1 regardless of the encoding used to read this test script.
$hangul = [string][char]0xd55c + [string][char]0xae00
$trackedAssetPath = "$hangul assets/$hangul source.uasset"
$trackedAsset = Join-Path $projectRoot $trackedAssetPath
[System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($trackedAsset)) | Out-Null
[System.IO.File]::WriteAllBytes($trackedAsset, [byte[]](0..255))
[System.IO.File]::WriteAllText((Join-Path $projectRoot '.gitignore'), "ignored.txt`n", $encoding)
[System.IO.File]::WriteAllText((Join-Path $projectRoot '.gitattributes'), "*.uasset filter=lfs diff=lfs merge=lfs -text`n", $encoding)
[System.IO.File]::WriteAllText((Join-Path $projectRoot 'tracked.txt'), 'tracked baseline', $encoding)
[System.IO.File]::WriteAllText((Join-Path $projectRoot 'deleted.txt'), 'deleted baseline', $encoding)
[System.IO.File]::WriteAllText((Join-Path $projectRoot 'ignored.txt'), 'ignored bytes', $encoding)
# The local LFS filter stores a pointer in Git while keeping real binary bytes
# in the worktree. No remote or network access is needed by this fixture.
& git -C $projectRoot lfs install --local | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Fixture local Git LFS setup failed.' }
& git -C $projectRoot add -- .gitignore .gitattributes tracked.txt deleted.txt $trackedAssetPath
if ($LASTEXITCODE -ne 0) { throw 'Fixture Git add failed.' }
& git -c user.name=PackageSnapshotTest -c user.email=package-snapshot@example.invalid `
    -c core.hooksPath=.git/no-hooks -c commit.gpgSign=false -C $projectRoot commit --quiet -m 'Source fixture baseline'
if ($LASTEXITCODE -ne 0) { throw 'Fixture Git commit failed.' }
$assetInGit = @(& git -C $projectRoot show "HEAD:$trackedAssetPath") -join "`n"
if ($LASTEXITCODE -ne 0) { throw 'Fixture Git LFS pointer read failed.' }
Assert-Test -Condition ($assetInGit.StartsWith('version https://git-lfs.github.com/spec/v1')) `
    -Message 'Tracked binary fixture is represented by a Git LFS pointer in HEAD.'
[System.IO.File]::WriteAllBytes($trackedAsset, [byte[]](255..0))
Remove-Item -LiteralPath (Join-Path $projectRoot 'deleted.txt')

$expectedPaths = @("$hangul folder/$hangul data.txt", 'spaces/sub file.ps1', 'special/[draft] # sample.ps1')
foreach ($relativePath in $expectedPaths) {
    $absolute = Join-Path $projectRoot $relativePath
    [System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($absolute)) | Out-Null
    [System.IO.File]::WriteAllText($absolute, "source bytes: $relativePath`n", $encoding)
}
& git -C $projectRoot config core.quotepath true
if ($LASTEXITCODE -ne 0) { throw 'Fixture Git configuration failed.' }
$untrackedPaths = @(Get-GitSourcePaths -Root $projectRoot -Kind Untracked)
Assert-Test -Condition ((@($untrackedPaths | Sort-Object) -join [char]0) -ceq
    (@($expectedPaths | Sort-Object) -join [char]0)) `
    -Message 'NUL-delimited enumeration preserves Korean, spaces and special characters while excluding tracked/ignored files.'

# Execute the actual packaging snapshot block against the fixture, then restore it.
$git = [pscustomobject]@{ Dirty = $true }
. ([scriptblock]::Create($snapshotAst.Extent.Text))
$archivePath = Join-Path $metadataRoot 'source-dirty-untracked.zip'
$restoreRoot = Join-Path $OutputRoot 'Restored'
[System.IO.Compression.ZipFile]::ExtractToDirectory($archivePath, $restoreRoot)
foreach ($relativePath in $expectedPaths) {
    $sourceHash = (Get-FileHash -LiteralPath (Join-Path $projectRoot $relativePath) -Algorithm SHA256).Hash
    $restoredHash = (Get-FileHash -LiteralPath (Join-Path $restoreRoot $relativePath) -Algorithm SHA256).Hash
    Assert-Test -Condition ($sourceHash -ceq $restoredHash) -Message "Snapshot restores exact filename and bytes: $relativePath"
}
Assert-Test -Condition (@(Get-ChildItem -LiteralPath $restoreRoot -Recurse -File).Count -eq $expectedPaths.Count) `
    -Message 'Snapshot contains only the expected untracked files.'
$expectedUntrackedList = @($expectedPaths | ForEach-Object { "?? $_" } | Sort-Object)
$actualUntrackedList = @(Get-Content -LiteralPath (Join-Path $metadataRoot 'source-dirty-untracked.txt') -Encoding UTF8 | Sort-Object)
Assert-Test -Condition (($actualUntrackedList -join [char]0) -ceq ($expectedUntrackedList -join [char]0)) `
    -Message 'Production snapshot retains the existing UTF-8 untracked-file list contract.'

$trackedPaths = @(Get-GitSourcePaths -Root $projectRoot -Kind TrackedChanges)
Assert-Test -Condition ($trackedPaths.Count -eq 1 -and $trackedPaths[0] -ceq $trackedAssetPath) `
    -Message 'Tracked-change enumeration preserves the asset path and excludes unchanged/deleted files.'
$trackedRestoreRoot = Join-Path $OutputRoot 'Restored tracked'
[System.IO.Compression.ZipFile]::ExtractToDirectory(
    (Join-Path $metadataRoot 'source-dirty-tracked.zip'), $trackedRestoreRoot)
$sourceAssetHash = (Get-FileHash -LiteralPath $trackedAsset -Algorithm SHA256).Hash
$restoredAssetHash = (Get-FileHash -LiteralPath (Join-Path $trackedRestoreRoot $trackedAssetPath) -Algorithm SHA256).Hash
Assert-Test -Condition ($sourceAssetHash -ceq $restoredAssetHash) `
    -Message 'Tracked LFS snapshot restores the exact changed materialized binary bytes by SHA-256.'
Assert-Test -Condition (@(Get-ChildItem -LiteralPath $trackedRestoreRoot -Recurse -File).Count -eq 1) `
    -Message 'Tracked snapshot contains only the changed working file.'
$sourcePatch = [System.IO.File]::ReadAllText((Join-Path $metadataRoot 'source-dirty.patch'), $encoding)
Assert-Test -Condition ($sourcePatch.Contains('oid sha256:' + $sourceAssetHash.ToLowerInvariant())) `
    -Message 'The tracked patch contains an LFS pointer while the ZIP separately preserves its binary bytes.'
Assert-Test -Condition ($sourcePatch.Contains('deleted file mode') -and $sourcePatch.Contains('deleted.txt')) `
    -Message 'Deleted tracked files remain represented by the binary patch.'

$unsafeArchiveRejected = $false
try {
    Write-SourceArchive -Root $projectRoot -Paths @('..\latest.json') `
        -ArchivePath (Join-Path $OutputRoot 'Unsafe snapshot.zip')
}
catch { $unsafeArchiveRejected = $true }
Assert-Test -Condition $unsafeArchiveRejected -Message 'The shared source-archive helper rejects paths outside the source root.'

$emptyRoot = Join-Path $OutputRoot 'Empty fixture'
[System.IO.Directory]::CreateDirectory($emptyRoot) | Out-Null
& git -C $emptyRoot init --quiet --initial-branch=main
if ($LASTEXITCODE -ne 0) { throw 'Empty fixture Git initialization failed.' }
Assert-Test -Condition (@(Get-GitSourcePaths -Root $emptyRoot -Kind Untracked).Count -eq 0) `
    -Message 'An empty untracked-file set remains an empty collection.'

$summary = [ordered]@{
    schemaVersion = 1
    status = 'passed'
    checkedAtUtc = [DateTime]::UtcNow.ToString('o')
    packageScriptSha256 = (Get-FileHash -LiteralPath $packageScript -Algorithm SHA256).Hash.ToLowerInvariant()
    checkCount = $checks.Count
    checks = @($checks)
    fixtures = $OutputRoot
}
$summaryPath = Join-Path $OutputRoot 'summary.json'
Write-Utf8Json -Value $summary -Path $summaryPath
Write-Output $summaryPath
