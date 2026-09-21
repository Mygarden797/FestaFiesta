"""저장된 8개국 영역 에셋을 실제 Frontend PIE에서 검증한다.

일회용 에디터에서 실행하며 에셋은 저장하지 않는다. 실행 전에
Saved/AtlasRegions/verify_world_request.json에 {"output":"검사별_새_폴더"}를 둔다.
1600x900에서 8개국, 960x720·1680x720에서 대표 국가를 캡처한다.
버튼 델리게이트·공개 선택/호버/광선 API 검사이며 물리 마우스 입력 검사가 아니다.
선택 가능·호버·선택·선택 중 다른 나라 호버, 날짜 변경선과 움직임 감소도 확인한다.
"""
from datetime import datetime, timezone
from pathlib import Path
from array import array
import gc
import hashlib
import json
import math
import struct
import statistics
import time
import traceback

import unreal


ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
BASE = ROOT / 'Saved/AtlasRegions'
REQUEST = BASE / 'verify_world_request.json'
config = json.loads(REQUEST.read_text(encoding='utf-8-sig')) if REQUEST.exists() else {}
OUT = (BASE / config.get('output', datetime.now().strftime('verify_%Y%m%d_%H%M%S_%f'))).resolve()
assert OUT != BASE.resolve() and BASE.resolve() in OUT.parents, '결과는 Saved/AtlasRegions 하위에 둔다.'
assert not OUT.exists(), '결과 덮어쓰기를 막기 위해 새로운 output 이름을 사용한다.'
OUT.mkdir(parents=True)
REPORT = OUT / 'report.json'
FRONTEND = '/Game/FestaFiesta/Maps/Frontend'
CATALOG = '/Game/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas'
REGIONS = '/Game/FestaFiesta/UI/Atlas/CountryRegions/DA_AtlasCountryRegions'
CASES = [('standard_16x9', 1600, 900), ('small_4x3', 960, 720), ('ultrawide_21x9', 1680, 720)]
COUNTRIES = ('KR', 'BR', 'DE', 'JP', 'US', 'ES', 'TH', 'AU')
SAMPLES = [('Seoul', 37.5665, 126.9780, 'KR'),
           ('Busan', 35.1796, 129.0756, 'KR'),
           ('Jeju_inland', 33.36, 126.53, 'KR'),
           ('Brasilia', -15.79, -47.88, 'BR'),
           ('Manaus', -3.10, -60.03, 'BR'),
           ('Berlin', 52.52, 13.40, 'DE'),
           ('Munich', 48.14, 11.58, 'DE'),
           ('Tokyo_inland', 35.72, 139.58, 'JP'),
           ('Hokkaido_inland', 43.15, 142.65, 'JP'),
           ('Kyushu_inland', 32.70, 130.85, 'JP'),
           ('Kansas', 38.50, -98.00, 'US'),
           ('Alaska_Fairbanks', 64.84, -147.72, 'US'),
           ('Hawaii_inland', 19.55, -155.55, 'US'),
           ('Madrid', 40.42, -3.70, 'ES'),
           ('Barcelona_inland', 41.60, 2.00, 'ES'),
           ('Bangkok_inland', 13.90, 100.55, 'TH'),
           ('ChiangMai', 18.79, 98.99, 'TH'),
           ('Canberra', -35.28, 149.13, 'AU'),
           ('Perth_inland', -31.90, 116.15, 'AU'),
           ('Tasmania_inland', -42.10, 146.65, 'AU'),
           ('Pyongyang', 39.0392, 125.7625, 'None'),
           ('Paris_nonlisted', 48.86, 2.35, 'None'),
           ('China_nonlisted', 39.90, 116.41, 'None'),
           ('Canada_nonlisted', 53.55, -113.49, 'None'),
           ('Mexico_nonlisted', 19.43, -99.13, 'None'),
           ('Portugal_nonlisted', 38.73, -9.14, 'None'),
           ('EastSea_offshore', 35.8, 130.5, 'None'),
           ('CentralPacific', 0., -150., 'None'),
           ('IndianOcean', -20., 80., 'None')]
# 원본 표시 메시가 생략한 작은 섬은 폴리곤 보존과 실제 클릭 가능 여부를 구분한다.
# 삼각형이 있으면 실제 광선도 반드시 일치해야 한다. 없으면 제한을 보고서에 남긴다.
SMALL_ISLAND_SAMPLES = [('Adak', 51.85, -176.65, 'US'),
                        ('Attu', 52.90, 173.0, 'US'),
                        ('Semisopochnoi_positive179', 51.95, 179.62, 'US'),
                        ('AleutianIsland_negative179', 51.26, -179.11, 'US'),
                        ('Tsushima', 34.40, 129.33, 'JP'),
                        ('Okinawa', 26.32, 127.80, 'JP'),
                        ('Mallorca', 39.63, 2.91, 'ES'),
                        ('Tenerife', 28.29, -16.57, 'ES'),
                        ('Phuket', 7.92, 98.34, 'TH')]
state = dict(started=time.monotonic(), next=time.monotonic() + 1., stage='preflight')
report = dict(status='running', started_at=datetime.now(timezone.utc).isoformat(),
              scope='8개국 동시 강조·독립 호버/선택·날짜 변경선·실제 표면 광선·카메라/타이틀 복원',
              asset_save_requested=False, physical_input_tested=False,
              physical_press_release_drag_tested=False,
              physical_hover_cursor_tooltip_tested=False,
              input_method='버튼 delegate broadcast + InspectCountry/SetHoveredCountry/PickCountry 공개 API',
              hover_capture_method='Slate post-tick에서 SetHoveredCountry 상태 유지; 실제 커서 호버/툴팁 입력 검사는 아님',
              feedback_capture_note='상태 비교 시 상세 패널과 카메라는 유지하고 공개 API로 지구 호버/선택만 바꾼다.',
              visual_readability='캡처를 별도로 육안 확인해야 한다. 자동 판정은 크기·상태·투영까지만 수행한다.',
              checks=[], ray_samples=[], polygon_samples=[], mesh_limitations=[], cases=[], images=[], cleanup_errors=[],
              callback_errors=[], report_serialization_errors=[],
              performance={'mode_entries': [], 'picking': [],
                           'note': '에디터 PIE·Python 바인딩을 포함한 CPU 실측이다. 패키징/GPU 프레임 성능 결과가 아니다.'})
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
handle = None


class PackedUVs:
    """백만 정점의 UV를 Python float/list 객체 대신 연속된 64비트 배열로 보관한다."""
    def __init__(self, points):
        self.components = array('d', (component for point in points for component in (point.x, point.y)))

    def __len__(self):
        return len(self.components) // 2

    def __getitem__(self, index):
        return self.components[index * 2], self.components[index * 2 + 1]


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


def report_json_default(value):
    # 성능용 연속 배열은 JSON 경계에서만 list로 바꾼다. UObject 등을 조용히 문자열로 숨기지 않는다.
    if isinstance(value, array):
        return value.tolist()
    raise TypeError('보고서에 지원하지 않는 값: ' + type(value).__name__)


def write():
    report['last_stage'] = state['stage']
    try:
        encoded = json.dumps(report, ensure_ascii=False, indent=2, default=report_json_default)
    except (TypeError, ValueError):
        # 보고서 형식 오류 때문에 비동기 finally/PIE 종료를 중단하지 않는다.
        report['report_serialization_errors'].append(traceback.format_exc())
        report['status'] = 'failed'
        encoded = json.dumps(report, ensure_ascii=False, indent=2,
                             default=lambda value: {'unsupported_type': type(value).__name__, 'value': str(value)[:500]})
        recovered = json.loads(encoded)
        report.clear()
        report.update(recovered)
    temporary = REPORT.with_suffix('.json.tmp')
    temporary.write_text(encoded, encoding='utf-8')
    temporary.replace(REPORT)


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


def material_state(hovered='None', selected='None', motion=1.):
    values = region_mids()
    expected = dict(RegionMotionEnabled=motion)
    for slot, region in enumerate(state['regions']):
        prefix = f'Region{slot}'
        country = str(region.country_id)
        expected.update({prefix + 'Enabled': 1., prefix + 'Hovered': float(country == hovered),
                         prefix + 'Selected': float(country == selected)})
    actual = {name: [float(value.get_scalar_parameter_value(name)) for value in values]
              for name in expected}
    for name, wanted in expected.items():
        assert all(abs(value - wanted) < .001 for value in actual[name]), f'{name}={actual[name]}, 기대값={wanted}'
    for slot, region in enumerate(state['regions']):
        assert all(value.get_texture_parameter_value(f'Region{slot}Mask') == region.mask for value in values), f'{region.country_id} 마스크 불일치'
        bounds = region.bounds
        for value in values:
            actual_bounds = value.get_vector_parameter_value(f'Region{slot}Bounds')
            assert max(abs(getattr(actual_bounds, key) - getattr(bounds, key)) for key in ('r', 'g', 'b', 'a')) < .001
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


def build_uv_grid():
    # 전체 지구를 표본마다 순회하지 않는다. 첫 한 번만 UV 셀에 삼각형을 분배한다.
    coords, indices = state['surface_uvs'], state['surface_indices']
    columns, rows = 360, 180
    cells = {}
    def cell(value, size):
        return min(size - 1, max(0, int(value * size)))
    for offset in range(0, len(indices), 3):
        points = [coords[indices[offset + i]] for i in range(3)]
        x0, x1 = (cell(fn(point[0] for point in points), columns) for fn in (min, max))
        y0, y1 = (cell(fn(point[1] for point in points), rows) for fn in (min, max))
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                cells.setdefault(y * columns + x, []).append(offset)
    state['uv_grid'] = cells, columns, rows
    checked('실제 전체 표면 UV 검색 인덱스', occupied_cells=len(cells), columns=columns, rows=rows,
            references=sum(len(values) for values in cells.values()))


def triangle_at_uv(uv):
    # 실제 LOD0 삼각형 안에서 원하는 UV의 무게중심 좌표를 구한다.
    coords, indices = state['surface_uvs'], state['surface_indices']
    matches = []
    cells, columns, rows = state['uv_grid']
    x = min(columns - 1, max(0, int(uv[0] * columns)))
    y = min(rows - 1, max(0, int(uv[1] * rows)))
    for offset in cells.get(y * columns + x, ()):
        # array('i') slice를 보고서에 그대로 넣으면 JSON 변환이 실패한다.
        ids = [indices[offset + index] for index in range(3)]
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


def ring_contains(points, longitude, latitude):
    inside = False
    for index, b in enumerate(points):
        a = points[index - 1]
        if (a[1] > latitude) != (b[1] > latitude):
            if longitude < a[0] + (latitude - a[1]) * (b[0] - a[0]) / (b[1] - a[1]):
                inside = not inside
    return inside


def polygon_checks():
    # 저장된 경계의 독립 판정이다. 런타임 C++ 선택 판정은 아래 실제 광선으로 따로 검사한다.
    cached = []
    for region in state['regions']:
        bounds = region.bounds
        polygons = [([[float(p.x), float(p.y)] for p in polygon.outer.points],
                     [[[float(p.x), float(p.y)] for p in hole.points] for hole in polygon.holes])
                    for polygon in region.polygons]
        cached.append((str(region.country_id), [bounds.r, bounds.g, bounds.b, bounds.a], polygons))
    for name, latitude, longitude, expected in SAMPLES + SMALL_ISLAND_SAMPLES:
        found = []
        for country, bounds, polygons in cached:
            # 날짜 변경선이 연속 경도 범위 밖에 있어도 원본 좌표를 훼손하지 않는다.
            center = (bounds[0] + bounds[2]) * .5
            lon = longitude + 360. * math.floor((center - longitude) / 360. + .5)
            if not (bounds[0] <= lon <= bounds[2] and bounds[1] <= latitude <= bounds[3]):
                continue
            if any(ring_contains(outer, lon, latitude)
                   and not any(ring_contains(hole, lon, latitude) for hole in holes)
                   for outer, holes in polygons):
                found.append(country)
        assert found == ([] if expected == 'None' else [expected]), f'{name}: 저장 경계 포함 판정 {found}/{expected}'
        report['polygon_samples'].append(dict(sample=name, latitude=latitude, longitude=longitude,
                                              expected=expected, actual=found))
    checked('8개국 저장 경계 본토·섬·날짜 변경선·외부 좌표', samples=len(report['polygon_samples']),
            method='DataAsset 폴리곤 독립 Python 판정; 실제 C++ 판정은 광선 검사에서 별도 수행')


def surface_point(sample, allow_missing=False):
    name, lat, lon, expected = sample
    uv = [(lon + 180.) / 360., (90. - lat) / 180.]
    match = triangle_at_uv(uv)
    if not match:
        assert expected == 'None' or allow_missing, name + ': 실제 육지 삼각형이 없음'
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
        pos = xyz(transform.transform_location(vector(mul(xyz(state['surface_vertices'][i]), scale))))
        delta = (thickness - float(data.base_land_thickness)
                 + state['surface_weights'][i] * (mountain - float(data.base_mountain_height)))
        world_vertices.append(add(pos, mul(unit(sub(pos, center)), delta)))
    pos = [sum(bary[j] * world_vertices[j][axis] for j in range(3)) for axis in range(3)]
    return vector(pos), dict(uv=uv, triangle=index, vertex_ids=ids, barycentric=bary,
                             source='실제 LOD0 삼각형 UV 보간 + 현재 MPC WPO',
                             shape_parameters=dict(LandThickness=thickness, MountainHeight=mountain))


def ray_checks(label, samples=SAMPLES, allow_missing=False):
    globe = state['globe']
    center = xyz(globe.get_editor_property('globe_root').get_world_location())
    elapsed = time.monotonic()
    for sample in samples:
        point, evidence = surface_point(sample, allow_missing)
        if allow_missing and evidence['triangle'] is None:
            report['mesh_limitations'].append(dict(sample=sample[0], country=sample[3],
                reason='기존 표시 메시의 해당 UV에 육지 삼각형이 없음; 폴리곤 보존과 별도로 해당 점의 클릭은 미검증'))
            report['ray_samples'].append(dict(case=label, sample=sample[0], logical_country=sample[3],
                                             ray_tested=False, status='mesh_coverage_missing', **evidence))
            continue
        normal = unit(sub(xyz(point), center))
        origin = vector(add(xyz(point), mul(normal, 30000.)))
        direction = vector(mul(normal, -1.))
        region = str(globe.pick_country_region(origin, direction))
        country = str(globe.pick_country(origin, direction))
        expected = sample[3]
        assert region == expected, f'{label}/{sample[0]} 영역={region}, 기대값={expected}'
        assert country == expected, f'{sample[0]}: 통합 국가 선택 경로 불일치 {country}/{expected}'
        if expected != 'None':
            backside = str(globe.pick_country_region(vector(sub(center, mul(normal, 30000.))), vector(normal)))
            assert backside != expected, sample[0] + ': 뒷면 국가 관통 선택'
        else:
            backside = None
        report['ray_samples'].append(dict(case=label, sample=sample[0], latitude=sample[1], longitude=sample[2],
                                         expected=expected, logical_country=sample[3], region=region, country=country, backside=backside,
                                         ray_tested=True, world_point=xyz(point), **evidence))
    assert str(globe.pick_country_region(vector(center), unreal.Vector())) == 'None'
    checked(label + ' 실제 삼각형 영역 광선·외부·후면·영벡터', samples=len(samples),
            elapsed_seconds=time.monotonic() - elapsed)


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


def picking_performance():
    """이미 준비된 BVH의 대표 좌표를 반복 측정한다. 표면 점 계산·보고서 쓰기는 측정 밖이다."""
    globe = state['globe']
    center = xyz(globe.get_editor_property('globe_root').get_world_location())
    selected_samples = [sample for sample in SAMPLES if sample[0] in
                        ('Seoul', 'Berlin', 'Kansas', 'Canberra', 'EastSea_offshore', 'Paris_nonlisted')]
    all_elapsed = []
    for sample in selected_samples:
        point, _ = surface_point(sample)
        normal = unit(sub(xyz(point), center))
        origin = vector(add(xyz(point), mul(normal, 30000.)))
        direction = vector(mul(normal, -1.))
        assert str(globe.pick_country(origin, direction)) == sample[3]  # 캐시 준비는 시간에서 제외
        elapsed = []
        for _ in range(21):
            started = time.perf_counter()
            country = str(globe.pick_country(origin, direction))
            elapsed.append((time.perf_counter() - started) * 1000.)
            assert country == sample[3]
        all_elapsed.extend(elapsed)
        report['performance']['picking'].append(dict(sample=sample[0], country=sample[3],
            calls=len(elapsed), median_ms=statistics.median(elapsed), maximum_ms=max(elapsed),
            minimum_ms=min(elapsed), samples_ms=elapsed))
    checked('준비된 BVH의 대표 PickCountry CPU 시간', calls=len(all_elapsed),
            median_ms=statistics.median(all_elapsed), maximum_ms=max(all_elapsed),
            benchmark='Python 바인딩 포함·패키징 프레임 성능 아님', absolute_threshold_enforced=False)


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


def feedback_capture(name, width, height, hovered='None', selected='None', motion=1.):
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
    yield from feedback_capture(case + '_KR_selected', width, height, selected='KR')

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
    yield from feedback_capture(case + '_KR_hovered', width, height, hovered='KR')
    globe.set_selected_country('KR', True)
    yield .3
    yield from feedback_capture(case + '_KR_selected_hovered', width, height, hovered='KR', selected='KR')
    api_hover('None')
    material_state(selected='KR')
    checked(case + ' 같은 카메라에서 기본·호버·선택·선택 중 호버 분리',
            states=[dict(hovered=0, selected=0), dict(hovered=1, selected=0),
                    dict(hovered=0, selected=1), dict(hovered=1, selected=1)],
            physical_input_tested=False, mid_count=len(region_mids()))

    if case == CASES[0][0]:
        access = state['accessibility']
        access.set_reduce_motion_enabled(True)
        yield .3  # Globe Tick에서 현재 접근성 값을 MID에 동기화한다.
        yield from feedback_capture(case + '_KR_selected_reduce_motion', width, height, selected='KR', motion=0.)
        access.set_reduce_motion_enabled(False)
        yield .3
        material_state(selected='KR', motion=1.)
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
    entry_started, entry_wall = time.perf_counter(), time.monotonic()
    click(screen(unreal.FF_PlayHubScreen), 'Button_NewFestival')
    report['performance']['mode_entries'].append(dict(case=case, source='실제 새 축제 버튼 delegate',
        elapsed_ms=(time.perf_counter() - entry_started) * 1000., wall_seconds=time.monotonic() - entry_wall,
        region_mode_active_on_return=state['globe'].is_country_region_mode(),
        includes='버튼 delegate 동기 호출 구간만 측정. 반환 시 영역 모드가 false면 이후 비동기 화면 활성화·BVH 구축은 포함하지 않음'))
    yield 2.
    state['rig'].skip_camera_move()
    state['screen'] = screen(unreal.FF_AtlasGlobeScreen)
    yield from wait_until(lambda: list(state['controller'].get_viewport_size()) == [width, height])
    state['base_camera'] = camera_location()
    assert state['globe'].is_country_region_mode(), '국가 화면에서 영역 모드가 켜지지 않음'
    if case == CASES[0][0]:
        # 실제 UI 진입과 별도로 새 BVH가 동기 구축되는 공개 API 구간을 측정한다.
        # off에서 캐시가 Reset됨을 C++에서 확인했다. 국가별 카메라 데이터는 건드리지 않는다.
        state['globe'].set_country_region_mode(False)
        entry_started, entry_wall = time.perf_counter(), time.monotonic()
        state['globe'].set_country_region_mode(True)
        evidence = dict(case=case, source='SetCountryRegionMode(False) 후 True 직접 호출',
            elapsed_ms=(time.perf_counter() - entry_started) * 1000., wall_seconds=time.monotonic() - entry_wall,
            includes='재질 교체·표면 캐시 확인/필요 시 구축·강조 갱신을 포함한 동기 재진입; 순수 BVH만의 시간이 아님')
        report['performance']['mode_entries'].append(evidence)
        assert state['globe'].is_country_region_mode()
        checked('국가 영역 모드 동기 재진입 CPU 시간', **evidence)
    api_hover('None')
    checked(case + ' 실제 타이틀→플레이 허브→새 축제 진입', viewport=[width, height])


def independent_feedback(case, width, height):
    globe = state['globe']
    state['screen'].inspect_country('DE')
    yield 2.
    yield from wait_until(settled)
    camera_check('DE', case + '_independent_hover', width, height)
    state['feedback_camera'] = camera_location()
    state['feedback_globe'] = globe.get_editor_property('globe_root').get_world_transform().export_text()
    api_hover('ES')
    yield .3
    yield from feedback_capture(case + '_DE_selected_ES_hovered', width, height, hovered='ES', selected='DE')
    positions = {}
    for country, lat, lon in (('DE', 51., 10.), ('ES', 40., -4.)):
        point = projected(geographic_point(lat, lon))
        positions[country] = dict(pixel=point, in_viewport=0 <= point[0] < width and 0 <= point[1] < height)
    checked('선택한 독일과 호버한 스페인의 독립 슬롯 강조', selected='DE', hovered='ES',
            projected_positions=positions, all_eight_regions_remain_enabled=True,
            physical_input_tested=False)
    api_hover('None')
    material_state(selected='DE')
    state.pop('feedback_camera', None)
    state.pop('feedback_globe', None)


def case_body(case, width, height):
    yield from start_case(case, width, height)
    state['stage'] = case + ':region'
    globe = state['globe']
    for country in COUNTRIES:
        assert not landmark(country).is_visible(), country + ' 프랍이 영역 모드에서도 표시됨'
    globe.set_selected_country('None')
    api_hover('None')
    material_state()
    for country in COUNTRIES:
        api_hover(country)
        material_state(hovered=country)
    api_hover('None')
    idle = material_state()
    checked(case + ' 8개 영역 동시 기본 표시와 개별 호버 MID 전환', country_slots=state['slot_ids'],
            all_slots_enabled=True, mid_count=idle['mid_count'])
    state['screen'].inspect_country('KR')
    yield 2.
    yield from wait_until(settled)
    material_state(selected='KR')
    camera_check('KR', case, width, height)
    if case == CASES[0][0]:
        ray_checks(case)
        ray_checks(case + '_small_islands_and_dateline', SMALL_ISLAND_SAMPLES, allow_missing=True)
        picking_performance()
        transformed_ray_checks()
    yield from feedback_checks(case, width, height)
    checked(case + ' 한국 선택 마스크·프랍 숨김·기존 카메라', enabled=1.)
    if case == CASES[0][0]:
        for country in COUNTRIES[1:]:
            state['stage'] = 'select:' + country
            state['screen'].inspect_country(country)
            yield 2.
            yield from wait_until(settled)
            assert not landmark(country).is_visible(), country + ' 프랍이 선택 후 다시 나타남'
            material_state(selected=country)
            camera_check(country, case, width, height)
            yield from capture(case + '_' + country + '_selected', width, height)
            api_hover(country)
            material_state(hovered=country, selected=country)
            api_hover('None')
            material_state(selected=country)
            checked(country + ' 영역 선택·호버·조형물 숨김·잠금 데이터 유지',
                    unlocked=bool(next(row for row in state['catalog'].countries if str(row.country_id) == country).unlocked))
        yield from independent_feedback(case, width, height)
        state['stage'] = 'rapid_switch_api'
        for country in ('BR', 'JP', 'DE', 'AU'):
            state['screen'].inspect_country(country)
            yield .1
        yield 2.
        yield from wait_until(settled)
        material_state(selected='AU')
        camera_check('AU', case + '_rapid_switch', width, height)
        checked('빠른 연속 국가 선택의 최종 목적지 유지', order=['BR', 'JP', 'DE', 'AU'])
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
    assert materials() == state['original_materials']
    title_camera = state['rig'].get_editor_property('title_camera').get_actor_transform()
    assert title_camera.export_text() == state['title_camera'].export_text(), '저장된 타이틀 카메라 변경'
    yield from capture(case + '_title_restored', width, height)
    # 기존 지평선 판정이 뒤편 프랍을 숨기므로 8개를 한 시점에 visible로 요구하지 않는다.
    # 각국을 정면으로 잠깐 돌려 복원을 검사하고, 캡처한 타이틀 방향을 되돌린다.
    orientation = globe.get_orientation()
    try:
        for country in COUNTRIES:
            globe.focus_on_country(country, True)
            # FocusOnCountry 즉시 회전은 장면 각도만 바꾼다. 프랍 지평선 가시성은 다음 Tick에 갱신된다.
            yield .1
            assert landmark(country).is_visible(), country + ': 타이틀 정면에서도 프랍 복원 실패'
    finally:
        globe.set_orientation(orientation)
    assert materials() == state['original_materials']
    checked(case + ' 타이틀 복귀·8개 프랍·원본 재질·카메라 복원')


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
    state['regions'] = list(state['data'].regions)
    state['slot_ids'] = [str(region.country_id) for region in state['regions']]
    assert len(state['slot_ids']) == len(COUNTRIES) and set(state['slot_ids']) == set(COUNTRIES), '국가별 영역 누락/중복'
    assert all(region.mask and region.polygons for region in state['regions']), '국가별 마스크/경계 누락'
    catalog_ids = {str(country.country_id) for country in state['catalog'].countries}
    assert set(COUNTRIES) <= catalog_ids, '영역이 국가 카탈로그에 없음'
    report['slot_order'] = state['slot_ids']
    polygon_checks()
    # Vector/weight 전체를 Python 중첩 list로 복제하지 않는다. 광선 표본의 정점만 읽는다.
    state['surface_vertices'] = state['data'].get_editor_property('SurfaceVertices')
    # UE 약어 분리 규칙에 의존하지 않고 원래 UPROPERTY 이름으로 읽는다.
    state['surface_uvs'] = PackedUVs(state['data'].get_editor_property('SurfaceUVs'))
    state['surface_weights'] = state['data'].get_editor_property('SurfaceWeights')
    state['surface_indices'] = array('i', state['data'].get_editor_property('SurfaceIndices'))
    count = len(state['surface_vertices'])
    assert count > 0 and len(state['surface_uvs']) == count and len(state['surface_weights']) == count
    assert state['surface_indices'] and len(state['surface_indices']) % 3 == 0
    assert all(0 <= index < count for index in state['surface_indices'])
    checked('저장된 DataAsset·실제 LOD0 표면 데이터', vertices=count,
            triangles=len(state['surface_indices']) // 3, data=state['data'].get_path_name())
    report['verification_memory_strategy'] = dict(
        vertices='Unreal Array 보관, 표본의 삼각형 정점만 Python Vector로 읽음',
        weights='Unreal Array 보관, 표본 정점만 읽음',
        uv_buffer_bytes=state['surface_uvs'].components.buffer_info()[1] * state['surface_uvs'].components.itemsize,
        index_buffer_bytes=len(state['surface_indices']) * state['surface_indices'].itemsize,
        full_surface_json_loaded=False,
        peak_process_memory_measured=False)
    build_uv_grid()
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
                        and not report['cleanup_errors'] and not report['callback_errors']
                        and not report['report_serialization_errors'] else 'failed')
    report['elapsed_seconds'] = time.monotonic() - state['started']
    report['finished_at'] = datetime.now(timezone.utc).isoformat()
    write()
    unreal.log('ATLAS_REGIONS_RESULT ' + report['status'] + ' ' + str(REPORT))


iterator = lifecycle()


def finish_callback():
    global handle
    unreal.unregister_slate_post_tick_callback(handle)
    handle = None
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)


def tick(delta):
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
        finish_callback()
    except Exception as error:
        details = traceback.format_exc()
        report['callback_errors'].append(details)
        unreal.log_error('ATLAS_REGIONS_CALLBACK_FAIL ' + details)
        state.pop('api_hover', None)
        try:
            # close()는 finally의 yield를 금지한다. 원래 생성기에 예외를 전달하면
            # lifecycle의 정상 실패 경로가 PIE 종료 대기·설정 복원·최종 보고까지 수행한다.
            delay = iterator.throw(error)
            state['next'] = time.monotonic() + max(.01, float(delay))
            write()
        except StopIteration:
            finish_callback()
        except Exception:
            # 복구 경로 자체의 예외는 실패로 기록하고 최소 종료 요청을 남긴다.
            report['cleanup_errors'].append(traceback.format_exc())
            report['status'] = 'failed'
            report['finished_at'] = datetime.now(timezone.utc).isoformat()
            unreal.log_error('ATLAS_REGIONS_CALLBACK_RECOVERY_FAIL ' + report['cleanup_errors'][-1])
            for key in ('world', 'flow', 'globe', 'rig', 'controller', 'screen', 'original_materials'):
                state.pop(key, None)
            gc.collect()
            if state.get('owns_pie'):
                level.editor_request_end_play()
            report['pie_ended'] = not level.is_in_play_in_editor()
            try:
                write()
            finally:
                finish_callback()


write()
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle = unreal.register_slate_post_tick_callback(tick)
