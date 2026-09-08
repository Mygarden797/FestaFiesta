"""윗면/옆면/바닥을 합친 시험 육지의 틈과 뒤집힌 공유 모서리를 검사한다."""
from pathlib import Path
from collections import defaultdict
import json

ROOT=Path(__file__).resolve().parents[2]
SRC=ROOT/'SourceArt/UI/Atlas/DioramaPilot_v1'
weld={};edges=defaultdict(list);count=0
for name in ['Top','Coast','Bottom']:
    vertices=[]
    for line in (SRC/f'SM_Diorama_{name}.obj').read_text(encoding='utf8').splitlines():
        if line.startswith('v '):
            p=tuple(round(float(x),4) for x in line.split()[1:])
            if p not in weld:weld[p]=len(weld)
            vertices.append(weld[p])
        elif line.startswith('f '):
            face=[vertices[int(x.split('/')[0])-1] for x in line.split()[1:]]
            assert len(set(face))==3,'축퇴 삼각형'
            count+=1
            for a,b in zip(face,face[1:]+face[:1]):edges[tuple(sorted((a,b)))].append(1 if a<b else -1)
bad=[k for k,v in edges.items() if len(v)!=2 or sum(v)!=0]
report={'triangles':count,'welded_vertices':len(weld),'invalid_edges':len(bad),'status':'complete' if not bad else 'failed'}
(SRC/'topology_validation.json').write_text(json.dumps(report,indent=2),encoding='utf8')
print(report)
assert not bad,'육지 경계가 닫히지 않았거나 면 방향 불일치'
