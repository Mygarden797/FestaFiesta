$ErrorActionPreference = 'Stop'

# The repository tracks Monolith's source, but ignores generated binaries.
# Restore the matching official UE 5.7 release binaries after a fresh checkout.
$projectRoot = Split-Path -Parent $PSScriptRoot
$pluginRoot = Join-Path $projectRoot 'Plugins/Monolith'
$archive = Join-Path ([System.IO.Path]::GetTempPath()) ('monolith-' + [guid]::NewGuid() + '.zip')
$expectedHash = '303F3BC7B9ABA389258BBA4E9755560B03F1CC50616C68453B6CE4814F29AE64'
$url = 'https://github.com/tumourlove/monolith/releases/download/v0.24.0/Monolith-v0.24.0-UE5.7.zip'

if (-not (Test-Path -LiteralPath (Join-Path $pluginRoot 'Monolith.uplugin'))) {
    throw 'Monolith source is missing. Restore Plugins/Monolith from the project repository first.'
}
if ((Get-Content -LiteralPath (Join-Path $pluginRoot 'Monolith.uplugin') -Raw | ConvertFrom-Json).VersionName -ne '0.24.0') {
    throw 'Monolith source version differs from the binaries this script installs.'
}

try {
    Invoke-WebRequest -Uri $url -OutFile $archive
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expectedHash) {
        throw 'Monolith archive SHA-256 does not match the official v0.24.0 UE 5.7 release.'
    }

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [System.IO.Compression.ZipFile]::OpenRead($archive)
    try {
        $count = 0
        foreach ($entry in $zip.Entries) {
            $name = $entry.FullName.Replace('\', '/')
            if ($name -notmatch '^Binaries/(monolith_proxy\.exe|monolith_query\.exe|Win64/[^/]+)$') { continue }
            $destination = Join-Path $pluginRoot $name
            [System.IO.Directory]::CreateDirectory((Split-Path -Parent $destination)) | Out-Null
            [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $destination, $true)
            $count++
        }
        if ($count -ne 23) { throw "Unexpected number of binary files in Monolith archive: $count" }
    }
    finally { $zip.Dispose() }
    Write-Host 'Monolith v0.24.0 UE 5.7 binaries installed. Rebuild the project if required, then restart Unreal Editor and the MCP client.'
}
finally {
    if (Test-Path -LiteralPath $archive) { Remove-Item -LiteralPath $archive -Force }
}
