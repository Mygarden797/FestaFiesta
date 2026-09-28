# build_expansion70.py
# 역할: 시설 모델·Data Asset·썸네일 작업에서 원본 모델이나 결과물을 생성한다.

"""Run through Blender MCP: import build_expansion70 as f; f.build_one(1).

Creates one editable .blend in the configured FF_FACILITY_BLEND_DIR (or project default) directory.
Reruns replace only collections/assets explicitly owned by this authoring job.
Other open Blender scenes, objects and existing .blend files are preserved.
"""
import hashlib
import importlib
import json
import math
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from expansion70_paths import blend_directory
import sys
import bpy
from mathutils import Matrix, Vector

ROOT=Path(__file__).resolve().parents[2]
ART=ROOT/'SourceArt'/'FacilityExpansion70'
BLEND=blend_directory()
sys.path.insert(0,str(Path(__file__).parent))
import expansion70_common as common


def point_camera(obj,target):
    obj.rotation_euler=(Vector(target)-obj.location).to_track_quat('-Z','Y').to_euler()


def bounds(objects,depsgraph):
    depsgraph.update()
    points=[]
    for obj in objects:
        evaluated=obj.evaluated_get(depsgraph)
        if obj.type not in {'MESH','CURVE','FONT'}:continue
        data=evaluated.to_mesh()
        points.extend(evaluated.matrix_world@v.co for v in data.vertices)
        evaluated.to_mesh_clear()
    lo=Vector([min(p[i] for p in points) for i in range(3)])
    hi=Vector([max(p[i] for p in points) for i in range(3)])
    return lo,hi


def camera_setup(b):
    scene=b.scene
    scene.render.engine='CYCLES'
    scene.cycles.samples=24
    scene.cycles.use_denoising=True
    scene.render.resolution_x=1024;scene.render.resolution_y=1024
    scene.render.resolution_percentage=100
    scene.render.image_settings.file_format='PNG'
    scene.render.image_settings.color_mode='RGBA'
    scene.render.film_transparent=True
    scene.view_settings.view_transform='AgX'
    scene.view_settings.look='AgX - Medium High Contrast'
    scene.view_settings.exposure=-.7
    scene.world=bpy.data.worlds.new('FF70_StudioWorld')
    scene.world.use_nodes=True
    scene.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.78,.85,.90,1)
    scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.45
    camdata=bpy.data.cameras.new('FF70_Camera');cam=bpy.data.objects.new('FF70_Camera',camdata)
    b.preview.objects.link(cam);scene.camera=cam
    camdata.type='ORTHO'
    return cam


def render_views(b,folder,dims,all_views=False):
    cam=camera_setup(b)
    w,d,h=dims;span=max(w,d,h,2)
    target=Vector((0,0,h*.44))
    # Orthographic views share framing, making silhouette comparison reproducible.
    views={'iso':((1.2,1.65,1.12),1024)}
    if all_views:views.update({'front':((0,2,.32),512),'rear':((0,-2,.32),512),'top':((0,.001,2),512)})
    lamps=[]
    for name,loc,power,size in [('Key',(4,6,8),1700,7),('Fill',(-5,1,5),950,6),('Rim',(0,-4,6),1400,5)]:
        data=bpy.data.lights.new(name,'AREA');data.energy=power*(span/5)**2;data.shape='DISK';data.size=size*span/5
        obj=bpy.data.objects.new(name,data);b.preview.objects.link(obj)
        obj.location=Vector(loc)*span/5;point_camera(obj,target);lamps.append(obj)
    for view,(direction,res) in views.items():
        cam.location=target+Vector(direction)*span
        point_camera(cam,target)
        cam.data.ortho_scale=span*1.6 if view=='iso' else span*1.2
        b.scene.render.resolution_x=res;b.scene.render.resolution_y=res
        b.scene.render.filepath=str(folder/(view+'.png'))
        bpy.ops.render.render(write_still=True)
    if all_views and b.entry['category']=='Lighting':
        cam.location=target+Vector((1.2,1.65,1.12))*span;point_camera(cam,target)
        cam.data.ortho_scale=span*1.6
        b.scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.035
        for lamp in lamps:lamp.data.energy*=.08
        b.scene.render.filepath=str(folder/'night.png');bpy.ops.render.render(write_still=True)
        b.scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.45
        for lamp in lamps:lamp.data.energy/=.08
    cam.location=target+Vector((1.2,1.65,1.12))*span;point_camera(cam,target)
    cam.data.ortho_scale=span*1.6
    b.scene.render.resolution_x=1024;b.scene.render.resolution_y=1024
    b.scene.render.filepath=str(folder/'iso.png')


def export_mesh(b,folder):
    deps=bpy.context.evaluated_depsgraph_get()
    exported=[]
    bpy.ops.object.select_all(action='DESELECT')
    for source in b.vis.objects:
        if source.type not in {'MESH','FONT','CURVE'}:continue
        data=bpy.data.meshes.new_from_object(source.evaluated_get(deps),depsgraph=deps)
        obj=bpy.data.objects.new('Export_'+source.name,data)
        b.scene.collection.objects.link(obj)
        obj.matrix_world=source.matrix_world.copy();obj.select_set(True);exported.append(obj)
    bpy.context.view_layer.objects.active=exported[0]
    bpy.ops.object.join()
    mesh=bpy.context.object;mesh.name='SM_FF_'+b.entry['id']
    b.scene.cursor.location=(0,0,0)
    bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    # UV0 generated for every surface, including converted lettering and curves.
    bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.uv.smart_project(angle_limit=math.radians(66),island_margin=.01)
    bpy.ops.object.mode_set(mode='OBJECT')
    mesh.data.uv_layers[0].name='UV0'
    uv1=mesh.data.uv_layers.new(name='UV1')
    for source,target in zip(mesh.data.uv_layers[0].data,uv1.data):target.uv=source.uv
    tri=mesh.modifiers.new('Export triangulation','TRIANGULATE')
    bpy.ops.object.modifier_apply(modifier=tri.name)
    mesh.data.calc_loop_triangles()
    triangles=len(mesh.data.loop_triangles)
    material_names=[m.name for m in mesh.data.materials if m]
    for col in b.col.objects:col.hide_set(False);col.select_set(True)
    path=folder/(mesh.name+'.fbx')
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH'},
        axis_forward='-Y',axis_up='Z',apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',
        mesh_smooth_type='FACE',bake_anim=False,use_mesh_modifiers=True)
    bpy.data.objects.remove(mesh,do_unlink=True)
    for col in b.col.objects:col.hide_set(True)
    return path,triangles,material_names


def build_one(number,all_views=False):
    entry=next(e for e in json.loads((ROOT/'Tools/facilities/expansion70_catalog.json').read_text(encoding='utf-8'))['facilities'] if e['number']==number)
    folder=ART/entry['category']/entry['id'];folder.mkdir(parents=True,exist_ok=True)
    blend_path=BLEND/('%02d_%s.blend'%(number,entry['id']))
    BLEND.mkdir(parents=True,exist_ok=True)
    manifest_path=ART/'build_manifest.json'
    manifest=json.loads(manifest_path.read_text(encoding='utf-8')) if manifest_path.exists() else {'schema_version':1,'facilities':[]}
    known={e['id']:e for e in manifest['facilities']}
    if blend_path.exists() and entry['id'] not in known:
        raise RuntimeError('Refusing to replace untracked blend: '+str(blend_path))
    original=bpy.context.scene
    old=[s for s in bpy.data.scenes if s.get('FF70.Owner')==entry['id']]
    for s in old:
        if bpy.context.window.scene==s:
            bpy.context.window.scene=next(x for x in bpy.data.scenes if x!=s)
        objs=list(s.objects);cols=list(s.collection.children)
        bpy.data.scenes.remove(s)
        for o in objs:
            if o.users_scene==():bpy.data.objects.remove(o,do_unlink=True)
        for c in cols:
            if c.users==0:bpy.data.collections.remove(c)
    b=common.Builder(entry)
    module='expansion70_geometry_food' if number<=7 else ('expansion70_geometry_services' if number<=31 else 'expansion70_geometry_environment')
    geometry=importlib.reload(importlib.import_module(module));geometry.build(b,entry)
    bpy.context.view_layer.update()
    lo,hi=bounds(b.vis.objects,bpy.context.evaluated_depsgraph_get())
    # Preserve the artist's aspect ratio; footprint can legitimately contain open space.
    # The importer records actual mesh dimensions and avoids stretch at runtime.
    target=Vector((*[v/100 for v in entry['footprint_cm']],entry['height_cm']/100))
    extent=hi-lo
    factor=min(target[i]/extent[i] for i in range(3))
    offset=Vector((-(lo.x+hi.x)/2,-(lo.y+hi.y)/2,-lo.z))
    factors=tuple(target[i]/extent[i] for i in range(3)) if number>=65 else (factor,factor,factor)
    transform=Matrix.Diagonal((*factors,1))@Matrix.Translation(offset)
    for obj in list(b.vis.objects)+list(b.col.objects)+list(b.guides.objects):obj.matrix_world=transform@obj.matrix_world
    b.guide('CustomerFront_PositiveY',(0,target.y/2+.6,0))
    b.guide('CharacterHeight_180cm',(target.x/2+.5,0,1.8))
    b.guide('GroundCentrePivot',(0,0,0))
    bpy.context.view_layer.update()
    lo,hi=bounds(b.vis.objects,bpy.context.evaluated_depsgraph_get());dims=list(hi-lo)
    fbx,triangles,materials=export_mesh(b,folder)
    render_views(b,folder,dims,all_views)
    text=bpy.data.texts.new('FACILITY_README')
    text.write(json.dumps({**entry,'source_units':'metres','front':'+X' if number>=65 else '+Y','pivot':'ground centre','production_status':'first art pass; runtime interaction and playtesting tracked separately'},ensure_ascii=False,indent=2))
    b.scene['FF70.Readme']=text
    b.scene['FF70.Reference']=entry['reference_image']
    for obj in b.vis.objects:obj.select_set(False)
    bpy.data.libraries.write(str(blend_path),{b.scene,text},fake_user=True,compress=True)
    row={**entry,'fbx':str(fbx),'blend':str(blend_path),'preview':str(folder/'iso.png'),
         'bounds_m':dims,'bounds_min_m':list(lo),'bounds_max_m':list(hi),'triangles':triangles,
         'material_names':materials,'collision_count':len(b.col.objects),'editable_parts':len(b.vis.objects),
         'fbx_sha256':hashlib.sha256(fbx.read_bytes()).hexdigest(),
         'art_status':'first_pass','views':['iso','front','rear','top'] if all_views else ['iso'],
         'light_positions_m':[list(o.location) for o in b.guides.objects if 'light' in o.name.lower()]}
    known[entry['id']]=row
    manifest['facilities']=sorted(known.values(),key=lambda x:x['number'])
    manifest['palette']=str(ART/'palette.json')
    (ART/'palette.json').write_text(json.dumps(common.palette_manifest(),indent=2),encoding='utf-8')
    manifest_path.write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
    (folder/'production_record.json').write_text(json.dumps(row,ensure_ascii=False,indent=2),encoding='utf-8')
    # Restore the user's pre-existing scene. The new facility remains independently accessible.
    if original and original.name in bpy.data.scenes:bpy.context.window.scene=original
    return {k:row[k] for k in ('number','id','blend','bounds_m','triangles','collision_count','editable_parts')}
