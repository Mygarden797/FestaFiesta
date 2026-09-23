# repair_experiment.py
# 역할: 플레이어 캐릭터 모델·애니메이션 작업에서 확인된 데이터 오류만 수정한다.

import bpy,bmesh,json
from pathlib import Path
OUT=Path(__file__).resolve().parents[2]/'SourceArt/MainMan_20260911/qa'
o=next(o for o in bpy.data.objects if o.type=='MESH');bm=bmesh.new();bm.from_mesh(o.data)
report=[]
def stat(label):
    report.append({'step':label,'v':len(bm.verts),'f':len(bm.faces),'bad':sum(not e.is_manifold for e in bm.edges),'boundary':sum(e.is_boundary for e in bm.edges),'multi':sum(len(e.link_faces)>2 for e in bm.edges)})
stat('source')
bmesh.ops.dissolve_degenerate(bm,edges=list(bm.edges),dist=.00001);stat('degen')
# remove zero-area faces and excess coplanar internal/sliver faces
bad=[f for f in bm.faces if f.calc_area()<1e-12]
bmesh.ops.delete(bm,geom=bad,context='FACES');stat('zero')
for i in range(6):
    excess=[e for e in bm.edges if len(e.link_faces)>2]
    if not excess:break
    remove=set()
    for e in excess:
        fs=sorted(e.link_faces,key=lambda f:f.calc_area())
        remove.update(fs[:len(fs)-2])
    bmesh.ops.delete(bm,geom=list(remove),context='FACES')
stat('multi')
bmesh.ops.holes_fill(bm,edges=[e for e in bm.edges if e.is_boundary],sides=0);stat('fill')
bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
print(json.dumps(report))
(OUT/'repair_experiment.json').write_text(json.dumps(report,indent=2))
