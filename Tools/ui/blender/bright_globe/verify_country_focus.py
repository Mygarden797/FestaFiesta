"""P12 국가 살펴보기의 실제 화면 투영 위치·화면비 검사.

별도 일회용 에디터에서 실행하며 에셋을 저장하지 않는다. 타이틀/허브는
버튼 델리게이트, 국가 선택은 목록과 공유하는 InspectCountry API를 호출한다.
물리 입력·나라 영역 클릭 판정·최종 외형 검사를 대신하지 않는다.
"""
from datetime import datetime, timezone
from pathlib import Path
import gc
import hashlib
import json
import math
import struct
import time
import traceback

import unreal


ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
BASE = ROOT / 'Saved/BrightGlobe'
REQUEST = BASE / 'country_focus_request.json'
config = json.loads(REQUEST.read_text(encoding='utf-8-sig')) if REQUEST.exists() else {}
BEFORE = bool(config.get('before', False))
OUT = (BASE / config.get('output', 'country_focus_' + ('before' if BEFORE else 'after') + '_v001')).resolve()
assert OUT != BASE.resolve() and BASE.resolve() in OUT.parents, '출력은 Saved/BrightGlobe 하위 전용 폴더여야 함'
assert not (OUT / 'report.json').exists(), '기존 결과 덮어쓰기 금지: 새 output 이름 사용'
OUT.mkdir(parents=True, exist_ok=True)
REPORT = OUT / 'report.json'
COUNTRIES = ('KR', 'BR', 'DE', 'JP', 'US', 'ES', 'TH', 'AU')
SCREENSHOT_COUNTRIES = {'KR', 'DE', 'US', 'AU'}
CASES = (('standard_16x9', 1600, 900, COUNTRIES),
         ('small_4x3', 960, 720, ('KR',)),
         ('ultrawide_21x9', 1680, 720, ('KR',)))
TOLERANCE = float(config.get('tolerance', .02))
assert math.isfinite(TOLERANCE) and 0 <= TOLERANCE < 1, 'tolerance는 정규화 화면 좌표의 허용 오차'
SETTLE_SECONDS = 2.
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
state = dict(stage='preflight', case_index=0, country_index=0,
             started=time.monotonic(), next=time.monotonic() + 2.)
handle = None
report = dict(
    status='running', before=BEFORE, started_at=datetime.now(timezone.utc).isoformat(),
    script_revision='p12_focus_lifecycle_v2',
    scope='실제 Frontend PIE의 InspectCountry 공유 API와 국가 핀 월드 위치의 화면 투영',
    entry_route='타이틀 Btn_Play 델리게이트 → 허브 Button_NewFestival 델리게이트 → Atlas',
    country_route='FF_AtlasGlobeScreen.inspect_country: 국가 목록 클릭과 공유하는 코어 API',
    country_row_click_delegate_tested=False, physical_mouse_keyboard_gamepad_tested=False,
    country_area_click_tested=False, visual_quality_verified=False,
    ui_included=True, asset_save_requested=False, settle_seconds=SETTLE_SECONDS,
    expected_uv=config.get('expected_uv', [.68, .5]), tolerance_uv=TOLERANCE,
    center_assertions_enabled=not BEFORE, cases=[], failures=[], cleanup_errors=[],
    lifecycle_api_checks=[], lifecycle_api_enabled=not BEFORE,
    process_exit_code=None,
)


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def hashes():
    paths = [ROOT / p for p in (
        'Content/FestaFiesta/Maps/Frontend.umap',
        'Content/FestaFiesta/UI/Title/GlassOrbit/WBP_TitleMain_MyCasual.uasset',
        'Content/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas.uasset',
        'Content/FestaFiesta/UI/Frontend/WBP_FF_PlayHub.uasset',
        'Content/FestaFiesta/UI/Atlas/WBP_FF_AtlasGlobe.uasset',
        'Content/FestaFiesta/UI/Atlas/WBP_FF_AtlasCountryRow.uasset',
    )]
    paths += sorted((ROOT / 'Content/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1/P10').glob('*.uasset'))
    return {p.relative_to(ROOT).as_posix(): digest(p) for p in paths}


def write_report():
    report['last_stage'] = state['stage']
    pending = REPORT.with_suffix('.writing.json')
    pending.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    pending.replace(REPORT)


def advance(stage, delay=.5):
    state.update(stage=stage, next=time.monotonic() + delay)
    write_report()


def active_screen(cls):
    screens = [obj for obj in unreal.ObjectIterator(cls)
               if obj.get_owning_player() == state['controller'] and obj.is_activated()]
    assert len(screens) == 1, f'{cls.__name__} 활성 인스턴스 수: {len(screens)}'
    return screens[0]


def find_widget(screen, name):
    tree = unreal.find_object(screen, 'WidgetTree')
    if tree is not None:
        assert tree.get_class().get_name() == 'WidgetTree' and tree.get_outer() == screen
    else:
        trees = {}
        for candidate in unreal.ObjectIterator(unreal.Widget):
            outer = candidate.get_outer()
            if outer and outer.get_class().get_name() == 'WidgetTree' and outer.get_outer() == screen:
                trees[outer.get_path_name()] = outer
        assert len(trees) == 1, f'{screen.get_name()} WidgetTree 유일성 확인 실패'
        tree = next(iter(trees.values()))
    widget = unreal.find_object(tree, name, unreal.Widget)
    assert widget, f'위젯 없음: {name}'
    return widget


def broadcast_button(button):
    assert isinstance(button, unreal.FF_CommonButtonBase)
    assert button.get_editor_property('is_enabled'), f'비활성 버튼: {button.get_name()}'
    button.get_editor_property('on_ff_button_clicked').broadcast(button)


def xyz(value):
    return [float(value.x), float(value.y), float(value.z)]


def rotation(value):
    return {key: float(value.get_editor_property(key)) for key in ('pitch', 'yaw', 'roll')}


def orientation(globe):
    value = globe.get_orientation()
    return {key: float(value.get_editor_property(key)) for key in ('spin', 'tilt')}


def orientation_delta(a, b):
    return max(abs((a[key] - b[key] + 180.) % 360. - 180.) for key in a)


def lifecycle_checked(name, **evidence):
    report['lifecycle_api_checks'].append(dict(name=name, passed=True, **evidence))
    write_report()


def adjacent_country(country, direction):
    # 나라 ID 순서가 아니라 현재 카탈로그의 지리 경도로 기대 이웃을 정한다.
    # 이 8개국은 서로 다른 경도이므로 일반적인 동률 인덱스 규칙을 복제하지 않는다.
    rows = report['catalog_longitudes']
    assert len(set(rows.values())) == len(rows), '경도가 같은 테스트 국가는 별도 기대값 필요'
    travel = {code: ((longitude - rows[country]) * direction) % 360.
              for code, longitude in rows.items() if code != country}
    return min(travel, key=travel.get)


def settled():
    assert time.monotonic() - state['selection_started'] < 20., '생명주기 검사 전환 완료 시간 초과'
    return not state['globe'].is_transitioning() and not state['rig'].is_moving()


def lifecycle_country(country):
    globe, controller = state['globe'], state['controller']
    assert str(globe.get_focused_country_id()) == country, f'생명주기 포커스 불일치: {country}'
    assert str(globe.get_selected_country_id()) == country, f'생명주기 선택 불일치: {country}'
    pin = next(pin for pin in globe.get_pin_states() if str(pin.get_editor_property('country_id')) == country)
    pixels = project(controller, pin.get_editor_property('world_location'))
    assert pixels is not None, '생명주기 선택국 투영 실패'
    viewport = list(controller.get_viewport_size())
    uv = [pixels[i] / viewport[i] for i in range(2)]
    target, source = expected_uv('standard_16x9')
    error = max(abs(uv[i] - target[i]) for i in range(2))
    assert error <= TOLERANCE, f'생명주기 선택국 중심 오차: {country}/{error}'
    return dict(country=country, normalized_projection=uv, expected_uv=target,
                expected_uv_source=source, max_axis_error_uv=error,
                orientation=orientation(globe))


def assert_idle_rotation(key):
    globe = state['globe']
    assert not globe.is_transitioning(), '취소/화면 이탈 뒤 전환이 재개됨'
    current = orientation(globe)
    delta = orientation_delta(state[key], current)
    assert delta < .001, f'자동 자전 일시정지 중 취소/이탈 뒤 지구가 다시 회전함: {delta}'
    return dict(before=state[key], after=current, orientation_delta_degrees=delta)


def expected_uv(case_name):
    # 이전 DLL의 before 검사에는 새 API가 없다. 수정 후에는 실제 UI에서 전달한
    # 액터 목표가 기준이며, 화면비별 고정 상수로 성공을 억지로 맞추지 않는다.
    globe = state['globe']
    if not BEFORE and hasattr(globe, 'get_focus_viewport_position'):
        value = globe.get_focus_viewport_position()
        return [float(value.x), float(value.y)], 'globe.get_focus_viewport_position'
    value = config.get('expected_uv', [.68, .5])
    if isinstance(value, dict):
        value = value.get(case_name, value.get('default', [.68, .5]))
    assert isinstance(value, (tuple, list)) and len(value) == 2, 'expected_uv는 [u,v] 또는 화면비별 매핑'
    value = [float(v) for v in value]
    assert all(math.isfinite(v) and 0 <= v <= 1 for v in value)
    return value, 'config.expected_uv 또는 임시 기본값: UI 경계 실측값 아님'


def project(controller, world_location):
    # 5.7의 bool + 출력 벡터 바인딩은 성공 시 Vector2D, 실패 시 None이다.
    # 튜플을 반환하는 바인딩도 명시적인 bool/Vector2D만 읽는다.
    result = controller.project_world_location_to_screen(world_location, False)
    if result is None:
        return None
    if isinstance(result, unreal.Vector2D):
        return [float(result.x), float(result.y)]
    if isinstance(result, (tuple, list)):
        if any(isinstance(item, bool) and not item for item in result):
            return None
        vectors = [item for item in result if isinstance(item, unreal.Vector2D)]
        assert len(vectors) == 1, f'예상하지 못한 투영 결과: {result}'
        return [float(vectors[0].x), float(vectors[0].y)]
    raise TypeError(f'예상하지 못한 투영 반환형: {type(result)}')


def geometry_evidence(screen):
    result = {}
    for name in ('Border_Panel', 'ScrollBox_Countries', 'Button_SelectCountry'):
        widget = find_widget(screen, name)
        geometry = widget.get_paint_space_geometry()
        size = unreal.SlateLibrary.get_local_size(geometry)
        available = size.x > 0 and size.y > 0
        row = dict(geometry_available=available, local_size=[size.x, size.y],
                   visible=widget.get_visibility() not in
                   (unreal.SlateVisibility.HIDDEN, unreal.SlateVisibility.COLLAPSED))
        if available:
            a, _ = unreal.SlateLibrary.local_to_viewport(state['world'], geometry, unreal.Vector2D(0, 0))
            b, _ = unreal.SlateLibrary.local_to_viewport(state['world'], geometry, size)
            row['bounds_pixels'] = [a.x, a.y, b.x, b.y]
        else:
            row['bounds_pixels'] = None
            row['limitation'] = '비표시 PIE geometry 없음: UI 배치는 SHOWUI PNG로 별도 확인'
        result[name] = row
    return result


def measure():
    name, width, height, countries = CASES[state['case_index']]
    country = countries[state['country_index']]
    globe, controller = state['globe'], state['controller']
    actual = list(controller.get_viewport_size())
    assert actual == [width, height], f'실제 viewport 불일치: 요청 {[width, height]} / 실제 {actual}'
    assert str(globe.get_selected_country_id()) == country, '선택국 불일치'
    assert str(globe.get_focused_country_id()) == country, '포커스국 불일치'
    pins = {str(pin.get_editor_property('country_id')): pin for pin in globe.get_pin_states()}
    assert country in pins, f'선택국 pin state 없음: {country}'
    pin = pins[country]
    location = pin.get_editor_property('world_location')
    pixels = project(controller, location)
    uv = [pixels[0] / actual[0], pixels[1] / actual[1]] if pixels is not None else None
    target, target_source = expected_uv(name)
    assert all(math.isfinite(v) and 0 <= v <= 1 for v in target), '목표 UV 범위 오류'
    delta = [uv[i] - target[i] for i in range(2)] if uv else None
    max_error = max(abs(v) for v in delta) if delta else None
    camera = controller.get_editor_property('player_camera_manager')
    root = globe.get_editor_property('globe_root')
    facing = bool(pin.get_editor_property('facing_viewer'))
    row = dict(case=name, country=country, requested=[width, height], viewport=actual,
               viewport_matches_requested=True, selected_country=str(globe.get_selected_country_id()),
               focused_country=str(globe.get_focused_country_id()), selected_world_location=xyz(location),
               facing_viewer=facing, selectable=bool(pin.get_editor_property('selectable')),
               projection_pixels=pixels, normalized_projection=uv, expected_uv=target,
               expected_uv_source=target_source,
               projection_delta_uv=delta, max_axis_error_uv=max_error,
               euclidean_error_uv=math.hypot(*delta) if delta else None,
               center_within_tolerance=bool(max_error is not None and max_error <= TOLERANCE),
               orientation=orientation(globe), actor_transform=globe.get_actor_transform().export_text(),
               actor_location=xyz(globe.get_actor_location()), actor_rotation=rotation(globe.get_actor_rotation()),
               globe_root_transform=root.get_world_transform().export_text(),
               camera_pov=dict(location=xyz(camera.get_camera_location()),
                               rotation=rotation(camera.get_camera_rotation()), fov=float(camera.get_fov_angle()),
                               view_target=controller.get_view_target().get_path_name()),
               widgets=geometry_evidence(state['screen']))
    report['cases'].append(row)
    if not BEFORE and (not row['center_within_tolerance'] or not facing):
        report['failures'].append(dict(case=name, country=country, max_axis_error_uv=max_error,
                                       facing_viewer=facing, reason='선택국 중심 투영 허용 오차 또는 앞면 판정 실패'))
    write_report()
    if country in SCREENSHOT_COUNTRIES:
        image = OUT / (name + '_' + country + '.png')
        assert not image.exists(), '기존 SHOWUI 캡처 덮어쓰기 금지'
        row['image'] = image.relative_to(ROOT).as_posix()
        state['image'] = image
        command('Shot SHOWUI filename="' + image.as_posix() + '" -nosuffix')
        advance('wait_image', 2.)
    else:
        advance('next_country', .1)


def command(text):
    unreal.SystemLibrary.execute_console_command(state['world'], text)


def release_world():
    for key in ('world', 'flow', 'controller', 'globe', 'rig', 'screen'):
        state.pop(key, None)
    gc.collect()


def cleanup(error=None):
    if error:
        report.setdefault('error', error)
        unreal.log_error('BRIGHT_GLOBE_FAIL ' + error)
    state['cleanup_started'] = time.monotonic()
    release_world()
    advance('end_pie', .2)


def finish():
    global handle
    try:
        assert not level.is_in_play_in_editor(), 'PIE 종료 전 callback 해제 금지'
        report['hashes_after'] = hashes()
        report['assets_unchanged'] = report.get('hashes_before') == report['hashes_after']
        assert report['assets_unchanged'], '검사 도중 에셋 바이트 변경'
        report['pie_ended'] = True
    except Exception:
        error = traceback.format_exc()
        report['cleanup_errors'].append(error)
        report.setdefault('error', error)
        unreal.log_error('BRIGHT_GLOBE_FAIL ' + error)
    failed = 'error' in report or bool(report['failures'])
    report['status'] = 'failed' if failed else ('measured' if BEFORE else 'passed')
    if report['failures']:
        unreal.log_error('BRIGHT_GLOBE_FAIL 국가 중심 투영 검사 실패: ' + str(len(report['failures'])))
    report['elapsed_seconds'] = time.monotonic() - state['started']
    report['finished_at'] = datetime.now(timezone.utc).isoformat()
    write_report()
    unreal.unregister_slate_post_tick_callback(handle)
    handle = None
    release_world()
    state.clear()
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)


def tick(delta):
    try:
        now = time.monotonic()
        if now < state['next']:
            return
        stage = state['stage']
        state['next'] = now + .2
        if stage not in ('end_pie', 'wait_end', 'final'):
            assert now - state['started'] < 300., '국가 포커스 검사 300초 초과'
        if stage == 'preflight':
            assert not level.is_in_play_in_editor(), '기존 PIE가 실행 중'
            report['hashes_before'] = hashes()
            assert level.load_level('/Game/FestaFiesta/Maps/Frontend')
            catalog = unreal.load_asset('/Game/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas')
            available = [str(item.get_editor_property('country_id'))
                         for item in catalog.get_editor_property('countries')]
            assert len(available) == len(set(available)) and set(available) == set(COUNTRIES), '8개국 카탈로그 변경 확인 필요'
            report['catalog_country_ids'] = available
            report['catalog_longitudes'] = {
                str(item.get_editor_property('country_id')):
                float(item.get_editor_property('location').get_editor_property('longitude'))
                for item in catalog.get_editor_property('countries')
            }
            report['script_sha256_at_preflight'] = digest(ROOT / 'Tools/ui/blender/bright_globe/verify_country_focus.py')
            advance('prepare_window')
        elif stage == 'prepare_window':
            assert not level.is_in_play_in_editor()
            unreal.SystemLibrary.execute_console_command(level.get_world(), 'r.SetRes 4096x2160w')
            advance('begin', 2.)
        elif stage == 'begin':
            _, width, height, _ = CASES[state['case_index']]
            # P11 실측 보정: 실제 viewport가 아래 requested와 같아야 검사한다.
            unreal.SystemLibrary.execute_console_command(level.get_world(), f'FF.FrontendBeginPIE {width-2} {height-12}')
            advance('wait_pie', 1.)
        elif stage == 'wait_pie':
            if not level.is_in_play_in_editor():
                return
            world = unreal.EditorLevelLibrary.get_game_world()
            if not world:
                return
            instance = unreal.GameplayStatics.get_game_instance(world)
            flows = [obj for obj in unreal.ObjectIterator(unreal.FF_UIFlowSubsystem) if obj.get_outer() == instance]
            if len(flows) != 1 or flows[0].get_frontend_screen() != unreal.FF_UIFlowScreen.TITLE:
                return
            globes = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.FF_AtlasGlobeActor)
            rigs = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.FF_AtlasCameraRig)
            assert len(globes) == len(rigs) == 1
            state.update(world=world, flow=flows[0], globe=globes[0], rig=rigs[0],
                         controller=unreal.GameplayStatics.get_player_controller(world, 0))
            globes[0].set_auto_spin_paused(True)
            unreal.AutomationLibrary.finish_loading_before_screenshot()
            command('DisableAllScreenMessages')
            advance('title', 2.)
        elif stage == 'title':
            title = active_screen(unreal.FF_TitleScreen)
            broadcast_button(find_widget(title, 'Btn_Play'))
            advance('hub', 1.)
        elif stage == 'hub':
            assert state['flow'].get_frontend_screen() == unreal.FF_UIFlowScreen.PLAY_HUB
            broadcast_button(find_widget(active_screen(unreal.FF_PlayHubScreen), 'Button_NewFestival'))
            advance('atlas', 2.)
        elif stage == 'atlas':
            assert state['flow'].get_frontend_screen() == unreal.FF_UIFlowScreen.ATLAS_GLOBE
            state['screen'] = active_screen(unreal.FF_AtlasGlobeScreen)
            state['rig'].skip_camera_move()
            state['globe'].set_auto_spin_paused(True)
            state['country_index'] = 0
            advance('select_country', 1.)
        elif stage == 'select_country':
            country = CASES[state['case_index']][3][state['country_index']]
            state['screen'].inspect_country(country)
            state['selection_started'] = now
            advance('settle', SETTLE_SECONDS)
        elif stage == 'settle':
            assert now - state['selection_started'] < 20., '국가 회전/카메라 이동 완료 제한 시간 초과'
            if state['globe'].is_transitioning() or state['rig'].is_moving():
                return
            country = CASES[state['case_index']][3][state['country_index']]
            if country in SCREENSHOT_COUNTRIES:
                state['warm_count'] = 0
                advance('warm', .1)
            else:
                advance('measure', .1)
        elif stage == 'warm':
            name, _, _, countries = CASES[state['case_index']]
            country = countries[state['country_index']]
            state['warm_count'] += 1
            warm = OUT / f'{name}_{country}_warm{state["warm_count"]}.png'
            assert not warm.exists(), '이전 warm 캡처를 덮어쓰지 않음'
            command('Shot SHOWUI filename="' + warm.as_posix() + '" -nosuffix')
            advance('warm_wait', 2.)
        elif stage == 'warm_wait':
            advance('warm' if state['warm_count'] < 2 else 'measure', .1)
        elif stage == 'measure':
            measure()
        elif stage == 'wait_image':
            image = state['image']
            if not image.exists():
                return
            header = image.read_bytes()[:24]
            assert header[:8] == b'\x89PNG\r\n\x1a\n', 'SHOWUI PNG 헤더 확인 실패'
            row = report['cases'][-1]
            row['png_size_including_window_frame'] = list(struct.unpack('>II', header[16:24]))
            row['png_bytes'] = image.stat().st_size
            row['png_sha256'] = digest(image)
            advance('next_country', .1)
        elif stage == 'next_country':
            state['country_index'] += 1
            if state['country_index'] < len(CASES[state['case_index']][3]):
                advance('select_country', .1)
            elif not BEFORE and state['case_index'] == 0:
                advance('lifecycle_start', .1)
            else:
                cleanup()
        elif stage == 'lifecycle_start':
            assert hasattr(state['globe'], 'clear_focus_viewport_position'), '수정 후 DLL의 포커스 취소 API 없음'
            state['screen'].inspect_country('JP')
            state['selection_started'] = now
            advance('lifecycle_japan', SETTLE_SECONDS)
        elif stage == 'lifecycle_japan':
            if not settled():
                return
            lifecycle_checked('일본 재선택 API 도착', **lifecycle_country('JP'))
            expected = adjacent_country('JP', -1)
            assert expected == 'KR', f'카탈로그 변경: JP 서쪽 기대국은 현재 {expected}'
            state['globe'].step_focus(-1)
            state['selection_started'] = now
            advance('lifecycle_step_west', SETTLE_SECONDS)
        elif stage == 'lifecycle_step_west':
            if not settled():
                return
            lifecycle_checked('StepFocus(-1): 일본에서 대한민국', **lifecycle_country('KR'))
            first = adjacent_country('KR', 1)
            second = adjacent_country(first, 1)
            assert first != second and first != 'KR' and second != 'KR'
            state['globe'].step_focus(1)
            pending = bool(state['globe'].is_transitioning())
            state['globe'].step_focus(1)
            state.update(rapid_first=first, rapid_expected=second, rapid_pending=pending,
                         selection_started=now)
            advance('lifecycle_rapid_steps', SETTLE_SECONDS)
        elif stage == 'lifecycle_rapid_steps':
            if not settled():
                return
            lifecycle_checked('같은 프레임 StepFocus(+1) 두 번: 다음 목표로 진행',
                              first_expected=state['rapid_first'], first_transition_pending=state['rapid_pending'],
                              transition_pending_path_covered=state['rapid_pending'],
                              **lifecycle_country(state['rapid_expected']))
            before_drag = orientation(state['globe'])
            state['globe'].add_drag_input(unreal.Vector2D(15., 0.))
            assert str(state['globe'].get_selected_country_id()) == 'None', '드래그 후 선택 미해제'
            assert str(state['globe'].get_focused_country_id()) == 'None', '드래그 후 포커스 미해제'
            assert not state['globe'].is_transitioning(), '드래그 후 전환 미취소'
            state['drag_orientation'] = orientation(state['globe'])
            assert orientation_delta(before_drag, state['drag_orientation']) > .01, '드래그 API가 지구를 돌리지 않음'
            lifecycle_checked('AddDragInput API 회전·선택/전환 해제', before=before_drag,
                              after=state['drag_orientation'], physical_input_tested=False)
            advance('lifecycle_drag_hold', SETTLE_SECONDS)
        elif stage == 'lifecycle_drag_hold':
            lifecycle_checked('드래그 후 자동 재정렬 없음', **assert_idle_rotation('drag_orientation'))
            # UI InspectCountry로 선택을 만들지 않고 순수 액터 API의 진행 중 전환을 취소한다.
            # 활성 UI가 다음 tick에 화면 기준점을 다시 전달해도 선택 None이므로 재스냅하면 안 된다.
            state['globe'].focus_on_country('DE')
            pending = bool(state['globe'].is_transitioning())
            state['globe'].clear_focus_viewport_position()
            assert not state['globe'].is_transitioning(), 'ClearFocusViewportPosition 전환 취소 실패'
            state['clear_orientation'] = orientation(state['globe'])
            lifecycle_checked('ClearFocusViewportPosition 순수 API 전환 취소',
                              transition_was_pending=pending, pending_cancel_path_covered=pending,
                              selected_country=str(state['globe'].get_selected_country_id()),
                              orientation=state['clear_orientation'])
            advance('lifecycle_clear_hold', SETTLE_SECONDS)
        elif stage == 'lifecycle_clear_hold':
            lifecycle_checked('포커스 기준 취소 후 방향 유지', **assert_idle_rotation('clear_orientation'))
            state['screen'].inspect_country('KR')
            state['selection_started'] = now
            advance('lifecycle_reinspect', SETTLE_SECONDS)
        elif stage == 'lifecycle_reinspect':
            if not settled():
                return
            lifecycle_checked('취소 이후 InspectCountry로 포커스 재활성', **lifecycle_country('KR'))
            state['screen'].inspect_country('BR')
            pending = bool(state['globe'].is_transitioning())
            assert state['flow'].go_back(), 'Atlas에서 플레이 허브 뒤로 요청 실패'
            state['globe'].set_auto_spin_paused(True)
            state['rig'].skip_camera_move()
            assert state['flow'].get_frontend_screen() == unreal.FF_UIFlowScreen.PLAY_HUB
            assert not state['globe'].is_transitioning(), 'Atlas 이탈 뒤 전환 미취소'
            assert str(state['globe'].get_selected_country_id()) == 'None', 'Atlas 이탈 뒤 선택 미해제'
            state['hub_orientation'] = orientation(state['globe'])
            lifecycle_checked('국가 회전 도중 Atlas→플레이 허브 이탈',
                              transition_was_pending=pending, pending_exit_path_covered=pending,
                              orientation=state['hub_orientation'])
            advance('lifecycle_hub_hold', SETTLE_SECONDS)
        elif stage == 'lifecycle_hub_hold':
            lifecycle_checked('플레이 허브에서 국가 방향 강제 복귀 없음', **assert_idle_rotation('hub_orientation'))
            assert state['flow'].go_back(), '플레이 허브에서 타이틀 뒤로 요청 실패'
            state['globe'].set_auto_spin_paused(True)
            state['rig'].skip_camera_move()
            assert state['flow'].get_frontend_screen() == unreal.FF_UIFlowScreen.TITLE
            state['title_orientation'] = orientation(state['globe'])
            advance('lifecycle_title_hold', SETTLE_SECONDS)
        elif stage == 'lifecycle_title_hold':
            # CommonUI 스택 전환은 비동기다. GoBack 호출 프레임에서 활성 타이틀을 찾지 않는다.
            active_screen(unreal.FF_TitleScreen)
            lifecycle_checked('타이틀 복귀 뒤 국가 방향 강제 복귀 없음', **assert_idle_rotation('title_orientation'))
            report['lifecycle_api_complete'] = True
            cleanup()
        elif stage == 'end_pie':
            level.editor_request_end_play()
            advance('wait_end', 2.)
        elif stage == 'wait_end':
            if level.is_in_play_in_editor():
                assert now - state['cleanup_started'] < 20., 'PIE 종료 시간 초과'
                return
            if 'error' in report:
                advance('final', 1.)
            else:
                state['case_index'] += 1
                advance('prepare_window' if state['case_index'] < len(CASES) else 'final', 1.)
        elif stage == 'final':
            assert len(report['cases']) == 10 or 'error' in report, '8개국 + KR 2개 화면비 결과 누락'
            assert BEFORE or report.get('lifecycle_api_complete') or 'error' in report, '수정 후 생명주기 API 검사 누락'
            finish()
    except Exception:
        error = traceback.format_exc()
        if state.get('stage') in ('end_pie', 'wait_end', 'final'):
            report.setdefault('error', error)
            report['cleanup_errors'].append(error)
            unreal.log_error('BRIGHT_GLOBE_FAIL ' + error)
            finish()
        else:
            cleanup(error)


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle = unreal.register_slate_post_tick_callback(tick)
