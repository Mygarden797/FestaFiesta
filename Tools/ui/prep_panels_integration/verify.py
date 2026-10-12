"""P6 통합 확인. P1 PIE 실행기를 재사용하고 실제 게임 viewport와 패널 동작을 검사한다.

전용 일회용 에디터에서만 실행한다. 물리 마우스/게임패드 입력 검사는 아니다.
HUD CTA 여백 보정 후 나머지 에셋·설정은 저장하지 않는다.
"""
from pathlib import Path
import importlib.util
import json
import os
import re
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
HERE = ROOT / 'Tools/ui/prep_panels_integration'
OUT = ROOT / 'Saved/PrepPanelsIntegration'
OUT.mkdir(parents=True, exist_ok=True)


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


fix = load_module('prep_cta_fix', HERE / 'fix_hud_cta.py')
fix_report = fix.apply()
tests = {
    'FestaFiesta.UI.PrepHUD.Center.ViewportGeometry',
    'FestaFiesta.UI.PrepHUD.Center.ResizeAndFirstFrame',
    'FestaFiesta.UI.PrepHUD.BrightBottom.RenderedLayout',
    'FestaFiesta.UI.LiveHUD.BrightBottom.RenderedLayout',
}
for source in (ROOT / 'Source/FestaFiesta/Private/UI/PrepHUD/Panels').glob('*Tests.cpp'):
    tests.update(re.findall(r'"(FestaFiesta\.UI\.(?:PrepProgram|PrepStaff|PrepPromotion|PrepCheck|PrepSkyCorrections)\.[^"\s]+)"',
                           source.read_text(encoding='utf-8-sig')))
options = dict(log_path=str(ROOT / 'Saved/BrightGlobe/prep_panels_integration.log'),
               native_probe_command='FF.PrepPanelsIntegrationProbe',
               native_probe_path='Saved/PrepPanelsIntegration/native-geometry.json',
               automation_tests=sorted(tests), timeout_seconds=600, quit_editor=True)
actions_only = os.environ.get('FF_PREP_INTEGRATION_ACTIONS_ONLY') == '1'
if actions_only:
    options.update(automation_tests=[], cases=[['standard_16x9', 1920, 1080]])
(OUT / 'verify-request.json').write_text(json.dumps(options, indent=2), encoding='utf-8')
previous = os.environ.get('FF_PREP_PANEL_VERIFY_OUT')
os.environ['FF_PREP_PANEL_VERIFY_OUT'] = 'Saved/PrepPanelsIntegration'
try:
    base = load_module('prep_center_integration_base', ROOT / 'Tools/ui/prep_panel_center/verify.py')
finally:
    if previous is None:
        os.environ.pop('FF_PREP_PANEL_VERIFY_OUT', None)
    else:
        os.environ['FF_PREP_PANEL_VERIFY_OUT'] = previous

base.REPORT.update(scope='실제 PIE 3해상도/네 패널/편집값/재오픈/하단 HUD 및 UI 처리 경로',
                   validation_mode='actions_followup' if actions_only else 'full',
                   hud_cta_fix=fix_report['status'], asset_save_requested=True,
                   saved_asset_scope='HUD Box_Cta 세로 여백만 보정, 이후 보호 해시 검사')
actions = load_module('prep_integration_actions', HERE / 'actions.py')
original_inspect = base.inspect_center
original_tick = base.tick


def capture(name):
    # Shot SHOWUI의 전역 viewport 선택 대신 소유 월드의 게임 SViewport만 캡처한다.
    base.command('FF.PrepPanelsIntegrationProbe ' + name)
    path = OUT / (name + '.png')
    if str(path) not in base.REPORT['images']:
        base.REPORT['images'].append(str(path))
    return path


def inspect_center(label, expected=None):
    row = original_inspect(label, expected)
    native = row['native_probe']
    assert native.get('integration_ok'), ('실제 화면 경계 검사 실패', label, native.get('failures'))
    assert native.get('capture_ok'), ('PIE viewport 캡처 실패', native)
    return row


def tick(delta):
    state = base.STATE
    if state['stage'] == 'measure_build' and state['case_index'] == len(base.CASES)-1:
        try:
            if not state.get('actions_started'):
                base.check_build()
                state['actions_started'] = time.monotonic()
                actions.start(base.hud())
            if not state.get('actions_done'):
                assert time.monotonic()-state['actions_started'] < 90, '실제 패널 처리 경로 검사 시간 초과'
                result = actions.tick()
                if result is None:
                    return
                base.REPORT['panel_actions'] = result
                assert result.get('status') == 'passed', ('패널 처리 경로 검사 실패', result)
                state['actions_done'] = True
                base.hud().select_tab(unreal.FF_PrepHudTab.BUILD)
                state['next'] = time.monotonic()+1
                return
        except Exception:
            base.REPORT.setdefault('errors', []).append(traceback.format_exc())
            base.advance('stop_error')
    original_tick(delta)


base.capture = capture
base.inspect_center = inspect_center
unreal.unregister_slate_post_tick_callback(base.HANDLE)
base.HANDLE = unreal.register_slate_post_tick_callback(tick)
base.write_report()
