# delivery.py
# 역할: 플레이어 캐릭터 모델·애니메이션 작업에서 관련 도구가 공유하는 설정과 처리 함수를 제공한다.

import bpy,json,zipfile,hashlib
from pathlib import Path
from mathutils import Vector
OUT=Path(__file__).resolve().parents[2]/'SourceArt/MainMan_20260911'
scene=bpy.context.scene;cam=scene.camera
for name,loc,target,scale in [('final_side',(3,0,.56),(0,0,.5),1.12),('final_face',(.34,-3,.88),(0,0,.855),.37)]:
    cam.location=Vector(loc)*170;cam.rotation_euler=(Vector(target)*170-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=scale*170;scene.render.resolution_x=1200;scene.render.resolution_y=1200;scene.cycles.samples=40;scene.render.filepath=str(OUT/'qa'/(name+'.png'));bpy.ops.render.render(write_still=True)
validation=json.loads((OUT/'qa/validation_report.json').read_text());ue=json.loads((OUT/'qa/unreal_import_report.json').read_text());fbx=json.loads((OUT/'qa/fbx_roundtrip.json').read_text());mats=json.loads((OUT/'qa/unreal_material_report.json').read_text())
assert validation['mesh_checks_pass'] and validation['pose_checks_pass'] and ue['pass'] and fbx['pass'] and mats['helper_executed_successfully']
assert json.loads((OUT/'qa/self_intersections.json').read_text())['total_crossings']==0
assert ue['fbx_sha256']==hashlib.sha256((OUT/'MainMan_Unreal.fbx').read_bytes()).hexdigest()
validation['limitations']=[x for x in validation['limitations'] if 'no Unreal Editor' not in x];validation['unreal_import_verified']=ue['engine'];validation['unreal_material_helper_verified']=True
(OUT/'qa/validation_report.json').write_text(json.dumps(validation,indent=2),encoding='utf-8')
files=['MainMan_Production.blend','MainMan_Unreal.fbx','Import_MainMan_Unreal.py','Materials.json','README.md']
files+=['qa/'+name for name in ['final_front.png','final_three_quarter.png','final_back.png','final_side.png','final_face.png','final_hand.png','pose_elbows_90.png','pose_knees_90.png','pose_finger_curl_60.png','validation_report.json','fbx_roundtrip.json','unreal_import_report.json','unreal_material_report.json','self_intersections.json','source_audit.json']]
manifest={name:{'bytes':(OUT/name).stat().st_size,'sha256':hashlib.sha256((OUT/name).read_bytes()).hexdigest()} for name in files}
(OUT/'Manifest.json').write_text(json.dumps(manifest,indent=2));files.append('Manifest.json')
archive=OUT.parent/'MainMan_20260911_Package.zip'
with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as z:
    for name in files:z.write(OUT/name,'MainMan/'+name)
print('DELIVERED',archive,archive.stat().st_size)
