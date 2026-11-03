[CmdletBinding()]
param(
    [ValidateSet('Status', 'Logs', 'Build', 'BuildOnly', 'VerifyDrive', 'RepairShell', 'StopBuild')][string] $Action = 'Status',
    [string] $InstallRoot = 'C:\FestaFiestaCI'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$settings = Get-Content -LiteralPath (Join-Path $InstallRoot 'settings.json') -Raw | ConvertFrom-Json
$baseUrl = 'http://127.0.0.1:' + $settings.Port
$password = [IO.File]::ReadAllText((Join-Path $InstallRoot 'secrets\admin-password.txt')).Trim()
$headers = @{ Authorization = 'Basic ' + [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes('festafiesta-admin:' + $password)) }
$session = New-Object Microsoft.PowerShell.Commands.WebRequestSession
$jobUrl = $baseUrl + '/job/FestaFiesta-Dev-Daily'
function Send-FFJenkinsPost([string]$Url, [hashtable]$Form = @{}, [string]$XmlBody = '') {
    Add-Type -AssemblyName System.Net.Http
    $handler = New-Object Net.Http.HttpClientHandler
    $handler.AllowAutoRedirect = $false
    $handler.CookieContainer = $session.Cookies
    $client = New-Object Net.Http.HttpClient($handler)
    try {
        foreach ($key in $headers.Keys) { $null = $client.DefaultRequestHeaders.TryAddWithoutValidation($key, [string]$headers[$key]) }
        $values = New-Object 'Collections.Generic.Dictionary[string,string]'
        foreach ($key in $Form.Keys) { $values.Add($key, [string]$Form[$key]) }
        $content = if ($XmlBody) { New-Object Net.Http.StringContent($XmlBody, [Text.Encoding]::UTF8, 'application/xml') } else { New-Object Net.Http.FormUrlEncodedContent($values) }
        try {
            $response = $client.PostAsync($Url, $content).GetAwaiter().GetResult()
            try {
                if ([int]$response.StatusCode -ge 400) { throw ('Jenkins request failed with HTTP ' + [int]$response.StatusCode) }
            } finally { $response.Dispose() }
        } finally { $content.Dispose() }
    } finally { $client.Dispose(); $handler.Dispose() }
}
if ($Action -eq 'Logs') {
    $response = Invoke-WebRequest -UseBasicParsing -Uri ($jobUrl + '/lastBuild/consoleText') -Headers $headers -WebSession $session
    $console = if ($response.Content -is [byte[]]) { [Text.Encoding]::UTF8.GetString($response.Content) } else { [string]$response.Content }
    $console -split '\r?\n' | Select-Object -Last 40
    return
}
if ($Action -eq 'Status') {
    $status = Invoke-RestMethod -Uri ($jobUrl + '/api/json?tree=name,buildable,nextBuildNumber,inQueue,lastBuild[number,building,result,duration,url],property[parameterDefinitions[name,defaultParameterValue[value]]],triggers[spec]') -Headers $headers -WebSession $session
    $computers = Invoke-RestMethod -Uri ($baseUrl + '/computer/api/json?tree=computer[displayName,offline,numExecutors]') -Headers $headers -WebSession $session
    $provider = if ($settings.PSObject.Properties.Name -contains 'UploadProvider') { $settings.UploadProvider } else { 'Rclone' }
    $tokenPresent = (Test-Path -LiteralPath $settings.RcloneConfig) -and (Select-String -LiteralPath $settings.RcloneConfig -Pattern '^token\s*=\s*\{' -Quiet)
    [pscustomobject]@{ Job=$status; Computers=$computers.computer;UploadProvider=$provider;RcloneTokenPresent=[bool]$tokenPresent;ConsoleUrl=($jobUrl + '/lastBuild/console') } | ConvertTo-Json -Depth 12
    return
}
$crumb = Invoke-RestMethod -Uri ($baseUrl + '/crumbIssuer/api/json') -Headers $headers -WebSession $session
$headers[$crumb.crumbRequestField] = $crumb.crumb
if ($Action -eq 'RepairShell') {
    $jobs = Invoke-RestMethod -Uri ($baseUrl + '/api/json?tree=jobs[name]') -Headers $headers -WebSession $session
    foreach ($name in @('FestaFiesta-Dev-Daily','FestaFiesta-Drive-ConnectionCheck')) {
        if ($name -notin @($jobs.jobs | Select-Object -ExpandProperty name)) { continue }
        $configurationUrl = $baseUrl + '/job/' + $name + '/config.xml'
        $response = Invoke-WebRequest -UseBasicParsing -Uri $configurationUrl -Headers $headers -WebSession $session
        $xmlText = if ($response.Content -is [byte[]]) { [Text.Encoding]::UTF8.GetString($response.Content) } else { [string]$response.Content }
        # Jenkins emits XML 1.1; these managed definitions use only XML 1.0
        # characters, while .NET XmlDocument does not accept the 1.1 declaration.
        $xmlText = $xmlText.Replace('<?xml version="1.1"', '<?xml version="1.0"')
        try { [xml]$configuration = $xmlText } catch { throw 'The managed Jenkins job configuration could not be parsed.' }
        $scriptNode = $configuration.SelectSingleNode('/flow-definition/definition/script')
        if ($null -eq $scriptNode) { throw 'The managed job does not contain its expected Pipeline definition.' }
        $script = $scriptNode.InnerText.Replace("`r`n","`n")
        if (-not $script.Contains('set "PSModulePath="')) {
            $scriptNode.InnerText = $script.Replace('@echo off', "@echo off`nset `"PSModulePath=`"")
            Send-FFJenkinsPost -Url $configurationUrl -XmlBody $configuration.OuterXml
        }
    }
    Write-Output 'Managed Jenkins jobs now let Windows PowerShell initialize its own module search path.'
    return
}
if ($Action -eq 'VerifyDrive') {
    $checkName = 'FestaFiesta-Drive-ConnectionCheck'
    $jobs = Invoke-RestMethod -Uri ($baseUrl + '/api/json?tree=jobs[name]') -Headers $headers -WebSession $session
    if ($checkName -notin @($jobs.jobs | Select-Object -ExpandProperty name)) {
        $checkScript = (Join-Path $InstallRoot 'scripts\Test-FestaFiestaLiveDrive.ps1').Replace('\','\\')
        $checkSettings = (Join-Path $InstallRoot 'settings.json').Replace('\','\\')
        $pipeline = "node('windows-festafiesta') { timeout(time: 10, unit: 'MINUTES') { stage('Windows credentials and Drive round trip') { bat '''@echo off`nset `"PSModulePath=`"`npowershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File `"$checkScript`" -SettingsPath `"$checkSettings`"`n''' } } }"
        $xml = '<flow-definition plugin="workflow-job"><description>Manual Windows-agent Google Drive upload and SHA256 readback check. Does not build or publish a game.</description><keepDependencies>false</keepDependencies><properties><org.jenkinsci.plugins.workflow.job.properties.DisableConcurrentBuildsJobProperty/></properties><definition class="org.jenkinsci.plugins.workflow.cps.CpsFlowDefinition" plugin="workflow-cps"><script>' + [Security.SecurityElement]::Escape($pipeline) + '</script><sandbox>true</sandbox></definition><triggers/><disabled>false</disabled></flow-definition>'
        Send-FFJenkinsPost -Url ($baseUrl + '/createItem?name=' + $checkName) -XmlBody $xml
    }
    Send-FFJenkinsPost -Url ($baseUrl + '/job/' + $checkName + '/build')
    Write-Output ('Queued the Windows-agent Drive integration check at ' + $baseUrl + '/job/' + $checkName)
    return
}
if ($Action -eq 'StopBuild') {
    $last = Invoke-RestMethod -Uri ($jobUrl + '/lastBuild/api/json?tree=number,building') -Headers $headers -WebSession $session
    if (-not $last.building) { Write-Output 'No build is currently running.'; return }
    Send-FFJenkinsPost -Url ($jobUrl + '/' + $last.number + '/stop')
    Write-Output ('Requested stop for build ' + $last.number)
} else {
    $body = @{ BUILD_ONLY = if ($Action -eq 'BuildOnly') { 'true' } else { 'false' } }
    Send-FFJenkinsPost -Url ($jobUrl + '/buildWithParameters') -Form $body
    Write-Output ('Queued ' + $Action + ' at ' + $jobUrl)
}
