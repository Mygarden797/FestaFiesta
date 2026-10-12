# FestaFiesta.Build.psm1
# 역할: 빌드 도구가 공유하는 프로젝트·엔진 탐색, LFS 확인, 결과 기록 함수를 제공한다.

Set-StrictMode -Version Latest

$script:FFProjectFileOverride = $null

function Set-FFProjectFileOverride {
    [CmdletBinding()]
    param(
        [string] $ProjectFile
    )

    if ([string]::IsNullOrWhiteSpace($ProjectFile)) {
        $script:FFProjectFileOverride = $null
        return
    }

    # Keep the caller's short junction path. Resolving its physical target here
    # would put the long path back into UBT's Cook output directory.
    $candidate = [System.IO.Path]::GetFullPath($ProjectFile)
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
        throw "Unreal project file was not found: $candidate"
    }
    if ([System.IO.Path]::GetFileName($candidate) -cne 'FestaFiesta.uproject') {
        throw "Expected FestaFiesta.uproject, got: $candidate"
    }

    $referenceFile = Join-Path ([System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))) 'FestaFiesta.uproject'
    $reference = Get-Content -LiteralPath $referenceFile -Raw | ConvertFrom-Json
    $descriptor = Get-Content -LiteralPath $candidate -Raw | ConvertFrom-Json
    $runtimeModules = @($descriptor.Modules | Where-Object { $_.Name -ceq 'FestaFiesta' -and $_.Type -ceq 'Runtime' })
    $candidateRoot = [System.IO.Path]::GetDirectoryName($candidate)
    $referenceConfig = Join-Path ([System.IO.Path]::GetDirectoryName($referenceFile)) 'Config\DefaultGame.ini'
    $candidateConfig = Join-Path $candidateRoot 'Config\DefaultGame.ini'
    $gameTarget = Join-Path $candidateRoot 'Source\FestaFiesta.Target.cs'
    $editorTarget = Join-Path $candidateRoot 'Source\FestaFiestaEditor.Target.cs'
    if (-not (Test-Path -LiteralPath $candidateConfig -PathType Leaf) -or
        $runtimeModules.Count -ne 1 -or
        -not (Test-Path -LiteralPath $gameTarget -PathType Leaf) -or
        -not (Test-Path -LiteralPath $editorTarget -PathType Leaf)) {
        throw "ProjectFile is not this FestaFiesta project (runtime module, targets, or config differ): $candidate"
    }
    $gameTargetSource = Get-Content -LiteralPath $gameTarget -Raw
    $editorTargetSource = Get-Content -LiteralPath $editorTarget -Raw
    if ($gameTargetSource -notmatch '\bclass\s+FestaFiestaTarget\s*:\s*TargetRules\b' -or
        $gameTargetSource -notmatch 'Type\s*=\s*TargetType\.Game' -or
        $editorTargetSource -notmatch '\bclass\s+FestaFiestaEditorTarget\s*:\s*TargetRules\b' -or
        $editorTargetSource -notmatch 'Type\s*=\s*TargetType\.Editor') {
        throw "ProjectFile targets are not the FestaFiesta Game and Editor targets: $candidate"
    }
    $referenceProjectId = @(Select-String -LiteralPath $referenceConfig -Pattern '^ProjectID=(.+)$' | Select-Object -Last 1)
    $candidateProjectId = @(Select-String -LiteralPath $candidateConfig -Pattern '^ProjectID=(.+)$' | Select-Object -Last 1)
    if ($referenceProjectId.Count -ne 1 -or $candidateProjectId.Count -ne 1 -or
        $referenceProjectId[0].Matches[0].Groups[1].Value -cne $candidateProjectId[0].Matches[0].Groups[1].Value -or
        [string]$descriptor.EngineAssociation -cne [string]$reference.EngineAssociation) {
        throw "ProjectFile is not this FestaFiesta project (project identity or EngineAssociation differs): $candidate"
    }

    $script:FFProjectFileOverride = $candidate
}

function Get-FFProjectRoot {
    [CmdletBinding()]
    param()

    if ($script:FFProjectFileOverride) {
        return [System.IO.Path]::GetDirectoryName($script:FFProjectFileOverride)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
}

function Get-FFBuildDdcConfiguration {
    [CmdletBinding()]
    param()

    $graph = 'FestaFiestaBuildDDC'
    $root = Get-FFProjectRoot
    $config = Join-Path $root 'Config\DefaultEngine.ini'
    if (-not (Select-String -LiteralPath $config -Pattern "^\[$graph\]$" -Quiet)) {
        throw "The project-owned build DDC graph is missing from $config."
    }
    $localPath = [System.IO.Path]::GetFullPath((Join-Path $root 'DerivedDataCache\BuildFileSystem'))
    [System.IO.Directory]::CreateDirectory($localPath) | Out-Null
    return [pscustomobject]@{
        Graph = $graph
        Argument = "-ddc=$graph"
        LocalPath = $localPath
    }
}

function Assert-FFBuildDdcSelection {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string] $LogPath
    )

    # Engine source: DerivedDataBackends.cpp falls back when a named graph
    # cannot be opened; FileSystemCacheStore.cpp logs its actual writable path.
    # Generic LogZenServiceInstance startup is not proof of a Zen DDC backend.
    $log = Get-Content -LiteralPath $LogPath -Raw
    if ($log -match "Unable to create or use cache graph 'FestaFiestaBuildDDC'" -or
        $log -match 'LogDerivedDataCache[^\r\n]*Using ZenServer HTTP service') {
        throw "Build DDC fell back or activated a Zen HTTP cache. See: $LogPath"
    }
    $cacheMatches = [regex]::Matches($log,
        'LogDerivedDataCache[^\r\n]*FFBuildLocal: Using data cache path (.+?): Writable')
    if ($cacheMatches.Count -eq 0) {
        throw "Build DDC did not report its writable filesystem cache. See: $LogPath"
    }
    foreach ($match in $cacheMatches) {
        $cachePath = $match.Groups[1].Value.Trim().Trim('"').Replace('\', '/')
        if ($cachePath -notmatch '(?i)/DerivedDataCache/BuildFileSystem/?$') {
            throw "Build DDC selected an unexpected filesystem path '$cachePath'. See: $LogPath"
        }
    }
    return $cacheMatches[$cacheMatches.Count - 1].Groups[1].Value.Trim()
}

function Get-FFProjectFile {
    [CmdletBinding()]
    param()

    $projectFile = if ($script:FFProjectFileOverride) {
        $script:FFProjectFileOverride
    }
    else {
        Join-Path (Get-FFProjectRoot) 'FestaFiesta.uproject'
    }
    if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf)) {
        throw "Unreal project file was not found: $projectFile"
    }

    return $projectFile
}

function Resolve-FFEngineRoot {
    [CmdletBinding()]
    param(
        [string] $EngineRoot
    )

    $projectFile = Get-FFProjectFile
    $projectDescriptor = Get-Content -LiteralPath $projectFile -Raw | ConvertFrom-Json
    $association = [string]$projectDescriptor.EngineAssociation

    $candidates = New-Object System.Collections.Generic.List[string]
    if (-not [string]::IsNullOrWhiteSpace($EngineRoot)) {
        $candidates.Add($EngineRoot)
    }
    if (-not [string]::IsNullOrWhiteSpace($env:FESTAFIESTA_UE_ROOT)) {
        $candidates.Add($env:FESTAFIESTA_UE_ROOT)
    }
    if (-not [string]::IsNullOrWhiteSpace($association)) {
        # Launcher manifest도 읽어 C:\Unreal 같은 팀원별 설치 위치를 자동으로 찾는다.
        $launcherManifest = Join-Path $env:ProgramData 'Epic\UnrealEngineLauncher\LauncherInstalled.dat'
        if (Test-Path -LiteralPath $launcherManifest -PathType Leaf) {
            try {
                $launcherData = Get-Content -LiteralPath $launcherManifest -Raw | ConvertFrom-Json
                foreach ($installation in $launcherData.InstallationList) {
                    if ($installation.AppName -eq "UE_$association") {
                        $candidates.Add([string]$installation.InstallLocation)
                    }
                }
            }
            catch {
                Write-Verbose "Launcher manifest could not be read: $($_.Exception.Message)"
            }
        }
        $candidates.Add((Join-Path $env:ProgramFiles "Epic Games\UE_$association"))

        try {
            $registeredBuilds = Get-ItemProperty -LiteralPath 'HKCU:\Software\Epic Games\Unreal Engine\Builds' -ErrorAction Stop
            $registeredPath = $registeredBuilds.PSObject.Properties |
                Where-Object { $_.Name -eq $association } |
                Select-Object -First 1 -ExpandProperty Value
            if (-not [string]::IsNullOrWhiteSpace([string]$registeredPath)) {
                $candidates.Add([string]$registeredPath)
            }
        }
        catch {
            # Launcher installations normally resolve through Program Files. A missing
            # per-user registration key is therefore not an error by itself.
        }
    }

    foreach ($candidate in $candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) {
            continue
        }

        $fullCandidate = [System.IO.Path]::GetFullPath($candidate)
        $runUat = Join-Path $fullCandidate 'Engine\Build\BatchFiles\RunUAT.bat'
        $editorCmd = Join-Path $fullCandidate 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
        if ((Test-Path -LiteralPath $runUat -PathType Leaf) -and
            (Test-Path -LiteralPath $editorCmd -PathType Leaf)) {
            return $fullCandidate
        }
    }

    $candidateText = if ($candidates.Count -gt 0) {
        ($candidates | ForEach-Object { "  - $_" }) -join [Environment]::NewLine
    }
    else {
        '  (none)'
    }
    throw "A compatible Unreal Engine installation could not be found. Checked:$([Environment]::NewLine)$candidateText$([Environment]::NewLine)Pass -EngineRoot or set FESTAFIESTA_UE_ROOT."
}

function Get-FFEngineVersion {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string] $EngineRoot
    )

    $buildVersionFile = Join-Path $EngineRoot 'Engine\Build\Build.version'
    if (-not (Test-Path -LiteralPath $buildVersionFile -PathType Leaf)) {
        return 'unknown'
    }

    $buildVersion = Get-Content -LiteralPath $buildVersionFile -Raw | ConvertFrom-Json
    return '{0}.{1}.{2}' -f $buildVersion.MajorVersion, $buildVersion.MinorVersion, $buildVersion.PatchVersion
}

function Resolve-FFMsvcInstallation {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [ValidatePattern('^[0-9]+\.[0-9]+\.[0-9]+$')]
        [string] $InstallVersion,

        # Optional discovery paths also let a build worker use a supplied locator.
        [string] $VsWherePath = (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'),

        [string] $VisualStudio2022Root = (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\2022')
    )

    $installationRoots = New-Object System.Collections.Generic.List[string]
    if (Test-Path -LiteralPath $VsWherePath -PathType Leaf) {
        # Do not use -latest: an older VS instance may hold the locked compiler.
        # -products * includes standalone Build Tools and custom install paths.
        # Validate the exact binaries below instead of requiring the component
        # ID for the latest VC tools; a worker may install only a pinned version.
        $discoveredRoots = @(& $VsWherePath -products '*' -version '[17.0,18.0)' -property installationPath)
        if ($LASTEXITCODE -ne 0) {
            throw "Visual Studio discovery failed with exit code ${LASTEXITCODE}: $VsWherePath"
        }
        foreach ($root in $discoveredRoots) {
            if (-not [string]::IsNullOrWhiteSpace([string]$root)) {
                $installationRoots.Add(([string]$root).Trim())
            }
        }
    }
    # Preserve standard-location discovery on machines without vswhere.
    if (Test-Path -LiteralPath $VisualStudio2022Root -PathType Container) {
        foreach ($installation in (Get-ChildItem -LiteralPath $VisualStudio2022Root -Directory)) {
            $installationRoots.Add($installation.FullName)
        }
    }

    foreach ($root in ($installationRoots | Select-Object -Unique)) {
        $toolchainRoot = Join-Path $root "VC\Tools\MSVC\$InstallVersion"
        $compilerRoot = Join-Path $toolchainRoot 'bin\Hostx64\x64'
        # A leftover version directory is not an installed Win64 compiler.
        if ((Test-Path -LiteralPath (Join-Path $compilerRoot 'cl.exe') -PathType Leaf) -and
            (Test-Path -LiteralPath (Join-Path $compilerRoot 'link.exe') -PathType Leaf)) {
            return [System.IO.Path]::GetFullPath($toolchainRoot)
        }
    }

    $checkedRoots = if ($installationRoots.Count -gt 0) {
        ($installationRoots | Select-Object -Unique) -join ', '
    }
    else { "$VisualStudio2022Root (no installations discovered)" }
    throw "Locked MSVC install $InstallVersion with Win64 cl.exe and link.exe was not found. Checked: $checkedRoots"
}

function Get-FFProjectVersion {
    [CmdletBinding()]
    param()

    $defaultGame = Join-Path (Get-FFProjectRoot) 'Config\DefaultGame.ini'
    $versionLine = Select-String -LiteralPath $defaultGame -Pattern '^ProjectVersion=(.+)$' |
        Select-Object -Last 1
    if ($null -eq $versionLine) {
        return 'unversioned'
    }

    return $versionLine.Matches[0].Groups[1].Value.Trim()
}

function Assert-FFCookPathBudget {
    [CmdletBinding()]
    param()

    $projectRoot = Get-FFProjectRoot
    $contentRoot = Join-Path $projectRoot 'Content'
    $cookedContentRoot = Join-Path $projectRoot 'Saved\Cooked\Windows\FestaFiesta\Content'
    if (-not (Test-Path -LiteralPath $contentRoot -PathType Container)) {
        throw "Content directory was not found: $contentRoot"
    }

    $overlong = New-Object System.Collections.Generic.List[string]
    $longestLength = 0
    $assetCount = 0
    foreach ($asset in (Get-ChildItem -LiteralPath $contentRoot -Recurse -File)) {
        if ($asset.Extension -notin @('.uasset', '.umap')) {
            continue
        }
        ++$assetCount
        $relativePath = Get-FFRelativePath -BasePath $contentRoot -Path $asset.FullName
        $cookPath = Join-Path $cookedContentRoot $relativePath
        $longestLength = [Math]::Max($longestLength, $cookPath.Length)
        if ($cookPath.Length -ge 260) {
            $overlong.Add("$($cookPath.Length) chars: $cookPath")
        }
    }

    if ($overlong.Count -gt 0) {
        $examples = ($overlong | Select-Object -First 3) -join [Environment]::NewLine
        throw "Cook output path reaches Windows MAX_PATH for $($overlong.Count) asset(s). Examples:$([Environment]::NewLine)$examples$([Environment]::NewLine)Use a short checkout or junction such as C:\FFProj and pass -ProjectFile C:\FFProj\FestaFiesta.uproject. No source content needs deletion."
    }

    return [PSCustomObject]@{
        AssetCount = $assetCount
        LongestCookPathLength = $longestLength
    }
}

function Get-FFGitMetadata {
    [CmdletBinding()]
    param()

    $projectRoot = Get-FFProjectRoot
    $safeRoot = $projectRoot.Replace('\', '/')

    $commit = (& git -c "safe.directory=$safeRoot" -C $projectRoot rev-parse --short=12 HEAD 2>$null | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($commit)) {
        $commit = 'nogit'
    }

    $branch = (& git -c "safe.directory=$safeRoot" -C $projectRoot branch --show-current 2>$null | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($branch)) {
        $branch = 'detached'
    }

    $status = (& git -c "safe.directory=$safeRoot" -C $projectRoot status --porcelain --untracked-files=normal 2>$null | Out-String).Trim()
    $dirty = $LASTEXITCODE -ne 0 -or -not [string]::IsNullOrWhiteSpace($status)

    return [PSCustomObject]@{
        Commit = $commit
        Branch = $branch
        Dirty  = $dirty
    }
}

function Assert-FFAssetsMaterialized {
    [CmdletBinding()]
    param()

    $projectRoot = Get-FFProjectRoot
    $scanRoots = @(
        (Join-Path $projectRoot 'Content'),
        (Join-Path $projectRoot 'Plugins')
    ) | Where-Object { Test-Path -LiteralPath $_ -PathType Container }

    $pointerSignature = 'version https://git-lfs.github.com/spec/v1'
    $pointers = New-Object System.Collections.Generic.List[string]

    foreach ($scanRoot in $scanRoots) {
        $smallFiles = Get-ChildItem -LiteralPath $scanRoot -Recurse -File |
            Where-Object { $_.Length -le 2048 }
        foreach ($file in $smallFiles) {
            $stream = [System.IO.File]::OpenRead($file.FullName)
            try {
                $buffer = New-Object byte[] ([Math]::Min(128, [int]$stream.Length))
                $bytesRead = $stream.Read($buffer, 0, $buffer.Length)
                $header = [System.Text.Encoding]::ASCII.GetString($buffer, 0, $bytesRead)
                if ($header.StartsWith($pointerSignature, [System.StringComparison]::Ordinal)) {
                    $pointers.Add($file.FullName)
                }
            }
            finally {
                $stream.Dispose()
            }
        }
    }

    if ($pointers.Count -gt 0) {
        $pointerText = ($pointers | ForEach-Object { "  - $_" }) -join [Environment]::NewLine
        throw "Git LFS objects are not materialized. Run 'git lfs pull' before building:$([Environment]::NewLine)$pointerText"
    }
}

function Assert-FFGitLfsHealth {
    [CmdletBinding()]
    param(
        # The LFS client only materializes files and `lfs fsck` proves their
        # integrity, so it does not shape build output. Require a floor rather
        # than an exact patch; exact pins stay reserved for engine/MSVC/SDK.
        [string] $MinimumVersion
    )

    $projectRoot = Get-FFProjectRoot
    $safeRoot = $projectRoot.Replace('\', '/')
    $versionOutput = @(& git -c "safe.directory=$safeRoot" -C $projectRoot lfs version 2>&1)
    $versionExitCode = $LASTEXITCODE
    if ($versionExitCode -ne 0) {
        throw "Git LFS is unavailable or failed with exit code ${versionExitCode}: $($versionOutput -join ' ')"
    }

    $versionText = $versionOutput -join ' '
    $versionMatch = [regex]::Match($versionText, 'git-lfs/([0-9]+(?:\.[0-9]+){2})')
    if (-not $versionMatch.Success) {
        throw "Could not parse the Git LFS version from: $versionText"
    }

    $installedVersion = $versionMatch.Groups[1].Value
    if (-not [string]::IsNullOrWhiteSpace($MinimumVersion) -and
        [version]$installedVersion -lt [version]$MinimumVersion) {
        throw "Git LFS is too old (minimum $MinimumVersion, found $installedVersion)."
    }

    $fsckOutput = @(& git -c "safe.directory=$safeRoot" -C $projectRoot lfs fsck 2>&1)
    $fsckExitCode = $LASTEXITCODE
    if ($fsckExitCode -ne 0) {
        throw "Git LFS integrity check failed with exit code ${fsckExitCode}: $($fsckOutput -join ' ')"
    }

    return $installedVersion
}

function Get-FFGameBinaryBaseName {
    [CmdletBinding()]
    param(
        [ValidateSet('Shipping', 'Development')]
        [string] $Configuration = 'Shipping'
    )

    # UE 5.7 TargetRules.UndecoratedConfiguration defaults to Development;
    # UEBuildTarget.MakeBinaryFileName adds -Platform-Configuration otherwise.
    # FestaFiesta.Target.cs uses that default for its Win64 Game target.
    if ($Configuration -eq 'Development') {
        return 'FestaFiesta'
    }
    return 'FestaFiesta-Win64-Shipping'
}

function Get-FFRelativePath {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string] $BasePath,

        [Parameter(Mandatory = $true)]
        [string] $Path
    )

    $baseFullPath = [System.IO.Path]::GetFullPath($BasePath).TrimEnd('\', '/')
    $fileFullPath = [System.IO.Path]::GetFullPath($Path)
    $prefix = $baseFullPath + [System.IO.Path]::DirectorySeparatorChar
    if (-not $fileFullPath.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Path '$fileFullPath' is outside '$baseFullPath'."
    }

    return $fileFullPath.Substring($prefix.Length).Replace('\', '/')
}

function Write-FFSha256Manifest {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string] $RootPath,

        [Parameter(Mandatory = $true)]
        [string] $OutputFile
    )

    $resolvedRoot = (Resolve-Path -LiteralPath $RootPath).Path
    $lines = Get-ChildItem -LiteralPath $resolvedRoot -Recurse -File |
        Sort-Object FullName |
        ForEach-Object {
            $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            $relativePath = Get-FFRelativePath -BasePath $resolvedRoot -Path $_.FullName
            "$hash  $relativePath"
        }

    [System.IO.File]::WriteAllLines(
        [System.IO.Path]::GetFullPath($OutputFile),
        [string[]]$lines,
        (New-Object System.Text.UTF8Encoding($false)))
}

function Get-FFContentBucket {
    param(
        [string] $FileName,
        [string] $ChunkType
    )

    $path = $FileName.Replace('\', '/')
    if ($path -match '/ShaderArchive-([^/]+)\.ushaderbytecode$') {
        return "Shaders/$($Matches[1])"
    }
    # IoStore names chunks without a package file as <ShaderCode>, <ContainerHeader>, ...
    if ($path -match '^<(.+)>$') {
        return $(if ($Matches[1] -eq 'ShaderCode') { 'Shaders/ShaderCode' } else { "Internal/$($Matches[1])" })
    }
    if ($path -match '^\.\./\.\./\.\./FestaFiesta/Content/([^/]+)(?:/([^/]+))?/') {
        $second = if ($Matches[2]) { "/$($Matches[2])" } else { '' }
        return "Game/$($Matches[1])$second"
    }
    if ($path -match '^\.\./\.\./\.\./FestaFiesta/Plugins/(?:.+/)?([^/]+)/Content/') {
        return "ProjectPlugin/$($Matches[1])"
    }
    if ($path -match '^\.\./\.\./\.\./Engine/Plugins/(?:.+/)?([^/]+)/Content/') {
        return "EnginePlugin/$($Matches[1])"
    }
    if ($path -match '^\.\./\.\./\.\./Engine/Content/([^/]+)/') {
        return "Engine/$($Matches[1])"
    }
    if ([string]::IsNullOrWhiteSpace($path)) {
        return "Unnamed/$ChunkType"
    }
    return 'Other'
}

function Write-FFSizeReport {
    <#
    Summarizes where the shipped bytes go so every release can be compared
    against the previous one. Container sizes come from the IoStore listing
    (compressed, i.e. what the player downloads and reads from disk).
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string] $PayloadRoot,

        [Parameter(Mandatory = $true)]
        [string] $IoStoreCsv,

        [Parameter(Mandatory = $true)]
        [string] $OutputFile,

        [string] $ArchivePath,

        [string] $PreviousReport,

        [int] $TopCount = 30
    )

    $bucketBytes = @{}
    $chunkTypeBytes = @{}
    $assetBytes = @{}
    $uncompressedTotal = [int64]0
    $compressedTotal = [int64]0
    $reader = New-Object System.IO.StreamReader($IoStoreCsv)
    try {
        $null = $reader.ReadLine()
        while ($null -ne ($line = $reader.ReadLine())) {
            $fields = $line.Split(',')
            if ($fields.Count -lt 17) {
                continue
            }
            # Numeric and trailing columns are read from the end so a comma in a
            # file name cannot shift them.
            $count = $fields.Count
            $size = [int64]$fields[$count - 9].Trim()
            $compressed = [int64]$fields[$count - 8].Trim()
            $chunkType = $fields[$count - 6].Trim()
            $fileName = ($fields[4..($count - 13)] -join ',').Trim()

            $uncompressedTotal += $size
            $compressedTotal += $compressed
            $bucket = Get-FFContentBucket -FileName $fileName -ChunkType $chunkType
            $bucketBytes[$bucket] = [int64]$bucketBytes[$bucket] + $compressed
            $chunkTypeBytes[$chunkType] = [int64]$chunkTypeBytes[$chunkType] + $compressed
            if (-not [string]::IsNullOrWhiteSpace($fileName)) {
                # .uasset/.uexp/.ubulk/.uptnl of one package are reported together.
                # String ops only: cooked names may contain characters Path APIs reject.
                $asset = ($fileName.Replace('\', '/') -replace '\.[^./]+$', '') -replace '^(\.\./)+', ''
                $assetBytes[$asset] = [int64]$assetBytes[$asset] + $compressed
            }
        }
    }
    finally {
        $reader.Dispose()
    }

    $payloadFiles = @(Get-ChildItem -LiteralPath $PayloadRoot -Recurse -File)
    $payloadGroups = @{}
    foreach ($file in $payloadFiles) {
        $relative = Get-FFRelativePath -BasePath $PayloadRoot -Path $file.FullName
        $group = switch -Regex ($relative) {
            '^FestaFiesta/Content/Paks/' { 'containers'; break }
            '^FestaFiesta/Binaries/' { 'gameBinaries'; break }
            '^Engine/Extras/Redist/' { 'prerequisites'; break }
            '^Engine/Binaries/' { 'engineBinaries'; break }
            '^Engine/' { 'engineOther'; break }
            '^[^/]+\.exe$' { 'launcher'; break }
            default { 'documents' }
        }
        $payloadGroups[$group] = [int64]$payloadGroups[$group] + $file.Length
    }

    $toSortedList = {
        param($Table, $Take)
        $rows = $Table.GetEnumerator() | Sort-Object Value -Descending
        if ($Take -gt 0) {
            $rows = $rows | Select-Object -First $Take
        }
        @($rows | ForEach-Object { [ordered]@{ name = $_.Key; bytes = [int64]$_.Value } })
    }

    $report = [ordered]@{
        schemaVersion = 1
        payloadBytes = [int64]($payloadFiles | Measure-Object Length -Sum).Sum
        archiveBytes = if ($ArchivePath -and (Test-Path -LiteralPath $ArchivePath -PathType Leaf)) {
            (Get-Item -LiteralPath $ArchivePath).Length
        }
        else {
            $null
        }
        payloadGroups = & $toSortedList $payloadGroups 0
        mainContainer = [ordered]@{
            uncompressedBytes = $uncompressedTotal
            compressedBytes = $compressedTotal
            byBucket = & $toSortedList $bucketBytes 0
            byChunkType = & $toSortedList $chunkTypeBytes 0
            largestAssets = & $toSortedList $assetBytes $TopCount
        }
        comparedTo = $null
    }

    if ($PreviousReport -and (Test-Path -LiteralPath $PreviousReport -PathType Leaf)) {
        $previous = Get-Content -LiteralPath $PreviousReport -Raw | ConvertFrom-Json
        $report.comparedTo = [ordered]@{
            report = $PreviousReport
            payloadBytesDelta = $report.payloadBytes - [int64]$previous.payloadBytes
            archiveBytesDelta = if ($null -ne $report.archiveBytes -and $null -ne $previous.archiveBytes) {
                [int64]$report.archiveBytes - [int64]$previous.archiveBytes
            }
            else {
                $null
            }
            containerBytesDelta = $compressedTotal - [int64]$previous.mainContainer.compressedBytes
        }
    }

    [System.IO.File]::WriteAllText(
        [System.IO.Path]::GetFullPath($OutputFile),
        ($report | ConvertTo-Json -Depth 6) + [Environment]::NewLine,
        (New-Object System.Text.UTF8Encoding($false)))
    return $report
}

Export-ModuleMember -Function @(
    'Set-FFProjectFileOverride',
    'Get-FFProjectRoot',
    'Get-FFProjectFile',
    'Get-FFBuildDdcConfiguration',
    'Assert-FFBuildDdcSelection',
    'Assert-FFCookPathBudget',
    'Resolve-FFEngineRoot',
    'Get-FFEngineVersion',
    'Resolve-FFMsvcInstallation',
    'Get-FFProjectVersion',
    'Get-FFGitMetadata',
    'Assert-FFAssetsMaterialized',
    'Assert-FFGitLfsHealth',
    'Get-FFGameBinaryBaseName',
    'Get-FFRelativePath',
    'Write-FFSha256Manifest',
    'Write-FFSizeReport'
)
