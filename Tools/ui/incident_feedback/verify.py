"""사건 연속 해소·종료 문구·복귀 및 소식 이력 경로만 검사한다. 에셋은 저장하지 않는다."""
import json
import re
import time
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUTPUT = ROOT / 'Saved/IncidentFeedback'
OUTPUT.mkdir(parents=True, exist_ok=True)
TESTS = (
    'FestaFiesta.Incident.CrowdBridge.SustainedClearAndHandleSafety',
    'FestaFiesta.Incident.ClosedToast',
    'FestaFiesta.UI.Incident.DetailPanelResponseStatus',
    'FestaFiesta.Incident.StaffDispatch.ArrivalAndCancellationLifecycle',
    'FestaFiesta.UI.OperationAlert.RulesPickKinds',
    'FestaFiesta.UI.OperationAlert.SubsystemCooldown',
    'FestaFiesta.UI.OperationAlert.PanelToastsAndHistory',
    'FestaFiesta.UI.OperationAlert.IncidentHistory',
    'FestaFiesta.UI.SNS.UnifiedIndividualRead',
)
REPORT = {'status': 'running', 'requested_tests': TESTS}
STARTED = time.monotonic()
LOG = ROOT / 'Saved/BrightGlobe/incident_feedback_verify.log'


def write_report():
    (OUTPUT / 'verify-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')


def finish(delta_time):
    elapsed = time.monotonic() - STARTED
    if elapsed < 10:
        return
    text = LOG.read_text(encoding='utf-8', errors='replace') if LOG.exists() else ''
    matches = re.findall(r'Test Completed\. Result=\{([^}]+)\}.*?Path=\{([^}]+)\}', text)
    results = {name: state for state, name in matches if name in TESTS}
    if len(results) != len(TESTS) and elapsed < 90:
        return
    passed = all(results.get(test) == 'Success' for test in TESTS)
    REPORT.update(status='passed' if passed else 'failed', tests=results)
    write_report()
    if not passed:
        unreal.log_error('INCIDENT_FEEDBACK_FAIL: 사건 해소·종료 안내 회귀 확인 실패')
    unreal.unregister_slate_post_tick_callback(exit_handle)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()


write_report()
world = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_world()
unreal.SystemLibrary.execute_console_command(world, 'Automation RunTests ' + '+'.join(TESTS))
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
exit_handle = unreal.register_slate_post_tick_callback(finish)
