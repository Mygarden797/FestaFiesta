"""실제 프론트엔드→대한민국→축제 진입 화면을 캡처한다. 에셋을 저장하지 않는다."""
from pathlib import Path
import json
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
MAP_ONLY = globals().get('FF_CAPTURE_MAP_ONLY', False)
OUT = ROOT / ('Saved/BrightCountryHud/FinalMap' if MAP_ONLY else 'Saved/BrightCountryHud/Runtime')
OUT.mkdir(parents=True, exist_ok=True)
LEVEL = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
STATE = dict(stage='setup', next=time.monotonic()+1, start=time.monotonic(), busy=False)
REPORT = dict(status='running', images=[], physical_input_tested=False)
HANDLE = None


def advance(stage, delay=2):
    STATE.update(stage=stage, next=time.monotonic()+delay)


def world():
    return unreal.EditorLevelLibrary.get_game_world()


def flow():
    gi = unreal.GameplayStatics.get_game_instance(world())
    return next((o for o in unreal.ObjectIterator(unreal.FF_UIFlowSubsystem) if o.get_outer() == gi), None)


def widget(cls):
    return next((o for o in unreal.ObjectIterator(cls) if o.get_world() == world()
                 and o.get_visibility() not in (unreal.SlateVisibility.COLLAPSED, unreal.SlateVisibility.HIDDEN)), None)


def capture(name):
    path = OUT / (name+'.png')
    unreal.SystemLibrary.execute_console_command(world(), 'Shot SHOWUI filename="'+path.as_posix()+'" -nosuffix')
    REPORT['images'].append(str(path))


def tick(delta):
    if STATE['busy'] or time.monotonic() < STATE['next']:
        return
    STATE['busy'] = True
    try:
        STATE['next'] = time.monotonic()+1
        stage = STATE['stage']
        if stage not in ('stop', 'done'):
            assert time.monotonic()-STATE['start'] < 420, '실제 화면 캡처 시간 초과'
        if stage == 'setup':
            assert not LEVEL.is_in_play_in_editor()
            atlas = unreal.load_asset('/Game/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas')
            entries = list(atlas.get_editor_property('entries'))
            entry = next(e for e in entries if e.get_editor_property('unlocked')
                         and '다대포' in str(e.get_editor_property('festival_name')))
            STATE['festival'] = str(entry.get_editor_property('festival_id'))
            assert LEVEL.load_level('/Game/FestaFiesta/Maps/Frontend')
            unreal.SystemLibrary.execute_console_command(LEVEL.get_world(), 'r.SetRes 4096x2160w')
            advance('pie')
        elif stage == 'pie':
            unreal.SystemLibrary.execute_console_command(LEVEL.get_world(), 'FF.FrontendBeginPIE 1598 888')
            advance('country', 5)
        elif stage == 'country':
            if not world() or not flow(): return
            flow().set_active_country(unreal.Name('KR'))
            flow().go_to_screen(unreal.FF_UIFlowScreen.ATLAS_COUNTRY)
            advance('country_empty', 5)
        elif stage == 'country_empty':
            assert widget(unreal.FF_AtlasCountryScreen)
            unreal.AutomationLibrary.finish_loading_before_screenshot()
            unreal.SystemLibrary.execute_console_command(world(), 'DisableAllScreenMessages')
            capture('01-korea-empty')
            advance('focus')
        elif stage == 'focus':
            pin = next(o for o in unreal.ObjectIterator(unreal.FF_AtlasPinButton)
                       if o.get_world() == world() and str(o.get_pin_id()) == STATE['festival'])
            pin.set_keyboard_focus()
            advance('country_selected', 3)
        elif stage == 'country_selected':
            capture('02-korea-selected')
            advance('stop' if MAP_ONLY else 'start_festival')
        elif stage == 'start_festival':
            assert flow().start_festival(unreal.Name(STATE['festival']))
            advance('hud', 8)
        elif stage == 'hud':
            if not world() or not widget(unreal.FF_PrepHudBar): return
            unreal.AutomationLibrary.finish_loading_before_screenshot()
            capture('03-bottom-hud')
            advance('stop', 3)
        elif stage == 'stop':
            LEVEL.editor_request_end_play()
            advance('done', 3)
        elif stage == 'done':
            if LEVEL.is_in_play_in_editor(): return
            if MAP_ONLY:
                REPORT['status'] = 'failed' if 'error' in REPORT else 'captured'
                (OUT/'report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
                unreal.unregister_slate_post_tick_callback(HANDLE)
                unreal.EditorPythonScripting.set_keep_python_script_alive(False)
                return
            unreal.SystemLibrary.execute_console_command(LEVEL.get_world(),
                'Automation RunTests FestaFiesta.UI.PrepHUD.BrightBottom.RenderedLayout')
            advance('layout_test', 5)
        elif stage == 'layout_test':
            log = (ROOT/'Saved/BrightGlobe/bright_country_hud_runtime.log').read_text(encoding='utf-8', errors='replace')
            results = [line for line in log.splitlines() if 'Test Completed. Result=' in line
                       and 'FestaFiesta.UI.PrepHUD.BrightBottom.RenderedLayout' in line]
            if not results: return
            REPORT['layout_test'] = results[-1]
            if 'Result={Success}' not in results[-1]:
                REPORT['error'] = 'HUD 배치 테스트 실패. 실제 로그를 확인한다.'
            REPORT['status'] = 'failed' if 'error' in REPORT else 'captured'
            REPORT['elapsed_seconds'] = round(time.monotonic()-STATE['start'], 1)
            (OUT/'report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
            unreal.unregister_slate_post_tick_callback(HANDLE)
            unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    except Exception:
        REPORT['error'] = traceback.format_exc()
        unreal.log_error(REPORT['error'])
        advance('stop')
    finally:
        STATE['busy'] = False


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
HANDLE = unreal.register_slate_post_tick_callback(tick)
