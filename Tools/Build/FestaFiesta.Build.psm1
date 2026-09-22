Set-StrictMode -Version Latest

function Get-FFProjectRoot {
    [CmdletBinding()]
    param()

    return [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
}

function Get-FFProjectFile {
    [CmdletBinding()]
    param()

    $projectFile = Join-Path (Get-FFProjectRoot) 'FestaFiesta.uproject'
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

function Get-FFGitMetadata {
    [CmdletBinding()]
    param()

    $projectRoot = Get-FFProjectRoot
    $safeRoot = $projectRoot.Replace('\', '/')

    $commit = (& git -c "safe.directory=$safeRoot" rev-parse --short=12 HEAD 2>$null | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($commit)) {
        $commit = 'nogit'
    }

    $branch = (& git -c "safe.directory=$safeRoot" branch --show-current 2>$null | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($branch)) {
        $branch = 'detached'
    }

    $status = (& git -c "safe.directory=$safeRoot" status --porcelain --untracked-files=normal 2>$null | Out-String).Trim()
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
    $versionOutput = @(& git -c "safe.directory=$safeRoot" lfs version 2>&1)
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

    $fsckOutput = @(& git -c "safe.directory=$safeRoot" lfs fsck 2>&1)
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
    'Get-FFProjectRoot',
    'Get-FFProjectFile',
    'Resolve-FFEngineRoot',
    'Get-FFEngineVersion',
    'Get-FFProjectVersion',
    'Get-FFGitMetadata',
    'Assert-FFAssetsMaterialized',
    'Assert-FFGitLfsHealth',
    'Get-FFGameBinaryBaseName',
    'Get-FFRelativePath',
    'Write-FFSha256Manifest',
    'Write-FFSizeReport'
)
