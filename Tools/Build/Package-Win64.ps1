# Package-Win64.ps1
# 역할: Windows 게임 패키지를 만들고 빌드·배포 검증을 묶어 실행한다.

[CmdletBinding()]
param(
    [string] $EngineRoot,

    [string] $ProjectFile,

    [string] $OutputRoot,

    [string] $BuildLabel,

    [ValidateSet('Shipping', 'Development')]
    [string] $Configuration = 'Shipping',

    [switch] $Incremental,

    [switch] $SkipTests,

    [switch] $SkipSmokeTest,

    # Requires a GPU/driver supporting both paths; tests real RHI startup, not gameplay.
    [switch] $GraphicsSmokeTest,

    [switch] $SkipZip,

    [ValidateLength(10, 500)]
    [string] $QualityOverrideReason
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Import-Module (Join-Path $PSScriptRoot 'FestaFiesta.Build.psm1') -Force
Set-FFProjectFileOverride -ProjectFile $ProjectFile

function Write-Utf8Json {
    param(
        [Parameter(Mandatory = $true)]
        [object] $Value,

        [Parameter(Mandatory = $true)]
        [string] $Path,

        [int] $Depth = 8,

        [switch] $Atomic
    )

    $json = $Value | ConvertTo-Json -Depth $Depth
    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $encoding = New-Object System.Text.UTF8Encoding($false)
    if (-not $Atomic) {
        [System.IO.File]::WriteAllText($fullPath, $json + [Environment]::NewLine, $encoding)
        return
    }

    # Keep the temporary file on the same volume: readers see either the old
    # complete index or the new complete index, even if publication fails.
    $temporaryPath = Join-Path ([System.IO.Path]::GetDirectoryName($fullPath)) (
        '.{0}.{1}.tmp' -f [System.IO.Path]::GetFileName($fullPath), [Guid]::NewGuid().ToString('N'))
    try {
        [System.IO.File]::WriteAllText($temporaryPath, $json + [Environment]::NewLine, $encoding)
        if ([System.IO.File]::Exists($fullPath)) {
            # PowerShell converts $null to an empty string for this overload;
            # NullString passes an actual null backup path on both 5.1 and 7.
            [System.IO.File]::Replace($temporaryPath, $fullPath, [NullString]::Value)
        }
        else {
            [System.IO.File]::Move($temporaryPath, $fullPath)
        }
    }
    finally {
        if ([System.IO.File]::Exists($temporaryPath)) {
            [System.IO.File]::Delete($temporaryPath)
        }
    }
}

function Move-ReleaseDirectory {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string] $OutputRoot,

        [Parameter(Mandatory = $true)]
        [string] $BuildLabel,

        [ValidateRange(1, 20)]
        [int] $MaximumAttempts = 8,

        [ValidateRange(1, 4000)]
        [int] $InitialDelayMilliseconds = 500
    )

    if ($BuildLabel -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]{0,119}$') {
        throw 'Release promotion requires a single valid build label.'
    }
    $outputFullPath = [System.IO.Path]::GetFullPath($OutputRoot)
    if ($outputFullPath.Length -gt [System.IO.Path]::GetPathRoot($outputFullPath).Length) {
        $outputFullPath = $outputFullPath.TrimEnd('\', '/')
    }
    $stagingFullPath = [System.IO.Path]::GetFullPath((Join-Path $outputFullPath '.staging'))
    $sourceFullPath = [System.IO.Path]::GetFullPath((Join-Path $stagingFullPath $BuildLabel))
    $destinationFullPath = [System.IO.Path]::GetFullPath((Join-Path $outputFullPath $BuildLabel))
    if ([System.IO.Path]::GetDirectoryName($destinationFullPath) -ine $outputFullPath -or
        [System.IO.Path]::GetDirectoryName($sourceFullPath) -ine $stagingFullPath) {
        throw 'Release promotion paths must remain directly inside the selected output/staging directories.'
    }

    $firstFailure = $null
    $delayMilliseconds = $InitialDelayMilliseconds
    for ($attempt = 1; $attempt -le $MaximumAttempts; $attempt++) {
        try {
            # Recheck on every attempt: never follow a replaced staging link or
            # overwrite a destination that appeared while waiting for a lock.
            foreach ($directoryPath in @($outputFullPath, $stagingFullPath, $sourceFullPath)) {
                $directory = Get-Item -LiteralPath $directoryPath -Force -ErrorAction Stop
                if (-not $directory.PSIsContainer -or
                    ($directory.Attributes -band [System.IO.FileAttributes]::ReparsePoint)) {
                    throw "Release promotion requires a real directory: $directoryPath"
                }
            }
            if ($null -ne (Get-Item -LiteralPath $destinationFullPath -Force -ErrorAction SilentlyContinue)) {
                throw "Release destination already exists; refusing to overwrite: $destinationFullPath"
            }
            [System.IO.Directory]::Move($sourceFullPath, $destinationFullPath)
            return
        }
        catch {
            if ($null -eq $firstFailure) { $firstFailure = $_ }
            $moveException = $_.Exception
            while ($null -ne $moveException.InnerException) { $moveException = $moveException.InnerException }
            $nativeError = $moveException.HResult -band 0xffff
            $mayBeTemporaryLock = ($moveException -is [System.IO.IOException] -or
                $moveException -is [System.UnauthorizedAccessException]) -and $nativeError -in @(5, 32, 33)
            if (-not $mayBeTemporaryLock -or $attempt -eq $MaximumAttempts) {
                # Keep the original exception/Win32 code, including when a later
                # safety check fails. The extra context does not replace it.
                $firstFailure.Exception.Data['ReleasePromotionAttempts'] = $attempt
                $firstFailure.Exception.Data['ReleasePromotionLastError'] = $_.Exception.Message
                $PSCmdlet.ThrowTerminatingError($firstFailure)
            }
            Write-Warning (("Release promotion attempt {0}/{1} failed (Windows {2}); retrying in {3} ms. " +
                'A temporary lock is possible; its owner has not been identified.') -f
                $attempt, $MaximumAttempts, $nativeError, $delayMilliseconds)
            Start-Sleep -Milliseconds $delayMilliseconds
            $delayMilliseconds = [Math]::Min($delayMilliseconds * 2, 4000)
        }
    }
}

function Get-GitSourcePaths {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Root,

        [Parameter(Mandatory = $true)]
        [ValidateSet('Untracked', 'TrackedChanges')]
        [string] $Kind
    )

    # Porcelain text quotes Unicode and special characters. Read Git's UTF-8
    # NUL-delimited output directly so the snapshot uses the actual filenames,
    # independent of the console encoding or core.quotepath setting.
    $fullRoot = [System.IO.Path]::GetFullPath($Root)
    $safeRoot = $fullRoot.Replace('\', '/')
    $gitArguments = if ($Kind -eq 'Untracked') {
        'ls-files --others --exclude-standard -z'
    }
    else {
        # Deleted paths remain in the patch; the archive holds files that exist.
        'diff --name-only --diff-filter=ACMRT -z HEAD'
    }
    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = 'git'
    $startInfo.Arguments = '-c "safe.directory={0}" -C "{1}" {2}' -f $safeRoot, $fullRoot, $gitArguments
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.StandardOutputEncoding = New-Object System.Text.UTF8Encoding($false)
    $startInfo.StandardErrorEncoding = New-Object System.Text.UTF8Encoding($false)
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $startInfo
    try {
        if (-not $process.Start()) {
            throw "Git could not start while enumerating $Kind source files."
        }
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        $stdout = $stdoutTask.GetAwaiter().GetResult()
        $stderr = $stderrTask.GetAwaiter().GetResult()
        if ($process.ExitCode -ne 0) {
            throw "Git $Kind file enumeration failed with exit code $($process.ExitCode): $stderr"
        }
        return $stdout.Split([char[]]@([char]0), [System.StringSplitOptions]::RemoveEmptyEntries)
    }
    finally {
        $process.Dispose()
    }
}

function Write-SourceArchive {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Root,

        [Parameter(Mandatory = $true)]
        [AllowEmptyCollection()]
        [string[]] $Paths,

        [Parameter(Mandatory = $true)]
        [string] $ArchivePath
    )

    if ($Paths.Count -eq 0) { return }
    $rootPrefix = [System.IO.Path]::GetFullPath($Root).TrimEnd('\') + '\'
    Add-Type -AssemblyName @('System.IO.Compression', 'System.IO.Compression.FileSystem')
    $zip = [System.IO.Compression.ZipFile]::Open($ArchivePath,
        [System.IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($relativePath in $Paths) {
            $absolutePath = [System.IO.Path]::GetFullPath((Join-Path $Root $relativePath))
            if (-not $absolutePath.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase) -or
                -not (Test-Path -LiteralPath $absolutePath -PathType Leaf)) {
                throw "Unsafe or missing source file: $relativePath"
            }
            # Store a normalized path, even if a caller supplied redundant segments.
            $entryPath = $absolutePath.Substring($rootPrefix.Length).Replace('\', '/')
            [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
                $zip, $absolutePath, $entryPath) | Out-Null
        }
    }
    finally { $zip.Dispose() }
}

function Resolve-PayloadRoot {
    param(
        [Parameter(Mandatory = $true)]
        [string] $ArchiveRoot
    )

    $launchers = @(Get-ChildItem -LiteralPath $ArchiveRoot -Recurse -File -Filter 'FestaFiesta.exe' |
        Sort-Object { $_.FullName.Length })
    foreach ($launcher in $launchers) {
        $candidateRoot = $launcher.Directory.FullName
        $gameExecutable = Join-Path $candidateRoot "FestaFiesta\Binaries\Win64\$gameBinaryBaseName.exe"
        $pakDirectory = Join-Path $candidateRoot 'FestaFiesta\Content\Paks'
        if ((Test-Path -LiteralPath $gameExecutable -PathType Leaf) -and
            (Test-Path -LiteralPath $pakDirectory -PathType Container)) {
            return $candidateRoot
        }
    }

    throw "UAT completed without producing a complete FestaFiesta payload below: $ArchiveRoot"
}

function Assert-LockedToolchainSelection {
    param(
        [Parameter(Mandatory = $true)]
        [string] $LogPath,

        [Parameter(Mandatory = $true)]
        [object] $Lock
    )

    $selections = @(Select-String -LiteralPath $LogPath -Pattern (
        'Using Visual Studio .*?([0-9]+\.[0-9]+\.[0-9]+) toolchain.*?Windows ([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+) SDK'))
    foreach ($selection in $selections) {
        if ($selection.Matches[0].Groups[1].Value -ne [string]$Lock.windows.msvcToolchain -or
            $selection.Matches[0].Groups[2].Value -ne [string]$Lock.windows.sdk) {
            throw "UBT selected a toolchain different from toolchain.lock.json: $($selection.Line)"
        }
    }
    return , $selections
}

$gameBinaryBaseName = Get-FFGameBinaryBaseName -Configuration $Configuration
$projectRoot = Get-FFProjectRoot
$projectFile = Get-FFProjectFile
$cookPathAudit = Assert-FFCookPathBudget
$projectDescriptor = Get-Content -LiteralPath $projectFile -Raw | ConvertFrom-Json
$resolvedEngineRoot = Resolve-FFEngineRoot -EngineRoot $EngineRoot
$engineVersion = Get-FFEngineVersion -EngineRoot $resolvedEngineRoot
$buildDdc = Get-FFBuildDdcConfiguration
$engineAssociation = [string]$projectDescriptor.EngineAssociation
$engineBuildFile = Join-Path $resolvedEngineRoot 'Engine\Build\Build.version'
$engineBuild = Get-Content -LiteralPath $engineBuildFile -Raw | ConvertFrom-Json
$toolchainLockFile = Join-Path $PSScriptRoot 'toolchain.lock.json'
$toolchainLock = Get-Content -LiteralPath $toolchainLockFile -Raw | ConvertFrom-Json
$hasQualityOverride = [bool]$SkipTests -or [bool]$SkipSmokeTest
if ($hasQualityOverride -and [string]::IsNullOrWhiteSpace($QualityOverrideReason)) {
    throw '-SkipTests or -SkipSmokeTest requires -QualityOverrideReason so the exception is auditable.'
}
if (-not $hasQualityOverride -and -not [string]::IsNullOrWhiteSpace($QualityOverrideReason)) {
    throw '-QualityOverrideReason is valid only with -SkipTests or -SkipSmokeTest.'
}

if (-not [string]::IsNullOrWhiteSpace($engineAssociation) -and
    -not $engineVersion.StartsWith("$engineAssociation.", [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Engine mismatch: the project requests $engineAssociation, but '$resolvedEngineRoot' is $engineVersion."
}
if ($engineVersion -ne [string]$toolchainLock.unrealEngine.version -or
    [int64]$engineBuild.Changelist -ne [int64]$toolchainLock.unrealEngine.changelist) {
    throw "Engine build does not match Tools/Build/toolchain.lock.json (expected $($toolchainLock.unrealEngine.version) CL $($toolchainLock.unrealEngine.changelist), found $engineVersion CL $($engineBuild.Changelist))."
}

$windowsKitsRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10'
$lockedSdkPath = Join-Path $windowsKitsRoot "Lib\$($toolchainLock.windows.sdk)"
if (-not (Test-Path -LiteralPath $lockedSdkPath -PathType Container)) {
    throw "Locked Windows SDK $($toolchainLock.windows.sdk) is not installed: $lockedSdkPath"
}

$lockedMsvcInstallation = Resolve-FFMsvcInstallation -InstallVersion ([string]$toolchainLock.windows.msvcInstall)
Write-Verbose "Locked MSVC installation: $lockedMsvcInstallation"

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $projectRoot 'Artifacts\Releases'
}
$OutputRoot = [System.IO.Path]::GetFullPath($OutputRoot)

$outputDriveRoot = [System.IO.Path]::GetPathRoot($OutputRoot)
$driveInfo = New-Object System.IO.DriveInfo($outputDriveRoot)
if ($driveInfo.AvailableFreeSpace -lt 10GB) {
    throw ('At least 10 GiB of free disk space is required. Available: {0:N2} GiB on {1}' -f
        ($driveInfo.AvailableFreeSpace / 1GB), $outputDriveRoot)
}

# A clean build deletes the project's editor DLLs; an open editor (or a game
# started from Binaries) holds them and fails UAT only after validation ran.
$projectFileName = [System.IO.Path]::GetFileName($projectFile)
$lockingProcesses = @(Get-CimInstance Win32_Process | Where-Object {
        ($_.Name -like 'UnrealEditor*.exe' -and [string]$_.CommandLine -like "*$projectFileName*") -or
        ($_.ExecutablePath -and $_.ExecutablePath.StartsWith($projectRoot, [System.StringComparison]::OrdinalIgnoreCase))
    })
if ($lockingProcesses.Count -gt 0) {
    $processText = ($lockingProcesses | ForEach-Object { "  - $($_.Name) (PID $($_.ProcessId))" }) -join [Environment]::NewLine
    throw "Close the editor/game using this project before packaging:$([Environment]::NewLine)$processText"
}

Assert-FFAssetsMaterialized
$gitLfsVersion = Assert-FFGitLfsHealth -MinimumVersion ([string]$toolchainLock.sourceControl.gitLfsMinimum)

$git = Get-FFGitMetadata
$projectVersion = Get-FFProjectVersion
$timestamp = [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')
if ([string]::IsNullOrWhiteSpace($BuildLabel)) {
    $dirtySuffix = if ($git.Dirty) { '-dirty' } else { '' }
    $BuildLabel = "FestaFiesta-Win64-$Configuration-$timestamp-$($git.Commit)$dirtySuffix"
}
if ($BuildLabel -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]{0,119}$') {
    throw 'BuildLabel must be 1-120 characters and contain only letters, digits, dot, underscore, or hyphen.'
}

$finalReleaseRoot = Join-Path $OutputRoot $BuildLabel
$stagingParent = Join-Path $OutputRoot '.staging'
$releaseRoot = Join-Path $stagingParent $BuildLabel
if ((Test-Path -LiteralPath $finalReleaseRoot) -or (Test-Path -LiteralPath $releaseRoot)) {
    throw "Release or staging output already exists; refusing to overwrite build label '$BuildLabel'."
}

# .NET ZipFile and several Windows tools stop at MAX_PATH (260). The deepest
# staged file is ~80 characters below the payload root (Engine ThirdParty DLLs),
# so fail before the long build instead of at the archive step.
$payloadPathBudget = 100
$deepestPayloadRoot = Join-Path $releaseRoot 'Package\Windows\'
if ($deepestPayloadRoot.Length + $payloadPathBudget -ge 260) {
    throw ("Output path is too deep for Windows MAX_PATH: '{0}' ({1} chars) leaves less than {2} characters for staged files. " +
        'Use a shorter -OutputRoot (for example C:\Builds\FestaFiesta) or a shorter -BuildLabel.') -f
        $deepestPayloadRoot, $deepestPayloadRoot.Length, $payloadPathBudget
}

$archiveRoot = Join-Path $releaseRoot 'Package'
$metadataRoot = Join-Path $releaseRoot 'Metadata'
$symbolsRoot = Join-Path $releaseRoot 'Symbols'
$logsRoot = Join-Path $releaseRoot 'Logs'
$validationRoot = Join-Path $releaseRoot 'Validation'
foreach ($directory in @($archiveRoot, $metadataRoot, $symbolsRoot, $logsRoot)) {
    [System.IO.Directory]::CreateDirectory($directory) | Out-Null
}

$startedAt = [DateTime]::UtcNow
$failureFile = Join-Path $metadataRoot 'failure.json'
$currentPhase = 'prerequisites'
$currentLog = $null

try {
    Write-Host "Release: $BuildLabel"
    Write-Host "Engine:  $resolvedEngineRoot ($engineVersion)"
    Write-Host "Configuration: $Configuration"
    Write-Host "Build DDC: $($buildDdc.Graph) ($($buildDdc.LocalPath))"
    Write-Host "Output:  $releaseRoot"
    Write-Host ('Mode:    {0}' -f $(if ($Incremental) { 'incremental' } else { 'clean release' }))

    if ($git.Dirty) {
        # A commit hash alone cannot rebuild a dirty artifact. Keep the exact
        # tracked delta and exact working file bytes beside the build metadata.
        $safeRoot = $projectRoot.Replace('\', '/')
        & git -c "safe.directory=$safeRoot" -C $projectRoot diff HEAD --binary `
            "--output=$(Join-Path $metadataRoot 'source-dirty.patch')"
        if ($LASTEXITCODE -ne 0) {
            throw "Git tracked-source snapshot failed with exit code $LASTEXITCODE."
        }
        $trackedPaths = @(Get-GitSourcePaths -Root $projectRoot -Kind TrackedChanges)
        # Git LFS diffs contain pointers, not the changed asset's bytes. Retain
        # the materialized files as well; the patch still records deletions.
        Write-SourceArchive -Root $projectRoot -Paths $trackedPaths `
            -ArchivePath (Join-Path $metadataRoot 'source-dirty-tracked.zip')
        $untrackedPaths = @(Get-GitSourcePaths -Root $projectRoot -Kind Untracked)
        $untracked = @($untrackedPaths | ForEach-Object { "?? $_" })
        [System.IO.File]::WriteAllLines(
            (Join-Path $metadataRoot 'source-dirty-untracked.txt'),
            [string[]]$untracked,
            (New-Object System.Text.UTF8Encoding($false)))
        # A patch cannot reconstruct files that have not been added to Git.
        # Keep their exact bytes beside the manifest so a dirty candidate can
        # be reproduced without silently omitting newly added code or scripts.
        Write-SourceArchive -Root $projectRoot -Paths $untrackedPaths `
            -ArchivePath (Join-Path $metadataRoot 'source-dirty-untracked.zip')
        Write-Warning 'Building from a dirty working tree; the patch and exact source files are stored in Metadata/source-dirty*.'
    }

    $currentPhase = 'validation'
    $currentLog = 'Validation'
    if (-not $SkipTests) {
        & (Join-Path $PSScriptRoot 'Run-AutomationTests.ps1') `
            -EngineRoot $resolvedEngineRoot `
            -ProjectFile $projectFile `
            -OutputRoot $validationRoot
    }
    else {
        # A test override never bypasses asset validation or the editor compile gate.
        & (Join-Path $PSScriptRoot 'Run-AutomationTests.ps1') `
            -EngineRoot $resolvedEngineRoot `
            -ProjectFile $projectFile `
            -OutputRoot $validationRoot `
            -TestFilter @()
    }
    # The editor build selects the same toolchain as the game build. Checking
    # its log here fails a drifted machine before the long clean BuildCookRun.
    $null = Assert-LockedToolchainSelection `
        -LogPath (Join-Path $validationRoot 'EditorBuild.log') -Lock $toolchainLock

    $runUat = Join-Path $resolvedEngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
    $uatLog = Join-Path $logsRoot 'BuildCookRun.log'
    $uatArguments = @(
        'BuildCookRun',
        "-project=$projectFile",
        $buildDdc.Argument,
        # UAT forwards -ddc to the cooker, but container compression has its
        # own UnrealPak process and requires explicit additional options.
        # Each child must also forward Log-level DDC selection evidence to
        # BuildCookRun.log; default Display-only stdout hides a valid backend.
        '-AdditionalCookerOptions=-FullStdOutLogOutput',
        "-AdditionalPakOptions=$($buildDdc.Argument) -FullStdOutLogOutput",
        "-AdditionalIoStoreOptions=$($buildDdc.Argument) -FullStdOutLogOutput",
        '-target=FestaFiesta',
        '-noP4',
        '-utf8output',
        '-unattended',
        '-WaitForUATMutex',
        '-platform=Win64',
        "-clientconfig=$Configuration",
        '-nodebuginfo',
        '-makebinaryconfig',
        '-build',
        '-cook',
        '-stage',
        '-package',
        '-pak',
        '-iostore',
        '-compressed',
        '-prereqs',
        '-archive',
        "-archivedirectory=$archiveRoot"
    )
    if (-not $Incremental) {
        $uatArguments += '-clean'
    }

    Write-Host 'Running Unreal AutomationTool BuildCookRun...'
    $currentPhase = 'build-cook-stage-package-archive'
    $currentLog = 'Logs/BuildCookRun.log'
    $uatExitCode = -1
    $savedErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & $runUat @uatArguments 2>&1 | Tee-Object -FilePath $uatLog
        $uatExitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $savedErrorActionPreference
    }
    if ($uatExitCode -ne 0) {
        throw "BuildCookRun failed with exit code $uatExitCode. See: $uatLog"
    }
    $selectedDdcPath = Assert-FFBuildDdcSelection -LogPath $uatLog

    # The cooker still succeeds when a cooked package hard-references content
    # it did not cook (NeverCook rules, editor-only targets). The player would
    # get a null reference at runtime, so treat it as a release blocker.
    $missingContent = @(Select-String -LiteralPath $uatLog -Pattern 'Content is missing from cook')
    if ($missingContent.Count -gt 0) {
        throw "The cook dropped $($missingContent.Count) referenced package(s): $($missingContent[0].Line.Trim()) See: $uatLog"
    }

    # Installation presence is not evidence that UBT selected that toolchain.
    # An up-to-date build may omit these lines; record unknown rather than
    # attributing the requested version to binaries without evidence.
    $toolchainSelections = Assert-LockedToolchainSelection -LogPath $uatLog -Lock $toolchainLock
    $selectedMsvc = if ($toolchainSelections.Count -gt 0) { $toolchainSelections[0].Matches[0].Groups[1].Value } else { $null }
    $selectedSdk = if ($toolchainSelections.Count -gt 0) { $toolchainSelections[0].Matches[0].Groups[2].Value } else { $null }

    $payloadRoot = Resolve-PayloadRoot -ArchiveRoot $archiveRoot

    # UAT stages both redistributable architectures. A Win64 payload only needs x64.
    $arm64Prerequisite = Join-Path $payloadRoot 'Engine\Extras\Redist\en-us\vc_redist.arm64.exe'
    if (Test-Path -LiteralPath $arm64Prerequisite -PathType Leaf) {
        Remove-Item -LiteralPath $arm64Prerequisite -Force
    }

    $licenseRoot = Join-Path $payloadRoot 'Licenses'
    [System.IO.Directory]::CreateDirectory($licenseRoot) | Out-Null
    Copy-Item -LiteralPath (Join-Path $projectRoot 'THIRD_PARTY_NOTICES.md') `
        -Destination (Join-Path $licenseRoot 'THIRD_PARTY_NOTICES.md')
    $readmeName = if ($Configuration -eq 'Development') { 'DEVELOPMENT_TEST_GUIDE_KO.md' } else { 'README_KO.md' }
    Copy-Item -LiteralPath (Join-Path $projectRoot "Distribution\$readmeName") `
        -Destination (Join-Path $payloadRoot 'README_KO.md')
    # UAT's standard bootstrap is the sole player-facing entry point. The engine
    # chooses the supported default RHI; QA can still pass explicit RHI options
    # to this same executable without distributing multiple launcher scripts.

    if ($hasQualityOverride) {
        $overrideNotice = @(
            'QUALITY-OVERRIDDEN BUILD - NOT APPROVED FOR PUBLIC RELEASE',
            '',
            "Build: $BuildLabel",
            "Reason: $QualityOverrideReason",
            '',
            'This artifact is for internal diagnosis and acceptance testing only.'
        ) -join [Environment]::NewLine
        $overrideNoticePath = Join-Path $metadataRoot 'QUALITY-OVERRIDE.txt'
        [System.IO.File]::WriteAllText(
            $overrideNoticePath,
            $overrideNotice + [Environment]::NewLine,
            (New-Object System.Text.UTF8Encoding($false)))
        Copy-Item -LiteralPath $overrideNoticePath -Destination (Join-Path $payloadRoot 'QUALITY-OVERRIDE.txt')
    }

    $gamePdb = Join-Path $projectRoot "Binaries\Win64\$gameBinaryBaseName.pdb"
    $gameTarget = Join-Path $projectRoot "Binaries\Win64\$gameBinaryBaseName.target"
    if (-not (Test-Path -LiteralPath $gamePdb -PathType Leaf)) {
        throw "$Configuration symbols were not generated: $gamePdb"
    }
    if (-not (Test-Path -LiteralPath $gameTarget -PathType Leaf)) {
        throw "$Configuration target receipt was not generated: $gameTarget"
    }
    $targetReceipt = Get-Content -LiteralPath $gameTarget -Raw | ConvertFrom-Json
    if ($targetReceipt.TargetName -ne 'FestaFiesta' -or $targetReceipt.Platform -ne 'Win64' -or
        $targetReceipt.Configuration -ne $Configuration -or
        [System.IO.Path]::GetFileName([string]$targetReceipt.Launch) -ne "$gameBinaryBaseName.exe") {
        throw "Target receipt does not describe the requested Win64 $Configuration executable: $gameTarget"
    }
    Copy-Item -LiteralPath $gamePdb -Destination $symbolsRoot
    Copy-Item -LiteralPath $gameTarget -Destination $symbolsRoot

    $gameExecutable = Join-Path $payloadRoot "FestaFiesta\Binaries\Win64\$gameBinaryBaseName.exe"
    $signature = Get-AuthenticodeSignature -LiteralPath $gameExecutable
    $containerFiles = @(Get-ChildItem -LiteralPath (Join-Path $payloadRoot 'FestaFiesta\Content\Paks') -File |
        Where-Object { $_.Extension -in @('.pak', '.utoc', '.ucas') } |
        Sort-Object Name |
        ForEach-Object {
            [ordered]@{
                path  = Get-FFRelativePath -BasePath $payloadRoot -Path $_.FullName
                bytes = $_.Length
            }
        })

    $manifest = [ordered]@{
        schemaVersion = 1
        releaseStatus = if ($hasQualityOverride) { 'quality-override' } else { 'validated' }
        product       = 'FestaFiesta'
        projectVersion = $projectVersion
        buildLabel    = $BuildLabel
        builtAtUtc    = [DateTime]::UtcNow.ToString('o')
        durationSeconds = [Math]::Round(([DateTime]::UtcNow - $startedAt).TotalSeconds, 3)
        source        = [ordered]@{
            branch = $git.Branch
            commit = $git.Commit
            dirty  = $git.Dirty
            projectDescriptorSha256 = (Get-FileHash -LiteralPath $projectFile -Algorithm SHA256).Hash.ToLowerInvariant()
        }
        engine        = [ordered]@{
            association = $engineAssociation
            version     = $engineVersion
            changelist  = [int64]$engineBuild.Changelist
            compatibleChangelist = [int64]$engineBuild.CompatibleChangelist
        }
        toolchain     = [ordered]@{
            windowsSdk = $selectedSdk
            msvc       = $selectedMsvc
            selectionEvidence = if ($toolchainSelections.Count -gt 0) { 'Logs/BuildCookRun.log' } else { 'not-reported-by-UBT' }
            requestedWindowsSdk = [string]$toolchainLock.windows.sdk
            requestedMsvc = [string]$toolchainLock.windows.msvcToolchain
            gitLfs     = $gitLfsVersion
            lockSha256 = (Get-FileHash -LiteralPath $toolchainLockFile -Algorithm SHA256).Hash.ToLowerInvariant()
        }
        target        = [ordered]@{
            platform      = 'Win64'
            configuration = $Configuration
            launcher      = 'FestaFiesta.exe'
            executable    = "FestaFiesta/Binaries/Win64/$gameBinaryBaseName.exe"
            cleanBuild    = -not [bool]$Incremental
            graphicsPaths = @('D3D11-SM5', 'D3D12-SM6')
            defaultRhi    = 'D3D12'
        }
        packaging     = [ordered]@{
            derivedDataCache = [ordered]@{
                graph = $buildDdc.Graph
                backend = 'FileSystem'
                projectRelativePath = 'DerivedDataCache/BuildFileSystem'
                selectedPath = $selectedDdcPath
                selectionEvidence = 'Logs/BuildCookRun.log'
            }
            pak            = $true
            ioStore        = $true
            compressed     = $true
            prerequisites  = @('vc_redist.x64.exe')
            crashReporter  = $false
            publicSymbols  = $false
            requiredMaps   = @(
                '/Game/FestaFiesta/Maps/Frontend',
                '/Game/FestaFiesta/Maps/Test_Main'
            )
            cultures      = @('en', 'ko')
        }
        validation    = [ordered]@{
            automationTests = -not [bool]$SkipTests
            dataValidation  = $true
            smokeTest      = -not [bool]$SkipSmokeTest
            graphicsStartupTest = [bool]$GraphicsSmokeTest
            defaultLauncherStartupTest = [bool]$GraphicsSmokeTest
            manualGameplayTest = 'not-recorded-by-this-script'
            cookPathAssetCount = $cookPathAudit.AssetCount
            longestCookPathLength = $cookPathAudit.LongestCookPathLength
            qualityOverride = $hasQualityOverride
            overrideReason = if ($hasQualityOverride) { $QualityOverrideReason } else { $null }
        }
        signing       = [ordered]@{
            authenticodeStatus = [string]$signature.Status
            signerSubject      = if ($null -ne $signature.SignerCertificate) {
                $signature.SignerCertificate.Subject
            }
            else {
                $null
            }
        }
        containers    = $containerFiles
    }

    $manifestPath = Join-Path $metadataRoot 'build-manifest.json'
    Write-Utf8Json -Value $manifest -Path $manifestPath
    Copy-Item -LiteralPath $manifestPath -Destination (Join-Path $payloadRoot 'build-manifest.json')

    $verifyArguments = @{
        PackagePath    = $archiveRoot
        Configuration  = $Configuration
        EngineRoot     = $resolvedEngineRoot
        ProjectFile    = $projectFile
        ReportDirectory = $metadataRoot
    }
    if (-not $SkipSmokeTest) {
        $verifyArguments.SmokeTest = $true
    }
    if ($GraphicsSmokeTest) {
        $verifyArguments.GraphicsSmokeTest = $true
    }
    $currentPhase = 'package-verification'
    $currentLog = 'Metadata'
    $verificationJson = & (Join-Path $PSScriptRoot 'Verify-Package.ps1') @verifyArguments
    [System.IO.File]::WriteAllLines(
        (Join-Path $metadataRoot 'verification.json'),
        [string[]]$verificationJson,
        (New-Object System.Text.UTF8Encoding($false)))

    Write-FFSha256Manifest `
        -RootPath $payloadRoot `
        -OutputFile (Join-Path $metadataRoot 'PACKAGE-SHA256SUMS.txt')
    Write-FFSha256Manifest `
        -RootPath $symbolsRoot `
        -OutputFile (Join-Path $metadataRoot 'SYMBOLS-SHA256SUMS.txt')

    $zipPath = $null
    if (-not $SkipZip) {
        $currentPhase = 'archive-compression'
        $currentLog = $null
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        $zipPath = Join-Path $releaseRoot "$BuildLabel.zip"
        [System.IO.Compression.ZipFile]::CreateFromDirectory(
            $payloadRoot,
            $zipPath,
            [System.IO.Compression.CompressionLevel]::Optimal,
            $false)
        $zipHash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
        [System.IO.File]::WriteAllText(
            (Join-Path $metadataRoot 'ARCHIVE-SHA256.txt'),
            "$zipHash  $([System.IO.Path]::GetFileName($zipPath))$([Environment]::NewLine)",
            (New-Object System.Text.UTF8Encoding($false)))
    }

    # Size is a release budget: record where the bytes go and the change
    # against the last promoted build so regressions are caught per release.
    $currentPhase = 'size-report'
    $previousSizeReport = $null
    $latestIndexPath = Join-Path $OutputRoot 'latest.json'
    if (Test-Path -LiteralPath $latestIndexPath -PathType Leaf) {
        $previousLatest = Get-Content -LiteralPath $latestIndexPath -Raw | ConvertFrom-Json
        $previousSizeReport = Join-Path $OutputRoot "$($previousLatest.releasePath)\Metadata\size-report.json"
    }
    $sizeReport = Write-FFSizeReport `
        -PayloadRoot $payloadRoot `
        -IoStoreCsv (Join-Path $metadataRoot 'IoStore-Contents.csv') `
        -OutputFile (Join-Path $metadataRoot 'size-report.json') `
        -ArchivePath $zipPath `
        -PreviousReport $previousSizeReport
    Write-Host ('Size:    payload {0:N1} MiB, containers {1:N1} MiB{2}' -f
        ($sizeReport.payloadBytes / 1MB),
        ($sizeReport.mainContainer.compressedBytes / 1MB),
        $(if ($null -ne $sizeReport.archiveBytes) { ', archive {0:N1} MiB' -f ($sizeReport.archiveBytes / 1MB) } else { '' }))
    if ($null -ne $sizeReport.comparedTo) {
        Write-Host ('         vs {0}: payload {1:+#,0;-#,0;0} bytes' -f
            $previousLatest.buildLabel, $sizeReport.comparedTo.payloadBytesDelta)
    }

    $latest = [ordered]@{
        schemaVersion = 1
        buildLabel    = $BuildLabel
        releasePath   = $BuildLabel
        archive       = if ($null -ne $zipPath) { [System.IO.Path]::GetFileName($zipPath) } else { $null }
        updatedAtUtc  = [DateTime]::UtcNow.ToString('o')
    }
    [System.IO.Directory]::CreateDirectory($OutputRoot) | Out-Null
    $currentPhase = 'release-promotion'
    $currentLog = 'Metadata/verification.json'
    Move-ReleaseDirectory -OutputRoot $OutputRoot -BuildLabel $BuildLabel

    $finalPayloadRoot = Join-Path $finalReleaseRoot (Get-FFRelativePath -BasePath $releaseRoot -Path $payloadRoot)
    $finalSymbolsRoot = Join-Path $finalReleaseRoot 'Symbols'
    $finalMetadataRoot = Join-Path $finalReleaseRoot 'Metadata'
    $finalVerificationPath = Join-Path $finalMetadataRoot 'verification.json'
    $verificationSummary = Get-Content -LiteralPath $finalVerificationPath -Raw | ConvertFrom-Json
    $verificationSummary.payloadRoot = ([string]$verificationSummary.payloadRoot).Replace(
        $releaseRoot, $finalReleaseRoot)
    $verificationSummary.reportDirectory = ([string]$verificationSummary.reportDirectory).Replace(
        $releaseRoot, $finalReleaseRoot)
    Write-Utf8Json -Value $verificationSummary -Path $finalVerificationPath
    $finalZipPath = if ($null -ne $zipPath) {
        Join-Path $finalReleaseRoot ([System.IO.Path]::GetFileName($zipPath))
    }
    else {
        $null
    }

    # Publish only after final metadata is complete. A failed promotion keeps
    # the previous successful build discoverable through an intact latest.json.
    if (-not $hasQualityOverride) {
        $currentLog = 'latest.json'
        Write-Utf8Json -Value $latest -Path (Join-Path $OutputRoot 'latest.json') -Atomic
    }

    Write-Host 'Release packaging completed successfully.' -ForegroundColor Green
    if ($hasQualityOverride) {
        Write-Warning 'This artifact has a recorded quality override and was not promoted to latest.json.'
    }
    Write-Host "Payload:  $finalPayloadRoot"
    if ($null -ne $finalZipPath) {
        Write-Host "Archive:  $finalZipPath"
    }
    Write-Host "Symbols:  $finalSymbolsRoot"
    Write-Host "Metadata: $finalMetadataRoot"
}
catch {
    $caughtError = $_
    $failure = [ordered]@{
        schemaVersion = 1
        buildLabel    = $BuildLabel
        failedAtUtc   = [DateTime]::UtcNow.ToString('o')
        phase         = $currentPhase
        message       = $caughtError.Exception.Message
        log           = $currentLog
    }
    $failureDestination = if (Test-Path -LiteralPath $finalReleaseRoot -PathType Container) {
        Join-Path $finalReleaseRoot 'Metadata\failure.json'
    }
    else {
        $failureFile
    }
    Write-Utf8Json -Value $failure -Path $failureDestination
    throw $caughtError
}
