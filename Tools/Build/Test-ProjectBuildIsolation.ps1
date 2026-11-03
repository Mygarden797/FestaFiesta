# Offline process/path fixtures. No editor, game, build, or CIM query is run.
[CmdletBinding()]
param([string] $FixtureParent = [IO.Path]::GetTempPath())
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$module = Import-Module (Join-Path $PSScriptRoot 'FestaFiesta.Build.psm1') -Force -PassThru
$script:IsolationAssertions = 0
$fixtureParentFull = [IO.Path]::GetFullPath($FixtureParent).TrimEnd('\')
$fixtureRoot = Join-Path $fixtureParentFull ('ff-project-isolation-' + [Guid]::NewGuid().ToString('N'))
$fixtureRoot = [IO.Path]::GetFullPath($fixtureRoot)
if (-not $fixtureRoot.StartsWith($fixtureParentFull + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'The unique fixture directory must remain inside its requested parent.'
}
$directories = @($fixtureRoot, (Join-Path $fixtureRoot 'CI checkout'), (Join-Path $fixtureRoot 'Other checkout'),
    (Join-Path $fixtureRoot 'CI checkout-other'))
$ciRoot = $directories[1]
$otherRoot = $directories[2]
$siblingRoot = $directories[3]
$directories += @((Join-Path $ciRoot 'Binaries'), (Join-Path $ciRoot 'Binaries\Win64'),
    (Join-Path $siblingRoot 'Binaries'), (Join-Path $siblingRoot 'Binaries\Win64'))
$ciProject = Join-Path $ciRoot 'FestaFiesta.uproject'
$otherProject = Join-Path $otherRoot 'FestaFiesta.uproject'
$ciGame = Join-Path $ciRoot 'Binaries\Win64\FestaFiesta.exe'
$siblingGame = Join-Path $siblingRoot 'Binaries\Win64\FestaFiesta.exe'
$files = @($ciProject, $otherProject, $ciGame, $siblingGame)
$ciAlias = Join-Path $fixtureRoot 'CI junction'
$otherAlias = Join-Path $fixtureRoot 'Other junction'
$aliases = @($ciAlias, $otherAlias)

function New-IsolationProcess([string]$CommandLine, [string]$ExecutablePath = '', [string]$Name = 'UnrealEditor.exe') {
    [pscustomobject]@{ Name=$Name; ProcessId=12345; CommandLine=$CommandLine; ExecutablePath=$ExecutablePath }
}
function Assert-Isolation {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw ('Assertion failed: ' + $Message) }
    $script:IsolationAssertions++
}
function Test-IsolationProcess {
    param($Process, [bool]$Blocked, [string]$Message, [string]$Reason = '', [string]$SelectedProject = $ciProject)
    $blockers = @(& $module {
        param($Project, $InputProcesses)
        Get-FFProjectBuildBlockers -ProjectFile $Project -Processes $InputProcesses
    } $SelectedProject @($Process))
    Assert-Isolation (($blockers.Count -gt 0) -eq $Blocked) $Message
    if ($Reason) { Assert-Isolation ($blockers.Count -eq 1 -and $blockers[0].Reason -eq $Reason) ($Message + ' (reason)') }
}
try {
    foreach ($directory in $directories) { [IO.Directory]::CreateDirectory($directory) | Out-Null }
    foreach ($file in $files) { [IO.File]::WriteAllText($file, '{}') }
    $null = New-Item -ItemType Junction -Path $ciAlias -Target $ciRoot
    $null = New-Item -ItemType Junction -Path $otherAlias -Target $otherRoot
    Assert-Isolation ($null -ne (Get-Command Assert-FFProjectBuildAvailable -ErrorAction Stop)) 'The production assertion is exported.'
    Test-IsolationProcess (New-IsolationProcess ('"C:\Engine\UnrealEditor.exe" "' + $ciProject + '"')) $true 'A quoted positional project identifies this checkout.' 'EditorUsingCheckout'
    Test-IsolationProcess (New-IsolationProcess ('UnrealEditor.exe -Project="' + $ciProject.Replace('\','/') + '"')) $true 'Forward slashes and a quoted -Project value identify this checkout.' 'EditorUsingCheckout'
    Test-IsolationProcess (New-IsolationProcess ('UnrealEditor.exe -project "' + $ciProject + '"')) $true 'A separate -project argument identifies this checkout.' 'EditorUsingCheckout'
    Test-IsolationProcess (New-IsolationProcess ('UnrealEditor.exe "' + $ciProject.ToUpperInvariant() + '"')) $true 'Windows path comparison is case insensitive.' 'EditorUsingCheckout'
    Test-IsolationProcess (New-IsolationProcess ('UnrealEditor.exe "' + $otherProject + '"')) $false 'A developer checkout with the same project filename remains available.'
    Test-IsolationProcess (New-IsolationProcess ('UnrealEditor.exe -SomeOtherArgument="' + $ciProject + '"')) $false 'An unrelated argument is not a project selector.'
    Test-IsolationProcess (New-IsolationProcess '' $ciGame 'FestaFiesta.exe') $true 'A game executable from the CI checkout blocks output replacement.' 'ExecutableInCheckout'
    Test-IsolationProcess (New-IsolationProcess '' $siblingGame 'FestaFiesta.exe') $false 'A sibling path sharing the CI root prefix is a separate checkout.'
    $aliasProject = Join-Path $ciAlias 'FestaFiesta.uproject'
    Test-IsolationProcess (New-IsolationProcess ('UnrealEditor.exe "' + $aliasProject + '"')) $true 'A junction alias identifies the same physical project directory.' 'EditorUsingCheckout'
    Test-IsolationProcess (New-IsolationProcess ('UnrealEditor.exe "' + $ciProject + '"')) $true 'Selecting the junction still blocks the physical project path.' 'EditorUsingCheckout' $aliasProject
    Test-IsolationProcess (New-IsolationProcess '' (Join-Path $ciAlias 'Binaries\Win64\FestaFiesta.exe') 'FestaFiesta.exe') $true 'A game launched through a junction still blocks CI output replacement.' 'ExecutableInCheckout'
    Test-IsolationProcess (New-IsolationProcess ('UnrealEditor.exe "' + (Join-Path $otherAlias 'FestaFiesta.uproject') + '"')) $false 'A junction to a different physical checkout remains available.'
    Test-IsolationProcess (New-IsolationProcess 'UnrealEditor.exe FestaFiesta.uproject') $true 'A same-name relative project is unresolved instead of assumed separate.' 'UnresolvedProjectPath'
    Test-IsolationProcess (New-IsolationProcess 'UnrealEditor.exe C:FestaFiesta.uproject') $true 'A drive-relative project cannot be resolved from the caller working directory.' 'UnresolvedProjectPath'
    Test-IsolationProcess (New-IsolationProcess 'UnrealEditor.exe \Unknown\FestaFiesta.uproject') $true 'A current-drive-relative project remains unresolved.' 'UnresolvedProjectPath'
    Test-IsolationProcess (New-IsolationProcess 'UnrealEditor.exe FestaFiesta') $true 'An Unreal named-project launch remains unresolved.' 'UnresolvedProjectPath'
    Test-IsolationProcess (New-IsolationProcess ('UnrealEditor.exe "' + (Join-Path $fixtureRoot 'Unavailable\FestaFiesta.uproject') + '"')) $true 'An unavailable same-name absolute directory cannot prove isolation.' 'UnresolvedProjectPath'
    Test-IsolationProcess (New-IsolationProcess 'UnrealEditor.exe OtherProject.uproject') $false 'A different relative project name does not block this project.'
    Test-IsolationProcess (New-IsolationProcess '') $true 'An unreadable editor command line cannot prove an isolated checkout.' 'UnresolvedEditorProject'
    Test-IsolationProcess ([pscustomobject]@{Name='UnrealEditor-Cmd.exe';ProcessId=12346;ExecutablePath=''}) $true 'A missing editor command line fails closed.' 'UnresolvedEditorProject'
    Test-IsolationProcess (New-IsolationProcess ('Rider64.exe "' + $ciProject + '"') '' 'Rider64.exe') $false 'An IDE opening the descriptor is not treated as the editor or game.'
    $unc = '\\server\share name\FestaFiesta.uproject'
    $uncTokens = & $module { param($Path) [FestaFiestaBuildIsolation.NativeMethods]::SplitCommandLine('UnrealEditor.exe -Project="' + $Path + '"') } $unc
    Assert-Isolation ($uncTokens.Count -eq 2 -and $uncTokens[1] -ceq ('-Project=' + $unc)) 'Official command-line parsing preserves a quoted UNC project path without contacting the server.'
    $empty = @(& $module { param($Project) Get-FFProjectBuildBlockers -ProjectFile $Project -Processes @() } $ciProject)
    Assert-Isolation ($empty.Count -eq 0) 'An empty process inventory does not block a build.'
    Write-Output ('PASS: {0} project isolation assertions; no real process or build was started.' -f $script:IsolationAssertions)
}
finally {
    # Remove the two junction entries first, never recurse through their targets.
    foreach ($alias in $aliases) {
        if (Test-Path -LiteralPath $alias) {
            $entry = Get-Item -LiteralPath $alias -Force
            if (-not ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'A fixture junction was unexpectedly replaced; cleanup stopped.' }
            [IO.Directory]::Delete($alias)
        }
    }
    foreach ($file in $files) { if ([IO.File]::Exists($file)) { [IO.File]::Delete($file) } }
    foreach ($directory in ($directories | Sort-Object Length -Descending)) {
        if ([IO.Directory]::Exists($directory)) { [IO.Directory]::Delete($directory) }
    }
}
