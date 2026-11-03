[CmdletBinding()]
param([string] $InstallRoot = 'C:\FestaFiestaCI')
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$downloads = Join-Path $InstallRoot 'downloads'
if (-not (Test-Path -LiteralPath (Join-Path $InstallRoot 'installation.json'))) { throw 'Install the private FestaFiesta Jenkins runtime first.' }
$packages = @(
    @{Name='duck-9.5.4.45528.exe';Url='https://dist.duck.sh//duck-9.5.4.45528.exe';Algorithm='SHA256';Checksum='a85ccfb221f0a15706b55f951f7bb3d97fda9b67733d896d64ecc7a595b41843'},
    @{Name='Cyberduck-Installer-9.5.4.45528.exe';Url='https://update.cyberduck.io/windows/Cyberduck-Installer-9.5.4.45528.exe';Algorithm='MD5';Checksum='8ccb59877992ef76b3374b08924c4c4f'}
)
foreach ($package in $packages) {
    $path = Join-Path $downloads $package.Name
    if (-not (Test-Path -LiteralPath $path)) { Invoke-WebRequest -UseBasicParsing -Uri $package.Url -OutFile $path }
    if ((Get-FileHash -LiteralPath $path -Algorithm $package.Algorithm).Hash -ine $package.Checksum) { throw 'The official package checksum did not match.' }
    $signature = Get-AuthenticodeSignature -LiteralPath $path
    if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'CN=iterate GmbH,') { throw 'The package must have a trusted iterate GmbH Authenticode signature.' }
    $process = Start-Process -FilePath $path -ArgumentList '/quiet','/norestart' -WindowStyle Hidden -Wait -PassThru
    if ($process.ExitCode -notin 0,3010) { throw ('Installation failed for ' + $package.Name + ' with exit ' + $process.ExitCode) }
    Write-Output ('Installed the signed package: ' + $package.Name)
}
& (Join-Path $PSScriptRoot 'Repair-FestaFiestaDuckRuntime.ps1') -InstallRoot $InstallRoot
