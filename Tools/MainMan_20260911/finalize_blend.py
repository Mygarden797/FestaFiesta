import bpy,json
from pathlib import Path
OUT=Path(__file__).resolve().parents[2]/'SourceArt/MainMan_20260911'
palette={}
for m in bpy.data.materials:
    if not m.name.startswith('M_MainMan_'):continue
    p=m.node_tree.nodes.get('Principled BSDF')
    palette[m.name]={'linear_rgb':list(p.inputs['Base Color'].default_value)[:3],'roughness':.82,'metallic':0.,'specular':.22}
(OUT/'Materials.json').write_text(json.dumps(palette,indent=2),encoding='utf-8')
text=bpy.data.texts.get('README | MainMan') or bpy.data.texts.new('README | MainMan')
text.clear();text.write('MAINMAN - STYLIZED LOW-POLY\n\n14,934 triangles, 52 bones, 9 matte materials.\nNeutral T-pose; fingers: thumb/index/middle/ring/pinky, three bones each.\nSeparate closed authoring parts; one merged SkeletalMesh in the delivered FBX.\n\nThe face is intentionally faceted geometry, with simple black capsule eyes.\nSkin/clothing attachment surfaces overlap inside the model.\n\nScene units: centimeters, approximately 170 cm tall.\nArmature: custom FK deformation skeleton; no facial blendshapes or IK controllers.\nManny/Quinn animations need an IK Retargeter; no compatibility is implied.\nThe pose tests are QA renders, not shipping animations.\n\nUnreal 5.7.4 import verified. Use Import Normals to retain flat facial/hair facets.\nThe adjacent Import_MainMan_Unreal.py creates the exact matte materials.\nPreview cameras/lights are not included in FBX.\n')
rig=next(o for o in bpy.data.objects if o.type=='ARMATURE');rig.show_in_front=False
for o in bpy.data.collections['PREVIEW | excluded from FBX'].objects:o.hide_set(True)
bpy.context.scene.render.film_transparent=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'MainMan_Production.blend'))
