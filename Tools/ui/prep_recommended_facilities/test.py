"""권장 시설 판정 한 건만 실행한다. PIE나 렌더링은 실행하지 않는다."""
from pathlib import Path
import json
import re
import time
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
LOG = ROOT / 'Saved/BrightGlobe/prep_recommended_test.log'
REPORT = ROOT / 'Saved/PrepRecommended/test-report.json'
TEST = 'FestaFiesta.UI.PrepCheck.RecommendedFacilities'
START = time.monotonic()

def finish(error=None):
    unreal.unregister_slate_post_tick_callback(HANDLE)
    REPORT.write_text(json.dumps({'status': 'failed' if error else 'passed', 'test': TEST,
        'error': error, 'elapsed_seconds': round(time.monotonic()-START, 2)}, ensure_ascii=False, indent=2), encoding='utf-8')
    if error:
        unreal.log_error('PREP_RECOMMENDED_TEST_FAIL: ' + error)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()

def tick(delta):
    text = LOG.read_text(encoding='utf-8', errors='replace') if LOG.exists() else ''
    # 완료 행만 성공 근거로 사용한다. 필터에 테스트가 없는 경우는 시간 초과다.
    results = re.findall(r'Test Completed\. Result=\{([^}]+)\}.*?Path=\{([^}]+)\}', text)
    for state, name in results:
        if name == TEST and time.monotonic()-START > 10:
            finish(None if state == 'Success' else '판정 실패: ' + state)
            return
    if time.monotonic()-START > 90:
        finish('90초 내 테스트 완료 행을 찾지 못함')

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
world = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_world()
unreal.SystemLibrary.execute_console_command(world, 'Automation RunTests ' + TEST)
HANDLE = unreal.register_slate_post_tick_callback(tick)
