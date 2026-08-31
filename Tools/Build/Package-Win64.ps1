[CmdletBinding()]
param(
    [string] $EngineRoot,

    [string] $OutputRoot,

    [string] $BuildLabel,

    [switch] $Incremental,

    [switch] $SkipTests,

    [switch] $SkipSmokeTest,

    [switch] $SkipZip,

    [ValidateLength(10, 500)]
    [string] $QualityOverrideReason
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Import-Module (Join-Path $PSScriptRoot 'FestaFiesta.Build.psm1') -Force

function Write-Utf8Json {
    param(
        [Parameter(Mandatory = $true)]
        [object] $Value,

        [Parameter(Mandatory = $true)]
        [string] $Path,

        [int] $Depth = 8
    )

    $json = $Value | ConvertTo-Json -Depth $Depth
    [System.IO.File]::WriteAllText(
        [System.IO.Path]::GetFullPath($Path),
        $json + [Environment]::NewLine,
        (New-Object System.Text.UTF8Encoding($false)))
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
        $shippingExecutable = Join-Path $candidateRoot 'FestaFiesta\Binaries\Win64\FestaFiesta-Win64-Shipping.exe'
        $pakDirectory = Join-Path $candidateRoot 'FestaFiesta\Content\Paks'
        if ((Test-Path -LiteralPath $shippingExecutable -PathType Leaf) -and
            (Test-Path -LiteralPath $pakDirectory -PathType Container)) {
            return $candidateRoot
        }
    }

    throw "UAT completed without producing a complete FestaFiesta payload below: $ArchiveRoot"
}

$projectRoot = Get-FFProjectRoot
$projectFile = Get-FFProjectFile
$projectDescriptor = Get-Content -LiteralPath $projectFile -Raw | ConvertFrom-Json
$resolvedEngineRoot = Resolve-FFEngineRoot -EngineRoot $EngineRoot
$engineVersion = Get-FFEngineVersion -EngineRoot $resolvedEngineRoot
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

$visualStudioRoot = Join-Path $env:ProgramFiles 'Microsoft Visual Studio\2022'
$lockedMsvcPaths = @(Get-ChildItem -LiteralPath $visualStudioRoot -Directory -ErrorAction SilentlyContinue |
    ForEach-Object { Join-Path $_.FullName "VC\Tools\MSVC\$($toolchainLock.windows.msvcInstall)" } |
    Where-Object { Test-Path -LiteralPath $_ -PathType Container })
if ($lockedMsvcPaths.Count -eq 0) {
    throw "Locked MSVC toolchain $($toolchainLock.windows.msvcToolchain) is not installed below: $visualStudioRoot"
}

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

Assert-FFAssetsMaterialized
$gitLfsVersion = Assert-FFGitLfsHealth -ExpectedVersion ([string]$toolchainLock.sourceControl.gitLfs)

$git = Get-FFGitMetadata
$projectVersion = Get-FFProjectVersion
$timestamp = [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')
if ([string]::IsNullOrWhiteSpace($BuildLabel)) {
    $dirtySuffix = if ($git.Dirty) { '-dirty' } else { '' }
    $BuildLabel = "FestaFiesta-Win64-Shipping-$timestamp-$($git.Commit)$dirtySuffix"
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
    Write-Host "Output:  $releaseRoot"
    Write-Host ('Mode:    {0}' -f $(if ($Incremental) { 'incremental' } else { 'clean release' }))

    $currentPhase = 'validation'
    $currentLog = 'Validation'
    if (-not $SkipTests) {
        & (Join-Path $PSScriptRoot 'Run-AutomationTests.ps1') `
            -EngineRoot $resolvedEngineRoot `
            -OutputRoot $validationRoot
    }
    else {
        # A test override never bypasses asset validation or the editor compile gate.
        & (Join-Path $PSScriptRoot 'Run-AutomationTests.ps1') `
            -EngineRoot $resolvedEngineRoot `
            -OutputRoot $validationRoot `
            -TestFilter @()
    }

    $runUat = Join-Path $resolvedEngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
    $uatLog = Join-Path $logsRoot 'BuildCookRun.log'
    $uatArguments = @(
        'BuildCookRun',
        "-project=$projectFile",
        '-target=FestaFiesta',
        '-noP4',
        '-utf8output',
        '-unattended',
        '-WaitForUATMutex',
        '-platform=Win64',
        '-clientconfig=Shipping',
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
    Copy-Item -LiteralPath (Join-Path $projectRoot 'Distribution\README_KO.md') `
        -Destination (Join-Path $payloadRoot 'README_KO.md')

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

    $shippingPdb = Join-Path $projectRoot 'Binaries\Win64\FestaFiesta-Win64-Shipping.pdb'
    $shippingTarget = Join-Path $projectRoot 'Binaries\Win64\FestaFiesta-Win64-Shipping.target'
    if (-not (Test-Path -LiteralPath $shippingPdb -PathType Leaf)) {
        throw "Shipping symbols were not generated: $shippingPdb"
    }
    Copy-Item -LiteralPath $shippingPdb -Destination $symbolsRoot
    if (Test-Path -LiteralPath $shippingTarget -PathType Leaf) {
        Copy-Item -LiteralPath $shippingTarget -Destination $symbolsRoot
    }

    $shippingExecutable = Join-Path $payloadRoot 'FestaFiesta\Binaries\Win64\FestaFiesta-Win64-Shipping.exe'
    $signature = Get-AuthenticodeSignature -LiteralPath $shippingExecutable
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
        }
        engine        = [ordered]@{
            association = $engineAssociation
            version     = $engineVersion
            changelist  = [int64]$engineBuild.Changelist
            compatibleChangelist = [int64]$engineBuild.CompatibleChangelist
        }
        toolchain     = [ordered]@{
            windowsSdk = [string]$toolchainLock.windows.sdk
            msvc       = [string]$toolchainLock.windows.msvcToolchain
            gitLfs     = $gitLfsVersion
            lockSha256 = (Get-FileHash -LiteralPath $toolchainLockFile -Algorithm SHA256).Hash.ToLowerInvariant()
        }
        target        = [ordered]@{
            platform      = 'Win64'
            configuration = 'Shipping'
            cleanBuild    = -not [bool]$Incremental
        }
        packaging     = [ordered]@{
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
        EngineRoot     = $resolvedEngineRoot
        ReportDirectory = $metadataRoot
    }
    if (-not $SkipSmokeTest) {
        $verifyArguments.SmokeTest = $true
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

    $latest = [ordered]@{
        schemaVersion = 1
        buildLabel    = $BuildLabel
        releasePath   = $BuildLabel
        archive       = if ($null -ne $zipPath) { [System.IO.Path]::GetFileName($zipPath) } else { $null }
        updatedAtUtc  = [DateTime]::UtcNow.ToString('o')
    }
    [System.IO.Directory]::CreateDirectory($OutputRoot) | Out-Null
    [System.IO.Directory]::Move($releaseRoot, $finalReleaseRoot)
    if (-not $hasQualityOverride) {
        Write-Utf8Json -Value $latest -Path (Join-Path $OutputRoot 'latest.json')
    }

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
