"""나라별 보기 마커·PIE 접근·잔잔한 햇살의 통합 검사.

일회용 에디터에서만 실행한다. 실 카탈로그 대신 /Engine/Transient 복사본을
공유하며 지도·카탈로그·머터리얼은 저장하지 않는다. 물리 입력 검사가 아니다.
Saved/BrightGlobe/custom_views_request.json의 output으로 결과 폴더를 지정한다.
"""
from datetime import datetime, timezone
from pathlib import Path
import gc
import hashlib
import json
import math
import time
import traceback

import unreal


ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
BASE = ROOT / 'Saved/BrightGlobe'
REQUEST = BASE / 'custom_views_request.json'
config = json.loads(REQUEST.read_text(encoding='utf-8-sig')) if REQUEST.exists() else {}
OUT = (BASE / config.get('output', 'custom_views_v001')).resolve()
assert OUT != BASE.resolve() and BASE.resolve() in OUT.parents
assert not (OUT / 'report.json').exists(), '새 output 이름을 사용하세요.'
OUT.mkdir(parents=True, exist_ok=True)
REPORT = OUT / 'report.json'
CATALOG = '/Game/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas'
CASES = (('standard_16x9', 1600, 900), ('small_4x3', 960, 720), ('ultrawide_21x9', 1680, 720))
# 프랍 좌표와 확실히 다른 점을 사용해 별도 구도 경로를 검사한다.
VIEWS = {'KR': (35.2, 129.0, .18), 'DE': (48.0, 8.0, .025), 'BR': (-12.0, -45.0, .28)}
state = {'started': time.monotonic(), 'next': time.monotonic() + 1., 'stage': 'preflight'}
report = dict(status='running', started_at=datetime.now(timezone.utc).isoformat(),
              scope='transient catalog의 나라별 보기 지점·접근과 에디터 마커·햇살 접근성',
              asset_save_requested=False, physical_input_tested=False,
              country_area_click_tested=False, checks=[], cases=[], cleanup_errors=[])
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
handle = None


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def hashes():
    paths = [ROOT / rel for rel in (
        'Content/FestaFiesta/Maps/Frontend.umap',
        'Content/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas.uasset',
        'Content/FestaFiesta/UI/Title/GlassOrbit/WBP_TitleMain_MyCasual.uasset',
        'Content/FestaFiesta/UI/Atlas/WBP_FF_AtlasGlobe.uasset')]
    paths += sorted((ROOT / 'Content/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1/P10').glob('*.uasset'))
    return {path.relative_to(ROOT).as_posix(): digest(path) for path in paths}


def write():
    report['last_stage'] = state['stage']
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')


def checked(name, **evidence):
    report['checks'].append(dict(name=name, passed=True, **evidence))
    write()


def vec(value):
    return [float(value.x), float(value.y), float(value.z)]


def distance(a, b):
    return math.dist(vec(a), vec(b))


def ll(value):
    return [float(value.latitude), float(value.longitude)]


def point(globe, location, radius=None):
    lat, lon = map(math.radians, ll(location))
    radius = radius or float(globe.get_editor_property('globe_radius'))
    local = unreal.Vector(radius * math.cos(lat) * math.cos(lon),
                          -radius * math.cos(lat) * math.sin(lon), radius * math.sin(lat))
    return globe.get_editor_property('globe_root').get_world_transform().transform_location(local)


def orientation(globe):
    value = globe.get_orientation()
    return [float(value.spin), float(value.tilt)]


def angular_error(a, b):
    return max(abs((x - y + 180.) % 360. - 180.) for x, y in zip(a, b))


def wait_until(predicate, timeout=20.):
    deadline = time.monotonic() + timeout
    while not predicate():
        assert time.monotonic() < deadline, '대기 시간 초과: ' + state['stage']
        yield .1


def screen(cls):
    found = [obj for obj in unreal.ObjectIterator(cls)
             if obj.get_owning_player() == state['controller'] and obj.is_activated()]
    assert len(found) == 1, f'{cls.__name__} 활성 인스턴스 수 {len(found)}'
    return found[0]


def click(widget, name):
    tree = unreal.find_object(widget, 'WidgetTree')
    if not tree:
        trees = {obj.get_outer().get_path_name(): obj.get_outer()
                 for obj in unreal.ObjectIterator(unreal.Widget)
                 if obj.get_outer() and obj.get_outer().get_class().get_name() == 'WidgetTree'
                 and obj.get_outer().get_outer() == widget}
        assert len(trees) == 1
        tree = next(iter(trees.values()))
    button = unreal.find_object(tree, name, unreal.Widget)
    assert isinstance(button, unreal.FF_CommonButtonBase) and button.get_editor_property('is_enabled')
    button.get_editor_property('on_ff_button_clicked').broadcast(button)


def command(text):
    unreal.SystemLibrary.execute_console_command(state.get('world') or level.get_world(), text)


def settled():
    return (not state['globe'].is_transitioning() and not state['rig'].is_moving()
            and not state['rig'].is_country_inspection_moving())


def camera_location():
    return state['rig'].get_editor_property('globe_camera').get_actor_location()


def project(location):
    result = state['controller'].project_world_location_to_screen(location, False)
    if isinstance(result, unreal.Vector2D):
        return [float(result.x), float(result.y)]
    if isinstance(result, (list, tuple)) and not any(isinstance(v, bool) and not v for v in result):
        vectors = [v for v in result if isinstance(v, unreal.Vector2D)]
        if len(vectors) == 1:
            return [float(vectors[0].x), float(vectors[0].y)]
    raise AssertionError('보기 지점 화면 투영 실패')


def measure(country, case, width, height):
    globe = state['globe']
    assert str(globe.get_selected_country_id()) == country
    assert str(globe.get_focused_country_id()) == country
    assert list(state['controller'].get_viewport_size()) == [width, height]
    location = globe.get_country_inspection_location(country)
    assert max(abs(a - b) for a, b in zip(ll(location), VIEWS[country][:2])) < .001
    pixels = project(point(globe, location))
    target = globe.get_focus_viewport_position()
    uv = [pixels[0] / width, pixels[1] / height]
    error = max(abs(uv[0] - target.x), abs(uv[1] - target.y))
    assert error < .005, f'{case}/{country} 중심 오차: {error}'
    start = state['base_camera']
    center = globe.get_actor_location()
    base_distance = distance(start, center)
    radius = float(globe.get_editor_property('globe_radius'))
    expected_approach = min(base_distance * VIEWS[country][2], max(0., base_distance - radius * 1.35))
    observed_approach = distance(start, camera_location())
    assert abs(observed_approach - expected_approach) < .1, f'나라별 접근 거리 오류 {observed_approach}/{expected_approach}'
    original = state['original_locations'][country]
    pin = next(row for row in globe.get_pin_states() if str(row.country_id) == country)
    assert distance(pin.world_location, point(globe, original)) < .1, '보기 지점이 프랍 위치를 변경함'
    result = dict(case=case, country=country, viewport=[width, height],
                  inspection_location=ll(location), original_prop_location=ll(original),
                  approach_fraction=VIEWS[country][2], expected_approach_uu=expected_approach,
                  observed_approach_uu=observed_approach, projected_uv=uv,
                  target_uv=[target.x, target.y], max_axis_error_uv=error,
                  original_prop_preserved=True)
    report['cases'].append(result)
    write()
    return result


def capture(name):
    path = OUT / (name + '.png')
    assert not path.exists()
    command('Shot SHOWUI filename="' + path.as_posix() + '" -nosuffix')
    yield from wait_until(path.exists)
    yield .25
    assert path.stat().st_size > 1000
    report.setdefault('images', []).append(dict(path=path.relative_to(ROOT).as_posix(), sha256=digest(path)))
    write()


def marker_checks():
    globe = next(actor for actor in actors.get_all_level_actors() if isinstance(actor, unreal.FF_AtlasGlobeActor))
    root = globe.get_editor_property('globe_root')
    original_transform = root.get_world_transform()
    marker = actors.spawn_actor_from_class(unreal.FF_AtlasCountryViewMarker, unreal.Vector(), transient=True)
    try:
        marker.set_editor_property('globe', globe)
        marker.set_editor_property('catalog', state['catalog'])
        marker.set_editor_property('country_id', 'KR')
        # 변환이 없는 경우만 성공하는 잘못된 좌표 저장을 검출한다.
        # NativeMakeFunc는 Python에서 MathLibrary 함수가 아니라 Struct 생성자로 노출된다.
        transform = unreal.Transform(location=unreal.Vector(1200., -380., 210.),
                    rotation=unreal.Rotator(13., 71., -8.), scale=unreal.Vector(1.2, .8, 1.1))
        root.set_world_transform(transform, False, True)
        latitude, longitude, fraction = VIEWS['KR']
        location = unreal.FF_AtlasLatLong(latitude=latitude, longitude=longitude)
        marker.set_actor_location(point(globe, location, 5100.), False, False)
        marker.set_editor_property('approach_fraction', fraction)
        assert max(abs(a - b) for a, b in zip(ll(marker.get_marker_location()), ll(location))) < .001
        original_row = next(row for row in state['catalog'].countries if str(row.country_id) == 'KR').copy()
        assert marker.apply_country_view(), '에디터 마커 메모리 반영 실패'
        row = next(row for row in state['catalog'].countries if str(row.country_id) == 'KR')
        assert row.override_inspection_view and abs(row.inspection_approach_fraction - fraction) < .0001
        assert ll(row.location) == ll(original_row.location), '마커가 프랍 위치까지 변경함'
        marker.set_actor_location(unreal.Vector(1., 2., 3.), False, False)
        marker.load_country_view()
        assert max(abs(a - b) for a, b in zip(ll(marker.get_marker_location()), ll(location))) < .001
        assert abs(marker.get_editor_property('approach_fraction') - fraction) < .0001
        checked('에디터 마커 비균일 스케일·회전 왕복과 메모리 반영',
                location=ll(marker.get_marker_location()), transform=transform.export_text(),
                catalog=state['catalog'].get_path_name(), disk_save_called=False)
    finally:
        actors.destroy_actor(marker)
        root.set_world_transform(original_transform, False, True)


def run():
    assert not level.is_in_play_in_editor(), '기존 PIE가 실행 중'
    report['hashes_before'] = hashes()
    state['user_settings'] = {path: path.read_bytes() for path in (ROOT / 'Saved/Config').glob('*/GameUserSettings.ini')}
    assert level.load_level('/Game/FestaFiesta/Maps/Frontend')
    original = unreal.load_asset(CATALOG)
    state['catalog'] = unreal.new_object(unreal.FF_FestivalAtlasCatalog, name='CustomViewVerificationTransient')
    state['original_locations'] = {str(row.country_id): row.location.copy() for row in original.countries}
    state['catalog'].set_editor_property('countries', [row.copy() for row in original.countries])
    state['catalog'].set_editor_property('entries', [row.copy() for row in original.entries])
    rows = [row.copy() for row in state['catalog'].countries]
    for row in rows:
        code = str(row.country_id)
        if code in VIEWS:
            lat, lon, fraction = VIEWS[code]
            row.set_editor_property('override_inspection_view', True)
            row.set_editor_property('inspection_location', unreal.FF_AtlasLatLong(latitude=lat, longitude=lon))
            row.set_editor_property('inspection_approach_fraction', fraction)
    state['catalog'].set_editor_property('countries', rows)
    marker_checks()
    state['settings'] = unreal.get_default_object(unreal.FF_UIFlowSettings)
    state['original_catalog_setting'] = state['settings'].get_editor_property('atlas_catalog')
    state['settings'].set_editor_property('atlas_catalog', state['catalog'])
    for case, width, height in CASES:
        state['stage'] = case + ':begin'
        command('r.SetRes 4096x2160w')
        yield 2.
        command(f'FF.FrontendBeginPIE {width-2} {height-12}')
        yield from wait_until(level.is_in_play_in_editor)
        yield 2.
        world = unreal.EditorLevelLibrary.get_game_world()
        instance = unreal.GameplayStatics.get_game_instance(world)
        flow = next(obj for obj in unreal.ObjectIterator(unreal.FF_UIFlowSubsystem) if obj.get_outer() == instance)
        globe = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.FF_AtlasGlobeActor)[0]
        rig = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.FF_AtlasCameraRig)[0]
        access = next(obj for obj in unreal.ObjectIterator(unreal.FF_UIAccessibilitySubsystem) if obj.get_outer() == instance)
        state.update(world=world, flow=flow, globe=globe, rig=rig, access=access,
                     controller=unreal.GameplayStatics.get_player_controller(world, 0),
                     original_reduce_motion=access.is_reduce_motion_enabled())
        globe.set_editor_property('catalog_override', state['catalog'])
        globe.rebuild_pins()
        globe.set_auto_spin_paused(True)
        access.set_reduce_motion_enabled(False)
        command('DisableAllScreenMessages')
        yield 2.
        state['title_camera_location'] = rig.get_editor_property('title_camera').get_actor_location()
        if case == CASES[0][0]:
            backdrop = next(actor for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
                            if actor.get_actor_label() == 'FF_Clay_Backdrop')
            sunlight = backdrop.get_component_by_class(unreal.StaticMeshComponent).get_material(0)
            assert isinstance(sunlight, unreal.MaterialInstanceDynamic), '햇살 런타임 MID 없음'
            assert abs(sunlight.get_scalar_parameter_value('SunMotionEnabled') - 1.) < .001
            yield from capture('title_sun_t0')
            yield 7.
            yield from capture('title_sun_t7')
            state['sunlight'] = sunlight
        click(screen(unreal.FF_TitleScreen), 'Btn_Play')
        yield 1.
        click(screen(unreal.FF_PlayHubScreen), 'Button_NewFestival')
        yield 2.
        rig.skip_camera_move()
        state['screen'] = screen(unreal.FF_AtlasGlobeScreen)
        state['base_camera'] = camera_location()
        for country in VIEWS:
            state['stage'] = case + ':' + country
            before = camera_location()
            state['screen'].inspect_country(country)
            assert distance(before, camera_location()) < .1, '선택 프레임에서 카메라가 순간 이동함'
            yield 2.
            yield from wait_until(settled)
            measure(country, case, width, height)
            if country == 'KR':
                yield from capture(case + '_KR_custom')
        if case == CASES[0][0]:
            state['stage'] = 'rapid_switch'
            state['screen'].inspect_country('KR')
            yield .15
            before = camera_location()
            state['screen'].inspect_country('DE')
            assert distance(before, camera_location()) < .1, '진행 중 국가 전환에서 카메라 점프'
            state['screen'].inspect_country('BR')
            assert distance(before, camera_location()) < .1
            yield 2.
            yield from wait_until(settled)
            result = measure('BR', 'rapid_switch', width, height)
            checked('연속 국가 선택은 현재 카메라 위치에서 이어짐', **result)
            state['stage'] = 'drag_cancel'
            state['screen'].inspect_country('DE')
            yield .15
            pending = rig.is_country_inspection_moving()
            assert pending, '드래그 시험 전에 접근이 진행 중이어야 함'
            globe.add_drag_input(unreal.Vector2D(12., 0.))
            frozen_camera, frozen_orientation = camera_location(), orientation(globe)
            assert str(globe.get_selected_country_id()) == 'None' and not globe.is_transitioning()
            yield 1.5
            assert not rig.is_country_inspection_moving()
            assert distance(camera_location(), frozen_camera) < .1, '드래그 이후 카메라가 계속 이동함'
            assert angular_error(orientation(globe), frozen_orientation) < .001
            checked('드래그 API가 지구 회전과 카메라 접근을 함께 취소', pending_before_drag=pending)
            state['stage'] = 'reduce_motion'
            access.set_reduce_motion_enabled(True)
            yield .1
            assert abs(state['sunlight'].get_scalar_parameter_value('SunMotionEnabled')) < .001
            state['screen'].inspect_country('KR')
            assert not globe.is_transitioning(), '움직임 감소에서 지구 회전이 지연됨'
            yield .1
            assert not rig.is_country_inspection_moving(), '움직임 감소에서 접근이 지연됨'
            # 실제 카메라 POV 갱신 후 투영을 읽는다.
            yield .2
            measure('KR', 'reduce_motion', width, height)
            access.set_reduce_motion_enabled(False)
            yield .1
            assert abs(state['sunlight'].get_scalar_parameter_value('SunMotionEnabled') - 1.) < .001
            checked('움직임 감소 즉시 도착·햇살 정지와 재개', sunlight_gate=[1., 0., 1.])
        state['stage'] = case + ':return_title'
        assert flow.go_back()
        yield 1.
        rig.skip_camera_move()
        assert distance(camera_location(), state['base_camera']) < .1, '나라 선택 이탈 후 카메라 원위치 실패'
        assert flow.go_back()
        yield 2.
        rig.skip_camera_move()
        screen(unreal.FF_TitleScreen)
        assert distance(rig.get_editor_property('title_camera').get_actor_location(), state['title_camera_location']) < .1
        checked(case + ' 타이틀 복귀 시 접근 초기화')
        access.set_reduce_motion_enabled(state['original_reduce_motion'])
        state.pop('access', None)
        for key in ('world', 'flow', 'globe', 'rig', 'controller', 'screen', 'sunlight'):
            state.pop(key, None)
        world = instance = flow = globe = rig = access = None
        gc.collect()
        level.editor_request_end_play()
        yield from wait_until(lambda: not level.is_in_play_in_editor())
        yield 1.
    report['completed_all_cases'] = True


def teardown():
    state['stage'] = 'cleanup'
    if state.get('access'):
        state['access'].set_reduce_motion_enabled(state['original_reduce_motion'])
    if state.get('settings'):
        state['settings'].set_editor_property('atlas_catalog', state['original_catalog_setting'])
    for key in ('world', 'flow', 'globe', 'rig', 'controller', 'screen', 'sunlight', 'access', 'catalog'):
        state.pop(key, None)
    gc.collect()
    if level.is_in_play_in_editor():
        level.editor_request_end_play()
        yield from wait_until(lambda: not level.is_in_play_in_editor())
    for path, contents in state.get('user_settings', {}).items():
        path.write_bytes(contents)
    report['hashes_after'] = hashes()
    report['assets_unchanged'] = report.get('hashes_before') == report['hashes_after']
    assert report['assets_unchanged'], '검사 중 에셋 바이트 변경'
    report['pie_ended'] = not level.is_in_play_in_editor()


def lifecycle():
    try:
        yield from run()
    except Exception:
        report['error'] = traceback.format_exc()
        unreal.log_error('BRIGHT_GLOBE_FAIL ' + report['error'])
    finally:
        try:
            yield from teardown()
        except Exception:
            report['cleanup_errors'].append(traceback.format_exc())
            unreal.log_error('BRIGHT_GLOBE_FAIL ' + report['cleanup_errors'][-1])
    report['status'] = 'passed' if report.get('completed_all_cases') and 'error' not in report and not report['cleanup_errors'] else 'failed'
    report['elapsed_seconds'] = time.monotonic() - state['started']
    report['finished_at'] = datetime.now(timezone.utc).isoformat()
    write()


iterator = lifecycle()


def tick(delta):
    global handle
    if time.monotonic() < state['next']:
        return
    try:
        delay = next(iterator)
        state['next'] = time.monotonic() + max(0., float(delay))
        write()
    except StopIteration:
        unreal.unregister_slate_post_tick_callback(handle)
        handle = None
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle = unreal.register_slate_post_tick_callback(tick)
