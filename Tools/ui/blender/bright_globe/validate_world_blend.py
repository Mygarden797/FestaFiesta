"""별도 Blender 프로세스에서 저장한 세계판과 포함 텍스처를 읽기 검사한다."""
from pathlib import Path
import json,bpy
ROOT=Path('C:/UE5/8th-Team13-Final-Project')
OUT=ROOT/'Saved/BrightGlobe'
expected=json.loads((OUT/'P05v001/exports.json').read_text(encoding='utf-8'))
assert bpy.context.scene.name=='FF_BrightGlobe_P05_World_v001'
rows=[]
for name,record in expected.items():
    obj=bpy.context.scene.objects.get('GEO-'+name)
    assert obj and len(obj.data.polygons)==record['triangles']
    assert len(obj.data.uv_layers)==1, name
    # Bottom은 UE에 임포트하지 않는 숨김 보존 메시다. 표시 재질은 Top/Coast만 필요하다.
    if name.endswith('_Bottom'):assert obj.hide_get(), name
    else:assert len(obj.data.materials)>0, name
    rows.append({'name':name,'triangles':len(obj.data.polygons)})
assert len(rows)==24
images=[n.image for m in bpy.context.scene.objects if m.type=='MESH' for slot in m.material_slots if slot.material and slot.material.use_nodes for n in slot.material.node_tree.nodes if n.type=='TEX_IMAGE' and n.image]
assert images and all(i.packed_file for i in images)
(OUT/'p05_blend_reopen.json').write_text(json.dumps({'status':'passed','file':bpy.data.filepath,'meshes':rows,'images_packed':True,'preview_ocean':bpy.context.scene.objects.get('GEO-BrightWorld_OceanPreview') is not None},ensure_ascii=False,indent=2),encoding='utf-8')
print('P05_WORLD_BLEND_REOPEN_PASSED')
