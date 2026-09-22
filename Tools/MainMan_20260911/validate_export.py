# validate_export.py
# 역할: 플레이어 캐릭터 모델·애니메이션 작업에서 저장된 산출물의 구조와 품질을 검증한다.

import bpy,bmesh,json,math
from pathlib import Path
from mathutils import Vector
from mathutils.bvhtree import BVHTree
OUT=Path(__file__).resolve().parents[2]/'SourceArt/MainMan_20260911'
scene=bpy.context.scene;rig=next(o for o in scene.objects if o.type=='ARMATURE');objs=[o for o in scene.objects if o.type=='MESH']
report={'source':'MainMan.blend','output':'MainMan_Production.blend','objects':[],'pose_tests':[],'limitations':['Custom skeleton; Manny/Quinn retargeting is not configured.','FBX round-trip checked separately; no Unreal Editor import test performed.','Intentional concealed intersections attach closed clothing, hands and accessories.']}
for o in objs:
    me=o.data;me.calc_loop_triangles();b=bmesh.new();b.from_mesh(me)
    remaining=set(b.verts);counts=[]
    while remaining:
        v=remaining.pop();todo=[v];count=1
        while todo:
            v=todo.pop()
            for e in v.link_edges:
                n=e.other_vert(v)
                if n in remaining:remaining.remove(n);todo.append(n);count+=1
        counts.append(count)
    weights=[];invalid=0;maxinf=0
    for v in me.vertices:
        gs=[g for g in v.groups if g.weight>1e-6];maxinf=max(maxinf,len(gs));weights.append(sum(g.weight for g in gs))
        if any(o.vertex_groups[g.group].name not in rig.data.bones for g in gs):invalid+=1
    uv=me.uv_layers.active;uvvalid=bool(uv) and all(math.isfinite(t) for loop in uv.data for t in loop.uv)
    d={'object':o.name,'vertices':len(me.vertices),'triangles':len(me.loop_triangles),'quads':sum(len(p.vertices)==4 for p in me.polygons),'boundary_edges':sum(e.is_boundary for e in b.edges),'nonmanifold_edges':sum(not e.is_manifold for e in b.edges),'nonmanifold_vertices':sum(not v.is_manifold for v in b.verts),'loose_vertices':sum(not v.link_faces for v in b.verts),'zero_area_faces':sum(f.calc_area()<1e-9 for f in b.faces),'inconsistent_normals':sum(e.is_manifold and not e.is_contiguous for e in b.edges),'components':sorted(counts,reverse=True),'unweighted_vertices':sum(w<.999 for w in weights),'max_weight_error':max(abs(w-1) for w in weights),'max_influences':maxinf,'invalid_bone_references':invalid,'uv_finite':uvvalid,'volume_cm3':b.calc_volume(signed=True)}
    d['pass']=all(d[k]==0 for k in ['boundary_edges','nonmanifold_edges','nonmanifold_vertices','loose_vertices','zero_area_faces','inconsistent_normals','unweighted_vertices','invalid_bone_references']) and d['max_weight_error']<1e-5 and maxinf<=4 and uvvalid
    report['objects'].append(d);b.free()
report['total_vertices']=sum(d['vertices'] for d in report['objects']);report['total_triangles']=sum(d['triangles'] for d in report['objects']);report['bone_count']=len(rig.data.bones)
coords=[o.matrix_world@v.co for o in objs for v in o.data.vertices]
report['bounds_cm']={'min':[min(v[i] for v in coords) for i in range(3)],'max':[max(v[i] for v in coords) for i in range(3)]}
report['t_pose']=all((rig.data.bones['upperarm_'+s].tail_local-rig.data.bones['upperarm_'+s].head_local).normalized().dot(Vector((1 if s=='l' else -1,0,0)))>.99999 for s in ['l','r'])
report['five_fingers_per_hand']=all(f'{f}_{i:02}_{s}' in rig.data.bones for s in ['l','r'] for f in ['thumb','index','middle','ring','pinky'] for i in [1,2,3])
rest={o.name:[v.co.copy() for v in o.data.vertices] for o in objs}
def pose_rot(name,axis,degrees):
    p=rig.pose.bones[name];p.rotation_mode='XYZ';p.rotation_euler[axis]=math.radians(degrees)
def reset():
    for p in rig.pose.bones:p.rotation_mode='XYZ';p.rotation_euler=(0,0,0)
    bpy.context.view_layer.update()
tests={'elbows_90':[('lowerarm_l',0,90),('lowerarm_r',0,-90)],'knees_90':[('calf_l',0,90),('calf_r',0,90)],'shoulders_down_45':[('upperarm_l',0,-45),('upperarm_r',0,45)],'finger_curl_60':[(f'{f}_{i:02}_{s}',0,60) for s in ['l','r'] for f in ['thumb','index','middle','ring','pinky'] for i in [1,2,3]]}
cam=scene.camera
for name,rotations in tests.items():
    reset()
    for n,a,d in rotations:pose_rot(n,a,d)
    bpy.context.view_layer.update();dg=bpy.context.evaluated_depsgraph_get();maxstretch=0;finite=True
    for o in objs:
        eo=o.evaluated_get(dg);em=eo.to_mesh()
        for v in em.vertices:finite=finite and all(math.isfinite(t) for t in v.co)
        for e in em.edges:
            a,b=e.vertices;original=(rest[o.name][a]-rest[o.name][b]).length
            if original>1e-5:maxstretch=max(maxstretch,(em.vertices[a].co-em.vertices[b].co).length/original)
        eo.to_mesh_clear()
    report['pose_tests'].append({'name':name,'finite_vertices':finite,'max_edge_stretch':maxstretch})
    if name in ['elbows_90','knees_90','finger_curl_60']:
        if name=='finger_curl_60':cam.location=(70,-40,170);target=Vector((64,0,114.6));cam.data.ortho_scale=35
        else:cam.location=(240,-480,220);target=Vector((0,0,85));cam.data.ortho_scale=200
        cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();scene.render.filepath=str(OUT/'qa'/('pose_'+name+'.png'));scene.cycles.samples=24;bpy.ops.render.render(write_still=True)
reset()
report['mesh_checks_pass']=all(d['pass'] for d in report['objects'])
report['pose_checks_pass']=all(p['finite_vertices'] and p['max_edge_stretch']<4 for p in report['pose_tests'])
(OUT/'qa/validation_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('VALIDATION',json.dumps({k:v for k,v in report.items() if k!='objects'}),flush=True)
print('ISSUES',json.dumps([d for d in report['objects'] if not d['pass']]),flush=True)
if not report['mesh_checks_pass']:raise RuntimeError('Mesh validation failed; FBX withheld')
cam.location=(229.5,-510,204);cam.rotation_euler=(Vector((0,0,85))-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=193.8
rig.name='Armature'
bpy.ops.object.select_all(action='DESELECT')
# One skeletal mesh in FBX, with shared material slots. Authoring parts stay separate.
copies=[]
for o in objs:
    c=o.copy();c.data=o.data.copy();scene.collection.objects.link(c);c.select_set(True);copies.append(c)
bpy.context.view_layer.objects.active=copies[0];bpy.ops.object.join();export_mesh=bpy.context.object;export_mesh.name='SK_MainMan'
rig.select_set(True)
bpy.ops.export_scene.fbx(filepath=str(OUT/'MainMan_Unreal.fbx'),use_selection=True,object_types={'MESH','ARMATURE'},apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',axis_forward='-Y',axis_up='Z',add_leaf_bones=False,use_armature_deform_only=False,bake_anim=False,mesh_smooth_type='FACE',use_mesh_modifiers=True,use_triangles=True,path_mode='AUTO',primary_bone_axis='Y',secondary_bone_axis='X')
bpy.data.objects.remove(export_mesh,do_unlink=True)
for o in objs:o.select_set(False)
rig.select_set(False);objs[0].select_set(True);bpy.context.view_layer.objects.active=objs[0]
scene.cycles.samples=32
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'MainMan_Production.blend'))
print('EXPORT_COMPLETE',flush=True)
