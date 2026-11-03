[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('Inspect','Apply','Verify','Test','Capture')]
    [string] $Phase,
    [ValidatePattern('^[A-Za-z0-9._+]+$')]
    [string] $TestFilter = 'FestaFiesta.UI.PrepBuild+FestaFiesta.UI.FacilityInfo',
    [string] $EngineRoot = 'C:\Unreal\UE_5.7'
)
$ErrorActionPreference = 'Stop'
$catalogRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$catalogOut = Join-Path $catalogRoot 'Saved/BuildCatalogV10'
New-Item -ItemType Directory -Path $catalogOut -Force | Out-Null
if (Get-Process UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
    throw 'Use an idle editor process for this isolated validation/migration runner.'
}
$catalogStarted = Get-Date
$catalogRunName = $Phase.ToLower() + '-' + $catalogStarted.ToString('yyyyMMdd-HHmmss')
$catalogUser = Join-Path $catalogOut ($catalogRunName + '-User')
$catalogLog = Join-Path $catalogOut ($catalogRunName + '.log')
$catalogArgs = @(('"' + (Join-Path $catalogRoot 'FestaFiesta.uproject') + '"'),
    '-unattended','-nosound','-nosplash','-NoSourceControl','-DisablePlugins=RiderLink',
    '-ddc=FestaFiestaBuildDDC',('-UserDir="' + $catalogUser + '"'),('-abslog="' + $catalogLog + '"'))
$catalogReport = $null
if ($Phase -in @('Inspect','Apply','Verify')) {
    $catalogScript = Join-Path $PSScriptRoot ($Phase.ToLower() + '.py')
    $catalogArgs += @('-run=pythonscript', ('-script="' + $catalogScript + '"'), '-NullRHI')
    $catalogReport = Join-Path $catalogOut (@{Inspect='preflight.json';Apply='apply-result.json';Verify='verify.json'}[$Phase])
}
elseif ($Phase -eq 'Test') {
    $catalogExport = Join-Path $catalogOut ($catalogRunName + '-Report')
    $catalogArgs += @('/Engine/Maps/Entry','-RenderOffscreen','-nocefaccelpaint','-SaveToUserDir',
        ('-ReportExportPath="' + $catalogExport + '"'), '-TestExit="Automation Test Queue Empty"',
        ('-ExecCmds="Automation RunTests ' + $TestFilter + '"'))
    $catalogReport = Join-Path $catalogExport 'index.json'
}
else {
    $catalogArgs += @('/Engine/Maps/Entry','-RenderOffscreen','-nocefaccelpaint','-SaveToUserDir',
        ('-ExecutePythonScript="' + (Join-Path $PSScriptRoot 'capture.py') + '"'))
    $catalogReport = Join-Path $catalogOut 'runtime-latest.json'
}
$catalogProcess = Start-Process -FilePath (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') -ArgumentList $catalogArgs -WindowStyle Hidden -PassThru
Write-Output ('CATALOG_' + $Phase.ToUpper() + '_STARTED pid=' + $catalogProcess.Id + ' log=' + $catalogLog)
$catalogProcess.WaitForExit()
if ($catalogProcess.ExitCode -ne 0) { throw ('Unreal exited ' + $catalogProcess.ExitCode + ': ' + $catalogLog) }
if (!(Test-Path -LiteralPath $catalogReport) -or (Get-Item -LiteralPath $catalogReport).LastWriteTime -lt $catalogStarted) {
    throw ('No current report: ' + $catalogReport + '; log=' + $catalogLog)
}
$catalogResult = Get-Content -LiteralPath $catalogReport -Raw | ConvertFrom-Json
if ($Phase -eq 'Test') {
    if ($catalogResult.failed -ne 0 -or ($catalogResult.succeeded + $catalogResult.succeededWithWarnings) -lt 1 -or $catalogResult.notRun -gt 0 -or $catalogResult.inProcess -gt 0) {
        throw ('Automation failed: ' + $catalogReport)
    }
}
elseif ($Phase -ne 'Inspect' -and $catalogResult.status -ne 'PASS') {
    throw ('Validation/migration failed: ' + $catalogReport)
}
Write-Output ('CATALOG_' + $Phase.ToUpper() + '_PASS report=' + $catalogReport)
