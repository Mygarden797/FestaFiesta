"""공감 지급·저장·입력과 사건 보상·기존 종료 흐름의 관련 검사만 실행한다."""
import json
import os
import re
import time
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUTPUT = ROOT / 'Saved/NewsRewards'
OUTPUT.mkdir(parents=True, exist_ok=True)
TESTS = (
    'FestaFiesta.NPC.Insights.ReactionRewardAndGuards',
    'FestaFiesta.NPC.Insights.ReactionPersistenceAndReset',
    'FestaFiesta.UI.SNS.PostReactionInteraction',
    'FestaFiesta.UI.SNS.PostReadInteraction',
    'FestaFiesta.UI.SNS.UnifiedIndividualRead',
    'FestaFiesta.Incident.Rewards.ActualReceiptAndSingleCompletion',
    'FestaFiesta.Incident.Rewards.ConfiguredAssets',
    'FestaFiesta.Incident.ClosedToast',
    'FestaFiesta.UI.OperationAlert.IncidentHistory',
    'FestaFiesta.Staff.Facilities.AtomicDispatchFromAssignedExit',
)
requested_test = os.environ.get('FF_NEWS_REWARDS_TEST', '')
if requested_test:
    if requested_test not in TESTS:
        raise ValueError('지정한 검사가 SNS 공감·사건 보상 검사 목록에 없습니다: ' + requested_test)
    TESTS = (requested_test,)
REPORT = {'status': 'running', 'requested_tests': TESTS}
STARTED = time.monotonic()
LOG = ROOT / 'Saved/BrightGlobe/news_rewards_verify.log'


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
    REPORT.update(status='passed' if all(results.get(test) == 'Success' for test in TESTS) else 'failed', tests=results)
    write_report()
    if REPORT['status'] != 'passed':
        unreal.log_error('NEWS_REWARDS_FAIL: 공감·사건 보상 검사 실패')
    unreal.unregister_slate_post_tick_callback(exit_handle)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()


write_report()
world = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_world()
unreal.SystemLibrary.execute_console_command(world, 'Automation RunTests ' + '+'.join(TESTS))
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
exit_handle = unreal.register_slate_post_tick_callback(finish)
