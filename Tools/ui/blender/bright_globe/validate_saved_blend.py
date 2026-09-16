"""별도 Blender 프로세스에서 저장 파일을 읽기만 하여 재열기 검증."""
from pathlib import Path
import bpy
import json

ROOT=Path(__file__).resolve().parents[4]
scene=bpy.data.scenes['FF_BrightGlobe_P03_Pilot_v002']
objects=[o for o in scene.objects if o.type=='MESH']
assert len(objects)==9
assert bpy.data.filepath.endswith('FF_BrightGlobe_P03_pilot_v002.blend')
assert all(len(o.data.uv_layers)==1 for o in objects)
assert all(o.data.materials for o in objects if o.name.endswith(('_Top','_Coast')))
mat=bpy.data.materials['MAT-BrightP03v2-Top']
images=[n.image for n in mat.node_tree.nodes if n.type=='TEX_IMAGE']
assert len(images)==1 and images[0].packed_file
report={'status':'passed','file':bpy.data.filepath,'scene':scene.name,'texture_packed':True,
    'objects':[{'name':o.name,'vertices':len(o.data.vertices),'triangles':len(o.data.polygons)} for o in objects]}
(ROOT/'Saved/BrightGlobe/p03_v002_blend_reopen.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('P03_V002_SAVED_BLEND_PASSED')
