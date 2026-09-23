# roundtrip.py
# 역할: 플레이어 캐릭터 모델·애니메이션 작업에서 관련 도구가 공유하는 설정과 처리 함수를 제공한다.

import bpy,json,math
from pathlib import Path
from mathutils import Vector
OUT=Path(__file__).resolve().parents[2]/'SourceArt/MainMan_20260911'
original=json.loads((OUT/'qa/validation_report.json').read_text())
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(OUT/'MainMan_Unreal.fbx'),use_anim=False)
meshes=[o for o in bpy.data.objects if o.type=='MESH'];rigs=[o for o in bpy.data.objects if o.type=='ARMATURE']
points=[o.matrix_world@v.co*100 for o in meshes for v in o.data.vertices]
bound={'min':[min(p[i] for p in points) for i in range(3)],'max':[max(p[i] for p in points) for i in range(3)]}
err=max(abs(bound[k][i]-original['bounds_cm'][k][i]) for k in ['min','max'] for i in range(3))
r={'mesh_count':len(meshes),'rig_count':len(rigs),'bone_count':len(rigs[0].data.bones) if rigs else 0,'material_slots':len(meshes[0].data.materials) if meshes else 0,'bounds_cm':bound,'bounds_error_cm':err,'unweighted_vertices':sum(not v.groups for o in meshes for v in o.data.vertices),'uv_layers':{o.name:[x.name for x in o.data.uv_layers] for o in meshes},'triangles':sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in meshes),'all_finger_bones':bool(rigs) and all(f'{f}_{i:02}_{s}' in rigs[0].data.bones for s in ['l','r'] for f in ['thumb','index','middle','ring','pinky'] for i in [1,2,3])}
r['pass']=len(meshes)==1 and len(rigs)==1 and err<.01 and r['unweighted_vertices']==0 and r['all_finger_bones'] and r['triangles']==original['total_triangles']
(OUT/'qa/fbx_roundtrip.json').write_text(json.dumps(r,indent=2));print(json.dumps(r))
assert r['pass']
