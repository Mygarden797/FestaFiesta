Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Write-FFState {
    param([Parameter(Mandatory = $true)] $State, [Parameter(Mandatory = $true)][string] $Path)
    $temporaryPath = $Path + '.' + [Guid]::NewGuid().ToString('N') + '.tmp'
    try {
        $json = $State | ConvertTo-Json -Depth 12
        [IO.File]::WriteAllText($temporaryPath, $json + [Environment]::NewLine,
            (New-Object Text.UTF8Encoding($false)))
        if ([IO.File]::Exists($Path)) {
            [IO.File]::Replace($temporaryPath, $Path, [NullString]::Value)
        }
        else { [IO.File]::Move($temporaryPath, $Path) }
    }
    finally {
        if ([IO.File]::Exists($temporaryPath)) { [IO.File]::Delete($temporaryPath) }
    }
}

function Read-FFState {
    param([string] $Path, $Binding)
    if (-not [IO.File]::Exists($Path)) {
        return [pscustomobject]@{ schemaVersion = 1; binding = $Binding; published = $null; pending = $null }
    }
    $state = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    if ($state.schemaVersion -ne 1) { throw 'Unsupported daily-build state schema.' }
    foreach ($name in @('repository', 'branch', 'checkoutRoot', 'outputRoot', 'driveFolderId', 'rcloneRemote', 'configuration')) {
        if ([string]$state.binding.$name -cne [string]$Binding.$name) {
            throw "Daily-build state belongs to a different $name. Use a separate StateRoot."
        }
    }
    foreach ($item in @($state.published, $state.pending)) {
        if ($null -ne $item -and [string]$item.commit -notmatch '^[a-f0-9]{40}([a-f0-9]{24})?$') {
            throw 'Daily-build state contains an invalid commit.'
        }
    }
    return $state
}

function Get-FFBuildPlan {
    param($State, [string] $CandidateCommit)
    $actions = @()
    if ($null -ne $State.pending) {
        $actions += if ($State.pending.phase -eq 'ready') { 'ResumePendingUpload' } else { 'RecoverInterruptedBuild' }
    }
    if ([string]::IsNullOrWhiteSpace($CandidateCommit)) { $actions += 'DiscoverRemoteCommit' }
    elseif ($null -ne $State.pending -and $State.pending.commit -ceq $CandidateCommit) {
        $actions += 'SkipAfterPendingPublication'
    }
    elseif ($null -eq $State.pending -and $null -ne $State.published -and $State.published.commit -ceq $CandidateCommit) {
        $actions += 'SkipUnchanged'
    }
    else { $actions += 'BuildAndPublish' }
    return [pscustomobject]@{ actions = $actions; candidateCommit = $CandidateCommit }
}

function New-FFBuildLabel {
    param([string] $ProjectVersion, [string] $Commit, [string] $BuildNumber,
        [DateTimeOffset] $Now = [DateTimeOffset]::UtcNow)
    if ($ProjectVersion -notmatch '^[0-9][A-Za-z0-9._-]{0,23}$') { throw 'ProjectVersion is not safe for a build label.' }
    if ($Commit -notmatch '^[a-f0-9]{40}([a-f0-9]{24})?$') { throw 'A full lowercase commit SHA is required.' }
    if ([string]::IsNullOrWhiteSpace($BuildNumber)) { $BuildNumber = 'manual' }
    if ($BuildNumber -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]{0,31}$') { throw 'BuildNumber contains unsupported characters.' }
    $kstDate = $Now.ToOffset([TimeSpan]::FromHours(9)).ToString('yyyyMMdd-HHmmss')
    return 'FF-v{0}-{1}-b{2}-{3}' -f $ProjectVersion, $kstDate, $BuildNumber, $Commit.Substring(0, 8)
}

function New-FFPublicationName {
    param([ValidateRange(1, 2147483647)][long] $Version, [DateTimeOffset] $StartedAt)
    $date = $StartedAt.ToOffset([TimeSpan]::FromHours(9)).ToString('yyyy.MM.dd.HH:mm', [Globalization.CultureInfo]::InvariantCulture)
    return 'V{0}({1})' -f $Version, $date
}

function Initialize-FFPublicationSequence {
    param($State)
    # The first release in the simplified distribution sequence is V24.
    # A migrated published version or a reserved pending version takes precedence.
    $sequence = 23L
    $values = @()
    if ($State.PSObject.Properties.Name -contains 'publicationSequence') { $values += $State.publicationSequence }
    foreach ($item in @($State.published, $State.pending)) {
        if ($null -ne $item -and $item.PSObject.Properties.Name -contains 'publicationVersion') { $values += $item.publicationVersion }
    }
    foreach ($value in $values) {
        $parsed = 0L
        if ([string]$value -notmatch '^[0-9]+$' -or -not [long]::TryParse([string]$value, [ref]$parsed) -or
            $parsed -lt 0 -or $parsed -gt 2147483647) { throw 'Invalid publication version in daily-build state.' }
        $sequence = [Math]::Max($sequence, $parsed)
    }
    $State | Add-Member -NotePropertyName publicationSequence -NotePropertyValue $sequence -Force
}

function Get-FFPendingPublicationStartedAt {
    param($Pending)
    # PowerShell 7 can deserialize ISO timestamps as DateTime; preserve its UTC
    # kind rather than formatting it as a timezone-free local string first.
    if ($Pending.startedAtUtc -is [DateTime] -or $Pending.startedAtUtc -is [DateTimeOffset]) {
        return [DateTimeOffset]$Pending.startedAtUtc
    }
    return [DateTimeOffset]::Parse([string]$Pending.startedAtUtc, [Globalization.CultureInfo]::InvariantCulture)
}

function Set-FFPendingPublicationIdentity {
    param($State)
    $pending = $State.pending
    $hasVersion = $pending.PSObject.Properties.Name -contains 'publicationVersion'
    $hasName = $pending.PSObject.Properties.Name -contains 'publicationName'
    if ($hasVersion -ne $hasName) { throw 'The pending publication identity is incomplete.' }
    $startedAt = Get-FFPendingPublicationStartedAt -Pending $pending
    if ($hasVersion) {
        if ([string]$pending.publicationVersion -notmatch '^[1-9][0-9]*$' -or
            [long]$pending.publicationVersion -gt 2147483647 -or
            [string]$pending.publicationName -cne (New-FFPublicationName -Version $pending.publicationVersion -StartedAt $startedAt)) {
            throw 'The pending publication name does not match its reserved version and Korean build date/time.'
        }
        return
    }
    if ($State.publicationSequence -ge 2147483647) { throw 'The publication version sequence is exhausted.' }
    $State.publicationSequence++
    $pending | Add-Member -NotePropertyName publicationVersion -NotePropertyValue $State.publicationSequence
    $pending | Add-Member -NotePropertyName publicationName -NotePropertyValue (
        New-FFPublicationName -Version $State.publicationSequence -StartedAt $startedAt)
}

function Update-FFPendingPublicationLayout {
    param($Options, $State, [string] $StatePath)
    $pending = $State.pending
    if (-not ($pending.PSObject.Properties.Name -contains 'publicationName') -or
        [string]$pending.publicationName -cnotmatch '^V[1-9][0-9]*\([0-9]{4}\.[0-9]{2}\.[0-9]{2}(\.[0-9]{2}:[0-9]{2})?\)(\.zip)?$') { return }
    if ($pending.buildLabel -cnotmatch '^[A-Za-z0-9][A-Za-z0-9._-]{0,119}$') { throw 'Invalid pending build label.' }
    $startedAt = Get-FFPendingPublicationStartedAt -Pending $pending
    $publicationName = New-FFPublicationName -Version $pending.publicationVersion -StartedAt $startedAt
    if ([string]$pending.publicationName -ceq $publicationName) { return }
    $dateOnly = $startedAt.ToOffset([TimeSpan]::FromHours(9)).ToString('yyyy.MM.dd', [Globalization.CultureInfo]::InvariantCulture)
    $expectedOldFolder = 'V{0}({1})' -f $pending.publicationVersion, $dateOnly
    $isFlat = ([string]$pending.publicationName).EndsWith('.zip', [StringComparison]::Ordinal)
    $expectedOldName = if ($isFlat) { $expectedOldFolder + '.zip' } else { $expectedOldFolder }
    if ([string]$pending.publicationName -cne $expectedOldName -and
        -not ($isFlat -and [string]$pending.publicationName -ceq ($publicationName + '.zip'))) {
        throw 'The legacy publication name does not match its reserved version and Korean build date/time.'
    }
    if ($pending.files.Count -eq 1) {
        $file = $pending.files[0]
        $expectedPath = Join-Path (Join-Path $Options.OutputRoot $pending.buildLabel) ($pending.buildLabel + '.zip')
        $expectedRemoteName = if ($isFlat) { $pending.publicationName } else { 'FestaFiesta!.zip' }
        if ([IO.Path]::GetFullPath([string]$file.path) -cne [IO.Path]::GetFullPath($expectedPath) -or
            $file.remoteName -cne $expectedRemoteName) { throw 'Invalid legacy publication file list.' }
        if ((Get-FileHash -LiteralPath $file.path -Algorithm SHA256).Hash.ToLowerInvariant() -cne $file.sha256 -or
            (Get-Item -LiteralPath $file.path).Length -ne $file.bytes) {
            throw 'A pending publication file changed on disk; refusing to upload different bytes.'
        }
    }
    elseif ($pending.phase -cne 'building' -or $pending.files.Count -ne 0) { throw 'Invalid legacy publication file list.' }
    # Keep the reserved version and derive the missing time from the recorded
    # build start. No ZIP bytes or expected hashes are replaced.
    $pending.publicationName = $publicationName
    Set-FFPendingPublicationIdentity -State $State
    if ($pending.files.Count -eq 1) { $pending.files[0].remoteName = 'FestaFiesta!.zip' }
    Write-FFState -State $State -Path $StatePath
}

function Invoke-FFNative {
    param([string] $FilePath, [string[]] $Arguments, [string] $Operation)
    # Argument arrays are never evaluated as shell code. Keep credentials and
    # remote diagnostics out of Jenkins logs, including error messages.
    $priorPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $output = @(& $FilePath @Arguments 2>&1)
        $exitCode = $LASTEXITCODE
    }
    finally { $ErrorActionPreference = $priorPreference }
    if ($exitCode -ne 0) { throw "$Operation failed (exit $exitCode). Credential-bearing diagnostics were suppressed." }
    return ($output | Out-String).Trim()
}

function Get-FFRemoteCommit {
    param($Options)
    $reference = 'refs/heads/' + $Options.SourceBranch
    $result = Invoke-FFNative -FilePath 'git.exe' -Arguments @(
        'ls-remote', '--exit-code', $Options.SourceRepository, $reference) -Operation 'Discover branch head'
    $lines = @($result -split '\r?\n' | Where-Object { $_ -match '\S' })
    if ($lines.Count -ne 1 -or $lines[0] -notmatch ('^([a-f0-9]{40}(?:[a-f0-9]{24})?)\s+' + [regex]::Escape($reference) + '$')) {
        throw 'The requested branch did not resolve to exactly one full commit SHA.'
    }
    return $Matches[1]
}

function Invoke-FFCheckout {
    param($Options, [string] $Commit)
    $root = $Options.CheckoutRoot
    $markerPath = Join-Path $root '.git\festafiesta-jenkins-owner.json'
    if (-not (Test-Path -LiteralPath $markerPath -PathType Leaf)) {
        if (Test-Path -LiteralPath $root) {
            $existingFiles = @(Get-ChildItem -LiteralPath $root -Force)
            if ($existingFiles.Count -gt 0) { throw 'CheckoutRoot must be empty or owned by this Jenkins pipeline.' }
        }
        [IO.Directory]::CreateDirectory($root) | Out-Null
        $null = Invoke-FFNative -FilePath 'git.exe' -Arguments @('init', $root) -Operation 'Initialize CI checkout'
        $null = Invoke-FFNative -FilePath 'git.exe' -Arguments @('-C', $root, 'remote', 'add', 'origin', $Options.SourceRepository) -Operation 'Configure source remote'
        Write-FFState -State ([ordered]@{ repository = $Options.SourceRepository; checkoutRoot = $root }) -Path $markerPath
    }
    $owner = Get-Content -LiteralPath $markerPath -Raw | ConvertFrom-Json
    if ($owner.repository -cne $Options.SourceRepository -or $owner.checkoutRoot -cne $root) {
        throw 'CI checkout ownership does not match the requested repository/path.'
    }
    $configuredRemote = Invoke-FFNative -FilePath 'git.exe' -Arguments @('-C', $root, 'remote', 'get-url', 'origin') -Operation 'Verify source remote'
    if ($configuredRemote -cne $Options.SourceRepository) { throw 'CI checkout origin does not match SourceRepository.' }
    $previousSmudge = [Environment]::GetEnvironmentVariable('GIT_LFS_SKIP_SMUDGE', 'Process')
    try {
        [Environment]::SetEnvironmentVariable('GIT_LFS_SKIP_SMUDGE', '1', 'Process')
        $null = Invoke-FFNative -FilePath 'git.exe' -Arguments @('-C', $root, 'fetch', '--no-tags', '--depth=1', 'origin', $Commit) -Operation 'Fetch pinned source commit'
        $null = Invoke-FFNative -FilePath 'git.exe' -Arguments @('-C', $root, 'checkout', '--detach', '--force', $Commit) -Operation 'Checkout pinned source commit'
    }
    finally { [Environment]::SetEnvironmentVariable('GIT_LFS_SKIP_SMUDGE', $previousSmudge, 'Process') }
    $actual = Invoke-FFNative -FilePath 'git.exe' -Arguments @('-C', $root, 'rev-parse', 'HEAD') -Operation 'Verify source commit'
    if ($actual -cne $Commit) { throw 'Checked-out source does not match the pinned commit.' }
    $null = Invoke-FFNative -FilePath 'git.exe' -Arguments @('-C', $root, 'lfs', 'install', '--local') -Operation 'Initialize Git LFS'
    $null = Invoke-FFNative -FilePath 'git.exe' -Arguments @('-C', $root, 'lfs', 'fetch', 'origin', $Commit) -Operation 'Fetch pinned LFS assets'
    $null = Invoke-FFNative -FilePath 'git.exe' -Arguments @('-C', $root, 'lfs', 'checkout') -Operation 'Materialize LFS assets'
    if (Test-Path -LiteralPath (Join-Path $root '.gitmodules')) {
        $null = Invoke-FFNative -FilePath 'git.exe' -Arguments @('-C', $root, 'submodule', 'update', '--init', '--recursive') -Operation 'Checkout submodules'
    }
    $dirty = Invoke-FFNative -FilePath 'git.exe' -Arguments @('-C', $root, 'status', '--porcelain', '--untracked-files=normal') -Operation 'Verify clean CI checkout'
    if (-not [string]::IsNullOrWhiteSpace($dirty)) { throw 'The dedicated CI checkout is dirty; refusing to publish it.' }
}

function Invoke-FFPackage {
    param($Options, [string] $BuildLabel)
    $packageScript = Join-Path $Options.CheckoutRoot 'Tools\Build\Package-Win64.ps1'
    if (-not (Test-Path -LiteralPath $packageScript -PathType Leaf)) { throw 'The source branch does not contain Package-Win64.ps1.' }
    $powerShellExe = Join-Path $PSHOME 'powershell.exe'
    if (-not (Test-Path -LiteralPath $powerShellExe -PathType Leaf)) { $powerShellExe = Join-Path $PSHOME 'pwsh.exe' }
    # Run in a separate PowerShell process so errors have an unambiguous exit code.
    # Package logs contain build diagnostics, while upload/auth output is suppressed.
    & $powerShellExe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $packageScript `
        -EngineRoot $Options.EngineRoot -ProjectFile (Join-Path $Options.CheckoutRoot 'FestaFiesta.uproject') `
        -OutputRoot $Options.OutputRoot -BuildLabel $BuildLabel -Configuration $Options.Configuration -GraphicsSmokeTest | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "Package-Win64 failed (exit $LASTEXITCODE). See the build logs." }
}

function Get-FFPublicationFiles {
    param($Options, $Pending)
    if ($Pending.buildLabel -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]{0,119}$') { throw 'Invalid pending build label.' }
    $releaseRoot = Join-Path $Options.OutputRoot $Pending.buildLabel
    $latestPath = Join-Path $Options.OutputRoot 'latest.json'
    if (-not (Test-Path -LiteralPath $latestPath -PathType Leaf)) { throw 'A completed package publication index is required.' }
    $latest = Get-Content -LiteralPath $latestPath -Raw | ConvertFrom-Json
    if ($latest.buildLabel -cne $Pending.buildLabel -or $latest.releasePath -cne $Pending.buildLabel) {
        throw 'The completed package index does not identify this build.'
    }
    if (Test-Path -LiteralPath (Join-Path $releaseRoot 'Metadata\failure.json')) { throw 'This package has a recorded build failure.' }
    $manifestPath = Join-Path $releaseRoot 'Metadata\build-manifest.json'
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if ($manifest.releaseStatus -cne 'validated' -or $manifest.buildLabel -cne $Pending.buildLabel -or $manifest.source.dirty) {
        throw 'Only clean, fully validated packages may be uploaded.'
    }
    $sourceCommit = [string]$manifest.source.commit
    if ($sourceCommit -notmatch '^[a-f0-9]{12,64}$' -or -not $Pending.commit.StartsWith($sourceCommit, [StringComparison]::Ordinal)) {
        throw 'Package manifest does not match the pinned source commit.'
    }
    foreach ($check in @('automationTests', 'dataValidation', 'smokeTest', 'graphicsStartupTest', 'defaultLauncherStartupTest')) {
        if ($manifest.validation.$check -ne $true) { throw "Package validation is incomplete: $check." }
    }
    if ($manifest.validation.qualityOverride -ne $false) { throw 'Packages with a quality override cannot be uploaded.' }
    $verificationPath = Join-Path $releaseRoot 'Metadata\verification.json'
    $verification = Get-Content -LiteralPath $verificationPath -Raw | ConvertFrom-Json
    foreach ($check in @('unrealPakTested', 'ioStoreInspected', 'smokeTested', 'graphicsSmokeTested', 'defaultLauncherTested')) {
        if ($verification.$check -ne $true) { throw "Package verification is incomplete: $check." }
    }
    $zipName = $Pending.buildLabel + '.zip'
    if ($latest.archive -cne $zipName) { throw 'Package index does not identify the expected ZIP.' }
    $zipPath = Join-Path $releaseRoot $zipName
    $checksumPath = Join-Path $releaseRoot 'Metadata\ARCHIVE-SHA256.txt'
    $sha256 = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
    $expectedChecksum = $sha256 + '  ' + $zipName
    if ((Get-Content -LiteralPath $checksumPath -Raw).Trim() -cne $expectedChecksum) { throw 'Package ZIP does not match ARCHIVE-SHA256.txt.' }
    $files = @()
    foreach ($filePath in @($zipPath)) {
        $files += [pscustomobject]@{
            path = $filePath
            remoteName = 'FestaFiesta!.zip'
            bytes = (Get-Item -LiteralPath $filePath).Length
            sha256 = (Get-FileHash -LiteralPath $filePath -Algorithm SHA256).Hash.ToLowerInvariant()
            md5 = (Get-FileHash -LiteralPath $filePath -Algorithm MD5).Hash.ToLowerInvariant()
        }
    }
    return $files
}

function Invoke-FFUploadFile {
    param($Options, [string] $BuildLabel, $File, [string] $PublicationFolder)
    if ([string]$File.remoteName -cne 'FestaFiesta!.zip') { throw 'Unsafe remote filename in pending state.' }
    if ($PublicationFolder -cnotmatch '^V[1-9][0-9]*\([0-9]{4}\.[0-9]{2}\.[0-9]{2}\.[0-9]{2}:[0-9]{2}\)$') { throw 'Unsafe publication folder in pending state.' }
    $localHash = (Get-FileHash -LiteralPath $File.path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($localHash -cne $File.sha256 -or (Get-Item -LiteralPath $File.path).Length -ne $File.bytes) {
        throw 'A pending publication file changed on disk; refusing to upload different bytes.'
    }
    if ($Options.UploadProvider -eq 'Duck') {
        Import-Module (Join-Path $PSScriptRoot 'FestaFiesta.Duck.psm1') -Force
        Invoke-FFDuckUploadFile -Options $Options -BuildLabel $BuildLabel -File $File -PublicationFolder $PublicationFolder
        return
    }
    $remotePath = $Options.RcloneRemote + ':/' + $PublicationFolder + '/' + $File.remoteName
    $commonArguments = @('--config', $Options.RcloneConfig, '--drive-root-folder-id', $Options.DriveFolderId,
        '--retries', '3', '--low-level-retries', '10', '--contimeout', '30s', '--timeout', '10m')
    $null = Invoke-FFNative -FilePath $Options.RcloneExe -Arguments (@('copyto', $File.path, $remotePath, '--checksum') + $commonArguments) -Operation 'Upload build artifact'
    $remoteHash = Invoke-FFNative -FilePath $Options.RcloneExe -Arguments (@('md5sum', $remotePath) + $commonArguments) -Operation 'Verify uploaded artifact'
    # Drive exposes MD5 for binary uploads; the SHA-256 remains in local metadata.
    $hashLines = @($remoteHash -split '\r?\n' | Where-Object { $_ -match '\S' })
    if ($hashLines.Count -ne 1 -or $hashLines[0] -notmatch '^([a-fA-F0-9]{32})\s+' -or $Matches[1].ToLowerInvariant() -cne $File.md5) {
        throw 'Google Drive artifact checksum verification failed.'
    }
}

function Complete-FFPendingPublication {
    param($Options, $State, [string] $StatePath)
    $pending = $State.pending
    if ($pending.phase -cne 'ready') { throw 'A pending build is not ready to upload.' }
    if ($pending.buildLabel -cnotmatch '^[A-Za-z0-9][A-Za-z0-9._-]{0,119}$') { throw 'Invalid pending build label.' }
    if ($pending.files.Count -eq 4 -and -not ($pending.PSObject.Properties.Name -contains 'publicationName')) {
        # Upgrade a pre-existing queue only after checking every recorded byte.
        # Never recalculate expected hashes over a modified legacy pending file.
        $releaseRoot = [IO.Path]::GetFullPath((Join-Path $Options.OutputRoot $pending.buildLabel))
        $expectedPaths = @((Join-Path $releaseRoot ($pending.buildLabel + '.zip')),
            (Join-Path $releaseRoot 'Metadata\build-manifest.json'),
            (Join-Path $releaseRoot 'Metadata\ARCHIVE-SHA256.txt'),
            (Join-Path $releaseRoot 'Metadata\verification.json'))
        $seenPaths = @()
        foreach ($file in $pending.files) {
            $fullPath = [IO.Path]::GetFullPath([string]$file.path)
            if ($expectedPaths -cnotcontains $fullPath -or $seenPaths -ccontains $fullPath -or
                $file.remoteName -cne [IO.Path]::GetFileName($fullPath)) { throw 'Invalid legacy publication file list.' }
            $seenPaths += $fullPath
            if ((Get-FileHash -LiteralPath $fullPath -Algorithm SHA256).Hash.ToLowerInvariant() -cne $file.sha256 -or
                (Get-Item -LiteralPath $fullPath).Length -ne $file.bytes) {
                throw 'A pending publication file changed on disk; refusing to upload different bytes.'
            }
        }
        Set-FFPendingPublicationIdentity -State $State
        $pending.files = @(Get-FFPublicationFiles -Options $Options -Pending $pending)
        Write-FFState -State $State -Path $StatePath
    }
    Set-FFPendingPublicationIdentity -State $State
    if ($pending.files.Count -ne 1 -or $pending.files[0].remoteName -cne 'FestaFiesta!.zip') {
        throw 'The pending publication must contain exactly one FestaFiesta!.zip.'
    }
    foreach ($file in $pending.files) {
        $expectedPath = Join-Path (Join-Path $Options.OutputRoot $pending.buildLabel) ($pending.buildLabel + '.zip')
        $fileFullPath = [IO.Path]::GetFullPath([string]$file.path)
        if ($fileFullPath -cne [IO.Path]::GetFullPath($expectedPath)) {
            throw 'The pending ZIP is outside its expected release path.'
        }
        Invoke-FFUploadFile -Options $Options -BuildLabel $pending.buildLabel -File $file -PublicationFolder $pending.publicationName
    }
    $State.published = [pscustomobject]@{
        commit = $pending.commit
        buildLabel = $pending.buildLabel
        publicationVersion = $pending.publicationVersion
        publicationName = $pending.publicationName
        publicationArchiveName = 'FestaFiesta!.zip'
        publishedAtUtc = [DateTimeOffset]::UtcNow.ToString('o')
        driveFolderId = $Options.DriveFolderId
    }
    $State.pending = $null
    Write-FFState -State $State -Path $StatePath
    Write-Host ('Published: {0}/{1} ({2})' -f $State.published.publicationName, $State.published.publicationArchiveName, $State.published.commit.Substring(0, 12))
}

function Invoke-FFDailyBuild {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string] $SourceRepository,
        [string] $SourceBranch = 'Dev',
        [Parameter(Mandatory = $true)][string] $CheckoutRoot,
        [string] $OutputRoot = 'C:\FestaFiestaBuild',
        [Parameter(Mandatory = $true)][string] $StateRoot,
        [Parameter(Mandatory = $true)][string] $EngineRoot,
        [Parameter(Mandatory = $true)][string] $RcloneExe,
        [Parameter(Mandatory = $true)][string] $RcloneConfig,
        [string] $RcloneRemote = 'festafiesta-drive',
        [ValidateSet('Rclone', 'Duck')][string] $UploadProvider = 'Rclone',
        [string] $DuckExe,
        [string] $DuckConfig,
        [string] $DriveFolderId = '13QQv-LeeTXIWG00bcQ9N9q-1I2vE_spL',
        [ValidateSet('Shipping', 'Development')][string] $Configuration = 'Shipping',
        [string] $BuildNumber = $env:BUILD_NUMBER,
        [switch] $BuildOnly,
        [switch] $Plan,
        [string] $CandidateCommit
    )
    if ($SourceRepository -notmatch '^https://[^\s]+$' -and $SourceRepository -notmatch '^git@[A-Za-z0-9.-]+:[A-Za-z0-9._/-]+$') {
        throw 'SourceRepository must be a credential-free HTTPS or git@host:path URL.'
    }
    if ($SourceRepository.StartsWith('https://', [StringComparison]::OrdinalIgnoreCase)) {
        $sourceUri = [Uri]$SourceRepository
        if ($sourceUri.UserInfo -or $sourceUri.Query -or $sourceUri.Fragment) { throw 'Do not put credentials or query parameters in SourceRepository.' }
    }
    if ($SourceBranch -notmatch '^[A-Za-z0-9][A-Za-z0-9._/-]{0,100}$' -or $SourceBranch.Contains('..') -or $SourceBranch.EndsWith('/')) {
        throw 'SourceBranch contains unsupported characters.'
    }
    if ($DriveFolderId -notmatch '^[A-Za-z0-9_-]{10,100}$' -or $RcloneRemote -notmatch '^[A-Za-z][A-Za-z0-9_-]{0,49}$') {
        throw 'Invalid Google Drive folder ID or rclone remote name.'
    }
    if ($CandidateCommit -and (-not $Plan -or $CandidateCommit -notmatch '^[a-f0-9]{40}([a-f0-9]{24})?$')) {
        throw 'CandidateCommit is a full lowercase SHA for offline -Plan only.'
    }
    $options = [pscustomobject]@{
        SourceRepository = $SourceRepository; SourceBranch = $SourceBranch
        CheckoutRoot = [IO.Path]::GetFullPath($CheckoutRoot).TrimEnd('\')
        OutputRoot = [IO.Path]::GetFullPath($OutputRoot).TrimEnd('\')
        StateRoot = [IO.Path]::GetFullPath($StateRoot).TrimEnd('\')
        EngineRoot = [IO.Path]::GetFullPath($EngineRoot).TrimEnd('\')
        RcloneExe = $RcloneExe; RcloneConfig = [IO.Path]::GetFullPath($RcloneConfig)
        RcloneRemote = $RcloneRemote; DriveFolderId = $DriveFolderId; Configuration = $Configuration
        UploadProvider = $UploadProvider; DuckExe = $DuckExe; DuckConfig = $DuckConfig
    }
    foreach ($path in @($options.CheckoutRoot, $options.OutputRoot, $options.StateRoot)) {
        if ($path.Length -le [IO.Path]::GetPathRoot($path).Length -or $path.Contains('!') -or $path -match '[\r\n]') {
            throw 'Use dedicated, non-root absolute paths without exclamation marks or newlines.'
        }
    }
    $paths = @($options.CheckoutRoot, $options.OutputRoot, $options.StateRoot)
    for ($first = 0; $first -lt $paths.Count; $first++) {
        for ($second = 0; $second -lt $paths.Count; $second++) {
            if ($first -ne $second -and ($paths[$first] -ieq $paths[$second] -or
                $paths[$first].StartsWith($paths[$second] + '\', [StringComparison]::OrdinalIgnoreCase))) {
                throw 'CheckoutRoot, OutputRoot, and StateRoot must be separate directories, without nesting.'
            }
        }
    }
    $binding = [pscustomobject]@{
        repository = $SourceRepository; branch = $SourceBranch; checkoutRoot = $options.CheckoutRoot
        outputRoot = $options.OutputRoot; driveFolderId = $DriveFolderId; rcloneRemote = $RcloneRemote; configuration = $Configuration
    }
    $statePath = Join-Path $options.StateRoot 'daily-build-state.json'
    if ($Plan) {
        $state = Read-FFState -Path $statePath -Binding $binding
        Initialize-FFPublicationSequence -State $state
        $planResult = Get-FFBuildPlan -State $state -CandidateCommit $CandidateCommit
        [pscustomobject]@{ mode = 'OfflinePlan'; branch = $SourceBranch; checkoutRoot = $options.CheckoutRoot
            outputRoot = $options.OutputRoot; statePath = $statePath; actions = $planResult.actions; buildOnly = [bool]$BuildOnly
            note = 'No files, credentials, remote repositories, or uploads were changed or contacted.' } | ConvertTo-Json -Depth 5
        return
    }
    # Validate the label components before doing any mutating work.
    $null = New-FFBuildLabel -ProjectVersion '0' -Commit ('0' * 40) -BuildNumber $BuildNumber
    if (-not $BuildOnly) {
        if ($UploadProvider -eq 'Rclone' -and -not (Test-Path -LiteralPath $RcloneConfig -PathType Leaf)) { throw 'The Jenkins rclone credential file is missing.' }
        if ($UploadProvider -eq 'Duck' -and (-not (Test-Path -LiteralPath $DuckExe -PathType Leaf) -or -not (Test-Path -LiteralPath $DuckConfig -PathType Leaf))) { throw 'The Jenkins Cyberduck executable or protected configuration is missing.' }
    }
    [IO.Directory]::CreateDirectory($options.StateRoot) | Out-Null
    $lock = $null
    $previousPrompt = [Environment]::GetEnvironmentVariable('GIT_TERMINAL_PROMPT', 'Process')
    try {
        try {
            $lock = New-Object IO.FileStream((Join-Path $options.StateRoot 'daily-build.lock'),
                [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
        }
        catch { throw 'Another daily-build process holds the state lock, or StateRoot is not writable.' }
        [Environment]::SetEnvironmentVariable('GIT_TERMINAL_PROMPT', '0', 'Process')
        $state = Read-FFState -Path $statePath -Binding $binding
        Initialize-FFPublicationSequence -State $state
        if ($null -ne $state.pending) {
            Update-FFPendingPublicationLayout -Options $options -State $state -StatePath $statePath
            if ($state.pending.phase -eq 'building') {
                # A crash may occur after Package-Win64 succeeds but before state is saved.
                $previousLabel = $state.pending.buildLabel
                $completedRoot = Join-Path $options.OutputRoot $previousLabel
                if (Test-Path -LiteralPath $completedRoot -PathType Container) {
                    Set-FFPendingPublicationIdentity -State $state
                    $state.pending.files = @(Get-FFPublicationFiles -Options $options -Pending $state.pending)
                    $state.pending.phase = 'ready'
                    Write-FFState -State $state -Path $statePath
                }
                else {
                    Write-Host ('Previous package attempt was interrupted before completion: {0}' -f $previousLabel)
                    $state.pending = $null
                    Write-FFState -State $state -Path $statePath
                }
            }
            if ($null -ne $state.pending) {
                if ($BuildOnly) {
                    Write-Host ('Package is ready and awaiting Drive authentication/publication: {0}' -f $state.pending.buildLabel)
                    return
                }
                Write-Host 'Resuming the pending publication before checking newer source.'
                Complete-FFPendingPublication -Options $options -State $state -StatePath $statePath
            }
        }
        $commit = Get-FFRemoteCommit -Options $options
        if ($null -ne $state.published -and $state.published.commit -ceq $commit) {
            Write-Host ('No source changes since the last verified Drive publication ({0}); skipped.' -f $commit.Substring(0, 12))
            return
        }
        Write-Host ('Building {0} at pinned commit {1}.' -f $SourceBranch, $commit.Substring(0, 12))
        Invoke-FFCheckout -Options $options -Commit $commit
        $versionMatches = @(Select-String -LiteralPath (Join-Path $options.CheckoutRoot 'Config\DefaultGame.ini') -Pattern '^ProjectVersion=(.+)$')
        if ($versionMatches.Count -ne 1) { throw 'Expected exactly one ProjectVersion in DefaultGame.ini.' }
        $label = New-FFBuildLabel -ProjectVersion $versionMatches[0].Matches[0].Groups[1].Value.Trim() -Commit $commit -BuildNumber $BuildNumber
        $state.pending = [pscustomobject]@{
            phase = 'building'; commit = $commit; buildLabel = $label
            startedAtUtc = [DateTimeOffset]::UtcNow.ToString('o'); files = @()
        }
        Set-FFPendingPublicationIdentity -State $state
        Write-FFState -State $state -Path $statePath
        Invoke-FFPackage -Options $options -BuildLabel $label
        $state.pending.files = @(Get-FFPublicationFiles -Options $options -Pending $state.pending)
        $state.pending.phase = 'ready'
        Write-FFState -State $state -Path $statePath
        if ($BuildOnly) {
            Write-Host ('Package completed; Drive publication is pending: {0}' -f $label)
            return
        }
        Complete-FFPendingPublication -Options $options -State $state -StatePath $statePath
    }
    finally {
        [Environment]::SetEnvironmentVariable('GIT_TERMINAL_PROMPT', $previousPrompt, 'Process')
        if ($null -ne $lock) { $lock.Dispose() }
    }
}

Export-ModuleMember -Function Invoke-FFDailyBuild, Get-FFBuildPlan, New-FFBuildLabel, Read-FFState, Write-FFState
