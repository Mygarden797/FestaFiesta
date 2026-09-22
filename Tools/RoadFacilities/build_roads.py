# build_roads.py
# 역할: 설치 가능한 도로·타일 작업에서 원본 모델이나 결과물을 생성한다.

"""Execute build_all() through the connected Blender MCP. Metres, +X forward.

Owns only newly created FF_ROAD_* scenes and SourceArt/Roads_20260910.
Reuses the project's audited FBX/closed-part geometry validation helpers.
"""
import bpy, bmesh, math, json, random, sys
from pathlib import Path
from mathutils import Matrix, Vector

PROJECT = Path(__file__).resolve().parents[2]
ROOT = PROJECT / 'SourceArt/Roads_20260910'
sys.path.insert(0, str(PROJECT / 'SourceArt/FestivalLighting_20260910/code'))
import export_lighting as qa
PALETTE = [('Sand','EAC98E'),('SandLight','F5D89F'),('SandDark','D9B77C'),
           ('Curb','DAD6C5'),('Asphalt','8997A3'),('White','F5F0E5'),
           ('Wood','AF734A'),('WoodLight','C38A5B'),('WoodDark','815335'),
           ('Metal','565C5E'),('Stone','EEDBBC'),('StoneLight','F8E5C5'),
           ('Terracotta','D89770'),('Mint','ACD6BA'),('MintLight','C0DEC4'),('Grout','86B39F')]
NAMES = ['Sand','Asphalt','Wood','Stone','Herringbone','Concrete']
BUILT = {}

def linear(v): return v/12.92 if v <= .04045 else ((v+.055)/1.055)**2.4
def dump(path,value):
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(json.dumps(value,ensure_ascii=False,indent=2),encoding='utf-8')

class Road:
    def __init__(self,name):
        self.name=name; self.parts=[]; self.mats={}
        self.scene=bpy.data.scenes.new('FF_ROAD_'+name+'_SOURCE')
        bpy.context.window.scene=self.scene
        self.scene.unit_settings.system='METRIC'; self.scene.unit_settings.scale_length=1
        self.col=bpy.data.collections.new('EDITABLE_'+name); self.scene.collection.children.link(self.col)
        for key,h in PALETTE:
            rgb=[int(h[i:i+2],16)/255 for i in (0,2,4)]
            mat=bpy.data.materials.new('M_RoadSource_'+name+'_'+key); mat.use_nodes=True
            p=mat.node_tree.nodes.get('Principled BSDF')
            p.inputs['Base Color'].default_value=(*[linear(x) for x in rgb],1)
            p.inputs['Roughness'].default_value=.78; mat.diffuse_color=p.inputs['Base Color'].default_value
            self.mats[key]=mat
    def prism(self,label,poly,z0,z1,mat,bevel=.005,detail=0):
        # Consistent softened outer corners across every layer and separate part.
        for nx,ny in [(1,1),(1,-1),(-1,1),(-1,-1)]:
            clipped=[]
            for a,c in zip(poly,poly[1:]+poly[:1]):
                da=nx*a[0]+ny*a[1]-3.94;dc=nx*c[0]+ny*c[1]-3.94
                if da<=0:clipped.append(a)
                if (da<=0)!=(dc<=0):
                    t=da/(da-dc);clipped.append((a[0]+t*(c[0]-a[0]),a[1]+t*(c[1]-a[1])))
            poly=clipped
        n=len(poly)
        if n<3:return None
        verts=[(x,y,z) for z in (z0,z1) for x,y in poly]
        faces=[tuple(reversed(range(n))),tuple(range(n,2*n))]+[(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
        me=bpy.data.meshes.new(label);me.from_pydata(verts,[],faces);me.update()
        bm=bmesh.new();bm.from_mesh(me);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(me);bm.free()
        ob=bpy.data.objects.new(self.name+'__'+label,me);self.col.objects.link(ob)
        me.materials.append(self.mats[mat]);ob['part_label']=label;ob['detail']=detail
        if bevel:
            mod=ob.modifiers.new('FinishedEdges','BEVEL');mod.width=bevel;mod.segments=2
        self.parts.append(ob);return ob
    def box(self,label,x,y,sx,sy,z0,z1,mat,bevel=.006,detail=0):
        return self.prism(label,[(x-sx/2,y-sy/2),(x+sx/2,y-sy/2),(x+sx/2,y+sy/2),(x-sx/2,y+sy/2)],z0,z1,mat,bevel,detail)
    def disc(self,label,x,y,rx,ry,z0,z1,mat,detail=1):
        return self.prism(label,[(x+rx*math.cos(i*math.tau/10),y+ry*math.sin(i*math.tau/10)) for i in range(10)],z0,z1,mat,0,detail)
    def base(self,mat): self.box('ClosedFoundation',0,0,4,4,0,.045,mat,.012)
    def curbs(self,mat,side='both'):
        for y in ([-1.83,1.83] if side=='both' else [-1.83]):
            for i in range(8): self.box('Curb_%s_%02d'%(y,i),-1.75+i*.5,y,.492,.34,.042,.08,mat,.009)

def sand(b):
    b.base('SandDark');b.box('SandBed',0,0,4,3.31,.04,.079,'Sand',.01);b.curbs('SandLight')
    r=random.Random(19)
    for i in range(22):
        x=r.uniform(-1.8,1.8);y=r.uniform(-1.52,1.52);radius=r.uniform(.013,.035)
        b.disc('SandPebble_%02d'%i,x,y,radius,radius*.65,.078,.0797,'SandDark' if i%3==0 else 'SandLight',2)

def asphalt(b):
    b.base('Asphalt');b.box('AsphaltSurface',0,0,4,3.31,.04,.079,'Asphalt',.01);b.curbs('Curb')
    # +X travel direction; dashed centre line repeats seamlessly every4m.
    for i,x in enumerate([-1.3333,0,1.3333]):b.box('LaneStripe_%02d'%i,x,0,.75,.18,.0785,.08,'White',.009)

def wood(b):
    b.base('WoodDark')
    for i in range(7):
        x=-2+(i+.5)*4/7
        b.box('DeckPlank_%02d'%i,x,0,4/7-.013,4,.043,.078,'WoodLight' if i%3==0 else 'Wood',.008)
        for j,y in enumerate([-1.83,1.83]):
            b.disc('Fastener_%02d_%d'%(i,j),x,y,.026,.026,.077,.08,'Metal',1)
            b.box('FastenerSlot_%02d_%d'%(i,j),x,y,.024,.006,.0798,.08,'WoodDark',0,2)
        for j in range(3):
            cy=-1.10+j*1.05;xx=x+(.13 if j%2 else -.12)
            poly=[(xx+.006*math.sin(t*.8),cy-.37+t*.074) for t in range(11)]
            poly +=[(px+.004,py) for px,py in reversed(poly)]
            b.prism('WoodGrain_%02d_%d'%(i,j),poly,.0775,.079,'WoodDark',0,2)
    # Closed inset knot, intentionally subtle and removable at distance.
    b.disc('PlankKnot',-.87,.44,.034,.10,.0775,.079,'WoodDark',2)

def stone(b):
    b.base('SandDark')
    for i in range(4):
        for j in range(4):
            x=-1.5+i;y=-1.5+j;h=.493;c=.075
            p=[(x-h+c,y-h),(x+h-c,y-h),(x+h,y-h+c),(x+h,y+h-c),(x+h-c,y+h),(x-h+c,y+h),(x-h,y+h-c),(x-h,y-h+c)]
            b.prism('PavingTile_%d_%d'%(i,j),p,.04,.08,'StoneLight' if (i+j)%3==0 else 'Stone',.007)
    for i in range(3):
        for j in range(3):
            x=i-1;y=j-1;r=.093
            b.prism('InsetDiamond_%d_%d'%(i,j),[(x-r,y),(x,y-r),(x+r,y),(x,y+r)],.041,.079,'Terracotta' if (i+j)%2==0 else 'Stone',.003)

def clip(poly,axis,bound,sign):
    out=[]
    for a,c in zip(poly,poly[1:]+poly[:1]):
        ia=sign*(a[axis]-bound)<=1e-8;ic=sign*(c[axis]-bound)<=1e-8
        if ia:out.append(a)
        if ia!=ic:
            t=(bound-a[axis])/(c[axis]-a[axis]);out.append(tuple(a[k]+t*(c[k]-a[k]) for k in range(2)))
    return out

def herringbone(b):
    b.base('Grout');b.box('MortarBed',0,0,4,3.32,.042,.055,'Grout',0);b.curbs('MintLight')
    s=.44;cs=math.sqrt(.5);count=0
    for i in range(-13,14):
        for j in range(-13,14):
            if (i-j)%4 not in (0,3):continue
            w,h=(2,1) if (i-j)%4==0 else (1,2)
            g=.012
            p=[(i*s+g,j*s+g),((i+w)*s-g,j*s+g),((i+w)*s-g,(j+h)*s-g),(i*s+g,(j+h)*s-g)]
            p=[((x-y)*cs,(x+y)*cs) for x,y in p]
            for ax,bo,sg in [(0,-2,-1),(0,2,1),(1,-1.656,-1),(1,1.656,1)]:
                if p:p=clip(p,ax,bo,sg)
            if len(p)<3:continue
            area=abs(sum(a[0]*c[1]-c[0]*a[1] for a,c in zip(p,p[1:]+p[:1])))*.5
            if area<.0001:continue
            b.prism('HerringboneBrick_%03d'%count,p,.048,.08,'MintLight' if count%4==0 else 'Mint',.004);count+=1

def concrete(b):
    b.base('Asphalt');b.curbs('Curb','left')
    # Four sections surround a real recessed drain, with a closed bottom.
    x0,x1,y0,y1=1.10,1.72,.88,1.45
    b.box('Slab_Left',(-2+x0)/2,(-1.65+2)/2,x0+2,3.65,.042,.08,'Asphalt',0)
    b.box('Slab_Right',(x1+2)/2,.175,2-x1,3.65,.042,.08,'Asphalt',0)
    b.box('Slab_Near',(x0+x1)/2,(-1.65+y0)/2,x1-x0,y0+1.65,.042,.08,'Asphalt',0)
    b.box('Slab_Far',(x0+x1)/2,(y1+2)/2,x1-x0,2-y1,.042,.08,'Asphalt',0)
    b.box('DrainRecess',(x0+x1)/2,(y0+y1)/2,x1-x0,y1-y0,.045,.060,'Metal',0)
    for label,x,y,w,h in [('L',x0+.025,(y0+y1)/2,.05,y1-y0),('R',x1-.025,(y0+y1)/2,.05,y1-y0),('N',(x0+x1)/2,y0+.025,x1-x0-.10,.05),('F',(x0+x1)/2,y1-.025,x1-x0-.10,.05)]:
        b.box('DrainFrame_'+label,x,y,w,h,.06,.08,'Curb',.003)
    for i in range(5):b.box('DrainBar_%d'%i,(x0+x1)/2,y0+.105+i*.09,x1-x0-.10,.031,.062,.078,'Curb',.002)
    # Reference presents the sole curb on the left and the drain at far right.
    for ob in b.parts:
        for v in ob.data.vertices:v.co.y=-v.co.y
        bm=bmesh.new();bm.from_mesh(ob.data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(ob.data);bm.free()

def atlas():
    colors=[[int(h[i:i+2],16)/255 for i in (0,2,4)] for _,h in PALETTE]
    path=ROOT/'textures/T_Road_Palette.png';qa._palette_png(path,colors)
    im=bpy.data.images.load(str(path),check_existing=True);im.pack()
    mat=bpy.data.materials.new('M_Road_Palette');mat.use_nodes=True
    tex=mat.node_tree.nodes.new('ShaderNodeTexImage');tex.image=im;tex.interpolation='Closest'
    uv=mat.node_tree.nodes.new('ShaderNodeUVMap');uv.uv_map='UV0_Palette'
    p=mat.node_tree.nodes.get('Principled BSDF');p.inputs['Roughness'].default_value=.78
    mat.node_tree.links.new(uv.outputs['UV'],tex.inputs['Vector']);mat.node_tree.links.new(tex.outputs['Color'],p.inputs['Base Color'])
    return mat

def uv1(mesh):
    # Separate unique triangle islands, with deterministic padding. UV0 is a color atlas.
    layer=mesh.uv_layers.new(name='UV1_Lightmap');n=math.ceil(math.sqrt(len(mesh.polygons)))
    for i,p in enumerate(mesh.polygons):
        for loop,(x,y) in zip(p.loop_indices,[(.15,.15),(.85,.15),(.15,.85)]):
            layer.data[loop].uv=((i%n+x)/n,(i//n+y)/n)
    mesh.uv_layers.active_index=0;mesh.uv_layers[0].active_render=True
    return {'status':'PASS','method':'One distinct padded grid island per triangle','islands':len(mesh.polygons),'grid':n,'lightmap_resolution_recommended':1024}

def export(b,mat):
    sc=b.scene;bpy.context.window.scene=sc
    col=qa._new_collection(sc,'EXPORT_'+b.name);name='SM_Road_'+b.name;lods=[];objs=[]
    render_name='SM_FF_Road_Surface_'+b.name
    materials=list(b.mats.values());lookup={m.name:i for i,m in enumerate(materials)}
    # A single shallow convex platform permits walking across every decorative groove.
    bm=bmesh.new();bmesh.ops.create_cube(bm,size=1)
    for v in bm.verts:v.co=Vector((v.co.x*4,v.co.y*4,v.co.z*.08+.04))
    bmesh.ops.triangulate(bm,faces=list(bm.faces));me=bpy.data.meshes.new('RoadCollision');bm.to_mesh(me);bm.free()
    ucx=bpy.data.objects.new('UCX_'+render_name+'_00',me);col.objects.link(ucx);ucx.hide_render=True;ucx.display_type='WIRE'
    for level in range(3):
        lod_sources=b.parts;temporary=[]
        if level==1:
            for src in b.parts:
                for mod in src.modifiers:
                    if mod.type=='BEVEL':mod.segments=1
            bpy.context.view_layer.update();deps=bpy.context.evaluated_depsgraph_get()
            for src in b.parts:
                mesh=bpy.data.meshes.new_from_object(src.evaluated_get(deps))
                copy=src.copy();copy.data=mesh;copy.modifiers.clear();col.objects.link(copy);temporary.append(copy)
            lod_sources=temporary
        ob,parts=qa._build_lod(sc,lod_sources,level,materials,lookup,col,Matrix.Identity(4),render_name)
        if ob.name != render_name+('' if level==0 else '_LOD'+str(level)):
            raise RuntimeError('Export namespace already used; preserve existing scene and choose a new task prefix')
        for copy in temporary:
            mesh=copy.data;bpy.data.objects.remove(copy,do_unlink=True);bpy.data.meshes.remove(mesh)
        for src in b.parts:
            for mod in src.modifiers:
                if mod.type=='BEVEL':mod.segments=2
        uvreport=uv1(ob.data)
        ob.data.materials.clear();ob.data.materials.append(mat)
        for p in ob.data.polygons:p.material_index=0
        report=qa._mesh_audit(ob,False);report['self_intersections']=qa._self_intersections(ob,parts)
        path=ROOT/'export'/((name if level==0 else name+'_LOD'+str(level))+'.fbx')
        qa._export_fbx(path,[ob,ucx] if level==0 else [ob])
        reimport=qa._reimport(path,ob,sc,1 if level==0 else 0)
        if report['status']!='PASS' or reimport['status']!='PASS' or report['self_intersections']['status']!='PASS':
            dump(ROOT/'qa'/(name+'_failure.json'),{'geometry':report,'reimport':reimport});raise RuntimeError(name+' LOD QA failed')
        lods.append({'lod':level,'fbx':str(path),'triangles':report['triangles'],'geometry':report,'uv1':uvreport,'fbx_reimport':reimport})
        ob.hide_render=True;ob.hide_set(True);objs.append(ob)
    ucx.hide_set(True)
    for ob in b.parts:ob.hide_render=False
    manifest={'asset':name,'fbx_render_object':render_name,'fbx_collision_object':ucx.name,'size_cm':[400,400,8],'front':'+X','pivot_cm':[0,0,0],'collision_hulls':1,'lods':lods,'source_parts':len(b.parts),'material_slots':1,'palette':'textures/T_Road_Palette.png','nanite':False,'unreal_import':'NOT_RUN','performance':'NOT_RUN','assumptions':'Reference has no dimensions or underside; inferred 4m square, 8cm shallow modular slab. Closed foundation and recessed drain; seams backed by foundation.'}
    dump(ROOT/'qa'/(name+'_validation.json'),manifest)
    # Write only this scene and its dependent editable objects, not other open work.
    path=ROOT/'blend'/(name+'.blend');path.parent.mkdir(parents=True,exist_ok=True)
    bpy.data.libraries.write(str(path),{sc},fake_user=True,compress=True)
    BUILT[b.name]={'road':b,'manifest':manifest,'lods':objs}
    return manifest

def build_all():
    ROOT.mkdir(parents=True,exist_ok=True)
    if any((ROOT/'blend'/(('SM_Road_'+n)+'.blend')).exists() for n in NAMES):raise RuntimeError('Existing road delivery: use explicit revision path before rebuilding')
    backup=ROOT/'backup';backup.mkdir(exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(backup/'Blender_PreRoads.blend'),copy=True)
    mat=atlas();results=[]
    for n,fn in zip(NAMES,[sand,asphalt,wood,stone,herringbone,concrete]):
        b=Road(n);fn(b);results.append(export(b,mat))
    calibration=qa._calibration(b.scene,ROOT,b.scene.collection,mat,'SM_Road')
    dump(ROOT/'qa/calibration.json',calibration)
    dump(ROOT/'validation_report.json',{'blender_version':bpy.app.version_string,'mesh_checks':'PASS','fbx_reimport':'PASS','reference_reproduction':'NOT_RUN','unreal_import':'NOT_RUN','game_performance':'NOT_RUN','game_performance_ms':None,'assets':results})
    return [{'asset':x['asset'],'triangles':[y['triangles'] for y in x['lods']],'parts':x['source_parts']} for x in results]

def setup_render(b):
    sc=b.scene;bpy.context.window.scene=sc
    sc.render.engine='CYCLES';sc.cycles.samples=24
    try:
        p=bpy.context.preferences.addons['cycles'].preferences;p.compute_device_type='OPTIX';p.get_devices()
        for device in p.devices:device.use=device.type=='OPTIX'
        sc.cycles.device='GPU'
    except Exception:sc.cycles.device='CPU'
    sc.render.resolution_x=512;sc.render.resolution_y=512;sc.render.resolution_percentage=100
    sc.render.image_settings.file_format='PNG';sc.render.film_transparent=True
    sc.view_settings.view_transform='Standard';sc.view_settings.exposure=-.65
    world=bpy.data.worlds.new('RoadStudio');world.use_nodes=True;world.node_tree.nodes['Background'].inputs[0].default_value=(.72,.80,.80,1);world.node_tree.nodes['Background'].inputs[1].default_value=.65;sc.world=world
    data=bpy.data.cameras.new('RoadQA');camera=bpy.data.objects.new('RoadQA',data);sc.collection.objects.link(camera);data.type='ORTHO';data.ortho_scale=5.35;sc.camera=camera
    for label,loc,power,size in [('Key',(-3,-4,7),850,5),('Fill',(4,1,5),400,4)]:
        ld=bpy.data.lights.new('RoadStudio_'+label,'AREA');ld.energy=power;ld.shape='DISK';ld.size=size
        lo=bpy.data.objects.new(ld.name,ld);sc.collection.objects.link(lo);lo.location=loc;lo.rotation_euler=(Vector((0,0,0))-lo.location).to_track_quat('-Z','Y').to_euler()
    return sc,camera

def render_asset(name):
    b=BUILT[name]['road'];sc,cam=setup_render(b)
    views={'Hero':(-3.3,0,10),'Front':(-7,0,2),'Rear':(7,0,2),'Left':(0,-7,2),'Right':(0,7,2),'Top':(0,0,8),'Bottom':(0,0,-8),'Game':(-5,-6,7)}
    paths=[]
    for label,pos in views.items():
        cam.data.ortho_scale=6.2 if label=='Game' else 5.35
        cam.location=pos;cam.rotation_euler=(Vector((0,0,.04))-cam.location).to_track_quat('-Z','Y').to_euler()
        sc.render.filepath=str(ROOT/'qa'/('SM_Road_'+name+'_'+label+'.png'));bpy.ops.render.render(write_still=True);paths.append(sc.render.filepath)
    # Explicit LOD captures use the same game camera.
    for ob in b.parts:ob.hide_render=True
    for i,ob in enumerate(BUILT[name]['lods']):
        ob.hide_render=False;sc.render.filepath=str(ROOT/'qa'/('SM_Road_'+name+'_LOD'+str(i)+'.png'));bpy.ops.render.render(write_still=True);ob.hide_render=True
    for ob in b.parts:ob.hide_render=False
    bpy.data.libraries.write(str(ROOT/'blend'/('SM_Road_'+name+'.blend')),{sc},fake_user=True,compress=True)
    return paths
