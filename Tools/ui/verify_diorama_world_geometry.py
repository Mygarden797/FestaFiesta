"""각 경도 구간의 윗면·베벨·옆면·바닥 폐합과 면 방향을 검사한다."""
from pathlib import Path
from collections import defaultdict
import json
ROOT=Path(__file__).resolve().parents[2]
SRC=ROOT/'SourceArt/UI/Atlas/DioramaWorld_v1'
reports=[]
for tile in range(8):
    weld={};edges=defaultdict(list);count=0
    for suffix in ['Top','Coast','Bottom']:
        vertices=[]
        with (SRC/f'SM_Diorama_{tile:02d}_{suffix}.obj').open(encoding='utf8') as source:
            for line in source:
                if line.startswith('v '):
                    p=tuple(round(float(x),4) for x in line.split()[1:])
                    if p not in weld:weld[p]=len(weld)
                    vertices.append(weld[p])
                elif line.startswith('f '):
                    f=[vertices[int(x.split('/')[0])-1] for x in line.split()[1:]]
                    assert len(set(f))==3,'축퇴 삼각형'
                    count+=1
                    for a,b in zip(f,f[1:]+f[:1]):edges[tuple(sorted((a,b)))].append(1 if a<b else -1)
    bad=[k for k,v in edges.items() if len(v)!=2 or sum(v)!=0]
    item={'tile':tile,'triangles':count,'invalid_edges':len(bad)};reports.append(item);print(item,flush=True)
    assert not bad,'경계 틈 또는 면 방향 불일치'
(SRC/'topology_validation.json').write_text(json.dumps({'status':'complete','tiles':reports},indent=2),encoding='utf8')
