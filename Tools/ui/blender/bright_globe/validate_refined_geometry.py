"""Blender/UE 없이 v002 실제 OBJ·원본 UV·고도 PNG의 접합과 방향을 독립 검증."""
from collections import Counter, defaultdict
from itertools import product
import hashlib
import json
import math

import numpy as np
from PIL import Image
from prepare_roundtrip import ROOT, ART, OUT, parse_obj
from validate_roundtrip import validate, cyclic

CASES = [(40., 650.), (75., 650.), (40., 1000.)]
REPORT = OUT/'p03_v002_validation.json'
EXPORTS = ART/'P03v2/Exports'
UV_TOL = 1.1e-6
JOIN_TOL = .05  # OBJ UV 소수 6자리 반올림에 따른 고도 샘플 오차 포함.


def unit(v):
    return v/np.linalg.norm(v, axis=-1, keepdims=True)


def shaped(h):
    return np.maximum(0., (np.asarray(h)-.008)/.992)**1.15


def sample_height(uv, texture):
    height, width = texture.shape
    x, y = uv[:, 0]*width-.5, (1.-uv[:, 1])*height-.5
    ix, iy = np.floor(x).astype(int), np.floor(y).astype(int)
    tx, ty = x-ix, y-iy
    jx = (ix+1)%width
    ix %= width
    jy = np.clip(iy+1, 0, height-1)
    iy = np.clip(iy, 0, height-1)
    return (texture[iy, ix]*(1-tx)*(1-ty)+texture[iy, jx]*tx*(1-ty)
            +texture[jy, ix]*(1-tx)*ty+texture[jy, jx]*tx*ty)


def vertex_uv(data):
    vertices, uv, faces = data
    values = np.full((len(vertices), 2), np.nan)
    for face in faces:
        for vi, ui in face:
            value = np.asarray(uv[ui])
            if np.isfinite(values[vi]).all():
                assert np.max(np.abs(value-values[vi])) <= UV_TOL, '단일 정점의 UV 불일치'
            values[vi] = value
    assert np.isfinite(values).all()
    return values


def normals(vertices, faces):
    a, b, c = (vertices[faces[:, i]] for i in range(3))
    return np.cross(b-a, c-a)


def metrics(vertices, faces):
    n = normals(vertices, faces)
    double_area = np.linalg.norm(n, axis=1)
    edges = np.stack([vertices[faces[:, (i+1)%3]]-vertices[faces[:, i]] for i in range(3)], axis=1)
    lengths = np.linalg.norm(edges, axis=2)
    return dict(triangles=len(faces), degenerate_triangles=int(np.count_nonzero(double_area <= 1.e-6)),
                short_edge_triangles=int(np.count_nonzero(lengths.min(axis=1) <= 2.e-5)),
                minimum_double_area_uu2=float(double_area.min()), minimum_edge_length_uu=float(lengths.min()))


def source_mapping(source, target):
    """정점 순서를 가정하지 않고 보존된 UV로 원본/출력을 연결한다."""
    source_uv, target_uv = vertex_uv(source), vertex_uv(target)
    buckets = defaultdict(list)
    scale = 2.e-6
    for i, uv in enumerate(source_uv):
        buckets[tuple(np.floor(uv/scale).astype(int))].append(i)
    mapping = []
    for uv in target_uv:
        key = np.floor(uv/scale).astype(int)
        candidates = [i for delta in product((-1, 0, 1), repeat=2)
                      for i in buckets.get(tuple(key+delta), ())
                      if np.max(np.abs(source_uv[i]-uv)) <= UV_TOL]
        assert len(candidates) == 1, ('UV 정점 대응 모호함', uv, candidates)
        mapping.append(candidates[0])
    mapping = np.asarray(mapping)
    assert len(set(mapping)) == len(source[0]) == len(target[0])
    expected = Counter(cyclic(tuple(v for v, u in face)) for face in source[2])
    actual = Counter(cyclic(tuple(int(mapping[v]) for v, u in face)) for face in target[2])
    assert expected == actual, '윗면 연결/방향 변화'
    return mapping, source_uv, target_uv


def coastline_reference(source, exported_top, mapping, tile):
    """원본 해안 지점과 실제 윗면 위치로 단면을 독립 구성한다."""
    original = np.asarray(source[0])
    h = np.maximum(0., (np.linalg.norm(original, axis=1)-5100.)/350.)
    top = np.empty_like(original)
    top[mapping] = np.asarray(exported_top[0])
    source_index = {p: i for i, p in enumerate(source[0])}
    cv, _, _ = parse_obj(ROOT/'SourceArt/UI/Atlas/DioramaWorld_v1'/f'SM_Diorama_{tile:02d}_Coast.obj')
    vertices, uv, faces, exterior = [], [], [], []
    source_indices, joins = [], []
    for start in range(0, len(cv), 8):
        a, b = source_index.get(cv[start+7]), source_index.get(cv[start+6])
        if a is None or b is None:
            continue
        outer = unit(np.asarray([cv[start], cv[start+1]]))
        # 해안 옆면의 외향: 원본 경계 변에 수직이며 방사 축에 수직인 방향.
        lateral = unit(np.cross(unit(outer[0]+outer[1]), outer[1]-outer[0]))
        base = len(vertices)
        for ring in range(8):
            for side, index in enumerate((a, b)):
                if ring == 0:
                    point = outer[side]*4998.
                elif ring == 7:
                    point = top[index]
                    joins.append((len(vertices), index))
                else:
                    angle = (ring-1)/6.*math.pi/2.
                    direction = unit(outer[side]*math.cos(angle)+unit(top[index])*(1.-math.cos(angle)))
                    point = direction*(5040.+650.*shaped(h[index])-12.*(1.-math.sin(angle)))
                vertices.append(tuple(point))
                uv.append((float(h[index]), 1. if ring == 0 else 0.))
                source_indices.append(index)
        for band in range(7):
            k = base+band*2
            faces.extend(((k, k+3, k+1), (k, k+2, k+3)))
            exterior.extend((lateral, lateral))
    return (vertices, uv, [tuple((v, v) for v in face) for face in faces]), np.asarray(exterior), joins


def matched_join(top, coast):
    lookup = {tuple(point): i for i, point in enumerate(top[0])}
    result = [(i, lookup[tuple(point)]) for i, point in enumerate(coast[0]) if tuple(point) in lookup]
    assert len(result) > 100, 'Top/Coast 동일 좌표 접합 부족'
    return np.asarray(result)


def run():
    report = dict(status='running', method='실제 OBJ 독립 재읽기 + 원본 면별 UV/해안 + PNG bilinear CPU WPO',
                  position_tolerance_uu=.002, uv_tolerance=UV_TOL, join_tolerance_uu=JOIN_TOL,
                  gpu_render_verified=False, cases=[], tiles=[])
    report['coast_orientation_method'] = ('원본 경계 변의 winding을 유지하여 실제 Top 위치까지 단면을 재구성하고 '
        '모든 방향 있는 삼각형·코너 UV를 대조한다. 조절 시 기준 법선 대비 반전도 검사한다. '
        '고정 옆방향 dot는 둥근 어깨에서 음수가 가능하므로 진단값이며 단독 합격 기준은 아니다.')
    texture = np.asarray(Image.open(ART/'P02/Inputs/T_Diorama_Height.png'), dtype=float)/255.
    for tile in (3, 4, 5):
        source = parse_obj(ART/'P02/Inputs'/f'SM_Diorama_{tile:02d}_Top.obj')
        paths = {kind: EXPORTS/f'SM_BrightP03v2_{tile:02d}_{kind}.obj' for kind in ('Top', 'Coast', 'Bottom')}
        data = {kind: parse_obj(path) for kind, path in paths.items()}
        mapping, source_uv, top_uv = source_mapping(source, data['Top'])
        reference, lateral, joins_expected = coastline_reference(source, data['Top'], mapping, tile)
        coast_match = validate(reference, data['Coast'])
        bottom_match = validate(parse_obj(ART/'P02/Inputs'/f'SM_Diorama_{tile:02d}_Bottom.obj'), data['Bottom'])
        vertices = {kind: np.asarray(value[0]) for kind, value in data.items()}
        faces = {kind: np.asarray([[v for v, u in face] for face in value[2]]) for kind, value in data.items()}
        tuv, cuv = top_uv, vertex_uv(data['Coast'])
        ht, hc = sample_height(tuv, texture), cuv[:, 0]
        weight = 1.-cuv[:, 1]
        assert np.isin(weight, [0., 1.]).all(), 'Coast weight 0/1 계약 오류'
        joins = matched_join(data['Top'], data['Coast'])
        assert len(joins) == len({index for _, index in joins_expected})
        assert np.all(weight[joins[:, 0]] == 1.)
        original = np.asarray(source[0])[mapping]
        original_h = np.maximum(0., (np.linalg.norm(original, axis=1)-5100.)/350.)
        baseline = unit(original)*(5040.+650.*shaped(original_h))[:, None]
        base_sign = np.einsum('ij,ij->i', normals(baseline, faces['Top']), baseline[faces['Top'][:, 0]])
        top_sign = np.einsum('ij,ij->i', normals(vertices['Top'], faces['Top']), vertices['Top'][faces['Top'][:, 0]])
        radii_error = abs(np.linalg.norm(vertices['Top'], axis=1)-(5040.+650.*shaped(original_h)))
        assert radii_error.max() < .002
        assert top_sign.min() > 0. and np.min(top_sign/base_sign) >= .1499
        ref_v, ref_f = np.asarray(reference[0]), np.asarray([[v for v, u in face] for face in reference[2]])
        ref_norm = normals(ref_v, ref_f)
        coast_outside_dot = np.einsum('ij,ij->i', ref_norm, lateral)
        record = dict(tile=tile, exported_sha256={kind: hashlib.sha256(path.read_bytes()).hexdigest() for kind, path in paths.items()},
                      top_uv_max_error=float(np.max(abs(tuv-source_uv[mapping]))),
                      top_height_radius_max_error_uu=float(radii_error.max()),
                      min_projected_area_ratio=float(np.min(top_sign/base_sign)),
                      coast_oriented_geometry_and_uv=coast_match, bottom_unchanged=bottom_match,
                      coast_lateral_dot_min=float(coast_outside_dot.min()),
                      coast_lateral_negative_count=int(np.count_nonzero(coast_outside_dot < -1.e-6)),
                      joined_vertices=len(joins), join_height_h_max_error=float(abs(ht[joins[:, 1]]-hc[joins[:, 0]]).max()),
                      meshes={kind: metrics(vertices[kind], faces[kind]) for kind in data})
        report['tiles'].append(record)
        for thickness, mountain in CASES:
            displaced = dict(vertices)
            displaced['Top'] = vertices['Top']+unit(vertices['Top'])*((thickness-40.)+shaped(ht)*(mountain-650.))[:, None]
            displaced['Coast'] = vertices['Coast']+unit(vertices['Coast'])*(((thickness-40.)+shaped(hc)*(mountain-650.))*weight)[:, None]
            gap = np.linalg.norm(displaced['Coast'][joins[:, 0]]-displaced['Top'][joins[:, 1]], axis=1)
            top_out = np.einsum('ij,ij->i', normals(displaced['Top'], faces['Top']), displaced['Top'][faces['Top'][:, 0]])
            # 조절 후 Coast가 자기 기준 방향과 반전됐는지 비교. 방사 dot는 옆벽에 부적합하다.
            coast_normal_before = normals(vertices['Coast'], faces['Coast'])
            coast_normal_after = normals(displaced['Coast'], faces['Coast'])
            normal_dot = np.einsum('ij,ij->i', coast_normal_before, coast_normal_after)
            reversed_indices = np.flatnonzero(normal_dot <= 0.)
            result = dict(tile=tile, land_thickness=thickness, mountain_height=mountain,
                          maximum_join_gap_uu=float(gap.max()), minimum_top_outward_dot=float(top_out.min()),
                          coast_reversed_faces=int(np.count_nonzero(normal_dot <= 0.)),
                          coast_reversed_face_indices=reversed_indices.tolist(),
                          coast_minimum_normal_cosine=float(np.min(np.einsum('ij,ij->i', unit(coast_normal_before), unit(coast_normal_after)))),
                          top_metrics=metrics(displaced['Top'], faces['Top']), coast_metrics=metrics(displaced['Coast'], faces['Coast']))
            report['cases'].append(result)
        print('TILE_VALIDATED', tile, 'H_ERROR', record['join_height_h_max_error'], 'COAST_NEGATIVE', record['coast_lateral_negative_count'], flush=True)
    issues = []
    for tile in report['tiles']:
        for kind, stat in tile['meshes'].items():
            if stat['degenerate_triangles'] or stat['short_edge_triangles']:
                issues.append(f"{tile['tile']} {kind}: 퇴화 또는 짧은 변")
    for case in report['cases']:
        if case['maximum_join_gap_uu'] > JOIN_TOL or case['minimum_top_outward_dot'] <= 0. or case['coast_reversed_faces']:
            issues.append(f"{case['tile']} {case['land_thickness']}/{case['mountain_height']}: 접합/방향 범위 초과")
    report['issues'] = issues
    report['status'] = 'passed' if not issues else 'failed'
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print('V002_VALIDATION', report['status'], 'MAX_GAP', max(case['maximum_join_gap_uu'] for case in report['cases']))
    return report


if __name__ == '__main__':
    run()
