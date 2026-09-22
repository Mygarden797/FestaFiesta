# quick_stats.py
# 역할: 플레이어 캐릭터 모델·애니메이션 작업에서 관련 도구가 공유하는 설정과 처리 함수를 제공한다.

import bpy,bmesh,json
d=[]
for o in bpy.data.objects:
    if o.type!='MESH':continue
    b=bmesh.new();b.from_mesh(o.data);d.append({'name':o.name,'v':len(b.verts),'f':len(b.faces),'boundary':sum(e.is_boundary for e in b.edges),'multi':sum(len(e.link_faces)>2 for e in b.edges),'bad_v':sum(not v.is_manifold for v in b.verts),'vol':b.calc_volume(signed=True)});b.free()
print(json.dumps(d))
