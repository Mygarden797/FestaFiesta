[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string] $PackagePath,

    [string] $EngineRoot,

    [string] $ReportDirectory,

    [switch] $SmokeTest,

    [ValidateRange(15, 600)]
    [int] $SmokeTimeoutSeconds = 90
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Import-Module (Join-Path $PSScriptRoot 'FestaFiesta.Build.psm1') -Force

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
        $shippingExecutable = Join-Path $candidateRoot 'FestaFiesta\Binaries\Win64\FestaFiesta-Win64-Shipping.exe'
        $pakDirectory = Join-Path $candidateRoot 'FestaFiesta\Content\Paks'
        if ((Test-Path -LiteralPath $shippingExecutable -PathType Leaf) -and
            (Test-Path -LiteralPath $pakDirectory -PathType Container)) {
            return $candidateRoot
        }
    }

    throw "A FestaFiesta Shipping payload was not found below: $resolvedRoot"
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

$payloadRoot = Resolve-PayloadRoot -Root $PackagePath
$resolvedEngineRoot = Resolve-FFEngineRoot -EngineRoot $EngineRoot
$launcherPath = Join-Path $payloadRoot 'FestaFiesta.exe'
$shippingExecutablePath = Join-Path $payloadRoot 'FestaFiesta\Binaries\Win64\FestaFiesta-Win64-Shipping.exe'
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
[System.IO.Directory]::CreateDirectory($ReportDirectory) | Out-Null

$payloadFiles = @(Get-ChildItem -LiteralPath $payloadRoot -Recurse -File)
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
Assert-Condition -Condition ((Get-Item -LiteralPath $shippingExecutablePath).Length -gt 10MB) `
    -Message "The Shipping executable is unexpectedly small: $shippingExecutablePath"
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
    '/Game/FestaFiesta/UI/Frontend/WBP_FF_ConfirmDialog',
    '/Game/FestaFiesta/UI/Settings/WBP_FF_FestivalSettings',
    '/Game/FestaFiesta/Data/Festivals/DA_Festival_Busan',
    '/Game/FestaFiesta/Minigame/Data/DA_Minigame_Surfing',
    '/Game/FestaFiesta/Minigame/Data/DT_SurfingCourse'
)
$ioStoreContents = (Get-Content -LiteralPath $ioStoreCsv -Raw).Replace('\', '/')
$missingAssets = @($requiredAssets | Where-Object {
        $logicalPath = $_
        # Depending on the UE point release, ListContainer emits either the
        # logical /Game path or the mounted ../../../Project/Content path.
        $mountedContentPath = $logicalPath -replace '^/Game/', '/Content/'
        $ioStoreContents.IndexOf($logicalPath, [System.StringComparison]::OrdinalIgnoreCase) -lt 0 -and
        $ioStoreContents.IndexOf($mountedContentPath, [System.StringComparison]::OrdinalIgnoreCase) -lt 0
    })
Assert-Condition -Condition ($missingAssets.Count -eq 0) `
    -Message "Required cooked assets are missing from the IoStore container: $($missingAssets -join ', ')"

if ($SmokeTest) {
    $smokeArguments = @(
        '-unattended',
        '-nosplash',
        '-nullrhi',
        '-nosound',
        # ExecCmds and SECONDS are compiled out of Shipping. This engine flag
        # exits only after early startup and packaged containers are mounted.
        '-ForceQuitAfterEarlyReads'
    )

    Write-Host "Starting packaged-build smoke test (timeout: $SmokeTimeoutSeconds seconds)..."
    $process = Start-Process `
        -FilePath $shippingExecutablePath `
        -ArgumentList $smokeArguments `
        -WorkingDirectory $payloadRoot `
        -WindowStyle Hidden `
        -PassThru

    if (-not $process.WaitForExit($SmokeTimeoutSeconds * 1000)) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        throw "The packaged game did not complete the startup smoke test within $SmokeTimeoutSeconds seconds."
    }

    Assert-Condition -Condition ($process.ExitCode -eq 0) `
        -Message "The packaged game smoke test exited with code $($process.ExitCode)."
}

$summary = [ordered]@{
    schemaVersion  = 1
    verifiedAtUtc  = [DateTime]::UtcNow.ToString('o')
    payloadRoot    = $payloadRoot
    fileCount      = $payloadFiles.Count
    payloadBytes   = $payloadBytes
    containerBytes = $containerBytes
    utocCount      = $utocFiles.Count
    ucasCount      = $ucasFiles.Count
    pakCount       = $pakFiles.Count
    requiredAssetCount = $requiredAssets.Count
    unrealPakTested = $true
    ioStoreInspected = $true
    smokeTested    = [bool]$SmokeTest
    reportDirectory = $ReportDirectory
}

Write-Host 'Package verification passed.' -ForegroundColor Green
$summary | ConvertTo-Json -Depth 3
