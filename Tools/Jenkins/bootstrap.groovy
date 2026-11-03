import jenkins.model.Jenkins
import hudson.model.Node
import hudson.model.Result
import hudson.model.BooleanParameterDefinition
import hudson.model.ParametersDefinitionProperty
import hudson.security.HudsonPrivateSecurityRealm
import hudson.security.FullControlOnceLoggedInAuthorizationStrategy
import hudson.slaves.DumbSlave
import hudson.slaves.JNLPLauncher
import hudson.slaves.RetentionStrategy
import hudson.tasks.LogRotator
import hudson.triggers.TimerTrigger
import groovy.json.JsonSlurper
import org.jenkinsci.plugins.workflow.job.WorkflowJob
import org.jenkinsci.plugins.workflow.cps.CpsFlowDefinition
import org.jenkinsci.plugins.workflow.job.properties.DisableConcurrentBuildsJobProperty
import org.jenkinsci.plugins.workflow.job.properties.PipelineTriggersJobProperty

def root = new File(System.getenv('FESTAFIESTA_CI_ROOT'))
def settings = new JsonSlurper().parse(new File(root, 'settings.json'))
def j = Jenkins.get()
j.setNumExecutors(0)
j.setSlaveAgentPort(-1)
if (!(j.getSecurityRealm() instanceof HudsonPrivateSecurityRealm)) {
    def realm = new HudsonPrivateSecurityRealm(false)
    def password = new File(root, 'secrets/admin-password.txt').getText('UTF-8').trim()
    realm.createAccount('festafiesta-admin', password)
    j.setSecurityRealm(realm)
}
def authorization = new FullControlOnceLoggedInAuthorizationStrategy()
authorization.setAllowAnonymousRead(false)
j.setAuthorizationStrategy(authorization)
def location = jenkins.model.JenkinsLocationConfiguration.get()
location.setUrl("http://127.0.0.1:${settings.Port}/")
location.save()

def node = j.getNode('windows-local')
if (node == null) {
    def launcher = new JNLPLauncher()
    launcher.setWebSocket(true)
    node = new DumbSlave('windows-local', new File(root, 'agent').path, launcher)
    node.setNumExecutors(1)
    node.setMode(Node.Mode.EXCLUSIVE)
    node.setLabelString('windows-festafiesta')
    node.setNodeDescription('Local interactive Windows agent for UE 5.7 GPU validation')
    node.setRetentionStrategy(new RetentionStrategy.Always())
    j.addNode(node)
}
new File(root, 'secrets/agent-secret.txt').setText(j.getComputer('windows-local').getJnlpMac(), 'UTF-8')

// Only create the managed job once; subsequent restarts preserve operator changes.
def jobName = 'FestaFiesta-Dev-Daily'
if (j.getItem(jobName) == null) {
    def job = j.createProject(WorkflowJob, jobName)
    job.setDescription('Dev changes at 19:00 Asia/Seoul. Verified Win64 builds to C:\\FestaFiestaBuild, then version/date Google Drive upload. Full validation; no skip flags. Drive authentication must be completed before normal publication.')
    job.setBuildDiscarder(new LogRotator(30, 30, -1, -1))
    job.addProperty(new DisableConcurrentBuildsJobProperty())
    job.addProperty(new ParametersDefinitionProperty(new BooleanParameterDefinition('BUILD_ONLY', false, 'Validate and retain a pending build without Drive authentication or upload.')))
    job.addProperty(new PipelineTriggersJobProperty([new TimerTrigger('TZ=Asia/Seoul\n0 19 * * *')]))
    def scriptPath = new File(root, 'scripts/Run-DailyBuild.ps1').path.replace('\\', '\\\\')
    def settingsPath = new File(root, 'settings.json').path.replace('\\', '\\\\')
    def pipeline = """
node('windows-festafiesta') {
    timeout(time: 6, unit: 'HOURS') {
        stage('Dev build and Drive publication') {
            def buildOnlyFlag = params.BUILD_ONLY ? '-BuildOnly' : ''
            bat '''@echo off
set "PSModulePath="
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "${scriptPath}" -SettingsPath "${settingsPath}" -BuildNumber "%BUILD_NUMBER%" ''' + buildOnlyFlag
        }
    }
}
"""
    job.setDefinition(new CpsFlowDefinition(pipeline, true))
    job.save()
}
j.save()
new File(root, 'bootstrap-complete.txt').setText('Jenkins local setup complete. No build was automatically triggered.\n', 'UTF-8')
