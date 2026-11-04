# Offline transport fixtures. No executable, OAuth prompt, or network is invoked.
[CmdletBinding()]
param([string] $FixtureParent = [IO.Path]::GetTempPath(), [string] $CliHelpPath)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$module = Import-Module (Join-Path $PSScriptRoot 'FestaFiesta.Duck.psm1') -Force -PassThru
$script:Assertions = 0
function Assert-DuckTest {
    param([bool] $Condition, [string] $Message)
    if (-not $Condition) { throw ('Assertion failed: ' + $Message) }
    $script:Assertions++
}
function Assert-DuckThrows {
    param([scriptblock] $Action, [string] $Message, [string] $Expected = '')
    $caught = $null
    try { & $Action | Out-Null } catch { $caught = $_ }
    Assert-DuckTest ($null -ne $caught) $Message
    if ($Expected) { Assert-DuckTest ($caught.Exception.Message -like ('*' + $Expected + '*')) ($Message + ' (reason)') }
}
$fixtureRoot = Join-Path $FixtureParent ('ff-duck-test-' + [Guid]::NewGuid().ToString('N'))
$fixtureFullPath = [IO.Path]::GetFullPath($fixtureRoot)
$knownFiles = New-Object 'System.Collections.Generic.List[string]'
try {
    if ($CliHelpPath) {
        $cliHelp = Get-Content -LiteralPath $CliHelpPath -Raw
        $requiredHelpPatterns = @('--username\s+<username or access key>', '--assumeyes\s', '--quiet\s',
            '--retry\s+<count>', '--existing\s+<action>', '--upload\s+<url>\s+<file>', '--download\s+<url>\s+<file>',
            '--list\s+<url>', '--mkdir\s+<url>')
        $missingHelpPatterns = @($requiredHelpPatterns | Where-Object { $cliHelp -notmatch $_ })
        Assert-DuckTest ($missingHelpPatterns.Count -eq 0) 'The captured official CLI help supports every transport option and parameter shape.'
    }
    [IO.Directory]::CreateDirectory($fixtureRoot) | Out-Null
    $stateRoot = Join-Path $fixtureRoot 'state'
    [IO.Directory]::CreateDirectory($stateRoot) | Out-Null
    $duckPath = Join-Path $fixtureRoot 'fixture-duck.exe'
    $configPath = Join-Path $fixtureRoot 'duck.json'
    $artifactPath = Join-Path $fixtureRoot 'build.zip'
    foreach ($path in @($duckPath, $configPath, $artifactPath)) { $knownFiles.Add($path) }
    [IO.File]::WriteAllText($duckPath, 'Fixture only, never executed.')
    [IO.File]::WriteAllText($artifactPath, 'An immutable build artifact for transport verification.')
    $configuration = [pscustomobject]@{
        username = 'builder@example.com'; rootUri = 'googledrive:/My%20Drive/FestaFiesta%21'
        folderId = 'fixture_folder_id_12345'; markerFile = '.festafiesta-ci-target.json'; markerSha256 = ''
    }
    $markerText = [pscustomobject]@{ folderId = $configuration.folderId; nonce = [Guid]::NewGuid().ToString('N') } | ConvertTo-Json -Compress
    $markerBytes = [Text.Encoding]::UTF8.GetBytes($markerText)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $configuration.markerSha256 = ([BitConverter]::ToString($sha.ComputeHash($markerBytes))).Replace('-', '').ToLowerInvariant() }
    finally { $sha.Dispose() }
    $configuration | ConvertTo-Json | Set-Content -LiteralPath $configPath -Encoding UTF8
    $options = [pscustomobject]@{ DuckExe = $duckPath; DuckConfig = $configPath; StateRoot = $stateRoot; DriveFolderId = $configuration.folderId }
    $file = [pscustomobject]@{
        path = $artifactPath; remoteName = 'build.zip'; bytes = (Get-Item -LiteralPath $artifactPath).Length
        sha256 = (Get-FileHash -LiteralPath $artifactPath -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    $label = 'FF-v1.0-20261103-b1-12345678'
    $markerUri = $configuration.rootUri + '/' + $configuration.markerFile
    $artifactUri = $configuration.rootUri + '/' + $label + '/' + $file.remoteName
    $directoryUri = $configuration.rootUri + '/' + $label + '/'
    $harness = [pscustomobject]@{
        Events = (New-Object 'System.Collections.Generic.List[string]')
        Invocations = (New-Object 'System.Collections.Generic.List[object]')
        RemoteFiles = @{}; Directories = @{}; FailCreateDirectory = $false; FailDirectoryConfirmation = $false
        FailUpload = $false; OmitDownload = $false; OmitArtifactDownload = $false; TamperArtifact = $false
    }
    $harness.RemoteFiles[$markerUri] = $markerBytes
    & $module {
        param($Harness)
        $script:DuckFixture = $Harness
        function script:Invoke-FFDuckNative {
            param([string] $FilePath, [string[]] $Arguments, [string] $Operation)
            $script:DuckFixture.Invocations.Add([pscustomobject]@{ executable = $FilePath; arguments = $Arguments; operation = $Operation })
            $download = [Array]::IndexOf($Arguments, '--download')
            $upload = [Array]::IndexOf($Arguments, '--upload')
            $list = [Array]::IndexOf($Arguments, '--list')
            $mkdir = [Array]::IndexOf($Arguments, '--mkdir')
            if ($list -ge 0) {
                $uri = $Arguments[$list + 1]
                $script:DuckFixture.Events.Add('list:' + $uri)
                if ($script:DuckFixture.FailDirectoryConfirmation -or -not $script:DuckFixture.Directories.ContainsKey($uri)) {
                    throw 'Fixture remote directory list failed.'
                }
                return 'Directory listing output must not be forwarded.'
            }
            if ($mkdir -ge 0) {
                $uri = $Arguments[$mkdir + 1]
                $script:DuckFixture.Events.Add('mkdir:' + $uri)
                if ($script:DuckFixture.FailCreateDirectory) { throw 'Fixture directory creation failed.' }
                $script:DuckFixture.Directories[$uri] = $true
                return 'Directory creation output must not be forwarded.'
            }
            if ($download -ge 0) {
                $uri = $Arguments[$download + 1]; $path = $Arguments[$download + 2]
                $script:DuckFixture.Events.Add('download:' + $uri)
                if ($script:DuckFixture.OmitDownload) { return }
                if ($script:DuckFixture.OmitArtifactDownload -and $uri.EndsWith('.zip')) { return }
                if (-not $script:DuckFixture.RemoteFiles.ContainsKey($uri)) { throw 'Fixture remote download failed.' }
                $bytes = [byte[]]$script:DuckFixture.RemoteFiles[$uri]
                if ($script:DuckFixture.TamperArtifact -and $uri.EndsWith('.zip')) {
                    $bytes = [byte[]]$bytes.Clone()
                    $bytes[0] = $bytes[0] -bxor 1
                }
                [IO.File]::WriteAllBytes($path, $bytes)
                return 'Credential-bearing output must never be forwarded.'
            }
            if ($upload -ge 0) {
                $targetUri = $Arguments[$upload + 1]; $path = $Arguments[$upload + 2]
                if ($targetUri.EndsWith('/')) {
                    $directoryUri = $targetUri
                    $uri = $directoryUri + [IO.Path]::GetFileName($path)
                }
                else {
                    $uri = $targetUri
                    $directoryUri = $targetUri.Substring(0, $targetUri.LastIndexOf('/') + 1)
                }
                $script:DuckFixture.Events.Add('upload:' + $uri)
                if ($script:DuckFixture.FailUpload) { throw 'Fixture remote upload failed.' }
                if (-not $script:DuckFixture.Directories.ContainsKey($directoryUri)) { throw 'Fixture refuses an upload into a missing directory.' }
                $script:DuckFixture.RemoteFiles[$uri] = [IO.File]::ReadAllBytes($path)
                return 'Credential-bearing output must never be forwarded.'
            }
            throw 'Unexpected native invocation in the offline Duck fixture.'
        }
    } $harness

    $output = @(Invoke-FFDuckUploadFile -Options $options -BuildLabel $label -File $file)
    Assert-DuckTest ($output.Count -eq 0) 'Native authentication diagnostics never escape the adapter.'
    Assert-DuckTest ($harness.Events.Count -eq 6 -and $harness.Events[0] -eq ('download:' + $markerUri) -and
        $harness.Events[1] -eq ('list:' + $directoryUri) -and $harness.Events[2] -eq ('mkdir:' + $directoryUri) -and
        $harness.Events[3] -eq ('list:' + $directoryUri) -and $harness.Events[4] -eq ('upload:' + $artifactUri) -and
        $harness.Events[5] -eq ('download:' + $artifactUri)) 'A new directory is created and confirmed after the marker, then the artifact is uploaded and read back.'
    Assert-DuckTest ($harness.RemoteFiles.ContainsKey($artifactUri)) 'The artifact reaches the expected version directory.'
    foreach ($invocation in $harness.Invocations) {
        Assert-DuckTest ($invocation.arguments -contains '--assumeyes' -and $invocation.arguments -contains '--quiet' -and
            $invocation.arguments -notcontains '--verbose' -and $invocation.arguments -notcontains '--password' -and
            $invocation.arguments -notcontains '--nochecksum' -and $invocation.arguments -notcontains '--nokeychain') 'CLI calls use the native credential store and keep checksum verification enabled without exposing credentials.'
    }
    $harness.Events.Clear()
    Invoke-FFDuckUploadFile -Options $options -BuildLabel $label -File $file
    Assert-DuckTest ($harness.Events.Count -eq 4 -and $harness.Events -notcontains ('mkdir:' + $directoryUri)) 'Retries reuse an existing verified build directory without recreating it.'
    $harness.Events.Clear(); $harness.Directories.Clear(); $harness.FailCreateDirectory = $true
    Assert-DuckThrows { Invoke-FFDuckUploadFile -Options $options -BuildLabel $label -File $file } 'A failed remote directory creation prevents upload.' 'directory creation failed'
    Assert-DuckTest (@($harness.Events | Where-Object { $_ -like 'upload:*' }).Count -eq 0) 'No artifact is sent after mkdir fails.'
    $harness.FailCreateDirectory = $false; $harness.FailDirectoryConfirmation = $true; $harness.Events.Clear()
    Assert-DuckThrows { Invoke-FFDuckUploadFile -Options $options -BuildLabel $label -File $file } 'A created directory must still pass the confirmation listing.' 'directory list failed'
    Assert-DuckTest (@($harness.Events | Where-Object { $_ -like 'upload:*' }).Count -eq 0) 'No artifact is sent after directory confirmation fails.'
    $harness.FailDirectoryConfirmation = $false
    $harness.Events.Clear()
    $harness.RemoteFiles[$markerUri] = [Text.Encoding]::UTF8.GetBytes('{"folderId":"another_folder","nonce":"different"}')
    Assert-DuckThrows { Invoke-FFDuckUploadFile -Options $options -BuildLabel $label -File $file } 'A marker from another identically named folder is rejected.' 'marker SHA-256'
    Assert-DuckTest ($harness.Events.Count -eq 1 -and $harness.Events[0] -like 'download:*') 'A bad marker prevents every upload.'
    $wrongFolderMarker = [Text.Encoding]::UTF8.GetBytes('{"folderId":"different_folder_123","nonce":"fixture-other-folder"}')
    $harness.RemoteFiles[$markerUri] = $wrongFolderMarker
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $wrongFolderHash = ([BitConverter]::ToString($sha.ComputeHash($wrongFolderMarker))).Replace('-', '').ToLowerInvariant() }
    finally { $sha.Dispose() }
    $wrongFolderConfig = $configuration.PSObject.Copy(); $wrongFolderConfig.markerSha256 = $wrongFolderHash
    $wrongFolderConfig | ConvertTo-Json | Set-Content -LiteralPath $configPath -Encoding UTF8
    Assert-DuckThrows { Invoke-FFDuckUploadFile -Options $options -BuildLabel $label -File $file } 'A marker with a matching hash must still identify the requested folder.' 'does not identify'
    $configuration | ConvertTo-Json | Set-Content -LiteralPath $configPath -Encoding UTF8
    $harness.RemoteFiles[$markerUri] = $markerBytes
    $harness.Events.Clear(); $harness.FailUpload = $true
    Assert-DuckThrows { Invoke-FFDuckUploadFile -Options $options -BuildLabel $label -File $file } 'Upload errors fail closed.' 'upload failed'
    Assert-DuckTest ($harness.Events.Count -eq 3 -and $harness.Events[2] -eq ('upload:' + $artifactUri)) 'An upload failure cannot become a readback success.'
    $harness.FailUpload = $false; $harness.TamperArtifact = $true
    Assert-DuckThrows { Invoke-FFDuckUploadFile -Options $options -BuildLabel $label -File $file } 'A same-length remote content change is detected.' 'readback SHA-256'
    $harness.TamperArtifact = $false; $harness.OmitDownload = $true
    Assert-DuckThrows { Invoke-FFDuckUploadFile -Options $options -BuildLabel $label -File $file } 'Exit success without a downloaded marker is rejected.' 'not downloaded'
    $harness.OmitDownload = $false
    $harness.OmitArtifactDownload = $true
    Assert-DuckThrows { Invoke-FFDuckUploadFile -Options $options -BuildLabel $label -File $file } 'Exit success without the artifact readback is rejected.' 'not downloaded'
    $harness.OmitArtifactDownload = $false
    $wrongLengthFile = $file.PSObject.Copy(); $wrongLengthFile.bytes++
    Assert-DuckThrows { Invoke-FFDuckUploadFile -Options $options -BuildLabel $label -File $wrongLengthFile } 'Correct readback bytes cannot satisfy an incorrect expected length.' 'byte length'

    $rootFile = $file.PSObject.Copy(); $rootFile.remoteName = 'V24(2026.11.03).zip'
    $rootArtifactUri = $configuration.rootUri + '/V24%282026.11.03%29.zip'
    $harness.Directories[$configuration.rootUri + '/'] = $true
    $harness.Events.Clear(); $harness.Invocations.Clear()
    Invoke-FFDuckUploadFile -Options $options -BuildLabel $label -File $rootFile -RootFile
    Assert-DuckTest ($harness.Events.Count -eq 3 -and $harness.Events[0] -eq ('download:' + $markerUri) -and
        $harness.Events[1] -eq ('upload:' + $rootArtifactUri) -and
        $harness.Events[2] -eq ('download:' + $rootArtifactUri)) 'A root ZIP is sent directly after marker verification, with no build directory listing or creation.'
    Assert-DuckTest ($harness.RemoteFiles.ContainsKey($rootArtifactUri)) 'The versioned ZIP is stored directly in the target root.'
    $rootUpload = @($harness.Invocations | Where-Object { $_.arguments -contains '--upload' })[0]
    Assert-DuckTest ($rootUpload.arguments -contains $artifactPath -and $rootUpload.arguments -contains $rootArtifactUri) 'The exact remote file URI renames the upload without copying or renaming the local source ZIP.'
    Assert-DuckTest ($file.remoteName -ceq 'build.zip' -and (Get-Item -LiteralPath $artifactPath).Name -ceq 'build.zip') 'Root publication preserves the original local archive name.'
    $maxVersionFile = $rootFile.PSObject.Copy(); $maxVersionFile.remoteName = 'V2147483647(2026.11.03).zip'
    Invoke-FFDuckUploadFile -Options $options -BuildLabel $label -File $maxVersionFile -RootFile
    Assert-DuckTest ($harness.RemoteFiles.ContainsKey($configuration.rootUri + '/V2147483647%282026.11.03%29.zip')) 'The maximum publication version supported by the main pipeline can be uploaded.'
    $harness.TamperArtifact = $true
    Assert-DuckThrows { Invoke-FFDuckUploadFile -Options $options -BuildLabel $label -File $rootFile -RootFile } 'Root ZIP readback still detects content corruption.' 'readback SHA-256'
    $harness.TamperArtifact = $false; $harness.OmitArtifactDownload = $true
    Assert-DuckThrows { Invoke-FFDuckUploadFile -Options $options -BuildLabel $label -File $rootFile -RootFile } 'Root ZIP readback cannot succeed when the download is missing.' 'not downloaded'
    $harness.OmitArtifactDownload = $false
    $priorCalls = $harness.Invocations.Count
    foreach ($unsafeName in @('../V24(2026.11.03).zip', 'V24(2026.11.03)/build.zip',
        'V24%282026.11.03%29.zip', 'V24(2026.11.03).json', 'V0(2026.11.03).zip', 'V24(2026.02.30).zip',
        'V2147483648(2026.11.03).zip', 'V9999999999(2026.11.03).zip', 'V10000000000(2026.11.03).zip')) {
        $unsafeRootFile = $rootFile.PSObject.Copy(); $unsafeRootFile.remoteName = $unsafeName
        Assert-DuckThrows { Invoke-FFDuckUploadFile -Options $options -BuildLabel $label -File $unsafeRootFile -RootFile } 'Root publication refuses unsafe filenames and invalid dates.'
    }
    Assert-DuckTest ($harness.Invocations.Count -eq $priorCalls) 'Invalid root publication names fail before any native call.'
    Assert-DuckThrows { Invoke-FFDuckUploadFile -Options $options -BuildLabel $label -File $rootFile } 'Versioned basename remapping is enabled only by explicit root publication mode.' 'single filename'

    $folderFile = $file.PSObject.Copy(); $folderFile.remoteName = 'FestaFiesta!.zip'
    $publicationFolder = 'V24(2026.11.03.18:44)'
    $publicationDirectoryUri = $configuration.rootUri + '/V24%282026.11.03.18%3A44%29/'
    $publicationArtifactUri = $publicationDirectoryUri + 'FestaFiesta%21.zip'
    $harness.Events.Clear(); $harness.Invocations.Clear()
    Invoke-FFDuckUploadFile -Options $options -BuildLabel $label -File $folderFile -PublicationFolder $publicationFolder
    Assert-DuckTest ($harness.Events.Count -eq 6 -and $harness.Events[0] -eq ('download:' + $markerUri) -and
        $harness.Events[1] -eq ('list:' + $publicationDirectoryUri) -and
        $harness.Events[2] -eq ('mkdir:' + $publicationDirectoryUri) -and
        $harness.Events[3] -eq ('list:' + $publicationDirectoryUri) -and
        $harness.Events[4] -eq ('upload:' + $publicationArtifactUri) -and
        $harness.Events[5] -eq ('download:' + $publicationArtifactUri)) 'Version folders are created and confirmed after marker verification, then the renamed ZIP is uploaded and read back.'
    $folderArtifacts = @($harness.RemoteFiles.Keys | Where-Object { $_.StartsWith($publicationDirectoryUri, [StringComparison]::Ordinal) })
    Assert-DuckTest ($folderArtifacts.Count -eq 1 -and $folderArtifacts[0] -ceq $publicationArtifactUri) 'The version folder receives exactly one FestaFiesta!.zip archive.'
    $folderUpload = @($harness.Invocations | Where-Object { $_.arguments -contains '--upload' })[0]
    Assert-DuckTest ($folderUpload.arguments -contains $artifactPath -and $folderUpload.arguments -contains $publicationArtifactUri) 'An exact destination file URI maps the original local ZIP to the fixed archive name.'
    $harness.Events.Clear()
    Invoke-FFDuckUploadFile -Options $options -BuildLabel $label -File $folderFile -PublicationFolder $publicationFolder
    Assert-DuckTest ($harness.Events.Count -eq 4 -and $harness.Events -notcontains ('mkdir:' + $publicationDirectoryUri)) 'A version folder retry reuses its existing directory.'
    $harness.Directories.Remove($publicationDirectoryUri); $harness.Events.Clear(); $harness.FailCreateDirectory = $true
    Assert-DuckThrows { Invoke-FFDuckUploadFile -Options $options -BuildLabel $label -File $folderFile -PublicationFolder $publicationFolder } 'A failed version-folder creation prevents uploading the archive.' 'directory creation failed'
    Assert-DuckTest (@($harness.Events | Where-Object { $_ -like 'upload:*' }).Count -eq 0) 'No version archive is uploaded when folder creation fails.'
    $harness.FailCreateDirectory = $false; $harness.FailDirectoryConfirmation = $true; $harness.Events.Clear()
    Assert-DuckThrows { Invoke-FFDuckUploadFile -Options $options -BuildLabel $label -File $folderFile -PublicationFolder $publicationFolder } 'A new version folder must pass confirmation before upload.' 'directory list failed'
    Assert-DuckTest (@($harness.Events | Where-Object { $_ -like 'upload:*' }).Count -eq 0) 'An unconfirmed version folder receives no archive.'
    $harness.FailDirectoryConfirmation = $false; $harness.TamperArtifact = $true
    Assert-DuckThrows { Invoke-FFDuckUploadFile -Options $options -BuildLabel $label -File $folderFile -PublicationFolder $publicationFolder } 'The fixed archive filename still receives full SHA-256 verification.' 'readback SHA-256'
    $harness.TamperArtifact = $false; $harness.OmitArtifactDownload = $true
    Assert-DuckThrows { Invoke-FFDuckUploadFile -Options $options -BuildLabel $label -File $folderFile -PublicationFolder $publicationFolder } 'A missing version archive readback fails publication.' 'not downloaded'
    $harness.OmitArtifactDownload = $false
    Invoke-FFDuckUploadFile -Options $options -BuildLabel $label -File $folderFile -PublicationFolder 'V2147483647(2026.11.03.18:44)'
    Assert-DuckTest ($harness.RemoteFiles.ContainsKey($configuration.rootUri + '/V2147483647%282026.11.03.18%3A44%29/FestaFiesta%21.zip')) 'Version-folder mode accepts the same maximum version as the main pipeline.'
    $priorCalls = $harness.Invocations.Count
    foreach ($unsafeFolder in @('../V24(2026.11.03.18:44)', 'V24(2026.11.03.18:44)/child', 'V24%282026.11.03.18%3A44%29',
        'V24(2026.11.03.18:44).zip', 'V0(2026.11.03.18:44)', 'V24(2026.02.30.18:44)', 'V2147483648(2026.11.03.18:44)',
        'V24(2026.11.03.24:00)', 'V24(2026.11.03.18:60)', 'V24(2026.11.03)', '')) {
        Assert-DuckThrows { Invoke-FFDuckUploadFile -Options $options -BuildLabel $label -File $folderFile -PublicationFolder $unsafeFolder } 'Unsafe version folders and invalid calendar dates are refused.'
    }
    Assert-DuckThrows { Invoke-FFDuckUploadFile -Options $options -BuildLabel $label -File $folderFile -PublicationFolder $publicationFolder -RootFile } 'Ambiguous root and folder publication modes are refused.' 'either root'
    $wrongArchiveFile = $folderFile.PSObject.Copy(); $wrongArchiveFile.remoteName = 'other.zip'
    Assert-DuckThrows { Invoke-FFDuckUploadFile -Options $options -BuildLabel $label -File $wrongArchiveFile -PublicationFolder $publicationFolder } 'Version folders accept only the fixed FestaFiesta!.zip filename.' 'FestaFiesta!.zip'
    Assert-DuckTest ($harness.Invocations.Count -eq $priorCalls) 'Invalid version-folder requests fail before native calls.'

    $changedOptions = $options.PSObject.Copy(); $changedOptions.DriveFolderId = 'different_requested_folder'
    $priorCalls = $harness.Invocations.Count
    Assert-DuckThrows { Invoke-FFDuckUploadFile -Options $changedOptions -BuildLabel $label -File $file } 'A configuration bound to another Drive folder is refused.' 'folder ID'
    Assert-DuckTest ($harness.Invocations.Count -eq $priorCalls) 'Destination configuration mismatches cause no native command.'
    foreach ($unsafeRoot in @('googledrive:/My%20Drive/../Wrong', 'googledrive:/My%20Drive/%2e%2e/Wrong',
        'googledrive:/My%20Drive/%252e%252e/Wrong', 'googledrive:/My%20Drive/Foo%2FBar',
        'googledrive:/My%20Drive/Folder?token=secret', 'googledrive:/My%20Drive//Wrong', 'googledrive://My%20Drive/Folder', 'googledrive:/')) {
        $unsafeConfig = $configuration.PSObject.Copy(); $unsafeConfig.rootUri = $unsafeRoot
        $unsafeConfig | ConvertTo-Json | Set-Content -LiteralPath $configPath -Encoding UTF8
        Assert-DuckThrows { Invoke-FFDuckUploadFile -Options $options -BuildLabel $label -File $file } 'Unsafe root path is refused before executing the CLI.'
    }
    $configuration | ConvertTo-Json | Set-Content -LiteralPath $configPath -Encoding UTF8
    Assert-DuckThrows { Invoke-FFDuckUploadFile -Options $options -BuildLabel '../wrong' -File $file } 'Build-label traversal is refused.' 'build label'
    $unsafeFile = $file.PSObject.Copy(); $unsafeFile.remoteName = '../build.zip'
    Assert-DuckThrows { Invoke-FFDuckUploadFile -Options $options -BuildLabel $label -File $unsafeFile } 'Artifact-name traversal is refused.' 'single filename'
    $unsafeConfig = $configuration.PSObject.Copy(); $unsafeConfig.markerFile = '../target.json'
    $unsafeConfig | ConvertTo-Json | Set-Content -LiteralPath $configPath -Encoding UTF8
    Assert-DuckThrows { Invoke-FFDuckUploadFile -Options $options -BuildLabel $label -File $file } 'Marker-name traversal is refused.' 'safe marker filename'
    $configuration | ConvertTo-Json | Set-Content -LiteralPath $configPath -Encoding UTF8
    $verificationRoot = Join-Path $stateRoot 'duck-verification'
    Assert-DuckTest (@(Get-ChildItem -LiteralPath $verificationRoot -File -Recurse).Count -eq 0) 'Success and failure clean only their known temporary files.'
    Assert-DuckTest ((Get-FileHash -LiteralPath $artifactPath -Algorithm SHA256).Hash.ToLowerInvariant() -eq $file.sha256) 'The source artifact remains unchanged.'
    Write-Host ('PASS: {0} offline Duck assertions; no executable or network was used.' -f $script:Assertions) -ForegroundColor Green
}
finally {
    Remove-Module FestaFiesta.Duck -Force -ErrorAction SilentlyContinue
    # Only this fixture's explicitly known files are deleted. Empty directories
    # are retained so cleanup never recursively deletes a path.
    $expectedParent = [IO.Path]::GetFullPath($FixtureParent).TrimEnd('\') + '\'
    if ($fixtureFullPath.StartsWith($expectedParent, [StringComparison]::OrdinalIgnoreCase) -and
        [IO.Path]::GetFileName($fixtureFullPath) -match '^ff-duck-test-[a-f0-9]{32}$') {
        foreach ($path in $knownFiles) { if ([IO.File]::Exists($path)) { [IO.File]::Delete($path) } }
    }
}
