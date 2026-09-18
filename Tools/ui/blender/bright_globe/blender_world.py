"""P05 전 세계: 승인된 v002 해안을 전체 원본 8타일에 적용한다.

일반 Python: prepare()로 공유 경계를 한 번에 계산하고 NPZ 캐시를 만든다.
Blender: build_tile(0..7), export_tile(0..7), save()를 별도 호출한다.
호출만으로 실행되지 않는다. 기존 시험본·원본·Content는 수정하지 않는다.
"""
from pathlib import Path
import hashlib
import json
import math
import os

import numpy as np

ROOT = Path(__file__).resolve().parents[4]
SOURCE = ROOT / 'SourceArt/UI/Atlas/DioramaWorld_v1'
ART = ROOT / 'SourceArt/UI/Atlas/BrightGlobe_v1'
OUT = ROOT / 'Saved/BrightGlobe'
CACHE = OUT / 'P05v001'
EXPORTS = ART / 'P05/Exports'
REPORT = OUT / 'p05_geometry.json'
BLEND = ART / 'Blender/FF_BrightGlobe_P05_world_v001.blend'
NAME = 'FF_BrightGlobe_P05_World_v001'
TILES = tuple(range(8))
KINDS = ('Top', 'Coast', 'Bottom')
CONFIG = dict(radius=5000., land_thickness=40., mountain_height=650.,
              height_floor=.008, height_power=1.15, round_depth=12.,
              coast_width_target_uu=10., coast_width_max_uu=12.,
              source_inset_preserve_below_uu=.01,
              minimum_projected_area_ratio=.15, coast_round_segments=6,
              scope_lon_lat=[-180., 180., -90., 90.], version='p05-v001')


def name(tile, kind):
    assert tile in TILES and kind in KINDS
    return f'SM_BrightWorld_{tile:02d}_{kind}'


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def unit(values):
    return values / np.linalg.norm(values, axis=-1, keepdims=True)


def shaped(height):
    return np.maximum(0., (np.asarray(height)-CONFIG['height_floor']) /
                      (1.-CONFIG['height_floor']))**CONFIG['height_power']


def normals(vertices, faces):
    a, b, c = (vertices[faces[:, i]] for i in range(3))
    return np.cross(b-a, c-a)


def sign(vertices, faces):
    return np.einsum('ij,ij->i', normals(vertices, faces), vertices[faces[:, 0]])


def read_obj(path):
    """원본의 정점당 UV를 읽는다. import 시 Blender는 필요하지 않다."""
    positions, uv, faces, uv_faces = [], [], [], []
    with Path(path).open(encoding='utf-8') as source:
        for line in source:
            if line.startswith('v '):
                positions.append(line[2:])
            elif line.startswith('vt '):
                uv.append(line[3:])
            elif line.startswith('f '):
                words = line.split()[1:]
                assert len(words) == 3, '삼각형 계약 필요'
                parts = [word.split('/') for word in words]
                faces.extend(int(part[0])-1 for part in parts)
                uv_faces.extend(int(part[1])-1 for part in parts)
    vertices = np.fromstring(' '.join(positions), sep=' ', dtype=np.float64).reshape(-1, 3)
    texcoords = np.fromstring(' '.join(uv), sep=' ', dtype=np.float64).reshape(-1, 2)
    faces = np.asarray(faces, dtype=np.int32).reshape(-1, 3)
    uv_faces = np.asarray(uv_faces, dtype=np.int32).reshape(-1, 3)
    assert np.isfinite(vertices).all() and np.isfinite(texcoords).all()
    return vertices, texcoords, faces, uv_faces


def _source(tile, kind):
    return SOURCE / f'SM_Diorama_{tile:02d}_{kind}.obj'


def _fingerprint():
    paths = [_source(tile, kind) for tile in TILES for kind in KINDS]
    paths.append(SOURCE / 'T_Diorama_Height.png')
    sources = {str(path.relative_to(ROOT)): digest(path) for path in paths}
    value = dict(config=CONFIG, source_sha256=sources,
                 generator_sha256=digest(Path(__file__)))
    value['fingerprint'] = hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()
    return value


def _atomic_json(path, value):
    temporary = path.with_suffix(path.suffix+'.partial')
    # 이 스크립트가 생성하는 단일 임시 파일만 교체하며 디렉터리 삭제는 하지 않는다.
    temporary.write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding='utf-8')
    os.replace(temporary, path)


def _row_keys(array):
    """반올림된 원본 좌표의 정확한 정체성. +0/-0은 같은 극점이다."""
    value = np.asarray(array, dtype=np.float64).copy()
    value[value == 0.] = 0.
    return np.ascontiguousarray(value).view(np.dtype((np.void, value.dtype.itemsize*3))).ravel()


def _match_positions(reference, query):
    keys, wanted = _row_keys(reference), _row_keys(query)
    order = np.argsort(keys, kind='stable')
    indices = np.searchsorted(keys[order], wanted)
    assert np.all(indices < len(order)) and np.all(keys[order[indices]] == wanted), '원본 Top/Coast 연결 좌표 누락'
    return order[indices]


def _prepare_global():
    """원본 전체 Top을 함께 풀어 모든 타일의 공통 정점 이동을 동일하게 한다."""
    tiles = []
    for tile in TILES:
        pos, uv, faces, uv_faces = read_obj(_source(tile, 'Top'))
        assert len(uv) == len(pos) and np.array_equal(faces, uv_faces), '원본 Top 정점 UV 계약 변경'
        cv, _, _, _ = read_obj(_source(tile, 'Coast'))
        assert len(cv) % 8 == 0, '원본 해안 8정점 계약 변경'
        pairs = _match_positions(pos, cv.reshape(-1, 8, 3)[:, [7, 6], :].reshape(-1, 3)).reshape(-1, 2)
        outer = unit(cv.reshape(-1, 8, 3)[:, [0, 1], :].reshape(-1, 3)).reshape(-1, 2, 3)
        direction = unit(pos)
        h = np.maximum(0., (np.linalg.norm(pos, axis=1)-5100.)/350.)
        radius = 5040.+650.*shaped(h)
        baseline = (direction*radius[:, None]).astype(np.float32).astype(np.float64)
        baseline_sign = sign(baseline, faces)
        assert baseline_sign.min() > 0., ('원본 Top float32 방향 확인 필요', tile, float(baseline_sign.min()))
        tiles.append(dict(tile=tile, pos=pos, uv=uv, faces=faces, direction=direction,
                          h=h, radius=radius, pairs=pairs, outer=outer, baseline_sign=baseline_sign))
        print('P05_SOURCE_READ', tile, len(pos), len(faces), flush=True)
    sizes = [len(item['pos']) for item in tiles]
    positions = np.concatenate([item['pos'] for item in tiles])
    # 배열 기반 정체성으로 수백만 Python tuple/dict 항목을 만들지 않는다.
    _, first, inverse, counts = np.unique(_row_keys(positions), return_index=True,
                                        return_inverse=True, return_counts=True)
    global_direction = unit(positions[first])
    del positions
    offsets = np.r_[0, np.cumsum(sizes)]
    shortest = np.full(len(first), np.inf)
    outer_direction = np.zeros((len(first), 3))
    has_outer = np.zeros(len(first), dtype=bool)
    locked = np.zeros(len(first), dtype=bool)
    tile_presence = np.zeros(len(first), dtype=np.uint16)
    for ti, item in enumerate(tiles):
        ids = inverse[offsets[ti]:offsets[ti+1]]
        item['global_ids'] = ids
        np.bitwise_or.at(tile_presence, ids, np.uint16(1 << ti))
        faces = item['faces']
        edge_pairs = np.concatenate([faces[:, [i, (i+1)%3]] for i in range(3)])
        edge_pairs.sort(axis=1)
        unique_edges, edge_counts = np.unique(edge_pairs, axis=0, return_counts=True)
        lengths = np.linalg.norm((item['direction'][unique_edges[:, 0]]-
                                  item['direction'][unique_edges[:, 1]])*5000., axis=1)
        np.minimum.at(shortest, ids[unique_edges[:, 0]], lengths)
        np.minimum.at(shortest, ids[unique_edges[:, 1]], lengths)
        # 해안 원본이 포함하지 않는 열린 경계는 이동시키지 않는다.
        coast_pairs = np.sort(item['pairs'], axis=1)
        edge_code = unique_edges[:, 0].astype(np.int64)*len(ids)+unique_edges[:, 1]
        coast_code = coast_pairs[:, 0].astype(np.int64)*len(ids)+coast_pairs[:, 1]
        boundary = unique_edges[(edge_counts == 1) & ~np.isin(edge_code, coast_code)]
        locked[ids[boundary.ravel()]] = True
        cids, outer = ids[item['pairs'].ravel()], item['outer'].reshape(-1, 3)
        # 중복 endpoint도 하나의 원본 해안 방향인지 확인한다.
        unique, index, inv = np.unique(cids, return_index=True, return_inverse=True)
        assert np.linalg.norm(outer-outer[index][inv], axis=1).max() < 1.e-8
        existing = has_outer[unique]
        if existing.any():
            assert np.linalg.norm(outer_direction[unique[existing]]-outer[index[existing]], axis=1).max() < 1.e-8
        outer_direction[unique] = outer[index]
        has_outer[unique] = True
    shared = (tile_presence & (tile_presence-1)) != 0
    source_ids = np.flatnonzero(has_outer)
    original, outer = global_direction[source_ids], outer_direction[source_ids]
    dot = np.einsum('ij,ij->i', original, outer)
    tangent = original-outer*dot[:, None]
    tangent_length = np.linalg.norm(tangent, axis=1)
    source_width = np.arctan2(tangent_length, dot)*5000.
    preserved = source_width < CONFIG['source_inset_preserve_below_uu']
    active = ~preserved & ~locked[source_ids]
    candidate = source_ids[active]
    width = np.maximum(source_width[active], np.minimum(
        min(CONFIG['coast_width_target_uu'], CONFIG['coast_width_max_uu']), shortest[candidate]*2.))
    angle = width/5000.
    target = outer[active]*np.cos(angle[:, None])+unit(tangent[active])*np.sin(angle[:, None])
    factors = np.ones(len(candidate))
    candidate_index = np.full(len(first), -1, dtype=np.int32)
    candidate_index[candidate] = np.arange(len(candidate), dtype=np.int32)
    iterations = 0
    while True:
        moved = global_direction.copy()
        moved[candidate] = unit(global_direction[candidate]+(target-global_direction[candidate])*factors[:, None])
        bad = []
        for item in tiles:
            item['moved'] = moved[item['global_ids']]
            item['vertices'] = (item['moved']*item['radius'][:, None]).astype(np.float32)
            item['sign'] = sign(item['vertices'].astype(np.float64), item['faces'])
            item['ratio'] = item['sign']/item['baseline_sign']
            mask = item['ratio'] < CONFIG['minimum_projected_area_ratio']
            if mask.any():
                bad.append(item['global_ids'][item['faces'][mask].ravel()])
        if not bad:
            break
        affected = np.unique(candidate_index[np.concatenate(bad)])
        affected = affected[affected >= 0]
        assert len(affected), '원본 고정 경계의 면 방향 확인 필요'
        iterations += 1
        assert iterations <= 40, '전 세계 국소 backoff 수렴 실패'
        factors[affected] = factors[affected]*.5 if iterations < 25 else 0.
        print('P05_BACKOFF', iterations, len(affected), flush=True)
    # 공통 좌표는 같은 높이/방향으로 만들어지므로 float32 결과도 동일해야 한다.
    maximum_shared_error = 0.
    expected = np.zeros((len(first), 3), dtype=np.float32)
    seen = np.zeros(len(first), dtype=bool)
    for item in tiles:
        ids, vertices = item['global_ids'], item['vertices']
        present = seen[ids]
        if present.any():
            maximum_shared_error = max(maximum_shared_error,
                float(np.linalg.norm(vertices[present]-expected[ids[present]], axis=1).max()))
        expected[ids] = vertices
        seen[ids] = True
        item['shared'] = shared[ids]
    assert maximum_shared_error == 0., '타일 공유 위치 불일치'
    return tiles, dict(shared_vertex_count=int(shared.sum()),
        maximum_shared_position_error_uu=maximum_shared_error,
        locked_boundary_vertices=int(locked.sum()), local_backoff_iterations=iterations,
        preserved_zero_inset_vertices=int(preserved.sum()),
        reduced_coast_vertices=int(np.count_nonzero(factors < 1.)))


def _coast(item):
    endpoint_indices, index, inverse = np.unique(item['pairs'].ravel(), return_index=True, return_inverse=True)
    outer = item['outer'].reshape(-1, 3)[index]
    top_direction = item['moved'][endpoint_indices]
    rings = CONFIG['coast_round_segments']+2
    positions = np.empty((len(endpoint_indices), rings, 3), dtype=np.float32)
    positions[:, 0] = outer*4998.
    for ring, angle in enumerate(np.linspace(0., math.pi/2., rings-1), start=1):
        direction = unit(outer*math.cos(angle)+top_direction*(1.-math.cos(angle)))
        positions[:, ring] = direction*(item['radius'][endpoint_indices]-
                                        CONFIG['round_depth']*(1.-math.sin(angle)))[:, None]
    positions[:, -1] = item['vertices'][endpoint_indices]
    uv = np.zeros((len(endpoint_indices), rings, 2), dtype=np.float64)
    uv[:, :, 0] = item['h'][endpoint_indices, None]
    uv[:, 0, 1] = 1.
    pair = inverse.reshape(-1, 2)*rings
    lower = pair[:, None, :]+np.arange(rings-1)[None, :, None]
    upper = lower+1
    faces = np.stack([np.stack([lower[:, :, 0], upper[:, :, 1], lower[:, :, 1]], axis=-1),
                      np.stack([lower[:, :, 0], upper[:, :, 0], upper[:, :, 1]], axis=-1)], axis=2).reshape(-1, 3)
    positions, uv = positions.reshape(-1, 3), uv.reshape(-1, 2)
    # 극점에 모이는 동일 좌표 endpoint는 원본도 면을 제거한다. Top 면은 변경하지 않는다.
    valid = np.linalg.norm(normals(positions.astype(np.float64), faces), axis=1) > 1.e-6
    removed = int(np.count_nonzero(~valid))
    faces = faces[valid]
    used, inverse = np.unique(faces, return_inverse=True)
    joins = used % rings == rings-1
    return dict(vertices=positions[used], uv=uv[used], faces=inverse.reshape(-1, 3).astype(np.int32),
                join_coast=np.flatnonzero(joins), join_top=endpoint_indices[used[joins]//rings],
                removed_zero_area_coast_faces=removed)


def prepare():
    """CPU 전용. 새 경로에 계산 캐시를 저장한다. Blender/UE는 호출하지 않는다."""
    signature = _fingerprint()
    CACHE.mkdir(parents=True, exist_ok=True)
    manifest = CACHE/'manifest.json'
    if manifest.exists():
        saved = json.loads(manifest.read_text(encoding='utf-8'))
        assert saved['fingerprint'] == signature['fingerprint'], '원본/스크립트/설정 변경: 캐시 재사용 금지'
        if saved.get('status') == 'prepared':
            assert all(digest(CACHE/entry['file']) == entry['sha256'] for entry in saved['cache_files'])
            return saved
    else:
        assert not REPORT.exists() and not BLEND.exists(), '기존 전 세계 결과 덮어쓰기 금지'
        _atomic_json(manifest, dict(signature, status='preparing'))
    tiles, validation = _prepare_global()
    records, cache_files = [], []
    for item in tiles:
        tile = item['tile']
        coast = _coast(item)
        bv, buv, bf, buf = read_obj(_source(tile, 'Bottom'))
        assert len(bv) == len(buv) and np.array_equal(bf, buf)
        arrays = dict(top_vertices=item['vertices'], top_faces=item['faces'], top_uv=item['uv'],
                      top_source_vertices=item['pos'], top_shared=item['shared'],
                      coast_vertices=coast['vertices'], coast_faces=coast['faces'], coast_uv=coast['uv'],
                      join_coast=coast['join_coast'], join_top=coast['join_top'],
                      bottom_vertices=bv.astype(np.float32), bottom_faces=bf, bottom_uv=buv)
        target = CACHE/f'tile_{tile:02d}.npz'
        temporary = target.with_suffix('.npz.partial')
        with temporary.open('wb') as stream:
            np.savez_compressed(stream, **arrays)
        if target.exists():
            # 준비 단계 중 중단된 경우 이전 완료 타일이 수치상 동일한지 확인한다.
            with np.load(target) as old:
                assert all(np.array_equal(old[key], value) for key, value in arrays.items()), '부분 캐시 불일치'
        os.replace(temporary, target)
        cache_files.append(dict(file=target.name, sha256=digest(target)))
        widths = np.linalg.norm(item['moved'][item['pairs']]-item['outer'], axis=2).ravel()*5000.
        records.append(dict(tile=tile, top_triangles=len(item['faces']), coast_triangles=len(coast['faces']),
                            bottom_triangles=len(bf), joined_vertices=len(coast['join_top']),
                            minimum_top_outward_dot=float(item['sign'].min()),
                            minimum_projected_area_ratio=float(item['ratio'].min()),
                            coast_width_uu_percentiles=np.percentile(widths, [0, 50, 90, 100]).tolist(),
                            removed_zero_area_coast_faces=coast['removed_zero_area_coast_faces'],
                            height_above_sea_range=[float(item['radius'].min()-5000.), float(item['radius'].max()-5000.)]))
        print('P05_TILE_CACHED', tile, flush=True)
    report = dict(status='prepared', config=CONFIG, fingerprint=signature['fingerprint'],
                  validation=validation, tiles=records, world_expanded=True,
                  blender_built=False, exported=False, ue_imported=False, render_verified=False,
                  limits='전 세계 형상 계산 캐시. Blender 저장·UE 임포트·성능/화면 검증은 별도 단계.')
    _atomic_json(REPORT, report)
    result = dict(signature, status='prepared', cache_files=cache_files)
    _atomic_json(manifest, result)
    return report


def _ready():
    result = json.loads((CACHE/'manifest.json').read_text(encoding='utf-8'))
    assert result['status'] == 'prepared' and result['config'] == CONFIG
    assert result['generator_sha256'] == digest(Path(__file__)), '준비 후 생성 스크립트 변경'
    return result


def load_tile(tile):
    assert tile in TILES
    manifest = _ready()
    entry = next(entry for entry in manifest['cache_files'] if entry['file'] == f'tile_{tile:02d}.npz')
    path = CACHE/entry['file']
    assert digest(path) == entry['sha256'], '캐시 해시 불일치'
    with np.load(path) as data:
        return {key: data[key] for key in data.files}


def build_tile(tile):
    """라이브 Blender에서 한 타일씩 호출. 다른 씬과 파일은 그대로 둔다."""
    import bpy
    arrays = load_tile(tile)
    scene = bpy.data.scenes.get(NAME)
    if scene is None:
        scene = bpy.data.scenes.new(NAME)
        scene.unit_settings.scale_length = .01
        scene['bright_globe_config'] = json.dumps(CONFIG)
    assert scene.get('bright_globe_config') == json.dumps(CONFIG)
    bpy.context.window.scene = scene
    for kind in KINDS:
        asset = name(tile, kind)
        existing = bpy.data.objects.get('GEO-'+asset)
        if existing:
            assert existing.name in scene.objects and existing.get('bright_p05_v001_complete'), '불완전 오브젝트 직접 확인 필요'
            continue
        assert asset+'_Mesh' not in bpy.data.meshes, '불완전 메시 직접 확인 필요'
        prefix = kind.lower()
        vertices, faces, uv = (arrays[prefix+'_'+key] for key in ('vertices', 'faces', 'uv'))
        mesh = bpy.data.meshes.new(asset+'_Mesh')
        mesh.vertices.add(len(vertices))
        mesh.vertices.foreach_set('co', np.asarray(vertices, dtype=np.float32).ravel())
        mesh.loops.add(faces.size)
        mesh.loops.foreach_set('vertex_index', faces.ravel())
        mesh.polygons.add(len(faces))
        mesh.polygons.foreach_set('loop_start', np.arange(len(faces), dtype=np.int32)*3)
        mesh.polygons.foreach_set('loop_total', np.full(len(faces), 3, dtype=np.int32))
        mesh.polygons.foreach_set('use_smooth', np.ones(len(faces), dtype=bool))
        mesh.update(calc_edges=True)
        layer = mesh.uv_layers.new(name='UVMap')
        layer.data.foreach_set('uv', np.asarray(uv[faces], dtype=np.float32).ravel())
        material = bpy.data.materials.get('MAT-BrightP03v2-'+kind)
        if material:
            mesh.materials.append(material)
        obj = bpy.data.objects.new('GEO-'+asset, mesh)
        scene.collection.objects.link(obj)
        obj['bright_p05_v001_export'] = asset
        obj['bright_p05_v001_complete'] = True
        if kind == 'Bottom':
            obj.hide_set(True, view_layer=scene.view_layers[0])
    scene['bright_p05_v001_tile_'+str(tile)] = True
    if bpy.context.screen:
        for area in bpy.context.screen.areas:
            if area.type == 'VIEW_3D':
                area.spaces.active.clip_start = 10.
                area.spaces.active.clip_end = 200000.
    print('P05_TILE_BUILT', tile)


def export_tile(tile):
    """기존 파일은 export 기록 해시와 같을 때만 건너뛴다."""
    import bpy
    _ready()
    scene = bpy.context.scene
    assert scene.name == NAME and scene.get('bright_p05_v001_tile_'+str(tile))
    EXPORTS.mkdir(parents=True, exist_ok=True)
    manifest_path = CACHE/'exports.json'
    records = json.loads(manifest_path.read_text(encoding='utf-8')) if manifest_path.exists() else {}
    selected, active = list(bpy.context.selected_objects), bpy.context.view_layer.objects.active
    try:
        for kind in KINDS:
            asset = name(tile, kind)
            path = EXPORTS/(asset+'.obj')
            if path.exists():
                assert asset in records and digest(path) == records[asset]['sha256'], '미확인 export 파일 덮어쓰기 금지'
                continue
            obj = bpy.data.objects['GEO-'+asset]
            hidden = obj.hide_get()
            try:
                obj.hide_set(False)
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
            records[asset] = dict(sha256=digest(path), bytes=path.stat().st_size,
                                  triangles=len(obj.data.polygons))
            _atomic_json(manifest_path, records)
    finally:
        for obj in scene.objects:
            obj.select_set(obj in selected)
        bpy.context.view_layer.objects.active = active
    print('P05_TILE_EXPORTED', tile)


def save(checkpoint=False):
    """체크포인트는 완성 타일 개수별 새 파일, 최종본은 24개 확인 후 1회 저장."""
    import bpy
    scene = bpy.context.scene
    assert scene.name == NAME
    tiles = [tile for tile in TILES if scene.get('bright_p05_v001_tile_'+str(tile))]
    path = BLEND.with_stem(BLEND.stem+f'_checkpoint_{len(tiles):02d}') if checkpoint else BLEND
    if not checkpoint:
        assert len(tiles) == 8
        expected = {'GEO-'+name(tile, kind) for tile in TILES for kind in KINDS}
        assert expected <= {obj.name for obj in scene.objects}
    assert not path.exists(), '기존 blend 덮어쓰기 금지'
    path.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.file.pack_all()
    bpy.ops.wm.save_as_mainfile(filepath=str(path), check_existing=False, copy=checkpoint)
    if not checkpoint:
        report = json.loads(REPORT.read_text(encoding='utf-8'))
        report['blender_built'] = True
        report['blend_file'] = str(path)
        report['exported'] = all((EXPORTS/(name(tile, kind)+'.obj')).is_file() for tile in TILES for kind in KINDS)
        _atomic_json(REPORT, report)
    print('P05_BLEND_SAVED', str(path))


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare', action='store_true', help='명시적으로 전 세계 CPU 계산 시작')
    arguments = parser.parse_args()
    if arguments.prepare:
        prepare()
    else:
        parser.print_help()
