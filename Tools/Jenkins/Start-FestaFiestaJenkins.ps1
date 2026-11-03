[CmdletBinding()]
param([string] $InstallRoot = 'C:\FestaFiestaCI')
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$settings = Get-Content -LiteralPath (Join-Path $InstallRoot 'settings.json') -Raw | ConvertFrom-Json
$java = Join-Path $InstallRoot 'tools\java\bin\java.exe'
$baseUrl = 'http://127.0.0.1:' + $settings.Port
$logs = Join-Path $InstallRoot 'logs'
$null = New-Item -ItemType Directory -Path $logs -Force
$mutex = New-Object System.Threading.Mutex($false, 'Local\FestaFiestaJenkinsLauncher')
if (-not $mutex.WaitOne(0)) { exit 0 }
try {
    $env:JENKINS_HOME = Join-Path $InstallRoot 'home'
    $env:FESTAFIESTA_CI_ROOT = $InstallRoot
    $ready = $false
    try { $ready = (Invoke-WebRequest -UseBasicParsing -Uri ($baseUrl + '/login') -TimeoutSec 3).StatusCode -eq 200 } catch { }
    if (-not $ready) {
        # Listen only on loopback. The agent connects with WebSocket over this same port.
        $controllerArgs = @('-Xms256m', '-Xmx1536m', '-Duser.timezone=Asia/Seoul', '-Djenkins.install.runSetupWizard=false', '-jar', ('"' + (Join-Path $InstallRoot 'tools\jenkins.war') + '"'), '--httpListenAddress=127.0.0.1', ('--httpPort=' + $settings.Port))
        $controller = Start-Process -FilePath $java -ArgumentList $controllerArgs -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $logs 'controller.out.log') -RedirectStandardError (Join-Path $logs 'controller.err.log')
        [System.IO.File]::WriteAllText((Join-Path $InstallRoot 'controller.pid'), [string]$controller.Id)
        for ($attempt = 0; $attempt -lt 90; $attempt++) {
            if ($controller.HasExited) { throw 'Jenkins exited during startup. See controller.err.log.' }
            try { $ready = (Invoke-WebRequest -UseBasicParsing -Uri ($baseUrl + '/login') -TimeoutSec 2).StatusCode -eq 200 } catch { }
            if ($ready -and (Test-Path -LiteralPath (Join-Path $InstallRoot 'secrets\agent-secret.txt'))) { break }
            Start-Sleep -Seconds 2
        }
        if (-not $ready) { throw 'Jenkins startup timed out.' }
    }
    $password = [IO.File]::ReadAllText((Join-Path $InstallRoot 'secrets\admin-password.txt')).Trim()
    $headers = @{ Authorization = 'Basic ' + [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes('festafiesta-admin:' + $password)) }
    try {
        $computer = Invoke-RestMethod -Uri ($baseUrl + '/computer/windows-local/api/json?tree=offline') -Headers $headers -TimeoutSec 3
        if (-not $computer.offline) { Write-Output 'Jenkins controller and local Windows agent are running.'; return }
    } catch { }
    $agentPidPath = Join-Path $InstallRoot 'agent-process.json'
    if (Test-Path -LiteralPath $agentPidPath) {
        try {
            $record = Get-Content -LiteralPath $agentPidPath -Raw | ConvertFrom-Json
            $existing = Get-Process -Id ([int]$record.id) -ErrorAction SilentlyContinue
            if ($existing -and $existing.Path -eq $java -and $existing.StartTime.ToUniversalTime().ToString('o') -eq $record.startedAtUtc) {
                Write-Output 'The local Windows agent process is reconnecting.'; return
            }
        } catch {
            Write-Warning 'The previous agent process record is invalid; a fresh agent will be started.'
        }
    }
    $agentJar = Join-Path $InstallRoot 'tools\agent.jar'
    Invoke-WebRequest -UseBasicParsing -Uri ($baseUrl + '/jnlpJars/agent.jar') -OutFile $agentJar
    $agentArgs = @('-Xmx512m', '-jar', ('"' + $agentJar + '"'), '-url', $baseUrl, '-name', 'windows-local', '-secret', ('"@' + (Join-Path $InstallRoot 'secrets\agent-secret.txt') + '"'), '-webSocket', '-workDir', ('"' + (Join-Path $InstallRoot 'agent') + '"'))
    $agent = Start-Process -FilePath $java -ArgumentList $agentArgs -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $logs 'agent.out.log') -RedirectStandardError (Join-Path $logs 'agent.err.log')
    $agentRecord = [ordered]@{id=$agent.Id;startedAtUtc=$agent.StartTime.ToUniversalTime().ToString('o');executable=$java}
    $agentRecordTemporaryPath = $agentPidPath + '.' + [Guid]::NewGuid().ToString('N') + '.tmp'
    try {
        [IO.File]::WriteAllText($agentRecordTemporaryPath, ($agentRecord | ConvertTo-Json), (New-Object Text.UTF8Encoding($false)))
        if ([IO.File]::Exists($agentPidPath)) {
            [IO.File]::Replace($agentRecordTemporaryPath, $agentPidPath, [NullString]::Value)
        } else {
            [IO.File]::Move($agentRecordTemporaryPath, $agentPidPath)
        }
    } finally {
        if ([IO.File]::Exists($agentRecordTemporaryPath)) { [IO.File]::Delete($agentRecordTemporaryPath) }
    }
    Write-Output ('Jenkins started at ' + $baseUrl + '. Agent PID: ' + $agent.Id)
} finally {
    $mutex.ReleaseMutex()
    $mutex.Dispose()
}
