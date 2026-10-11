"""기존 소식 WBP를 저장하지 않고 개별 읽음·행 입력 회귀만 확인한다."""
import hashlib
import json
import re
import time
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUTPUT = ROOT / 'Saved/NewsRead'
OUTPUT.mkdir(parents=True, exist_ok=True)
TESTS = (
    'FestaFiesta.UI.SNS.UnifiedIndividualRead',
    'FestaFiesta.UI.SNS.UnifiedNewsSources',
    'FestaFiesta.UI.SNS.PostReadInteraction',
)
ASSETS = tuple(ROOT / 'Content/FestaFiesta/UI/LiveHUD' / name for name in (
    'Panels/WBP_FF_LiveSnsFlyout.uasset',
    'Panels/WBP_FF_LiveSnsFlyoutPostRow.uasset',
    'WBP_FF_LiveUtilityDock.uasset',
))
BEFORE = {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest() for path in ASSETS}
REPORT = {'status': 'running', 'requested_tests': TESTS, 'asset_sha256_before': BEFORE}
STARTED = time.monotonic()
LOG = ROOT / 'Saved/BrightGlobe/news_read_verify.log'


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
    after = {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest() for path in ASSETS}
    passed = all(results.get(test) == 'Success' for test in TESTS) and after == BEFORE
    REPORT.update(status='passed' if passed else 'failed', tests=results, assets_unchanged=after == BEFORE)
    write_report()
    if not passed:
        unreal.log_error('NEWS_READ_FAIL: 소식 읽음 회귀 또는 원본 WBP 보존 확인 실패')
    unreal.unregister_slate_post_tick_callback(exit_handle)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()


write_report()
world = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_world()
unreal.SystemLibrary.execute_console_command(world, 'Automation RunTests ' + '+'.join(TESTS))
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
exit_handle = unreal.register_slate_post_tick_callback(finish)
