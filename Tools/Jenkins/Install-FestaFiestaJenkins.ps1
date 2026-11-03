[CmdletBinding()]
param(
    [string] $InstallRoot = 'C:\FestaFiestaCI',
    [string] $CheckoutRoot = 'C:\FFCI\Dev',
    [string] $OutputRoot = 'C:\FestaFiestaBuild',
    [string] $EngineRoot = 'C:\Unreal\UE_5.7',
    [ValidateRange(1024,65535)][int] $Port = 8090,
    [switch] $Start
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
Set-StrictMode -Version Latest
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$InstallRoot = [IO.Path]::GetFullPath($InstallRoot)
if ($InstallRoot -notmatch '^[A-Za-z]:\\[A-Za-z0-9_-]+$') { throw 'Use a dedicated, short installation root such as C:\FestaFiestaCI.' }
$marker = Join-Path $InstallRoot 'installation.json'
$previousInstallation = $null
if (Test-Path -LiteralPath $marker -PathType Leaf) {
    try { $previousInstallation = Get-Content -LiteralPath $marker -Raw | ConvertFrom-Json }
    catch { $previousInstallation = $null }
}
if ((Test-Path -LiteralPath $InstallRoot) -and -not (Test-Path -LiteralPath $marker)) {
    if (@(Get-ChildItem -LiteralPath $InstallRoot -Force).Count -gt 0) { throw 'Refusing to use an unrecognized nonempty installation directory.' }
}
foreach ($relative in @('', 'tools', 'downloads', 'scripts', 'home', 'home\plugins', 'home\init.groovy.d', 'secrets', 'state', 'logs', 'agent')) {
    $null = New-Item -ItemType Directory -Path (Join-Path $InstallRoot $relative) -Force
}
# This runtime contains Git/Drive access and Jenkins secrets. Only the current user,
# SYSTEM, and local Administrators can access it. Never copy it into the repository.
$sid = [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
& icacls.exe $InstallRoot '/inheritance:r' '/grant:r' ('*' + $sid + ':(OI)(CI)F') '*S-1-5-18:(OI)(CI)F' '*S-1-5-32-544:(OI)(CI)F' | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Could not protect the Jenkins runtime directory.' }
if (-not (Test-Path -LiteralPath $marker)) {
    [IO.File]::WriteAllText($marker, '{"SchemaVersion":1,"Status":"installing"}', (New-Object Text.UTF8Encoding($false)))
}

function Get-VerifiedDownload([string]$Url, [string]$Path, [string]$Sha256) {
    if ($Sha256 -notmatch '^[a-fA-F0-9]{64}$') { throw 'A valid SHA256 checksum is required.' }
    if ((Test-Path -LiteralPath $Path) -and (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ieq $Sha256) { return }
    Invoke-WebRequest -UseBasicParsing -Uri $Url -OutFile $Path
    if ((Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ine $Sha256) { throw ('Download checksum mismatch: ' + [IO.Path]::GetFileName($Path)) }
}
function Get-RemoteText([string]$Url) {
    $response = Invoke-WebRequest -UseBasicParsing -Uri $Url
    if ($response.Content -is [byte[]]) { return [Text.Encoding]::UTF8.GetString($response.Content) }
    return [string]$response.Content
}
$javaMeta = @(Invoke-RestMethod -Uri 'https://api.adoptium.net/v3/assets/latest/21/hotspot?architecture=x64&image_type=jre&os=windows&vendor=eclipse')[0]
$javaZip = Join-Path $InstallRoot 'downloads\java.zip'
Get-VerifiedDownload $javaMeta.binary.package.link $javaZip $javaMeta.binary.package.checksum
$javaDestination = Join-Path $InstallRoot 'tools\java'
$installedJavaVersion = $null
$installedJavaSha256 = $null
if (-not (Test-Path -LiteralPath (Join-Path $javaDestination 'bin\java.exe'))) {
    $javaExtract = Join-Path $InstallRoot 'downloads\java-expanded'
    Expand-Archive -LiteralPath $javaZip -DestinationPath $javaExtract -Force
    $javaSource = @(Get-ChildItem -LiteralPath $javaExtract -Directory | Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'bin\java.exe') })
    if ($javaSource.Count -ne 1) { throw 'Unexpected Java archive layout.' }
    $null = New-Item -ItemType Directory -Path $javaDestination -Force
    Copy-Item -Path (Join-Path $javaSource[0].FullName '*') -Destination $javaDestination -Recurse -Force
    $installedJavaVersion = [string]$javaMeta.version.semver
    $installedJavaSha256 = [string]$javaMeta.binary.package.checksum
} elseif ($null -ne $previousInstallation -and $previousInstallation.PSObject.Properties['Java'] -and
    -not [string]::IsNullOrWhiteSpace([string]$previousInstallation.Java)) {
    # Downloading a newer archive does not change the runtime already in use.
    $installedJavaVersion = [string]$previousInstallation.Java
    if ($previousInstallation.PSObject.Properties['JavaSha256'] -and
        [string]$previousInstallation.JavaSha256 -match '^[a-fA-F0-9]{64}$') {
        $installedJavaSha256 = [string]$previousInstallation.JavaSha256
    }
} else {
    # An existing runtime without a receipt has no verified archive checksum.
    $javaReleasePath = Join-Path $javaDestination 'release'
    $javaVersionLine = Select-String -LiteralPath $javaReleasePath -Pattern '^JAVA_RUNTIME_VERSION="([^"]+)"$' | Select-Object -First 1
    if ($null -eq $javaVersionLine) {
        $javaVersionLine = Select-String -LiteralPath $javaReleasePath -Pattern '^JAVA_VERSION="([^"]+)"$' | Select-Object -First 1
    }
    if ($null -eq $javaVersionLine) { throw 'The existing Java runtime has no readable release version.' }
    $installedJavaVersion = $javaVersionLine.Matches[0].Groups[1].Value
}
$java = Join-Path $javaDestination 'bin\java.exe'
$jenkinsVersion = (Get-RemoteText 'https://updates.jenkins.io/stable/latestCore.txt').Trim()
if ($jenkinsVersion -notmatch '^\d+\.\d+\.\d+$') { throw 'Unexpected Jenkins LTS version.' }
$jenkinsUrl = 'https://get.jenkins.io/war-stable/' + $jenkinsVersion + '/jenkins.war'
$jenkinsHash = ((Get-RemoteText ($jenkinsUrl + '.sha256')) -split '\s+')[0]
Get-VerifiedDownload $jenkinsUrl (Join-Path $InstallRoot 'tools\jenkins.war') $jenkinsHash

$rcloneVersion = (Get-RemoteText 'https://downloads.rclone.org/version.txt').Trim() -replace '^rclone v',''
if ($rcloneVersion -notmatch '^\d+\.\d+\.\d+$') { throw 'Unexpected rclone version.' }
$rcloneName = 'rclone-v' + $rcloneVersion + '-windows-amd64.zip'
$rcloneBase = 'https://downloads.rclone.org/v' + $rcloneVersion + '/'
$sums = Get-RemoteText ($rcloneBase + 'SHA256SUMS')
$rcloneLine = @($sums -split "`n" | Where-Object { $_.TrimEnd() -match ([regex]::Escape($rcloneName) + '$') })
if ($rcloneLine.Count -ne 1) { throw 'rclone checksum was not uniquely identified.' }
$rcloneHash = ($rcloneLine[0].Trim() -split '\s+')[0]
$rcloneZip = Join-Path $InstallRoot ('downloads\' + $rcloneName)
Get-VerifiedDownload ($rcloneBase + $rcloneName) $rcloneZip $rcloneHash
$rcloneExtract = Join-Path $InstallRoot ('downloads\rclone-' + $rcloneVersion + '-expanded')
Expand-Archive -LiteralPath $rcloneZip -DestinationPath $rcloneExtract -Force
$rcloneSource = @(Get-ChildItem -LiteralPath $rcloneExtract -Filter rclone.exe -File -Recurse)
if ($rcloneSource.Count -ne 1) { throw 'Unexpected rclone archive layout.' }
$rcloneDir = Join-Path $InstallRoot 'tools\rclone'
$null = New-Item -ItemType Directory -Path $rcloneDir -Force
Copy-Item -LiteralPath $rcloneSource[0].FullName -Destination (Join-Path $rcloneDir 'rclone.exe') -Force

$pluginMeta = Invoke-RestMethod -Uri 'https://api.github.com/repos/jenkinsci/plugin-installation-manager-tool/releases/latest'
$pluginAsset = @($pluginMeta.assets | Where-Object { $_.name -match '^jenkins-plugin-manager-.*\.jar$' })
if ($pluginAsset.Count -ne 1) { throw 'Could not identify the official Jenkins plugin manager.' }
$pluginJar = Join-Path $InstallRoot 'tools\plugin-manager.jar'
if ($pluginAsset[0].PSObject.Properties.Name -contains 'digest' -and $pluginAsset[0].digest -match '^sha256:([a-f0-9]{64})$') {
    Get-VerifiedDownload $pluginAsset[0].browser_download_url $pluginJar $Matches[1]
} else {
    Invoke-WebRequest -UseBasicParsing -Uri $pluginAsset[0].browser_download_url -OutFile $pluginJar
}
Write-Output 'Installing the minimal Jenkins Pipeline plugins and their dependencies...'
& $java -jar $pluginJar --war (Join-Path $InstallRoot 'tools\jenkins.war') --plugin-download-directory (Join-Path $InstallRoot 'home\plugins') --plugins workflow-job workflow-cps workflow-durable-task-step workflow-basic-steps pipeline-stage-step --latest true
if ($LASTEXITCODE -ne 0) { throw 'Jenkins plugin installation failed.' }

$settings = [ordered]@{
    SourceRepository = 'https://github.com/Mygarden797/FestaFiesta.git'
    SourceBranch = 'Dev'
    CheckoutRoot = $CheckoutRoot
    OutputRoot = $OutputRoot
    StateRoot = (Join-Path $InstallRoot 'state')
    EngineRoot = $EngineRoot
    RcloneExe = (Join-Path $InstallRoot 'tools\rclone\rclone.exe')
    RcloneConfig = (Join-Path $InstallRoot 'secrets\rclone.conf')
    DriveFolderId = '13QQv-LeeTXIWG00bcQ9N9q-1I2vE_spL'
    Port = $Port
}
$encoding = New-Object Text.UTF8Encoding($false)
if (-not (Test-Path -LiteralPath (Join-Path $InstallRoot 'settings.json'))) { [IO.File]::WriteAllText((Join-Path $InstallRoot 'settings.json'), ($settings | ConvertTo-Json), $encoding) }
$passwordPath = Join-Path $InstallRoot 'secrets\admin-password.txt'
if (-not (Test-Path -LiteralPath $passwordPath)) {
    $bytes = New-Object byte[] 36
    $rng = [Security.Cryptography.RandomNumberGenerator]::Create()
    try { $rng.GetBytes($bytes) } finally { $rng.Dispose() }
    [IO.File]::WriteAllText($passwordPath, [Convert]::ToBase64String($bytes), $encoding)
}
Get-ChildItem -LiteralPath $PSScriptRoot -File | Where-Object { $_.Extension -in '.ps1','.psm1','.groovy','.md' } | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $InstallRoot 'scripts') -Force }
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'bootstrap.groovy') -Destination (Join-Path $InstallRoot 'home\init.groovy.d\10-festafiesta.groovy') -Force
$installed = [ordered]@{SchemaVersion=1;InstalledAtUtc=[DateTimeOffset]::UtcNow.ToString('o');Jenkins=$jenkinsVersion;JenkinsSha256=$jenkinsHash;Java=$installedJavaVersion;JavaSha256=$installedJavaSha256;Rclone=$rcloneVersion;RcloneSha256=$rcloneHash;PluginManager=$pluginMeta.tag_name;RuntimeRoot=$InstallRoot}
[IO.File]::WriteAllText($marker, ($installed | ConvertTo-Json), $encoding)

# Interactive user task preserves GPU access for UE's offscreen rendering tests.
$powershell = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
$taskArgs = '-NoLogo -NoProfile -NonInteractive -WindowStyle Hidden -ExecutionPolicy Bypass -File "' + (Join-Path $InstallRoot 'scripts\Start-FestaFiestaJenkins.ps1') + '" -InstallRoot "' + $InstallRoot + '"'
$action = New-ScheduledTaskAction -Execute $powershell -Argument $taskArgs
$triggers = @((New-ScheduledTaskTrigger -AtLogOn -User $sid), (New-ScheduledTaskTrigger -Once -At (Get-Date).AddMinutes(5) -RepetitionInterval (New-TimeSpan -Minutes 5)))
$principal = New-ScheduledTaskPrincipal -UserId $sid -LogonType Interactive -RunLevel Limited
$taskSettings = New-ScheduledTaskSettingsSet -StartWhenAvailable -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -MultipleInstances IgnoreNew -ExecutionTimeLimit (New-TimeSpan -Minutes 10)
$task = New-ScheduledTask -Action $action -Trigger $triggers -Principal $principal -Settings $taskSettings -Description 'Keep local Jenkins and the interactive UE build agent running while this user is logged in.'
Register-ScheduledTask -TaskName 'FestaFiesta-Jenkins-Local' -InputObject $task -Force | Out-Null
Write-Output ('Installed Jenkins ' + $jenkinsVersion + ' at ' + $InstallRoot + '. Credentials remain in the protected secrets directory.')
if ($Start) { & (Join-Path $InstallRoot 'scripts\Start-FestaFiestaJenkins.ps1') -InstallRoot $InstallRoot }
