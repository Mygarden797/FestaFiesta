# expansion70_common.py
# 역할: 시설 70종 제작에 사용하는 Blender 도형·재질·컬렉션 구성 함수를 제공한다.

"""Reusable, scoped Blender authoring primitives for the Facility70 collection.

All dimensions are metres. Only newly created task collections are modified.
The render export is derived from editable VIS parts; COL and GUIDES remain separate.
"""
import math
import bpy
from mathutils import Vector

PALETTE = {
    'Teal': ((.012,.36,.40,1),.52,0,0),
    'DarkTeal': ((.009,.095,.115,1),.55,.1,0),
    'Cream': ((.88,.82,.67,1),.62,0,0),
    'White': ((.94,.94,.89,1),.48,0,0),
    'Wood': ((.54,.29,.105,1),.72,0,0),
    'DarkWood': ((.19,.085,.038,1),.8,0,0),
    'Metal': ((.07,.095,.105,1),.36,.65,0),
    'Glass': ((.035,.19,.24,1),.18,.25,0),
    'Amber': ((1,.52,.10,1),.32,0,2),
    'Coral': ((.8,.15,.085,1),.58,0,0),
    'Green': ((.12,.39,.13,1),.78,0,0),
    'Blue': ((.04,.29,.62,1),.5,0,0),
    'Pink': ((.80,.30,.47,1),.6,0,0),
    'Sand': ((.65,.45,.23,1),.86,0,0),
    'Screen': ((.025,.08,.105,1),.3,0,.2),
}


def palette_manifest():
    return {'M_F70_'+k:{'base_color':list(v[0]),'roughness':v[1],
            'metallic':v[2],'emissive_strength':v[3]} for k,v in PALETTE.items()}


class Builder:
    def __init__(self, entry):
        self.entry = entry
        self.scene = bpy.data.scenes.new('FAC_'+entry['id'])
        self.scene['FF70.Owner'] = entry['id']
        bpy.context.window.scene = self.scene
        self.scene.unit_settings.system = 'METRIC'
        self.scene.unit_settings.scale_length = 1.0
        self.root = bpy.data.collections.new('FAC_'+entry['id'])
        self.scene.collection.children.link(self.root)
        self.vis = self.collection('VIS')
        self.col = self.collection('COL')
        self.guides = self.collection('GUIDES')
        self.preview = self.collection('PREVIEW_ONLY')
        self.col.hide_render = True
        self.guides.hide_render = True
        self.materials = {}
        self.part_number = 0
        for key,(color,rough,metal,emission) in PALETTE.items():
            m = bpy.data.materials.get('M_F70_'+key) or bpy.data.materials.new('M_F70_'+key)
            m.use_nodes = True
            m.diffuse_color = color
            bs = m.node_tree.nodes.get('Principled BSDF')
            bs.inputs['Base Color'].default_value = color
            bs.inputs['Roughness'].default_value = rough
            bs.inputs['Metallic'].default_value = metal
            bs.inputs['Emission Color'].default_value = color
            bs.inputs['Emission Strength'].default_value = emission
            self.materials[key] = m

    def collection(self, name):
        c = bpy.data.collections.new(name)
        self.root.children.link(c)
        return c

    def own(self,obj,name,mat=None,target=None):
        self.part_number += 1
        obj.name = '%03d_%s' % (self.part_number,name)
        for c in list(obj.users_collection): c.objects.unlink(obj)
        (target or self.vis).objects.link(obj)
        obj['FF70.Part'] = name
        if mat: obj.data.materials.append(self.materials[mat])
        return obj

    def hull(self, obj):
        # Called only for convex primitives or deliberately convex custom parts.
        c = obj.copy()
        c.data = obj.data.copy()
        c.modifiers.clear()
        self.col.objects.link(c)
        c.name = 'UCX_SM_FF_%s_%02d' % (self.entry['id'],len(self.col.objects)-1)
        c.display_type = 'WIRE'
        c.hide_render = True
        c.data.materials.clear()
        return c

    def box(self,name,loc,dims,mat='Teal',bevel=.025,rotation=(0,0,0),collision=False):
        bpy.ops.mesh.primitive_cube_add(size=1,location=loc,rotation=rotation)
        o=self.own(bpy.context.object,name,mat)
        o.dimensions=dims
        bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
        if bevel:
            m=o.modifiers.new('Soft manufactured edges','BEVEL')
            m.width=min(bevel,min(dims)*.2);m.segments=2
        if collision:self.hull(o)
        return o

    def cyl(self,name,loc,radius,depth,mat='Metal',vertices=24,rotation=(0,0,0),collision=False):
        bpy.ops.mesh.primitive_cylinder_add(vertices=vertices,radius=radius,depth=depth,location=loc,rotation=rotation)
        o=self.own(bpy.context.object,name,mat)
        if collision:self.hull(o)
        return o

    def sphere(self,name,loc,scale,mat='Cream'):
        bpy.ops.mesh.primitive_uv_sphere_add(segments=20,ring_count=12,radius=1,location=loc)
        o=self.own(bpy.context.object,name,mat);o.scale=scale
        bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
        for f in o.data.polygons:f.use_smooth=True
        return o

    def torus(self,name,loc,major,minor,mat='Metal',rotation=(math.pi/2,0,0)):
        bpy.ops.mesh.primitive_torus_add(major_radius=major,minor_radius=minor,major_segments=32,minor_segments=8,location=loc,rotation=rotation)
        o=self.own(bpy.context.object,name,mat)
        for f in o.data.polygons:f.use_smooth=True
        return o

    def beam(self,name,start,end,radius,mat='Metal',collision=False):
        a,z=Vector(start),Vector(end);d=z-a
        o=self.cyl(name,(a+z)/2,radius,d.length,mat,12,collision=collision)
        o.rotation_euler=d.to_track_quat('Z','Y').to_euler()
        if collision:list(self.col.objects)[-1].rotation_euler=o.rotation_euler
        return o

    def mesh(self,name,verts,faces,mat='Teal',collision=False):
        data=bpy.data.meshes.new(name);data.from_pydata(verts,[],faces);data.update()
        o=bpy.data.objects.new(name,data);self.vis.objects.link(o);self.own(o,name,mat)
        if collision:self.hull(o)
        return o

    def text(self,name,body,loc,size,mat='Cream',rotation=(math.pi/2,0,math.pi)):
        data=bpy.data.curves.new(name,'FONT');data.body=body
        data.align_x='CENTER';data.align_y='CENTER';data.size=size
        data.extrude=.003;data.bevel_depth=.001;data.resolution_u=4
        o=bpy.data.objects.new(name,data);self.vis.objects.link(o)
        o.location=loc;o.rotation_euler=rotation
        return self.own(o,name,mat)

    def curve(self,name,points,radius,mat='Teal'):
        data=bpy.data.curves.new(name,'CURVE');data.dimensions='3D'
        data.resolution_u=12;data.bevel_depth=radius;data.bevel_resolution=2
        data.use_fill_caps=True
        sp=data.splines.new('POLY');sp.points.add(len(points)-1)
        for p,co in zip(sp.points,points):p.co=(*co,1)
        o=bpy.data.objects.new(name,data);self.vis.objects.link(o)
        return self.own(o,name,mat)

    def guide(self,name,loc):
        o=bpy.data.objects.new(name,None);self.guides.objects.link(o)
        o.location=loc;o.empty_display_type='ARROWS';o.empty_display_size=.35
        return o

    def bench(self,name,x,y,width=1.8):
        for i in range(3):self.box(name+'Slat',(x,y+(i-1)*.16,.49),(width,.14,.08),'Wood')
        self.box(name+'Back',(x,y-.21,.79),(width,.09,.48),'Teal')
        for dx in (-width*.35,width*.35):
            self.box(name+'Leg',(x+dx,y,.25),(.12,.46,.48),'Metal',collision=True)
        self.box(name+'SeatHull',(x,y,.44),(width,.48,.09),'DarkWood',collision=True)

    def table(self,name,x,y,width=1.8,depth=.8):
        self.box(name+'Top',(x,y,.78),(width,depth,.12),'Wood',collision=True)
        for dx in (-width*.38,width*.38):
            for dy in (-depth*.32,depth*.32):self.box(name+'Leg',(x+dx,y+dy,.39),(.1,.1,.72),'Metal')

    def kiosk(self,name,width=4,depth=2.5,accent='Teal'):
        self.box(name+'Deck',(0,0,.065),(width,depth,.13),'DarkWood',collision=True)
        self.box(name+'Back',(0,-depth*.44,1.28),(width*.95,.12,2.4),'Cream',collision=True)
        self.box(name+'Counter',(0,depth*.26,.60),(width*.84,.55,1.05),accent,collision=True)
        self.box(name+'Countertop',(0,depth*.28,1.16),(width*.9,.74,.12),'Wood')
        for x in (-width*.45,width*.45):
            for y in (-depth*.42,depth*.35):self.box(name+'Post',(x,y,1.37),(.13,.13,2.65),'Metal',collision=True)
        for i in range(10):
            self.box(name+'AwningStripe',(-width*.45+i*width/10,0,2.63),(width/10+.005,depth, .14),accent if i%2==0 else 'Cream',rotation=(.10,0,0))
        self.box(name+'Header',(0,-.3,2.86),(width*.68,.18,.48),accent)
        return self

    def truck(self,name,accent='Teal'):
        self.box(name+'Chassis',(0,0,.75),(5.5,2.25,.65),accent,collision=True)
        self.box(name+'Rear',( -2.57,0,1.76),(.18,2.24,2.1),'Cream',collision=True)
        self.box(name+'BackWall',( -.4,-1.02,1.8),(4.4,.18,2),'Cream',collision=True)
        self.box(name+'FrontPanel',(-2.12,1.06,1.84),(.74,.13,1.92),'Cream',collision=True)
        self.box(name+'Cab',(2.06,0,1.65),(1.38,2.23,1.3),'Cream',collision=True)
        self.box(name+'Windshield',(2.772,0,1.99),(.035,1.94,.78),'Glass',rotation=(0,-.13,0))
        self.box(name+'SideWindow',(2.02,1.126,2.09),(.85,.025,.56),'Glass')
        self.box(name+'Roof',(0,0,2.83),(5.55,2.32,.19),'Cream')
        self.box(name+'Counter',(-.44,1.27,1.30),(3.48,.68,.14),'Wood')
        self.box(name+'ServiceDarkInterior',(-.45,-.72,1.94),(3.3,.04,1.14),'DarkTeal')
        for i in range(10):self.box(name+'Awning',(-2.03+i*.36,1.29,2.48),(.365,.93,.13),accent if i%2==0 else 'Cream',rotation=(.14,0,0))
        for x in (-1.90,1.89):
            for y in (-1.16,1.16):
                self.cyl(name+'Wheel',(x,y,.51),.49,.24,'Metal',32,(math.pi/2,0,0),True)
                self.cyl(name+'Hub',(x,y*1.115,.51),.23,.03,'Cream',24,(math.pi/2,0,0))
        for y in (-.77,.77):self.cyl(name+'Headlamp',(2.79,y,1.23),.15,.04,'Cream',20,(0,math.pi/2,0))
        for z in (.91,1.02,1.13):self.box(name+'Grille',(2.79,0,z),(.035,1.13,.055),'Metal')
        self.box(name+'Bumper',(2.82,0,.72),(.18,2.3,.22),'DarkTeal')
        self.box(name+'Vent',(-1.8,-.1,3),(1,.7,.2),'White')
        for x in (-2.0,-1.8,-1.6):self.box(name+'VentSlots',(x,-.1,3.105),(.06,.55,.012),'Metal')
        self.text(name+'SideBrand','FESTA\nFIESTA',(-2.12,1.145,1.99),.14,'DarkTeal')
        return self
