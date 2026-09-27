# Measure-PackagedPerformance.ps1
# 역할: 이미 만든 개발용 배포본에서 같은 화면 조건으로 성능을 측정한다.
# 측정 시나리오와 로그·CSV·장비 정보를 별도 폴더에 기록해 팀원이 재검토할 수 있게 한다.

<#
.SYNOPSIS
Captures a packaged FestaFiesta Development build on DX12/SM6 at 1920x1080.
.DESCRIPTION
AuthoredMap opens Test_Main directly. This does not start the festival UI flow or
prove scheduled natural arrivals. SyntheticCrowd adds FF.Crowd.Demo at CSV frame
180 and disables debug drawing at frame 181. That synthetic command bypasses the
arrival schedule and may spawn fewer than 500 agents because groups stay intact.
FestivalFlow starts at Frontend, opens the festival through FF.Flow.Start, restores
the authored day1 fixture, then uses a Development command to enter Live. It checks
schedule-driven arrivals but does not validate the player's UI progression buttons.

Every result directory is new. By default game user data is also new; -ReuseUserDir
explicitly reuses an existing directory for a warm repeat. Driver/shared caches are
never cleared. The payload is read only and all logs, CSV, and trace files are
written outside it.
.EXAMPLE
.\Tools\Build\Measure-PackagedPerformance.ps1 -PayloadRoot 'C:\FFBuilds\Dev\Package\Windows' -Scenario AuthoredMap
.EXAMPLE
.\Tools\Build\Measure-PackagedPerformance.ps1 -PayloadRoot 'C:\FFBuilds\Dev\Package\Windows' -Scenario SyntheticCrowd -ReuseUserDir 'C:\FFBuilds\Measurements\First\UserData'
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string] $PayloadRoot,

    [Parameter(Mandatory = $true)]
    [ValidateSet('AuthoredMap', 'SyntheticCrowd', 'FestivalFlow')]
    [string] $Scenario,

    [string] $OutputRoot,

    [string] $EngineRoot,

    [ValidateRange(300, 100000)]
    [int] $CaptureFrames = 4200,

    [ValidateRange(0, 99999)]
    [int] $WarmupFrames = 1200,

    # Optional authored-camera evidence. Capture in warmup so readback does not
    # alter the analysed frame interval. The image includes UI for review.
    [ValidateRange(0, 99999)]
    [int] $ScreenshotFrame = 0,

    [ValidateRange(30, 7200)]
    [int] $TimeoutSeconds = 900,

    [string] $ReuseUserDir,

    # nvidia-smi reports the entire GPU, not the game's own VRAM allocation.
    [switch] $SampleGlobalGpuMemory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'FestaFiesta.Build.psm1') -Force

function Test-IsAtOrBelowPath {
    param([string] $Path, [string] $Parent)
    $left = [System.IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
    $right = [System.IO.Path]::GetFullPath($Parent).TrimEnd('\', '/')
    return $left.Equals($right, [StringComparison]::OrdinalIgnoreCase) -or
        $left.StartsWith($right + [System.IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)
}

function Write-JsonFile {
    param([object] $Value, [string] $Path)
    [System.IO.File]::WriteAllText(
        $Path,
        ($Value | ConvertTo-Json -Depth 12) + [Environment]::NewLine,
        (New-Object System.Text.UTF8Encoding($false)))
}

function Get-HardwareSnapshot {
    $result = [ordered]@{
        os = $null
        cpu = @()
        gpu = @()
        totalPhysicalMemoryBytes = $null
        error = $null
    }
    try {
        $os = Get-CimInstance Win32_OperatingSystem
        $system = Get-CimInstance Win32_ComputerSystem
        $result.os = [ordered]@{
            caption = [string]$os.Caption
            version = [string]$os.Version
            buildNumber = [string]$os.BuildNumber
        }
        $result.cpu = @(Get-CimInstance Win32_Processor | ForEach-Object {
            [ordered]@{
                name = ([string]$_.Name).Trim()
                cores = [int]$_.NumberOfCores
                logicalProcessors = [int]$_.NumberOfLogicalProcessors
            }
        })
        $result.gpu = @(Get-CimInstance Win32_VideoController | ForEach-Object {
            [ordered]@{
                name = [string]$_.Name
                driverVersion = [string]$_.DriverVersion
            }
        })
        $result.totalPhysicalMemoryBytes = [int64]$system.TotalPhysicalMemory
    }
    catch {
        $result.error = $_.Exception.Message
    }
    return $result
}

function Add-RamSample {
    param([System.Diagnostics.Process] $Process, [System.IO.StreamWriter] $Writer)
    if ($Process.HasExited) { return }
    try {
        $Process.Refresh()
        $Writer.WriteLine(('{0},{1},{2},{3}' -f
            [DateTime]::UtcNow.ToString('o'), $Process.WorkingSet64,
            $Process.PrivateMemorySize64, $Process.PeakWorkingSet64))
        $Writer.Flush()
    }
    catch [System.InvalidOperationException] {
        # The owned game may exit between HasExited and Refresh.
    }
}

function Add-GlobalGpuSample {
    param([string] $NvidiaSmi, [System.IO.StreamWriter] $Writer)
    $queriedAt = [DateTime]::UtcNow.ToString('o')
    $output = @(& $NvidiaSmi '--query-gpu=index,memory.used,memory.total' '--format=csv,noheader,nounits' 2>$null)
    if ($LASTEXITCODE -ne 0) { return $false }
    foreach ($line in $output) {
        $parts = ([string]$line).Split(',')
        if ($parts.Count -ne 3) { continue }
        $index = 0
        $used = 0
        $total = 0
        if ([int]::TryParse($parts[0].Trim(), [ref]$index) -and
            [int]::TryParse($parts[1].Trim(), [ref]$used) -and
            [int]::TryParse($parts[2].Trim(), [ref]$total)) {
            $Writer.WriteLine(('{0},{1},{2},{3}' -f $queriedAt, $index, $used, $total))
        }
    }
    $Writer.Flush()
    return $true
}

if ($WarmupFrames -ge $CaptureFrames) {
    throw 'WarmupFrames must be smaller than CaptureFrames.'
}
if ($ScreenshotFrame -gt 0 -and ($ScreenshotFrame -ge $WarmupFrames -or
    ($Scenario -eq 'FestivalFlow' -and $ScreenshotFrame -le 600))) {
    throw 'ScreenshotFrame must be inside warmup and after the FestivalFlow Live transition (frame 600).'
}
if ($Scenario -eq 'FestivalFlow' -and ($CaptureFrames -lt 1800 -or $WarmupFrames -lt 900)) {
    throw 'FestivalFlow requires CaptureFrames >= 1800 and WarmupFrames >= 900 for the map transition, layout restore, Live, and arrival evidence.'
}

$resolvedPayload = Resolve-Path -LiteralPath $PayloadRoot -ErrorAction Stop
if ($resolvedPayload.Provider.Name -ne 'FileSystem' -or
    -not (Test-Path -LiteralPath $resolvedPayload.ProviderPath -PathType Container)) {
    throw 'PayloadRoot must be an existing filesystem directory.'
}
$PayloadRoot = [System.IO.Path]::GetFullPath($resolvedPayload.ProviderPath)
$gameExecutable = Join-Path $PayloadRoot 'FestaFiesta\Binaries\Win64\FestaFiesta.exe'
if (-not (Test-Path -LiteralPath $gameExecutable -PathType Leaf)) {
    throw "Development game executable is missing: $gameExecutable"
}
$manifestPath = Join-Path $PayloadRoot 'build-manifest.json'
$manifest = $null
if (Test-Path -LiteralPath $manifestPath -PathType Leaf) {
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if ([string]$manifest.target.configuration -cne 'Development') {
        throw "A Development payload is required; manifest says '$($manifest.target.configuration)'."
    }
}

$resolvedEngineRoot = Resolve-FFEngineRoot -EngineRoot $EngineRoot
$python = Join-Path $resolvedEngineRoot 'Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
$analyzer = Join-Path $PSScriptRoot 'Analyze-UnrealCsv.py'
if ((Test-Path -LiteralPath $analyzer -PathType Leaf) -and
    -not (Test-Path -LiteralPath $python -PathType Leaf)) {
    throw "CSV analyzer exists but installed engine Python is missing: $python"
}

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $runId = '{0}-{1}' -f [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss'),
        [Guid]::NewGuid().ToString('N').Substring(0, 8)
    $OutputRoot = Join-Path (Get-FFProjectRoot) "Artifacts\Measurements\$Scenario-$runId"
}
$OutputRoot = [System.IO.Path]::GetFullPath($OutputRoot)
if (Test-IsAtOrBelowPath -Path $OutputRoot -Parent $PayloadRoot) {
    throw 'OutputRoot must be outside the payload.'
}
if (Test-Path -LiteralPath $OutputRoot) {
    throw "OutputRoot already exists; choose a unique result path: $OutputRoot"
}

$cacheState = 'new-isolated-user-directory; shared-driver-and-system-caches-untouched'
if ([string]::IsNullOrWhiteSpace($ReuseUserDir)) {
    $userDir = Join-Path $OutputRoot 'UserData'
}
else {
    $resolvedUserDir = Resolve-Path -LiteralPath $ReuseUserDir -ErrorAction Stop
    if ($resolvedUserDir.Provider.Name -ne 'FileSystem' -or
        -not (Test-Path -LiteralPath $resolvedUserDir.ProviderPath -PathType Container)) {
        throw 'ReuseUserDir must be an existing filesystem directory.'
    }
    $userDir = [System.IO.Path]::GetFullPath($resolvedUserDir.ProviderPath)
    $cacheState = 'reused-user-directory-warm-repeat; shared-driver-and-system-caches-untouched'
}
if (Test-IsAtOrBelowPath -Path $userDir -Parent $PayloadRoot) {
    throw 'The game user directory must be outside the payload.'
}

[System.IO.Directory]::CreateDirectory($OutputRoot) | Out-Null
if ([string]::IsNullOrWhiteSpace($ReuseUserDir)) {
    [System.IO.Directory]::CreateDirectory($userDir) | Out-Null
}

$fixtureSource = $null
$fixtureDestination = $null
$fixtureSourceHash = $null
if ($Scenario -eq 'FestivalFlow') {
    $fixtureDescription = Join-Path (Get-FFProjectRoot) 'Tools\Scenarios\day1.layout.json'
    $fixtureSource = Join-Path (Get-FFProjectRoot) 'Saved\SaveGames\day1.sav'
    if (-not (Test-Path -LiteralPath $fixtureDescription -PathType Leaf) -or
        -not (Test-Path -LiteralPath $fixtureSource -PathType Leaf)) {
        throw 'FestivalFlow requires Tools/Scenarios/day1.layout.json and Saved/SaveGames/day1.sav.'
    }
    $fixture = Get-Content -LiteralPath $fixtureDescription -Raw | ConvertFrom-Json
    if ($fixture.map -cne '/Game/FestaFiesta/Maps/Test_Main' -or $fixture.slot -cne 'day1' -or
        @($fixture.facilities).Count -lt 1) {
        throw 'day1.layout.json does not describe the expected Test_Main/day1 facility fixture.'
    }
    $fixtureDestination = Join-Path $userDir 'Saved\SaveGames\day1.sav'
    if (Test-IsAtOrBelowPath -Path $fixtureSource -Parent $userDir) {
        throw 'ReuseUserDir would expose the source day1 save to the game; choose an isolated user directory.'
    }
    $fixtureSourceHash = (Get-FileHash -LiteralPath $fixtureSource -Algorithm SHA256).Hash.ToLowerInvariant()
    [System.IO.Directory]::CreateDirectory((Split-Path -Parent $fixtureDestination)) | Out-Null
    if (Test-Path -LiteralPath $fixtureDestination -PathType Leaf) {
        $existingHash = (Get-FileHash -LiteralPath $fixtureDestination -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($existingHash -cne $fixtureSourceHash) {
            throw "Warm user data contains a different day1.sav; refusing to overwrite: $fixtureDestination"
        }
    }
    else {
        Copy-Item -LiteralPath $fixtureSource -Destination $fixtureDestination -ErrorAction Stop
    }
}

$summaryPath = Join-Path $OutputRoot 'run.json'
$gameLog = Join-Path $OutputRoot 'Game.log'
$stdoutLog = Join-Path $OutputRoot 'stdout.log'
$stderrLog = Join-Path $OutputRoot 'stderr.log'
$traceFile = Join-Path $OutputRoot 'capture.utrace'
$memoryCsv = Join-Path $OutputRoot 'process-memory.csv'
$globalGpuCsv = Join-Path $OutputRoot 'global-gpu-memory.csv'
$framesCsv = Join-Path $OutputRoot 'frames.csv'
$analysisJson = Join-Path $OutputRoot 'analysis.json'
$analysisStdout = Join-Path $OutputRoot 'analysis.stdout.log'
$analysisStderr = Join-Path $OutputRoot 'analysis.stderr.log'
$csvRoot = Join-Path $userDir 'Saved\Profiling\CSV'
$priorCsv = @{}
if (Test-Path -LiteralPath $csvRoot -PathType Container) {
    foreach ($file in Get-ChildItem -LiteralPath $csvRoot -File -Filter '*.csv') {
        $priorCsv[$file.FullName] = '{0}:{1}' -f $file.LastWriteTimeUtc.Ticks, $file.Length
    }
}

$execCommands = @(
    't.MaxFPS 0',
    'r.VSync 0',
    'r.ScreenPercentage',
    'sg.ViewDistanceQuality',
    'sg.ShadowQuality',
    'sg.GlobalIlluminationQuality',
    'sg.ReflectionQuality',
    'sg.PostProcessQuality',
    'sg.TextureQuality',
    'sg.EffectsQuality',
    'sg.FoliageQuality',
    'sg.ShadingQuality',
    'r.PSOPrecaching',
    'r.PSOPrecache.Components',
    'r.PSOPrecache.Resources',
    'r.PSOPrecache.GlobalShaders',
    'r.PSOPrecache.Validation',
    "CSVProfile FRAMES=$CaptureFrames"
)
$csvExecCommands = @()
$screenshotFile = if ($ScreenshotFrame -gt 0) { Join-Path $OutputRoot 'scene.png' } else { $null }
if ($Scenario -eq 'FestivalFlow') {
    $csvExecCommands += '120:FF.Flow.Start'
    $csvExecCommands += '500:FF.Zone.LoadLayout day1'
    $csvExecCommands += '600:FF.Crowd.GoLive'
    $csvExecCommands += '900:FF.Crowd.Arrival'
    $csvExecCommands += '900:FF.Crowd.Stats'
    $csvExecCommands += '1200:FF.Crowd.Arrival'
    $csvExecCommands += '1200:FF.Crowd.Stats'
    $csvExecCommands += ('{0}:FF.Crowd.Arrival' -f ($CaptureFrames - 60))
    $csvExecCommands += ('{0}:FF.Crowd.Stats' -f ($CaptureFrames - 60))
}
else {
    $csvExecCommands += @('120:FF.Crowd.Stats', '120:FF.Crowd.Arrival', '120:FF.Flow.Status')
}
if ($Scenario -eq 'SyntheticCrowd') {
    $csvExecCommands += '180:FF.Crowd.Demo 500 1500'
    $csvExecCommands += '181:FF.Crowd.Debug 0'
    $csvExecCommands += '240:FF.Crowd.Stats'
}
if ($Scenario -ne 'FestivalFlow' -and $WarmupFrames -gt 240 -and $WarmupFrames -lt $CaptureFrames) {
    $csvExecCommands += "${WarmupFrames}:FF.Crowd.Stats"
    $csvExecCommands += "${WarmupFrames}:FF.Crowd.Arrival"
}
if ($null -ne $screenshotFile) {
    $csvExecCommands += "${ScreenshotFrame}:Shot SHOWUI filename=$screenshotFile -nosuffix"
}

# Immediate -csvCaptureFrames triggers capture during UE startup. A prior UE
# 5.7.4 DX12 run asserted in IsRayTracingAllowed before RHI initialization.
# The installed CsvProfiler.cpp supports deferred CSVProfile FRAMES=N via ExecCmds.
$gameArguments = @()
if ($Scenario -ne 'FestivalFlow') {
    $gameArguments += '/Game/FestaFiesta/Maps/Test_Main'
}
$gameArguments += @(
    '-unattended', '-nop4', '-nosplash', '-windowed',
    '-ResX=1920', '-ResY=1080', '-ForceRes', '-novsync', '-dx12', '-sm6',
    ('-UserDir="{0}"' -f $userDir),
    ('-abslog="{0}"' -f $gameLog),
    '-stdout', '-FullStdOutLogOutput',
    '-trace=cpu,gpu,frame,bookmark,loadtime,file,stats',
    ('-tracefile="{0}"' -f $traceFile),
    '-statnamedevents', '-csvGpuStats', '-csvCategories=FFCrowd',
    '-csvCompression=0', '-ExitAfterCsvProfiling',
    ('-ExecCmds="{0}"' -f ($execCommands -join ',')),
    ('-csvExecCmds="{0}"' -f ($csvExecCommands -join ','))
)

$nvidiaSmi = $null
if ($SampleGlobalGpuMemory) {
    $candidate = Get-Command nvidia-smi.exe -ErrorAction SilentlyContinue
    if ($null -ne $candidate) { $nvidiaSmi = $candidate.Source }
}

$beforeHash = (Get-FileHash -LiteralPath $gameExecutable -Algorithm SHA256).Hash.ToLowerInvariant()
$startedAtUtc = [DateTime]::UtcNow
$summary = [ordered]@{
    schemaVersion = 1
    status = 'RUNNING'
    scope = 'rendered-packaged-development-diagnostic'
    scenario = $Scenario
    scenarioDescription = if ($Scenario -eq 'SyntheticCrowd') {
        'Test_Main direct load; synthetic FF.Crowd.Demo 500 1500 at CSV frame 180, debug drawing off at 181'
    } elseif ($Scenario -eq 'FestivalFlow') {
        'Frontend -> FF.Flow.Start -> authored day1 layout restore -> diagnostic GoLive -> schedule-driven arrivals'
    } else { 'Test_Main direct load; authored map without automatic UI festival start' }
    naturalArrivalValidated = $false
    productionMaximumValidated = $false
    requestedSyntheticCount = if ($Scenario -eq 'SyntheticCrowd') { 500 } else { $null }
    observedSyntheticCount = $null
    syntheticCommandObserved = $null
    debugDisableCommandObserved = $null
    festivalFlowEvidence = if ($Scenario -eq 'FestivalFlow') {
        [ordered]@{
            fixtureSource = $fixtureSource
            fixtureSourceSha256 = $fixtureSourceHash
            fixtureCopy = $fixtureDestination
            flowStartSucceeded = $false
            layoutRestoredFacilityCount = $null
            goLiveSucceeded = $false
            scheduledArrivalPlanned = $null
            scheduledArrivalAdmittedMax = $null
            csvActiveAgentsMax = $null
            uiProgressionButtonsValidated = $false
        }
    } else { $null }
    payloadRoot = $PayloadRoot
    manifest = if ($null -ne $manifest) { $manifestPath } else { $null }
    developmentConfigurationEvidence = if ($null -ne $manifest) { 'build-manifest.json' } else { 'unverified: no build-manifest.json in payload' }
    executable = $gameExecutable
    executableSha256Before = $beforeHash
    executableSha256After = $null
    engineRoot = $resolvedEngineRoot
    outputRoot = $OutputRoot
    userDir = $userDir
    cacheState = $cacheState
    capture = [ordered]@{
        rhiRequested = 'D3D12-SM6'
        rhiConfirmedByGameLog = $false
        mapRequested = if ($Scenario -eq 'FestivalFlow') {
            'Frontend followed by /Game/FestaFiesta/Maps/Test_Main'
        } else { '/Game/FestaFiesta/Maps/Test_Main' }
        mapConfirmedByGameLog = $false
        resolution = @(1920, 1080)
        quality = 'project defaults; requested CVar values logged by ExecCmds'
        frameCountRequested = $CaptureFrames
        warmupFramesExcludedFromAnalysis = $WarmupFrames
        screenshotFrameRequested = if ($ScreenshotFrame -gt 0) { $ScreenshotFrame } else { $null }
        trigger = "deferred ExecCmds CSVProfile FRAMES=$CaptureFrames"
        triggerReason = 'Immediate -csvCaptureFrames previously asserted before RHI initialization on UE 5.7.4 DX12'
        traceChannels = @('cpu', 'gpu', 'frame', 'bookmark', 'loadtime', 'file', 'stats')
    }
    hardware = Get-HardwareSnapshot
    gpuMemoryScope = if ($null -ne $nvidiaSmi) { 'nvidia-smi whole-GPU used memory, all processes, 5-second samples' } else { 'not sampled' }
    command = [ordered]@{ executable = $gameExecutable; arguments = $gameArguments }
    startedAtUtc = $startedAtUtc.ToString('o')
    completedAtUtc = $null
    durationSeconds = $null
    timeoutSeconds = $TimeoutSeconds
    processId = $null
    exitCode = $null
    timedOut = $false
    outputs = [ordered]@{
        gameLog = $gameLog
        stdout = $stdoutLog
        stderr = $stderrLog
        trace = $traceFile
        processMemoryCsv = $memoryCsv
        globalGpuMemoryCsv = if ($null -ne $nvidiaSmi) { $globalGpuCsv } else { $null }
        rawCsv = $null
        framesCsv = $null
        analysisJson = $null
        screenshot = $screenshotFile
    }
    limitations = @(
        'AuthoredMap and SyntheticCrowd load Test_Main directly; FestivalFlow uses diagnostic commands for Live and does not validate UI progression buttons.',
        'SyntheticCrowd bypasses arrival times and may produce fewer than 500 agents because generation preserves whole groups.',
        'A new UserDir is not a cold driver or shared-DDC cache; no shared caches are cleared.',
        'Process RAM is sampled externally; UE GPU counters are in frames.csv and optional nvidia-smi values include other processes.',
        'Minimum-spec PC and first-install prerequisites are not validated by this local capture.'
    )
    error = $null
}
Write-JsonFile -Value $summary -Path $summaryPath

$gameProcess = $null
$ramWriter = $null
$gpuWriter = $null
$failure = $null
try {
    $ramWriter = New-Object System.IO.StreamWriter($memoryCsv, $false, (New-Object System.Text.UTF8Encoding($false)))
    $ramWriter.WriteLine('sampleUtc,workingSetBytes,privateBytes,peakWorkingSetBytes')
    $ramWriter.Flush()
    if ($null -ne $nvidiaSmi) {
        $gpuWriter = New-Object System.IO.StreamWriter($globalGpuCsv, $false, (New-Object System.Text.UTF8Encoding($false)))
        $gpuWriter.WriteLine('sampleUtc,gpuIndex,globalUsedMiB,globalTotalMiB')
        $gpuWriter.Flush()
    }

    # Launch the inner executable directly so the owned PID is the rendering game.
    # Start-Process joins ArgumentList; embedded quotes preserve paths with spaces.
    $gameProcess = Start-Process -FilePath $gameExecutable -ArgumentList $gameArguments `
        -WorkingDirectory $OutputRoot -PassThru `
        -RedirectStandardOutput $stdoutLog -RedirectStandardError $stderrLog
    $summary.processId = $gameProcess.Id
    Write-JsonFile -Value $summary -Path $summaryPath

    $clock = [System.Diagnostics.Stopwatch]::StartNew()
    $lastGpuSample = -5.0
    while (-not $gameProcess.WaitForExit(1000)) {
        Add-RamSample -Process $gameProcess -Writer $ramWriter
        if ($null -ne $gpuWriter -and ($clock.Elapsed.TotalSeconds - $lastGpuSample) -ge 5.0) {
            $lastGpuSample = $clock.Elapsed.TotalSeconds
            $null = Add-GlobalGpuSample -NvidiaSmi $nvidiaSmi -Writer $gpuWriter
        }
        if ($clock.Elapsed.TotalSeconds -ge $TimeoutSeconds) {
            $summary.timedOut = $true
            if (-not $gameProcess.HasExited) {
                $gameProcess.Kill()
                $gameProcess.WaitForExit(5000) | Out-Null
            }
            throw "Packaged capture exceeded $TimeoutSeconds seconds."
        }
    }
    $gameProcess.WaitForExit()
    $gameProcess.Refresh()
    if (-not $gameProcess.HasExited) { throw 'Owned game process was not confirmed exited.' }
    $rawExitCode = $gameProcess.ExitCode
    if ($null -eq $rawExitCode) { throw 'Owned game process ExitCode is unavailable after WaitForExit.' }
    $summary['exitCode'] = [int]$rawExitCode
    if ($summary['exitCode'] -ne 0) { throw "Game exited with code $($summary['exitCode'])." }

    if (-not (Test-Path -LiteralPath $gameLog -PathType Leaf)) {
        throw 'The game did not write its requested Game.log.'
    }
    $gameLogText = Get-Content -LiteralPath $gameLog -Raw
    $summary.capture.rhiConfirmedByGameLog = [bool](
        $gameLogText -match 'RHI D3D12 with Feature Level SM6 is supported and will be used' -or
        $gameLogText -match 'Creating D3D12 RHI with Max Feature Level SM6')
    $summary.capture.mapConfirmedByGameLog = [bool]($gameLogText -match 'Load map complete /Game/FestaFiesta/Maps/Test_Main')
    if (-not $summary.capture.rhiConfirmedByGameLog) { throw 'Game.log does not confirm D3D12 SM6 initialization.' }
    if (-not $summary.capture.mapConfirmedByGameLog) { throw 'Game.log does not confirm Test_Main load completion.' }
    if ($Scenario -eq 'SyntheticCrowd') {
        $summary.syntheticCommandObserved = [bool]($gameLogText -match 'CSVEvent "Cmd: FF\.Crowd\.Demo 500 1500"')
        if (-not $summary.syntheticCommandObserved) { throw 'The synthetic crowd command was not observed at CSV frame 180.' }
        $summary.debugDisableCommandObserved = [bool]($gameLogText -match 'CSVEvent "Cmd: FF\.Crowd\.Debug 0"')
        if (-not $summary.debugDisableCommandObserved) { throw 'The debug drawing disable command was not observed at CSV frame 181.' }
        if ($gameLogText -match '(?m)^.*?(\d+)명 스폰 \(활성') {
            $summary.observedSyntheticCount = [int]$Matches[1]
        }
    }
    elseif ($Scenario -eq 'FestivalFlow') {
        $flow = $summary.festivalFlowEvidence
        $flowStartCommand = $gameLogText -match 'CSVEvent "Cmd: FF\.Flow\.Start"'
        $flowStartLog = $gameLogText -match 'UFF_UIFlowSubsystem: 축제 .* 시작 .* 레벨 /Game/FestaFiesta/Maps/Test_Main 를 연다'
        $flow.flowStartSucceeded = [bool]($flowStartCommand -and $flowStartLog)
        if (-not $flow.flowStartSucceeded) {
            throw 'FF.Flow.Start was not confirmed to open the authored festival map.'
        }
        if ($gameLogText -match "슬롯 'day1' 배치를 복원했다 .* 시설 (\d+)개") {
            $flow.layoutRestoredFacilityCount = [int]$Matches[1]
        }
        if ($null -eq $flow.layoutRestoredFacilityCount -or $flow.layoutRestoredFacilityCount -lt 1) {
            throw 'The day1 authored layout was not confirmed restored.'
        }
        $goLiveCommand = $gameLogText -match 'CSVEvent "Cmd: FF\.Crowd\.GoLive"'
        $liveTransition = $gameLogText -match '\[Stage\] .* -> EFF_FestivalStage::Live'
        $flow.goLiveSucceeded = [bool]($goLiveCommand -and $liveTransition)
        if (-not $flow.goLiveSucceeded) {
            throw 'The FF.Crowd.GoLive command did not produce a Live stage transition.'
        }
        $arrivalMatches = [regex]::Matches($gameLogText,
            '\[CrowdArrival\] Initialized=1 Stage=EFF_FestivalStage::Live Started=1 TimeScale=[0-9.]+ TableLoaded=1 Planned=(\d+) Admitted=(\d+) Active=(\d+) Bound=(\d+)')
        $maxPlanned = 0
        $maxAdmitted = 0
        foreach ($arrival in $arrivalMatches) {
            $maxPlanned = [Math]::Max($maxPlanned, [int]$arrival.Groups[1].Value)
            $maxAdmitted = [Math]::Max($maxAdmitted, [int]$arrival.Groups[2].Value)
        }
        $flow.scheduledArrivalPlanned = $maxPlanned
        $flow.scheduledArrivalAdmittedMax = $maxAdmitted
        if ($maxPlanned -le 0 -or $maxAdmitted -le 0) {
            throw 'Live arrival diagnostics did not confirm a loaded table, positive plan, and positive scheduled admissions.'
        }
    }

    $trace = Get-Item -LiteralPath $traceFile -ErrorAction SilentlyContinue
    if ($null -eq $trace -or $trace.Length -eq 0) { throw 'The requested Unreal Insights trace is missing or empty.' }
    if ($null -ne $screenshotFile) {
        $screenshot = Get-Item -LiteralPath $screenshotFile -ErrorAction SilentlyContinue
        if ($null -eq $screenshot) {
            # Some UE screenshot paths append a numeric suffix despite the flag.
            $screenshot = Get-ChildItem -LiteralPath $OutputRoot -File -Filter 'scene*.png' |
                Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
        }
        if ($null -eq $screenshot -or $screenshot.Length -eq 0) {
            throw "The requested authored-camera screenshot is missing or empty: $screenshotFile"
        }
        $summary.outputs.screenshot = $screenshot.FullName
    }
    $freshCsv = @(Get-ChildItem -LiteralPath $csvRoot -File -Filter '*.csv' -ErrorAction SilentlyContinue |
        Where-Object {
            $fingerprint = '{0}:{1}' -f $_.LastWriteTimeUtc.Ticks, $_.Length
            $_.LastWriteTimeUtc -ge $startedAtUtc.AddSeconds(-2) -and
            (-not $priorCsv.ContainsKey($_.FullName) -or $priorCsv[$_.FullName] -ne $fingerprint)
        } | Sort-Object LastWriteTimeUtc -Descending)
    if ($freshCsv.Count -eq 0) { throw "No fresh CSV profiler file appeared under $csvRoot."
    }
    if ($freshCsv[0].Length -eq 0) { throw 'The newest fresh CSV profiler file is empty.' }
    $summary.outputs.rawCsv = $freshCsv[0].FullName
    Copy-Item -LiteralPath $freshCsv[0].FullName -Destination $framesCsv -ErrorAction Stop
    $summary.outputs.framesCsv = $framesCsv

    if (Test-Path -LiteralPath $analyzer -PathType Leaf) {
        $analysisArguments = @($analyzer, $framesCsv, '--output', $analysisJson,
            '--start-frame', [string]$WarmupFrames)
        if ($Scenario -eq 'SyntheticCrowd') {
            # A direct Test_Main launch remains in Setup. The demo creates and binds
            # actors, but the Live-only CSV crowd category has no ActiveAgents column.
            # The demo count is checked from the game's own command/status log above.
            $summary.limitations += 'SyntheticCrowd runs in Setup: active and bound counts are confirmed by game log, while the Live-only FFCrowd/ActiveAgents CSV counter is unavailable.'
        }
        & $python @analysisArguments 1> $analysisStdout 2> $analysisStderr
        $analysisExitCode = $LASTEXITCODE
        if ($analysisExitCode -ne 0 -or -not (Test-Path -LiteralPath $analysisJson -PathType Leaf)) {
            throw "CSV analysis failed (exit $analysisExitCode). See $analysisStdout and $analysisStderr."
        }
        $analysis = Get-Content -LiteralPath $analysisJson -Raw | ConvertFrom-Json
        if ($analysis.status -cne 'PASS') { throw 'CSV analyzer did not report PASS.' }
        $summary.outputs.analysisJson = $analysisJson
        if ($Scenario -eq 'FestivalFlow') {
            $active = $analysis.crowd.active_agents
            if ($null -eq $active -or $null -eq $active.max -or [double]$active.max -le 0) {
                throw 'FestivalFlow CSV does not show positive active NPC samples after warmup.'
            }
            $summary.festivalFlowEvidence.csvActiveAgentsMax = [double]$active.max
            $summary.naturalArrivalValidated = $true
        }
    }
    else {
        throw "CSV analyzer is unavailable: $analyzer"
    }
    $summary.status = 'PASS'
}
catch {
    $summary.status = 'FAIL'
    $summary.error = $_.Exception.Message
    $failure = $summary.error
}
finally {
    if ($null -ne $gameProcess) {
        try {
            if (-not $gameProcess.HasExited) {
                $gameProcess.Kill()
                $gameProcess.WaitForExit(5000) | Out-Null
            }
            if ($gameProcess.HasExited) {
                $gameProcess.Refresh()
                if ($null -ne $gameProcess.ExitCode) {
                    $summary['exitCode'] = [int]$gameProcess.ExitCode
                }
            }
        }
        catch {
            $summary.status = 'FAIL'
            $summary.error = 'Could not stop the process started by this script: ' + $_.Exception.Message
            $failure = $summary.error
        }
        finally { $gameProcess.Dispose() }
    }
    if ($null -ne $ramWriter) { $ramWriter.Dispose() }
    if ($null -ne $gpuWriter) { $gpuWriter.Dispose() }
    $summary.executableSha256After = (Get-FileHash -LiteralPath $gameExecutable -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($summary.executableSha256After -cne $beforeHash) {
        $summary.status = 'FAIL'
        $summary.error = 'The executable hash changed during measurement; package immutability was not preserved.'
        $failure = $summary.error
    }
    if ($Scenario -eq 'FestivalFlow' -and $null -ne $fixtureSourceHash) {
        $fixtureAfterHash = (Get-FileHash -LiteralPath $fixtureSource -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($fixtureAfterHash -cne $fixtureSourceHash) {
            $summary.status = 'FAIL'
            $summary.naturalArrivalValidated = $false
            $summary.error = 'The source day1.sav changed during measurement; fixture immutability was not preserved.'
            $failure = $summary.error
        }
    }
    $completedAtUtc = [DateTime]::UtcNow
    $summary.completedAtUtc = $completedAtUtc.ToString('o')
    $summary.durationSeconds = [Math]::Round(($completedAtUtc - $startedAtUtc).TotalSeconds, 3)
    Write-JsonFile -Value $summary -Path $summaryPath
}

if ($null -ne $failure) {
    throw "Packaged performance capture failed: $failure See $summaryPath"
}
Write-Output $summaryPath
