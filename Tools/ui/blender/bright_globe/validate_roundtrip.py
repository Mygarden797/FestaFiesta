"""OBJ 왕복의 좌표·면 방향·면별 UV를 대조한다. 정점 순서는 가정하지 않는다."""
from pathlib import Path
from collections import Counter, defaultdict
from itertools import product
import json
import math
from prepare_roundtrip import parse_obj, ROOT, ART, OUT

POSITION_TOL = .002  # 5000 uu 좌표의 float32 왕복 허용 오차
UV_TOL = 1.1e-6
BUCKET = .01


def cyclic(face):
    return min(face, face[1:] + face[:1], face[2:] + face[:2])


def validate(source, target):
    sv, su, sf = source
    tv, tu, tf = target
    assert len(sf) == len(tf), (len(sf), len(tf))
    buckets = defaultdict(list)
    source_uv = defaultdict(set)
    identities = {}
    source_ids = {}
    for f in sf:
        for v, u in f:
            source_uv[v].add(u)
            key = (sv[v], su[u])
            source_ids[(v, u)] = identities.setdefault(key, len(identities))
    for index, position in enumerate(sv):
        buckets[tuple(math.floor(c / BUCKET) for c in position)].append(index)
    distances, uv_errors = [], []
    target_ids = {}
    for v, u in {corner for f in tf for corner in f}:
        point, uv = tv[v], tu[u]
        key = tuple(math.floor(c / BUCKET) for c in point)
        best = None
        for delta in product((-1, 0, 1), repeat=3):
            for vi in buckets.get(tuple(key[a] + delta[a] for a in range(3)), ()):
                dist = math.dist(point, sv[vi])
                if dist > POSITION_TOL:
                    continue
                for ui in source_uv[vi]:
                    error = max(abs(uv[a] - su[ui][a]) for a in range(2))
                    if error <= UV_TOL:
                        candidate = (dist, error, source_ids[(vi, ui)])
                        if best is None or candidate < best:
                            best = candidate
        assert best is not None, ('좌표 또는 UV 불일치', point, uv)
        distances.append(best[0])
        uv_errors.append(best[1])
        target_ids[(v, u)] = best[2]
    source_faces = Counter(cyclic(tuple(source_ids[c] for c in face)) for face in sf)
    target_faces = Counter(cyclic(tuple(target_ids[c] for c in face)) for face in tf)
    assert source_faces == target_faces, ('면 연결/방향/면별 UV가 달라짐',
                                        sum((source_faces - target_faces).values()))
    return {'status': 'passed', 'triangles': len(tf),
            'max_position_error_uu': max(distances), 'max_uv_error': max(uv_errors),
            'oriented_faces_and_corner_uv_equal': True}


def main():
    results = []
    for path in sorted((ART / 'P02/Inputs').glob('*.obj')):
        result = validate(parse_obj(path), parse_obj(ART / 'P02/Exports' / path.name))
        result['mesh'] = path.stem
        results.append(result)
    report = {'status': 'passed', 'position_tolerance_uu': POSITION_TOL,
              'uv_tolerance': UV_TOL, 'meshes': results,
              'ue_import_verified': False}
    (OUT / 'p02_obj_validation.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print('P02_OBJ_VALIDATION_PASSED', len(results),
          'position_max_uu', max(r['max_position_error_uu'] for r in results),
          'uv_max', max(r['max_uv_error'] for r in results))


if __name__ == '__main__':
    main()
