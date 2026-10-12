"""프로그램 규칙과 시설별 편성 흐름만 확인한다. PIE와 렌더링은 실행하지 않는다."""
from pathlib import Path
import json
import re
import time
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
LOG = ROOT / 'Saved/BrightGlobe/prep_program_feedback_test.log'
REPORT = ROOT / 'Saved/PrepProgramFeedback/test-report.json'
PREFIX = 'FestaFiesta.UI.PrepProgram.'
EXPECTED = {PREFIX + name for name in ('BlockReason', 'ReasonOrder', 'Quote', 'FacilityNavigation')}
START = time.monotonic()

def finish(results, error=None):
    unreal.unregister_slate_post_tick_callback(HANDLE)
    REPORT.write_text(json.dumps({'status': 'failed' if error else 'passed', 'tests': results,
        'error': error, 'elapsed_seconds': round(time.monotonic()-START, 2)}, ensure_ascii=False, indent=2), encoding='utf-8')
    if error: unreal.log_error('PREP_PROGRAM_FEEDBACK_TEST_FAIL: ' + error)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()

def tick(delta):
    text = LOG.read_text(encoding='utf-8', errors='replace') if LOG.exists() else ''
    matches = re.findall(r'Test Completed\. Result=\{([^}]+)\}.*?Path=\{([^}]+)\}', text)
    results = {name: state for state, name in matches if name.startswith(PREFIX)}
    if EXPECTED.issubset(results) and time.monotonic()-START > 10:
        finish(results, None if all(state == 'Success' for state in results.values()) else '프로그램 검증 실패')
    elif time.monotonic()-START > 90:
        finish(results, '90초 내 프로그램 검사 완료 행을 찾지 못함')

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
world = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_world()
unreal.SystemLibrary.execute_console_command(world, 'Automation RunTests ' + PREFIX)
HANDLE = unreal.register_slate_post_tick_callback(tick)
