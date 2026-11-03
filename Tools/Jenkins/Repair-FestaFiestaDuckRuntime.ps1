# Keep the vendor installation unchanged. The CLI's net10 bundle can carry net8
# ConfigurationManager dependencies; use matching Microsoft-signed net10 packages
# in the private Jenkins copy instead.
[CmdletBinding()]
param([string] $InstallRoot = 'C:\FestaFiestaCI', [string] $VendorRoot = 'C:\Program Files\Cyberduck CLI')
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
if (-not (Test-Path -LiteralPath (Join-Path $InstallRoot 'installation.json'))) { throw 'The protected Jenkins runtime must be installed first.' }
$runtime = Get-Content -LiteralPath (Join-Path $VendorRoot 'duck.runtimeconfig.json') -Raw | ConvertFrom-Json
if ($runtime.runtimeOptions.tfm -cne 'net10.0') { throw 'This dependency repair is limited to the verified net10 CLI bundle.' }
$framework = @($runtime.runtimeOptions.includedFrameworks | Where-Object name -eq 'Microsoft.NETCore.App')
if ($framework.Count -ne 1 -or $framework[0].version -notmatch '^10\.0\.\d+$') { throw 'Could not identify the bundled .NET 10 servicing version.' }
$version = [string]$framework[0].version
$privateRoot = Join-Path $InstallRoot 'tools\duck'
if ((Test-Path -LiteralPath $privateRoot) -and -not (Test-Path -LiteralPath (Join-Path $privateRoot 'festafiesta-runtime.json'))) {
    throw 'Refusing to overwrite an unrecognized private CLI directory.'
}
$null = New-Item -ItemType Directory -Path $privateRoot -Force
$receiptPath = Join-Path $privateRoot 'festafiesta-runtime.json'
[IO.File]::WriteAllText($receiptPath, '{"status":"preparing"}')
Copy-Item -Path (Join-Path $VendorRoot '*') -Destination $privateRoot -Recurse -Force
Add-Type -AssemblyName System.IO.Compression.FileSystem
$packages = @(
    @{Id='system.configuration.configurationmanager';Dll='System.Configuration.ConfigurationManager.dll'},
    @{Id='system.diagnostics.eventlog';Dll='System.Diagnostics.EventLog.dll'},
    @{Id='system.security.cryptography.protecteddata';Dll='System.Security.Cryptography.ProtectedData.dll'}
)
$receipts = @()
foreach ($package in $packages) {
    $name = $package.Id + '.' + $version + '.nupkg'
    $archivePath = Join-Path $InstallRoot ('downloads\' + $name)
    $url = 'https://api.nuget.org/v3-flatcontainer/' + $package.Id + '/' + $version + '/' + $name
    if (-not (Test-Path -LiteralPath $archivePath)) { Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $archivePath }
    $archive = [IO.Compression.ZipFile]::OpenRead($archivePath)
    try {
        $entryName = 'runtimes/win/lib/net10.0/' + $package.Dll
        $entry = $archive.GetEntry($entryName)
        if ($null -eq $entry) {
            $entryName = 'lib/net10.0/' + $package.Dll
            $entry = $archive.GetEntry($entryName)
        }
        if ($null -eq $entry) { throw ('The Microsoft package does not contain ' + $entryName) }
        $targetPath = Join-Path $privateRoot $package.Dll
        [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $targetPath, $true)
    } finally { $archive.Dispose() }
    $signature = Get-AuthenticodeSignature -LiteralPath $targetPath
    if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'O=Microsoft Corporation') {
        throw ('A trusted Microsoft signature is required for ' + $package.Dll)
    }
    if ([Reflection.AssemblyName]::GetAssemblyName($targetPath).Version.Major -ne 10) { throw 'The replacement dependency must be version 10.' }
    $receipts += @{package=$package.Id;version=$version;sha256=(Get-FileHash -LiteralPath $targetPath -Algorithm SHA256).Hash.ToLowerInvariant()}
}
$receipt = @{status='ready';vendorVersion=[Diagnostics.FileVersionInfo]::GetVersionInfo((Join-Path $VendorRoot 'duck.exe')).FileVersion;runtime=$version;dependencies=$receipts;preparedAtUtc=[DateTimeOffset]::UtcNow.ToString('o')}
[IO.File]::WriteAllText($receiptPath, ($receipt | ConvertTo-Json -Depth 5), (New-Object Text.UTF8Encoding($false)))
Write-Output ('The private CLI runtime is ready at ' + (Join-Path $privateRoot 'duck.exe'))
