# Optional Cyberduck CLI transport. Authentication stays in Cyberduck's native
# Windows Credential Manager integration; no OAuth tokens are read or exported.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Invoke-FFDuckNative {
    param([string] $FilePath, [string[]] $Arguments, [string] $Operation)
    $previousPreference = $ErrorActionPreference
    $exitCode = -1
    try {
        $ErrorActionPreference = 'Continue'
        $global:LASTEXITCODE = -1
        # Never expose protocol transcripts, authorization prompts, or account
        # diagnostics in the Jenkins console. Every argument is a separate value.
        $null = & $FilePath @Arguments 2>&1
        $exitCode = $LASTEXITCODE
    }
    catch { throw "$Operation could not launch Cyberduck CLI. Diagnostics were suppressed." }
    finally { $ErrorActionPreference = $previousPreference }
    if ($exitCode -ne 0) { throw "$Operation failed (exit $exitCode). Diagnostics were suppressed." }
}

function Assert-FFDuckDirectory {
    param([string] $Path)
    $item = Get-Item -LiteralPath $Path -Force -ErrorAction Stop
    if (-not $item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw 'Duck verification requires ordinary local directories, not links or files.'
    }
}

function Get-FFDuckSettings {
    param($Options)
    foreach ($name in @('DuckExe', 'DuckConfig', 'StateRoot', 'DriveFolderId')) {
        if (-not ($Options.PSObject.Properties.Name -contains $name) -or
            [string]::IsNullOrWhiteSpace([string]$Options.$name)) {
            throw "The Duck uploader requires Options.$name."
        }
    }
    if (-not [IO.Path]::IsPathRooted([string]$Options.DuckExe) -or
        -not (Test-Path -LiteralPath ([string]$Options.DuckExe) -PathType Leaf)) {
        throw 'DuckExe must identify the installed CLI executable by absolute path.'
    }
    if (-not [IO.Path]::IsPathRooted([string]$Options.DuckConfig) -or
        -not (Test-Path -LiteralPath ([string]$Options.DuckConfig) -PathType Leaf)) {
        throw 'The protected Duck configuration file is missing.'
    }
    if (-not [IO.Path]::IsPathRooted([string]$Options.StateRoot)) { throw 'Duck StateRoot must be an absolute path.' }
    $stateRoot = [IO.Path]::GetFullPath([string]$Options.StateRoot).TrimEnd('\')
    if ($stateRoot.Length -le [IO.Path]::GetPathRoot($stateRoot).Length) { throw 'Duck StateRoot must be a dedicated directory.' }
    Assert-FFDuckDirectory -Path $stateRoot
    $configuration = Get-Content -LiteralPath ([string]$Options.DuckConfig) -Raw | ConvertFrom-Json
    foreach ($name in @('username', 'rootUri', 'folderId', 'markerFile', 'markerSha256')) {
        if (-not ($configuration.PSObject.Properties.Name -contains $name) -or
            [string]::IsNullOrWhiteSpace([string]$configuration.$name)) {
            throw "The Duck configuration requires $name."
        }
    }
    if ([string]$configuration.username -notmatch '^[A-Za-z0-9][A-Za-z0-9._%+\-]{0,63}@[A-Za-z0-9][A-Za-z0-9.\-]{0,188}\.[A-Za-z]{2,63}$') {
        throw 'The Duck username must be the email used for Cyberduck desktop authentication.'
    }
    if ([string]$configuration.folderId -notmatch '^[A-Za-z0-9_-]{10,100}$' -or
        [string]$configuration.folderId -cne [string]$Options.DriveFolderId) {
        throw 'The Duck target folder ID does not match the requested Drive folder.'
    }
    if ([string]$configuration.markerFile -notmatch '^\.[A-Za-z0-9][A-Za-z0-9._-]{0,119}$' -or
        [string]$configuration.markerSha256 -notmatch '^[a-fA-F0-9]{64}$') {
        throw 'The Duck target requires a safe marker filename and its pinned SHA-256.'
    }
    $rootUri = [string]$configuration.rootUri
    if ($rootUri -cnotmatch '^googledrive:/[^?#\\\r\n]+$' -or $rootUri -match '%(?![a-fA-F0-9]{2})') {
        throw 'Duck rootUri must be a Google Drive path without query, fragment, or backslash.'
    }
    $encodedPath = $rootUri.Substring('googledrive:'.Length)
    # Reject encoded separators instead of allowing path interpretation to vary
    # between URI parsing and the provider's named-path lookup.
    if ($encodedPath.StartsWith('//') -or $encodedPath -match '%(2f|5c)') {
        throw 'An ambiguous root or encoded path separator is not allowed in Duck rootUri.'
    }
    $decodedPath = [Uri]::UnescapeDataString($encodedPath)
    if ($decodedPath -match '[%?#\\\x00-\x1f\x7f:]') { throw 'Duck rootUri contains unsafe path characters or double encoding.' }
    $segments = @($decodedPath.Trim('/').Split('/'))
    if ($segments.Count -lt 2 -or @($segments | Where-Object { $_ -in @('', '.', '..') }).Count -gt 0) {
        throw 'Duck rootUri must name a specific folder without traversal or empty path segments.'
    }
    # A unique marker downloaded before every upload binds this named path to
    # the original folder ID, including accounts with duplicate folder names.
    return [pscustomobject]@{
        username = [string]$configuration.username
        rootUri = $rootUri.TrimEnd('/')
        folderId = [string]$configuration.folderId
        markerFile = [string]$configuration.markerFile
        markerSha256 = ([string]$configuration.markerSha256).ToLowerInvariant()
        stateRoot = $stateRoot
        executable = [string]$Options.DuckExe
    }
}

function Get-FFDuckDownloadedHash {
    param([string] $Path, [string] $Description)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Description was not downloaded; refusing to accept an empty CLI result."
    }
    $item = Get-Item -LiteralPath $Path -Force
    if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "$Description must be an ordinary downloaded file." }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Invoke-FFDuckUploadFile {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)] $Options,
        [Parameter(Mandatory = $true)][string] $BuildLabel,
        [Parameter(Mandatory = $true)] $File
    )
    if ($BuildLabel -cnotmatch '^[A-Za-z0-9][A-Za-z0-9._-]{0,119}$') { throw 'Invalid Duck build label.' }
    foreach ($name in @('path', 'remoteName', 'sha256', 'bytes')) {
        if (-not ($File.PSObject.Properties.Name -contains $name)) { throw "The upload file requires $name." }
    }
    if ([string]$File.remoteName -cnotmatch '^[A-Za-z0-9][A-Za-z0-9._-]{0,239}$' -or
        [string]$File.remoteName -cne [IO.Path]::GetFileName([string]$File.path)) {
        throw 'The Duck artifact requires a safe single filename matching the source file.'
    }
    if (-not [IO.Path]::IsPathRooted([string]$File.path) -or
        -not (Test-Path -LiteralPath ([string]$File.path) -PathType Leaf)) {
        throw 'The Duck source file must exist at an absolute path.'
    }
    if ([string]$File.sha256 -notmatch '^[a-fA-F0-9]{64}$' -or [long]$File.bytes -lt 0) {
        throw 'The Duck artifact requires its expected SHA-256 and byte length.'
    }
    $settings = Get-FFDuckSettings -Options $Options
    $verificationRoot = Join-Path $settings.stateRoot 'duck-verification'
    if (-not (Test-Path -LiteralPath $verificationRoot)) { [IO.Directory]::CreateDirectory($verificationRoot) | Out-Null }
    Assert-FFDuckDirectory -Path $verificationRoot
    $temporaryRoot = Join-Path $verificationRoot ([Guid]::NewGuid().ToString('N'))
    if (Test-Path -LiteralPath $temporaryRoot) { throw 'The unique Duck verification directory already exists.' }
    [IO.Directory]::CreateDirectory($temporaryRoot) | Out-Null
    Assert-FFDuckDirectory -Path $temporaryRoot
    $markerPath = Join-Path $temporaryRoot 'target-marker.json'
    $readbackPath = Join-Path $temporaryRoot 'artifact-readback.bin'
    # Confirmed against the installed official CLI 9.5.4 --help. Keep its native
    # checksum verification enabled in addition to the SHA-256 readback below.
    $commonArguments = @('--username', $settings.username, '--assumeyes', '--quiet', '--retry', '3', '--existing', 'overwrite')
    $completed = $false
    try {
        $markerUri = $settings.rootUri + '/' + $settings.markerFile
        $null = Invoke-FFDuckNative -FilePath $settings.executable -Arguments (
            $commonArguments + @('--download', $markerUri, $markerPath)) -Operation 'Verify Duck target marker'
        $markerHash = Get-FFDuckDownloadedHash -Path $markerPath -Description 'The target marker'
        if ($markerHash -cne $settings.markerSha256) { throw 'The Duck target marker SHA-256 does not match the original Drive folder.' }
        # The pinned marker is also self-describing. Neither nonce nor marker
        # contents are logged or used as credentials.
        $marker = Get-Content -LiteralPath $markerPath -Raw | ConvertFrom-Json
        if (-not ($marker.PSObject.Properties.Name -contains 'folderId') -or
            [string]$marker.folderId -cne $settings.folderId) {
            throw 'The verified Duck marker does not identify the requested Drive folder.'
        }
        $artifactUri = $settings.rootUri + '/' + $BuildLabel + '/' + [string]$File.remoteName
        $uploadDirectoryUri = $settings.rootUri + '/' + $BuildLabel + '/'
        $directoryExists = $false
        try {
            $null = Invoke-FFDuckNative -FilePath $settings.executable -Arguments (
                $commonArguments + @('--list', $uploadDirectoryUri)) -Operation 'Inspect Duck build directory'
            $directoryExists = $true
        }
        catch {
            # CLI diagnostics deliberately remain suppressed. If this is a
            # permission/network failure, mkdir or its confirmation also fails
            # and no artifact is uploaded or recorded as published.
        }
        if (-not $directoryExists) {
            $null = Invoke-FFDuckNative -FilePath $settings.executable -Arguments (
                $commonArguments + @('--mkdir', $uploadDirectoryUri)) -Operation 'Create Duck build directory'
            $null = Invoke-FFDuckNative -FilePath $settings.executable -Arguments (
                $commonArguments + @('--list', $uploadDirectoryUri)) -Operation 'Confirm Duck build directory'
        }
        $null = Invoke-FFDuckNative -FilePath $settings.executable -Arguments (
            $commonArguments + @('--upload', $uploadDirectoryUri, [string]$File.path)) -Operation 'Upload build artifact with Duck'
        $null = Invoke-FFDuckNative -FilePath $settings.executable -Arguments (
            $commonArguments + @('--download', $artifactUri, $readbackPath)) -Operation 'Read back the uploaded Duck artifact'
        $actualHash = Get-FFDuckDownloadedHash -Path $readbackPath -Description 'The uploaded artifact'
        if ($actualHash -cne ([string]$File.sha256).ToLowerInvariant() -or
            (Get-Item -LiteralPath $readbackPath).Length -ne [long]$File.bytes) {
            throw 'Duck readback SHA-256 or byte length does not match the published artifact.'
        }
        $completed = $true
    }
    finally {
        $cleanupFailed = $false
        # Delete only the two known files in this invocation's GUID directory.
        # Leave empty directories; never recursively remove or move any path.
        foreach ($path in @($markerPath, $readbackPath)) {
            try { if ([IO.File]::Exists($path)) { [IO.File]::Delete($path) } }
            catch { $cleanupFailed = $true }
        }
        if ($cleanupFailed -and $completed) { throw 'Duck verification finished, but its temporary files could not be removed.' }
        if ($cleanupFailed -and -not $completed) { Write-Warning 'Duck temporary-file cleanup did not finish; the original publication failure remains authoritative.' }
    }
}

Export-ModuleMember -Function Invoke-FFDuckUploadFile
