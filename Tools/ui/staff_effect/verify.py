"""숙련도 작업시간과 병합된 결산·운영 알림 경로를 검사한다. 에셋은 저장하지 않는다."""
import json
import re
import time
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUTPUT = ROOT / 'Saved/StaffEffect'
OUTPUT.mkdir(parents=True, exist_ok=True)
TESTS = (
    'FestaFiesta.Staff.Facilities.AtomicDispatchFromAssignedExit',
    'FestaFiesta.Incident.StaffDispatch.TrainingWorkMinutes',
    'FestaFiesta.Incident.StaffDispatch.ArrivalAndCancellationLifecycle',
    'FestaFiesta.UI.ViewModel.EconomyRelay.StaffPromotion',
    'FestaFiesta.UI.Incident.DetailPanelResponseStatus',
    'FestaFiesta.UI.SatisfactionBreakdown.ReportPanelHostsWidget',
    'FestaFiesta.UI.SatisfactionBreakdown.LossesAddUpToScore',
    'FestaFiesta.UI.ViewModel.EconomyRelay.RecordedDailySatisfaction',
    'FestaFiesta.UI.OperationAlert.RulesPickKinds',
)
REPORT = {'status': 'running', 'requested_tests': TESTS}
STARTED = time.monotonic()
LOG = ROOT / 'Saved/BrightGlobe/staff_effect_verify.log'


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
        unreal.log_error('STAFF_EFFECT_FAIL: 스태프 숙련도 및 병합 영향 회귀 확인 실패')
    unreal.unregister_slate_post_tick_callback(exit_handle)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()


write_report()
world = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_world()
unreal.SystemLibrary.execute_console_command(world, 'Automation RunTests ' + '+'.join(TESTS))
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
exit_handle = unreal.register_slate_post_tick_callback(finish)
