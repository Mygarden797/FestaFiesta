"""인트로 「집에서 출발」 프리비즈 — 침실 회색 세트와 5초 지구본 접근 파일럿.

켜진 Blender에 MCP로 모듈을 불러 build() → pilot() → save() 순으로 실행한다.
렌더는 UI를 막지 않도록 render_previs.py를 별도 Blender 프로세스(-b)로 돌린다.

치수·배치는 전부 프리비즈용 `assumption`(미터, Z 위)이며 외형은 회색 박스다.
기존 BrightGlobe .blend는 건드리지 않는다. 계획: 로컬 docs/ui/intro-astra-blender-higgsfield-plan.md
"""
from pathlib import Path
import math
import os
import bpy
import bmesh
from mathutils import Vector
from bpy_extras import anim_utils

ROOT = Path(__file__).resolve().parents[4]
ART = ROOT / 'SourceArt/UI/Intro/HouseDeparture_v1'
BLEND = ART / 'Blender/FF_Intro_HouseDeparture_v001.blend'
EARTH_TEX = ROOT / 'SourceArt/ThirdParty/SolarSystemScope/PlanetTextures/Earth/2k_earth_daymap.jpg'

SCENE = 'FF_Intro_HouseDeparture_v001'
TOP = 'FF_Intro_Previs'
FPS = 30
PILOT_FRAMES = (1, 150)          # 5초
GLOBE_SPIN_DEG_PER_SEC = 60.0    # 위에서 볼 때 반시계(실제 지구 자전과 같은 서→동)

# 방: 서쪽 벽에 문, 동쪽 벽에 창(침대 위), 북쪽 벽에 책상·지구본. 남쪽은 카메라 쪽이라 벽을 두지 않는다.
ROOM = {'x': (-2.0, 2.0), 'y': (-2.5, 2.5), 'h': 2.6, 'wall': 0.1}
DOOR = {'y': (-1.35, -0.45), 'h': 2.1}
WINDOW = {'y': (-1.0, 0.2), 'z': (0.9, 2.1)}
GLOBE = {'base': (-0.4, 2.2, 0.75), 'radius': 0.15, 'ring': 0.165, 'center_up': 0.29, 'tilt_deg': 23.5}
BACKPACK = {'base': (1.3, -0.6, 0.53), 'open_deg': 110.0}
PACK_SPOT = (0.72, -0.6)         # 캐릭터가 배낭을 마주 보는(+X) 자리
S01_CAM, S02_CAM, S03_CAM = (1.75, -2.35, 1.65), (0.45, 1.75, 1.3), (0.35, -2.4, 1.45)

# 샷별 대역 자세. yaw는 CHR_Root Z 회전(90=+X, 180=+Y), 팔은 (어깨, 팔꿈치) X 회전(음수=앞으로).
# S02 오른손은 지구본 앞면 중앙에 닿고 서→동(화면 왼→오)으로 밀어 돌린다.
POSES = {
    # 문은 -90°보다 더 열어야 S01 카메라(남동쪽)에서 문짝이 들어오는 캐릭터를 가리지 않는다.
    'S01': {'root': (-1.75, -0.9), 'yaw': 156, 'L': (0, -5), 'R': (-10, -10), 'door': -105},
    'S02': {'root': (-0.65, 1.5), 'yaw': 180, 'L': (-5, 0), 'R': (-50, -20), 'door': 0},
    'S03': {'root': PACK_SPOT, 'yaw': 90, 'L': (-40, -30), 'R': (-40, -30), 'door': 0},
}

COLORS = {
    'floor': (0.40, 0.33, 0.27), 'wall': (0.80, 0.79, 0.76), 'trim': (0.62, 0.60, 0.57),
    'door': (0.58, 0.46, 0.34), 'outside': (1.00, 0.93, 0.78), 'sky': (0.62, 0.80, 0.96),
    'wood': (0.50, 0.39, 0.29), 'wood_dark': (0.33, 0.27, 0.22), 'mattress': (0.88, 0.88, 0.90),
    'blanket': (0.36, 0.48, 0.64), 'pillow': (0.95, 0.95, 0.95), 'rug': (0.68, 0.58, 0.46),
    'lamp': (0.95, 0.88, 0.70), 'book_a': (0.70, 0.25, 0.22), 'book_b': (0.24, 0.45, 0.35),
    'book_c': (0.85, 0.70, 0.30), 'frame': (0.20, 0.20, 0.22),
    'globe_stand': (0.22, 0.19, 0.17), 'globe_ring': (0.72, 0.56, 0.26),
    'bag': (0.86, 0.42, 0.12), 'bag_dark': (0.50, 0.24, 0.08),
    'item_cloth': (0.55, 0.72, 0.86), 'item_dark': (0.12, 0.12, 0.13), 'item_red': (0.55, 0.10, 0.12),
    'item_bottle': (0.30, 0.65, 0.70), 'item_book': (0.90, 0.80, 0.55),
    'chr': (0.62, 0.66, 0.72), 'chr_mark': (0.10, 0.10, 0.12),
    'guide_in': (0.10, 0.85, 0.95), 'guide_out': (0.95, 0.20, 0.75), 'guide_cam': (0.98, 0.80, 0.10),
    'label': (0.08, 0.08, 0.08),
}


# ---------------------------------------------------------------- 공통 도우미

def _mat(key):
    name = 'M_Previs_' + key
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    rgb = COLORS[key]
    m.diffuse_color = (*rgb, 1.0)
    bsdf = next((n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None)
    if bsdf is not None:
        next(i for i in bsdf.inputs if i.identifier == 'Base Color').default_value = (*rgb, 1.0)
        next(i for i in bsdf.inputs if i.identifier == 'Roughness').default_value = 0.7
    return m


def _coll(name, parent):
    c = bpy.data.collections.get(name) or bpy.data.collections.new(name)
    if c.name not in parent.children.keys():
        parent.children.link(c)
    return c


def _link(ob, coll, mat=None, parent=None, loc=(0, 0, 0), rot=(0, 0, 0)):
    coll.objects.link(ob)
    if mat is not None and ob.data is not None:
        ob.data.materials.append(mat)
    ob.parent = parent
    ob.location = loc
    ob.rotation_euler = rot
    return ob


def _smooth(me, angle=40.0):
    me.shade_smooth()
    try:
        me.set_sharp_from_angle(angle=math.radians(angle))
    except AttributeError:
        pass


def box(name, lo, hi, coll, key, parent=None, pivot=None, bevel=0.0):
    """lo/hi는 부모 기준 최소·최대 좌표. pivot을 주면 원점을 그 점에 둔다(문·덮개 회전축)."""
    lo, hi = Vector(lo), Vector(hi)
    center = (lo + hi) / 2
    origin = Vector(pivot) if pivot is not None else center
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=hi - lo, verts=bm.verts)
    bmesh.ops.translate(bm, vec=center - origin, verts=bm.verts)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = _link(bpy.data.objects.new(name, me), coll, _mat(key), parent, origin)
    if bevel > 0:
        mod = ob.modifiers.new('Bevel', 'BEVEL')
        mod.width = bevel
        mod.segments = 3
    return ob


def cylinder(name, radius, z0, z1, coll, key, parent=None, xy=(0.0, 0.0), segments=24):
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=segments,
                          radius1=radius, radius2=radius, depth=z1 - z0)
    bmesh.ops.translate(bm, vec=(0, 0, (z0 + z1) / 2), verts=bm.verts)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    _smooth(me)
    return _link(bpy.data.objects.new(name, me), coll, _mat(key), parent, (xy[0], xy[1], 0))


def sphere(name, radius, loc, coll, key=None, parent=None, u=32, v=16, uvs=False, mat=None):
    bm = bmesh.new()
    if uvs:
        bm.loops.layers.uv.new('UVMap')   # 레이어가 없으면 calc_uvs가 아무것도 쓰지 않는다
    bmesh.ops.create_uvsphere(bm, u_segments=u, v_segments=v, radius=radius, calc_uvs=uvs)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    me.shade_smooth()
    return _link(bpy.data.objects.new(name, me), coll, mat or _mat(key), parent, loc)


def empty(name, coll, parent=None, loc=(0, 0, 0), rot=(0, 0, 0), size=0.1, kind='PLAIN_AXES'):
    ob = bpy.data.objects.new(name, None)
    ob.empty_display_type = kind
    ob.empty_display_size = size
    return _link(ob, coll, None, parent, loc, rot)


def poly_curve(name, points, coll, key, width, cyclic=False, parent=None):
    cu = bpy.data.curves.new(name, 'CURVE')
    cu.dimensions = '3D'
    cu.bevel_depth = width
    cu.bevel_resolution = 2
    sp = cu.splines.new('POLY')
    sp.points.add(len(points) - 1)
    for p, co in zip(sp.points, points):
        p.co = (*co, 1.0)
    sp.use_cyclic_u = cyclic
    return _link(bpy.data.objects.new(name, cu), coll, _mat(key), parent)


def label(name, text, loc, coll, size=0.16):
    cu = bpy.data.curves.new(name, 'FONT')
    cu.body = text
    cu.size = size
    cu.align_x = 'CENTER'
    cu.align_y = 'CENTER'
    return _link(bpy.data.objects.new(name, cu), coll, _mat('label'), None, loc)


def look_at(ob, target):
    ob.rotation_euler = (Vector(target) - ob.location).to_track_quat('-Z', 'Y').to_euler()


def camera(name, loc, target, lens, coll, focus=None, fstop=4.0, ortho=None):
    cd = bpy.data.cameras.new(name)
    cd.lens = lens
    cd.clip_start = 0.03
    cd.clip_end = 60.0
    if ortho is not None:
        cd.type = 'ORTHO'
        cd.ortho_scale = ortho
    if focus is not None:
        cd.dof.use_dof = True
        cd.dof.focus_object = focus
        cd.dof.aperture_fstop = fstop
    ob = _link(bpy.data.objects.new(name, cd), coll, None, None, loc)
    if target is not None:
        look_at(ob, target)
    return ob


def _fcurves(ob):
    ad = ob.animation_data
    return anim_utils.action_get_channelbag_for_slot(ad.action, ad.action_slot).fcurves


def _purge(coll):
    for child in list(coll.children):
        _purge(child)
        bpy.data.collections.remove(child)
    for ob in list(coll.objects):
        bpy.data.objects.remove(ob, do_unlink=True)


# ---------------------------------------------------------------- 씬

def _scene():
    sc = bpy.data.scenes.get(SCENE) or bpy.data.scenes.new(SCENE)
    sc.render.fps = FPS
    sc.render.fps_base = 1.0
    sc.render.resolution_x, sc.render.resolution_y = 1920, 1080
    sc.render.resolution_percentage = 100
    sc.frame_start, sc.frame_end = PILOT_FRAMES
    sc.unit_settings.system = 'METRIC'
    sc.unit_settings.scale_length = 1.0
    try:
        sc.render.engine = 'BLENDER_WORKBENCH'
    except TypeError as e:
        print('Workbench 지정 실패:', e)
    sh = sc.display.shading
    sh.light = 'STUDIO'
    sh.color_type = 'TEXTURE'
    sh.show_shadows = True
    sh.show_cavity = True
    w = bpy.data.worlds.get('FF_Intro_World') or bpy.data.worlds.new('FF_Intro_World')
    bg = next((n for n in w.node_tree.nodes if n.type == 'BACKGROUND'), None)
    if bg is not None:
        bg.inputs[0].default_value = (0.62, 0.66, 0.72, 1.0)
        bg.inputs[1].default_value = 0.5
    sc.world = w
    for win in bpy.context.window_manager.windows:
        win.scene = sc
    return sc


def _drop_default_scene(keep):
    """저장 전 기본 씬(Cube·Light·Camera만 있는 'Scene')이면 지운다. 다른 내용이 있으면 남긴다."""
    old = bpy.data.scenes.get('Scene')
    if old is None or old == keep or bpy.data.filepath:
        return False
    if {o.name for o in old.objects} - {'Cube', 'Light', 'Camera'}:
        return False
    for o in list(old.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    bpy.data.scenes.remove(old)
    return True


# ---------------------------------------------------------------- 세트

def _room(c):
    x0, x1 = ROOM['x']
    y0, y1 = ROOM['y']
    h, t = ROOM['h'], ROOM['wall']
    box('SET_Floor', (x0 - t, y0, -0.05), (x1 + t, y1 + t, 0), c, 'floor')
    box('SET_Wall_N', (x0 - t, y1, 0), (x1 + t, y1 + t, h), c, 'wall')
    # 서쪽 벽 — 문 구멍
    dy0, dy1 = DOOR['y']
    box('SET_Wall_W_South', (x0 - t, y0, 0), (x0, dy0, h), c, 'wall')
    box('SET_Wall_W_North', (x0 - t, dy1, 0), (x0, y1 + t, h), c, 'wall')
    box('SET_Wall_W_OverDoor', (x0 - t, dy0, DOOR['h']), (x0, dy1, h), c, 'wall')
    # 동쪽 벽 — 창 구멍
    wy0, wy1 = WINDOW['y']
    wz0, wz1 = WINDOW['z']
    box('SET_Wall_E_South', (x1, y0, 0), (x1 + t, wy0, h), c, 'wall')
    box('SET_Wall_E_North', (x1, wy1, 0), (x1 + t, y1 + t, h), c, 'wall')
    box('SET_Wall_E_UnderWindow', (x1, wy0, 0), (x1 + t, wy1, wz0), c, 'wall')
    box('SET_Wall_E_OverWindow', (x1, wy0, wz1), (x1 + t, wy1, h), c, 'wall')
    box('SET_Window_Sill', (x1 - 0.1, wy0 - 0.05, wz0 - 0.04), (x1, wy1 + 0.05, wz0), c, 'trim')
    box('SET_Window_MullionV', (x1 + 0.02, (wy0 + wy1) / 2 - 0.02, wz0), (x1 + 0.06, (wy0 + wy1) / 2 + 0.02, wz1), c, 'trim')
    box('SET_Window_MullionH', (x1 + 0.02, wy0, (wz0 + wz1) / 2 - 0.02), (x1 + 0.06, wy1, (wz0 + wz1) / 2 + 0.02), c, 'trim')
    # 문 — 원점이 남쪽 경첩. 안쪽(+X)으로 열리며 열림은 Z -90°.
    door = box('SET_Door', (x0, dy0 + 0.01, 0), (x0 + 0.04, dy1 - 0.01, DOOR['h'] - 0.01), c, 'door',
               pivot=(x0 + 0.02, dy0 + 0.01, 0))
    door['previs_note'] = '원점=남쪽 경첩. rotation Z 0=닫힘, -90°=활짝 열림(방 안쪽)'
    sphere('SET_Door_Knob', 0.03, (0.05, 0.8, 1.0), c, 'trim', parent=door, u=16, v=8)
    # 문·창 너머 — 화면에 보일 때 빈 공간 대신 밝은 면만 둔다.
    box('SET_Outside_Hall_Floor', (x0 - 1.0, dy0 - 0.7, -0.05), (x0 - t, dy1 + 0.6, 0), c, 'floor')
    box('SET_Outside_Hall_Backdrop', (x0 - 1.05, dy0 - 0.7, 0), (x0 - 1.0, dy1 + 0.6, 2.5), c, 'outside')
    box('SET_Outside_Sky', (x1 + 0.9, wy0 - 1.2, 0), (x1 + 0.95, wy1 + 1.2, 3.2), c, 'sky')


def _furniture(c):
    # 침대 — 동쪽 벽에 붙이고 머리는 북쪽. 창이 바로 위에 온다.
    box('FUR_Bed_Frame', (1.0, -1.6, 0.0), (2.0, 0.4, 0.3), c, 'wood')
    box('FUR_Bed_Mattress', (1.03, -1.57, 0.3), (1.97, 0.37, 0.5), c, 'mattress', bevel=0.03)
    box('FUR_Bed_Blanket', (1.01, -1.59, 0.47), (1.99, -0.05, 0.53), c, 'blanket', bevel=0.02)
    box('FUR_Bed_Pillow', (1.15, 0.02, 0.5), (1.85, 0.33, 0.62), c, 'pillow', bevel=0.04)
    box('FUR_Bed_Headboard', (1.0, 0.4, 0.0), (2.0, 0.46, 0.95), c, 'wood_dark')
    # 협탁·스탠드
    box('FUR_Nightstand', (1.55, 0.52, 0.0), (2.0, 0.97, 0.55), c, 'wood')
    cylinder('FUR_Lamp_Stem', 0.015, 0.55, 0.72, c, 'trim', xy=(1.78, 0.75))
    cylinder('FUR_Lamp_Shade', 0.1, 0.72, 0.88, c, 'lamp', xy=(1.78, 0.75))
    # 책상 — 북쪽 벽, 지구본은 오른쪽 끝(x -0.5)
    box('FUR_Desk_Top', (-1.4, 1.9, 0.72), (-0.2, 2.5, 0.75), c, 'wood')
    box('FUR_Desk_Side_W', (-1.4, 1.92, 0.0), (-1.36, 2.48, 0.72), c, 'wood_dark')
    box('FUR_Desk_Side_E', (-0.24, 1.92, 0.0), (-0.2, 2.48, 0.72), c, 'wood_dark')
    # 의자 — S02 캐릭터 자리와 지구본 접근 경로를 막지 않게 책상 왼쪽 끝에 넣어 둔다.
    box('FUR_Chair_Seat', (-1.34, 1.45, 0.43), (-0.96, 1.9, 0.47), c, 'wood_dark')
    box('FUR_Chair_Back', (-1.34, 1.4, 0.47), (-0.96, 1.44, 0.9), c, 'wood_dark')
    for i, (lx, ly) in enumerate([(-1.33, 1.41), (-1.0, 1.41), (-1.33, 1.86), (-1.0, 1.86)]):
        box(f'FUR_Chair_Leg_{i}', (lx, ly, 0.0), (lx + 0.04, ly + 0.04, 0.43), c, 'wood_dark')
    # 선반·책·액자 — 지구본 뒤 배경에 카메라 접근 시 시차(parallax)를 준다.
    box('FUR_Shelf', (-1.35, 2.3, 1.48), (-0.25, 2.5, 1.51), c, 'wood')
    box('FUR_Book_A', (-1.3, 2.33, 1.51), (-1.24, 2.49, 1.75), c, 'book_a')
    box('FUR_Book_B', (-1.23, 2.33, 1.51), (-1.16, 2.49, 1.72), c, 'book_b')
    box('FUR_Book_C', (-1.15, 2.33, 1.51), (-1.1, 2.49, 1.78), c, 'book_c')
    box('FUR_Picture_Frame', (0.3, 2.47, 1.25), (0.95, 2.5, 1.75), c, 'frame')
    box('FUR_Picture', (0.34, 2.465, 1.29), (0.91, 2.47, 1.71), c, 'sky')
    # 옷장 — 서쪽 벽, 문과 책상 사이
    box('FUR_Wardrobe', (-2.0, 0.2, 0.0), (-1.45, 1.4, 2.0), c, 'wood')
    box('FUR_Wardrobe_Seam', (-1.452, 0.795, 0.05), (-1.445, 0.805, 1.95), c, 'wood_dark')
    # 러그
    box('FUR_Rug', (-0.9, -1.4, 0.0), (0.9, 0.8, 0.012), c, 'rug')


def _globe(c):
    """책상 지구본. Root(받침) → Axis(23.5° 기울기, 자오환) → Earth(국소 Z 회전)."""
    g = GLOBE
    root = empty('GLB_Root', c, loc=g['base'], size=0.08)
    cylinder('GLB_Base', 0.085, 0.0, 0.025, c, 'globe_stand', parent=root, segments=32)
    cylinder('GLB_Post', 0.012, 0.025, g['center_up'] - g['ring'], c, 'globe_stand', parent=root)
    axis = empty('GLB_Axis', c, parent=root, loc=(0, 0, g['center_up']),
                 rot=(0, math.radians(g['tilt_deg']), 0), size=0.2, kind='SINGLE_ARROW')
    n = 64
    ring = [(g['ring'] * math.cos(2 * math.pi * i / n), 0.0, g['ring'] * math.sin(2 * math.pi * i / n))
            for i in range(n)]
    poly_curve('GLB_Meridian', ring, c, 'globe_ring', 0.006, cyclic=True, parent=axis)
    cylinder('GLB_Pin', 0.004, -g['ring'], g['ring'], c, 'globe_ring', parent=axis, segments=12)

    earth_mat = bpy.data.materials.get('M_Previs_Earth_Basic') or bpy.data.materials.new('M_Previs_Earth_Basic')
    nt = earth_mat.node_tree
    bsdf = next(n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED')
    tex = next((n for n in nt.nodes if n.type == 'TEX_IMAGE'), None) or nt.nodes.new('ShaderNodeTexImage')
    tex.image = bpy.data.images.load(str(EARTH_TEX), check_existing=True)
    tex.location = (bsdf.location.x - 350, bsdf.location.y)
    base_in = next(i for i in bsdf.inputs if i.identifier == 'Base Color')
    nt.links.new(next(o for o in tex.outputs if o.identifier == 'Color'), base_in)
    nt.nodes.active = tex
    earth_mat.diffuse_color = (0.25, 0.45, 0.75, 1.0)
    earth = sphere('GLB_Earth', g['radius'], (0, 0, 0), c, parent=axis, u=64, v=32, uvs=True, mat=earth_mat)
    earth['previs_note'] = '기본 지구 텍스처(Solar System Scope 2K daymap, CC BY 4.0). 국소 Z 양수 회전=서→동'
    return earth


def _backpack(c):
    """배낭 — 앞면이 캐릭터 쪽(-X). 덮개 원점은 등판 쪽 윗모서리, 열림 Y +110° / 닫힘 0°."""
    root = empty('BAG_Root', c, loc=BACKPACK['base'], size=0.1)
    box('BAG_Body', (-0.1, -0.16, 0.0), (0.1, 0.16, 0.44), c, 'bag', parent=root, bevel=0.03)
    box('BAG_FrontPocket', (-0.14, -0.12, 0.06), (-0.09, 0.12, 0.25), c, 'bag_dark', parent=root, bevel=0.015)
    for side, sy in (('L', 0.07), ('R', -0.11)):
        box(f'BAG_Strap_{side}', (0.1, sy, 0.08), (0.125, sy + 0.04, 0.4), c, 'bag_dark', parent=root, bevel=0.008)
    pivot = empty('BAG_FlapPivot', c, parent=root, loc=(0.1, 0.0, 0.44),
                  rot=(0, math.radians(BACKPACK['open_deg']), 0), size=0.08, kind='SINGLE_ARROW')
    box('BAG_Flap', (-0.215, -0.165, 0.0), (0.005, 0.165, 0.04), c, 'bag_dark', parent=pivot, bevel=0.012)
    pivot['previs_note'] = 'rotation Y 110°=열림, 0°=닫힘. S04 닫힘 소리 타이밍 기준'
    # 넣을 짐 — 침대 발치 쪽(카메라 가까이)에 펼쳐 둔다.
    z = BACKPACK['base'][2]
    box('ITEM_Clothes', (1.18, -1.2, z), (1.46, -0.96, z + 0.1), c, 'item_cloth', bevel=0.02)
    box('ITEM_Camera', (1.55, -1.18, z), (1.67, -1.1, z + 0.08), c, 'item_dark', bevel=0.01)
    box('ITEM_Passport', (1.55, -1.02, z), (1.64, -0.9, z + 0.012), c, 'item_red')
    box('ITEM_Guidebook', (1.7, -1.22, z), (1.86, -1.0, z + 0.03), c, 'item_book')
    cylinder('ITEM_Bottle', 0.035, z, z + 0.22, c, 'item_bottle', xy=(1.34, -0.86))


def _standin(c):
    """키 1.8m 마네킹. 국소 기준으로 -Y를 바라보게 만들고 Root를 돌려 배치한다(+90° → +X 향함)."""
    root = empty('CHR_Root', c, size=0.3, kind='SINGLE_ARROW')
    root['previs_note'] = 'Tripo 리깅 모델로 교체할 대역. 국소 -Y가 정면, 캐릭터 왼쪽=+X. 자세는 pose(샷)'
    for side, sx in (('L', 0.1), ('R', -0.1)):
        cylinder(f'CHR_Leg_{side}', 0.07, 0.06, 0.88, c, 'chr', parent=root, xy=(sx, 0.0))
        box(f'CHR_Foot_{side}', (sx - 0.06, -0.22, 0.0), (sx + 0.06, 0.06, 0.07), c, 'chr', parent=root, bevel=0.02)
    box('CHR_Pelvis', (-0.17, -0.1, 0.84), (0.17, 0.1, 1.0), c, 'chr', parent=root, bevel=0.04)
    box('CHR_Torso', (-0.2, -0.11, 1.0), (0.2, 0.11, 1.44), c, 'chr', parent=root, bevel=0.05)
    cylinder('CHR_Neck', 0.05, 1.44, 1.56, c, 'chr', parent=root)
    sphere('CHR_Head', 0.12, (0, 0, 1.68), c, 'chr', parent=root)
    box('CHR_FaceMark', (-0.025, -0.15, 1.64), (0.025, -0.1, 1.7), c, 'chr_mark', parent=root)
    # 팔 — 어깨·팔꿈치 빈 객체를 X축으로 돌려 블로킹한다(음수=앞으로 듦).
    for side, sx in (('L', 0.25), ('R', -0.25)):
        sh = empty(f'CHR_Shoulder_{side}', c, parent=root, loc=(sx, 0.0, 1.4), size=0.06)
        cylinder(f'CHR_UpperArm_{side}', 0.045, -0.29, 0.0, c, 'chr', parent=sh)
        el = empty(f'CHR_Elbow_{side}', c, parent=sh, loc=(0, 0, -0.29), size=0.05)
        cylinder(f'CHR_Forearm_{side}', 0.04, -0.26, 0.0, c, 'chr', parent=el)
        sphere(f'CHR_Hand_{side}', 0.05, (0, 0, -0.3), c, 'chr', parent=el, u=16, v=8)


def pose(shot):
    """대역과 문을 샷 시작 자세로 놓는다. 키프레임은 넣지 않는다(배치 확인용)."""
    p = POSES[shot]
    root = bpy.data.objects['CHR_Root']
    root.location = (*p['root'], 0.0)
    root.rotation_euler = (0, 0, math.radians(p['yaw']))
    for side in ('L', 'R'):
        s, e = p[side]
        bpy.data.objects[f'CHR_Shoulder_{side}'].rotation_euler = (math.radians(s), 0, 0)
        bpy.data.objects[f'CHR_Elbow_{side}'].rotation_euler = (math.radians(e), 0, 0)
    bpy.data.objects['SET_Door'].rotation_euler = (0, 0, math.radians(p['door']))


def _pilot_path():
    center = Vector(GLOBE['base']) + Vector((0, 0, GLOBE['center_up']))
    d = Vector((0.25, -1.0, 0.18)).normalized()   # 지구본에서 카메라 쪽(남남동, 약간 위)
    return center, center + d * 2.4, center + d * 0.8


def _guides(paths, labels):
    """위에서 본 배치도용 동선(paths)과 이름표·카메라 위치(labels). 샷 렌더에서는 숨긴다."""
    z = 0.03
    s01, s02, s03 = (POSES[k]['root'] for k in ('S01', 'S02', 'S03'))
    poly_curve('GUIDE_Path_In', [(-2.7, -0.85, z), (-1.6, -0.85, z), (-1.05, 0.3, z), (*s02, z),
                                 (0.1, 0.3, z), (*s03, z)], paths, 'guide_in', 0.02)
    poly_curve('GUIDE_Path_Out', [(s03[0], s03[1] - 0.1, z), (-0.6, -1.0, z), (-1.6, -1.0, z), (-2.7, -1.0, z)],
               paths, 'guide_out', 0.02)
    zl = 2.7
    _, start, end = _pilot_path()
    poly_curve('GUIDE_Cam_Pilot', [(start.x, start.y, zl), (end.x, end.y, zl)], labels, 'guide_cam', 0.025)
    label('TXT_Door', 'DOOR  in S01 / out S04 (sound)', (-1.2, -1.75, zl), labels)
    label('TXT_Globe', 'GLOBE', (GLOBE['base'][0], 2.0, zl), labels, size=0.12)
    label('TXT_Pack', 'PACK SPOT (S03)', (0.3, -1.35, zl), labels)
    label('TXT_S02Spot', 'S02 spot', (s02[0] - 0.1, s02[1] - 0.3, zl), labels, size=0.12)
    label('TXT_Bed', 'BED + BACKPACK', (1.5, -1.8, zl), labels)
    label('TXT_Window', 'WINDOW', (2.45, -0.4, zl), labels, size=0.14)
    label('TXT_Wardrobe', 'WARDROBE', (-1.72, 0.8, zl), labels, size=0.12)
    label('TXT_Desk', 'DESK', (-1.05, 2.2, zl), labels, size=0.12)
    label('TXT_South', 'camera side (no wall)', (-0.9, -2.3, zl), labels, size=0.14)
    for name, (x, y), dx in (('S01 cam', S01_CAM[:2], -0.45), ('S02 cam', S02_CAM[:2], 0.35),
                             ('S03 cam', S03_CAM[:2], 0.35), ('P0 start', start.xy, 0.35), ('P0 end', end.xy, 0.35)):
        label('TXT_Cam_' + name.replace(' ', '_'), '[' + name + ']', (x + dx, y, zl), labels, size=0.1)


def _cameras(c, earth, standin_head):
    camera('CAM_S01_Enter', S01_CAM, (-1.1, 0.3, 1.0), 18, c)
    # S02는 캐릭터 오른쪽 옆에서 손·지구본을 함께 담는다. 등 뒤에서 찍으면 팔이 화면을 덮는다.
    camera('CAM_S02_Spin', S02_CAM, (-0.45, 2.1, 1.08), 30, c, focus=earth, fstop=2.8)
    camera('CAM_S03_Pack', S03_CAM, (0.7, 0.3, 1.05), 24, c, focus=standin_head, fstop=2.8)
    camera('CAM_Layout_Top', (0.0, 0.05, 9.0), None, 50, c, ortho=6.8).rotation_euler = (0, 0, 0)
    # 남쪽(벽 없음) 높은 곳에서 내려다보는 조감. 동쪽에서 보면 동쪽 벽 바깥면만 보인다.
    camera('CAM_Layout_Overview', (0.4, -5.0, 4.8), (0.0, 0.5, 0.4), 30, c)


def _lights(c):
    sun = bpy.data.lights.new('LGT_Sun_Window', 'SUN')
    sun.energy = 3.0
    ob = _link(bpy.data.objects.new('LGT_Sun_Window', sun), c, None, None, (3.5, -0.4, 3.5))
    look_at(ob, (0.8, -0.2, 0.0))
    area = bpy.data.lights.new('LGT_Area_Window', 'AREA')
    area.energy = 150.0
    area.size = 1.2
    ob = _link(bpy.data.objects.new('LGT_Area_Window', area), c, None, None, (1.9, -0.4, 1.5))
    look_at(ob, (-1.0, -0.4, 1.2))


def build(force=False):
    sc = _scene()
    dropped = _drop_default_scene(sc)
    top = bpy.data.collections.get(TOP)
    if top is not None and len(top.all_objects) and not force:
        print('이미 세트가 있어 새로 만들지 않음. 다시 만들려면 build(force=True)')
        return sc
    top = _coll(TOP, sc.collection)
    _purge(top)
    parts = {k: _coll(k, top) for k in
             ('SET_Room', 'SET_Furniture', 'PROP_Globe', 'PROP_Backpack', 'CHR_Standin', 'CAM_Shots',
              'LGT_Room', 'GUIDE_Layout', 'GUIDE_Labels')}
    _room(parts['SET_Room'])
    _furniture(parts['SET_Furniture'])
    earth = _globe(parts['PROP_Globe'])
    _backpack(parts['PROP_Backpack'])
    _standin(parts['CHR_Standin'])
    pose('S03')
    _guides(parts['GUIDE_Layout'], parts['GUIDE_Labels'])
    _cameras(parts['CAM_Shots'], earth, bpy.data.objects['CHR_Head'])
    _lights(parts['LGT_Room'])
    sc.camera = bpy.data.objects['CAM_Layout_Overview']
    sc['ff_intro_previs'] = 'v001 침실 회색 세트(assumption 치수)'
    print(f'세트 생성: 객체 {len(top.all_objects)}개, 기본 씬 제거={dropped}')
    return sc


# ---------------------------------------------------------------- 5초 파일럿

def pilot(force=False):
    """책상 지구본이 도는 동안 카메라가 5초간 직선으로 다가간다(S04 접근 구간 시험)."""
    sc = bpy.data.scenes[SCENE]
    if bpy.data.objects.get('CAM_P0_Pilot') is not None and not force:
        print('파일럿 카메라가 이미 있음. 다시 만들려면 pilot(force=True)')
        return
    old = bpy.data.objects.get('CAM_P0_Pilot')
    if old is not None:
        bpy.data.objects.remove(old, do_unlink=True)
    earth = bpy.data.objects['GLB_Earth']
    center, start, end = _pilot_path()
    f0, f1 = PILOT_FRAMES
    cam = camera('CAM_P0_Pilot', start, center, 35, bpy.data.collections['CAM_Shots'], focus=earth, fstop=2.8)
    cam.keyframe_insert('location', frame=f0)
    cam.location = end
    cam.keyframe_insert('location', frame=f1)
    cam.location = start
    # 지구 자전 — 선형·외삽. 프레임당 각도는 초당 각도/FPS.
    earth.rotation_euler.z = 0.0
    earth.keyframe_insert('rotation_euler', index=2, frame=f0)
    earth.rotation_euler.z = math.radians(GLOBE_SPIN_DEG_PER_SEC * (f1 - f0 + 1) / FPS)
    earth.keyframe_insert('rotation_euler', index=2, frame=f1 + 1)
    earth.rotation_euler.z = 0.0
    for fc in _fcurves(earth):
        for kp in fc.keyframe_points:
            kp.interpolation = 'LINEAR'
        fc.extrapolation = 'LINEAR'
    # 마커에 카메라를 묶지 않는다 — 묶으면 다른 카메라로 찍는 배치 렌더가 파일럿 카메라로 바뀐다.
    sc.timeline_markers.clear()
    sc.timeline_markers.new('P0_start', frame=f0)
    sc.timeline_markers.new('P0_end', frame=f1)
    sc.camera = cam
    sc.frame_set(f0)
    print(f'파일럿: {start[:]} → {end[:]}, 거리 2.4m → 0.8m, 지구 {GLOBE_SPIN_DEG_PER_SEC}°/s')


# ---------------------------------------------------------------- 저장·보기

def save():
    target = str(BLEND)
    if os.path.exists(target) and os.path.normcase(bpy.data.filepath) != os.path.normcase(target):
        raise RuntimeError(f'다른 파일이 이미 있음: {target}')
    os.makedirs(BLEND.parent, exist_ok=True)
    for sub in ('Previs/Layout', 'Previs/Pilot'):
        os.makedirs(ART / sub, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=target, relative_remap=True)
    print('저장:', target)


def show(cam_name=None, guides=True):
    """켜진 Blender 뷰포트를 카메라 시점·텍스처 색으로 맞춘다(MCP 스크린샷 확인용)."""
    sc = bpy.data.scenes[SCENE]
    if cam_name:
        sc.camera = bpy.data.objects[cam_name]
    for name in ('GUIDE_Layout', 'GUIDE_Labels'):
        sc.view_layers[0].layer_collection.children[TOP].children[name].hide_viewport = not guides
    for win in bpy.context.window_manager.windows:
        for area in win.screen.areas:
            if area.type != 'VIEW_3D':
                continue
            sp = area.spaces.active
            sp.shading.type = 'SOLID'
            sp.shading.color_type = 'TEXTURE'
            sp.shading.show_shadows = True
            sp.overlay.show_overlays = False
            if cam_name:
                sp.region_3d.view_perspective = 'CAMERA'
                region = next(r for r in area.regions if r.type == 'WINDOW')
                with bpy.context.temp_override(window=win, area=area, region=region):
                    bpy.ops.view3d.view_center_camera()
