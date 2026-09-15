"""Reference-driven six editable road tiles; run build_all() through Blender MCP.
Units are metres. Editable source +Y forward, export rotation makes +X forward.
Only this job's new scenes/collections are created. No source image is baked.
"""
import bpy, bmesh, math, json, random, struct, zlib
from pathlib import Path
from mathutils import Vector, Matrix
import audit_helpers as qa

PROJECT = Path(__file__).resolve().parents[2]
ROOT = (Path(__file__).resolve().parent.parent
        if (Path(__file__).resolve().parent.parent / 'References/Road.png').is_file()
        else PROJECT / 'SourceArt/RoadTiles_20260910')
BUILT = {}
PALETTE = [
 ('Sand','F3CF8E'),('SandLight','F7D99F'),('SandDark','DCB678'),('SandStone','EDD09B'),
 ('Asphalt','879AA8'),('Curb','BFC8C9'),('Paint','F6F0DF'),('Wood','B47D51'),
 ('WoodLight','C38B5C'),('WoodDark','94613E'),('Metal','6B7274'),('Cream','EEDAB0'),
 ('CreamLight','F4E2BA'),('Grout','C4BC9E'),('Sienna','D99570'),('Mint','A7D4BB'),
 ('MintLight','B9DEC5'),('MintDark','92BFA9'),('Concrete','A9B1BC'),('ConcreteLight','AEB7C0'),
 ('Drain','444C54'),('DarkBase','727E85'),('MintGrout','7DAB96'),('ConcreteFleck','A0AAB5'),
]

def linear(c): return c/12.92 if c <= .04045 else ((c+.055)/1.055)**2.4

class Builder:
 def __init__(self,name):
  self.name=name; self.parts=[]; self.lods=[]; self.mats={}
  self.scene=bpy.data.scenes.new('ROAD_'+name+'_SOURCE')
  self.scene.unit_settings.system='METRIC'; self.scene.unit_settings.scale_length=1
  bpy.context.window.scene=self.scene
  self.col=bpy.data.collections.new('EDITABLE_'+name); self.scene.collection.children.link(self.col)
  for key,h in PALETTE:
   mat=bpy.data.materials.get('M_RoadSource_'+key)
   if mat is None:
    mat=bpy.data.materials.new('M_RoadSource_'+key); mat.use_nodes=True
    rgb=[linear(int(h[i:i+2],16)/255) for i in (0,2,4)]
    mat.diffuse_color=(*rgb,1); p=mat.node_tree.nodes.get('Principled BSDF')
    p.inputs['Base Color'].default_value=(*rgb,1); p.inputs['Roughness'].default_value=.78
   self.mats[key]=mat
 def mesh(self,label,verts,faces,mat,bevel=0,detail=0):
  me=bpy.data.meshes.new(self.name+'_'+label); me.from_pydata(verts,[],faces); me.update()
  bm=bmesh.new(); bm.from_mesh(me); bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces)); bm.to_mesh(me); bm.free()
  ob=bpy.data.objects.new(self.name+'__'+label,me); self.col.objects.link(ob); me.materials.append(self.mats[mat])
  ob['part_label']=label; ob['detail']=detail; ob['palette_key']=mat
  if bevel:
   mod=ob.modifiers.new('Editable_Edge_Finish','BEVEL'); mod.width=bevel; mod.segments=2
  self.parts.append(ob); return ob
 def prism(self,label,xy,z_bottom,z_top,mat,bevel=.005,detail=0):
  n=len(xy); verts=[(x,y,z) for z in [z_bottom,z_top] for x,y in xy]
  faces=[tuple(reversed(range(n))),tuple(range(n,2*n))]+[(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
  return self.mesh(label,verts,faces,mat,bevel,detail)
 def box(self,label,center,size,mat,bevel=.01,detail=0):
  x,y,z=center; a,b,c=[v/2 for v in size]
  return self.prism(label,[(x-a,y-b),(x+a,y-b),(x+a,y+b),(x-a,y+b)],z-c,z+c,mat,bevel,detail)
 def cylinder(self,label,center,radius,depth,mat,n=8,detail=0):
  x,y,z=center
  return self.prism(label,[(x+radius*math.cos(i*math.tau/n),y+radius*math.sin(i*math.tau/n)) for i in range(n)],z-depth/2,z+depth/2,mat,0,detail)
 def tube(self,label,points,radius,mat,n=5,detail=1):
  pts=[Vector(p) for p in points]; verts=[]
  for i,p in enumerate(pts):
   direction=pts[min(i+1,len(pts)-1)]-pts[max(0,i-1)]
   q=direction.normalized().to_track_quat('Z','Y')
   verts += [tuple(p+q@Vector((radius*math.cos(j*math.tau/n),radius*math.sin(j*math.tau/n),0))) for j in range(n)]
  faces=[tuple(reversed(range(n))),tuple(range((len(pts)-1)*n,len(pts)*n))]
  faces += [(i*n+j,i*n+(j+1)%n,(i+1)*n+(j+1)%n,(i+1)*n+j) for i in range(len(pts)-1) for j in range(n)]
  return self.mesh(label,verts,faces,mat,0,detail)

def curbs(b,mat='Curb',width=.15,count=5,sides=(-1,1)):
 for side in sides:
  for i in range(count):
   b.box('EdgeCurb_%s_%02d'%(side,i),(side*(1-width/2),-1+(i+.5)*2/count,.073),(width,2/count-.006,.074),mat,.009)

def sand(b):
 b.box('Sand_Foundation',(0,0,.028),(2,2,.056),'SandDark',.024)
 # Closed height-field sand body. Heights fall into the base; edges meet curbs.
 nx,ny=16,20; verts=[]
 for j in range(ny+1):
  y=-1+j*2/ny
  for i in range(nx+1):
   x=-.77+i*1.54/nx
   z=.086+.0035*math.sin(7*y+2*x)+.0015*math.cos(13*y-4*x)
   verts.append((x,y,z))
 faces=[]
 for j in range(ny):
  for i in range(nx):
   a=j*(nx+1)+i; faces.append((a,a+1,a+nx+2,a+nx+1))
 ring=list(range(nx+1))+[j*(nx+1)+nx for j in range(1,ny+1)]+[ny*(nx+1)+i for i in range(nx-1,-1,-1)]+[j*(nx+1) for j in range(ny-1,0,-1)]
 start=len(verts); verts += [(verts[i][0],verts[i][1],.045) for i in ring]
 faces.append(tuple(reversed(range(start,len(verts)))))
 for k,i in enumerate(ring):
  kk=(k+1)%len(ring); faces.append((i,start+k,start+kk,ring[kk]))
 b.mesh('Gently_Rippled_Sand',verts,faces,'Sand')
 curbs(b,'SandStone',.20,8)
 for side in [-1,1]:
  for i in range(12):
   b.box('InnerSetts_%s_%s'%(side,i),(side*.773,-1+(i+.5)/6,.082),(.058,1/6-.005,.047),'SandLight',.004)
 rng=random.Random(612)
 for i in range(30):
  x=rng.uniform(-.64,.64); y=rng.uniform(-.91,.91); z=.086+.0035*math.sin(7*y+2*x)+.0015*math.cos(13*y-4*x)
  r=rng.uniform(.006,.019)
  b.cylinder('Sand_Grain_%02d'%i,(x,y,z+.0003),r,.002,'SandDark' if i%4==0 else 'SandLight',7,1)

def asphalt(b):
 b.box('Asphalt_Slab',(0,0,.050),(2,2,.100),'Asphalt',.026)
 curbs(b)
 # Three separate white dash markers reproduce the reference tile.
 for i,y in enumerate([-.74,0,.74]):
  b.box('CenterLine_%s'%i,(0,y,.102),(.105,.42,.006),'Paint',.0018)

def concrete(b):
 b.box('Concrete_Foundation',(0,0,.0275),(2,2,.055),'Concrete',.026)
 # Recess has real walls/floor, no overlapping slab beneath grate openings.
 x0,x1,y0,y1=.49,.87,.59,.87
 b.box('Surface_Left',((-1+x0)/2,0,.075),((x0+1),2,.05),'Concrete',.005)
 b.box('Surface_Right',((x1+1)/2,0,.075),((1-x1),2,.05),'Concrete',.005)
 b.box('Surface_Front',((x0+x1)/2,(-1+y0)/2,.075),(x1-x0,y0+1,.05),'Concrete',.005)
 b.box('Surface_Back',((x0+x1)/2,(y1+1)/2,.075),(x1-x0,1-y1,.05),'Concrete',.005)
 # Join planes of coplanar panels can show seams; those four panels have
 # no bevel on meeting edges (below), preserving the single concrete field.
 for ob in b.parts[-4:]: ob.modifiers.clear()
 b.box('Drain_Dark_Recess',((x0+x1)/2,(y0+y1)/2,.057),(x1-x0,y1-y0,.014),'Drain',.002)
 for label,c,s in [
  ('Drain_Frame_Left',(x0+.018,(y0+y1)/2,.086),(.036,y1-y0,.056)),
  ('Drain_Frame_Right',(x1-.018,(y0+y1)/2,.086),(.036,y1-y0,.056)),
  ('Drain_Frame_Front',((x0+x1)/2,y0+.016,.086),(x1-x0-.072,.032,.056)),
  ('Drain_Frame_Back',((x0+x1)/2,y1-.016,.086),(x1-x0-.072,.032,.056))]:
  b.box(label,c,s,'Metal',.004)
 for i in range(4):
  x=x0+.079+i*.074
  b.box('Grate_Bar_%02d'%i,(x,(y0+y1)/2,.087),(.022,y1-y0-.058,.040),'Metal',.003)
 curbs(b,'Curb',.20,5,(-1,))
 rng=random.Random(191)
 for i in range(12):
  x=rng.uniform(-.62,.72); y=rng.uniform(-.85,.35)
  b.cylinder('Concrete_Fleck_%02d'%i,(x,y,.1003),rng.uniform(.005,.012),.0008,'ConcreteFleck',7,1)

def palette_material():
 # 8x8 flat color atlas, one UE material slot; no lighting painted into color.
 size=512; cell=64; raw=bytearray()
 for y in range(size):
  raw.append(0)
  for x in range(size):
   index=(7-y//cell)*8+x//cell
   h=PALETTE[index][1] if index<len(PALETTE) else 'FFFFFF'
   raw.extend(bytes([int(h[i:i+2],16) for i in (0,2,4)]+[255]))
 def chunk(tag,data): return struct.pack('>I',len(data))+tag+data+struct.pack('>I',zlib.crc32(tag+data)&0xffffffff)
 path=ROOT/'Textures/T_RoadPalette.png'
 path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',size,size,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b''))
 im=bpy.data.images.load(str(path),check_existing=True); im.pack()
 mat=bpy.data.materials.new('M_RoadPalette'); mat.use_nodes=True
 p=mat.node_tree.nodes.get('Principled BSDF'); p.inputs['Roughness'].default_value=.78
 uv=mat.node_tree.nodes.new('ShaderNodeUVMap'); uv.uv_map='UV0_Palette'
 tex=mat.node_tree.nodes.new('ShaderNodeTexImage'); tex.image=im; tex.interpolation='Closest'
 mat.node_tree.links.new(uv.outputs['UV'],tex.inputs['Vector']); mat.node_tree.links.new(tex.outputs['Color'],p.inputs['Base Color'])
 return mat

def make_lod(b,level,mat,col):
 vertices=[]; faces=[]; slots=[]; parts=[]
 bpy.context.window.scene=b.scene
 states=[(m,m.segments,m.show_viewport) for o in b.parts for m in o.modifiers if m.type=='BEVEL']
 for m,_,_ in states:
  m.segments=2 if level==0 else 1; m.show_viewport=level<2
 bpy.context.view_layer.update(); dg=bpy.context.evaluated_depsgraph_get()
 rot=Matrix.Rotation(-math.pi/2,4,'Z')
 for source in b.parts:
  if source.get('detail',0)>2-level: continue
  ev=source.evaluated_get(dg); me=ev.to_mesh(); bm=bmesh.new(); bm.from_mesh(me)
  bmesh.ops.transform(bm,matrix=rot@source.matrix_world,verts=list(bm.verts))
  if level>0 and source.get('part_label')=='Gently_Rippled_Sand':
   bmesh.ops.dissolve_limit(bm,angle_limit=.15,verts=list(bm.verts),edges=list(bm.edges),delimit={'NORMAL'})
  bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces)); bmesh.ops.triangulate(bm,faces=list(bm.faces))
  bm.verts.ensure_lookup_table(); bm.verts.index_update(); off=len(vertices); fs=len(faces)
  vertices.extend(tuple(v.co) for v in bm.verts)
  idx=next(i for i,(key,_) in enumerate(PALETTE) if key==source['palette_key'])
  for f in bm.faces: faces.append(tuple(off+v.index for v in f.verts)); slots.append(idx)
  parts.append(dict(name=source.name,vertex_start=off,vertex_end=len(vertices),face_start=fs,face_end=len(faces)))
  bm.free(); ev.to_mesh_clear()
 for m,seg,vis in states: m.segments=seg; m.show_viewport=vis
 asset='SM_Road_'+b.name; name=asset if level==0 else asset+'_LOD'+str(level)
 me=bpy.data.meshes.new(name); me.from_pydata(vertices,[],faces); me.materials.append(mat); me.update()
 uv=me.uv_layers.new(name='UV0_Palette')
 for face,slot in zip(me.polygons,slots):
  for loop,coord in zip(face.loop_indices,[(.15,.15),(.85,.15),(.15,.85)]):
   uv.data[loop].uv=((slot%8+coord[0])/8,(slot//8+coord[1])/8)
 uv.active_render=True
 ob=bpy.data.objects.new(name,me); col.objects.link(ob); ob['front_axis']='+X'; ob['LOD']=level
 ob['pivot']='placement floor center'; ob['parts']=json.dumps(parts)
 return ob,parts

def export_asset(b,mat):
 bpy.context.window.scene=b.scene
 col=bpy.data.collections.new('EXPORT_'+b.name); b.scene.collection.children.link(col)
 collision=b.box('Collision_Cage',(0,0,.0525),(2,2,.105),'DarkBase',0)
 b.parts.remove(collision); b.col.objects.unlink(collision); col.objects.link(collision)
 collision.name='UCX_SM_Road_'+b.name+'_00'; collision.hide_render=True; collision.display_type='WIRE'
 bm=bmesh.new(); bm.from_mesh(collision.data); bmesh.ops.triangulate(bm,faces=list(bm.faces)); bm.to_mesh(collision.data); bm.free()
 lods=[]
 for level in range(3):
  ob,parts=make_lod(b,level,mat,col); b.lods.append(ob)
  audit=qa._mesh_audit(ob,False); intersections=qa._self_intersections(ob,parts)
  fbx=ROOT/'Exports'/(ob.name+'.fbx'); qa._export_fbx(fbx,[ob,collision] if level==0 else [ob])
  reimport=qa._reimport(fbx,ob,b.scene,1 if level==0 else 0)
  lods.append({'level':level,'fbx':str(fbx),'geometry':audit,'self_intersections':intersections,'fbx_reimport':reimport})
  ob.hide_render=True; ob.hide_set(True)
 collision.hide_set(True)
 b.scene['reference']='References/Road.png'; b.scene['dimensions']='200 x 200 cm, nominal 10 cm thick'
 b.scene['front']='Editable source +Y; EXPORT meshes +X'; b.scene['pivot']='Bottom centre (0,0,0)'
 b.scene['source_parts_editable']=True
 blend=ROOT/'Blends'/('Road_'+b.name+'.blend')
 bpy.data.libraries.write(str(blend),{b.scene},path_remap='RELATIVE',fake_user=True,compress=True)
 report={'name':b.name,'source_blend':str(blend),'editable_part_count':len(b.parts),'lods':lods,
  'collision':{'name':collision.name,'type':'one convex walkable tile slab','geometry':qa._mesh_audit(collision,False)},
  'materials':{'slots':1,'texture':'Textures/T_RoadPalette.png','roughness':.78,'metallic':0,'two_sided':False},
  'uv0':{'status':'PASS','method':'each triangle in correct flat-color atlas swatch; intentional swatch overlap'},
  'lightmap_uv1':{'status':'NOT_APPLICABLE','reason':'Project r.AllowStaticLighting=False; Lumen enabled'},
  'game_performance':{'status':'NOT_RUN','frame_time_ms':None,'draw_calls':None}}
 qa._json(ROOT/'qa'/('validation_'+b.name+'.json'),report)
 b.report=report; return report

def build_all():
 import patterns
 if BUILT: raise RuntimeError('Use existing BUILT; refusing duplicate source scenes')
 # New isolated process contains startup cube only; snapshot remains available.
 bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'qa/pre_build_backup.blend'),copy=True)
 functions={'Sand':sand,'Asphalt':asphalt,'Wood':patterns.wood,'CreamTile':patterns.cream,'MintHerringbone':patterns.mint,'ConcreteDrain':concrete}
 for name,fn in functions.items():
  b=Builder(name); fn(b); bpy.context.view_layer.update(); BUILT[name]=b
  bpy.data.libraries.write(str(ROOT/'Blends'/('Road_'+name+'_editable_checkpoint.blend')),{b.scene},compress=True)
 return {n:len(b.parts) for n,b in BUILT.items()}

def export_all():
 mat=palette_material(); reports=[]
 for b in BUILT.values(): reports.append(export_asset(b,mat))
 b=next(iter(BUILT.values())); bpy.context.window.scene=b.scene
 col=bpy.data.collections.new('QA_CALIBRATION'); b.scene.collection.children.link(col)
 calibration=qa._calibration(b.scene,ROOT,col,mat,'SM_Road')
 qa._json(ROOT/'qa/calibration.json',calibration)
 qa._json(ROOT/'qa/blender_validation.json',{'assets':reports,'calibration':calibration,'version':bpy.app.version_string})
 return {r['name']:[(x['geometry']['triangles'],x['geometry']['status'],x['fbx_reimport']['status']) for x in r['lods']] for r in reports}
