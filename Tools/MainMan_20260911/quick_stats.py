# quick_stats.py
# 역할: 열린 Blender 메시들의 점·면·구멍·겹친 연결선·부피를 읽어 콘솔에 JSON으로 출력한다.

import bpy,bmesh,json
d=[]
for o in bpy.data.objects:
    if o.type!='MESH':continue
    b=bmesh.new();b.from_mesh(o.data);d.append({'name':o.name,'v':len(b.verts),'f':len(b.faces),'boundary':sum(e.is_boundary for e in b.edges),'multi':sum(len(e.link_faces)>2 for e in b.edges),'bad_v':sum(not v.is_manifold for v in b.verts),'vol':b.calc_volume(signed=True)});b.free()
print(json.dumps(d))
