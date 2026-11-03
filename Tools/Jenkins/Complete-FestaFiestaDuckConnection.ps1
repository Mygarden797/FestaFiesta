[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string] $Username,
    [string] $RootUri = 'googledrive:/My%20Drive/FestaFiesta%21',
    [string] $InstallRoot = 'C:\FestaFiestaCI'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$encoding = New-Object Text.UTF8Encoding($false)
$settingsPath = Join-Path $InstallRoot 'settings.json'
$settings = Get-Content -LiteralPath $settingsPath -Raw | ConvertFrom-Json
$duckExe = Join-Path $InstallRoot 'tools\duck\duck.exe'
if (-not (Test-Path -LiteralPath $duckExe)) { throw 'Install and prepare the verified private Cyberduck CLI first.' }
if (-not (Test-Path -LiteralPath $settings.RcloneConfig)) { throw 'One-time exact-folder bootstrap requires the existing authorized rclone configuration.' }
$common = @('--config', $settings.RcloneConfig, '--drive-root-folder-id', $settings.DriveFolderId, '--retries', '3')
function Invoke-BootstrapDrive([string[]]$Arguments) {
    $prior = $ErrorActionPreference
    try { $ErrorActionPreference='Continue'; $output=@(& $settings.RcloneExe @Arguments @common 2>&1); $code=$LASTEXITCODE }
    finally { $ErrorActionPreference=$prior }
    if ($code -ne 0) { throw ('The exact-folder bootstrap request failed (exit ' + $code + '). Authentication output was suppressed.') }
    return ($output | Out-String).Trim()
}
$markerFile = '.festafiesta-ci-target.json'
$markerPath = Join-Path $InstallRoot 'secrets\drive-target-marker.json'
if (-not (Test-Path -LiteralPath $markerPath)) {
    $marker = [ordered]@{schemaVersion=1;purpose='FestaFiesta Jenkins build upload target';folderId=$settings.DriveFolderId;nonce=[Guid]::NewGuid().ToString('N')}
    [IO.File]::WriteAllText($markerPath, ($marker|ConvertTo-Json), $encoding)
}
$marker = Get-Content -LiteralPath $markerPath -Raw | ConvertFrom-Json
if ($marker.folderId -cne $settings.DriveFolderId) { throw 'The saved target marker belongs to another folder.' }
$markerRemote = 'festafiesta-drive:/' + $markerFile
$null = Invoke-BootstrapDrive @('copyto', $markerPath, $markerRemote, '--immutable', '--checksum')
$remoteMarkerMd5 = Invoke-BootstrapDrive @('md5sum', $markerRemote)
$localMarkerMd5 = (Get-FileHash -LiteralPath $markerPath -Algorithm MD5).Hash.ToLowerInvariant()
if ($remoteMarkerMd5 -notmatch ('^' + $localMarkerMd5 + '\s+')) { throw 'The exact target-folder marker upload did not verify.' }
$duckConfigPath = Join-Path $InstallRoot 'secrets\duck.json'
$configuration = [ordered]@{username=$Username;rootUri=$RootUri;folderId=$settings.DriveFolderId;markerFile=$markerFile;markerSha256=(Get-FileHash -LiteralPath $markerPath -Algorithm SHA256).Hash.ToLowerInvariant()}
[IO.File]::WriteAllText($duckConfigPath, ($configuration | ConvertTo-Json), $encoding)
$options = [pscustomobject]@{DuckExe=$duckExe;DuckConfig=$duckConfigPath;StateRoot=$settings.StateRoot;DriveFolderId=$settings.DriveFolderId}
Import-Module (Join-Path $PSScriptRoot 'FestaFiesta.Duck.psm1') -Force
$probeLabel = 'Jenkins-DuckVerification-' + [DateTimeOffset]::UtcNow.ToOffset([TimeSpan]::FromHours(9)).ToString('yyyyMMdd') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8)
$probeRoot = Join-Path $settings.StateRoot $probeLabel
$null = New-Item -ItemType Directory -Path $probeRoot
$probePath = Join-Path $probeRoot 'connection-test.txt'
[IO.File]::WriteAllText($probePath, 'FestaFiesta Jenkins verified Cyberduck connection. This is not a game build.', $encoding)
$probe = [pscustomobject]@{path=$probePath;remoteName='connection-test.txt';bytes=(Get-Item -LiteralPath $probePath).Length;sha256=(Get-FileHash -LiteralPath $probePath -Algorithm SHA256).Hash.ToLowerInvariant()}
Invoke-FFDuckUploadFile -Options $options -BuildLabel $probeLabel -File $probe
# Independently verify the same bytes through the originally requested folder ID.
$probeRemote = 'festafiesta-drive:/' + $probeLabel + '/' + $probe.remoteName
$probeMd5 = Invoke-BootstrapDrive @('md5sum', $probeRemote)
if ($probeMd5 -notmatch ('^' + (Get-FileHash -LiteralPath $probePath -Algorithm MD5).Hash.ToLowerInvariant() + '\s+')) { throw 'The Cyberduck named path did not resolve to the requested original folder ID.' }
$null = Invoke-BootstrapDrive @('deletefile', $probeRemote, '--drive-use-trash=true')
$null = Invoke-BootstrapDrive @('rmdir', ('festafiesta-drive:/' + $probeLabel), '--drive-use-trash=true')
$settings | Add-Member -MemberType NoteProperty -Name UploadProvider -Value 'Duck' -Force
$settings | Add-Member -MemberType NoteProperty -Name DuckExe -Value $duckExe -Force
$settings | Add-Member -MemberType NoteProperty -Name DuckConfig -Value $duckConfigPath -Force
$temporary = $settingsPath + '.' + [Guid]::NewGuid().ToString('N') + '.tmp'
try {
    [IO.File]::WriteAllText($temporary, ($settings|ConvertTo-Json -Depth 6), $encoding)
    [IO.File]::Replace($temporary, $settingsPath, [NullString]::Value)
} finally { if ([IO.File]::Exists($temporary)) { [IO.File]::Delete($temporary) } }
$receipt = [ordered]@{verifiedAtUtc=[DateTimeOffset]::UtcNow.ToString('o');provider='Cyberduck CLI';folderId=$settings.DriveFolderId;markerVerified=$true;uploadReadbackSha256Verified=$true;originalFolderIdCrossChecked=$true;probeMovedToTrash=$true;credentialSource='Windows Credential Manager'}
[IO.File]::WriteAllText((Join-Path $settings.StateRoot 'duck-connection-verification.json'), ($receipt|ConvertTo-Json), $encoding)
Write-Output 'Cyberduck folder identity, upload, and full SHA-256 readback verified. Jenkins now uses the Windows credential store.'
