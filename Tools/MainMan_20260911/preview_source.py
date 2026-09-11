import bpy, math, json, bmesh
from mathutils import Vector
from pathlib import Path
OUT=Path(__file__).resolve().parents[2]/'SourceArt/MainMan_20260911/qa'
s=bpy.context.scene
s.render.engine='CYCLES';s.cycles.samples=24
s.render.resolution_x=1200;s.render.resolution_y=1200;s.render.resolution_percentage=100
s.render.image_settings.file_format='PNG';s.render.image_settings.color_mode='RGBA';s.render.film_transparent=True
s.world.color=(.3,.3,.3)
s.view_settings.view_transform='Standard'
for o in list(bpy.data.objects):
    if o.type in {'CAMERA','LIGHT'}:bpy.data.objects.remove(o,do_unlink=True)
for name,loc,power,size in [('Key',(-2,-3,4),180,3),('Fill',(2,-1,2),90,2),('Rim',(0,2,3),140,2)]:
    bpy.ops.object.light_add(type='AREA', location=loc);o=bpy.context.object;o.name=name;o.data.energy=power;o.data.shape='DISK';o.data.size=size;o.rotation_euler=(Vector((0,0,.5))-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add();cam=bpy.context.object;s.camera=cam;cam.data.type='ORTHO';cam.data.ortho_scale=1.13
for name,loc,target in [('source_front',(0,-3,.6),(0,0,.5)),('source_three_quarter',(1.4,-3,1.3),(0,0,.5)),('source_back',(0,3,.6),(0,0,.5))]:
    cam.location=loc;cam.rotation_euler=(Vector(target)-cam.location).to_track_quat('-Z','Y').to_euler();s.render.filepath=str(OUT/(name+'.png'));bpy.ops.render.render(write_still=True)
rig=next(o for o in bpy.data.objects if o.type=='ARMATURE')
data={'bones':[{'name':b.name,'head':list(b.head_local),'tail':list(b.tail_local),'parent':b.parent.name if b.parent else None,'pose_rotation':list(rig.pose.bones[b.name].rotation_quaternion)} for b in rig.data.bones]}
obj=next(o for o in bpy.data.objects if o.type=='MESH');bm=bmesh.new();bm.from_mesh(obj.data)
bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=0.000001)
data['after_weld']={'verts':len(bm.verts),'faces':len(bm.faces),'nonmanifold':sum(not e.is_manifold for e in bm.edges),'boundary':sum(e.is_boundary for e in bm.edges),'noncontiguous':sum(e.is_manifold and not e.is_contiguous for e in bm.edges)}
(OUT/'rig_audit.json').write_text(json.dumps(data,indent=2),encoding='utf-8')
print('AUDIT',json.dumps(data))
