"""P05 CPU 검사: 실제 OBJ 왕복, 극점·타일 접합, 조절 3종의 Top/Coast 계약.

--cache-only는 Blender 실행 전 형상 캐시만 검사하며 export 합격으로 간주하지 않는다.
--tile N으로 타일별 재개하고 --finalize로 8개 결과를 합친다.
"""
from pathlib import Path
import argparse
import json

import numpy as np
from PIL import Image

import blender_world as world
from validate_refined_geometry import sample_height
from validate_roundtrip import validate as slow_validate

CASES = ((40., 650.), (75., 650.), (40., 1000.))
REPORT = world.OUT/'p05_validation.json'
JOIN_TOL = .05
UV_TOL = 1.1e-6


def _metrics(vertices, faces):
    normal = world.normals(vertices, faces)
    lengths = np.stack([np.linalg.norm(vertices[faces[:, (i+1)%3]]-
                                       vertices[faces[:, i]], axis=1) for i in range(3)], axis=1)
    area = np.linalg.norm(normal, axis=1)
    return dict(triangles=len(faces), degenerate_triangles=int(np.count_nonzero(area <= 1.e-6)),
                short_edge_triangles=int(np.count_nonzero(lengths.min(axis=1) <= 2.e-5)),
                minimum_double_area_uu2=float(area.min()), minimum_edge_uu=float(lengths.min()))


def _legacy_data(vertices, uv, faces, uv_faces):
    return (list(map(tuple, vertices)), list(map(tuple, uv)),
            [tuple(zip(map(int, f), map(int, u))) for f, u in zip(faces, uv_faces)])


def _check_export(tile, kind, arrays):
    path = world.EXPORTS/(world.name(tile, kind)+'.obj')
    actual = world.read_obj(path)
    prefix = kind.lower()
    expected_vertices, expected_uv, expected_faces = (arrays[prefix+'_'+key] for key in ('vertices', 'uv', 'faces'))
    vertices, uv, faces, uv_faces = actual
    direct = len(vertices) == len(expected_vertices) and np.max(np.abs(vertices-expected_vertices)) <= .002
    if direct and np.array_equal(faces, expected_faces):
        error = float(np.max(np.abs(uv[uv_faces]-expected_uv[expected_faces])))
        assert error <= UV_TOL, ('면 코너 UV 불일치', tile, kind, error)
        result = dict(status='passed', method='좌표 순서 직접 대조 + 동일 방향 면 + 모든 코너 UV',
                      max_position_error_uu=float(np.linalg.norm(vertices-expected_vertices, axis=1).max()),
                      max_uv_error=error, triangles=len(faces), oriented_faces_and_corner_uv_equal=True)
    else:
        # Blender 버전/OBJ 작성 순서가 바뀌어도 좌표와 UV로 방향 있는 면을 대조한다.
        result = slow_validate(_legacy_data(expected_vertices, expected_uv, expected_faces, expected_faces),
                               _legacy_data(vertices, uv, faces, uv_faces))
        result['method'] = '공간 bucket 기반 좌표/UV 대응과 방향 있는 면 대조'
    result['sha256'] = world.digest(path)
    return result, actual


def _vertex_uv(vertices, uv, faces, uv_faces):
    values = np.full((len(vertices), 2), np.nan)
    values[faces.ravel()] = uv[uv_faces.ravel()]
    error = np.abs(values[faces]-uv[uv_faces])
    assert np.max(error) <= UV_TOL, '한 정점의 UV 분기: WPO 검증 매핑 확인 필요'
    # 극점의 사용하지 않는 원본 정점은 렌더/검사에 쓰이지 않으므로 NaN 허용.
    return values


def _join_by_position(top, coast, weight, used_top):
    """실제 export에서 weight=1인 접합 좌표를 찾는다. 모서리 위쪽 링만 동일하다."""
    tkeys, ckeys = world._row_keys(top), world._row_keys(coast)
    order = used_top[np.argsort(tkeys[used_top], kind='stable')]
    index = np.searchsorted(tkeys[order], ckeys)
    inside = index < len(order)
    matches = np.flatnonzero(inside)
    matches = matches[tkeys[order[index[matches]]] == ckeys[matches]]
    matches = matches[np.isclose(weight[matches], 1.)]
    assert len(matches), '실제 Top/Coast의 동일 좌표 접합 없음'
    return matches, order[index[matches]]


def _audit_normal_rotations(before, after, faces, weight, indices):
    """기준 법선과 90도 이상 달라져도 방사 높이장 면은 뒤집힌 것이 아니다.

    각 정점은 동일한 양의 방사선 위에서 이동하므로 dot(cross(b-a,c-a),a)는
    세 양의 반지름과 고정된 세 방향의 determinant의 곱이다. 두꺼운 어깨의
    이 부호가 양수라면 전체 선형 조절 구간에서도 양수다. 추가로 법선 길이
    제곱(4차식)의 미분근을 풀어 중간 시점에 면적 0으로 붕괴하지 않는지 검사한다.
    옆벽/기타 미확인 사례에는 이 예외를 적용하지 않는다.
    """
    records, unresolved = [], []
    for face_index in indices:
        ids = faces[face_index]
        p, delta = before[ids], after[ids]-before[ids]
        edge1, edge2 = p[1]-p[0], p[2]-p[0]
        de1, de2 = delta[1]-delta[0], delta[2]-delta[0]
        a = np.cross(edge1, edge2)
        b = np.cross(de1, edge2)+np.cross(edge1, de2)
        c = np.cross(de1, de2)
        polynomial = [4.*(c@c), 6.*(b@c), 2.*((b@b)+2.*(a@c)), 2.*(a@b)]
        polynomial = np.trim_zeros(polynomial, 'f')
        roots = np.roots(polynomial) if len(polynomial) > 1 else []
        times = [0., 1.]+[float(root.real) for root in roots
                         if abs(root.imag) < 1.e-7 and 0. < root.real < 1.]
        minimum_area = min(float(np.linalg.norm(a+b*t+c*t*t)) for t in times)
        ray = world.unit(p[0])
        radial_before, radial_after = float(a@ray), float((a+b+c)@ray)
        shoulder = bool(np.all(weight[ids] == 1.))
        positive_radii = bool(np.all(np.einsum('ij,ij->i', p, after[ids]) > 0.))
        safe = shoulder and positive_radii and radial_before > 1.e-7 and radial_after > 1.e-7 and minimum_area > 1.e-6
        records.append(dict(face=int(face_index), verified_as_continuous_shoulder_rotation=safe,
                            radial_signed_area_before=radial_before, radial_signed_area_after=radial_after,
                            minimum_path_double_area_uu2=minimum_area))
        if not safe:
            unresolved.append(int(face_index))
    return records, unresolved


def validate_tile(tile, cache_only=False):
    arrays = world.load_tile(tile)
    source_top, source_uv, source_faces, source_uvfaces = world.read_obj(world._source(tile, 'Top'))
    assert np.array_equal(source_top, arrays['top_source_vertices']), '캐시 원본 좌표 변경'
    assert np.array_equal(source_faces, arrays['top_faces']) and np.array_equal(source_uvfaces, source_faces)
    assert np.array_equal(source_uv, arrays['top_uv']), '원본 Top UV 변경'
    bv, buv, bf, buf = world.read_obj(world._source(tile, 'Bottom'))
    assert np.array_equal(bv.astype(np.float32), arrays['bottom_vertices'])
    assert np.array_equal(buv, arrays['bottom_uv']) and np.array_equal(bf, arrays['bottom_faces']) and np.array_equal(buf, bf)
    vertices = {kind: arrays[kind.lower()+'_vertices'].astype(np.float64) for kind in world.KINDS}
    faces = {kind: arrays[kind.lower()+'_faces'] for kind in world.KINDS}
    uv = {kind: arrays[kind.lower()+'_uv'] for kind in world.KINDS}
    exports = {}
    if not cache_only:
        for kind in world.KINDS:
            check, actual = _check_export(tile, kind, arrays)
            exports[kind] = check
            vertices[kind], actual_uv, faces[kind], actual_uvfaces = actual
            uv[kind] = _vertex_uv(vertices[kind], actual_uv, faces[kind], actual_uvfaces)
    texture = np.asarray(Image.open(world.SOURCE/'T_Diorama_Height.png'), dtype=float)/255.
    used_top = np.unique(faces['Top'])
    htop = np.zeros(len(vertices['Top']))
    htop[used_top] = sample_height(uv['Top'][used_top], texture)
    hcoast = uv['Coast'][:, 0]
    weight = 1.-uv['Coast'][:, 1]
    assert np.isin(weight, [0., 1.]).all(), 'Coast WPO 0/1 계약 오류'
    jc, jt = _join_by_position(vertices['Top'], vertices['Coast'], weight, used_top)
    assert len(jc) == len(arrays['join_coast']), ('join 수 변경', len(jc), len(arrays['join_coast']))
    # 원본 Top 반지름의 산 높이 및 투영 면적을 별도 계산하여 캐시를 확인한다.
    source_h = np.maximum(0., (np.linalg.norm(source_top, axis=1)-5100.)/350.)
    desired_radius = 5040.+650.*world.shaped(source_h)
    baseline = world.unit(source_top)*desired_radius[:, None]
    baseline_sign = world.sign(baseline, source_faces)
    cache_sign = world.sign(arrays['top_vertices'].astype(np.float64), source_faces)
    area_ratio = cache_sign/baseline_sign
    radius_error = np.abs(np.linalg.norm(arrays['top_vertices'], axis=1)-desired_radius)
    assert radius_error.max() < .002 and area_ratio.min() >= .1499 and cache_sign.min() > 0.
    record = dict(tile=tile, status='running', cache_only=cache_only,
        source_top_sha256=world.digest(world._source(tile, 'Top')),
        cache_sha256=world.digest(world.CACHE/f'tile_{tile:02d}.npz'),
        exported=exports, top_oriented_faces_and_uv_unchanged=True, bottom_unchanged=True,
        top_height_radius_max_error_uu=float(radius_error.max()),
        minimum_projected_area_ratio=float(area_ratio.min()), joined_vertices=len(jc),
        height_h_join_max_error=float(np.max(np.abs(htop[jt]-hcoast[jc]))),
        meshes={kind: _metrics(vertices[kind], faces[kind]) for kind in world.KINDS}, cases=[])
    normal_before = world.normals(vertices['Coast'], faces['Coast'])
    for thickness, mountain in CASES:
        tv = vertices['Top']+world.unit(vertices['Top'])*((thickness-40.)+world.shaped(htop)*(mountain-650.))[:, None]
        cv = vertices['Coast']+world.unit(vertices['Coast'])*(((thickness-40.)+world.shaped(hcoast)*(mountain-650.))*weight)[:, None]
        gap = np.linalg.norm(cv[jc]-tv[jt], axis=1)
        top_sign = world.sign(tv, faces['Top'])
        normal_after = world.normals(cv, faces['Coast'])
        dot = np.einsum('ij,ij->i', normal_before, normal_after)
        indices = np.flatnonzero(dot <= 0.)
        rotations, unresolved = _audit_normal_rotations(vertices['Coast'], cv, faces['Coast'], weight, indices)
        record['cases'].append(dict(land_thickness=thickness, mountain_height=mountain,
            maximum_join_gap_uu=float(gap.max()), minimum_top_outward_dot=float(top_sign.min()),
            coast_reversed_faces=len(unresolved), coast_reversed_face_indices=unresolved[:100],
            coast_normal_rotations_over_90_degrees=len(indices), coast_rotation_audit=rotations,
            coast_minimum_normal_cosine=float(np.einsum('ij,ij->i', world.unit(normal_before), world.unit(normal_after)).min()),
            top_metrics=_metrics(tv, faces['Top']), coast_metrics=_metrics(cv, faces['Coast'])))
    issues = []
    for kind, metric in record['meshes'].items():
        if metric['degenerate_triangles'] or metric['short_edge_triangles']:
            issues.append(f'{kind}: 퇴화면 또는 0.00002uu 이하 변')
    for case in record['cases']:
        if case['maximum_join_gap_uu'] > JOIN_TOL or case['minimum_top_outward_dot'] <= 0. or case['coast_reversed_faces']:
            issues.append(f"{case['land_thickness']}/{case['mountain_height']}: 접합/방향 범위 초과")
        if case['top_metrics']['degenerate_triangles'] or case['coast_metrics']['degenerate_triangles']:
            issues.append(f"{case['land_thickness']}/{case['mountain_height']}: 조절 후 퇴화면")
    record['issues'] = issues
    record['status'] = 'failed' if issues else 'passed'
    suffix = '_cache' if cache_only else ''
    world._atomic_json(world.CACHE/f'validation_tile_{tile:02d}{suffix}.json', record)
    print('P05_TILE_VALIDATED', tile, record['status'], 'cache_only', cache_only, flush=True)
    return record


def _verify_shared():
    """일반 경도 경계·날짜 변경선·극점의 동일 위치와 WPO 방향을 확인한다."""
    original, current, heights, tile_ids = [], [], [], []
    texture = np.asarray(Image.open(world.SOURCE/'T_Diorama_Height.png'), dtype=float)/255.
    for tile in world.TILES:
        arrays = world.load_tile(tile)
        shared = arrays['top_shared']
        original.append(arrays['top_source_vertices'][shared])
        current.append(arrays['top_vertices'][shared].astype(np.float64))
        heights.append(sample_height(arrays['top_uv'][shared], texture))
        tile_ids.append(np.full(int(shared.sum()), tile, dtype=np.int32))
    original, current, heights, tile_ids = map(np.concatenate, (original, current, heights, tile_ids))
    _, first, inverse = np.unique(world._row_keys(original), return_index=True, return_inverse=True)
    position_error = np.linalg.norm(current-current[first][inverse], axis=1)
    h_error = np.abs(heights-heights[first][inverse])
    cases = []
    for thickness, mountain in CASES:
        changed = current+world.unit(current)*((thickness-40.)+world.shaped(heights)*(mountain-650.))[:, None]
        error = np.linalg.norm(changed-changed[first][inverse], axis=1)
        cases.append(dict(land_thickness=thickness, mountain_height=mountain,
                          maximum_shared_gap_uu=float(error.max())))
    polar = np.abs(original[:, 2]) / np.linalg.norm(original, axis=1) > 1.-1.e-10
    return dict(shared_source_positions=len(first), shared_instances=len(original),
                pole_instances=int(polar.sum()), maximum_position_error_uu=float(position_error.max()),
                maximum_height_sample_error=float(h_error.max()), cases=cases)


def finalize(cache_only=False):
    suffix = '_cache' if cache_only else ''
    records = [json.loads((world.CACHE/f'validation_tile_{tile:02d}{suffix}.json').read_text(encoding='utf-8')) for tile in world.TILES]
    for record in records:
        assert record['cache_only'] == cache_only
        tile = record['tile']
        assert record['cache_sha256'] == world.digest(world.CACHE/f'tile_{tile:02d}.npz')
        assert record['source_top_sha256'] == world.digest(world._source(tile, 'Top'))
        if not cache_only:
            for kind, data in record['exported'].items():
                assert data['sha256'] == world.digest(world.EXPORTS/(world.name(tile, kind)+'.obj'))
    shared = _verify_shared()
    passed = all(record['status'] == 'passed' for record in records)
    passed &= shared['maximum_position_error_uu'] == 0.
    passed &= all(case['maximum_shared_gap_uu'] <= JOIN_TOL for case in shared['cases'])
    report = dict(status='passed' if passed else 'failed', cache_only=cache_only,
                  world_expanded=True, export_verified=not cache_only,
                  gpu_render_verified=False, ue_import_verified=False,
                  uv_tolerance=UV_TOL, join_tolerance_uu=JOIN_TOL, tiles=records, shared=shared,
                  method='전체 원본 Top 연결/UV 보존 + 실제 OBJ 대조 + PNG bilinear WPO + 타일/극점 공통 위치',
                  coast_orientation_method='기준 법선과 90도 이상 다른 어깨 면은 방사 signed area와 선형 조절 전체 구간의 4차 면적식 최소값으로 연속 회전/붕괴를 구별한다. 미검증 부호 변화와 퇴화는 실패한다.',
                  limits='형상 및 좌표 계약만 검사. 카메라 화면/성능/충돌/UE 적용은 별도 검증 필요.')
    destination = REPORT.with_stem(REPORT.stem+'_cache') if cache_only else REPORT
    world._atomic_json(destination, report)
    print('P05_VALIDATION', report['status'], str(destination), flush=True)
    return report


def run(cache_only=False):
    for tile in world.TILES:
        validate_tile(tile, cache_only=cache_only)
    return finalize(cache_only=cache_only)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache-only', action='store_true')
    parser.add_argument('--tile', type=int, choices=world.TILES)
    parser.add_argument('--finalize', action='store_true')
    args = parser.parse_args()
    if args.tile is not None:
        result = validate_tile(args.tile, args.cache_only)
    elif args.finalize:
        result = finalize(args.cache_only)
    else:
        result = run(args.cache_only)
    raise SystemExit(0 if result['status'] == 'passed' else 1)
