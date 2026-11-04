# Offline regression tests: no Git/Unreal/rclone process or network is invoked.
[CmdletBinding()]
param([string] $FixtureParent = [IO.Path]::GetTempPath())
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$modulePath = Join-Path $PSScriptRoot 'FestaFiesta.Jenkins.psm1'
$module = Import-Module $modulePath -Force -PassThru
$script:Assertions = 0

function Assert-FFTest {
    param([bool] $Condition, [string] $Message)
    if (-not $Condition) { throw "Assertion failed: $Message" }
    $script:Assertions++
}

function Assert-FFThrows {
    param([scriptblock] $Action, [string] $Message, [string] $ExpectedMessage = '')
    $caught = $null
    try { & $Action | Out-Null } catch { $caught = $_ }
    Assert-FFTest -Condition ($null -ne $caught) -Message $Message
    if ($ExpectedMessage) {
        Assert-FFTest -Condition ($caught.Exception.Message -like ('*' + $ExpectedMessage + '*')) -Message ($Message + ' (expected reason)')
    }
}

$fixtureRoot = Join-Path $FixtureParent ('ff-jenkins-test-' + [Guid]::NewGuid().ToString('N'))
$fixtureFullPath = [IO.Path]::GetFullPath($fixtureRoot)
try {
    $shaA = 'a' * 40; $shaB = 'b' * 40; $shaC = 'c' * 40; $shaD = 'd' * 40; $shaE = 'e' * 40
    $empty = [pscustomobject]@{ published = $null; pending = $null }
    Assert-FFTest ((Get-FFBuildPlan -State $empty -CandidateCommit $shaA).actions -contains 'BuildAndPublish') 'Initial source is built.'
    $published = [pscustomobject]@{ published = [pscustomobject]@{ commit = $shaA }; pending = $null }
    Assert-FFTest ((Get-FFBuildPlan -State $published -CandidateCommit $shaA).actions -contains 'SkipUnchanged') 'An already uploaded commit is skipped.'
    $withPending = [pscustomobject]@{ published = $published.published; pending = [pscustomobject]@{ phase = 'ready'; commit = $shaB } }
    $plan = Get-FFBuildPlan -State $withPending -CandidateCommit $shaB
    Assert-FFTest ($plan.actions[0] -eq 'ResumePendingUpload' -and $plan.actions[1] -eq 'SkipAfterPendingPublication') 'Pending source resumes without rebuilding.'
    $plan = Get-FFBuildPlan -State $withPending -CandidateCommit $shaC
    Assert-FFTest ($plan.actions[0] -eq 'ResumePendingUpload' -and $plan.actions[1] -eq 'BuildAndPublish') 'Older pending source is uploaded before a newer commit.'
    $plan = Get-FFBuildPlan -State $withPending -CandidateCommit $shaA
    Assert-FFTest ($plan.actions[0] -eq 'ResumePendingUpload' -and $plan.actions[1] -eq 'BuildAndPublish') 'A branch rewind is compared with the commit that the pending upload will publish.'
    $label = New-FFBuildLabel -ProjectVersion '1.0.0.0' -Commit $shaA -BuildNumber '12' -Now ([DateTimeOffset]'2026-11-03T15:00:00Z')
    Assert-FFTest ($label -eq 'FF-v1.0.0.0-20261104-000000-b12-aaaaaaaa') 'Build labels use the Korean date across UTC midnight.'
    Assert-FFThrows { New-FFBuildLabel -ProjectVersion '1.0.0.0' -Commit $shaA -BuildNumber '1;exit' } 'Shell-like build numbers are rejected.'
    Assert-FFTest ((New-FFBuildLabel -ProjectVersion '1.0.0.0' -Commit $shaA -BuildNumber 'manual-20261103190000') -match '-bmanual-20261103190000-') 'Manual launcher build numbers are supported.'
    $publicationName = & $module { New-FFPublicationName -Version 25 -StartedAt ([DateTimeOffset]'2026-11-03T15:00:00Z') }
    Assert-FFTest ($publicationName -ceq 'V25(2026.11.04.00:00)') 'Publication folders use a sequential version and Korean build date/time across UTC midnight.'
    $timedPublicationName = & $module { New-FFPublicationName -Version 24 -StartedAt ([DateTimeOffset]'2026-11-03T09:44:30Z') }
    Assert-FFTest ($timedPublicationName -ceq 'V24(2026.11.03.18:44)') 'Publication folders include the actual Korean build hour and minute.'
    $seededState = [pscustomobject]@{ publicationSequence = 24; published = [pscustomobject]@{ publicationVersion = 24 }
        pending = [pscustomobject]@{ startedAtUtc = '2026-11-03T15:00:00Z' } }
    & $module { param($State) Initialize-FFPublicationSequence -State $State; Set-FFPendingPublicationIdentity -State $State } $seededState
    Assert-FFTest ($seededState.publicationSequence -eq 25 -and $seededState.pending.publicationName -ceq 'V25(2026.11.04.00:00)') 'A migrated V24 publication seeds the next build as V25.'
    & $module { param($State) Set-FFPendingPublicationIdentity -State $State } $seededState
    Assert-FFTest ($seededState.publicationSequence -eq 25 -and $seededState.pending.publicationName -ceq 'V25(2026.11.04.00:00)') 'A reserved publication identity keeps its build date/time and version on later retries.'
    $seededState.pending.publicationName = 'V25(2026.11.04.00:01)'
    Assert-FFThrows { & $module { param($State) Set-FFPendingPublicationIdentity -State $State } $seededState } 'An identity with a changed build minute is refused.' 'reserved version'

    $arguments = @{
        SourceRepository = 'https://github.com/example/FestaFiesta.git'
        SourceBranch = 'Dev'; CheckoutRoot = (Join-Path $fixtureRoot 'src')
        OutputRoot = (Join-Path $fixtureRoot 'output'); StateRoot = (Join-Path $fixtureRoot 'state')
        EngineRoot = (Join-Path $fixtureRoot 'engine'); RcloneExe = 'fixture-rclone.exe'
        RcloneConfig = (Join-Path $fixtureRoot 'rclone.conf'); BuildNumber = '1'
    }
    $offlinePlan = Invoke-FFDailyBuild @arguments -Plan -CandidateCommit $shaA | ConvertFrom-Json
    Assert-FFTest ($offlinePlan.mode -eq 'OfflinePlan' -and -not (Test-Path -LiteralPath $fixtureRoot)) 'Plan neither creates directories nor contacts external services.'
    $unsafeArguments = $arguments.Clone(); $unsafeArguments.SourceRepository = 'https://secret@example.com/repo.git'
    Assert-FFThrows { Invoke-FFDailyBuild @unsafeArguments -Plan } 'Credentials in repository URLs are rejected.' 'credentials'
    $unsafeArguments = $arguments.Clone(); $unsafeArguments.SourceRepository = 'HTTPS://user:secret@example.com/repo.git'
    Assert-FFThrows { Invoke-FFDailyBuild @unsafeArguments -Plan } 'Uppercase HTTPS cannot bypass credential checks.' 'credentials'
    $unsafeArguments = $arguments.Clone(); $unsafeArguments.SourceBranch = 'Dev;exit'
    Assert-FFThrows { Invoke-FFDailyBuild @unsafeArguments -Plan } 'Unsafe branch names are rejected.' 'SourceBranch'
    $unsafeArguments = $arguments.Clone(); $unsafeArguments.OutputRoot = Join-Path $arguments.CheckoutRoot 'output'
    Assert-FFThrows { Invoke-FFDailyBuild @unsafeArguments -Plan } 'Outputs nested inside source are rejected.' 'separate directories'

    [IO.Directory]::CreateDirectory($fixtureRoot) | Out-Null
    $harness = [pscustomobject]@{
        RemoteCommit = $shaA; RemoteCalls = 0; BuildCalls = 0; CopyCalls = 0
        FailCopyAt = 0; TamperRemoteMd5 = $false; FailBuild = $false
        Events = (New-Object 'System.Collections.Generic.List[string]')
        RemoteFiles = @{}
    }
    & $module {
        param($Harness)
        $script:FFHarness = $Harness
        function script:Get-FFRemoteCommit {
            param($Options)
            $script:FFHarness.RemoteCalls++
            $script:FFHarness.Events.Add('remote:' + $script:FFHarness.RemoteCommit)
            return $script:FFHarness.RemoteCommit
        }
        function script:Invoke-FFCheckout {
            param($Options, [string] $Commit)
            $script:FFHarness.Events.Add('checkout:' + $Commit)
            [IO.Directory]::CreateDirectory((Join-Path $Options.CheckoutRoot 'Config')) | Out-Null
            [IO.File]::WriteAllText((Join-Path $Options.CheckoutRoot 'Config\DefaultGame.ini'), "ProjectVersion=1.0.0.0`r`n")
        }
        function script:Invoke-FFPackage {
            param($Options, [string] $BuildLabel)
            $script:FFHarness.BuildCalls++
            $script:FFHarness.Events.Add('build:' + $BuildLabel)
            if ($script:FFHarness.FailBuild) { throw 'Fixture package failure.' }
            $state = Get-Content -LiteralPath (Join-Path $Options.StateRoot 'daily-build-state.json') -Raw | ConvertFrom-Json
            $root = Join-Path $Options.OutputRoot $BuildLabel
            $metadata = Join-Path $root 'Metadata'
            [IO.Directory]::CreateDirectory($metadata) | Out-Null
            $zipName = $BuildLabel + '.zip'
            $zipPath = Join-Path $root $zipName
            [IO.File]::WriteAllText($zipPath, 'Fixture package bytes for ' + $state.pending.commit)
            $manifest = [ordered]@{
                releaseStatus = 'validated'; buildLabel = $BuildLabel
                source = @{ commit = $state.pending.commit.Substring(0, 12); dirty = $false }
                validation = @{ automationTests = $true; dataValidation = $true; smokeTest = $true
                    graphicsStartupTest = $true; defaultLauncherStartupTest = $true; qualityOverride = $false }
            }
            Write-FFState -State $manifest -Path (Join-Path $metadata 'build-manifest.json')
            Write-FFState -State @{ unrealPakTested = $true; ioStoreInspected = $true; smokeTested = $true
                graphicsSmokeTested = $true; defaultLauncherTested = $true } -Path (Join-Path $metadata 'verification.json')
            $sha = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
            [IO.File]::WriteAllText((Join-Path $metadata 'ARCHIVE-SHA256.txt'), $sha + '  ' + $zipName)
            Write-FFState -State @{ buildLabel = $BuildLabel; releasePath = $BuildLabel; archive = $zipName } -Path (Join-Path $Options.OutputRoot 'latest.json')
        }
        function script:Invoke-FFNative {
            param([string] $FilePath, [string[]] $Arguments, [string] $Operation)
            if ($FilePath -ne 'fixture-rclone.exe') { throw 'Unexpected native process in offline test.' }
            if ($Arguments[0] -eq 'copyto') {
                $script:FFHarness.CopyCalls++
                $script:FFHarness.Events.Add('copy:' + $Arguments[2])
                if ($script:FFHarness.CopyCalls -eq $script:FFHarness.FailCopyAt) { throw 'Fixture upload failure.' }
                $script:FFHarness.RemoteFiles[$Arguments[2]] = $Arguments[1]
                return ''
            }
            if ($Arguments[0] -eq 'md5sum') {
                $path = $script:FFHarness.RemoteFiles[$Arguments[1]]
                $md5 = if ($script:FFHarness.TamperRemoteMd5) { '0' * 32 } else {
                    (Get-FileHash -LiteralPath $path -Algorithm MD5).Hash.ToLowerInvariant()
                }
                return $md5 + '  ' + [IO.Path]::GetFileName($path)
            }
            throw 'Unexpected rclone command in offline test.'
        }
    } $harness

    $statePath = Join-Path $arguments.StateRoot 'daily-build-state.json'
    Invoke-FFDailyBuild @arguments -BuildOnly
    $state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
    Assert-FFTest ($harness.BuildCalls -eq 1 -and $harness.CopyCalls -eq 0 -and $null -eq $state.published -and $state.pending.phase -eq 'ready') 'BuildOnly works without Drive credentials and preserves a ready pending artifact.'
    $firstPublicationName = $state.pending.publicationName
    Assert-FFTest ($state.publicationSequence -eq 24 -and $state.pending.publicationVersion -eq 24 -and
        $state.pending.files.Count -eq 1 -and $state.pending.files[0].remoteName -ceq 'FestaFiesta!.zip') 'The initial build reserves a V24 folder and queues only FestaFiesta!.zip.'
    $harness.RemoteCommit = $shaB
    Invoke-FFDailyBuild @arguments -BuildOnly
    Assert-FFTest ($harness.BuildCalls -eq 1 -and $harness.RemoteCalls -eq 1) 'BuildOnly preserves existing pending files even when newer source exists.'
    [IO.File]::WriteAllText($arguments.RcloneConfig, 'fixture placeholder; no real credential')
    $harness.RemoteCommit = $shaA
    Invoke-FFDailyBuild @arguments
    $state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
    Assert-FFTest ($harness.BuildCalls -eq 1 -and $harness.CopyCalls -eq 1 -and $state.published.commit -eq $shaA -and $null -eq $state.pending) 'An authenticated rerun uploads and verifies one pending ZIP without rebuilding.'
    Assert-FFTest ($state.published.publicationName -ceq $firstPublicationName -and $state.published.publicationVersion -eq 24 -and
        $state.published.publicationArchiveName -ceq 'FestaFiesta!.zip' -and
        $harness.RemoteFiles.ContainsKey('festafiesta-drive:/' + $firstPublicationName + '/FestaFiesta!.zip')) 'The verified ZIP is published as FestaFiesta!.zip in its reserved version folder.'
    Invoke-FFDailyBuild @arguments
    Assert-FFTest ($harness.BuildCalls -eq 1 -and $harness.CopyCalls -eq 1) 'An unchanged branch performs no build or upload.'

    $harness.RemoteCommit = $shaB; $arguments.BuildNumber = '2'
    $harness.FailCopyAt = $harness.CopyCalls + 1
    Assert-FFThrows { Invoke-FFDailyBuild @arguments } 'A failed ZIP upload fails the job.' 'Fixture upload failure'
    $state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
    Assert-FFTest ($state.published.commit -eq $shaA -and $state.pending.commit -eq $shaB -and $state.pending.phase -eq 'ready') 'Upload failure retains the previous published SHA and retryable pending files.'
    $pendingLabel = $state.pending.buildLabel
    $pendingPublicationName = $state.pending.publicationName
    Assert-FFTest ($state.pending.publicationVersion -eq 25) 'A new source build increments the publication version once.'
    $eventStart = $harness.Events.Count
    $harness.FailCopyAt = 0; $harness.RemoteCommit = $shaC; $arguments.BuildNumber = '3'
    Invoke-FFDailyBuild @arguments
    $state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
    $newEvents = @($harness.Events | Select-Object -Skip $eventStart)
    Assert-FFTest ($newEvents[0] -ceq ('copy:festafiesta-drive:/' + $pendingPublicationName + '/FestaFiesta!.zip')) 'The previous pending upload reuses its version folder and archive name before probing newer source.'
    Assert-FFTest ($harness.BuildCalls -eq 3 -and $state.published.commit -eq $shaC -and $null -eq $state.pending) 'New source is built only after the pending commit has been published.'
    Assert-FFTest ($state.publicationSequence -eq 26 -and $state.published.publicationVersion -eq 26) 'A retry does not consume an extra publication version.'

    $harness.RemoteCommit = $shaD; $arguments.BuildNumber = '4'; $harness.TamperRemoteMd5 = $true
    Assert-FFThrows { Invoke-FFDailyBuild @arguments } 'Incorrect remote hashes fail publication.' 'checksum verification failed'
    $state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
    Assert-FFTest ($state.published.commit -eq $shaC -and $state.pending.commit -eq $shaD) 'A remote checksum mismatch cannot advance the published SHA.'
    $pendingZip = $state.pending.files[0].path
    $originalBytes = [IO.File]::ReadAllBytes($pendingZip)
    [IO.File]::WriteAllText($pendingZip, 'modified artifact')
    $priorBuilds = $harness.BuildCalls; $priorCopies = $harness.CopyCalls
    $harness.TamperRemoteMd5 = $false
    Assert-FFThrows { Invoke-FFDailyBuild @arguments } 'Changed pending bytes are refused.' 'changed on disk'
    Assert-FFTest ($harness.BuildCalls -eq $priorBuilds -and $harness.CopyCalls -eq $priorCopies) 'Tampered pending files are neither rebuilt nor uploaded.'
    [IO.File]::WriteAllBytes($pendingZip, $originalBytes)
    Invoke-FFDailyBuild @arguments
    $state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
    Assert-FFTest ($state.published.commit -eq $shaD -and $harness.BuildCalls -eq $priorBuilds) 'Restoring the original pending bytes allows upload-only recovery.'

    $harness.RemoteCommit = $shaE; $arguments.BuildNumber = '5'; $harness.FailBuild = $true
    Assert-FFThrows { Invoke-FFDailyBuild @arguments } 'A failed package cannot be published.' 'Fixture package failure'
    $state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
    Assert-FFTest ($state.pending.phase -eq 'building' -and $state.published.commit -eq $shaD) 'Interrupted package state retains the last published SHA.'
    $failedPublicationVersion = $state.pending.publicationVersion
    $harness.FailBuild = $false; $arguments.BuildNumber = '6'
    Invoke-FFDailyBuild @arguments
    $state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
    Assert-FFTest ($state.published.commit -eq $shaE -and $null -eq $state.pending) 'An incomplete package is retried using a fresh build label.'
    Assert-FFTest ($state.published.publicationVersion -eq ($failedPublicationVersion + 1)) 'A newly created build after a failed package reserves a fresh publication version.'

    $publicationProbe = [pscustomobject]@{ commit = $shaE; buildLabel = $state.published.buildLabel; publicationName = $state.published.publicationName }
    $probeManifest = Join-Path $arguments.OutputRoot ($publicationProbe.buildLabel + '\Metadata\build-manifest.json')
    $originalManifest = [IO.File]::ReadAllBytes($probeManifest)
    $modifiedManifest = Get-Content -LiteralPath $probeManifest -Raw | ConvertFrom-Json
    $modifiedManifest.source.dirty = $true
    Write-FFState -State $modifiedManifest -Path $probeManifest
    Assert-FFThrows { & $module { param($Options, $Pending) Get-FFPublicationFiles -Options $Options -Pending $Pending } ([pscustomobject]$arguments) $publicationProbe } 'Dirty-source packages cannot enter the upload queue.' 'clean, fully validated'
    [IO.File]::WriteAllBytes($probeManifest, $originalManifest)
    $modifiedManifest = Get-Content -LiteralPath $probeManifest -Raw | ConvertFrom-Json
    $modifiedManifest.validation.graphicsStartupTest = $false
    Write-FFState -State $modifiedManifest -Path $probeManifest
    Assert-FFThrows { & $module { param($Options, $Pending) Get-FFPublicationFiles -Options $Options -Pending $Pending } ([pscustomobject]$arguments) $publicationProbe } 'Packages without graphics verification cannot enter the upload queue.' 'graphicsStartupTest'
    [IO.File]::WriteAllBytes($probeManifest, $originalManifest)

    # A pre-update pending queue contains all four original publication files.
    # Verify their original hashes before reducing it to one ZIP in a version folder.
    $legacyRoot = Join-Path $arguments.OutputRoot $state.published.buildLabel
    $legacyFiles = @()
    foreach ($path in @((Join-Path $legacyRoot ($state.published.buildLabel + '.zip')),
        (Join-Path $legacyRoot 'Metadata\build-manifest.json'), (Join-Path $legacyRoot 'Metadata\ARCHIVE-SHA256.txt'),
        (Join-Path $legacyRoot 'Metadata\verification.json'))) {
        $legacyFiles += [pscustomobject]@{ path = $path; remoteName = [IO.Path]::GetFileName($path)
            bytes = (Get-Item -LiteralPath $path).Length
            sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
            md5 = (Get-FileHash -LiteralPath $path -Algorithm MD5).Hash.ToLowerInvariant() }
    }
    $legacyPending = [pscustomobject]@{ phase = 'ready'; commit = $shaE; buildLabel = $state.published.buildLabel
        startedAtUtc = '2026-11-03T15:00:00Z'; files = $legacyFiles }
    $state.pending = $legacyPending
    $legacySequence = $state.publicationSequence
    Write-FFState -State $state -Path $statePath
    $originalLegacyManifest = [IO.File]::ReadAllBytes($probeManifest)
    [IO.File]::WriteAllText($probeManifest, 'tampered legacy sidecar')
    $priorCopies = $harness.CopyCalls
    Assert-FFThrows { Invoke-FFDailyBuild @arguments } 'Legacy pending sidecars are checked against their recorded bytes.' 'changed on disk'
    Assert-FFTest ($harness.CopyCalls -eq $priorCopies) 'A modified legacy sidecar prevents every upload.'
    [IO.File]::WriteAllBytes($probeManifest, $originalLegacyManifest)
    Invoke-FFDailyBuild @arguments
    $state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
    Assert-FFTest ($harness.CopyCalls -eq ($priorCopies + 1) -and $null -eq $state.pending -and
        $state.published.publicationVersion -eq ($legacySequence + 1) -and
        $state.published.publicationName -ceq ('V{0}(2026.11.04.00:00)' -f ($legacySequence + 1))) 'A legacy four-file pending queue migrates to one verified ZIP with its original Korean build date/time.'

    # Also recover the briefly supported flat Vn(date).zip pending layout.
    $flatVersion = $state.publicationSequence + 1
    $flatName = 'V{0}(2026.11.04).zip' -f $flatVersion
    $flatFile = $legacyFiles[0].PSObject.Copy()
    $flatFile.remoteName = $flatName
    $state.publicationSequence = $flatVersion
    $state.pending = [pscustomobject]@{ phase = 'ready'; commit = $shaE; buildLabel = $state.published.buildLabel
        startedAtUtc = '2026-11-03T15:00:00Z'; publicationVersion = $flatVersion; publicationName = $flatName; files = @($flatFile) }
    Write-FFState -State $state -Path $statePath
    $originalFlatBytes = [IO.File]::ReadAllBytes($flatFile.path)
    [IO.File]::WriteAllText($flatFile.path, 'tampered flat pending ZIP')
    $priorCopies = $harness.CopyCalls; $priorBuilds = $harness.BuildCalls
    Assert-FFThrows { Invoke-FFDailyBuild @arguments } 'A flat pending ZIP must match its recorded bytes before layout migration.' 'changed on disk'
    $failedFlatState = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
    Assert-FFTest ($harness.CopyCalls -eq $priorCopies -and $failedFlatState.pending.publicationName -ceq $flatName) 'A modified flat ZIP prevents upload and preserves the original pending identity.'
    [IO.File]::WriteAllBytes($flatFile.path, $originalFlatBytes)
    $harness.FailCopyAt = $harness.CopyCalls + 1
    Assert-FFThrows { Invoke-FFDailyBuild @arguments } 'A migrated flat pending ZIP remains retryable if uploading fails.' 'Fixture upload failure'
    $migratedFlatState = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
    Assert-FFTest ($migratedFlatState.publicationSequence -eq $flatVersion -and
        $migratedFlatState.pending.publicationName -ceq ('V{0}(2026.11.04.00:00)' -f $flatVersion) -and
        $migratedFlatState.pending.files[0].remoteName -ceq 'FestaFiesta!.zip' -and
        $migratedFlatState.pending.files[0].sha256 -ceq $flatFile.sha256) 'Flat migration preserves the reserved version and ZIP hash, derives the original build time, and saves the new folder layout before uploading.'
    $harness.FailCopyAt = 0
    Invoke-FFDailyBuild @arguments
    $state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
    Assert-FFTest ($null -eq $state.pending -and $state.published.publicationVersion -eq $flatVersion -and
        $state.publicationSequence -eq $flatVersion -and $harness.BuildCalls -eq $priorBuilds -and
        $harness.RemoteFiles.ContainsKey('festafiesta-drive:/' + $state.published.publicationName + '/FestaFiesta!.zip')) 'A migrated flat pending ZIP publishes in its version folder without rebuilding or consuming another version.'

    $dateOnlyVersion = $state.publicationSequence + 1
    $dateOnlyFile = $legacyFiles[0].PSObject.Copy()
    $dateOnlyFile.remoteName = 'FestaFiesta!.zip'
    $state.publicationSequence = $dateOnlyVersion
    $state.pending = [pscustomobject]@{ phase = 'ready'; commit = $shaE; buildLabel = $state.published.buildLabel
        startedAtUtc = '2026-11-03T09:44:30Z'; publicationVersion = $dateOnlyVersion
        publicationName = ('V{0}(2026.11.03)' -f $dateOnlyVersion); files = @($dateOnlyFile) }
    Write-FFState -State $state -Path $statePath
    $harness.FailCopyAt = $harness.CopyCalls + 1
    Assert-FFThrows { Invoke-FFDailyBuild @arguments } 'A date-only folder pending ZIP remains retryable after adding its original build time.' 'Fixture upload failure'
    $timedFolderState = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
    Assert-FFTest ($timedFolderState.publicationSequence -eq $dateOnlyVersion -and
        $timedFolderState.pending.publicationName -ceq ('V{0}(2026.11.03.18:44)' -f $dateOnlyVersion) -and
        $timedFolderState.pending.files[0].remoteName -ceq 'FestaFiesta!.zip' -and
        $timedFolderState.pending.files[0].sha256 -ceq $dateOnlyFile.sha256) 'Adding a missing build time preserves the reserved version and original ZIP hash.'
    $harness.FailCopyAt = 0
    Invoke-FFDailyBuild @arguments
    $state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
    Assert-FFTest ($null -eq $state.pending -and $state.published.publicationVersion -eq $dateOnlyVersion -and
        $harness.RemoteFiles.ContainsKey('festafiesta-drive:/' + $state.published.publicationName + '/FestaFiesta!.zip')) 'A migrated date-only pending build publishes in its folder with the original Korean build time.'
    Assert-FFThrows { & $module { Invoke-FFUploadFile -Options $null -BuildLabel 'safe' -File ([pscustomobject]@{ remoteName = '../V25(2026.11.04).zip' }) } } 'Publication filename traversal is refused before reading local files.' 'Unsafe remote filename'
    Assert-FFThrows { & $module { Invoke-FFUploadFile -Options $null -BuildLabel 'safe' -File ([pscustomobject]@{ remoteName = 'FestaFiesta!.zip' }) -PublicationFolder '../V25(2026.11.04)' } } 'Publication folder traversal is refused before reading local files.' 'Unsafe publication folder'

    $lockPath = Join-Path $arguments.StateRoot 'daily-build.lock'
    $heldLock = New-Object IO.FileStream($lockPath, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    try { Assert-FFThrows { Invoke-FFDailyBuild @arguments } 'Concurrent runs cannot enter the pipeline.' 'state lock' }
    finally { $heldLock.Dispose() }
    $changedDestination = $arguments.Clone(); $changedDestination.DriveFolderId = 'different_drive_folder_123'
    Assert-FFThrows { Invoke-FFDailyBuild @changedDestination } 'The same state cannot silently target a different Drive folder.' 'different driveFolderId'
    Assert-FFTest (@(Get-ChildItem -LiteralPath $arguments.StateRoot -Filter '*.tmp').Count -eq 0) 'Atomic state writes leave no temporary files after completion.'
    Write-Host ("PASS: {0} offline assertions; no real build, Git, or Drive access." -f $script:Assertions) -ForegroundColor Green
}
finally {
    Remove-Module FestaFiesta.Jenkins -Force -ErrorAction SilentlyContinue
    $temporaryRoot = [IO.Path]::GetFullPath($FixtureParent).TrimEnd('\') + '\'
    if ($fixtureFullPath.StartsWith($temporaryRoot, [StringComparison]::OrdinalIgnoreCase) -and
        [IO.Path]::GetFileName($fixtureFullPath) -match '^ff-jenkins-test-[a-f0-9]{32}$' -and
        (Test-Path -LiteralPath $fixtureFullPath)) {
        Remove-Item -LiteralPath $fixtureFullPath -Recurse -Force
    }
}
