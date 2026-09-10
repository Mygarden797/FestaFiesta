Set-StrictMode -Version Latest

function Invoke-RoadEditorProcess {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string[]] $ExtraArguments,
        [Parameter(Mandatory = $true)][ValidatePattern('^[A-Za-z0-9._-]+$')][string] $Label,
        [string] $EngineRoot,
        [ValidateRange(30, 14400)][int] $TimeoutSeconds = 1800
    )

    $ErrorActionPreference = 'Stop'
    Import-Module (Join-Path $PSScriptRoot '..\Build\FestaFiesta.Build.psm1') -Force
    $projectRoot = Get-FFProjectRoot
    $projectFile = Get-FFProjectFile
    $engineDirectory = Resolve-FFEngineRoot -EngineRoot $EngineRoot
    $editorExecutable = Join-Path $engineDirectory 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
    $qaRoot = Join-Path $projectRoot 'Saved\Roads_20260910'
    [IO.Directory]::CreateDirectory($qaRoot) | Out-Null
    $runName = '{0}_{1}_{2}' -f $Label, (Get-Date -Format 'yyyyMMdd_HHmmss_fff'), ([Guid]::NewGuid().ToString('N').Substring(0, 8))
    $logPath = Join-Path $qaRoot ($runName + '.log')
    $reportPath = Join-Path $qaRoot ($runName + '_Report')
    $runRecordPath = Join-Path $qaRoot ($runName + '_run.json')
    $previousCache = [Environment]::GetEnvironmentVariable('UE-LocalDataCachePath', 'Process')
    $arguments = @(
        ('"' + $projectFile + '"'), '/Engine/Maps/Entry',
        '-DisablePlugins=RiderLink', '-unattended', '-nosplash', '-nosound', '-NoSourceControl',
        '-RenderOffscreen', '-ini:Engine:[SystemSettings]:r.CEFGPUAcceleration=0',
        '-ddc=InstalledNoZenLocalFallback', '-noxgeshadercompile',
        ('-ShaderWorkingDir="' + (Join-Path $qaRoot '_shaderwork') + '"'),
        ('-abslog="' + $logPath + '"'),
        ('-ReportExportPath="' + $reportPath + '"')
    ) + $ExtraArguments
    $record = [ordered]@{
        status = 'STARTING'
        started_utc = [DateTime]::UtcNow.ToString('o')
        finished_utc = $null
        project = $projectFile
        executable = $editorExecutable
        arguments = $arguments
        pid = $null
        exit_code = $null
        timed_out = $false
        timeout_seconds = $TimeoutSeconds
        log = $logPath
        report = $reportPath
    }
    $editorProcess = $null
    try {
        [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', (Join-Path $qaRoot '_cache'), 'Process')
        # The real GPU RHI also supports preview captures and existing light-containing actors.
        $editorProcess = Start-Process -FilePath $editorExecutable -ArgumentList $arguments -WindowStyle Hidden -PassThru
        $record.pid = $editorProcess.Id
        $record.status = 'RUNNING'
        $record | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $runRecordPath -Encoding UTF8
        Write-Host ('ROAD_EDITOR_STARTED pid=' + $editorProcess.Id + ' log=' + $logPath)
        $stopwatch = [Diagnostics.Stopwatch]::StartNew()
        while (-not $editorProcess.WaitForExit(30000)) {
            if ($stopwatch.Elapsed.TotalSeconds -ge $TimeoutSeconds) {
                $record.timed_out = $true
                $record.status = 'TIMED_OUT'
                # Only terminate the commandlet started by this invocation.
                $editorProcess.Kill()
                $editorProcess.WaitForExit()
                throw ('Unreal exceeded ' + $TimeoutSeconds + ' seconds; log=' + $logPath)
            }
            Write-Host ('ROAD_EDITOR_RUNNING pid=' + $editorProcess.Id + ' seconds=' + [int]$stopwatch.Elapsed.TotalSeconds)
        }
        $editorProcess.WaitForExit()
        $record.exit_code = $editorProcess.ExitCode
        $record.status = if ($editorProcess.ExitCode -eq 0) { 'EXITED' } else { 'FAILED' }
        Write-Host ('ROAD_EDITOR_FINISHED exit=' + $editorProcess.ExitCode + ' log=' + $logPath)
        if ($editorProcess.ExitCode -ne 0) {
            throw ('Unreal exited with ' + $editorProcess.ExitCode + '; log=' + $logPath)
        }
        if (-not (Test-Path -LiteralPath $logPath -PathType Leaf)) {
            throw ('Unreal did not write its log: ' + $logPath)
        }
    }
    catch {
        if (-not $record.timed_out) { $record.status = 'FAILED' }
        $record['error'] = $_.Exception.Message
        throw
    }
    finally {
        if ($null -ne $editorProcess -and $editorProcess.HasExited) {
            $record.exit_code = $editorProcess.ExitCode
        }
        $record.finished_utc = [DateTime]::UtcNow.ToString('o')
        $record | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $runRecordPath -Encoding UTF8
        [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', $previousCache, 'Process')
        if ($null -ne $editorProcess) { $editorProcess.Dispose() }
    }
    return [PSCustomObject]@{
        LogPath = $logPath
        ReportPath = $reportPath
        RunRecordPath = $runRecordPath
        ValidationPath = (Join-Path $qaRoot ($runName + '_validation.json'))
        ExitCode = $record.exit_code
    }
}

Export-ModuleMember -Function Invoke-RoadEditorProcess
