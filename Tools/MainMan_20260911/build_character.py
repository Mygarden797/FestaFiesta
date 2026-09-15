"""Non-destructive reconstruction from MainMan source. Run in background Blender."""
import bpy,bmesh,math,json
from pathlib import Path
from mathutils import Vector
from mathutils.bvhtree import BVHTree
OUT=Path(__file__).resolve().parents[2]/'SourceArt/MainMan_20260911'
SCALE=170.0 # centimeters; preserve all normalized source proportions
source=next(o for o in bpy.data.objects if o.type=='MESH')
sm=source.data;sw=source.matrix_world.copy()
bm=bmesh.new();bm.from_mesh(sm);bm.verts.ensure_lookup_table()
remaining=set(bm.verts);components=[]
while remaining:
    seed=remaining.pop();todo=[seed];vs={seed}
    while todo:
        v=todo.pop()
        for e in v.link_edges:
            n=e.other_vert(v)
            if n in remaining:remaining.remove(n);todo.append(n);vs.add(n)
    components.append(vs)
components.sort(key=lambda vs:-len(vs))
parts=[]
for vs in components:
    vs=sorted(vs,key=lambda v:v.index);ix={v:i for i,v in enumerate(vs)};fs={f for v in vs for f in v.link_faces}
    parts.append(([tuple(sw@v.co) for v in vs],[tuple(ix[v] for v in f.verts) for f in fs]))
bm.free()
bpy.ops.object.mode_set(mode='OBJECT') if bpy.context.object and bpy.context.object.mode!='OBJECT' else None
for o in list(bpy.data.objects):bpy.data.objects.remove(o,do_unlink=True)
for c in list(bpy.data.collections):bpy.data.collections.remove(c)
scene=bpy.context.scene;scene.name='MainMan_Production';scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=.01;scene.unit_settings.length_unit='CENTIMETERS'
asset=bpy.data.collections.new('CHARACTER | MainMan');scene.collection.children.link(asset)
look=bpy.data.collections.new('PREVIEW | excluded from FBX');scene.collection.children.link(look)
materials={}
def linear(c):return c/12.92 if c<=.04045 else ((c+.055)/1.055)**2.4
def material(key,rgb):
    m=bpy.data.materials.new('M_MainMan_'+key);m.use_nodes=True;col=tuple(linear(c) for c in rgb)+(1,)
    m.diffuse_color=col;p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=col;p.inputs['Roughness'].default_value=.82;p.inputs['Specular IOR Level'].default_value=.22
    materials[key]=m;return m
for k,c in {'Skin':(.96,.65,.43),'Hair':(.3333,.2588,.2078),'Cloth':(.1922,.1804,.1765),'Shirt':(.165,.16,.155),'Eyes':(.055,.047,.04),'Sole':(.1725,.1686,.1686),'Trim':(.22,.21,.20),'Button':(.84,.84,.81),'Label':(.9,.9,.88)}.items():material(k,c)
objects=[];meta={};repairlog=[]
def mesh(name,verts,faces,mat,role,smooth=True,recalc=True):
    me=bpy.data.meshes.new(name+'_Mesh');me.from_pydata(verts,[],faces);me.update();o=bpy.data.objects.new(name,me);asset.objects.link(o);me.materials.append(materials[mat]);objects.append(o);meta[name]={'role':role}
    if recalc:normals(o)
    for p in me.polygons:p.use_smooth=smooth
    return o
def active(o):
    bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
def normals(o):
    b=bmesh.new();b.from_mesh(o.data);bmesh.ops.recalc_face_normals(b,faces=list(b.faces));b.to_mesh(o.data);b.free()
def clean(o,target=None,voxel=.0013):
    print('CLEAN',o.name,flush=True)
    raw=o.data.copy()
    active(o);b=bmesh.new();b.from_mesh(o.data)
    bmesh.ops.remove_doubles(b,verts=list(b.verts),dist=1e-7)
    bmesh.ops.dissolve_degenerate(b,edges=list(b.edges),dist=1e-7)
    for _ in range(5):
        excess=[e for e in b.edges if len(e.link_faces)>2]
        if not excess:break
        remove=set()
        for e in excess:remove.update(sorted(e.link_faces,key=lambda f:f.calc_area())[:len(e.link_faces)-2])
        bmesh.ops.delete(b,geom=list(remove),context='FACES')
    for _ in range(3):bmesh.ops.holes_fill(b,edges=[e for e in b.edges if e.is_boundary],sides=0)
    loose=[v for v in b.verts if not v.link_faces]
    if loose:bmesh.ops.delete(b,geom=loose,context='VERTS')
    bad=sum(not e.is_manifold for e in b.edges)
    bmesh.ops.recalc_face_normals(b,faces=list(b.faces));b.to_mesh(o.data);b.free()
    if bad:
        # Reconstruct from original face winding. Filling a damaged soup before
        # volume reconstruction can create spurious interior sheets.
        o.data=raw
        mod=o.modifiers.new('Closed surface reconstruction','REMESH');mod.mode='SMOOTH';mod.octree_depth=9;mod.scale=.9;mod.use_remove_disconnected=True;mod.threshold=.01;bpy.ops.object.modifier_apply(modifier=mod.name)
        mod=o.modifiers.new('Light surface cleanup','SMOOTH');mod.factor=.45;mod.iterations=2;bpy.ops.object.modifier_apply(modifier=mod.name)
    o.data.calc_loop_triangles()
    if target and len(o.data.loop_triangles)>target:
        mod=o.modifiers.new('Production topology budget','DECIMATE');mod.ratio=target/len(o.data.loop_triangles);bpy.ops.object.modifier_apply(modifier=mod.name)
    normals(o)
    for p in o.data.polygons:p.use_smooth=meta[o.name]['role']!='hair'
    repairlog.append({'object':o.name,'voxel_reconstructed':bool(bad),'faces':len(o.data.polygons)})
    print('CLEAN_DONE',o.name,len(o.data.polygons),flush=True)
    return o
def frompart(i,name,mat,role,target=None,smooth=True):
    v,f=parts[i];o=mesh(name,v,f,mat,role,smooth,False);return clean(o,target)
def cut(o,co,no,clear_outer=True):
    b=bmesh.new();b.from_mesh(o.data)
    bmesh.ops.bisect_plane(b,geom=list(b.verts)+list(b.edges)+list(b.faces),dist=1e-7,plane_co=co,plane_no=no,clear_outer=clear_outer,clear_inner=not clear_outer)
    bmesh.ops.holes_fill(b,edges=[e for e in b.edges if e.is_boundary],sides=0);b.to_mesh(o.data);b.free();normals(o)
def tube(name,rings,axis,mat,role,n=16):
    v=[];f=[]
    for center,r1,r2 in rings:
        for j in range(n):
            a=2*math.pi*j/n
            p=list(center)
            if axis=='Z':p[0]+=r1*math.cos(a);p[1]+=r2*math.sin(a)
            else:p[1]+=r1*math.cos(a);p[2]+=r2*math.sin(a)
            v.append(p)
    for k in range(len(rings)-1):
        for j in range(n):f.append((k*n+j,k*n+(j+1)%n,(k+1)*n+(j+1)%n,(k+1)*n+j))
    f+=[tuple(reversed(range(n))),tuple((len(rings)-1)*n+j for j in range(n))]
    return mesh(name,v,f,mat,role)
def sphere(name,loc,scale,mat,role,segments=16,rings=10):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments,ring_count=rings,location=loc);o=bpy.context.object;o.name=name;o.scale=scale;bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    for c in list(o.users_collection):c.objects.unlink(o)
    asset.objects.link(o);o.data.materials.clear();o.data.materials.append(materials[mat]);objects.append(o);meta[name]={'role':role}
    for p in o.data.polygons:p.use_smooth=True
    return o

# Explicit closed garment panels, preserving original outlines and placements.
def panel(name,rows,depth,mat='Cloth',role='torso',bevel=0):
    H=len(rows);W=len(rows[0]);vs=[tuple(p) for row in rows for p in row];vs+= [tuple(Vector(p)+Vector(depth)) for p in vs];fs=[];count=H*W
    for k in range(H-1):
        for j in range(W-1):
            a=k*W+j;f=(a,a+1,a+W+1,a+W);fs.extend([f,tuple(i+count for i in reversed(f))])
    boundary=list(range(W))+[k*W+W-1 for k in range(1,H)]+[(H-1)*W+j for j in range(W-2,-1,-1)]+[k*W for k in range(H-2,0,-1)]
    for i,a in enumerate(boundary):b=boundary[(i+1)%len(boundary)];fs.append((a,b,b+count,a+count))
    o=mesh(name,vs,fs,mat,role)
    if bevel:
        active(o);mod=o.modifiers.new('Tailored edge','BEVEL');mod.width=bevel;mod.segments=1;bpy.ops.object.modifier_apply(modifier=mod.name);normals(o)
    return o
torso=tube('Shirt_Torso',[((0,-.003,z),rx,ry) for z,rx,ry in [(.461,.085,.05),(.468,.087,.052),(.50,.087,.052),(.55,.084,.050),(.60,.086,.048),(.646,.092,.043),(.678,.108,.045),(.704,.106,.042),(.718,.073,.030),(.732,.028,.023)]],'Z','Shirt','torso',24)
specs=[(.428,.020,.121,-.083),(.440,.022,.119,-.082),(.49,.024,.113,-.078),(.55,.022,.105,-.073),(.60,.021,.103,-.067),(.645,.024,.102,-.060),(.688,.035,.110,-.052),(.708,.039,.104,-.043),(.723,.038,.071,-.027)]
for side,sg in [('L',1),('R',-1)]:
    rows=[]
    for z,inner,outer,y in specs:
        rows.append([(sg*(inner+(outer-inner)*t),y+.012*t*t,z) for t in [0,.14,.5,.84,1]])
    panel('Vest_Front_'+side,rows,(0,.005,0),bevel=.0007)
    rows=[]
    for k,(z,inner,outer,y) in enumerate(specs[:-1]):
        rows.append([(sg*outer,(y+.012)*(1-t)+(.059 if z>.6 else .071)*t,z) for t in [0,.25,.5,.75,1]])
    panel('Vest_Side_'+side,rows,(-sg*.004,0,0))
    rows=[]
    for z,outer,yc in [(.428,.121,.075),(.440,.119,.074),(.490,.113,.073),(.55,.105,.070),(.60,.103,.065),(.645,.104,.060),(.681,.111,.057),(.705,.102,.053),(.721,.065,.043),(.731,.025,.031)]:
        row=[]
        for t in [0,.18,.5,.82,1]:
            x=.001+(outer-.001)*t;zz=z+((.050 if z<.43 else .045)*max(0,1-t/.18) if z<.441 else 0)
            yy=yc-.015*t*t
            row.append((sg*x,yy,zz))
        rows.append(row)
    panel('Vest_Back_'+side,rows,(0,-.004,0),bevel=.0005)
    panel('Vest_Shoulder_'+side,[[(sg*x,y,z-.008) for x in [.041,.060,.078]] for y,z in [(-.030,.719),(-.006,.725),(.020,.724),(.043,.716)]],(0,0,-.003))
    # Original rolled sleeves; proper circumferential deformation loops.
    rings=[((sg*x,.008,.674),ry,rz) for x,ry,rz in [(.095,.046,.049),(.111,.046,.048),(.139,.044,.043),(.169,.041,.039),(.194,.038,.035),(.217,.036,.034)]]
    tube('Sleeve_'+side,rings,'X','Cloth','sleeve_'+side,16)
    tube('Sleeve_Cuff_'+side,[((sg*x,.008,.674),ry,rz) for x,ry,rz in [(.197,.039,.037),(.199,.041,.039),(.220,.041,.039),(.223,.038,.036)]],'X','Cloth','sleeve_'+side,16)
    # Closed front pocket with clipped corners and separate flap.
    rows=[[(sg*x,y,z) for x,y,z in row] for row in [[(.063,-.086,.461),(.087,-.089,.459),(.117,-.082,.464)],[(.060,-.089,.475),(.088,-.093,.474),(.12,-.085,.478)],[(.061,-.087,.516),(.089,-.092,.516),(.117,-.086,.520)],[(.065,-.084,.531),(.089,-.089,.531),(.115,-.083,.534)]]]
    panel('Pocket_'+side,rows,(0,.008,0),bevel=.0014)
    rows=[[(sg*x,y,z) for x,y,z in row] for row in [[(.063,-.094,.517),(.089,-.098,.515),(.120,-.091,.521)],[(.063,-.091,.536),(.089,-.095,.536),(.114,-.089,.540)],[(.065,-.089,.540),(.09,-.093,.541),(.113,-.087,.544)]]]
    panel('Pocket_Flap_'+side,rows,(0,.005,0),bevel=.001)
    # Readable chest welt and hem, at the reference positions.
    panel('Chest_Welt_'+side,[[(sg*x,-.066,z) for x in [.045,.077]] for z in [.597,.600]],(0,.002,0),'Eyes',bevel=.0004)
    panel('Vest_Hem_'+side,[[(sg*x,-.085,z) for x in [.021,.07,.121]] for z in [.428,.435]],(0,.003,0),'Cloth',bevel=.0004)
tie=frompart(14,'Tie','Cloth','tie',420)
for v in tie.data.vertices:v.co.y-=.024*max(0,min(1,(v.co.z-.61)/.08))
collar=frompart(16,'Collar','Cloth','collar',380)
for v in collar.data.vertices:v.co.y-=.022*max(0,min(1,(.015-v.co.y)/.04))
adjuster=frompart(19,'Back_Adjuster','Cloth','torso',220)
for v in adjuster.data.vertices:v.co.y+=.009
# Radial retopology of the actual source hair. Keep its asymmetric solid shape.
hv,hf=parts[6];hb=BVHTree.FromPolygons([Vector(v) for v in hv],hf);hc=Vector((0,.018,.871));AZ=36;RINGS=9
def hairhit(theta,phi):
    d=Vector((math.sin(theta)*math.cos(phi),math.sin(theta)*math.sin(phi),math.cos(theta)))
    hit=hb.ray_cast(hc+d*.6,-d,1.2)[0]
    return hit if hit is not None and (hit-hc).dot(d)>.048 else None
ends=[]
for j in range(AZ):
    phi=2*math.pi*j/AZ;last=.1
    for k in range(1,80):
        theta=.1+k*.034
        if hairhit(theta,phi) is None:break
        last=theta
    ends.append(last)
top=hairhit(0,0);vs=[tuple(top)];fs=[]
for k in range(1,RINGS+1):
    for j in range(AZ):
        hit=hairhit(ends[j]*k/RINGS,2*math.pi*j/AZ)
        vs.append(tuple(hit if hit is not None else hc+Vector((0,0,.12))))
for j in range(AZ):fs.append((0,1+j,1+(j+1)%AZ))
for k in range(RINGS-1):
    for j in range(AZ):
        a=1+k*AZ+j;b=1+k*AZ+(j+1)%AZ;c=b+AZ;d=a+AZ
        fs.extend([(a,b,c),(a,c,d)] if (k+j)%2 else [(a,b,d),(b,c,d)])
used=set()
for point in [max(hv,key=lambda p:p[2]),max(hv,key=lambda p:p[0]),min(hv,key=lambda p:p[0]),min([p for p in hv if p[2]>.87],key=lambda p:p[1])]:
    index=min((i for i in range(len(vs)) if i not in used),key=lambda i:(Vector(vs[i])-Vector(point)).length);vs[index]=point;used.add(index)
center=len(vs);vs.append(tuple(hc))
for j in range(AZ):fs.append((center,1+(RINGS-1)*AZ+(j+1)%AZ,1+(RINGS-1)*AZ+j))
hair=mesh('Hair_Solid_Asymmetric',vs,fs,'Hair','hair',False)

# Quad facial rings matched to the supplied front and side references.
verts=[];faces=[];N=20
headrings=[(.747,.023,-.065,.022),(.758,.038,-.075,.035),(.777,.060,-.080,.055),(.800,.073,-.080,.070),(.830,.078,-.080,.080),(.865,.076,-.080,.082),(.903,.062,-.064,.074),(.929,.039,-.048,.047),(.938,.020,-.024,.023)]
for z,rx,front,back in headrings:
    for j in range(N):
        a=2*math.pi*j/N;cx=math.cos(a);sy=math.sin(a)
        x=rx*cx;y=back*sy if sy>0 else front*min(1,abs(sy)/.72)
        # Tiny nose blend, not a separate facial prop.
        if y<0:y-=.0035*math.exp(-(x/.014)**2-((z-.806)/.012)**2)
        verts.append((x,y,z))
for k in range(len(headrings)-1):
    for j in range(N):faces.append((k*N+j,k*N+(j+1)%N,(k+1)*N+(j+1)%N,(k+1)*N+j))
faces.extend([tuple(reversed(range(N))),tuple((len(headrings)-1)*N+j for j in range(N))])
head=mesh('Head_QuadSurface',verts,faces,'Skin','head',False)
tube('Neck',[((0,.008,z),rx,ry) for z,rx,ry in [(.702,.022,.026),(.719,.023,.028),(.737,.024,.028),(.756,.025,.029)]],'Z','Skin','head',16)
for side,sg in [('L',1),('R',-1)]:
    # Preserve the sculpted source ear, clipping only the concealed attachment.
    v,f=parts[3];ear=mesh('Ear_'+side,v,f,'Skin','head',True,False)
    cut(ear,(sg*.081,0,0),(sg,0,0),False);cut(ear,(0,0,.781),(0,0,1),False);cut(ear,(0,0,.867),(0,0,1),True);clean(ear,200)
    for p in ear.data.polygons:p.use_smooth=False
# Avoid texture dependency: small solid, closed facial forms touching the skin.
bvh=BVHTree.FromPolygons([v.co for v in head.data.vertices],[p.vertices for p in head.data.polygons])
def face_y(x,z):
    hit=bvh.ray_cast(Vector((x,-.3,z)),Vector((0,1,0)))
    return hit[0].y if hit[0] is not None else -.073
for side,sg in [('L',1),('R',-1)]:
    x=sg*.034;z=.831;y=face_y(x,z)
    # Capsule outline with straight sides, as in the front reference.
    v=[];f=[];outline=[];radius=.0059;straight=.008
    for center,a0 in [(z+straight,0),(z-straight,math.pi)]:
        for i in range(9):
            a=a0+math.pi*i/8;outline.append((x+radius*math.cos(a),center+radius*math.sin(a)))
    for offset in [-.0018,.0007]:
        for xx,zz in outline:v.append((xx,face_y(xx,zz)+offset,zz))
    n=len(outline);f=[tuple(range(n)),tuple(reversed(range(n,2*n)))]+[(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    eye=mesh('Eye_'+side,v,f,'Eyes','head')
    x=sg*.034;z=.86;y=face_y(x,z)
    bpy.ops.mesh.primitive_cube_add(size=1,location=(x,y-.001,z));brow=bpy.context.object;brow.name='Brow_'+side;brow.dimensions=(.028,.004,.007);bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    for c in list(brow.users_collection):c.objects.unlink(brow)
    asset.objects.link(brow);brow.data.materials.append(materials['Hair']);objects.append(brow);meta[brow.name]={'role':'head'};mod=brow.modifiers.new('Soft corners','BEVEL');mod.width=.0015;mod.segments=2;bpy.ops.object.modifier_apply(modifier=mod.name)

# Clean closed shoe rings match the original footprint and thick soles.
def shoe_rings(side,sg,name,specs,mat,smooth):
    v=[];f=[];n=24
    for z,rx,ry,cy in specs:
        for j in range(n):
            a=2*math.pi*j/n;c=math.cos(a);s=math.sin(a)
            v.append((sg*.074+rx*math.copysign(abs(c)**.75,c),cy+ry*math.copysign(abs(s)**.75,s),z))
    for k in range(len(specs)-1):
        for j in range(n):f.append((k*n+j,k*n+(j+1)%n,(k+1)*n+(j+1)%n,(k+1)*n+j))
    f.extend([tuple(reversed(range(n))),tuple((len(specs)-1)*n+j for j in range(n))]);return mesh(name+'_'+side,v,f,mat,'shoe_'+side,smooth)
for side,sg in [('L',1),('R',-1)]:
    shoe_rings(side,sg,'Shoe_Sole',[(.001,.061,.113,-.042),(.004,.064,.116,-.042),(.027,.063,.114,-.042),(.034,.060,.111,-.041)],'Sole',False)
    upper=shoe_rings(side,sg,'Shoe_Upper',[(.029,.058,.109,-.040),(.038,.058,.108,-.040),(.055,.054,.101,-.035),(.070,.048,.088,-.023),(.084,.042,.068,-.009),(.099,.035,.045,.012),(.108,.031,.040,.015)],'Cloth',True)
    ubvh=BVHTree.FromPolygons([v.co for v in upper.data.vertices],[p.vertices for p in upper.data.polygons])
    for k,(y,z) in enumerate([(-.092,.066),(-.076,.078),(-.060,.088)]):
        for direction in [-1,1]:
            row=[]
            for t in [0,.5,1]:
                xx=sg*.074+(-.022+.044*t);yy=y+direction*(t-.5)*.01
                hit=ubvh.ray_cast(Vector((xx,yy,.3)),Vector((0,0,-1)))[0]
                row.append((xx,yy,(hit.z if hit else z)+.0018))
            panel(f'Lace_{side}_{k}_{direction}',[[(x,yy-.0018,zz) for x,yy,zz in row],[(x,yy+.0018,zz) for x,yy,zz in row]],(0,0,-.002),'Trim','shoe_'+side,bevel=.0007)
for i in [17,18]:
    sg=1 if sum(v[0] for v in parts[i][0])>0 else -1;side='L' if sg>0 else 'R'
    frompart(i,'Shoe_Strap_'+side,'Trim','shoe_'+side,180)
# Matched button placements; all intersect the garment face.
for name,x,y,z in [('Pocket_Button_L',.093,-.095,.528),('Pocket_Button_R',-.093,-.095,.528),('Back_Button_L',.0535,.079,.536),('Back_Button_R',-.0535,.079,.536)]:
    sphere(name,(x,y,z),(.0052,.0044,.0052),'Button','torso',12,8)
# STAFF marking rebuilt as clean extruded geometry in the original back placement.
bpy.ops.object.text_add(location=(0,.063,.624),rotation=(math.pi/2,0,math.pi));label=bpy.context.object;label.name='Back_STAFF';label.data.body='STAFF';label.data.align_x='CENTER';label.data.size=.059;label.data.extrude=.0012;label.data.bevel_depth=0;label.data.resolution_u=3
label.data.font=bpy.data.fonts.load('C:/Windows/Fonts/arialbd.ttf')
bpy.ops.object.convert(target='MESH');label=bpy.context.object;bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
b=bmesh.new();b.from_mesh(label.data);bmesh.ops.remove_doubles(b,verts=list(b.verts),dist=1e-6);bmesh.ops.triangulate(b,faces=list(b.faces));b.to_mesh(label.data);b.free()
xmin=min(v.co.x for v in label.data.vertices);xmax=max(v.co.x for v in label.data.vertices);zmin=min(v.co.z for v in label.data.vertices);zmax=max(v.co.z for v in label.data.vertices)
backbvh={side:BVHTree.FromPolygons([v.co for v in bpy.data.objects['Vest_Back_'+side].data.vertices],[p.vertices for p in bpy.data.objects['Vest_Back_'+side].data.polygons]) for side in ['L','R']}
for v in label.data.vertices:
    v.co.x=(v.co.x-(xmin+xmax)/2)*.135/(xmax-xmin);v.co.z=.624+(v.co.z-zmin)*.048/(zmax-zmin)
    hit=backbvh['L' if v.co.x>=0 else 'R'].ray_cast(Vector((v.co.x,.3,v.co.z)),Vector((0,-1,0)))[0]
    v.co.y=(hit.y if hit else .06)+.0012+(v.co.y-.063)
for c in list(label.users_collection):c.objects.unlink(label)
asset.objects.link(label);label.data.materials.append(materials['Label']);objects.append(label);meta[label.name]={'role':'torso'};normals(label)

# Independent leg loops with extra rings surrounding knee and hip.
for side,sg in [('L',1),('R',-1)]:
    specs=[(.087,.076,.023,.026),(.108,.067,.025,.028),(.14,.057,.029,.036),(.16,.055,.029,.037),(.20,.055,.031,.039),(.25,.055,.033,.041),(.276,.055,.034,.042),(.288,.055,.034,.0425),(.3,.055,.034,.043),(.315,.055,.035,.044),(.35,.054,.037,.046),(.39,.052,.040,.049),(.423,.048,.044,.053),(.445,.047,.046,.055),(.469,.047,.046,.056),(.483,.047,.045,.055)]
    rings=[((sg*x,-.005,z),rx,ry) for z,x,rx,ry in specs]
    tube('Trouser_Leg_'+side,rings,'Z','Cloth','leg_'+side,16)
    rings=[((sg*.056,-.005,z),rx,ry) for z,rx,ry in [(.136,.030,.039),(.139,.032,.041),(.160,.032,.041),(.163,.030,.039)]]
    tube('Trouser_Cuff_'+side,rings,'Z','Trim','cuff_'+side,16)
    specs=[(.118,.028,.028),(.148,.028,.027),(.19,.025,.025),(.218,.023,.022),(.24,.021,.020),(.25,.0195,.019),(.259,.0185,.018),(.27,.018,.017),(.288,.016,.015),(.316,.0115,.012),(.332,.0095,.0095),(.342,.009,.009)]
    tube('Arm_'+side,[((sg*x,.008,.674),ry,rz) for x,ry,rz in specs],'X','Skin','arm_'+side,12)

# Connected hand surface: palm grid with five branch openings and knuckle rings.
fingerpaths={}
def hand(side,sg):
    verts=[];faces=[];weights=[]
    def V(p,w='hand'):
        verts.append((sg*p[0],p[1],p[2]));weights.append(w);return len(verts)-1
    xs=[.325,.341,.351,.366,.373];ys=[-.025,-.0125,0,.0125,.025];grid={}
    for layer in range(2):
        for i,x in enumerate(xs):
            for j,y in enumerate(ys):
                fy=(x-.332)/(.373-.332);width=.42+.58*fy
                grid[layer,i,j]=V((x,.008+y*width,.674+(-1 if layer==0 else 1)*(.0085 if i<3 else .0075)))
    for l in range(2):
        for i in range(4):
            for j in range(4):faces.append((grid[l,i,j],grid[l,i+1,j],grid[l,i+1,j+1],grid[l,i,j+1]))
    for i in range(4):
        for j in [0,4]:
            if j==0 and i==2:continue # thumb root
            faces.append((grid[0,i,j],grid[1,i,j],grid[1,i+1,j],grid[0,i+1,j]))
    for j in range(4):faces.append((grid[0,0,j],grid[0,0,j+1],grid[1,0,j+1],grid[1,0,j]))
    for j,(name,length,spread) in enumerate([('index',.051,-.011),('middle',.059,-.003),('ring',.052,.006),('pinky',.037,.014)]):
        prev=[grid[0,4,j],grid[0,4,j+1],grid[1,4,j+1],grid[1,4,j]]
        y=.008+(ys[j]+ys[j+1])/2;path=[]
        for t in [0,.34,.69,1]:path.append(Vector((sg*(.373+length*t),y+spread*t,.674)))
        fingerpaths[(side,name)]=path
        for t in [.10,.27,.34,.41,.61,.69,.77,.93,1.0]:
            x=.373+length*t;cy=y+spread*t;rad=.006*(1-.38*t);rz=.007*(1-.42*t)
            if t==1:rad*=.5;rz*=.5
            bone=1 if t<.34 else 2 if t<.69 else 3
            ids=[V((x,cy-rad,.674-rz),(name,bone)),V((x,cy+rad,.674-rz),(name,bone)),V((x,cy+rad,.674+rz),(name,bone)),V((x,cy-rad,.674+rz),(name,bone))]
            for k in range(4):faces.append((prev[k],prev[(k+1)%4],ids[(k+1)%4],ids[k]))
            prev=ids
        faces.append(tuple(prev))
    prev=[grid[0,2,0],grid[0,3,0],grid[1,3,0],grid[1,2,0]]
    start=Vector((.3585,-.010,.674));end=Vector((.389,-.049,.670));path=[Vector((sg*(start+(end-start)*t).x,(start+(end-start)*t).y,(start+(end-start)*t).z)) for t in [0,.35,.7,1]];fingerpaths[(side,'thumb')]=path
    for t in [.18,.3,.38,.61,.7,.78,.94,1]:
        c=start.lerp(end,t);rad=.009*(1-.5*t);rz=.008*(1-.4*t)
        if t==1:rad*=.5;rz*=.5
        bone=1 if t<.35 else 2 if t<.7 else 3
        ids=[V((c.x-rad,c.y,c.z-rz),('thumb',bone)),V((c.x+rad,c.y,c.z-rz),('thumb',bone)),V((c.x+rad,c.y,c.z+rz),('thumb',bone)),V((c.x-rad,c.y,c.z+rz),('thumb',bone))]
        for k in range(4):faces.append((prev[k],prev[(k+1)%4],ids[(k+1)%4],ids[k]))
        prev=ids
    faces.append(tuple(prev));o=mesh('Hand_FiveFingers_'+side,verts,faces,'Skin','hand_'+side)
    for i,w in enumerate(weights):
        name='hand_'+side.lower() if w=='hand' else f'{w[0]}_{w[1]:02}_{side.lower()}'
        vg=o.vertex_groups.get(name) or o.vertex_groups.new(name=name);vg.add([i],1,'REPLACE')
    active(o);sub=o.modifiers.new('Rounded low-poly hand loops','SUBSURF');sub.levels=1;bpy.ops.object.modifier_apply(modifier=sub.name);normals(o)
    return o
hand('L',1);hand('R',-1)

# Consistent deformation skeleton, centimeters, identity object transforms.
arm=bpy.data.armatures.new('MainMan_Skeleton');rig=bpy.data.objects.new('MainMan_Rig',arm);asset.objects.link(rig);active(rig);bpy.ops.object.mode_set(mode='EDIT')
def bone(name,head,tail,parent=None,deform=True):
    b=arm.edit_bones.new(name);b.head=Vector(head)*SCALE;b.tail=Vector(tail)*SCALE;b.use_deform=deform
    if parent:b.parent=arm.edit_bones[parent]
    return b
bone('root',(0,0,0),(0,0,.05),None,False)
bone('pelvis',(0,0,.43),(0,0,.49),'root')
bone('spine_01',(0,0,.49),(0,0,.58),'pelvis')
bone('spine_02',(0,0,.58),(0,0,.673),'spine_01')
bone('neck_01',(0,0,.673),(0,0,.745),'spine_02')
bone('head',(0,0,.745),(0,0,.9),'neck_01')
for side,sg in [('L',1),('R',-1)]:
    s=side.lower()
    bone('clavicle_'+s,(sg*.016,0,.674),(sg*.12,.008,.674),'spine_02')
    bone('upperarm_'+s,(sg*.12,.008,.674),(sg*.255,.008,.674),'clavicle_'+s)
    bone('lowerarm_'+s,(sg*.255,.008,.674),(sg*.337,.008,.674),'upperarm_'+s)
    bone('hand_'+s,(sg*.337,.008,.674),(sg*.373,.008,.674),'lowerarm_'+s)
    bone('thigh_'+s,(sg*.048,-.005,.443),(sg*.056,-.005,.29),'pelvis')
    bone('calf_'+s,(sg*.056,-.005,.29),(sg*.075,-.005,.103),'thigh_'+s)
    bone('foot_'+s,(sg*.075,-.005,.103),(sg*.075,-.095,.038),'calf_'+s)
    bone('ball_'+s,(sg*.075,-.095,.038),(sg*.075,-.148,.033),'foot_'+s)
    for name in ['thumb','index','middle','ring','pinky']:
        path=fingerpaths[side,name]
        for i in range(3):bone(f'{name}_{i+1:02}_{s}',path[i],path[i+1],'hand_'+s if i==0 else f'{name}_{i:02}_{s}')
bpy.ops.object.mode_set(mode='OBJECT');rig.show_in_front=True;arm.display_type='OCTAHEDRAL'
def clamp(x):return max(0,min(1,x))
def blend(a,b,t):t=clamp(t);return {a:1-t,b:t}
def torso_weights(z):
    if z<.5:return {'pelvis':1}
    if z<.57:return blend('pelvis','spine_01',(z-.5)/.07)
    return blend('spine_01','spine_02',(z-.57)/.065)
for o in objects:
    role=meta[o.name]['role']
    if not role.startswith('hand_'):
        o.vertex_groups.clear()
        for v in o.data.vertices:
            x,y,z=v.co
            if role in ['head','hair']:w={'head':1} if z>.748 else blend('neck_01','head',(z-.724)/.024)
            elif role=='collar':w={'spine_02':1}
            elif role=='tie':w=torso_weights(z)
            elif role.startswith(('leg_','cuff_')):
                side=role[-1].lower()
                if role.startswith('cuff'):w={'calf_'+side:1}
                elif z>.425:w=blend('thigh_'+side,'pelvis',(z-.425)/.055)
                else:w=blend('calf_'+side,'thigh_'+side,(z-.265)/.05)
            elif role.startswith('shoe_'):w={'foot_'+role[-1].lower():1}
            elif role.startswith(('arm_','sleeve_')):
                side=role[-1].lower();x=abs(x)
                if x<.148:w=blend('spine_02','upperarm_'+side,(x-.096)/.052)
                elif x<.28:w=blend('upperarm_'+side,'lowerarm_'+side,(x-.237)/.036)
                else:w=blend('lowerarm_'+side,'hand_'+side,(x-.327)/.015)
            else:w=torso_weights(z)
            for name,weight in w.items():
                if weight>1e-7:
                    vg=o.vertex_groups.get(name) or o.vertex_groups.new(name=name);vg.add([v.index],weight,'REPLACE')
    # Normalize interpolation created by subdivision and cap to four influences.
    for v in o.data.vertices:
        gs=sorted([(g.group,g.weight) for g in v.groups if g.weight>1e-6],key=lambda t:-t[1])[:4];total=sum(w for _,w in gs)
        for g in list(v.groups):o.vertex_groups[g.group].remove([v.index])
        for i,w in gs:o.vertex_groups[i].add([v.index],w/total,'REPLACE')
    for v in o.data.vertices:v.co*=SCALE
    o.data.update();o.parent=rig;mod=o.modifiers.new('MainMan deformation','ARMATURE');mod.object=rig;mod.use_deform_preserve_volume=False
    o['part_role']=role;o['source']='MainMan.blend';o['attachment']='Skinned; concealed attachment surfaces intentionally overlap'
    # UV layout is stored even though the asset uses solid materials.
    active(o);bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.uv.smart_project(angle_limit=math.radians(66),island_margin=.015);bpy.ops.object.mode_set(mode='OBJECT')
    o.data.uv_layers.active.name='UV0'
rig['rig_type']='Custom FK deformation skeleton; T-pose; five fingers per hand'
rig['height_cm']=SCALE*.9995117
rig['ue_note']='Custom skeleton; create an IK Retargeter for Manny/Quinn animations'

# Studio cameras/lights are optional review helpers, not export geometry.
def helper(o):
    for c in list(o.users_collection):c.objects.unlink(o)
    look.objects.link(o)
for name,loc,power,size in [('Key',(-2,-3,4),150,3),('Fill',(2,-1,2),85,2),('Rim',(0,2,3),130,2)]:
    bpy.ops.object.light_add(type='AREA',location=Vector(loc)*SCALE);o=bpy.context.object;o.name=name;o.data.energy=power*SCALE*SCALE;o.data.shape='DISK';o.data.size=size*SCALE;o.rotation_euler=(Vector((0,0,.5))*SCALE-o.location).to_track_quat('-Z','Y').to_euler();helper(o)
bpy.ops.object.camera_add();cam=bpy.context.object;cam.name='Preview_Camera';helper(cam);scene.camera=cam;cam.data.type='ORTHO';cam.data.ortho_scale=1.14*SCALE;cam.data.clip_end=3000
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.render.resolution_x=1400;scene.render.resolution_y=1400;scene.render.resolution_percentage=100;scene.render.image_settings.file_format='PNG';scene.render.image_settings.color_mode='RGBA';scene.render.film_transparent=True;scene.world.color=(.3,.3,.3);scene.view_settings.view_transform='Standard'
def camera(loc,target=(0,0,.5),scale=1.14):
    cam.location=Vector(loc)*SCALE;cam.rotation_euler=(Vector(target)*SCALE-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=scale*SCALE
camera((1.35,-3,1.2))
# Remove unused packed 8K images and original material datablocks from delivery.
for im in list(bpy.data.images):
    if im.name not in ['Render Result','Viewer Node']:bpy.data.images.remove(im)
for m in list(bpy.data.materials):
    if m not in materials.values():bpy.data.materials.remove(m)
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type=='VIEW_3D':
            area.spaces.active.shading.type='MATERIAL';area.spaces.active.region_3d.view_distance=2.3*SCALE;area.spaces.active.region_3d.view_location=Vector((0,0,.52))*SCALE
            area.spaces.active.region_3d.view_rotation=cam.rotation_euler.to_quaternion()
active(torso);bpy.context.view_layer.update()
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'MainMan_Production.blend'))
(OUT/'qa/repair_log.json').write_text(json.dumps(repairlog,indent=2))
for name,loc,target,scale in [('final_front',(0,-3,.55),(0,0,.5),1.12),('final_three_quarter',(1.35,-3,1.2),(0,0,.5),1.14),('final_back',(0,3,.55),(0,0,.5),1.12),('final_hand',(.40,-.20,1.05),(.38,0,.674),.16)]:
    camera(loc,target,scale);scene.render.filepath=str(OUT/'qa'/(name+'.png'));bpy.ops.render.render(write_still=True)
print('BUILD_COMPLETE',len(objects),sum(len(o.data.polygons) for o in objects))
