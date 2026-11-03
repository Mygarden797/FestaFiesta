[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string] $ClientJsonPath,
    [string] $InstallRoot = 'C:\FestaFiestaCI',
    [switch] $Reconnect
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$settings = Get-Content -LiteralPath (Join-Path $InstallRoot 'settings.json') -Raw | ConvertFrom-Json
$expectedConfig = Join-Path $InstallRoot 'secrets\rclone.conf'
if ([IO.Path]::GetFullPath($settings.RcloneConfig) -ine [IO.Path]::GetFullPath($expectedConfig)) { throw 'The rclone configuration must remain in the protected Jenkins secrets directory.' }
$client = Get-Content -LiteralPath $ClientJsonPath -Raw | ConvertFrom-Json
if (-not ($client.PSObject.Properties.Name -contains 'installed')) { throw 'Download the JSON for a Desktop OAuth client created specifically for this Jenkins integration.' }
$clientId = [string]$client.installed.client_id
$clientSecret = [string]$client.installed.client_secret
if ($clientId -notmatch '^[A-Za-z0-9._-]+\.apps\.googleusercontent\.com$' -or $clientSecret -notmatch '^[A-Za-z0-9_-]+$') { throw 'Unexpected Desktop OAuth credential format.' }
$configuration = @(
    '[festafiesta-drive]',
    'type = drive',
    ('client_id = ' + $clientId),
    ('client_secret = ' + $clientSecret),
    'scope = drive',
    ('root_folder_id = ' + $settings.DriveFolderId),
    ''
) -join [Environment]::NewLine
if (Test-Path -LiteralPath $expectedConfig) {
    $existing = [IO.File]::ReadAllText($expectedConfig)
    if ($existing -notmatch ('(?m)^client_id\s*=\s*' + [regex]::Escape($clientId) + '\s*$')) { throw 'The existing Drive configuration belongs to a different OAuth client. Preserve it before changing clients.' }
    if (-not $Reconnect) { throw 'A Drive configuration already exists. Use -Reconnect to explicitly resume authorization with this same client.' }
} else {
    [IO.File]::WriteAllText($expectedConfig, $configuration, (New-Object Text.UTF8Encoding($false)))
}
Write-Output 'A Google sign-in/consent page will open. Complete the account authorization yourself. This OAuth scope permits Drive file access; the Jenkins upload path is fixed to the requested folder.'
$previousPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = 'Continue'
    # Do not echo authentication URLs, secrets, or token responses into a build log.
    $null = & $settings.RcloneExe config reconnect 'festafiesta-drive:' --config $expectedConfig --auto-confirm 2>&1
    $authExit = $LASTEXITCODE
} finally { $ErrorActionPreference = $previousPreference }
if ($authExit -ne 0) { throw 'Drive authorization did not finish. The partial configuration was preserved for diagnosis; no build was uploaded.' }
try {
    $ErrorActionPreference = 'Continue'
    $null = & $settings.RcloneExe lsjson 'festafiesta-drive:' --stat --config $expectedConfig --drive-root-folder-id $settings.DriveFolderId 2>&1
    $folderExit = $LASTEXITCODE
} finally { $ErrorActionPreference = $previousPreference }
if ($folderExit -ne 0) { throw 'Authorization finished but the requested Drive folder could not be accessed. Check Drive API enablement and account permissions.' }
Write-Output 'Drive authorization and target-folder access verified. Run the normal Jenkins job to upload a pending verified build.'
