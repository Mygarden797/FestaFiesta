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
        [string] $ExpectedVersion
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
    if (-not [string]::IsNullOrWhiteSpace($ExpectedVersion) -and
        $installedVersion -ne $ExpectedVersion) {
        throw "Git LFS version mismatch (expected $ExpectedVersion, found $installedVersion)."
    }

    $fsckOutput = @(& git -c "safe.directory=$safeRoot" lfs fsck 2>&1)
    $fsckExitCode = $LASTEXITCODE
    if ($fsckExitCode -ne 0) {
        throw "Git LFS integrity check failed with exit code ${fsckExitCode}: $($fsckOutput -join ' ')"
    }

    return $installedVersion
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

Export-ModuleMember -Function @(
    'Get-FFProjectRoot',
    'Get-FFProjectFile',
    'Resolve-FFEngineRoot',
    'Get-FFEngineVersion',
    'Get-FFProjectVersion',
    'Get-FFGitMetadata',
    'Assert-FFAssetsMaterialized',
    'Assert-FFGitLfsHealth',
    'Get-FFRelativePath',
    'Write-FFSha256Manifest'
)
