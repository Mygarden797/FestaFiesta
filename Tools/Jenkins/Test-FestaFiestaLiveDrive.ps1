# Manual integration check, executed on the same Windows agent as daily builds.
[CmdletBinding()]
param([string] $SettingsPath = 'C:\FestaFiestaCI\settings.json')
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
function Remove-FFDuckListingTerminalControls {
    param([AllowEmptyString()][string] $Text)
    # Even --quiet emits DEC save/restore cursor (ESC 7/8). Strip only
    # complete, recognized terminal sequences; unknown output stays nonempty.
    return [regex]::Replace($Text, '\x1B(?:\[[0-?]*[ -/]*[@-~]|[78])', '')
}
$settings = Get-Content -LiteralPath $SettingsPath -Raw | ConvertFrom-Json
if ($settings.UploadProvider -cne 'Duck') { throw 'This check requires the configured Cyberduck transport.' }
Import-Module (Join-Path $PSScriptRoot 'FestaFiesta.Duck.psm1') -Force
$options = [pscustomobject]@{DuckExe=$settings.DuckExe;DuckConfig=$settings.DuckConfig;StateRoot=$settings.StateRoot;DriveFolderId=$settings.DriveFolderId}
$configuration = Get-Content -LiteralPath $settings.DuckConfig -Raw | ConvertFrom-Json
$label = 'Jenkins-AgentCheck-' + [Guid]::NewGuid().ToString('N')
$localDirectory = Join-Path $settings.StateRoot $label
$null = New-Item -ItemType Directory -Path $localDirectory
$path = Join-Path $localDirectory 'connection-check.txt'
[IO.File]::WriteAllText($path, 'FestaFiesta Jenkins Windows-agent Drive connection verification.', (New-Object Text.UTF8Encoding($false)))
$file = [pscustomobject]@{path=$path;remoteName='connection-check.txt';bytes=(Get-Item -LiteralPath $path).Length;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
Invoke-FFDuckUploadFile -Options $options -BuildLabel $label -File $file
$rootUri = $configuration.rootUri.TrimEnd('/') + '/' + $label
function Invoke-CheckedDuck([string[]]$Arguments) {
    $previous=$ErrorActionPreference
    try { $ErrorActionPreference='Continue'; $output=@(& $settings.DuckExe --username $configuration.username --assumeyes --quiet --retry 3 @Arguments 2>&1); $code=$LASTEXITCODE }
    finally { $ErrorActionPreference=$previous }
    if ($code -ne 0) { throw ('Cyberduck verification cleanup failed (exit '+$code+'). Diagnostics were suppressed.') }
    return ($output|Out-String).Trim()
}
# Google Drive's regular delete moves this exact probe file to Trash.
$null = Invoke-CheckedDuck @('--delete', ($rootUri + '/' + $file.remoteName))
$remaining = Remove-FFDuckListingTerminalControls (Invoke-CheckedDuck @('--list', ($rootUri + '/')))
$probeFolderMovedToTrash = $false
if ([string]::IsNullOrWhiteSpace($remaining)) {
    # This is the fresh UUID probe directory, and listing confirmed it empty.
    # Never recursively remove a directory or delete a parent build folder.
    $null = Invoke-CheckedDuck @('--delete', ($rootUri + '/'))
    $probeFolderMovedToTrash = $true
}
$receipt = @{verifiedAtUtc=[DateTimeOffset]::UtcNow.ToString('o');provider='Cyberduck CLI';folderId=$settings.DriveFolderId;agentUser=[Security.Principal.WindowsIdentity]::GetCurrent().User.Value;markerVerified=$true;uploadAndSha256ReadbackVerified=$true;probeMovedToTrash=$true;probeFileMovedToTrash=$true;probeFolderMovedToTrash=$probeFolderMovedToTrash;probeFolderLabel=$label}
[IO.File]::WriteAllText((Join-Path $settings.StateRoot 'jenkins-agent-drive-verification.json'), ($receipt|ConvertTo-Json), (New-Object Text.UTF8Encoding($false)))
Write-Output 'PASS: Windows-agent credentials, target marker, upload, SHA-256 readback, and probe file moved to Trash.'
if ($probeFolderMovedToTrash) { Write-Output 'The empty probe directory was also moved to Trash.' }
else { Write-Output 'The probe directory was retained because its listing was not empty.' }
