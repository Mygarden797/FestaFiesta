"""P08 공용 Frontend의 실제 PIE API·델리게이트 배선 회귀 검사.

마우스/키보드/게임패드 이벤트를 합성하지 않는다. 렌더 외형 검사는 별도 캡처다.
에셋을 저장하지 않으며 PIE 종료 확인 뒤 callback과 Python keep-alive를 해제한다.
"""
from datetime import datetime, timezone
from pathlib import Path
import gc
import hashlib
import json
import time
import traceback

import unreal


ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = ROOT / 'Saved/BrightGlobe'
REPORT = OUT / 'p08_frontend_verify.json'
FRONTEND = '/Game/FestaFiesta/Maps/Frontend'
DEST = '/Game/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1/P05'
CATALOG = '/Game/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas'
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
handle = None
state = {'stage': 'preflight', 'started': time.monotonic(), 'next': time.monotonic() + 2.,
         'pie_requested': False}
report = {
    'status': 'running', 'started_at': datetime.now(timezone.utc).isoformat(),
    'scope': '저장된 Frontend PIE의 API 및 버튼 델리게이트 배선',
    'physical_mouse_keyboard_gamepad_tested': False, 'visual_quality_verified': False,
    'asset_save_requested': False, 'checks': [], 'cleanup_errors': [],
    'process_exit_code': None,
}


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def write_report():
    OUT.mkdir(parents=True, exist_ok=True)
    report['last_stage'] = state.get('stage')
    pending = REPORT.with_suffix('.writing.json')
    pending.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    pending.replace(REPORT)


def checked(name, **evidence):
    report['checks'].append({'name': name, 'passed': True, **evidence})
    write_report()


def advance(stage, delay=.6):
    state['stage'] = stage
    state['next'] = time.monotonic() + delay
    write_report()


def active_screen(cls):
    # UWidgetBlueprintLibrary의 Python 이름은 ScriptName에 따라 WidgetLibrary다.
    # 라이브러리 별칭 대신 현재 PIE 플레이어가 소유한 실제 인스턴스만 찾는다.
    active = [screen for screen in unreal.ObjectIterator(cls)
              if screen.get_owning_player() == state['controller'] and screen.is_activated()]
    assert len(active) == 1, f'{cls.__name__} 활성 위젯 {len(active)}개'
    return active[0]


def find_widget(screen, name):
    # WidgetTree는 UPROPERTY여도 CPF_Edit/BlueprintVisible이 없어 snake_case 이름이
    # Python 속성표에 등록되지 않는다. WidgetTree 클래스 자체도 노출되지 않는다.
    tree = unreal.find_object(screen, 'WidgetTree')
    if tree is not None:
        assert tree.get_class().get_name() == 'WidgetTree' and tree.get_outer() == screen
    else:
        trees = {}
        for candidate in unreal.ObjectIterator(unreal.Widget):
            outer = candidate.get_outer()
            if outer and outer.get_class().get_name() == 'WidgetTree' and outer.get_outer() == screen:
                trees[outer.get_path_name()] = outer
        assert len(trees) == 1, f'실기 WidgetTree 식별 실패: {screen.get_name()}/{len(trees)}'
        tree = next(iter(trees.values()))
    widget = unreal.find_object(tree, name, unreal.Widget)
    assert widget, f'실기 위젯을 찾지 못함: {screen.get_name()}/{name}'
    return widget


def broadcast_button(button, require_enabled=True):
    assert isinstance(button, unreal.FF_CommonButtonBase), button.get_name()
    if require_enabled:
        assert button.get_editor_property('is_enabled'), f'버튼 비활성: {button.get_name()}'
    # UFF_CommonButtonBase의 요청 이벤트부터 실행한다. 실제 포인터 입력 검사가 아니다.
    button.get_editor_property('on_ff_button_clicked').broadcast(button)


def title_panel_check(title):
    values = {}
    for name in ('Border_LeftPanel', 'Border_LeftPanelShadow'):
        widget = find_widget(title, name)
        assert widget.get_visibility() == unreal.SlateVisibility.COLLAPSED, name
        values[name] = str(widget.get_visibility())
    for name in ('Btn_Play', 'Btn_Options', 'Btn_Quit', 'Image_BrandMark', 'Image_Wordmark'):
        widget = find_widget(title, name)
        assert widget.get_visibility() not in (unreal.SlateVisibility.HIDDEN, unreal.SlateVisibility.COLLAPSED), name
    return values


def orientation(globe):
    value = globe.get_orientation()
    return {key: float(value.get_editor_property(key)) for key in ('spin', 'tilt')}


def orientation_delta(a, b):
    return max(abs((a[key] - b[key] + 180.) % 360. - 180.) for key in a)


def pin_readiness(globe):
    pins = list(globe.get_pin_states())
    rows = {str(pin.get_editor_property('country_id')): bool(pin.get_editor_property('selectable')) for pin in pins}
    assert len(rows) == len(pins), '중복 국가 핀'
    expected = {code: row['unlocked'] for code, row in state['countries'].items()}
    assert rows == expected, '실기 국가 선택 가능 상태가 카탈로그와 다름'
    return rows


def layer_contract(globe):
    components = list(globe.get_editor_property('land_layer_components'))
    assert len(components) == 16
    root = globe.get_editor_property('globe_root')
    expected = [f'SM_BrightWorld_{tile:02d}_{part}' for tile in range(8) for part in ('Top', 'Coast')]
    actual = []
    for comp, name in zip(components, expected):
        mesh = comp.get_editor_property('static_mesh')
        assert mesh and mesh.get_path_name() == DEST + '/' + name + '.' + name, comp.get_name()
        assert comp.get_owner() == globe and comp.get_attach_parent() == root
        assert comp.get_material(0) == mesh.get_material(0), comp.get_name()
        assert comp.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
        actual.append(mesh.get_path_name())
    ocean = globe.get_editor_property('globe_mesh')
    assert ocean.get_attach_parent() == root
    assert ocean.get_material(0).get_path_name() == DEST + '/M_BrightGlobe_water.M_BrightGlobe_water'
    return actual


def start_cleanup(error=None):
    if error and 'error' not in report:
        report['error'] = error
        unreal.log_error('BRIGHT_GLOBE_FAIL ' + error)
    report['status'] = 'ending_pie'
    state['stage'] = 'end_pie'
    state['cleanup_started'] = time.monotonic()
    # 강참조를 먼저 놓고 다음 tick에서 PIE 종료 요청을 한다.
    for key in ('world', 'flow', 'globe', 'rig', 'controller', 'globe_screen'):
        state.pop(key, None)
    state['next'] = time.monotonic() + .2
    write_report()


def finish():
    global handle
    try:
        assert not level.is_in_play_in_editor(), 'PIE가 끝나기 전에 검사 종료됨'
        if 'file_hashes_before' in report:
            after = {rel: digest(ROOT / rel) for rel in report['file_hashes_before']}
            report['file_hashes_after'] = after
            assert after == report['file_hashes_before'], '검사 중 저장 에셋 바이트가 바뀜'
            report['assets_unchanged'] = True
        report['pie_ended'] = True
    except Exception:
        error = traceback.format_exc()
        report['cleanup_errors'].append(error)
        if 'error' not in report:
            report['error'] = error
            unreal.log_error('BRIGHT_GLOBE_FAIL ' + error)
    report['status'] = 'failed' if 'error' in report else 'passed'
    report['finished_at'] = datetime.now(timezone.utc).isoformat()
    report['elapsed_seconds'] = time.monotonic() - state['started']
    write_report()
    unreal.unregister_slate_post_tick_callback(handle)
    handle = None
    state.clear()
    gc.collect()
    # 설치 5.7 Python 실행기가 다음 tick에 QUIT_EDITOR를 예약한다.
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)


def tick(delta_seconds):
    try:
        now = time.monotonic()
        if now < state['next']:
            return
        stage = state['stage']
        state['next'] = now + .2
        if stage not in ('end_pie', 'wait_end', 'settle_end') and now - state['started'] > 180.:
            raise TimeoutError('PIE 회귀 검사 180초 초과')
        if stage == 'preflight':
            assert not level.is_in_play_in_editor(), '다른 PIE가 실행 중'
            integration = json.loads((OUT / 'p06_frontend_integration.json').read_text(encoding='utf-8-sig'))
            assert integration.get('status') == 'passed', 'Frontend 연결 검사가 완료되지 않음'
            report['integration_report_sha256'] = digest(OUT / 'p06_frontend_integration.json')
            report['file_hashes_before'] = {row['file']: digest(ROOT / row['file']) for row in integration['file_hashes_after']}
            assert report['file_hashes_before'] == {row['file']: row['sha256'] for row in integration['file_hashes_after']}, '연결 뒤 변경된 에셋이 있음'
            assert level.load_level(FRONTEND)
            catalog = unreal.load_asset(CATALOG)
            state['catalog_before'] = {
                key: [row.export_text() for row in catalog.get_editor_property(key)]
                for key in ('countries', 'entries')
            }
            state['countries'] = {
                str(row.get_editor_property('country_id')): {
                    'unlocked': bool(row.get_editor_property('unlocked')),
                    'lock_reason': str(row.get_editor_property('lock_reason')),
                } for row in catalog.get_editor_property('countries')
            }
            assert state['countries']['KR']['unlocked'] and not state['countries']['DE']['unlocked'], 'KR/DE 준비 상태가 전제와 달라짐'
            state['pie_requested'] = True
            level.editor_request_begin_play()
            advance('wait_pie', 1.)
        elif stage == 'wait_pie':
            if not level.is_in_play_in_editor():
                return
            world = unreal.EditorLevelLibrary.get_game_world()
            if not world:
                return
            instance = unreal.GameplayStatics.get_game_instance(world)
            flows = [x for x in unreal.ObjectIterator(unreal.FF_UIFlowSubsystem) if x.get_outer() == instance]
            if len(flows) != 1 or flows[0].get_frontend_screen() != unreal.FF_UIFlowScreen.TITLE:
                return
            globes = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.FF_AtlasGlobeActor)
            rigs = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.FF_AtlasCameraRig)
            assert len(globes) == len(rigs) == 1
            state.update(world=world, flow=flows[0], globe=globes[0], rig=rigs[0],
                         controller=unreal.GameplayStatics.get_player_controller(world, 0))
            globes[0].set_auto_spin_paused(True)
            advance('title', 2.)
        elif stage == 'title':
            globe = state['globe']
            title = active_screen(unreal.FF_TitleScreen)
            assert title.get_class().get_name() == 'WBP_TitleMain_MyCasual_C'
            checked('실제 타이틀·16개 세계판 육지 연결', layers=layer_contract(globe),
                    runtime_panels=title_panel_check(title), readiness=pin_readiness(globe))
            broadcast_button(find_widget(title, 'Btn_Play'))
            advance('play_hub', 1.)
        elif stage == 'play_hub':
            assert state['flow'].get_frontend_screen() == unreal.FF_UIFlowScreen.PLAY_HUB
            hub = active_screen(unreal.FF_PlayHubScreen)
            checked('타이틀 플레이 요청이 플레이 허브로 연결')
            broadcast_button(find_widget(hub, 'Button_NewFestival'))
            advance('atlas_ready', 2.)
        elif stage == 'atlas_ready':
            assert state['flow'].get_frontend_screen() == unreal.FF_UIFlowScreen.ATLAS_GLOBE
            screen = active_screen(unreal.FF_AtlasGlobeScreen)
            state['rig'].skip_camera_move()
            state['globe_screen'] = screen
            checked('새 축제 요청이 지구 국가 목록으로 연결')
            screen.inspect_country('KR')
            advance('korea_focus', 1.)
        elif stage == 'korea_focus':
            globe = state['globe']
            if globe.is_transitioning():
                return
            assert str(globe.get_focused_country_id()) == 'KR'
            assert str(globe.get_selected_country_id()) == 'KR'
            assert pin_readiness(globe)['KR']
            button = find_widget(state['globe_screen'], 'Button_SelectCountry')
            assert button.get_editor_property('is_enabled')
            state['korea_orientation'] = orientation(globe)
            checked('대한민국 포커스·선택·국가 지도 버튼 활성', orientation=state['korea_orientation'])
            broadcast_button(button)
            advance('country_map', 2.)
        elif stage == 'country_map':
            assert state['flow'].get_frontend_screen() == unreal.FF_UIFlowScreen.ATLAS_COUNTRY
            assert str(state['flow'].get_active_country_id()) == 'KR'
            country = active_screen(unreal.FF_AtlasCountryScreen)
            state['rig'].skip_camera_move()
            checked('대한민국 국가 지도 진입', active_country=str(state['flow'].get_active_country_id()),
                    globe_hidden_observed=bool(state['globe'].get_editor_property('hidden')))
            country.get_editor_property('on_back_requested').broadcast()
            advance('country_back', 2.)
        elif stage == 'country_back':
            assert state['flow'].get_frontend_screen() == unreal.FF_UIFlowScreen.ATLAS_GLOBE
            state['rig'].skip_camera_move()
            assert orientation_delta(orientation(state['globe']), state['korea_orientation']) < .001
            state['globe_screen'] = active_screen(unreal.FF_AtlasGlobeScreen)
            checked('국가 지도 뒤로·지구 방향 보존')
            state['globe_screen'].inspect_country('DE')
            advance('germany_focus', 1.)
        elif stage == 'germany_focus':
            globe = state['globe']
            if globe.is_transitioning():
                return
            assert str(globe.get_focused_country_id()) == 'DE'
            assert str(globe.get_selected_country_id()) == 'DE'
            assert not pin_readiness(globe)['DE']
            button = find_widget(state['globe_screen'], 'Button_SelectCountry')
            assert not button.get_editor_property('is_enabled')
            text = find_widget(state['globe_screen'], 'Text_LockReason')
            assert str(text.get_text()) == state['countries']['DE']['lock_reason']
            assert text.get_visibility() not in (unreal.SlateVisibility.HIDDEN, unreal.SlateVisibility.COLLAPSED)
            checked('독일 준비 중 유지·지도 버튼 비활성', lock_reason=str(text.get_text()))
            # 비활성 상태에서도 핸들러 자체가 잠금 국가 진입을 방어하는지 검사한다.
            broadcast_button(button, require_enabled=False)
            advance('locked_guard', .6)
        elif stage == 'locked_guard':
            assert state['flow'].get_frontend_screen() == unreal.FF_UIFlowScreen.ATLAS_GLOBE
            assert str(state['flow'].get_active_country_id()) == 'KR'
            checked('잠긴 국가 지도 진입 요청 방어')
            globe = state['globe']
            before = orientation(globe)
            component = list(globe.get_editor_property('land_layer_components'))[0]
            before_world = component.get_world_transform().export_text()
            globe.add_drag_input(unreal.Vector2D(50., 12.))
            after = orientation(globe)
            assert orientation_delta(before, after) > .01
            assert str(globe.get_selected_country_id()) == 'None'
            assert str(globe.get_focused_country_id()) == 'None'
            assert component.get_world_transform().export_text() != before_world
            checked('드래그 API 회전·선택 해제·육지 동반 회전', before=before, after=after)
            globe.step_focus(1)
            globe.skip_transition()
            assert str(globe.get_focused_country_id()) in state['countries']
            checked('다음 국가 포커스 API', focused_country=str(globe.get_focused_country_id()))
            globe.set_globe_active(False)
            assert globe.get_editor_property('hidden')
            assert all(c.get_owner().get_editor_property('hidden') for c in globe.get_editor_property('land_layer_components'))
            checked('명시적 지구 숨김 API가 육지 소유 액터에도 적용')
            advance('restore_globe', .3)
        elif stage == 'restore_globe':
            state['globe'].set_globe_active(True)
            assert not state['globe'].get_editor_property('hidden')
            layer_contract(state['globe'])
            checked('지구 다시 표시·16개 연결 보존')
            assert state['flow'].go_back()
            advance('back_hub', 1.)
        elif stage == 'back_hub':
            assert state['flow'].get_frontend_screen() == unreal.FF_UIFlowScreen.PLAY_HUB
            assert state['flow'].go_back()
            advance('back_title', 1.)
        elif stage == 'back_title':
            assert state['flow'].get_frontend_screen() == unreal.FF_UIFlowScreen.TITLE
            state['rig'].skip_camera_move()
            checked('타이틀 복귀·외곽 패널 계속 숨김', runtime_panels=title_panel_check(active_screen(unreal.FF_TitleScreen)))
            catalog = unreal.load_asset(CATALOG)
            after = {key: [row.export_text() for row in catalog.get_editor_property(key)] for key in ('countries', 'entries')}
            assert after == state['catalog_before'], '검사 중 공유 카탈로그 메모리가 바뀜'
            checked('국가·축제 카탈로그 메모리 보존')
            start_cleanup()
        elif stage == 'end_pie':
            if state.get('pie_requested'):
                level.editor_request_end_play()
                advance('wait_end', .5)
            else:
                # 이 검사가 시작하지 않은 PIE를 종료하지 않는다.
                advance('settle_end', .2)
        elif stage == 'wait_end':
            if level.is_in_play_in_editor():
                if now - state['cleanup_started'] > 30.:
                    raise TimeoutError('PIE 종료가 30초 이내 완료되지 않음')
                return
            report['pie_ended'] = True
            advance('settle_end', 2.)
        elif stage == 'settle_end':
            finish()
        else:
            raise AssertionError('알 수 없는 검사 단계: ' + str(stage))
    except Exception:
        error = traceback.format_exc()
        if state.get('stage') in ('end_pie', 'wait_end', 'settle_end'):
            report['cleanup_errors'].append(error)
            report.setdefault('error', error)
            unreal.log_error('BRIGHT_GLOBE_FAIL ' + error)
            finish()
        else:
            start_cleanup(error)


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle = unreal.register_slate_post_tick_callback(tick)
