"""인트로 프리비즈 공용 도우미 — bmesh 박스·원기둥·구·판, 빈 객체, 곡선, 카메라, 머티리얼.

v002부터 쓴다. v001(intro_previs_set.py)은 당시 결과를 재현하는 기록이라 그대로 둔다.
Blender 5.2 기준: 소켓은 identifier로 찾고(이름은 UI 언어에 따라 번역됨), F-Curve는 channelbag으로 얻는다.
"""
import math
import bpy
import bmesh
from mathutils import Vector
from bpy_extras import anim_utils


def sock(node, ident, outputs=False):
    return next(s for s in (node.outputs if outputs else node.inputs) if s.identifier == ident)


def principled(name, rgb, rough=0.6, metal=0.0, emit=0.0):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.diffuse_color = (*rgb, 1.0)
    bsdf = next(n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
    sock(bsdf, 'Base Color').default_value = (*rgb, 1.0)
    sock(bsdf, 'Roughness').default_value = rough
    sock(bsdf, 'Metallic').default_value = metal
    if emit > 0:
        sock(bsdf, 'Emission Color').default_value = (*rgb, 1.0)
        sock(bsdf, 'Emission Strength').default_value = emit
    return m


def image_material(name, image_path, fallback_rgb, rough=0.6):
    m = principled(name, fallback_rgb, rough)
    nt = m.node_tree
    bsdf = next(n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED')
    tex = next((n for n in nt.nodes if n.type == 'TEX_IMAGE'), None) or nt.nodes.new('ShaderNodeTexImage')
    tex.image = bpy.data.images.load(str(image_path), check_existing=True)
    tex.location = (bsdf.location.x - 350, bsdf.location.y)
    nt.links.new(sock(tex, 'Color', outputs=True), sock(bsdf, 'Base Color'))
    nt.nodes.active = tex     # Workbench 텍스처 색은 활성 이미지 노드를 쓴다
    return m


def coll(name, parent):
    c = bpy.data.collections.get(name) or bpy.data.collections.new(name)
    if c.name not in parent.children.keys():
        parent.children.link(c)
    return c


def purge(c):
    for child in list(c.children):
        purge(child)
        bpy.data.collections.remove(child)
    for ob in list(c.objects):
        bpy.data.objects.remove(ob, do_unlink=True)


def link(ob, c, mat=None, parent=None, loc=(0, 0, 0), rot=(0, 0, 0)):
    c.objects.link(ob)
    if mat is not None and ob.data is not None:
        ob.data.materials.append(mat)
    ob.parent = parent
    ob.location = loc
    ob.rotation_euler = rot
    return ob


def _mesh(name, bm):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    return me


def smooth(me, angle=40.0):
    me.shade_smooth()
    try:
        me.set_sharp_from_angle(angle=math.radians(angle))
    except AttributeError:
        pass


def box(name, lo, hi, c, mat, parent=None, pivot=None, bevel=0.0):
    """lo/hi는 부모 기준 최소·최대 좌표. pivot을 주면 원점을 그 점에 둔다(문·덮개 회전축)."""
    lo, hi = Vector(lo), Vector(hi)
    center = (lo + hi) / 2
    origin = Vector(pivot) if pivot is not None else center
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=hi - lo, verts=bm.verts)
    bmesh.ops.translate(bm, vec=center - origin, verts=bm.verts)
    ob = link(bpy.data.objects.new(name, _mesh(name, bm)), c, mat, parent, origin)
    if bevel > 0:
        mod = ob.modifiers.new('Bevel', 'BEVEL')
        mod.width = bevel
        mod.segments = 3
    return ob


def cylinder(name, radius, z0, z1, c, mat, parent=None, xy=(0.0, 0.0), segments=24, radius2=None):
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=segments, radius1=radius,
                          radius2=radius if radius2 is None else radius2, depth=z1 - z0)
    bmesh.ops.translate(bm, vec=(0, 0, (z0 + z1) / 2), verts=bm.verts)
    me = _mesh(name, bm)
    smooth(me)
    return link(bpy.data.objects.new(name, me), c, mat, parent, (xy[0], xy[1], 0))


def sphere(name, radius, loc, c, mat, parent=None, u=32, v=16, uvs=False):
    bm = bmesh.new()
    if uvs:
        bm.loops.layers.uv.new('UVMap')   # 레이어가 없으면 calc_uvs가 아무것도 쓰지 않는다
    bmesh.ops.create_uvsphere(bm, u_segments=u, v_segments=v, radius=radius, calc_uvs=uvs)
    me = _mesh(name, bm)
    me.shade_smooth()
    return link(bpy.data.objects.new(name, me), c, mat, parent, loc)


def plane_facing_neg_y(name, x0, x1, z0, z1, y, c, mat):
    """-Y를 바라보는 판(북쪽 벽에 붙이는 지도·사진). UV는 왼쪽 아래 (0,0) → 오른쪽 위 (1,1)."""
    bm = bmesh.new()
    uv = bm.loops.layers.uv.new('UVMap')
    vs = [bm.verts.new(p) for p in ((x0, y, z0), (x1, y, z0), (x1, y, z1), (x0, y, z1))]
    face = bm.faces.new(vs)
    for loop, co in zip(face.loops, ((0, 0), (1, 0), (1, 1), (0, 1))):
        loop[uv].uv = co
    return link(bpy.data.objects.new(name, _mesh(name, bm)), c, mat)


def empty(name, c, parent=None, loc=(0, 0, 0), rot=(0, 0, 0), size=0.1, kind='PLAIN_AXES'):
    ob = bpy.data.objects.new(name, None)
    ob.empty_display_type = kind
    ob.empty_display_size = size
    return link(ob, c, None, parent, loc, rot)


def poly_curve(name, points, c, mat, width, cyclic=False, parent=None):
    cu = bpy.data.curves.new(name, 'CURVE')
    cu.dimensions = '3D'
    cu.bevel_depth = width
    cu.bevel_resolution = 2
    sp = cu.splines.new('POLY')
    sp.points.add(len(points) - 1)
    for p, co in zip(sp.points, points):
        p.co = (*co, 1.0)
    sp.use_cyclic_u = cyclic
    return link(bpy.data.objects.new(name, cu), c, mat, parent)


def circle(r, n=64, plane='XY', z=0.0):
    pts = []
    for i in range(n):
        a = 2 * math.pi * i / n
        pts.append((r * math.cos(a), r * math.sin(a), z) if plane == 'XY' else (r * math.cos(a), 0.0, r * math.sin(a)))
    return pts


def label(name, text, loc, c, mat, size=0.16):
    cu = bpy.data.curves.new(name, 'FONT')
    cu.body = text
    cu.size = size
    cu.align_x = 'CENTER'
    cu.align_y = 'CENTER'
    return link(bpy.data.objects.new(name, cu), c, mat, None, loc)


def look_at(ob, target):
    ob.rotation_euler = (Vector(target) - ob.location).to_track_quat('-Z', 'Y').to_euler()


def camera(name, loc, target, lens, c, focus=None, fstop=4.0, ortho=None):
    cd = bpy.data.cameras.new(name)
    cd.lens = lens
    cd.clip_start = 0.03
    cd.clip_end = 80.0
    if ortho is not None:
        cd.type = 'ORTHO'
        cd.ortho_scale = ortho
    if focus is not None:
        cd.dof.use_dof = True
        cd.dof.focus_object = focus
        cd.dof.aperture_fstop = fstop
    ob = link(bpy.data.objects.new(name, cd), c, None, None, loc)
    if target is not None:
        look_at(ob, target)
    return ob


def light(name, kind, loc, c, rgb, energy, target=None, radius=None, size=None, angle_deg=None):
    ld = bpy.data.lights.new(name, kind)
    ld.color = rgb
    ld.energy = energy
    if radius is not None:
        ld.shadow_soft_size = radius
    if size is not None:
        ld.size = size
    if angle_deg is not None:
        ld.angle = math.radians(angle_deg)
    ob = link(bpy.data.objects.new(name, ld), c, None, None, loc)
    if target is not None:
        look_at(ob, target)
    return ob


def fcurves(ob):
    ad = ob.animation_data
    return anim_utils.action_get_channelbag_for_slot(ad.action, ad.action_slot).fcurves
