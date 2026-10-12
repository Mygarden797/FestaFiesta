"""소식·중앙 매출 순위만 한 에디터에서 적용하고 해당 소식 필터 검사를 실행한다."""
from pathlib import Path
import json
import re
import runpy
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/UnifiedLiveStyle'
OUT.mkdir(parents=True, exist_ok=True)
REPORT = {'status': 'running', 'visual_check': '실제 화면은 사용자 확인 예정'}
try:
    results = {}
    for folder, report_folder in [('unified_news', 'UnifiedNews'), ('revenue_sky', 'RevenueSky')]:
        runpy.run_path(str(ROOT / 'Tools/ui' / folder / 'apply.py'), init_globals={'FF_BATCH_APPLY': True})
        result = json.loads((ROOT / 'Saved' / report_folder / 'apply-report.json').read_text(encoding='utf-8'))
        results[folder] = result
        assert result['status'] == 'passed', result
    REPORT.update(status='passed', applications=results)
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
    unreal.log_error('UNIFIED_LIVE_STYLE_FAIL: ' + REPORT['error'])

expected = {'FestaFiesta.UI.SNS.UnifiedNewsSources'}
started = time.monotonic()
log_path = ROOT / 'Saved/BrightGlobe/unified_live_style_apply.log'
if REPORT['status'] == 'passed':
    world = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_world()
    unreal.SystemLibrary.execute_console_command(world, 'Automation RunTests ' + '+'.join(sorted(expected)))


def finish(delta_time):
    elapsed = time.monotonic() - started
    if elapsed < 10:
        return
    if REPORT['status'] == 'passed':
        log = log_path.read_text(encoding='utf-8', errors='replace') if log_path.exists() else ''
        matches = re.findall(r'Test Completed\. Result=\{([^}]+)\}.*?Path=\{([^}]+)\}', log)
        results = {name: state for state, name in matches if name in expected}
        if not expected.issubset(results) and elapsed < 90:
            return
        REPORT['tests'] = results
        if not expected.issubset(results) or any(state != 'Success' for state in results.values()):
            REPORT.update(status='failed', error='소식 필터 검사 실패 또는 시간 초과')
            unreal.log_error('UNIFIED_LIVE_STYLE_FAIL: ' + REPORT['error'])
    (OUT / 'apply-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(exit_handle)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
exit_handle = unreal.register_slate_post_tick_callback(finish)
