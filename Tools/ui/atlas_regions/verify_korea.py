"""저장된 한국 영역 에셋을 실제 Frontend PIE에서 검증한다.

일회용 에디터에서 실행하며 에셋은 저장하지 않는다. 실행 전에
Saved/AtlasRegions/verify_korea_request.json에 {"output":"검사별_새_폴더"}를 둔다.
include_ultrawide=true이면 필수 1600x900·960x720 외에 1680x720도 캡처한다.
버튼 델리게이트·공개 선택/호버/광선 API 검사이며 물리 마우스 입력 검사가 아니다.
선택 가능·호버·선택·선택 중 호버를 같은 카메라에서 캡처하고 움직임 감소도 확인한다.
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
BASE = ROOT / 'Saved/AtlasRegions'
REQUEST = BASE / 'verify_korea_request.json'
config = json.loads(REQUEST.read_text(encoding='utf-8-sig')) if REQUEST.exists() else {}
OUT = (BASE / config.get('output', datetime.now().strftime('verify_%Y%m%d_%H%M%S_%f'))).resolve()
assert OUT != BASE.resolve() and BASE.resolve() in OUT.parents, '결과는 Saved/AtlasRegions 하위에 둔다.'
assert not OUT.exists(), '결과 덮어쓰기를 막기 위해 새로운 output 이름을 사용한다.'
OUT.mkdir(parents=True)
REPORT = OUT / 'report.json'
FRONTEND = '/Game/FestaFiesta/Maps/Frontend'
CATALOG = '/Game/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas'
REGIONS = '/Game/FestaFiesta/UI/Atlas/CountryRegions/DA_AtlasCountryRegions'
CASES = [('standard_16x9', 1600, 900), ('small_4x3', 960, 720)]
if config.get('include_ultrawide', False):
    CASES.append(('ultrawide_21x9', 1680, 720))
SAMPLES = [('Seoul', 37.5665, 126.9780, 'KR'),
           ('Busan', 35.1796, 129.0756, 'KR'),
           ('Jeju_inland', 33.36, 126.53, 'KR'),
           ('Pyongyang', 39.0392, 125.7625, 'None'),
           ('Tsushima', 34.38, 129.29, 'None'),
           ('EastSea_offshore', 35.8, 130.5, 'None')]
state = dict(started=time.monotonic(), next=time.monotonic() + 1., stage='preflight')
report = dict(status='running', started_at=datetime.now(timezone.utc).isoformat(),
              scope='한국 영역의 PIE 상태별 반사광·접근성·API 광선·기존 카메라/프랍 회귀',
              asset_save_requested=False, physical_input_tested=False,
              physical_press_release_drag_tested=False,
              physical_hover_cursor_tooltip_tested=False,
              input_method='버튼 delegate broadcast + InspectCountry/SetHoveredCountry/PickCountry 공개 API',
              hover_capture_method='Slate post-tick에서 SetHoveredCountry 상태 유지; 실제 커서 호버/툴팁 입력 검사는 아님',
              feedback_capture_note='카메라 비교를 위해 KR 상세 패널은 유지하고 지구 재질의 호버/선택 상태만 API로 바꾼다.',
              visual_readability='캡처를 별도로 육안 확인해야 한다. 자동 판정은 크기·상태·투영까지만 수행한다.',
              checks=[], ray_samples=[], cases=[], images=[], cleanup_errors=[])
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
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
    # 마스크·선택 재질 외에 원본 육지 메시/재질과 MPC도 보호한다.
    for rel in ('Content/FestaFiesta/UI/Atlas/CountryRegions',
                'Content/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1'):
        paths.extend(sorted((ROOT / rel).rglob('*.uasset')))
    assert paths and all(path.is_file() for path in paths), '보호할 에셋 파일이 없음'
    return {path.relative_to(ROOT).as_posix(): digest(path) for path in paths}


def write():
    report['last_stage'] = state['stage']
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')


def checked(name, **evidence):
    report['checks'].append(dict(name=name, passed=True, **evidence))
    write()


def xyz(value):
    return [float(value.x), float(value.y), float(value.z)]


def vector(values):
    return unreal.Vector(*values)


def add(a, b):
    return [x + y for x, y in zip(a, b)]


def sub(a, b):
    return [x - y for x, y in zip(a, b)]


def mul(a, amount):
    return [x * amount for x in a]


def unit(a):
    length = math.sqrt(sum(x * x for x in a))
    assert length > 1.e-8
    return mul(a, 1. / length)


def distance(a, b):
    return math.dist(xyz(a), xyz(b))


def command(text):
    unreal.SystemLibrary.execute_console_command(state.get('world') or level.get_world(), text)


def wait_until(predicate, timeout=25.):
    deadline = time.monotonic() + timeout
    while not predicate():
        assert time.monotonic() < deadline, '대기 시간 초과: ' + state['stage']
        yield .1


def screen(cls):
    objects = [obj for obj in unreal.ObjectIterator(cls)
               if obj.get_owning_player() == state['controller'] and obj.is_activated()]
    assert len(objects) == 1, f'{cls.__name__} 활성 인스턴스 수: {len(objects)}'
    return objects[0]


def click(widget, name):
    tree = unreal.find_object(widget, 'WidgetTree')
    if not tree:
        trees = {obj.get_outer().get_path_name(): obj.get_outer()
                 for obj in unreal.ObjectIterator(unreal.Widget)
                 if obj.get_outer() and obj.get_outer().get_class().get_name() == 'WidgetTree'
                 and obj.get_outer().get_outer() == widget}
        assert len(trees) == 1, '위젯 트리 탐색 실패'
        tree = next(iter(trees.values()))
    button = unreal.find_object(tree, name, unreal.Widget)
    assert isinstance(button, unreal.FF_CommonButtonBase) and button.get_editor_property('is_enabled')
    button.get_editor_property('on_ff_button_clicked').broadcast(button)


def settled():
    return (not state['globe'].is_transitioning() and not state['rig'].is_moving()
            and not state['rig'].is_country_inspection_moving())


def camera_location():
    return state['rig'].get_editor_property('globe_camera').get_actor_location()


def projected(location):
    value = state['controller'].project_world_location_to_screen(location, False)
    if isinstance(value, unreal.Vector2D):
        return [float(value.x), float(value.y)]
    if isinstance(value, (list, tuple)) and not any(isinstance(x, bool) and not x for x in value):
        vectors = [x for x in value if isinstance(x, unreal.Vector2D)]
        if len(vectors) == 1:
            return [float(vectors[0].x), float(vectors[0].y)]
    raise AssertionError('지표의 화면 투영 실패')


def geographic_point(latitude, longitude):
    globe = state['globe']
    lat, lon = math.radians(latitude), math.radians(longitude)
    radius = float(globe.get_editor_property('globe_radius'))
    local = vector([radius * math.cos(lat) * math.cos(lon),
                    -radius * math.cos(lat) * math.sin(lon), radius * math.sin(lat)])
    return globe.get_editor_property('globe_root').get_world_transform().transform_location(local)


def materials():
    return {(component.get_name(), slot): component.get_material(slot)
            for component in state['globe'].get_editor_property('land_layer_components')
            for slot in range(component.get_num_materials())}


def region_mids():
    originals = state['original_materials']
    target = state['data'].get_editor_property('target_material')
    values = [material for key, material in materials().items() if originals[key] == target]
    assert values and all(isinstance(value, unreal.MaterialInstanceDynamic) for value in values), '영역 MID 배선 누락'
    return values


def material_state(enabled=1., hovered=0., selected=0., motion=1.):
    values = region_mids()
    expected = dict(RegionEnabled=enabled, RegionHovered=hovered,
                    RegionSelected=selected, RegionMotionEnabled=motion)
    actual = {name: [float(value.get_scalar_parameter_value(name)) for value in values]
              for name in expected}
    for name, wanted in expected.items():
        assert all(abs(value - wanted) < .001 for value in actual[name]), f'{name}={actual[name]}, 기대값={wanted}'
    if enabled > 0.:
        region = state['region']
        assert all(value.get_texture_parameter_value('RegionMask') == region.mask for value in values), '한국 마스크 불일치'
        bounds = region.bounds
        for value in values:
            actual = value.get_vector_parameter_value('RegionBounds')
            assert max(abs(getattr(actual, key) - getattr(bounds, key)) for key in ('r', 'g', 'b', 'a')) < .001
    return dict(expected=expected, actual=actual, mid_count=len(values))


def api_hover(country):
    # NativeTick이 실제 커서를 읽어 다시 쓰므로 시안 캡처 동안만 공개 API 상태를 유지한다.
    # 실제 마우스 이벤트가 처리되었다고 보고하지 않는다.
    state['api_hover'] = country
    state['globe'].set_hovered_country(country)


def landmark(country):
    pin = next(row for row in state['globe'].get_pin_states() if str(row.country_id) == country)
    found = [component for component in state['globe'].get_components_by_class(unreal.StaticMeshComponent)
             if component.static_mesh and component.get_attach_parent()
             and distance(component.get_attach_parent().get_world_location(), pin.world_location) < .1]
    assert len(found) == 1, f'{country} 프랍 컴포넌트 탐색 실패: {len(found)}'
    return found[0]


def triangle_at_uv(uv):
    # 실제 LOD0 삼각형 안에서 원하는 UV의 무게중심 좌표를 구한다.
    coords, indices = state['surface_uvs'], state['surface_indices']
    matches = []
    for offset in range(0, len(indices), 3):
        ids = indices[offset:offset + 3]
        a, b, c = [coords[i] for i in ids]
        den = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
        if abs(den) < 1.e-14:
            continue
        p = ((b[1] - c[1]) * (uv[0] - c[0]) + (c[0] - b[0]) * (uv[1] - c[1])) / den
        q = ((c[1] - a[1]) * (uv[0] - c[0]) + (a[0] - c[0]) * (uv[1] - c[1])) / den
        weights = [p, q, 1. - p - q]
        if min(weights) >= -1.e-7:
            matches.append((min(weights), offset // 3, ids, weights))
    if not matches:
        return None
    _, index, ids, weights = max(matches)
    return index, ids, weights


def surface_point(sample):
    name, lat, lon, expected = sample
    uv = [(lon + 180.) / 360., (90. - lat) / 180.]
    match = triangle_at_uv(uv)
    if not match:
        assert expected == 'None', name + ': 실제 육지 삼각형이 없음'
        return geographic_point(lat, lon), dict(uv=uv, triangle=None, source='바다 구면 방향')
    index, ids, bary = match
    globe, data = state['globe'], state['data']
    transform = globe.get_editor_property('globe_root').get_world_transform()
    scale = float(globe.get_editor_property('globe_radius')) / float(data.source_radius)
    mpc = data.shape_parameters
    thickness = unreal.MaterialLibrary.get_scalar_parameter_value(state['world'], mpc, 'LandThickness')
    mountain = unreal.MaterialLibrary.get_scalar_parameter_value(state['world'], mpc, 'MountainHeight')
    world_vertices = []
    center = xyz(globe.get_actor_location())
    for i in ids:
        pos = xyz(transform.transform_location(vector(mul(state['surface_vertices'][i], scale))))
        delta = (thickness - float(data.base_land_thickness)
                 + state['surface_weights'][i] * (mountain - float(data.base_mountain_height)))
        world_vertices.append(add(pos, mul(unit(sub(pos, center)), delta)))
    pos = [sum(bary[j] * world_vertices[j][axis] for j in range(3)) for axis in range(3)]
    return vector(pos), dict(uv=uv, triangle=index, vertex_ids=ids, barycentric=bary,
                             source='실제 LOD0 삼각형 UV 보간 + 현재 MPC WPO',
                             shape_parameters=dict(LandThickness=thickness, MountainHeight=mountain))


def ray_checks(label):
    globe = state['globe']
    center = xyz(globe.get_editor_property('globe_root').get_world_location())
    for sample in SAMPLES:
        point, evidence = surface_point(sample)
        normal = unit(sub(xyz(point), center))
        origin = vector(add(xyz(point), mul(normal, 30000.)))
        direction = vector(mul(normal, -1.))
        region = str(globe.pick_country_region(origin, direction))
        country = str(globe.pick_country(origin, direction))
        assert region == sample[3], f'{label}/{sample[0]} 영역={region}, 기대값={sample[3]}'
        if sample[3] == 'KR':
            assert country == 'KR', sample[0] + ': 통합 국가 선택 경로 불일치'
            backside = str(globe.pick_country_region(vector(sub(center, mul(normal, 30000.))), vector(normal)))
            assert backside == 'None', sample[0] + ': 뒷면 한국 관통 선택'
        else:
            assert country != 'KR', sample[0] + ': 한국 외부가 한국으로 통합 선택됨'
            backside = None
        report['ray_samples'].append(dict(case=label, sample=sample[0], latitude=sample[1], longitude=sample[2],
                                         expected=sample[3], region=region, country=country, backside=backside,
                                         world_point=xyz(point), **evidence))
    assert str(globe.pick_country_region(vector(center), unreal.Vector())) == 'None'
    checked(label + ' 실제 삼각형 영역 광선·외부·후면·영벡터', samples=len(SAMPLES))


def transformed_ray_checks():
    globe = state['globe']
    before = globe.get_actor_transform()
    root = globe.get_editor_property('globe_root')
    root_before = root.get_world_transform()
    mpc = state['data'].shape_parameters
    controls_before = {name: unreal.MaterialLibrary.get_scalar_parameter_value(state['world'], mpc, name)
                       for name in ('LandThickness', 'MountainHeight')}
    try:
        # 액터 이동은 GlobeRoot와 지구 중심을 함께 이동한다. 지구 자전은 별도 자식 회전이다.
        globe.set_actor_transform(unreal.Transform(location=unreal.Vector(1200., -380., 210.),
                                  rotation=unreal.Rotator(13., 71., -8.), scale=unreal.Vector(1.2, .8, 1.1)), False, True)
        ray_checks('translated_rotated_nonuniform_actor_and_globe_root')
        # 루트 중심과 ActorPosition이 다른 경우에도 표시 메시의 세계 단위 WPO와 일치해야 한다.
        root.set_world_transform(unreal.Transform(location=unreal.Vector(1260., -410., 240.),
                                 rotation=unreal.Rotator(-17., 28., 11.), scale=unreal.Vector(.9, 1.3, 1.1)), False, True)
        unreal.MaterialLibrary.set_scalar_parameter_value(state['world'], mpc, 'LandThickness', 80.)
        unreal.MaterialLibrary.set_scalar_parameter_value(state['world'], mpc, 'MountainHeight', 800.)
        ray_checks('offset_globe_root_with_runtime_WPO')
    finally:
        for name, value in controls_before.items():
            unreal.MaterialLibrary.set_scalar_parameter_value(state['world'], mpc, name, value)
        globe.set_actor_transform(before, False, True)
        root.set_world_transform(root_before, False, True)


def camera_check(country, case, width, height):
    globe, data = state['globe'], state['catalog']
    row = next(value for value in data.countries if str(value.country_id) == country)
    expected = row.inspection_location if row.override_inspection_view else row.location
    actual = globe.get_country_inspection_location(country)
    assert max(abs(actual.latitude - expected.latitude), abs(actual.longitude - expected.longitude)) < .001
    assert str(globe.get_selected_country_id()) == country and str(globe.get_focused_country_id()) == country
    pixel = projected(geographic_point(actual.latitude, actual.longitude))
    uv = [pixel[0] / width, pixel[1] / height]
    target = globe.get_focus_viewport_position()
    error = max(abs(uv[0] - target.x), abs(uv[1] - target.y))
    assert error < .005, f'{case}/{country}: 사용자 지정 보기 지점의 화면 중심 오차 {error}'
    settings = unreal.get_default_object(unreal.FF_AtlasSettings)
    fraction = row.inspection_approach_fraction if row.override_inspection_view else settings.country_inspection_approach_fraction
    fraction = max(0., min(float(fraction), .5 if row.override_inspection_view else .2))
    base_distance = distance(state['base_camera'], globe.get_actor_location())
    radius = float(globe.get_editor_property('globe_radius')) * max(abs(x) for x in xyz(globe.get_actor_scale3d()))
    approach = min(base_distance * fraction, max(0., base_distance - radius * 1.35))
    observed = distance(state['base_camera'], camera_location())
    assert abs(approach - observed) < .1, f'저장된 국가별 카메라 접근량 변경 {observed}/{approach}'
    report['cases'].append(dict(case=case, country=country, viewport=[width, height],
                               authored_override=bool(row.override_inspection_view),
                               inspection_latitude=actual.latitude, inspection_longitude=actual.longitude,
                               approach_fraction=fraction, expected_approach_uu=approach, observed_approach_uu=observed,
                               projected_uv=uv, target_uv=[target.x, target.y], max_axis_error_uv=error))
    write()


def capture(name, width, height):
    path = OUT / (name + '.png')
    viewport = list(state['controller'].get_viewport_size())
    assert viewport == [width, height], f'실제 PIE 뷰포트 크기 불일치 {viewport}'
    command('Shot SHOWUI filename="' + path.as_posix() + '" -nosuffix')
    yield from wait_until(lambda: path.exists() and path.stat().st_size > 1000)
    yield .3
    with path.open('rb') as stream:
        header = stream.read(24)
    assert header[:8] == b'\x89PNG\r\n\x1a\n'
    actual = list(struct.unpack('>II', header[16:24]))
    assert list(state['controller'].get_viewport_size()) == viewport, '캡처 중 뷰포트 크기 변경'
    decoration = [actual[0] - viewport[0], actual[1] - viewport[1]]
    # verify_v001: 실제 뷰포트 1600x900, Shot SHOWUI PNG 1606x938.
    # 원본 육안 확인에서 Windows 창 테두리/제목 표시줄로 확인했다. 보정·리사이즈하지 않는다.
    assert decoration in ([0, 0], [6, 38]), f'미확인 캡처 여백: PNG={actual}, 뷰포트={viewport}'
    report['images'].append(dict(path=path.relative_to(ROOT).as_posix(), sha256=digest(path), size=actual,
                                 viewport_size=viewport, png_size=actual, window_decoration_size=decoration,
                                 elapsed_seconds=time.monotonic() - state['started'],
                                 country_feedback=state.get('feedback_state'),
                                 capture_includes_window_frame=decoration != [0, 0], image_modified=False,
                                 decoration_evidence=('verify_v001: controller 1600x900 / PNG 1606x938; '
                                                      'Windows 창 테두리·제목 표시줄을 원본 이미지에서 육안 확인')))
    write()


def feedback_capture(name, width, height, hovered=0., selected=0., motion=1.):
    evidence = material_state(hovered=hovered, selected=selected, motion=motion)
    reference = state['feedback_camera']
    assert distance(camera_location(), reference) < .1, '상태 비교 캡처 도중 카메라 이동'
    assert state['globe'].get_editor_property('globe_root').get_world_transform().export_text() == state['feedback_globe'], '상태 비교 캡처 도중 지구 이동'
    state['feedback_state'] = dict(**evidence['expected'], camera_location=xyz(reference),
                                   input='공개 API로 유지한 상태; 실제 커서 입력 아님')
    try:
        yield from capture(name, width, height)
        material_state(hovered=hovered, selected=selected, motion=motion)
        assert distance(camera_location(), reference) < .1, '상태 비교 캡처 도중 카메라 이동'
    finally:
        state.pop('feedback_state', None)


def feedback_checks(case, width, height):
    globe = state['globe']
    state['feedback_camera'] = camera_location()
    state['feedback_globe'] = globe.get_editor_property('globe_root').get_world_transform().export_text()
    api_hover('None')
    yield .3
    yield from feedback_capture(case + '_KR_selected', width, height, selected=1.)

    # 카메라 접근량은 유지한 채 선택만 해제한다. 사용자가 저장한 검사 위치는 바꾸지 않는다.
    globe.set_selected_country('None', True)
    yield .3
    yield from feedback_capture(case + '_KR_idle_t0', width, height)
    if case == CASES[0][0]:
        yield 1.25
        yield from feedback_capture(case + '_KR_idle_t1', width, height)
        checked('선택 가능 영역의 시간차 반사광 캡처',
                minimum_interval_seconds=1.25,
                visual_motion_requires_inspection=True,
                note='주변 햇살도 움직이므로 이미지 전체 차이를 국가 반사광 검증으로 간주하지 않는다.')

    api_hover('KR')
    yield .3
    yield from feedback_capture(case + '_KR_hovered', width, height, hovered=1.)
    globe.set_selected_country('KR', True)
    yield .3
    yield from feedback_capture(case + '_KR_selected_hovered', width, height, hovered=1., selected=1.)
    api_hover('None')
    material_state(selected=1.)
    checked(case + ' 같은 카메라에서 기본·호버·선택·선택 중 호버 분리',
            states=[dict(hovered=0, selected=0), dict(hovered=1, selected=0),
                    dict(hovered=0, selected=1), dict(hovered=1, selected=1)],
            physical_input_tested=False, mid_count=len(region_mids()))

    if case == CASES[0][0]:
        access = state['accessibility']
        access.set_reduce_motion_enabled(True)
        yield .3  # Globe Tick에서 현재 접근성 값을 MID에 동기화한다.
        yield from feedback_capture(case + '_KR_selected_reduce_motion', width, height, selected=1., motion=0.)
        access.set_reduce_motion_enabled(False)
        yield .3
        material_state(selected=1., motion=1.)
        checked('움직임 감소 반사광 정지·복원과 선택 강조 유지',
                RegionMotionEnabled=[1., 0., 1.], RegionSelected=[1., 1., 1.])
    state.pop('feedback_camera', None)
    state.pop('feedback_globe', None)


def start_case(case, width, height):
    state['stage'] = case + ':begin'
    command('r.SetRes 4096x2160w')
    yield 2.
    command(f'FF.FrontendBeginPIE {width - 2} {height - 12}')
    state['owns_pie'] = True
    yield from wait_until(level.is_in_play_in_editor)
    yield 2.
    state['world'] = unreal.EditorLevelLibrary.get_game_world()
    instance = unreal.GameplayStatics.get_game_instance(state['world'])
    state['flow'] = next(obj for obj in unreal.ObjectIterator(unreal.FF_UIFlowSubsystem) if obj.get_outer() == instance)
    state['accessibility'] = next(obj for obj in unreal.ObjectIterator(unreal.FF_UIAccessibilitySubsystem)
                                  if obj.get_outer() == instance)
    state['original_reduce_motion'] = state['accessibility'].is_reduce_motion_enabled()
    state['accessibility'].set_reduce_motion_enabled(False)
    state['globe'] = unreal.GameplayStatics.get_all_actors_of_class(state['world'], unreal.FF_AtlasGlobeActor)[0]
    state['rig'] = unreal.GameplayStatics.get_all_actors_of_class(state['world'], unreal.FF_AtlasCameraRig)[0]
    state['controller'] = unreal.GameplayStatics.get_player_controller(state['world'], 0)
    state['globe'].set_auto_spin_paused(True)
    assert not state['globe'].is_country_region_mode(), '타이틀에서 영역 모드 활성화됨'
    assert state['globe'].get_editor_property('region_data') == state['data'], '저장된 Frontend RegionData 참조 누락'
    state['original_materials'] = materials()
    state['title_camera'] = state['rig'].get_editor_property('title_camera').get_actor_transform()
    command('DisableAllScreenMessages')
    yield 1.
    click(screen(unreal.FF_TitleScreen), 'Btn_Play')
    yield 1.
    click(screen(unreal.FF_PlayHubScreen), 'Button_NewFestival')
    yield 2.
    state['rig'].skip_camera_move()
    state['screen'] = screen(unreal.FF_AtlasGlobeScreen)
    yield from wait_until(lambda: list(state['controller'].get_viewport_size()) == [width, height])
    state['base_camera'] = camera_location()
    assert state['globe'].is_country_region_mode(), '국가 화면에서 영역 모드가 켜지지 않음'
    api_hover('None')
    checked(case + ' 실제 타이틀→플레이 허브→새 축제 진입', viewport=[width, height])


def case_body(case, width, height):
    yield from start_case(case, width, height)
    state['stage'] = case + ':region'
    globe = state['globe']
    assert not landmark('KR').is_visible(), '한국 프랍이 영역 모드에서도 표시됨'
    globe.set_selected_country('None')
    api_hover('None')
    material_state()
    api_hover('KR')
    hover = material_state(hovered=1.)
    api_hover('None')
    material_state()
    checked(case + ' 한국 영역 기본 표시와 호버 MID 전환', enabled=[1., 1., 1.],
            hovered=[0., 1., 0.], mid_count=hover['mid_count'])
    state['screen'].inspect_country('KR')
    yield 2.
    yield from wait_until(settled)
    material_state(selected=1.)
    assert not landmark('KR').is_visible()
    camera_check('KR', case, width, height)
    ray_checks(case)
    if case == CASES[0][0]:
        transformed_ray_checks()
    yield from feedback_checks(case, width, height)
    checked(case + ' 한국 선택 마스크·프랍 숨김·기존 카메라', enabled=1., mask=state['region'].mask.get_path_name())
    if case == CASES[0][0]:
        for country in ('DE', 'BR'):
            state['stage'] = 'fallback:' + country
            state['screen'].inspect_country(country)
            yield 2.
            yield from wait_until(settled)
            component = landmark(country)
            assert component.is_visible(), country + ' 프랍이 영역 모드에서 사라짐'
            lo, hi = component.get_local_bounds()
            center = component.get_world_transform().transform_location(vector(mul(add(xyz(lo), xyz(hi)), .5)))
            outward = unit(sub(xyz(center), xyz(globe.get_actor_location())))
            origin, direction = vector(add(xyz(center), mul(outward, 30000.))), vector(mul(outward, -1.))
            assert str(globe.pick_landmark(origin, direction)) == country
            assert str(globe.pick_country(origin, direction)) == country, country + ' 기존 프랍 선택 대체 경로 실패'
            material_state()
            camera_check(country, case, width, height)
            checked(country + ' 기존 프랍·선택·잠금 데이터 유지',
                    unlocked=bool(next(row for row in state['catalog'].countries if str(row.country_id) == country).unlocked))
        state['stage'] = 'drag_api'
        state['screen'].inspect_country('KR')
        yield .15
        globe.add_drag_input(unreal.Vector2D(12., 0.))
        stopped = camera_location()
        assert str(globe.get_selected_country_id()) == 'None' and not globe.is_transitioning()
        yield 1.5
        assert not state['rig'].is_country_inspection_moving() and distance(stopped, camera_location()) < .1
        checked('드래그 API 선택·회전·카메라 접근 취소', physical_input_tested=False)
        state['screen'].inspect_country('KR')
        yield 2.
        yield from wait_until(settled)
    state['stage'] = case + ':return_title'
    state.pop('api_hover', None)
    assert state['flow'].go_back()
    yield 1.
    state['rig'].skip_camera_move()
    assert not globe.is_country_region_mode(), '국가 화면 이탈 후 영역 모드가 남음'
    assert materials() == state['original_materials'], '국가 화면 이탈 후 원본 재질 복원 실패'
    assert distance(camera_location(), state['base_camera']) < .1
    assert state['flow'].go_back()
    yield 2.
    state['rig'].skip_camera_move()
    screen(unreal.FF_TitleScreen)
    assert not globe.is_country_region_mode()
    assert landmark('KR').is_visible(), '타이틀 복귀 후 한국 프랍 복원 실패'
    assert materials() == state['original_materials']
    title_camera = state['rig'].get_editor_property('title_camera').get_actor_transform()
    assert title_camera.export_text() == state['title_camera'].export_text(), '저장된 타이틀 카메라 변경'
    yield from capture(case + '_title_restored', width, height)
    checked(case + ' 타이틀 복귀·한국 프랍·원본 재질·카메라 복원')


def end_case():
    state['stage'] = 'PIE_cleanup'
    state.pop('api_hover', None)
    access = state.get('accessibility')
    if access and 'original_reduce_motion' in state:
        access.set_reduce_motion_enabled(state['original_reduce_motion'])
        assert access.is_reduce_motion_enabled() == state['original_reduce_motion'], '접근성 설정 복원 실패'
    for key in ('world', 'flow', 'globe', 'rig', 'controller', 'screen', 'original_materials', 'title_camera', 'base_camera',
                'accessibility', 'original_reduce_motion', 'feedback_camera', 'feedback_globe', 'feedback_state'):
        state.pop(key, None)
    gc.collect()
    if state.get('owns_pie'):
        # 시작 요청 직후 실패해도 PIE 생성 요청을 취소하고 종료를 확인한다.
        level.editor_request_end_play()
        yield from wait_until(lambda: not level.is_in_play_in_editor())
        yield .5
        assert not level.is_in_play_in_editor(), 'PIE 종료가 유지되지 않음'
        state['owns_pie'] = False


def run():
    assert not level.is_in_play_in_editor(), '사용자의 기존 PIE에서는 실행하지 않는다.'
    report['hashes_before'] = hashes()
    state['user_settings'] = {path: path.read_bytes() for path in (ROOT / 'Saved/Config').glob('*/GameUserSettings.ini')}
    assert level.load_level(FRONTEND)
    state['catalog'] = unreal.load_asset(CATALOG)
    state['data'] = unreal.load_asset(REGIONS)
    assert state['catalog'] and isinstance(state['data'], unreal.FF_AtlasCountryRegions)
    state['region'] = next(row for row in state['data'].regions if str(row.country_id) == 'KR')
    assert state['region'].mask and state['region'].polygons
    state['surface_vertices'] = [xyz(point) for point in state['data'].surface_vertices]
    # UE 약어 분리 규칙에 의존하지 않고 원래 UPROPERTY 이름으로 읽는다.
    state['surface_uvs'] = [[float(uv.x), float(uv.y)] for uv in state['data'].get_editor_property('SurfaceUVs')]
    state['surface_weights'] = [float(value) for value in state['data'].surface_weights]
    state['surface_indices'] = [int(value) for value in state['data'].surface_indices]
    count = len(state['surface_vertices'])
    assert count > 0 and len(state['surface_uvs']) == count and len(state['surface_weights']) == count
    assert state['surface_indices'] and len(state['surface_indices']) % 3 == 0
    assert all(0 <= index < count for index in state['surface_indices'])
    checked('저장된 DataAsset·실제 LOD0 표면 데이터', vertices=count,
            triangles=len(state['surface_indices']) // 3, data=state['data'].get_path_name())
    for case, width, height in CASES:
        try:
            yield from case_body(case, width, height)
        finally:
            yield from end_case()
    report['completed_all_cases'] = True


def teardown():
    yield from end_case()
    state['stage'] = 'saved_state_cleanup'
    if 'user_settings' in state:
        current = set((ROOT / 'Saved/Config').glob('*/GameUserSettings.ini'))
        for path in current - set(state['user_settings']):
            assert (ROOT / 'Saved/Config').resolve() in path.resolve().parents
            path.unlink()
        for path, contents in state['user_settings'].items():
            path.write_bytes(contents)
    if 'hashes_before' in report:
        report['hashes_after'] = hashes()
        report['assets_unchanged'] = report['hashes_before'] == report['hashes_after']
        assert report['assets_unchanged'], '검사 중 저장 에셋 바이트 변경'
    report['pie_ended'] = not level.is_in_play_in_editor()


def lifecycle():
    try:
        yield from run()
    except Exception:
        report['error'] = traceback.format_exc()
        unreal.log_error('ATLAS_REGIONS_FAIL ' + report['error'])
    finally:
        try:
            yield from teardown()
        except Exception:
            report['cleanup_errors'].append(traceback.format_exc())
            unreal.log_error('ATLAS_REGIONS_FAIL ' + report['cleanup_errors'][-1])
    report['status'] = ('passed' if report.get('completed_all_cases') and 'error' not in report
                        and not report['cleanup_errors'] else 'failed')
    report['elapsed_seconds'] = time.monotonic() - state['started']
    report['finished_at'] = datetime.now(timezone.utc).isoformat()
    write()
    unreal.log('ATLAS_REGIONS_RESULT ' + report['status'] + ' ' + str(REPORT))


iterator = lifecycle()


def tick(delta):
    global handle
    try:
        # Slate가 실제 커서 상태를 반영한 뒤 시안 캡처용 상태만 다시 넣는다.
        if 'api_hover' in state and state.get('globe'):
            state['globe'].set_hovered_country(state['api_hover'])
        if time.monotonic() < state['next']:
            return
        delay = next(iterator)
        state['next'] = time.monotonic() + max(.01, float(delay))
        write()
    except StopIteration:
        unreal.unregister_slate_post_tick_callback(handle)
        handle = None
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    except Exception:
        # 생성기 밖 보고서 쓰기 실패도 콜백을 남겨 매 프레임 재실행하지 않게 한다.
        unreal.log_error('ATLAS_REGIONS_CALLBACK_FAIL ' + traceback.format_exc())
        unreal.unregister_slate_post_tick_callback(handle)
        handle = None
        try:
            iterator.close()
        except Exception:
            # GeneratorExit 중 finally의 비동기 cleanup은 yield할 수 없다.
            unreal.log_error('ATLAS_REGIONS_GENERATOR_CLOSE ' + traceback.format_exc())
        finally:
            for key in ('world', 'flow', 'globe', 'rig', 'controller', 'screen', 'original_materials'):
                state.pop(key, None)
            gc.collect()
            if state.get('owns_pie'):
                level.editor_request_end_play()
            unreal.EditorPythonScripting.set_keep_python_script_alive(False)


write()
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle = unreal.register_slate_post_tick_callback(tick)
