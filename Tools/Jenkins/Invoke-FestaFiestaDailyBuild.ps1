# Jenkins entry point. Keep this control script outside the checkout it builds.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string] $SourceRepository,
    [string] $SourceBranch = 'Dev',
    [Parameter(Mandatory = $true)][string] $CheckoutRoot,
    [string] $OutputRoot = 'C:\FestaFiestaBuild',
    [Parameter(Mandatory = $true)][string] $StateRoot,
    [Parameter(Mandatory = $true)][string] $EngineRoot,
    [Parameter(Mandatory = $true)][string] $RcloneExe,
    [Parameter(Mandatory = $true)][string] $RcloneConfig,
    [string] $RcloneRemote = 'festafiesta-drive',
    [ValidateSet('Rclone', 'Duck')][string] $UploadProvider = 'Rclone',
    [string] $DuckExe,
    [string] $DuckConfig,
    [string] $DriveFolderId = '13QQv-LeeTXIWG00bcQ9N9q-1I2vE_spL',
    [ValidateSet('Shipping', 'Development')][string] $Configuration = 'Shipping',
    [string] $BuildNumber = $env:BUILD_NUMBER,
    [switch] $BuildOnly,
    [switch] $Plan,
    # Optional offline input for -Plan. Execution always discovers the remote SHA.
    [string] $CandidateCommit
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'FestaFiesta.Jenkins.psm1') -Force
Invoke-FFDailyBuild @PSBoundParameters
