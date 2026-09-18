"""P03 v002 지역 시험: 원본 해안 고정, 국소 여유에 따른 둥근 10uu 해안.

Blender에서 build(), 검토 후 export()를 각각 호출한다. 재실행은 명시적으로
차단하며 P02/P03 오브젝트와 기존 파일은 수정하지 않는다.
"""
from collections import Counter, defaultdict
from pathlib import Path
import hashlib
import json
import math

import bpy
import numpy as np
from mathutils import Vector
from prepare_roundtrip import parse_obj

ROOT = Path(__file__).resolve().parents[4]
ART = ROOT / 'SourceArt/UI/Atlas/BrightGlobe_v1'
OUT = ROOT / 'Saved/BrightGlobe'
NAME = 'FF_BrightGlobe_P03_Pilot_v002'
REPORT = OUT / 'p03_v002_geometry.json'
BLEND = ART / 'Blender/FF_BrightGlobe_P03_pilot_v002.blend'
EXPORTS = ART / 'P03v2/Exports'
TILES = (3, 4, 5)
KINDS = ('Top', 'Coast', 'Bottom')
CONFIG = dict(radius=5000., land_thickness=40., mountain_height=650.,
              height_floor=.008, height_power=1.15, round_depth=12.,
              coast_width_target_uu=10., coast_width_max_uu=12.,
              source_inset_preserve_below_uu=.01,
              minimum_projected_area_ratio=.15,
              coast_round_segments=6, scope_lon_lat=[-20, 55, 20, 65])


def _name(tile, kind):
    return f'SM_BrightP03v2_{tile:02d}_{kind}'


def shape_height(h):
    return np.maximum(0., (np.asarray(h)-.008)/.992)**1.15


def _unit(points):
    return points / np.linalg.norm(points, axis=-1, keepdims=True)


def _sign(vertices, faces):
    a, b, c = (vertices[faces[:, i]] for i in range(3))
    return np.einsum('ij,ij->i', np.cross(b-a, c-a), a)


def _guard_outputs():
    assert not REPORT.exists(), 'v002 기록이 이미 있음: 재개 상태를 먼저 확인'
    assert not BLEND.exists(), 'v002 blend 덮어쓰기 금지'
    for tile in TILES:
        for kind in KINDS:
            assert not (EXPORTS / (_name(tile, kind)+'.obj')).exists()


def _object(scene, name, vertices, faces, uv):
    mesh = bpy.data.meshes.new(name+'_Mesh')
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    layer = mesh.uv_layers.new(name='UVMap')
    layer.data.foreach_set('uv', np.asarray(uv, dtype=np.float32).ravel())
    for face in mesh.polygons:
        face.use_smooth = True
    obj = bpy.data.objects.new('GEO-'+name, mesh)
    scene.collection.objects.link(obj)
    obj['bright_p03_v002_export'] = name
    kind = name.rsplit('_', 1)[-1]
    material = bpy.data.materials.get('MAT-BrightP02-'+kind)
    if material:
        mesh.materials.append(material)
    return obj


def _prepare():
    """전체 타일을 함께 계산하고 검증한다. Blender 데이터는 아직 만들지 않는다."""
    tiles, groups, locked = [], defaultdict(list), set()
    outer_by_key, shortest = {}, {}
    for tile in TILES:
        stem = f'SM_Diorama_{tile:02d}'
        path = ART / 'P02/Inputs' / (stem+'_Top.obj')
        tv, tuv, tf = parse_obj(path)
        pos = np.asarray(tv, dtype=np.float64)
        faces = np.asarray([[v for v, u in face] for face in tf], dtype=np.int32)
        direction = _unit(pos)
        h = np.maximum(0., (np.linalg.norm(pos, axis=1)-5100.)/350.)
        radius = 5040.+650.*shape_height(h)
        keys = [tuple(p) for p in tv]
        index_by_pos = {p: i for i, p in enumerate(keys)}
        edges, coast_pairs = [], set()
        cv, _, _ = parse_obj(ROOT / 'SourceArt/UI/Atlas/DioramaWorld_v1' / (stem+'_Coast.obj'))
        assert len(cv) % 8 == 0, '원본 해안 8정점 계약 불일치'
        for start in range(0, len(cv), 8):
            a, b = index_by_pos.get(cv[start+7]), index_by_pos.get(cv[start+6])
            if a is None or b is None:
                continue
            oa, ob = _unit(np.asarray([cv[start], cv[start+1]], dtype=np.float64))
            edges.append((a, b, oa, ob))
            coast_pairs.add(tuple(sorted((a, b))))
            for index, outer in ((a, oa), (b, ob)):
                key = keys[index]
                if key in outer_by_key:
                    assert np.linalg.norm(outer_by_key[key]-outer) < 1.e-8, '공유 해안 방향 불일치'
                else:
                    outer_by_key[key] = outer
        edge_counts = Counter(tuple(sorted((int(face[i]), int(face[(i+1)%3]))))
                              for face in faces for i in range(3))
        for (a, b), count in edge_counts.items():
            # 위경도 crop와 타일 외곽의 지리 방향을 고정한다.
            if count == 1 and (a, b) not in coast_pairs:
                locked.update((keys[a], keys[b]))
            length = float(np.linalg.norm((direction[a]-direction[b])*5000.))
            for index in (a, b):
                key = keys[index]
                shortest[key] = min(shortest.get(key, float('inf')), length)
        item = dict(tile=tile, path=path, keys=keys, pos=pos, direction=direction,
                    radius=radius, h=h, faces=faces, edges=edges,
                    uv=[tuv[u] for face in tf for v, u in face])
        baseline_vertices = (direction*radius[:, None]).astype(np.float32).astype(np.float64)
        item['baseline_sign'] = _sign(baseline_vertices, faces)
        assert item['baseline_sign'].min() > 0., '원본 윗면 방향 오류'
        for index, key in enumerate(keys):
            groups[key].append((len(tiles), index))
        tiles.append(item)

    # 같은 원본 위치는 동일한 목표 방향과 backoff 비율을 공유한다.
    target, factors, preserved_zero_inset = {}, {}, set()
    for key, outer in outer_by_key.items():
        ti, vi = groups[key][0]
        original = tiles[ti]['direction'][vi]
        tangent = original-outer*np.dot(original, outer)
        length = float(np.linalg.norm(tangent))
        source_width = math.atan2(length, float(np.dot(original, outer)))*5000.
        # 원본 절단면의 6자리 OBJ 반올림 잔차를 해안 inset 방향으로 증폭하지 않는다.
        if source_width < CONFIG['source_inset_preserve_below_uu']:
            preserved_zero_inset.add(key)
            continue
        if key in locked:
            continue
        original_width = source_width
        # 샘플 두 칸을 상한으로 두고, 좁은 지형은 아래 국소 면 방향 검사로 더 줄인다.
        width = max(original_width, min(CONFIG['coast_width_target_uu'],
                                       CONFIG['coast_width_max_uu'], shortest[key]*2.))
        angle = width/5000.
        target[key] = outer*math.cos(angle)+tangent/length*math.sin(angle)
        factors[key] = 1.

    iterations = 0
    while True:
        for item in tiles:
            item['moved'] = item['direction'].copy()
        for key, desired in target.items():
            ti, vi = groups[key][0]
            original = tiles[ti]['direction'][vi]
            moved = _unit(original+(desired-original)*factors[key])
            for tile_index, vertex_index in groups[key]:
                tiles[tile_index]['moved'][vertex_index] = moved
        bad_keys = set()
        for item in tiles:
            # Blender 저장 정밀도에서도 윗면이 뒤집히지 않는지 검증한다.
            item['vertices'] = (item['moved']*item['radius'][:, None]).astype(np.float32)
            item['sign'] = _sign(item['vertices'].astype(np.float64), item['faces'])
            # 방사 방향 투영 면적의 원본 대비 15% 이상을 유지해 슬리버를 피한다.
            item['area_ratio'] = item['sign']/item['baseline_sign']
            for vertex in np.unique(item['faces'][item['area_ratio'] < CONFIG['minimum_projected_area_ratio']]):
                if item['keys'][vertex] in factors:
                    bad_keys.add(item['keys'][vertex])
        if all(item['area_ratio'].min() >= CONFIG['minimum_projected_area_ratio'] for item in tiles):
            break
        assert bad_keys, '이동하지 않은 원본 경계에 퇴화면이 있음'
        iterations += 1
        assert iterations <= 40, '국소 backoff 수렴 실패'
        for key in bad_keys:
            factors[key] = factors[key]*.5 if iterations < 25 else 0.

    max_shared_error, shared_count = 0., 0
    for entries in groups.values():
        if len({ti for ti, vi in entries}) < 2:
            continue
        shared_count += 1
        ti, vi = entries[0]
        reference = tiles[ti]['moved'][vi]
        for tj, vj in entries[1:]:
            max_shared_error = max(max_shared_error, float(np.linalg.norm(reference-tiles[tj]['moved'][vj])))
    assert max_shared_error < 1.e-10, '타일 공유정점 방향 불일치'
    return tiles, dict(shared_vertex_count=shared_count,
                       maximum_shared_direction_error=max_shared_error,
                       locked_boundary_vertices=len(locked), local_backoff_iterations=iterations,
                       preserved_zero_inset_vertices=len(preserved_zero_inset),
                       reduced_coast_vertices=sum(value < 1. for value in factors.values()))


def _coast(item):
    vertices, uv, faces, lookup = [], [], [], {}
    rings = [(0., 0., 0.)] + [(1., 1.-math.cos(t), CONFIG['round_depth']*(1.-math.sin(t)))
                             for t in np.linspace(0., math.pi/2, CONFIG['coast_round_segments']+1)]
    for a, b, oa, ob in item['edges']:
        pair_rings = []
        for ring_index, (weight, blend, depth) in enumerate(rings):
            pair = []
            for index, outer in ((a, oa), (b, ob)):
                key = (index, ring_index)
                if key not in lookup:
                    lookup[key] = len(vertices)
                    if ring_index == len(rings)-1:
                        # 윗면과 같은 float32 좌표를 직접 사용: 연결 오차 0.
                        position = item['vertices'][index]
                    else:
                        direction = _unit(outer*(1.-blend)+item['moved'][index]*blend)
                        radius = 4998. if ring_index == 0 else item['radius'][index]-depth
                        position = direction*radius
                    vertices.append(tuple(position))
                    uv.append((float(item['h'][index]), 1.-weight))
                pair.append(lookup[key])
            pair_rings.append(pair)
        for lower, upper in zip(pair_rings, pair_rings[1:]):
            faces.extend(((lower[0], upper[1], lower[1]), (lower[0], upper[0], upper[1])))
    return vertices, faces, [uv[v] for face in faces for v in face]


def build():
    _guard_outputs()
    assert NAME not in bpy.data.scenes, '기존 v002 씬은 재개 상태를 먼저 확인'
    for tile in TILES:
        for kind in KINDS:
            assert 'GEO-'+_name(tile, kind) not in bpy.data.objects
            assert _name(tile, kind)+'_Mesh' not in bpy.data.meshes
    assert json.loads((OUT/'p02_ue_import.json').read_text(encoding='utf-8'))['status'] == 'passed'
    tiles, validation = _prepare()
    previous_scenes = list(bpy.data.scenes.keys())
    scene = bpy.data.scenes.new(NAME)
    bpy.context.window.scene = scene
    scene.unit_settings.scale_length = .01
    report = dict(status='complete', config=CONFIG, tiles=[], validation=validation,
                  preserved_scenes=previous_scenes, world_expanded=False,
                  limits='지역 절단면 열림. 전세계 완성본 아님. 원본 해안 외곽과 기존 타일 보존.')
    for item in tiles:
        tile = item['tile']
        _object(scene, _name(tile, 'Top'), item['vertices'].tolist(), item['faces'].tolist(), item['uv'])
        cv, cf, cuv = _coast(item)
        _object(scene, _name(tile, 'Coast'), cv, cf, cuv)
        bottom_path = ART/'P02/Inputs'/f'SM_Diorama_{tile:02d}_Bottom.obj'
        bv, bu, bf = parse_obj(bottom_path)
        bottom = _object(scene, _name(tile, 'Bottom'), bv,
                         [[v for v, u in face] for face in bf], [bu[u] for face in bf for v, u in face])
        bottom.hide_set(True, view_layer=scene.view_layers[0])
        widths = [float(np.linalg.norm(item['moved'][i]-outer)*5000.)
                  for a, b, oa, ob in item['edges'] for i, outer in ((a, oa), (b, ob))]
        report['tiles'].append(dict(tile=tile, coast_edges=len(item['edges']),
            triangles=len(item['faces'])+len(cf)+len(bf),
            minimum_top_outward_dot=float(item['sign'].min()),
            min_area_ratio=float(item['area_ratio'].min()),
            coast_width_uu_percentiles=np.percentile(widths, [0, 50, 90, 100]).tolist(),
            top_coast_join_max_error_uu=0.,
            source_top_sha256=hashlib.sha256(item['path'].read_bytes()).hexdigest(),
            height_above_sea_range=[float(item['radius'].min()-5000.), float(item['radius'].max()-5000.)]))
    scene['bright_globe_config'] = json.dumps(CONFIG)
    scene['bright_p03_v002_build_complete'] = True
    if bpy.context.screen:
        for area in bpy.context.screen.areas:
            if area.type == 'VIEW_3D':
                space = area.spaces.active
                space.clip_start, space.clip_end = 10., 200000.
                space.region_3d.view_location = (0, 0, 0)
                space.region_3d.view_distance = 12000.
                space.region_3d.view_rotation = Vector((.78, .15, .6)).to_track_quat('Z', 'Y')
    OUT.mkdir(parents=True, exist_ok=True)
    with REPORT.open('x', encoding='utf-8') as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2)
    print('P03_V002_BUILD_OK', json.dumps(validation))
    return report


def export():
    scene = bpy.context.scene
    assert scene.name == NAME and scene.get('bright_p03_v002_build_complete'), 'v002 build 검증 먼저 필요'
    assert REPORT.exists() and not BLEND.exists(), '보고서 누락 또는 기존 blend 덮어쓰기 차단'
    objects = [obj for obj in scene.objects if 'bright_p03_v002_export' in obj]
    expected = {_name(tile, kind) for tile in TILES for kind in KINDS}
    assert {obj['bright_p03_v002_export'] for obj in objects} == expected
    paths = [EXPORTS/(obj['bright_p03_v002_export']+'.obj') for obj in objects]
    assert all(not path.exists() for path in paths), '부분 export 재개는 검토 후 별도로 처리'
    EXPORTS.mkdir(parents=True, exist_ok=True)
    BLEND.parent.mkdir(parents=True, exist_ok=True)
    selected = list(bpy.context.selected_objects)
    active = bpy.context.view_layer.objects.active
    try:
        for obj, path in zip(objects, paths):
            hidden = obj.hide_get()
            obj.hide_set(False)
            try:
                for other in scene.objects:
                    other.select_set(False)
                obj.select_set(True)
                bpy.context.view_layer.objects.active = obj
                bpy.ops.wm.obj_export(filepath=str(path), export_selected_objects=True,
                    forward_axis='Y', up_axis='Z', global_scale=1., apply_modifiers=False,
                    export_uv=True, export_normals=True, export_materials=False,
                    export_triangulated_mesh=False, check_existing=False)
            finally:
                obj.hide_set(hidden)
    finally:
        for obj in scene.objects:
            obj.select_set(obj in selected)
        bpy.context.view_layer.objects.active = active
    # copy=True로 현재 라이브 파일 경로도 보존한다. 기존 이미지 pack 상태는 건드리지 않는다.
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND), check_existing=False, copy=True)
    print('P03_V002_EXPORTED', str(BLEND))
