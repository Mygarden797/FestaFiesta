"""P1 실제 PIE 공통 배치 검사. 시설 설치/경제 검증은 하지 않으며 에셋을 저장하지 않는다.

Live 진입 순간에만 기존 검증 방식대로 메모리의 필수시설 목록을 비우고 finally로 복원한다.
run.ps1이 개인 Saved 설정과 세이브를 별도 백업/복원한다. 물리 마우스 입력 검사가 아니다.
"""
from pathlib import Path
import gc
import hashlib
import json
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = ROOT / 'Saved/LivePanelsSky'
LOG = ROOT / 'Saved/BrightGlobe/live_panels_sky_verify.log'
REQUEST = OUT / 'verify-request.json'
OPTIONS = json.loads(REQUEST.read_text(encoding='utf-8-sig')) if REQUEST.exists() else {}
CASES = OPTIONS.get('cases', [('small', 1280, 720), ('standard', 1920, 1080), ('wide', 2560, 1080)])
TABS = ['OPERATIONS', 'FACILITIES', 'GUESTS', 'QUEUES', 'SNS', 'STAFF', 'DENSITY']
TESTS = OPTIONS.get('automation_tests', [
    'FestaFiesta.UI.LiveHUD.Center.ViewportGeometry',
    'FestaFiesta.UI.LiveHUD.Center.ResizeAndSavedLayout',
    'FestaFiesta.UI.LiveHUD.Center.SevenTabsOnly',
    'FestaFiesta.UI.LiveHUD.SkyFrame.PreserveContentAndDesignerEdits',
    'FestaFiesta.UI.PrepHUD.BrightBottom.RenderedLayout',
    'FestaFiesta.UI.LiveHUD.BrightBottom.RenderedLayout',
])
LEVEL = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
STATE = dict(stage='tests', next=time.monotonic()+1, start=time.monotonic(), busy=False, case=0, tab=0)
REPORT = dict(status='running', cases=[], physical_input_tested=False, asset_save_requested=False,
              fixture='Live 진입 순간 필수시설 목록을 메모리에서 비우고 즉시 복원. 게임 규칙 검사가 아니다.')
HANDLE = None
LIVE_PATH = '/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveHudScreen'
LIVE_CLASS = unreal.EditorAssetLibrary.load_blueprint_class(LIVE_PATH)
LIVE_CDO = unreal.get_default_object(LIVE_CLASS)
ORIGINAL_LAYOUTS = dict(LIVE_CDO.get_editor_property('panel_layouts'))


def hashes():
    return {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
            for folder in ('Content/FestaFiesta/UI', 'Content/FestaFiesta/Building/FF_WBP_Building', 'Config')
            for p in (ROOT/folder).rglob('*') if p.is_file() and p.suffix in ('.uasset', '.umap', '.ini')}


BEFORE = hashes()


def write():
    REPORT.update(stage=STATE['stage'], elapsed_seconds=round(time.monotonic()-STATE['start'], 2))
    (OUT/'verify-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')


def advance(stage, delay=.5):
    STATE.update(stage=stage, next=time.monotonic()+delay)
    write()


def world():
    return unreal.EditorLevelLibrary.get_game_world()


def subsystem(cls):
    gi = unreal.GameplayStatics.get_game_instance(world())
    return next((o for o in unreal.ObjectIterator(cls) if o.get_outer() == gi), None)


def widget(cls):
    return next((o for o in unreal.ObjectIterator(cls) if o.get_world() == world()
                 and o.get_parent() is not None and o.get_visibility() not in
                 (unreal.SlateVisibility.COLLAPSED, unreal.SlateVisibility.HIDDEN)), None)


def hud():
    return widget(unreal.FF_LiveHudScreen)


def command(value):
    unreal.SystemLibrary.execute_console_command(world() or LEVEL.get_world(), value)


def case():
    return REPORT['cases'][-1]


def edit_defaults(table):
    global LIVE_CDO
    LIVE_CDO = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(LIVE_PATH))
    LIVE_CDO.set_editor_property('panel_layouts', table)
    # 사용자와 동일하게 Compile까지 수행해야 BP의 캐시된 위젯 템플릿도 새 기본값을 쓴다.
    unreal.BlueprintEditorLibrary.compile_blueprint(unreal.load_asset(LIVE_PATH))
    LIVE_CDO = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(LIVE_PATH))
    assert {str(k): v.export_text() for k, v in LIVE_CDO.get_editor_property('panel_layouts').items()} == {
        str(k): v.export_text() for k, v in table.items()}


def capture(suffix='', nudge=''):
    tag = case()['name'] + '-' + TABS[STATE['tab']].lower() + suffix
    command('FF.LivePanelCenterProbe ' + tag + (' ' + nudge if nudge else ''))
    return OUT / ('native-' + tag + '.json')


def measure(suffix=''):
    result = json.loads(capture(suffix).read_text(encoding='utf-8-sig'))
    assert result['ok'], result['failures']
    assert result['capture_ok'] and Path(result['capture_path']).is_file()
    assert result['controller_viewport_px'] == case()['viewport'], result
    # 원본 치수/오프셋에 DPI를 한 번만 적용하고 화면·하단 메뉴 안에서 제한한 중심과 비교한다.
    width, height = result['controller_viewport_px']
    dpi = result['dpi']
    size = result['host_size']
    offset = result['layout_offset']
    left, top, right, bottom = result['host_pixel_bounds']
    margin = hud().get_editor_property('panel_margin') * dpi
    for index, (extent, pixels, delta) in enumerate(zip(size, (width, height), offset)):
        half = extent*dpi/2
        low = margin+half
        high = pixels-margin-half-(result['bottom_height']*dpi if index == 1 else 0)
        expected = max(low, min(pixels/2+delta*dpi, high))
        actual = (left+right)/2 if index == 0 else (top+bottom)/2
        assert abs(actual-expected) < 1.5, ('중심 불일치', index, actual, expected, result)
    assert str(hud().get_editor_property('text_gold').get_text()).find('Gold') < 0
    assert abs(hud().get_bottom_status_row_height()-36) < .1
    assert abs(hud().get_bottom_bar_height()-68) < .1
    case()['measurements'].append(result)


def finish():
    global HANDLE
    edit_defaults(ORIGINAL_LAYOUTS)
    after = hashes()
    if BEFORE != after:
        REPORT.setdefault('errors', []).append('검증 중 에셋/Config 파일 변경')
    REPORT['protected_files_unchanged'] = len(BEFORE) if BEFORE == after else False
    REPORT['status'] = 'failed' if REPORT.get('errors') else 'passed'
    STATE['stage'] = 'finished'
    write()
    unreal.unregister_slate_post_tick_callback(HANDLE)
    HANDLE = None
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()


def tick(delta):
    if STATE['busy'] or time.monotonic() < STATE['next']:
        return
    STATE['busy'] = True
    try:
        STATE['next'] = time.monotonic()+.5
        stage = STATE['stage']
        if stage not in ('stop_error', 'finish_error', 'finished'):
            assert time.monotonic()-STATE['start'] < OPTIONS.get('timeout_seconds', 480), '검증 시간 초과'
        if stage == 'tests':
            assert not LEVEL.is_in_play_in_editor()
            STATE['log_start'] = len(LOG.read_text(encoding='utf-8', errors='replace').splitlines())
            if TESTS:
                command('Automation RunTests ' + '+'.join(TESTS))
            advance('tests_done', 2)
        elif stage == 'tests_done':
            lines = LOG.read_text(encoding='utf-8', errors='replace').splitlines()[STATE['log_start']:]
            results = {name: [line for line in lines if 'Test Completed. Result=' in line and name in line] for name in TESTS}
            if not all(results.values()):
                return
            REPORT['automation_tests'] = {name: rows[-1] for name, rows in results.items()}
            assert all('Result={Success}' in row for row in REPORT['automation_tests'].values()), REPORT['automation_tests']
            atlas = unreal.load_asset('/Game/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas')
            entry = next(e for e in atlas.get_editor_property('entries') if e.get_editor_property('unlocked')
                         and '다대포' in str(e.get_editor_property('festival_name')))
            STATE['festival'] = str(entry.get_editor_property('festival_id'))
            advance('load')
        elif stage == 'load':
            assert LEVEL.load_level('/Game/FestaFiesta/Maps/Frontend')
            command('r.SetRes 4096x2160w')
            advance('pie', 2)
        elif stage == 'pie':
            name, width, height = CASES[STATE['case']]
            REPORT['cases'].append(dict(name=name, viewport=[width, height], measurements=[]))
            edited = dict(ORIGINAL_LAYOUTS)
            if STATE['case'] == 1:
                # EditDefaultsOnly 경로를 실제 인스턴스 생성 전에 검증한다. 저장은 하지 않는다.
                layout = unreal.FF_LivePanelLayout()
                layout.set_editor_property('width', 680)
                layout.set_editor_property('height', 540)
                edited[unreal.FF_LiveTab.STAFF] = layout
                case()['class_defaults_fixture'] = 'Staff 680x540'
            edit_defaults(edited)
            command(f'FF.FrontendBeginPIE {width-2} {height-12}')
            advance('festival', 5)
        elif stage == 'festival':
            if not world() or not subsystem(unreal.FF_UIFlowSubsystem):
                return
            assert subsystem(unreal.FF_UIFlowSubsystem).start_festival(unreal.Name(STATE['festival']))
            advance('live', 7)
        elif stage == 'live':
            prep = widget(unreal.FF_PrepHudBar)
            if not prep:
                return
            assert 'Gold' not in str(prep.get_editor_property('text_gold').get_text())
            assert abs(prep.get_bottom_bar_height()-64) < .1
            definition = subsystem(unreal.FF_FestivalProgressionSubsystem).get_definition()
            original = list(definition.get_editor_property('required_setup_facilities'))
            try:
                definition.set_editor_property('required_setup_facilities', [])
                subsystem(unreal.FF_FestivalStageSubsystem).request_transition(unreal.FF_FestivalStage.LIVE)
            finally:
                definition.set_editor_property('required_setup_facilities', original)
            advance('live_ready', 3)
        elif stage == 'live_ready':
            assert hud(), '라이브 HUD 없음'
            assert subsystem(unreal.FF_FestivalViewModelSubsystem).get_current_stage() == unreal.FF_FestivalStage.LIVE
            case()['start_remaining'] = subsystem(unreal.FF_FestivalViewModelSubsystem).get_remaining_seconds()
            case()['initial_layouts'] = {str(k): v.export_text() for k, v in hud().get_editor_property('panel_layouts').items()}
            unreal.AutomationLibrary.finish_loading_before_screenshot()
            command('DisableAllScreenMessages')
            STATE['tab'] = 0
            advance('open')
        elif stage == 'open':
            hud().toggle_tab(getattr(unreal.FF_LiveTab, TABS[STATE['tab']]))
            advance('warm1')
        elif stage == 'warm1':
            capture()
            advance('warm2')
        elif stage == 'warm2':
            capture()
            advance('measure', 1)
        elif stage == 'measure':
            measure()
            STATE['tab'] += 1
            if STATE['tab'] < len(TABS):
                advance('open')
            else:
                STATE['tab'] = TABS.index('STAFF')
                hud().toggle_tab(unreal.FF_LiveTab.STAFF)
                advance('edit')
        elif stage == 'edit':
            if STATE['case'] == 1:
                assert hud().get_open_panel_layout().width == 680
                assert hud().get_open_panel_layout().height == 540
            capture('-edit-request', '80 -40')
            advance('edit_measure', 1)
        elif stage == 'edit_measure':
            measure('-edited')
            offset = hud().get_open_panel_layout().offset
            assert abs(offset.x-80) < .01 and abs(offset.y+40) < .01
            assert case()['measurements'][-1]['nudge_allowed']
            capture('-nudge-request', '-25 35')
            advance('nudge_measure', 1)
        elif stage == 'nudge_measure':
            measure('-nudged')
            assert abs(hud().get_open_panel_layout().offset.x-55) < .01
            hud().close_current_panel()
            assert not hud().get_selection().has_selection
            advance('reopen')
        elif stage == 'reopen':
            hud().toggle_tab(unreal.FF_LiveTab.STAFF)
            advance('reopen_measure', 1)
        elif stage == 'reopen_measure':
            measure('-reopened')
            assert abs(hud().get_open_panel_layout().offset.y+5) < .01
            case()['end_remaining'] = subsystem(unreal.FF_FestivalViewModelSubsystem).get_remaining_seconds()
            assert case()['end_remaining'] < case()['start_remaining'], '패널 노출 중 Live 시계 정지'
            table = hud().get_editor_property('panel_layouts')
            for key, value in table.items():
                if key != unreal.FF_LiveTab.STAFF:
                    assert value.export_text() == case()['initial_layouts'][str(key)]
            case()['passed'] = True
            hud().close_current_panel()
            LEVEL.editor_request_end_play()
            advance('between', 3)
        elif stage == 'between':
            if LEVEL.is_in_play_in_editor():
                return
            gc.collect()
            STATE['case'] += 1
            if STATE['case'] < len(CASES):
                advance('load')
            else:
                finish()
        elif stage == 'stop_error':
            if LEVEL.is_in_play_in_editor():
                LEVEL.editor_request_end_play()
            STATE['cleanup_deadline'] = time.monotonic()+20
            advance('finish_error', 2)
        elif stage == 'finish_error':
            if LEVEL.is_in_play_in_editor() and time.monotonic() < STATE['cleanup_deadline']:
                return
            finish()
    except Exception:
        error = traceback.format_exc()
        REPORT.setdefault('errors', []).append(error)
        unreal.log_error('LIVE_PANELS_SKY_VERIFY: ' + error)
        if STATE['stage'] in ('stop_error', 'finish_error'):
            finish()
        else:
            advance('stop_error')
    finally:
        STATE['busy'] = False


write()
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
HANDLE = unreal.register_slate_post_tick_callback(tick)
