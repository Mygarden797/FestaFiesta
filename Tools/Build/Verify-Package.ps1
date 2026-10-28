# Verify-Package.ps1
# 역할: 패키징 결과의 필수 파일·플러그인·실행 조건을 검사한다.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string] $PackagePath,

    [string] $EngineRoot,

    [string] $ProjectFile,

    [ValidateSet('Shipping', 'Development')]
    [string] $Configuration = 'Shipping',

    [string] $ReportDirectory,

    # Produced from the live appearance table before cook; never infer the
    # expected crowd assets from an obsolete hard-coded mesh/AnimBP list.
    [string] $CrowdRequirementsFile,

    [switch] $SmokeTest,

    # Requires a GPU/driver that supports both requested paths. Also checks the
    # root EXE's default RHI selection. Fresh reports prove initialization only.
    [switch] $GraphicsSmokeTest,

    [ValidateRange(15, 600)]
    [int] $SmokeTimeoutSeconds = 90
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Import-Module (Join-Path $PSScriptRoot 'FestaFiesta.Build.psm1') -Force
Set-FFProjectFileOverride -ProjectFile $ProjectFile

function Resolve-PayloadRoot {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Root
    )

    $resolvedRoot = (Resolve-Path -LiteralPath $Root).Path
    if (-not (Test-Path -LiteralPath $resolvedRoot -PathType Container)) {
        throw "PackagePath must be an extracted package directory: $resolvedRoot"
    }

    $launchers = @(Get-ChildItem -LiteralPath $resolvedRoot -Recurse -File -Filter 'FestaFiesta.exe' |
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

    throw "A FestaFiesta $Configuration payload was not found below: $resolvedRoot"
}

function Assert-Condition {
    param(
        [Parameter(Mandatory = $true)]
        [bool] $Condition,

        [Parameter(Mandatory = $true)]
        [string] $Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Get-TotalBytes {
    param(
        [Parameter(Mandatory = $true)]
        [AllowEmptyCollection()]
        [System.IO.FileInfo[]] $Files
    )

    $total = [int64]0
    foreach ($file in $Files) {
        $total += $file.Length
    }
    return $total
}

function Read-CrowdCookRequirements {
    param(
        [string] $Path,

        [Parameter(Mandatory = $true)]
        [string] $PayloadRoot,

        [Parameter(Mandatory = $true)]
        [string] $ReportsRoot
    )

    if (-not [string]::IsNullOrWhiteSpace($Path)) {
        $requirementsPath = [System.IO.Path]::GetFullPath($Path)
        Assert-Condition -Condition (Test-Path -LiteralPath $requirementsPath -PathType Leaf) `
            -Message "Crowd cook requirements file was not found: $requirementsPath"
    }
    else {
        $candidates = @((Join-Path $ReportsRoot 'Crowd-CookRequirements.json'))
        # Supports both a build root and Package/Windows as PackagePath. Search
        # only this payload's nearby metadata, never another release directory.
        $ancestor = [System.IO.DirectoryInfo]::new($PayloadRoot)
        for ($level = 0; $level -lt 3 -and $null -ne $ancestor; ++$level) {
            $candidates += Join-Path $ancestor.FullName 'Metadata/Crowd-CookRequirements.json'
            $ancestor = $ancestor.Parent
        }
        $requirementsPath = $candidates | Where-Object {
            Test-Path -LiteralPath $_ -PathType Leaf
        } | Select-Object -First 1
        Assert-Condition -Condition (-not [string]::IsNullOrWhiteSpace($requirementsPath)) `
            -Message 'Crowd cook requirements are mandatory. Pass -CrowdRequirementsFile or retain this build''s Metadata/Crowd-CookRequirements.json.'
    }

    $requirements = Get-Content -LiteralPath $requirementsPath -Raw | ConvertFrom-Json
    foreach ($name in @('schemaVersion', 'sourceTable', 'sourceTableSha256', 'requiredAssets', 'animationBlueprintCount')) {
        Assert-Condition -Condition ($null -ne $requirements.PSObject.Properties[$name]) `
            -Message "Crowd cook requirements lack '$name': $requirementsPath"
    }
    Assert-Condition -Condition ($requirements.schemaVersion -eq 1) `
        -Message "Unsupported crowd cook requirements schema: $requirementsPath"

    # Require package paths, not object/class paths, extensions, or traversal.
    $packagePattern = '^/Game/(?:[^./\\:\x00-\x20]+/)*[^./\\:\x00-\x20]+$'
    Assert-Condition -Condition ($requirements.sourceTable -is [string] -and
        $requirements.sourceTable -cmatch $packagePattern) `
        -Message "Crowd sourceTable must be a /Game package path: $requirementsPath"
    Assert-Condition -Condition ($requirements.requiredAssets -is [array] -and
        $requirements.requiredAssets.Count -gt 0) `
        -Message "Crowd requiredAssets must be a non-empty array: $requirementsPath"
    foreach ($asset in $requirements.requiredAssets) {
        Assert-Condition -Condition ($asset -is [string] -and $asset -cmatch $packagePattern) `
            -Message "Invalid crowd required asset package path '$asset': $requirementsPath"
    }
    $crowdAssets = @($requirements.requiredAssets | Sort-Object -Unique)

    $counts = [ordered]@{}
    foreach ($name in @('animationBlueprintCount', 'skeletalMeshCount', 'meshCount', 'animationSequenceCount', 'blendSpaceCount', 'rowCount', 'requiredAssetCount')) {
        $property = $requirements.PSObject.Properties[$name]
        if ($null -eq $property) {
            continue
        }
        $count = 0
        Assert-Condition -Condition ([int]::TryParse([string]$property.Value, [ref]$count) -and $count -gt 0) `
            -Message "Crowd '$name' must be a positive integer: $requirementsPath"
        $counts[$name] = $count
    }
    Assert-Condition -Condition ($counts.animationBlueprintCount -le $crowdAssets.Count) `
        -Message "Crowd animationBlueprintCount exceeds requiredAssets: $requirementsPath"
    if ($counts.Contains('requiredAssetCount')) {
        Assert-Condition -Condition ($counts.requiredAssetCount -eq $crowdAssets.Count) `
            -Message "Crowd requiredAssetCount differs from unique requiredAssets: $requirementsPath"
    }

    Assert-Condition -Condition ($requirements.sourceTableSha256 -is [string] -and
        $requirements.sourceTableSha256 -match '^[a-fA-F0-9]{64}$') `
        -Message "Crowd sourceTableSha256 must be a SHA-256 digest: $requirementsPath"
    $sourceTableFile = Join-Path (Get-FFProjectRoot) (
        'Content/' + $requirements.sourceTable.Substring('/Game/'.Length) + '.uasset')
    Assert-Condition -Condition (Test-Path -LiteralPath $sourceTableFile -PathType Leaf) `
        -Message "Crowd source appearance table is missing: $sourceTableFile"
    $sourceTableHash = (Get-FileHash -LiteralPath $sourceTableFile -Algorithm SHA256).Hash.ToLowerInvariant()
    Assert-Condition -Condition ($sourceTableHash -eq $requirements.sourceTableSha256) `
        -Message "Crowd appearance table changed since requirements extraction; regenerate requirements and recook: $sourceTableFile"

    # The staging directory is renamed when a release is promoted. Keep its
    # metadata locator relative so the verification report remains usable.
    $fullRequirementsPath = [System.IO.Path]::GetFullPath($requirementsPath)
    $reportPath = [System.IO.Path]::GetFileName($fullRequirementsPath)
    $reportAnchor = 'external-file'
    $ancestor = [System.IO.DirectoryInfo]::new($PayloadRoot)
    $anchors = @('payload-root', 'package-root', 'build-root')
    for ($level = 0; $level -lt $anchors.Count -and $null -ne $ancestor; ++$level) {
        $ancestorPrefix = $ancestor.FullName.TrimEnd([char[]]'\/') + [System.IO.Path]::DirectorySeparatorChar
        if ($fullRequirementsPath.StartsWith($ancestorPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            $reportPath = Get-FFRelativePath -BasePath $ancestor.FullName -Path $fullRequirementsPath
            $reportAnchor = $anchors[$level]
            break
        }
        $ancestor = $ancestor.Parent
    }

    return [pscustomobject]@{
        Path = $fullRequirementsPath
        ReportPath = $reportPath
        ReportAnchor = $reportAnchor
        SourceTable = $requirements.sourceTable
        SourceTableSha256 = $sourceTableHash
        Counts = $counts
        RequiredAssets = $crowdAssets
    }
}

function Assert-NoRuntimeSavedDirectories {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Root
    )

    $savedDirectories = @(Get-ChildItem -LiteralPath $Root -Recurse -Directory -Filter 'Saved')
    $savedPaths = @($savedDirectories | ForEach-Object { $_.FullName })
    Assert-Condition -Condition ($savedDirectories.Count -eq 0) `
        -Message "Runtime Saved directories must not be included in a release payload: $($savedPaths -join ', ')"
}

function Assert-PresentationRuntimeFiles {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Root
    )

    # These are loose runtime files, so the IoStore asset list cannot prove their
    # presence. The intro deliberately skips a missing movie and the catalogue
    # can fall back to native Slate; startup success would hide either omission.
    $requiredPaths = @(
        'FestaFiesta/Content/Movies/FF_Intro_SpaceFireworks.mp4',
        'Engine/Binaries/Win64/EpicWebHelper.exe'
    )
    $cefRoot = Join-Path $Root 'Engine/Binaries/ThirdParty/CEF3/Win64'
    $cefLibraries = @()
    if (Test-Path -LiteralPath $cefRoot -PathType Container) {
        $cefLibraries = @(Get-ChildItem -LiteralPath $cefRoot -Recurse -File -Filter 'libcef.dll')
    }
    Assert-Condition -Condition ($cefLibraries.Count -eq 1) `
        -Message "Expected one Win64 CEF runtime for the building catalogue; found $($cefLibraries.Count) libcef.dll files below: $cefRoot"

    # UE 5.7.4 CEF3.Build.cs declares these runtime dependencies. Discover the
    # version directory rather than embedding a Chromium version in our tool.
    # Every companion must belong to that same runtime, not another CEF copy.
    $cefRelativeRoot = $cefLibraries[0].Directory.FullName.Substring(
        ([System.IO.Path]::GetFullPath($Root)).TrimEnd([char[]]'\/').Length + 1).Replace('\', '/')
    $cefCompanions = @(
        'libcef.dll', 'chrome_elf.dll', 'd3dcompiler_47.dll', 'libEGL.dll', 'libGLESv2.dll',
        'dxcompiler.dll', 'dxil.dll', 'vk_swiftshader.dll', 'vulkan-1.dll', 'icudtl.dat',
        'chrome_100_percent.pak', 'chrome_200_percent.pak', 'resources.pak',
        'snapshot_blob.bin', 'v8_context_snapshot.bin',
        'Resources/locales/en-US.pak', 'Resources/locales/ko.pak'
    )
    $requiredPaths += @($cefCompanions | ForEach-Object { "$cefRelativeRoot/$_" })
    foreach ($relativePath in $requiredPaths) {
        $fullPath = Join-Path $Root $relativePath
        Assert-Condition -Condition (Test-Path -LiteralPath $fullPath -PathType Leaf) `
            -Message "Required presentation runtime file is missing: $relativePath"
        Assert-Condition -Condition ((Get-Item -LiteralPath $fullPath).Length -gt 0) `
            -Message "Required presentation runtime file is empty: $relativePath"
    }

    # Existence checks do not prove video decoding or a rendered browser. Keep
    # the title intro and building catalogue in the packaged manual QA route.
    return $requiredPaths
}

function Invoke-StartupSmokeTest {
    param(
        [Parameter(Mandatory = $true)]
        [ValidateSet('NullRHI', 'DX11-SM5', 'DX12-SM6', 'Default-EXE')]
        [string] $Name,

        [Parameter(Mandatory = $true)]
        [string] $Scope,

        [Parameter(Mandatory = $true)]
        [AllowEmptyCollection()]
        [string[]] $RhiArguments
    )

    $graphicsProbe = $Name -ne 'NullRHI'
    $bootstrapProbe = $Name -eq 'Default-EXE'
    $probeExecutable = if ($bootstrapProbe) { $launcherPath } else { $gameExecutablePath }
    $probeToken = [Guid]::NewGuid().ToString('N')
    # Development builds otherwise write Saved logs/config beside the game.
    # Give each probe isolated user data outside the distributable payload.
    $runtimeDirectoryName = "SmokeRuntime-$Name-$probeToken"
    $runtimeDirectory = Join-Path $ReportDirectory $runtimeDirectoryName
    [System.IO.Directory]::CreateDirectory($runtimeDirectory) | Out-Null
    $runtimeLogPath = Join-Path $runtimeDirectory 'Startup.log'
    $smokeArguments = @('-unattended', '-nosplash', '-nosound',
        "-UserDir=`"$runtimeDirectory`"", "-abslog=`"$runtimeLogPath`"") + $RhiArguments
    $markerPath = Join-Path $ReportDirectory "GraphicsStartup-$Name-$probeToken.json"
    if ($graphicsProbe) {
        # Exit code zero alone is insufficient: unsupported forced RHI paths can
        # exit zero in UE. Require a fresh report written after engine init.
        $smokeArguments += "-FFGraphicsProbe=`"$markerPath`""
        $smokeArguments += "-FFGraphicsProbeToken=$probeToken"
        $smokeArguments += @('-RenderOffscreen', '-windowed', '-ResX=1280', '-ResY=720')
    }
    else {
        $smokeArguments += '-ForceQuitAfterEarlyReads'
    }
    $probe = [ordered]@{
        name = $Name
        scope = $Scope
        executable = $probeExecutable.Substring($payloadRoot.Length + 1).Replace('\', '/')
        rhiSelection = if ($bootstrapProbe) { 'project-default' } else { 'forced-by-probe' }
        runtimeDirectory = $runtimeDirectoryName
        arguments = $smokeArguments
        startedAtUtc = [DateTime]::UtcNow.ToString('o')
        durationSeconds = $null
        exitCode = $null
        status = 'running'
    }
    $probeReportPath = Join-Path $ReportDirectory "Smoke-$Name.json"
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    $process = $null
    try {
        Write-Host "Starting packaged $Name startup test (timeout: $SmokeTimeoutSeconds seconds)..."
        $process = Start-Process `
            -FilePath $probeExecutable `
            -ArgumentList $smokeArguments `
            -WorkingDirectory $payloadRoot `
            -WindowStyle Hidden `
            -PassThru

        if (-not $process.WaitForExit($SmokeTimeoutSeconds * 1000)) {
            $probe.status = 'timeout'
            # UE's bootstrap waits for its child and returns the child's code.
            # On timeout, terminate only this probe's bootstrap process tree;
            # killing just the launcher would leave the game running. Inner
            # executable probes keep their existing single-process cleanup.
            $process.Refresh()
            if (-not $process.HasExited) {
                if ($bootstrapProbe) {
                    $cleanupOutput = @(& (Join-Path $env:SystemRoot 'System32\taskkill.exe') /PID $process.Id /T /F 2>&1)
                    $probe.timeoutCleanupExitCode = $LASTEXITCODE
                    $probe.timeoutCleanup = $cleanupOutput -join [Environment]::NewLine
                }
                else {
                    Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
                }
            }
            throw "The packaged $Name startup test exceeded $SmokeTimeoutSeconds seconds. See: $probeReportPath"
        }

        $process.Refresh()
        $probe.exitCode = $process.ExitCode
        Assert-Condition -Condition ($process.ExitCode -eq 0) `
            -Message "The packaged $Name startup test exited with code $($process.ExitCode). See: $probeReportPath"
        if ($graphicsProbe) {
            Assert-Condition -Condition (Test-Path -LiteralPath $markerPath -PathType Leaf) `
                -Message "The packaged $Name exited without confirming engine/RHI initialization. Exit code zero is not sufficient. See: $probeReportPath"
            $marker = Get-Content -LiteralPath $markerPath -Raw | ConvertFrom-Json
            if ($bootstrapProbe) {
                $validRhiAndShaderModel = ($marker.rhi -eq 'D3D11' -and $marker.shaderModel -eq 'SM5') -or
                    ($marker.rhi -eq 'D3D12' -and $marker.shaderModel -eq 'SM6')
            }
            else {
                $expectedRhi = if ($Name -eq 'DX11-SM5') { 'D3D11' } else { 'D3D12' }
                $expectedShaderModel = if ($Name -eq 'DX11-SM5') { 'SM5' } else { 'SM6' }
                $validRhiAndShaderModel = $marker.rhi -eq $expectedRhi -and $marker.shaderModel -eq $expectedShaderModel
            }
            Assert-Condition -Condition ($marker.token -eq $probeToken -and
                $marker.initialized -eq $true -and $validRhiAndShaderModel) `
                -Message "The packaged $Name did not confirm a supported RHI/shader model pair and fresh initialization token. See: $markerPath"
            $probe.actualRhi = $marker.rhi
            $probe.actualShaderModel = $marker.shaderModel
            $probe.initializationReport = [System.IO.Path]::GetFileName($markerPath)
        }
        $probe.status = 'passed'
    }
    catch {
        if ($probe.status -eq 'running') {
            $probe.status = 'failed'
        }
        throw
    }
    finally {
        $stopwatch.Stop()
        $probe.durationSeconds = [Math]::Round($stopwatch.Elapsed.TotalSeconds, 3)
        [System.IO.File]::WriteAllText(
            $probeReportPath,
            ($probe | ConvertTo-Json -Depth 4) + [Environment]::NewLine,
            (New-Object System.Text.UTF8Encoding($false)))
        if ($null -ne $process) {
            $process.Dispose()
        }
    }

    return $probe
}

$gameBinaryBaseName = Get-FFGameBinaryBaseName -Configuration $Configuration
$payloadRoot = Resolve-PayloadRoot -Root $PackagePath
$resolvedEngineRoot = Resolve-FFEngineRoot -EngineRoot $EngineRoot
$launcherPath = Join-Path $payloadRoot 'FestaFiesta.exe'
$gameExecutablePath = Join-Path $payloadRoot "FestaFiesta\Binaries\Win64\$gameBinaryBaseName.exe"
$pakDirectory = Join-Path $payloadRoot 'FestaFiesta\Content\Paks'
$prerequisitePath = Join-Path $payloadRoot 'Engine\Extras\Redist\en-us\vc_redist.x64.exe'
$arm64PrerequisitePath = Join-Path $payloadRoot 'Engine\Extras\Redist\en-us\vc_redist.arm64.exe'
$mainPakPath = Join-Path $pakDirectory 'FestaFiesta-Windows.pak'
$mainUtocPath = Join-Path $pakDirectory 'FestaFiesta-Windows.utoc'
$mainUcasPath = Join-Path $pakDirectory 'FestaFiesta-Windows.ucas'
$globalUtocPath = Join-Path $pakDirectory 'global.utoc'

if ([string]::IsNullOrWhiteSpace($ReportDirectory)) {
    $ReportDirectory = Join-Path ([System.IO.Path]::GetTempPath()) (
        'FestaFiestaVerify-{0}' -f [Guid]::NewGuid().ToString('N'))
}
$ReportDirectory = [System.IO.Path]::GetFullPath($ReportDirectory)
$payloadPathPrefix = $payloadRoot.TrimEnd([char[]]'\/') + [System.IO.Path]::DirectorySeparatorChar
Assert-Condition -Condition (-not ($ReportDirectory.TrimEnd([char[]]'\/').Equals(
            $payloadRoot.TrimEnd([char[]]'\/'), [System.StringComparison]::OrdinalIgnoreCase) -or
        $ReportDirectory.StartsWith($payloadPathPrefix, [System.StringComparison]::OrdinalIgnoreCase))) `
    -Message 'ReportDirectory must be outside the distributable payload so verification does not add runtime files to it.'
Assert-NoRuntimeSavedDirectories -Root $payloadRoot
[System.IO.Directory]::CreateDirectory($ReportDirectory) | Out-Null
$crowdRequirements = Read-CrowdCookRequirements -Path $CrowdRequirementsFile `
    -PayloadRoot $payloadRoot -ReportsRoot $ReportDirectory

$payloadFiles = @(Get-ChildItem -LiteralPath $payloadRoot -Recurse -File)
$alternateLaunchers = @('Launch-DX11-SM5.cmd', 'Launch-DX12-SM6.cmd', 'Test-DX11-SM5.cmd', 'Test-DX12-SM6.cmd')
$shippedAlternateLaunchers = @($alternateLaunchers | Where-Object { Test-Path -LiteralPath (Join-Path $payloadRoot $_) })
Assert-Condition -Condition ($shippedAlternateLaunchers.Count -eq 0) `
    -Message "The payload must use FestaFiesta.exe as its single entry point; alternate root launchers remain: $($shippedAlternateLaunchers -join ', ')"
$utocFiles = @(Get-ChildItem -LiteralPath $pakDirectory -File -Filter '*.utoc')
$ucasFiles = @(Get-ChildItem -LiteralPath $pakDirectory -File -Filter '*.ucas')
$pakFiles = @(Get-ChildItem -LiteralPath $pakDirectory -File -Filter '*.pak')
$debugFiles = @($payloadFiles | Where-Object { $_.Extension -in @('.pdb', '.exp', '.lib', '.obj') })
$editorFiles = @($payloadFiles | Where-Object { $_.Name -match '(^|-)UnrealEditor|Editor\.modules$' })
$debugFilePaths = @($debugFiles | ForEach-Object { $_.FullName })
$editorFilePaths = @($editorFiles | ForEach-Object { $_.FullName })

$payloadBytes = Get-TotalBytes -Files $payloadFiles
$containerBytes = Get-TotalBytes -Files @($utocFiles + $ucasFiles + $pakFiles)

Assert-Condition -Condition (Test-Path -LiteralPath $launcherPath -PathType Leaf) `
    -Message "The package launcher is missing: $launcherPath"
Assert-Condition -Condition ((Get-Item -LiteralPath $gameExecutablePath).Length -gt 10MB) `
    -Message "The $Configuration executable is unexpectedly small: $gameExecutablePath"
Assert-Condition -Condition ($utocFiles.Count -gt 0) `
    -Message "No IoStore .utoc file was produced in: $pakDirectory"
Assert-Condition -Condition ($ucasFiles.Count -gt 0) `
    -Message "No IoStore .ucas content container was produced in: $pakDirectory"
Assert-Condition -Condition ($pakFiles.Count -gt 0) `
    -Message "No bootstrap .pak file was produced in: $pakDirectory"
Assert-Condition -Condition ($containerBytes -gt 10MB) `
    -Message "The cooked content containers are unexpectedly small ($containerBytes bytes)."
Assert-Condition -Condition ($payloadBytes -gt 50MB) `
    -Message "The packaged payload is unexpectedly small ($payloadBytes bytes)."
Assert-Condition -Condition ($debugFiles.Count -eq 0) `
    -Message "Debug/linker files leaked into the distributable payload: $($debugFilePaths -join ', ')"
Assert-Condition -Condition ($editorFiles.Count -eq 0) `
    -Message "Editor binaries leaked into the distributable payload: $($editorFilePaths -join ', ')"
Assert-Condition -Condition (Test-Path -LiteralPath $prerequisitePath -PathType Leaf) `
    -Message "The required x64 Visual C++ prerequisite installer is missing: $prerequisitePath"
Assert-Condition -Condition (-not (Test-Path -LiteralPath $arm64PrerequisitePath -PathType Leaf)) `
    -Message "The ARM64 prerequisite must not be shipped in a Win64-only payload: $arm64PrerequisitePath"
Assert-Condition -Condition (Test-Path -LiteralPath $mainPakPath -PathType Leaf) `
    -Message "The main bootstrap pak is missing: $mainPakPath"
Assert-Condition -Condition (Test-Path -LiteralPath $mainUtocPath -PathType Leaf) `
    -Message "The main IoStore table of contents is missing: $mainUtocPath"
Assert-Condition -Condition (Test-Path -LiteralPath $mainUcasPath -PathType Leaf) `
    -Message "The main IoStore content container is missing: $mainUcasPath"
Assert-Condition -Condition (Test-Path -LiteralPath $globalUtocPath -PathType Leaf) `
    -Message "The global IoStore table of contents is missing: $globalUtocPath"
$presentationRuntimeFiles = @(Assert-PresentationRuntimeFiles -Root $payloadRoot)

$unrealPak = Join-Path $resolvedEngineRoot 'Engine\Binaries\Win64\UnrealPak.exe'
Assert-Condition -Condition (Test-Path -LiteralPath $unrealPak -PathType Leaf) `
    -Message "UnrealPak is missing from the selected engine: $unrealPak"

$pakTestLog = Join-Path $ReportDirectory 'UnrealPak-Test.log'
$pakTestOutput = @(& $unrealPak $mainPakPath '-Test' 2>&1)
$pakTestExitCode = $LASTEXITCODE
[System.IO.File]::WriteAllLines(
    $pakTestLog,
    [string[]]$pakTestOutput,
    (New-Object System.Text.UTF8Encoding($false)))
Assert-Condition -Condition ($pakTestExitCode -eq 0) `
    -Message "UnrealPak integrity verification failed with exit code $pakTestExitCode. See: $pakTestLog"

$ioStoreCsv = Join-Path $ReportDirectory 'IoStore-Contents.csv'
$ioStoreListLog = Join-Path $ReportDirectory 'IoStore-List.log'
$ioStoreListOutput = @(& $unrealPak "-ListContainer=$mainUtocPath" "-Csv=$ioStoreCsv" 2>&1)
$ioStoreListExitCode = $LASTEXITCODE
[System.IO.File]::WriteAllLines(
    $ioStoreListLog,
    [string[]]$ioStoreListOutput,
    (New-Object System.Text.UTF8Encoding($false)))
Assert-Condition -Condition ($ioStoreListExitCode -eq 0) `
    -Message "IoStore content listing failed with exit code $ioStoreListExitCode. See: $ioStoreListLog"
Assert-Condition -Condition (Test-Path -LiteralPath $ioStoreCsv -PathType Leaf) `
    -Message "IoStore content listing did not create its CSV report: $ioStoreCsv"

$requiredAssets = @(
    '/Game/FestaFiesta/Maps/Frontend',
    '/Game/FestaFiesta/Maps/Test_Main',
    '/Game/FestaFiesta/Data/Time/DT_DaySequence',
    '/Game/FestaFiesta/Data/Time/DT_TimeSettings',
    '/Game/FestaFiesta/Data/Time/DT_ProgramSchedule',
    '/Game/FestaFiesta/Data/Incident/FF_IncidentCatalog',
    '/Game/FestaFiesta/Data/Incident/FF_ID_NPCCrowded',
    '/Game/FestaFiesta/Data/SNS/DT_SnsPostTemplates',
    # Test_Main's placed spawn actor, its presentation class, and the configured
    # arrival/appearance tables. The appearance table's soft mesh and animation
    # references must be present in the actual container, not just in source.
    '/Game/FestaFiesta/Crowd/BP_SpawnActor',
    '/Game/FestaFiesta/Crowd/BP_Crowd',
    '/Game/FestaFiesta/Crowd/ABP_CrowdAnimation',
    '/Game/FestaFiesta/Data/UtilityAI/DT_VisitorArrivalCurve',
    '/Game/FestaFiesta/Data/Crowd/FF_CrowdAppearanceTable',
    '/Game/FestaFiesta/UI/WBP_FF_UIRootLayout',
    '/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveHudScreen',
    '/Game/FestaFiesta/UI/Fonts/F_UI_Pretendard',
    # Native construction UI loads these fonts by string path (FF_BuildTypography.h).
    '/Game/Asset/Font/NEXON_Lv2_Gothic_OTF_Font',
    '/Game/Asset/Font/NEXON_Lv2_Gothic_OTF_Bold_Font',
    '/Game/Asset/Font/NEXON_Lv2_Gothic_OTF_Medium_Font',
    '/Game/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas',
    '/Game/FestaFiesta/Input/IMC_Camera',
    '/Game/FestaFiesta/Input/Character/IMC_Character',
    '/Game/FestaFiesta/Audio/DA_AudioBank',
    '/Game/FestaFiesta/Fireworks/DA_FinaleShow',
    '/Game/FestaFiesta/Building/FF_WBP_Building/Debug/WBP_FF_ZoneLifecycleTestPanel',
    '/Game/FestaFiesta/UI/Frontend/WBP_FF_ConfirmDialog',
    '/Game/FestaFiesta/UI/Settings/WBP_FF_FestivalSettings',
    '/Game/FestaFiesta/Data/Festivals/DA_Festival_Busan'
)
$requiredAssets = @($requiredAssets + $crowdRequirements.RequiredAssets | Sort-Object -Unique)
$ioStoreContents = (Get-Content -LiteralPath $ioStoreCsv -Raw).Replace('\', '/')
$ioStoreEntries = @(Import-Csv -LiteralPath $ioStoreCsv)
Assert-Condition -Condition ($ioStoreEntries.Count -gt 0 -and
    $null -ne $ioStoreEntries[0].PSObject.Properties['Filename']) `
    -Message "IoStore CSV is empty or lacks a Filename column: $ioStoreCsv"

# Compare complete package filenames. Searching the raw CSV text could accept
# BP_CrowdExtra.uasset when BP_Crowd.uasset is missing.
$cookedAssetPaths = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)
$mountedProjectContent = '../../../FestaFiesta/Content/'
foreach ($entry in $ioStoreEntries) {
    $filename = ([string]$entry.Filename).Trim().Replace('\', '/')
    $logicalFilename = $null
    if ($filename.StartsWith($mountedProjectContent, [System.StringComparison]::OrdinalIgnoreCase)) {
        $logicalFilename = '/Game/' + $filename.Substring($mountedProjectContent.Length)
    }
    elseif ($filename.StartsWith('/Content/', [System.StringComparison]::OrdinalIgnoreCase)) {
        $logicalFilename = '/Game/' + $filename.Substring('/Content/'.Length)
    }
    elseif ($filename.StartsWith('/Game/', [System.StringComparison]::OrdinalIgnoreCase)) {
        $logicalFilename = $filename
    }
    if ($null -ne $logicalFilename -and $logicalFilename -match '\.(uasset|umap)$') {
        [void]$cookedAssetPaths.Add($logicalFilename)
    }
}
$requiredMapAssets = @('/Game/FestaFiesta/Maps/Frontend', '/Game/FestaFiesta/Maps/Test_Main')
$missingAssets = @($requiredAssets | Where-Object {
        $extension = if ($requiredMapAssets -contains $_) { '.umap' } else { '.uasset' }
        -not $cookedAssetPaths.Contains("$_$extension")
    })
Assert-Condition -Condition ($missingAssets.Count -eq 0) `
    -Message "Required cooked assets are missing from the IoStore container: $($missingAssets -join ', ')"

# Checking project settings alone cannot prove that both shader formats were
# cooked. Require the actual Global and game libraries inside the distributable.
$requiredShaderFormats = @('PCD3D_SM5', 'PCD3D_SM6')
$requiredShaderLibraries = @(
    foreach ($shaderFormat in $requiredShaderFormats) {
        foreach ($library in @('Global', 'FestaFiesta')) {
            "ShaderArchive-$library-$shaderFormat"
        }
    }
)
$missingShaderLibraries = @($requiredShaderLibraries | Where-Object {
        $libraryPattern = [regex]::Escape($_) + '(?:-PCD3D_SM[56])?\.ushaderbytecode'
        -not [regex]::IsMatch($ioStoreContents, $libraryPattern, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
    })
Assert-Condition -Condition ($missingShaderLibraries.Count -eq 0) `
    -Message "Required SM5/SM6 shader libraries are missing from the IoStore container: $($missingShaderLibraries -join ', ')"

$smokeResults = @()
if ($SmokeTest) {
    $smokeResults += Invoke-StartupSmokeTest -Name 'NullRHI' `
        -Scope 'headless-early-startup' -RhiArguments @('-nullrhi')
}
if ($GraphicsSmokeTest) {
    $smokeResults += Invoke-StartupSmokeTest -Name 'DX11-SM5' `
        -Scope 'confirmed-rhi-and-engine-initialization' -RhiArguments @('-dx11', '-sm5')
    $smokeResults += Invoke-StartupSmokeTest -Name 'DX12-SM6' `
        -Scope 'confirmed-rhi-and-engine-initialization' -RhiArguments @('-dx12', '-sm6')
    $smokeResults += Invoke-StartupSmokeTest -Name 'Default-EXE' `
        -Scope 'confirmed-bootstrap-default-rhi-and-engine-initialization' -RhiArguments @()
}

# Also check after the probes: an ignored runtime path must fail verification
# rather than silently adding machine-specific settings/logs to the ZIP.
Assert-NoRuntimeSavedDirectories -Root $payloadRoot

$summary = [ordered]@{
    schemaVersion  = 3
    configuration  = $Configuration
    executable     = "FestaFiesta/Binaries/Win64/$gameBinaryBaseName.exe"
    verifiedAtUtc  = [DateTime]::UtcNow.ToString('o')
    payloadRoot    = $payloadRoot
    fileCount      = $payloadFiles.Count
    payloadBytes   = $payloadBytes
    containerBytes = $containerBytes
    utocCount      = $utocFiles.Count
    ucasCount      = $ucasFiles.Count
    pakCount       = $pakFiles.Count
    requiredAssetCount = $requiredAssets.Count
    crowdRequirementsFile = $crowdRequirements.ReportPath
    crowdRequirementsAnchor = $crowdRequirements.ReportAnchor
    crowdSourceTable = $crowdRequirements.SourceTable
    crowdSourceTableSha256 = $crowdRequirements.SourceTableSha256
    crowdSourceTableHashVerified = $true
    crowdCounts = $crowdRequirements.Counts
    crowdVerifiedAssetCount = $crowdRequirements.RequiredAssets.Count
    presentationRuntimeFiles = $presentationRuntimeFiles
    unrealPakTested = $true
    ioStoreInspected = $true
    shaderFormats = $requiredShaderFormats
    shaderLibraries = $requiredShaderLibraries
    smokeTested    = [bool]$SmokeTest
    graphicsSmokeTested = [bool]$GraphicsSmokeTest
    defaultLauncherTested = [bool]$GraphicsSmokeTest
    smokeResults = $smokeResults
    renderedGameplayTested = $false
    reportDirectory = $ReportDirectory
}

Write-Host 'Package verification passed.' -ForegroundColor Green
$summary | ConvertTo-Json -Depth 5
