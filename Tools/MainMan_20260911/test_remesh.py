# test_remesh.py
# 역할: 플레이어 캐릭터 모델·애니메이션 작업에서 오류 경계와 기존 데이터 보존 동작을 검증한다.

import bpy,bmesh
from pathlib import Path
from mathutils import Vector
OUT=Path(__file__).resolve().parents[2]/'SourceArt/MainMan_20260911/qa'
o=next(o for o in bpy.data.objects if o.type=='MESH');o.modifiers.clear();o.parent=None
bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
# Direct volume reconstruction, without recalculating the damaged source winding.
mod=o.modifiers.new('Volume','REMESH');mod.mode='SMOOTH';mod.octree_depth=9;mod.scale=.9;mod.use_remove_disconnected=False;bpy.ops.object.modifier_apply(modifier=mod.name)
print('REMESH',len(o.data.vertices),len(o.data.polygons),flush=True)
mod=o.modifiers.new('Reduction','DECIMATE');mod.ratio=.12;bpy.ops.object.modifier_apply(modifier=mod.name)
o.data.materials.clear();m=bpy.data.materials.new('clay');m.diffuse_color=(.18,.18,.18,1);o.data.materials.append(m)
for p in o.data.polygons:p.use_smooth=True
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=16;scene.render.resolution_x=1000;scene.render.resolution_y=1000;scene.render.resolution_percentage=100;scene.render.film_transparent=True
scene.world.color=(.3,.3,.3)
bpy.ops.object.camera_add(location=(0,-3,.6));cam=bpy.context.object;scene.camera=cam;cam.data.type='ORTHO';cam.data.ortho_scale=1.12;cam.rotation_euler=(Vector((0,0,.5))-cam.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.light_add(type='AREA',location=(-2,-3,3));bpy.context.object.data.energy=200;bpy.context.object.data.size=3;bpy.context.object.rotation_euler=(Vector((0,0,.5))-bpy.context.object.location).to_track_quat('-Z','Y').to_euler()
scene.render.filepath=str(OUT/'test_remesh.png');bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'test_remesh.blend'))
