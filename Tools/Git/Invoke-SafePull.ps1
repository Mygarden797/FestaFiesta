<#
.SYNOPSIS
    Guards 'git pull' against the Windows memory-mapped .uasset failure and repairs
    the half-applied working tree it leaves behind.

.DESCRIPTION
    On Windows, DeleteFileW fails with ERROR_USER_MAPPED_FILE(1224) when another
    process holds a memory-mapped section on the file. The CRT has no mapping for
    1224 and falls back to EINVAL, so git prints:

        error: unable to unlink old '<file>': Invalid argument

    git only retries unlink on sharing-violation and access-denied errors, so it
    gives up immediately here. The damage is that checkout has already rewritten
    the working tree by then; only the fast-forward is cancelled. The repository
    is left split:

        working tree = new commit
        HEAD / index = old commit

    In that state 'git status' shows thousands of incoming files as untracked and
    the rest as local modifications. Switching branches and committing on top of
    it silently commits rolled-back assets.

    This script blocks the pull while the Unreal Editor is running, and if the
    failure happens anyway it advances HEAD/index only (mixed reset) instead of
    re-smudging thousands of LFS files, then restores the files that failed.

    See Tools/Git/README_KO.md for the Korean walkthrough and manual recovery.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File Tools\Git\Invoke-SafePull.ps1

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File Tools\Git\Invoke-SafePull.ps1 -Branch Dev
#>
[CmdletBinding()]
param(
    [string] $Remote = 'origin',

    [string] $Branch,

    [ValidateRange(1, 10)]
    [int] $RestoreRetries = 3
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# These keep .uasset files memory-mapped, which is what breaks the pull.
$BlockingProcessNames = @(
    'UnrealEditor',
    'UnrealEditor-Cmd',
    'UnrealEditor-Win64-DebugGame',
    'UnrealEditor-Win64-Development',
    'UnrealLightmass',
    'ShaderCompileWorker',
    'FestaFiesta'
)

function Get-GitValue {
    param(
        [Parameter(Mandatory = $true)]
        [string[]] $Arguments
    )

    $value = & git @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "git $($Arguments -join ' ') failed (exit $LASTEXITCODE)"
    }
    return ($value | Out-String).Trim()
}

function Get-DirtyTrackedPaths {
    # Untracked files do not affect whether a mixed reset is safe, so skip them.
    return @(& git status --porcelain --untracked-files=no)
}

function Write-Section {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Text
    )

    Write-Host ''
    Write-Host "== $Text" -ForegroundColor Cyan
}

$repoRoot = Get-GitValue @('rev-parse', '--show-toplevel')
Set-Location -LiteralPath $repoRoot

if (-not $Branch) {
    $Branch = Get-GitValue @('rev-parse', '--abbrev-ref', 'HEAD')
}

# --- Precheck: processes that memory-map assets -------------------------------
Write-Section 'Precheck'

$running = @(Get-Process -Name $BlockingProcessNames -ErrorAction SilentlyContinue)
if ($running.Count -gt 0) {
    $names = ($running | Select-Object -ExpandProperty ProcessName -Unique) -join ', '
    Write-Host "Aborted: these processes are running: $names" -ForegroundColor Red
    Write-Host 'Close the Unreal Editor completely, then run again.' -ForegroundColor Red
    exit 1
}
Write-Host 'Unreal Editor: not running (OK)'

$dirtyBefore = Get-DirtyTrackedPaths
Write-Host "Locally modified tracked files before pull: $($dirtyBefore.Count)"

# --- Pull ---------------------------------------------------------------------
Write-Section "git pull $Remote $Branch"

# PowerShell 5.1 wraps native stderr in ErrorRecords and clobbers $? when you use
# 2>&1, so redirect at the cmd level to get the exit code and output verbatim.
$logPath = Join-Path ([System.IO.Path]::GetTempPath()) ("ff-safe-pull-{0}.log" -f $PID)
& cmd /c "git pull `"$Remote`" `"$Branch`" > `"$logPath`" 2>&1"
$pullExit = $LASTEXITCODE

$pullOutput = @()
if (Test-Path -LiteralPath $logPath) {
    $pullOutput = @(Get-Content -LiteralPath $logPath)
    Remove-Item -LiteralPath $logPath -Force
}
$pullOutput | ForEach-Object { Write-Host $_ }

if ($pullExit -eq 0) {
    Write-Section 'Done'
    Write-Host 'Pull succeeded.' -ForegroundColor Green
    exit 0
}

# --- Is this the failure we handle? ------------------------------------------
$unlinkFailures = @()
foreach ($line in $pullOutput) {
    $match = [regex]::Match($line, "unable to unlink old '(?<path>[^']+)'")
    if ($match.Success) {
        $unlinkFailures += $match.Groups['path'].Value
    }
}

if ($unlinkFailures.Count -eq 0) {
    Write-Section 'Failed'
    Write-Host "Pull failed with exit $pullExit but not on an unlink error." -ForegroundColor Yellow
    Write-Host 'Read the output above. This script does not auto-repair that.' -ForegroundColor Yellow
    exit $pullExit
}

Write-Section 'Repairing half-applied working tree'
Write-Host 'Files git could not replace:'
$unlinkFailures | ForEach-Object { Write-Host "  $_" }

# --- Can we repair automatically? --------------------------------------------
# If the tree was clean before the pull, every tracked file that is dirty now was
# written by the pull itself, so resetting to the target commit loses no work.
if ($dirtyBefore.Count -gt 0) {
    Write-Host ''
    Write-Host 'Not auto-repairing: the tree already had local modifications before' -ForegroundColor Yellow
    Write-Host 'the pull, so your edits cannot be told apart from the pull output.' -ForegroundColor Yellow
    Write-Host 'Follow the manual recovery in Tools/Git/README_KO.md.' -ForegroundColor Yellow
    exit 1
}

$target = Get-GitValue @('rev-parse', 'FETCH_HEAD')

& git merge-base --is-ancestor HEAD FETCH_HEAD
if ($LASTEXITCODE -ne 0) {
    Write-Host ''
    Write-Host 'Not auto-repairing: this pull needed a real merge, not a fast-forward.' -ForegroundColor Yellow
    Write-Host 'Follow the manual recovery in Tools/Git/README_KO.md.' -ForegroundColor Yellow
    exit 1
}

# The working tree already holds the target content, so move HEAD and the index
# only. A --hard reset would needlessly re-smudge thousands of LFS files.
Write-Host ''
Write-Host "Advancing HEAD/index to $target (working tree untouched)."
& git reset --quiet $target
if ($LASTEXITCODE -ne 0) {
    throw "git reset $target failed (exit $LASTEXITCODE)"
}

# Whatever still differs is what failed to be replaced. Retry while the mapping
# is released.
for ($attempt = 1; $attempt -le $RestoreRetries; $attempt++) {
    $remaining = Get-DirtyTrackedPaths
    if ($remaining.Count -eq 0) {
        break
    }

    Write-Host "Restore attempt $attempt/$RestoreRetries - $($remaining.Count) file(s) left"
    & git restore --worktree -- .

    if ($attempt -lt $RestoreRetries) {
        Start-Sleep -Seconds 2
    }
}

# --- Verify -------------------------------------------------------------------
Write-Section 'Verify'

$finalDirty = Get-DirtyTrackedPaths
$finalHead = Get-GitValue @('rev-parse', 'HEAD')

Write-Host "HEAD        : $finalHead"
Write-Host "Target      : $target"
Write-Host "Dirty left  : $($finalDirty.Count)"

if ($finalDirty.Count -eq 0 -and $finalHead -eq $target) {
    Write-Host ''
    Write-Host 'Repaired. Repository is consistent.' -ForegroundColor Green
    exit 0
}

Write-Host ''
Write-Host 'These files are still not repaired:' -ForegroundColor Yellow
$finalDirty | ForEach-Object { Write-Host "  $_" }
Write-Host ''
Write-Host 'Close whatever holds them (Unreal Editor, the diff preview pane in your' -ForegroundColor Yellow
Write-Host 'Git GUI, Explorer preview pane) and then run:' -ForegroundColor Yellow
Write-Host '    git restore --worktree -- .' -ForegroundColor Yellow
exit 1
