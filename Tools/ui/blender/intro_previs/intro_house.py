"""인트로 「집에서 출발」 프리비즈 — 여행가의 집(1층 거실 + 1층 침실), 밤 무드.

동선(사용자 지시 2026-09-29): 현관문을 열고 → 거실을 가로질러(거실을 살짝 보여 줌) → 열린 침실 문으로 →
침실에서 지구본을 돌리고 → 침대에서 짐을 싸서 → 나간다(S04는 소리로만).
v003: 거실 북쪽 벽(계단 옆)에 침실 문. v004: 침실 5.5×5m.
v005: 계단 입구를 막던 소파·스탠드 → 거실을 남쪽으로 2m 넓혀(7×11m) 라운지를 남쪽 창가로 옮김.
      침실 7×7m, 지구본은 **북쪽 벽 책상 위 책상형**(지름 40cm)으로 — S03에서 캐릭터보다 훨씬 뒤에 온다.

참고 무드: Fab 「DetectiveOffice」의 느낌만 참고했다(에셋·이미지 미사용 — 리스팅이 NoAI).
MCP에서 build() → pilot() → save(). 렌더는 render_house.py를 별도 Blender 프로세스(-b)로 돌린다.
치수·배치·조명은 전부 프리비즈용 `assumption`(미터, Z 위). 이전 버전 .blend는 기록으로 둔다.
"""
from pathlib import Path
import math
import os
import random
import bpy
from mathutils import Vector

import previs_kit as k

VERSION = 'v005'
ROOT = Path(__file__).resolve().parents[4]
ART = ROOT / 'SourceArt/UI/Intro/HouseDeparture_v1'
BLEND = ART / f'Blender/FF_Intro_HouseDeparture_{VERSION}.blend'
EARTH_TEX = ROOT / 'SourceArt/ThirdParty/SolarSystemScope/PlanetTextures/Earth/2k_earth_daymap.jpg'

SCENE = f'FF_Intro_HouseDeparture_{VERSION}'
TOP = f'FF_Intro_House_{VERSION}'
OLD_SCENES = tuple(f'FF_Intro_HouseDeparture_v00{i}' for i in range(1, 5))
OLD_TOPS = ('FF_Intro_Previs', 'FF_Intro_House', 'FF_Intro_House_v003', 'FF_Intro_House_v004')
FPS = 30
PILOT_FRAMES = (1, 150)
GLOBE_SPIN_DEG_PER_SEC = 60.0    # 위에서 볼 때 반시계 = 서→동

# 거실 7×11m, 층고 3.2m. 서쪽 벽 현관문, 남쪽 벽 블라인드 창(라운지 뒤), 동쪽 벽을 따라 2층 계단.
HALL = {'x': (-3.5, 3.5), 'y': (-6.5, 4.5), 'h': 3.2, 't': 0.15, 'wainscot': 1.05}
DOOR = {'y': (-3.0, -2.0), 'h': 2.2}
WINDOW = {'x': (1.0, 2.6), 'z': (0.9, 2.4)}
STAIR = {'x': (2.45, 3.5), 'y': (-1.0, 3.0), 'steps': 18}
OPENING = {'x': (2.4, 3.5), 'y': (0.2, 3.0)}
UPPER_H = 2.7
BOARD = {'x': (-1.9, 0.9), 'z': (1.25, 2.65)}
MAP = {'x': (-1.3, 0.3), 'z': (1.45, 2.25)}
COFFEE = (1.75, -4.6)            # 라운지 중심. 소파 북쪽 끝과 계단 첫 단 사이를 2m 이상 비운다.
FLOOR_LAMP = (3.25, COFFEE[1] + 1.3)

# 침실 — 거실 북쪽 7×7m. 문은 여행 보드와 계단 사이(서쪽 경첩, 80° 열린 채 고정).
BED_DOOR = {'x': (1.15, 2.05), 'h': 2.2, 'open_deg': 80.0}
BEDROOM = {'x': (-3.5, 3.5), 'y': (4.65, 11.65)}
BED_WINDOW = {'x': (-3.0, -1.6), 'z': (0.9, 2.3)}             # S03에서 캐릭터 바로 뒤
BED = {'x': (-3.5, -1.4), 'y': (7.0, 8.6), 'top': 0.57}       # 퀸, 머리는 서쪽 벽
DESK = {'x': (-1.3, 0.3), 'y': (10.9, 11.62), 'top': 0.76}   # 북쪽 벽 책상 — 지구본은 동쪽 끝
GLOBE = {'base': (-0.1, 11.2, 0.76), 'radius': 0.2, 'ring': 0.22, 'center_up': 0.345, 'tilt_deg': 23.5}
BACKPACK = {'base': (-2.3, 8.32, 0.57), 'yaw': -90.0, 'open_deg': 110.0}   # 앞면이 +Y(캐릭터 쪽)

S01_CAM = (2.2, -6.1, 1.85)
S02_CAM = (0.95, 10.35, 1.35)
S03_CAM = (-2.75, 6.0, 1.6)

# 샷별 대역 자세. yaw는 CHR_Root Z 회전(0=-Y, 90=+X, 180=+Y), 팔은 (어깨, 팔꿈치) X 회전(음수=앞으로).
# door는 현관문 Z 회전. S01b는 거실을 가로질러 침실 문에 닿은 순간(같은 S01 카메라).
POSES = {
    'S01': {'root': (-3.1, -2.5), 'yaw': 140, 'L': (0, -5), 'R': (-10, -10), 'door': -95},
    'S01b': {'root': (1.6, 4.1), 'yaw': 180, 'L': (5, -5), 'R': (-5, -5), 'door': 0},
    'S02': {'root': (-0.45, 10.5), 'yaw': 180, 'L': (-5, 0), 'R': (-50, -10), 'door': 0},
    'S03': {'root': (-2.3, 8.95), 'yaw': 0, 'L': (-45, -30), 'R': (-45, -30), 'door': 0},
}

PINS = [('Busan', 129.1, 35.2), ('Tokyo', 139.7, 35.7), ('Bangkok', 100.5, 13.8), ('Sydney', 151.2, -33.9),
        ('Rio', -43.2, -22.9), ('NewOrleans', -90.1, 30.0), ('Valencia', -0.4, 39.5), ('Munich', 11.6, 48.1)]

# 색(선형 RGB)·거칠기·금속·발광. 누아르: 짙은 나무 + 깊은 녹색 + 붉은 가죽, 따뜻한 전구색. 침실 벽지는 짙은 청록.
PALETTE = {
    'floor_a': ((0.28, 0.15, 0.08), 0.45), 'floor_b': ((0.36, 0.20, 0.10), 0.45), 'floor_gap': ((0.06, 0.035, 0.02), 0.6),
    'wallpaper': ((0.05, 0.13, 0.09), 0.8), 'wallpaper_bed': ((0.035, 0.075, 0.12), 0.8),
    'wood_dark': ((0.13, 0.065, 0.035), 0.5), 'wood': ((0.26, 0.13, 0.065), 0.45), 'wood_light': ((0.42, 0.24, 0.12), 0.45),
    'ceiling': ((0.10, 0.055, 0.03), 0.6),
    'leather_red': ((0.32, 0.05, 0.035), 0.4), 'leather_green': ((0.05, 0.16, 0.09), 0.45),
    'leather_brown': ((0.20, 0.09, 0.045), 0.45),
    'rug_red': ((0.34, 0.05, 0.04), 0.9), 'rug_dark': ((0.16, 0.03, 0.03), 0.9), 'rug_green': ((0.06, 0.16, 0.10), 0.9),
    'linen': ((0.78, 0.74, 0.66), 0.9), 'blanket': ((0.06, 0.09, 0.20), 0.9),
    'cork': ((0.58, 0.33, 0.15), 0.9), 'paper': ((0.84, 0.78, 0.64), 0.8), 'photo': ((0.26, 0.30, 0.34), 0.5),
    'ticket_a': ((0.80, 0.33, 0.12), 0.7), 'ticket_b': ((0.20, 0.42, 0.60), 0.7), 'pin': ((0.80, 0.04, 0.04), 0.4),
    'string': ((0.70, 0.04, 0.04), 0.8), 'brass': ((0.80, 0.58, 0.28), 0.3, 1.0), 'metal_dark': ((0.04, 0.04, 0.045), 0.4, 1.0),
    'shade_warm': ((0.95, 0.72, 0.42), 0.8, 0.0, 0.7), 'shade_green': ((0.04, 0.22, 0.12), 0.3, 0.0, 0.6),
    'bulb': ((1.0, 0.78, 0.48), 0.5, 0.0, 25.0),
    'night': ((0.02, 0.035, 0.085), 1.0, 0.0, 1.0), 'porch_floor': ((0.16, 0.16, 0.18), 0.8),
    'glass_dark': ((0.03, 0.05, 0.09), 0.1), 'clock_face': ((0.86, 0.82, 0.72), 0.6),
    'suitcase_tan': ((0.52, 0.33, 0.16), 0.5), 'suitcase_brown': ((0.26, 0.13, 0.06), 0.5), 'strap': ((0.10, 0.06, 0.035), 0.6),
    'plant': ((0.06, 0.22, 0.08), 0.6), 'pot': ((0.42, 0.26, 0.18), 0.7),
    'poster_a': ((0.05, 0.55, 0.60), 0.6), 'poster_b': ((0.95, 0.45, 0.10), 0.6), 'poster_c': ((0.80, 0.15, 0.45), 0.6),
    'poster_d': ((0.95, 0.80, 0.20), 0.6),
    'book_1': ((0.40, 0.08, 0.06), 0.6), 'book_2': ((0.08, 0.20, 0.14), 0.6), 'book_3': ((0.10, 0.12, 0.28), 0.6),
    'book_4': ((0.50, 0.36, 0.16), 0.6), 'book_5': ((0.22, 0.12, 0.06), 0.6), 'stone': ((0.60, 0.55, 0.45), 0.7),
    'hat': ((0.08, 0.07, 0.07), 0.6), 'scarf': ((0.50, 0.08, 0.10), 0.9), 'doorway_dark': ((0.01, 0.01, 0.012), 1.0),
    'bag': ((0.86, 0.42, 0.12), 0.6), 'bag_dark': ((0.50, 0.24, 0.08), 0.6),
    'item_cloth': ((0.55, 0.72, 0.86), 0.9), 'item_dark': ((0.10, 0.10, 0.11), 0.4), 'item_red': ((0.50, 0.08, 0.10), 0.6),
    'item_bottle': ((0.28, 0.62, 0.68), 0.3), 'item_book': ((0.88, 0.78, 0.52), 0.7),
    'chr': ((0.62, 0.66, 0.72), 0.5), 'chr_mark': ((0.10, 0.10, 0.12), 0.5),
    'guide_in': ((0.10, 0.85, 0.95), 0.5), 'guide_out': ((0.95, 0.20, 0.75), 0.5), 'guide_cam': ((0.98, 0.80, 0.10), 0.5),
    'label': ((0.95, 0.95, 0.92), 0.5),
}


def M(key):
    spec = PALETTE[key]
    rgb, rough = spec[0], spec[1]
    metal = spec[2] if len(spec) > 2 else 0.0
    emit = spec[3] if len(spec) > 3 else 0.0
    return k.principled('M_House_' + key, rgb, rough, metal, emit)


def _floor_material():
    m = bpy.data.materials.get('M_House_FloorBoards') or M('floor_a').copy()
    m.name = 'M_House_FloorBoards'
    nt = m.node_tree
    if any(n.type == 'TEX_BRICK' for n in nt.nodes):
        return m
    bsdf = next(n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED')
    co = nt.nodes.new('ShaderNodeTexCoord')
    brick = nt.nodes.new('ShaderNodeTexBrick')
    co.location = (bsdf.location.x - 650, bsdf.location.y)
    brick.location = (bsdf.location.x - 350, bsdf.location.y)
    nt.links.new(k.sock(co, 'Object', outputs=True), k.sock(brick, 'Vector'))
    for ident, key in (('Color1', 'floor_a'), ('Color2', 'floor_b'), ('Mortar', 'floor_gap')):
        k.sock(brick, ident).default_value = (*PALETTE[key][0], 1.0)
    k.sock(brick, 'Scale').default_value = 1.0
    k.sock(brick, 'Mortar Size').default_value = 0.004
    k.sock(brick, 'Brick Width').default_value = 1.1
    k.sock(brick, 'Row Height').default_value = 0.14
    nt.links.new(k.sock(brick, 'Color', outputs=True), k.sock(bsdf, 'Base Color'))
    return m


def _volume_material():
    m = bpy.data.materials.get('M_House_RoomHaze') or bpy.data.materials.new('M_House_RoomHaze')
    nt = m.node_tree
    for n in list(nt.nodes):
        if n.type == 'BSDF_PRINCIPLED':
            nt.nodes.remove(n)
    out = next(n for n in nt.nodes if n.type == 'OUTPUT_MATERIAL')
    vol = next((n for n in nt.nodes if n.type == 'PRINCIPLED_VOLUME'), None) or nt.nodes.new('ShaderNodeVolumePrincipled')
    k.sock(vol, 'Density').default_value = 0.012
    k.sock(vol, 'Anisotropy').default_value = 0.35
    nt.links.new(k.sock(vol, 'Volume', outputs=True), k.sock(out, 'Volume'))
    m.diffuse_color = (1, 1, 1, 0.05)
    return m


# ---------------------------------------------------------------- 씬

def _scene():
    sc = bpy.data.scenes.get(SCENE) or bpy.data.scenes.new(SCENE)
    sc.render.fps, sc.render.fps_base = FPS, 1.0
    sc.render.resolution_x, sc.render.resolution_y = 1920, 1080
    sc.render.resolution_percentage = 100
    sc.frame_start, sc.frame_end = PILOT_FRAMES
    sc.unit_settings.system = 'METRIC'
    sc.render.engine = 'BLENDER_EEVEE'
    ee = sc.eevee
    # 그림자 풀 기본 512MB로는 조명이 많은 닫힌 방에서 "섀도우 버퍼 전체"가 나며 가까운 샷이 검게 나온다.
    for name, value in (('taa_render_samples', 48), ('use_raytracing', True), ('use_shadows', True),
                        ('shadow_ray_count', 2), ('shadow_pool_size', '2048'), ('shadow_resolution_scale', 0.5),
                        ('use_volumetric_shadows', True), ('volumetric_samples', 64),
                        ('volumetric_end', 24.0), ('use_fast_gi', True)):
        try:
            setattr(ee, name, value)
        except (AttributeError, TypeError) as e:
            print('EEVEE 설정 건너뜀:', name, e)
    vs = sc.view_settings
    try:
        vs.view_transform = 'AgX'
        vs.look = 'AgX - Medium High Contrast'
    except TypeError as e:
        print('색 관리 룩 건너뜀:', e)
    sh = sc.display.shading          # Workbench 배치도용
    sh.light, sh.color_type = 'STUDIO', 'TEXTURE'
    sh.show_shadows, sh.show_cavity = False, True
    w = bpy.data.worlds.get('FF_House_Night') or bpy.data.worlds.new('FF_House_Night')
    bg = next((n for n in w.node_tree.nodes if n.type == 'BACKGROUND'), None)
    if bg is not None:
        bg.inputs[0].default_value = (0.004, 0.006, 0.012, 1.0)
        bg.inputs[1].default_value = 1.0
    sc.world = w
    for win in bpy.context.window_manager.windows:
        win.scene = sc
    return sc


def _drop_old():
    """세션에 열린 이전 버전 씬을 새 파일에 섞지 않는다. 디스크의 이전 .blend는 건드리지 않는다."""
    for name in OLD_TOPS:
        c = bpy.data.collections.get(name)
        if c is not None:
            k.purge(c)
            bpy.data.collections.remove(c)
    for name in OLD_SCENES:
        s = bpy.data.scenes.get(name)
        if s is not None and s.name != SCENE:
            bpy.data.scenes.remove(s)


# ---------------------------------------------------------------- 공통 도우미

def _wall(name, run, at, inward, a0, a1, z0, z1, c, paper='wallpaper', thick=None):
    """run='x'면 x∈[a0,a1]로 뻗는 벽. at은 방 쪽 면 좌표, inward는 방 쪽 방향(+1/-1).
    아래 1.05m는 나무 판벽(세로 몰딩 0.9m 간격)과 띠 몰딩, 위는 벽지. thick을 작게 주면 반대편 방의 벽 껍질."""
    t, W = HALL['t'] if thick is None else thick, HALL['wainscot']
    outer = at - inward * t
    span = lambda s0, s1, zz0, zz1: ((a0, s0, zz0), (a1, s1, zz1)) if run == 'x' else ((s0, a0, zz0), (s1, a1, zz1))
    k.box(name, *span(min(at, outer), max(at, outer), z0, z1), c, M(paper))
    if z0 >= W:
        return
    zt = min(z1, W)
    f0, f1 = sorted((at, at + inward * 0.03))
    k.box(name + '_Wainscot', *span(f0, f1, z0, zt), c, M('wood_dark'))
    if z1 > W:
        r0, r1 = sorted((at, at + inward * 0.05))
        k.box(name + '_Rail', *span(r0, r1, W - 0.02, W + 0.04), c, M('wood'))
    m0, m1 = sorted((at, at + inward * 0.045))
    p, i = a0 + 0.45, 0
    while p < a1 - 0.2:
        lo2, hi2 = ((p - 0.02, m0, z0 + 0.1), (p + 0.02, m1, zt - 0.1)) if run == 'x' else ((m0, p - 0.02, z0 + 0.1), (m1, p + 0.02, zt - 0.1))
        k.box(f'{name}_Mold_{i}', lo2, hi2, c, M('wood'))
        p, i = p + 0.9, i + 1


def _blinds(prefix, at, inward, x0, x1, z0, z1, c):
    """x 방향 벽(남·북)에 난 창의 틀·창턱·블라인드 살 20개. inward는 방 쪽(+1 남벽, -1 북벽)."""
    d = lambda a, b: tuple(sorted((at + inward * a, at + inward * b)))
    fy0, fy1 = d(0.0, 0.05)
    k.box(f'{prefix}_Frame_W', (x0 - 0.08, fy0, z0), (x0, fy1, z1), c, M('wood'))
    k.box(f'{prefix}_Frame_E', (x1, fy0, z0), (x1 + 0.08, fy1, z1), c, M('wood'))
    k.box(f'{prefix}_Frame_Top', (x0 - 0.08, fy0, z1), (x1 + 0.08, fy1, z1 + 0.08), c, M('wood'))
    sy0, sy1 = d(0.0, 0.12)
    k.box(f'{prefix}_Sill', (x0 - 0.12, sy0, z0 - 0.05), (x1 + 0.12, sy1, z0), c, M('wood'))
    ry0, ry1 = d(0.03, 0.08)
    k.box(f'{prefix}_Blind_Rail', (x0, ry0, z1 - 0.05), (x1, ry1, z1), c, M('wood_light'))
    ly0, ly1 = d(0.035, 0.075)
    n = 20
    for i in range(n):
        z = z0 + 0.04 + i * (z1 - z0 - 0.1) / (n - 1)
        s = k.box(f'{prefix}_Blind_Slat_{i:02d}', (x0 + 0.02, ly0, z - 0.002), (x1 - 0.02, ly1, z + 0.002), c, M('wood_light'))
        s.rotation_euler.x = math.radians(35 * inward)


def _armchair(name, loc, yaw_deg, c, key='leather_red'):
    """가죽 안락의자. 국소 -Y가 앞(yaw 0=-Y, 90=+X)."""
    arm = k.empty(f'{name}_Root', c, loc=(*loc, 0), rot=(0, 0, math.radians(yaw_deg)), size=0.2)
    k.box(f'{name}_Seat', (-0.42, -0.4, 0.1), (0.42, 0.4, 0.45), c, M(key), parent=arm, bevel=0.04)
    k.box(f'{name}_Back', (-0.42, 0.28, 0.45), (0.42, 0.42, 1.0), c, M(key), parent=arm, bevel=0.05)
    k.box(f'{name}_Arm_L', (0.3, -0.4, 0.45), (0.44, 0.42, 0.68), c, M(key), parent=arm, bevel=0.04)
    k.box(f'{name}_Arm_R', (-0.44, -0.4, 0.45), (-0.3, 0.42, 0.68), c, M(key), parent=arm, bevel=0.04)
    return arm


def _floor_lamp(name, xy, c):
    k.cylinder(f'{name}_Base', 0.15, 0.0, 0.03, c, M('brass'), xy=xy)
    k.cylinder(f'{name}_Pole', 0.015, 0.03, 1.5, c, M('brass'), xy=xy, segments=8)
    k.cylinder(f'{name}_Shade', 0.22, 1.42, 1.72, c, M('shade_warm'), xy=xy, radius2=0.16, segments=32)


def _poster_x(name, x0, x1, y_face, inward, z0, z1, key, c):
    """x 방향 벽(남·북)에 거는 액자 포스터."""
    a, b = sorted((y_face, y_face + inward * 0.035))
    k.box(f'{name}_Frame', (x0, a, z0), (x1, b, z1), c, M('wood_dark'))
    a2, b2 = sorted((y_face + inward * 0.035, y_face + inward * 0.04))
    k.box(name, (x0 + 0.05, a2, z0 + 0.05), (x1 - 0.05, b2, z1 - 0.05), c, M(key))


def _poster_y(name, y0, y1, x_face, inward, z0, z1, key, c):
    """y 방향 벽(동·서)에 거는 액자 포스터."""
    a, b = sorted((x_face, x_face + inward * 0.035))
    k.box(f'{name}_Frame', (a, y0, z0), (b, y1, z1), c, M('wood_dark'))
    a2, b2 = sorted((x_face + inward * 0.035, x_face + inward * 0.04))
    k.box(name, (a2, y0 + 0.04, z0 + 0.05), (b2, y1 - 0.04, z1 - 0.05), c, M(key))


# ---------------------------------------------------------------- 거실 뼈대: 벽·천장·계단

def _shell(c, c_south, c_out):
    (x0, x1), (y0, y1), h, t = HALL['x'], HALL['y'], HALL['h'], HALL['t']
    k.box('SET_Floor', (x0 - t, y0 - t, -0.05), (x1 + t, y1 + t, 0), c, _floor_material())
    # 북쪽 벽 — 침실 문 구멍. 침실 쪽 면에는 침실 벽지 껍질을 붙인다.
    bx0, bx1, bh = *BED_DOOR['x'], BED_DOOR['h']
    bxa, bxb = BEDROOM['x']
    _wall('SET_Wall_N_West', 'x', y1, -1, x0 - t, bx0, 0, h, c)
    _wall('SET_Wall_N_East', 'x', y1, -1, bx1, x1 + t, 0, h, c)
    _wall('SET_Wall_N_OverDoor', 'x', y1, -1, bx0, bx1, bh, h, c)
    skin = y1 + t + 0.01
    _wall('BED_Skin_S_West', 'x', skin, 1, bxa, bx0, 0, h, c, paper='wallpaper_bed', thick=0.01)
    _wall('BED_Skin_S_East', 'x', skin, 1, bx1, bxb, 0, h, c, paper='wallpaper_bed', thick=0.01)
    _wall('BED_Skin_S_OverDoor', 'x', skin, 1, bx0, bx1, bh, h, c, paper='wallpaper_bed', thick=0.01)
    for side, yy0, yy1 in (('Hall', y1 - 0.05, y1), ('Bed', y1 + t, y1 + t + 0.05)):
        k.box(f'SET_BedDoorJamb_W_{side}', (bx0 - 0.09, yy0, 0), (bx0, yy1, bh + 0.1), c, M('wood'))
        k.box(f'SET_BedDoorJamb_E_{side}', (bx1, yy0, 0), (bx1 + 0.09, yy1, bh + 0.1), c, M('wood'))
        k.box(f'SET_BedDoorHead_{side}', (bx0 - 0.09, yy0, bh), (bx1 + 0.09, yy1, bh + 0.12), c, M('wood'))
    bed_door = k.box('SET_BedDoor', (bx0 + 0.01, y1 + t, 0), (bx1 - 0.01, y1 + t + 0.05, bh - 0.01), c, M('wood_light'),
                     pivot=(bx0 + 0.01, y1 + t + 0.025, 0))
    bed_door.rotation_euler.z = math.radians(BED_DOOR['open_deg'])
    k.sphere('SET_BedDoor_Knob', 0.03, (0.8, -0.06, 1.0), c, M('brass'), parent=bed_door, u=16, v=8)
    _wall('SET_Wall_E', 'y', x1, -1, y0 - t, y1 + t, 0, h, c)
    dy0, dy1, dh = *DOOR['y'], DOOR['h']
    _wall('SET_Wall_W_South', 'y', x0, 1, y0 - t, dy0, 0, h, c)
    _wall('SET_Wall_W_North', 'y', x0, 1, dy1, y1 + t, 0, h, c)
    _wall('SET_Wall_W_OverDoor', 'y', x0, 1, dy0, dy1, dh, h, c)
    wx0, wx1, wz0, wz1 = *WINDOW['x'], *WINDOW['z']
    _wall('SET_Wall_S_West', 'x', y0, 1, x0 - t, wx0, 0, h, c_south)
    _wall('SET_Wall_S_East', 'x', y0, 1, wx1, x1 + t, 0, h, c_south)
    _wall('SET_Wall_S_UnderWindow', 'x', y0, 1, wx0, wx1, 0, wz0, c_south)
    _wall('SET_Wall_S_OverWindow', 'x', y0, 1, wx0, wx1, wz1, h, c_south)
    _blinds('SET_Window', y0, 1, wx0, wx1, wz0, wz1, c_south)
    # 현관문 틀·문 — 원점이 남쪽 경첩, 안쪽(+X)으로 열리며 열림은 Z 음수.
    k.box('SET_DoorJamb_S', (x0, dy0 - 0.09, 0), (x0 + 0.05, dy0, dh + 0.1), c, M('wood'))
    k.box('SET_DoorJamb_N', (x0, dy1, 0), (x0 + 0.05, dy1 + 0.09, dh + 0.1), c, M('wood'))
    k.box('SET_DoorHead', (x0, dy0 - 0.09, dh), (x0 + 0.05, dy1 + 0.09, dh + 0.12), c, M('wood'))
    door = k.box('SET_Door', (x0 - 0.05, dy0 + 0.01, 0), (x0, dy1 - 0.01, dh - 0.01), c, M('wood_light'),
                 pivot=(x0 - 0.025, dy0 + 0.01, 0))
    door['previs_note'] = '현관문. 원점=남쪽 경첩. rotation Z 0=닫힘, -90=직각(방 안쪽). S01은 -95'
    k.box('SET_Door_Window', (0.024, 0.25, 1.45), (0.032, 0.73, 1.95), c, M('glass_dark'), parent=door)
    k.sphere('SET_Door_Knob', 0.03, (0.06, 0.85, 1.0), c, M('brass'), parent=door, u=16, v=8)
    k.box('OUT_Porch_Floor', (x0 - 1.5, dy0 - 0.8, -0.05), (x0 - t, dy1 + 0.8, 0), c_out, M('porch_floor'))
    k.box('OUT_Porch_Backdrop', (x0 - 1.55, dy0 - 0.8, 0), (x0 - 1.5, dy1 + 0.8, 3.0), c_out, M('night'))
    k.box('OUT_Sky_South', (wx0 - 1.5, y0 - 1.65, -0.5), (wx1 + 1.5, y0 - 1.6, 4.0), c_out, M('night'))


def _ceiling(c):
    """격자 나무 천장과 계단 위 구멍, 구멍으로 보이는 2층 복도."""
    (x0, x1), (y0, y1), h, t = HALL['x'], HALL['y'], HALL['h'], HALL['t']
    (ox0, ox1), (oy0, oy1) = OPENING['x'], OPENING['y']
    slab = 0.12
    k.box('CEIL_Slab_Main', (x0 - t, y0 - t, h), (ox0, y1 + t, h + slab), c, M('ceiling'))
    k.box('CEIL_Slab_SouthEast', (ox0, y0 - t, h), (x1 + t, oy0, h + slab), c, M('ceiling'))
    k.box('CEIL_Slab_Landing', (ox0, oy1, h), (x1 + t, y1 + t, h + slab), c, M('ceiling'))
    y, i = y0 + 1.2, 0
    while y < y1 - 0.3:
        xe = ox0 if oy0 - 0.1 < y < oy1 + 0.1 else x1
        k.box(f'CEIL_Beam_X_{i}', (x0, y - 0.07, h - 0.2), (xe, y + 0.07, h), c, M('wood_dark'))
        y, i = y + 1.2, i + 1
    for i, x in enumerate((-2.3, -1.1, 0.1, 1.3)):
        k.box(f'CEIL_Beam_Y_{i}', (x - 0.07, y0, h - 0.2), (x + 0.07, y1, h), c, M('wood_dark'))
    uz0, uz1 = h + slab, h + slab + UPPER_H
    _wall('UP_Wall_E', 'y', x1, -1, oy0 - t, y1 + t, uz0, uz1, c)
    _wall('UP_Wall_N', 'x', y1, -1, ox0 - t, x1 + t, uz0, uz1, c)
    _wall('UP_Wall_S', 'x', oy0, 1, ox0 - t, x1 + t, uz0, uz1, c)
    _wall('UP_Wall_W_Landing', 'y', ox0, 1, oy1, y1 + t, uz0, uz1, c)
    k.box('UP_Doorway', (ox0, 3.35, uz0), (ox0 + 0.02, 4.2, uz0 + 2.1), c, M('doorway_dark'))
    k.box('UP_Ceiling', (ox0 - t, oy0 - t, uz1), (x1 + t, y1 + t, uz1 + 0.1), c, M('ceiling'))
    rail_z, rail_x = uz0 + 0.95, ox0 - 0.04     # 구멍 가장자리의 2층 바닥 위
    k.poly_curve('UP_Railing', [(rail_x, oy0, rail_z), (rail_x, oy1, rail_z)], c, M('wood_dark'), 0.03)
    y, i = oy0 + 0.12, 0
    while y < oy1:
        k.cylinder(f'UP_Baluster_{i}', 0.015, uz0, rail_z, c, M('wood_dark'), xy=(rail_x, y), segments=8)
        y, i = y + 0.18, i + 1
    for name, (px, py) in (('Study', (0.2, 3.0)), ('Lounge', COFFEE)):
        k.cylinder(f'CEIL_Pendant_{name}_Cord', 0.006, 2.55, h - 0.2, c, M('metal_dark'), xy=(px, py), segments=8)
        k.cylinder(f'CEIL_Pendant_{name}_Shade', 0.26, 2.35, 2.55, c, M('shade_green'), xy=(px, py), radius2=0.07)
        k.sphere(f'CEIL_Pendant_{name}_Bulb', 0.045, (px, py, 2.38), c, M('bulb'), u=12, v=6)


def _stairs(c):
    (sx0, sx1), (sy0, sy1), n = STAIR['x'], STAIR['y'], STAIR['steps']
    top = HALL['h'] + 0.12
    rise, run = top / n, (sy1 - sy0) / n
    for i in range(n):
        k.box(f'STAIR_Step_{i:02d}', (sx0, sy0 + i * run, 0), (sx1, sy0 + (i + 1) * run, (i + 1) * rise), c, M('wood'))
        k.box(f'STAIR_Nose_{i:02d}', (sx0 - 0.02, sy0 + i * run - 0.02, (i + 1) * rise - 0.03),
              (sx1, sy0 + i * run + 0.03, (i + 1) * rise), c, M('wood_light'))
    rail = lambda y: 0.95 + (y - sy0) / (sy1 - sy0) * top
    k.poly_curve('STAIR_Handrail', [(sx0 + 0.03, sy0 - 0.08, rail(sy0)), (sx0 + 0.03, sy1, rail(sy1))],
                 c, M('wood_dark'), 0.035)
    for i in range(n):
        y = sy0 + (i + 0.5) * run
        k.cylinder(f'STAIR_Baluster_{i:02d}', 0.016, (i + 1) * rise, rail(y), c, M('wood_dark'),
                   xy=(sx0 + 0.03, y), segments=8)
    k.box('STAIR_Newel', (sx0 - 0.04, sy0 - 0.16, 0), (sx0 + 0.1, sy0 - 0.02, 1.2), c, M('wood_dark'), bevel=0.01)


# ---------------------------------------------------------------- 거실 서재 쪽: 여행 보드·책상·책장

def _travel_board(c):
    (bx0, bx1), (bz0, bz1), y1 = BOARD['x'], BOARD['z'], HALL['y'][1]
    k.box('BRD_Frame', (bx0 - 0.07, y1 - 0.05, bz0 - 0.07), (bx1 + 0.07, y1 - 0.005, bz1 + 0.07), c, M('wood'))
    k.box('BRD_Cork', (bx0, y1 - 0.06, bz0), (bx1, y1 - 0.045, bz1), c, M('cork'))
    face = y1 - 0.062
    (mx0, mx1), (mz0, mz1) = MAP['x'], MAP['z']
    k.box('BRD_Map_Border', (mx0 - 0.03, face - 0.002, mz0 - 0.03), (mx1 + 0.03, face, mz1 + 0.03), c, M('paper'))
    world = k.image_material('M_House_WorldMap', EARTH_TEX, (0.25, 0.40, 0.55), rough=0.8)
    k.plane_facing_neg_y('BRD_WorldMap', mx0, mx1, mz0, mz1, face - 0.004, c, world)
    pts = []
    for name, lon, lat in PINS:
        x = mx0 + (lon + 180) / 360 * (mx1 - mx0)
        z = mz0 + (lat + 90) / 180 * (mz1 - mz0)
        k.sphere(f'BRD_Pin_{name}', 0.014, (x, face - 0.02, z), c, M('pin'), u=10, v=6)
        pts.append((x, face - 0.016, z))
    k.poly_curve('BRD_String_Route', pts, c, M('string'), 0.0025)
    rng = random.Random(11)
    slots = [(-1.72, 2.42), (-1.55, 2.05), (-1.74, 1.66), (-1.5, 1.38), (0.52, 2.45), (0.72, 2.12),
             (0.5, 1.8), (0.74, 1.48), (-0.95, 2.47), (-0.25, 2.47), (-0.8, 1.33), (-0.05, 1.32)]
    for i, (x, z) in enumerate(slots):
        w, hh = rng.choice(((0.2, 0.15), (0.16, 0.2), (0.22, 0.14)))
        k.box(f'BRD_Photo_{i:02d}', (x - w / 2, face - 0.006, z - hh / 2), (x + w / 2, face, z + hh / 2), c, M('paper'))
        key = rng.choice(('photo', 'poster_a', 'poster_b', 'poster_c', 'poster_d'))
        k.box(f'BRD_Photo_{i:02d}_Img', (x - w / 2 + 0.015, face - 0.008, z - hh / 2 + 0.03),
              (x + w / 2 - 0.015, face - 0.006, z + hh / 2 - 0.015), c, M(key))
        k.sphere(f'BRD_Photo_{i:02d}_Pin', 0.01, (x, face - 0.014, z + hh / 2 - 0.012), c, M('pin'), u=8, v=4)
    for i, (x, z, key) in enumerate(((-1.25, 2.52, 'ticket_a'), (0.15, 2.55, 'ticket_b'),
                                     (0.35, 1.32, 'ticket_a'), (-1.82, 1.9, 'ticket_b'))):
        k.box(f'BRD_Ticket_{i}', (x - 0.1, face - 0.009, z - 0.035), (x + 0.1, face - 0.006, z + 0.035), c, M(key))


def _banker_lamp(name, xy, top, c):
    """녹색 갓 은행원 스탠드(빛은 _lights)."""
    k.cylinder(f'{name}_Base', 0.07, top, top + 0.03, c, M('brass'), xy=xy)
    k.cylinder(f'{name}_Stem', 0.01, top + 0.03, top + 0.35, c, M('brass'), xy=xy, segments=8)
    shade = k.cylinder(f'{name}_Shade', 0.07, -0.18, 0.18, c, M('shade_green'), segments=16)
    shade.location, shade.rotation_euler = (xy[0], xy[1] - 0.07, top + 0.36), (0, math.radians(90), 0)


def _desk(c):
    x0, x1, y0, y1, top = -1.5, 0.3, 2.9, 3.8, 0.78
    k.box('DSK_Top', (x0, y0, top - 0.04), (x1, y1, top), c, M('wood'), bevel=0.01)
    k.box('DSK_Leather', (x0 + 0.15, y0 + 0.12, top), (x1 - 0.15, y1 - 0.12, top + 0.004), c, M('leather_green'))
    k.box('DSK_Pedestal_W', (x0 + 0.02, y0 + 0.02, 0), (x0 + 0.47, y1 - 0.02, top - 0.04), c, M('wood_dark'))
    k.box('DSK_Pedestal_E', (x1 - 0.47, y0 + 0.02, 0), (x1 - 0.02, y1 - 0.02, top - 0.04), c, M('wood_dark'))
    for side, xa in (('W', x0 + 0.02), ('E', x1 - 0.47)):
        for j, z in enumerate((0.2, 0.42, 0.62)):
            k.box(f'DSK_Drawer_{side}{j}', (xa + 0.03, y0 + 0.005, z - 0.08), (xa + 0.42, y0 + 0.02, z + 0.08), c, M('wood'))
            k.sphere(f'DSK_Knob_{side}{j}', 0.012, (xa + 0.225, y0 - 0.005, z), c, M('brass'), u=8, v=4)
    _banker_lamp('DSK_Lamp', (-1.18, 3.5), top, c)
    k.box('DSK_OpenMap', (-0.85, 3.05, top + 0.004), (-0.3, 3.45, top + 0.007), c, M('paper'))
    k.box('DSK_Passport', (-0.2, 3.1, top + 0.004), (-0.1, 3.24, top + 0.014), c, M('item_red'))
    k.box('DSK_Notebook', (-1.0, 3.2, top + 0.004), (-0.82, 3.44, top + 0.02), c, M('leather_brown'))
    k.cylinder('DSK_Compass', 0.04, top + 0.004, top + 0.02, c, M('brass'), xy=(-0.55, 3.6))
    k.box('DSK_Phone', (-0.05, 3.5, top + 0.004), (0.18, 3.72, top + 0.1), c, M('item_red'), bevel=0.02)
    k.cylinder('DSK_Mug', 0.04, top + 0.004, top + 0.1, c, M('clock_face'), xy=(-1.3, 3.1))
    k.box('DSK_Chair_Seat', (-0.9, 3.95, 0.25), (-0.35, 4.4, 0.48), c, M('leather_red'), bevel=0.04)
    k.box('DSK_Chair_Back', (-0.9, 4.3, 0.48), (-0.35, 4.42, 1.12), c, M('leather_red'), bevel=0.05)


def _bookcase(name, x0, x1, c, rng, y1=None, h=2.3):
    y1 = (HALL['y'][1] - 0.03) if y1 is None else y1
    y0 = y1 - 0.38
    k.box(f'{name}_Back', (x0, y1 - 0.02, 0), (x1, y1, h), c, M('wood_dark'))
    k.box(f'{name}_Side_W', (x0, y0, 0), (x0 + 0.04, y1, h), c, M('wood'))
    k.box(f'{name}_Side_E', (x1 - 0.04, y0, 0), (x1, y1, h), c, M('wood'))
    k.box(f'{name}_Top', (x0 - 0.03, y0 - 0.03, h), (x1 + 0.03, y1, h + 0.06), c, M('wood'))
    shelves = [z for z in (0.08, 0.5, 0.92, 1.34, 1.76) if z < h - 0.3]
    for i, z in enumerate(shelves):
        k.box(f'{name}_Shelf_{i}', (x0 + 0.04, y0, z - 0.03), (x1 - 0.04, y1 - 0.02, z), c, M('wood'))
        x = x0 + 0.06
        souvenir_at = rng.random() < 0.5 and i in (1, 2, 3)      # 기념품 = S05 나라별 조형물 복선
        limit = x1 - (0.3 if souvenir_at else 0.06)
        j = 0
        while x < limit - 0.03:
            w, hh, dp = rng.uniform(0.025, 0.06), rng.uniform(0.22, 0.34), rng.uniform(0.2, 0.28)
            key = rng.choice(('book_1', 'book_2', 'book_3', 'book_4', 'book_5'))
            k.box(f'{name}_Book_{i}_{j:02d}', (x, y1 - 0.03 - dp, z), (x + w, y1 - 0.03, z + hh), c, M(key))
            x, j = x + w + 0.003, j + 1
        if souvenir_at:
            sx, sy = x1 - 0.17, y0 + 0.19
            kind = rng.choice(('tower', 'pyramid', 'dome', 'pagoda'))
            if kind == 'tower':
                k.cylinder(f'{name}_Souvenir_{i}', 0.05, z, z + 0.3, c, M('brass'), xy=(sx, sy), segments=4, radius2=0.004)
            elif kind == 'pyramid':
                k.cylinder(f'{name}_Souvenir_{i}', 0.09, z, z + 0.12, c, M('stone'), xy=(sx, sy), segments=4, radius2=0.0)
            elif kind == 'dome':
                k.cylinder(f'{name}_Souvenir_{i}_Base', 0.07, z, z + 0.06, c, M('stone'), xy=(sx, sy))
                k.sphere(f'{name}_Souvenir_{i}', 0.07, (sx, sy, z + 0.06), c, M('brass'), u=16, v=8)
            else:
                for n_, (r, dz) in enumerate(((0.08, 0.0), (0.065, 0.07), (0.05, 0.14))):
                    k.box(f'{name}_Souvenir_{i}_{n_}', (sx - r, sy - r, z + dz), (sx + r, sy + r, z + dz + 0.05), c, M('item_red'))


def _plant(name, xy, c, height):
    k.cylinder(f'{name}_Pot', 0.17, 0.0, 0.34, c, M('pot'), xy=xy, radius2=0.2)
    k.cylinder(f'{name}_Stem', 0.015, 0.34, height - 0.25, c, M('plant'), xy=xy, segments=8)
    rng = random.Random(len(name))
    for i in range(6):
        a, r, z = rng.uniform(0, 2 * math.pi), rng.uniform(0.08, 0.2), rng.uniform(0.7, height)
        leaf = k.sphere(f'{name}_Leaf_{i}', 0.16, (xy[0] + r * math.cos(a), xy[1] + r * math.sin(a), z), c, M('plant'), u=12, v=6)
        leaf.scale = (1.0, 0.45, 0.25)
        leaf.rotation_euler = (math.radians(rng.uniform(-30, 30)), 0, a)


def _study(c):
    rng = random.Random(7)
    _travel_board(c)
    _desk(c)
    _bookcase('BKC_West', -3.4, -2.3, c, rng)
    y1 = HALL['y'][1]
    for i, x in enumerate((-3.15, -2.85, -2.55)):          # 세계 시계 셋 — 서쪽 책장 위
        rim = k.cylinder(f'CLK_{i}_Rim', 0.12, -0.02, 0.02, c, M('wood_dark'), segments=32)
        face = k.cylinder(f'CLK_{i}_Face', 0.1, 0.02, 0.025, c, M('clock_face'), segments=32)
        for ob in (rim, face):
            ob.location, ob.rotation_euler = (x, y1 - 0.03, 2.72), (math.radians(90), 0, 0)
    for i, (z, dx, rot, key) in enumerate(((0.0, 0.0, 0, 'suitcase_brown'), (0.22, 0.03, 6, 'suitcase_tan'),
                                          (0.42, -0.02, -4, 'suitcase_brown'))):
        s = k.box(f'LUG_Suitcase_{i}', (-0.33, -0.21, 0), (0.33, 0.21, 0.2 if i != 2 else 0.18), c, M(key), bevel=0.02)
        s.location, s.rotation_euler = (-2.05 + dx, 3.55, z), (0, 0, math.radians(rot))
        k.box(f'LUG_Suitcase_{i}_Strap', (-0.2, -0.215, -0.005), (-0.16, 0.215, 0.205), c, M('strap'), parent=s)
        k.box(f'LUG_Suitcase_{i}_Strap2', (0.16, -0.215, -0.005), (0.2, 0.215, 0.205), c, M('strap'), parent=s)
    k.box('LUG_Trunk', (-3.45, 2.7, 0), (-2.9, 3.6, 0.5), c, M('suitcase_tan'), bevel=0.02)
    k.box('LUG_Trunk_Band', (-3.46, 2.95, -0.005), (-2.89, 3.0, 0.505), c, M('brass'))
    k.box('LUG_Trunk_Band2', (-3.46, 3.3, -0.005), (-2.89, 3.35, 0.505), c, M('brass'))
    _plant('PLT_Study', (-2.75, 2.2), c, 1.4)
    x0 = HALL['x'][0]
    k.box('SDB_Sideboard', (x0 + 0.02, 0.6, 0), (x0 + 0.47, 2.2, 0.9), c, M('wood'), bevel=0.01)
    k.box('SDB_Radio', (x0 + 0.12, 1.2, 0.9), (x0 + 0.4, 1.62, 1.12), c, M('wood_light'), bevel=0.02)
    k.box('SDB_Radio_Dial', (x0 + 0.4, 1.28, 0.95), (x0 + 0.405, 1.54, 1.05), c, M('clock_face'))
    for i, (y, key) in enumerate(((0.8, 'poster_a'), (1.4, 'poster_b'), (2.0, 'poster_c'))):
        _poster_y(f'SDB_Poster_{i}', y - 0.24, y + 0.24, x0, 1, 1.35, 2.05, key, c)


# ---------------------------------------------------------------- 거실 라운지(남쪽 창가)

def _lounge(c):
    """라운지는 소파(동쪽 벽)·탁자·안락의자(탁자 서쪽, 소파와 마주 봄)를 COFFEE 기준으로 둔다."""
    cx, cy = COFFEE
    k.cylinder('LNG_Rug_Border', 1.45, 0.0, 0.012, c, M('rug_green'), xy=(cx, cy), segments=64)
    k.cylinder('LNG_Rug', 1.3, 0.012, 0.016, c, M('rug_red'), xy=(cx, cy), segments=64)
    k.cylinder('LNG_Rug_Medallion', 0.5, 0.016, 0.018, c, M('rug_dark'), xy=(cx, cy), segments=48)
    ya, yb = cy - 1.0, cy + 1.0
    k.box('LNG_Sofa_Seat', (2.6, ya, 0.1), (3.4, yb, 0.42), c, M('leather_red'), bevel=0.04)
    k.box('LNG_Sofa_Back', (3.15, ya, 0.42), (3.45, yb, 0.86), c, M('leather_red'), bevel=0.06)
    k.box('LNG_Sofa_Arm_S', (2.58, ya - 0.04, 0.1), (3.45, ya + 0.22, 0.7), c, M('leather_red'), bevel=0.06)
    k.box('LNG_Sofa_Arm_N', (2.58, yb - 0.22, 0.1), (3.45, yb + 0.04, 0.7), c, M('leather_red'), bevel=0.06)
    for i, (p, q) in enumerate(((ya + 0.24, cy - 0.02), (cy + 0.02, yb - 0.24))):
        k.box(f'LNG_Sofa_Cushion_{i}', (2.62, p, 0.42), (3.14, q, 0.52), c, M('leather_red'), bevel=0.04)
    k.box('LNG_Table_Top', (cx - 0.55, cy - 0.3, 0.42), (cx + 0.55, cy + 0.3, 0.46), c, M('wood'), bevel=0.01)
    k.box('LNG_Table_Shelf', (cx - 0.5, cy - 0.25, 0.1), (cx + 0.5, cy + 0.25, 0.12), c, M('wood_dark'))
    for i, (sx, sy) in enumerate(((-1, -1), (1, -1), (-1, 1), (1, 1))):
        x, y = cx + sx * 0.5, cy + sy * 0.25
        k.box(f'LNG_Table_Leg_{i}', (x - 0.025, y - 0.025, 0), (x + 0.025, y + 0.025, 0.42), c, M('wood_dark'))
    k.box('LNG_Tray', (cx - 0.3, cy - 0.18, 0.46), (cx + 0.05, cy + 0.18, 0.475), c, M('brass'))
    k.cylinder('LNG_Mug_0', 0.04, 0.475, 0.57, c, M('clock_face'), xy=(cx - 0.18, cy - 0.05))
    k.cylinder('LNG_Mug_1', 0.04, 0.475, 0.57, c, M('clock_face'), xy=(cx - 0.05, cy + 0.08))
    k.box('LNG_Magazine_0', (cx + 0.15, cy - 0.2, 0.46), (cx + 0.42, cy + 0.02, 0.47), c, M('poster_b'))
    k.box('LNG_Magazine_1', (cx + 0.18, cy - 0.05, 0.47), (cx + 0.45, cy + 0.17, 0.48), c, M('poster_a'))
    _armchair('LNG_Armchair', (cx - 1.2, cy), 90, c)
    _floor_lamp('LNG_FloorLamp', FLOOR_LAMP, c)
    y0 = HALL['y'][0]
    k.box('LNG_Drawers', (-0.9, y0 + 0.03, 0), (0.3, y0 + 0.48, 0.85), c, M('wood'), bevel=0.01)
    for j, z in enumerate((0.18, 0.44, 0.7)):
        k.box(f'LNG_Drawers_Front_{j}', (-0.86, y0 + 0.48, z - 0.1), (0.26, y0 + 0.49, z + 0.1), c, M('wood_light'))
    k.cylinder('LNG_TableLamp_Base', 0.06, 0.85, 1.15, c, M('brass'), xy=(-0.55, y0 + 0.25), radius2=0.03)
    k.cylinder('LNG_TableLamp_Shade', 0.2, 1.12, 1.36, c, M('shade_warm'), xy=(-0.55, y0 + 0.25), radius2=0.13, segments=32)
    k.box('LNG_Book', (-0.1, y0 + 0.12, 0.85), (0.2, y0 + 0.36, 0.9), c, M('book_4'))
    _plant('PLT_Window', (0.68, y0 + 0.35), c, 1.3)
    x1 = HALL['x'][1]
    for i, (dy, key) in enumerate(((-0.6, 'poster_d'), (0.0, 'poster_a'), (0.6, 'poster_b'))):
        _poster_y(f'LNG_Poster_{i}', cy + dy - 0.22, cy + dy + 0.22, x1, -1, 1.35, 1.95, key, c)
    # 계단 입구 앞은 비워 둔다 — 대신 계단 옆 벽(동쪽) 포스터 한 장과 러그만
    _poster_y('LNG_StairPoster', -2.5, -1.9, x1, -1, 1.35, 2.05, 'poster_c', c)


def _entry(c):
    x0 = HALL['x'][0]
    dy0, dy1 = DOOR['y']
    k.box('ENT_Doormat', (x0 + 0.05, dy0 + 0.05, 0), (x0 + 0.75, dy1 - 0.05, 0.012), c, M('rug_dark'))
    cx, cy = x0 + 0.3, -3.85
    k.cylinder('ENT_CoatRack_Base', 0.22, 0.0, 0.04, c, M('wood_dark'), xy=(cx, cy))
    k.cylinder('ENT_CoatRack_Pole', 0.022, 0.04, 1.8, c, M('wood_dark'), xy=(cx, cy), segments=12)
    k.sphere('ENT_CoatRack_Top', 0.04, (cx, cy, 1.82), c, M('wood_dark'), u=12, v=6)
    k.cylinder('ENT_Hat_Brim', 0.17, 1.78, 1.8, c, M('hat'), xy=(cx + 0.12, cy), segments=32)
    k.cylinder('ENT_Hat_Crown', 0.1, 1.8, 1.93, c, M('hat'), xy=(cx + 0.12, cy), segments=24)
    k.box('ENT_Scarf', (cx - 0.14, cy - 0.06, 1.05), (cx - 0.04, cy + 0.06, 1.7), c, M('scarf'))
    k.cylinder('ENT_UmbrellaStand', 0.1, 0.0, 0.5, c, M('brass'), xy=(x0 + 0.25, -1.75))
    k.cylinder('ENT_Umbrella', 0.015, 0.4, 0.95, c, M('hat'), xy=(x0 + 0.27, -1.73), segments=8)
    k.box('ENT_Console', (x0 + 0.02, -1.45, 0), (x0 + 0.38, -0.45, 0.8), c, M('wood'), bevel=0.01)
    k.cylinder('ENT_KeyBowl', 0.1, 0.8, 0.84, c, M('brass'), xy=(x0 + 0.2, -0.75), radius2=0.12)
    for i, (y, key) in enumerate(((-1.25, 'poster_d'), (-0.65, 'poster_c'))):
        _poster_y(f'ENT_Poster_{i}', y - 0.22, y + 0.22, x0, 1, 1.35, 1.95, key, c)
    # 거실 가운데 긴 러그 — 현관에서 침실 문으로 가는 동선이 그 위를 지난다
    k.box('HAL_Rug_Border', (-2.3, -2.4, 0), (0.9, 1.6, 0.012), c, M('rug_green'))
    k.box('HAL_Rug', (-2.2, -2.3, 0.012), (0.8, 1.5, 0.016), c, M('rug_red'))


# ---------------------------------------------------------------- 침실

def _desk_globe(c):
    """책상 지구본. Root(받침, 책상 위) → Axis(23.5° 기울기, 자오환) → Earth(국소 Z 회전)."""
    g = GLOBE
    root = k.empty('GLB_Root', c, loc=g['base'], size=0.1)
    k.cylinder('GLB_Base', 0.1, 0.0, 0.025, c, M('wood_dark'), parent=root, segments=32)
    k.cylinder('GLB_Post', 0.014, 0.025, g['center_up'] - g['ring'], c, M('brass'), parent=root)
    axis = k.empty('GLB_Axis', c, parent=root, loc=(0, 0, g['center_up']),
                   rot=(0, math.radians(g['tilt_deg']), 0), size=0.3, kind='SINGLE_ARROW')
    k.poly_curve('GLB_Meridian', k.circle(g['ring'], plane='XZ'), c, M('brass'), 0.007, cyclic=True, parent=axis)
    k.cylinder('GLB_Pin', 0.005, -g['ring'], g['ring'], c, M('brass'), parent=axis, segments=12)
    earth_mat = k.image_material('M_House_Earth_Basic', EARTH_TEX, (0.25, 0.45, 0.75), rough=0.35)
    earth = k.sphere('GLB_Earth', g['radius'], (0, 0, 0), c, earth_mat, parent=axis, u=96, v=48, uvs=True)
    earth['previs_note'] = '기본 지구 텍스처(Solar System Scope 2K, CC BY 4.0). 국소 Z 양수 회전=서→동'
    return earth


def _bedroom(c, c_ceiling, c_out):
    """거실 북쪽 7×7m. 서쪽 벽 퀸 침대 머리, 북쪽 벽 블라인드 창(S03 캐릭터 뒤)과 책상(지구본), 북동 독서 구석,
    동쪽 벽 옷장·포스터, 문 옆 서랍장."""
    (bxa, bxb), (bya, byb), h, t = BEDROOM['x'], BEDROOM['y'], HALL['h'], HALL['t']
    y1 = HALL['y'][1]
    k.box('BED_Floor', (bxa - t, y1 + t, -0.05), (bxb + t, byb + t, 0), c, _floor_material())
    _wall('BED_Wall_W', 'y', bxa, 1, bya, byb + t, 0, h, c, paper='wallpaper_bed')
    _wall('BED_Wall_E', 'y', bxb, -1, bya, byb + t, 0, h, c, paper='wallpaper_bed')
    wx0, wx1, wz0, wz1 = *BED_WINDOW['x'], *BED_WINDOW['z']
    _wall('BED_Wall_N_West', 'x', byb, -1, bxa - t, wx0, 0, h, c, paper='wallpaper_bed')
    _wall('BED_Wall_N_East', 'x', byb, -1, wx1, bxb + t, 0, h, c, paper='wallpaper_bed')
    _wall('BED_Wall_N_UnderWindow', 'x', byb, -1, wx0, wx1, 0, wz0, c, paper='wallpaper_bed')
    _wall('BED_Wall_N_OverWindow', 'x', byb, -1, wx0, wx1, wz1, h, c, paper='wallpaper_bed')
    _blinds('BED_Window', byb, -1, wx0, wx1, wz0, wz1, c)
    k.box('OUT_Sky_North', (wx0 - 1.5, byb + 1.6, -0.5), (wx1 + 1.5, byb + 1.65, 4.0), c_out, M('night'))
    k.box('CEIL_Bedroom', (bxa - t, y1, h), (bxb + t, byb + t, h + 0.12), c_ceiling, M('ceiling'))
    y, i = bya + 1.4, 0
    while y < byb - 0.3:
        k.box(f'CEIL_Bedroom_Beam_{i}', (bxa, y - 0.07, h - 0.18), (bxb, y + 0.07, h), c_ceiling, M('wood_dark'))
        y, i = y + 1.4, i + 1
    # 퀸 침대 — 머리는 서쪽 벽. 캐릭터는 북쪽에서 카메라(남쪽)를 보며 짐을 싼다.
    (ex0, ex1), (ey0, ey1), top = BED['x'], BED['y'], BED['top']
    ecy = (ey0 + ey1) / 2
    k.box('BED_Frame', (ex0 + 0.02, ey0, 0), (ex1, ey1, 0.32), c, M('wood_dark'))
    k.box('BED_Mattress', (ex0 + 0.04, ey0 + 0.03, 0.32), (ex1 - 0.03, ey1 - 0.03, 0.54), c, M('linen'), bevel=0.03)
    k.box('BED_Blanket', (ex0 + 0.6, ey0 - 0.01, 0.5), (ex1 + 0.02, ey1 + 0.01, top), c, M('blanket'), bevel=0.02)
    for i, (ya, yb) in enumerate(((ey0 + 0.1, ecy - 0.03), (ecy + 0.03, ey1 - 0.1))):
        k.box(f'BED_Pillow_{i}', (ex0 + 0.08, ya, 0.54), (ex0 + 0.55, yb, 0.67), c, M('linen'), bevel=0.04)
    k.box('BED_Headboard', (ex0, ey0 - 0.05, 0), (ex0 + 0.08, ey1 + 0.05, 1.15), c, M('wood'), bevel=0.01)
    k.box('BED_Footboard', (ex1 - 0.04, ey0 - 0.03, 0), (ex1 + 0.04, ey1 + 0.03, 0.5), c, M('wood'), bevel=0.01)
    k.box('BED_Bench', (ex1 + 0.12, ey0 + 0.15, 0), (ex1 + 0.55, ey1 - 0.15, 0.45), c, M('leather_brown'), bevel=0.03)
    k.box('BED_Map_Frame', (bxa, ecy - 0.85, 1.3), (bxa + 0.03, ecy + 0.85, 2.1), c, M('wood_dark'))
    world = k.image_material('M_House_WorldMap', EARTH_TEX, (0.25, 0.40, 0.55), rough=0.8)
    wm = k.plane_facing_neg_y('BED_WorldMap', -0.75, 0.75, -0.36, 0.36, 0.0, c, world)
    wm.location, wm.rotation_euler = (bxa + 0.035, ecy, 1.7), (0, 0, math.radians(90))
    for side, (ya, yb) in (('N', (ey1 + 0.08, ey1 + 0.52)), ('S', (ey0 - 0.52, ey0 - 0.08))):
        k.box(f'BED_Nightstand_{side}', (ex0 + 0.02, ya, 0), (ex0 + 0.48, yb, 0.55), c, M('wood'), bevel=0.01)
        lx, ly = ex0 + 0.25, (ya + yb) / 2
        k.cylinder(f'BED_Lamp_{side}_Base', 0.05, 0.55, 0.8, c, M('brass'), xy=(lx, ly), radius2=0.025)
        k.cylinder(f'BED_Lamp_{side}_Shade', 0.16, 0.78, 0.98, c, M('shade_warm'), xy=(lx, ly), radius2=0.11, segments=32)
    k.box('BED_Rug_Border', (ex1 - 1.5, ey0 - 0.6, 0), (ex1 + 1.2, ey1 + 0.6, 0.012), c, M('rug_green'))
    k.box('BED_Rug', (ex1 - 1.4, ey0 - 0.5, 0.012), (ex1 + 1.1, ey1 + 0.5, 0.016), c, M('rug_red'))
    # 북쪽 벽 책상 — 은행원 스탠드·책·지도, 동쪽 끝에 지구본(위 벽등). 의자는 서쪽으로 빼 둔다(S02 자리).
    (dx0, dx1), (dy0, dy1), dtop = DESK['x'], DESK['y'], DESK['top']
    k.box('BDK_Top', (dx0, dy0, dtop - 0.04), (dx1, dy1, dtop), c, M('wood'), bevel=0.01)
    k.box('BDK_Leather', (dx0 + 0.12, dy0 + 0.1, dtop), (dx1 - 0.12, dy1 - 0.1, dtop + 0.004), c, M('leather_green'))
    k.box('BDK_Pedestal_W', (dx0 + 0.02, dy0 + 0.02, 0), (dx0 + 0.45, dy1 - 0.02, dtop - 0.04), c, M('wood_dark'))
    k.box('BDK_Pedestal_E', (dx1 - 0.45, dy0 + 0.02, 0), (dx1 - 0.02, dy1 - 0.02, dtop - 0.04), c, M('wood_dark'))
    for side, xa in (('W', dx0 + 0.02), ('E', dx1 - 0.45)):
        for j, z in enumerate((0.2, 0.42, 0.62)):
            k.box(f'BDK_Drawer_{side}{j}', (xa + 0.03, dy0 + 0.005, z - 0.08), (xa + 0.4, dy0 + 0.02, z + 0.08), c, M('wood'))
    _banker_lamp('BDK_Lamp', (dx0 + 0.3, dy1 - 0.2), dtop, c)
    for j, (bw, key) in enumerate(((0.05, 'book_1'), (0.04, 'book_3'), (0.06, 'book_4'))):
        xb = dx0 + 0.55 + j * 0.07
        k.box(f'BDK_Book_{j}', (xb, dy1 - 0.3, dtop), (xb + bw, dy1 - 0.08, dtop + 0.26), c, M(key))
    k.box('BDK_Map', (dx0 + 0.6, dy0 + 0.1, dtop + 0.004), (dx0 + 1.05, dy0 + 0.42, dtop + 0.007), c, M('paper'))
    k.box('BDK_Passport', (dx0 + 0.3, dy0 + 0.12, dtop + 0.004), (dx0 + 0.4, dy0 + 0.26, dtop + 0.014), c, M('item_red'))
    chair = k.empty('BDK_Chair_Root', c, loc=(dx0 - 0.25, dy0 - 0.35, 0), rot=(0, 0, math.radians(25)), size=0.15)
    k.box('BDK_Chair_Seat', (-0.25, -0.22, 0.43), (0.25, 0.22, 0.47), c, M('leather_brown'), parent=chair, bevel=0.02)
    k.box('BDK_Chair_Back', (-0.25, 0.18, 0.47), (0.25, 0.24, 0.95), c, M('leather_brown'), parent=chair, bevel=0.02)
    for i, (lx, ly) in enumerate(((-0.21, -0.18), (0.21, -0.18), (-0.21, 0.18), (0.21, 0.18))):
        k.box(f'BDK_Chair_Leg_{i}', (lx - 0.02, ly - 0.02, 0), (lx + 0.02, ly + 0.02, 0.43), c, M('wood_dark'), parent=chair)
    gx, gy = GLOBE['base'][:2]
    k.box('BED_Sconce_Arm', (gx - 0.02, byb - 0.18, 2.05), (gx + 0.02, byb, 2.09), c, M('brass'))
    k.cylinder('BED_Sconce_Shade', 0.1, 2.0, 2.18, c, M('shade_warm'), xy=(gx, byb - 0.2), radius2=0.06, segments=24)
    _bookcase('BKC_Bedroom', 0.6, 1.5, c, random.Random(3), y1=byb - 0.03, h=2.2)
    _plant('PLT_Bedroom_NW', (bxa + 0.35, byb - 0.4), c, 1.4)
    # 북동 독서 구석 — 안락의자·스탠드·둥근 러그
    k.cylinder('BED_ReadRug', 0.9, 0.0, 0.012, c, M('rug_dark'), xy=(2.55, 10.5), segments=48)
    _armchair('BED_ReadChair', (2.6, 10.6), -45, c, key='leather_green')    # 등은 북동 구석, 앞은 남서(방 안)
    _floor_lamp('BED_ReadLamp', (3.15, 11.2), c)
    # 동쪽 벽 옷장·포스터, 문 옆 서랍장·포스터
    k.box('BED_Wardrobe', (2.95, 5.4, 0), (bxb - 0.02, 7.4, 2.1), c, M('wood'), bevel=0.01)
    k.box('BED_Wardrobe_Seam', (2.945, 6.395, 0.05), (2.95, 6.405, 2.05), c, M('wood_dark'))
    for i, y in enumerate((6.33, 6.47)):
        k.sphere(f'BED_Wardrobe_Knob_{i}', 0.015, (2.935, y, 1.05), c, M('brass'), u=8, v=4)
    for i, (y, key) in enumerate(((8.1, 'poster_a'), (8.8, 'poster_d'), (9.5, 'poster_b'))):
        _poster_y(f'BED_Poster_E{i}', y - 0.25, y + 0.25, bxb, -1, 1.3, 2.0, key, c)
    k.box('BED_Dresser', (2.25, bya + 0.03, 0), (3.45, bya + 0.5, 0.85), c, M('wood'), bevel=0.01)
    for j, z in enumerate((0.2, 0.46, 0.72)):
        k.box(f'BED_Dresser_Front_{j}', (2.29, bya + 0.5, z - 0.1), (3.41, bya + 0.51, z + 0.1), c, M('wood_light'))
    _poster_x('BED_Poster_S', 2.5, 3.2, bya + 0.01, 1, 1.3, 2.0, 'poster_c', c)


# ---------------------------------------------------------------- 배낭·대역

def _backpack(c):
    """배낭 — 국소 -X가 앞면. 침대 위에서 yaw -90°로 앞면이 +Y(캐릭터 쪽). 덮개 Y 110° 열림 / 0° 닫힘."""
    root = k.empty('BAG_Root', c, loc=BACKPACK['base'], rot=(0, 0, math.radians(BACKPACK['yaw'])), size=0.1)
    k.box('BAG_Body', (-0.1, -0.16, 0.0), (0.1, 0.16, 0.44), c, M('bag'), parent=root, bevel=0.03)
    k.box('BAG_FrontPocket', (-0.14, -0.12, 0.06), (-0.09, 0.12, 0.25), c, M('bag_dark'), parent=root, bevel=0.015)
    for side, sy in (('L', 0.07), ('R', -0.11)):
        k.box(f'BAG_Strap_{side}', (0.1, sy, 0.08), (0.125, sy + 0.04, 0.4), c, M('bag_dark'), parent=root, bevel=0.008)
    pivot = k.empty('BAG_FlapPivot', c, parent=root, loc=(0.1, 0.0, 0.44),
                    rot=(0, math.radians(BACKPACK['open_deg']), 0), size=0.08, kind='SINGLE_ARROW')
    k.box('BAG_Flap', (-0.215, -0.165, 0.0), (0.005, 0.165, 0.04), c, M('bag_dark'), parent=pivot, bevel=0.012)
    pivot['previs_note'] = 'rotation Y 110°=열림, 0°=닫힘. S04 닫힘 소리 타이밍 기준'
    bx, by, z = BACKPACK['base']
    k.box('ITEM_Clothes', (bx - 0.45, by - 0.62, z), (bx - 0.18, by - 0.37, z + 0.1), c, M('item_cloth'), bevel=0.02)
    k.box('ITEM_Camera', (bx + 0.25, by - 0.32, z), (bx + 0.37, by - 0.24, z + 0.08), c, M('item_dark'), bevel=0.01)
    k.box('ITEM_Passport', (bx + 0.45, by - 0.57, z), (bx + 0.54, by - 0.45, z + 0.012), c, M('item_red'))
    k.box('ITEM_Guidebook', (bx + 0.15, by - 0.72, z), (bx + 0.33, by - 0.5, z + 0.03), c, M('item_book'))
    k.cylinder('ITEM_Bottle', 0.035, z, z + 0.22, c, M('item_bottle'), xy=(bx + 0.55, by - 0.12))


def _standin(c):
    """키 1.8m 마네킹. 국소 -Y가 정면, 캐릭터 왼쪽=+X. 자세는 pose(샷)."""
    root = k.empty('CHR_Root', c, size=0.3, kind='SINGLE_ARROW')
    root['previs_note'] = 'Tripo 리깅 모델로 교체할 대역'
    for side, sx in (('L', 0.1), ('R', -0.1)):
        k.cylinder(f'CHR_Leg_{side}', 0.07, 0.06, 0.88, c, M('chr'), parent=root, xy=(sx, 0.0))
        k.box(f'CHR_Foot_{side}', (sx - 0.06, -0.22, 0.0), (sx + 0.06, 0.06, 0.07), c, M('chr'), parent=root, bevel=0.02)
    k.box('CHR_Pelvis', (-0.17, -0.1, 0.84), (0.17, 0.1, 1.0), c, M('chr'), parent=root, bevel=0.04)
    k.box('CHR_Torso', (-0.2, -0.11, 1.0), (0.2, 0.11, 1.44), c, M('chr'), parent=root, bevel=0.05)
    k.cylinder('CHR_Neck', 0.05, 1.44, 1.56, c, M('chr'), parent=root)
    k.sphere('CHR_Head', 0.12, (0, 0, 1.68), c, M('chr'), parent=root)
    k.box('CHR_FaceMark', (-0.025, -0.15, 1.64), (0.025, -0.1, 1.7), c, M('chr_mark'), parent=root)
    for side, sx in (('L', 0.25), ('R', -0.25)):
        sh = k.empty(f'CHR_Shoulder_{side}', c, parent=root, loc=(sx, 0.0, 1.4), size=0.06)
        k.cylinder(f'CHR_UpperArm_{side}', 0.045, -0.29, 0.0, c, M('chr'), parent=sh)
        el = k.empty(f'CHR_Elbow_{side}', c, parent=sh, loc=(0, 0, -0.29), size=0.05)
        k.cylinder(f'CHR_Forearm_{side}', 0.04, -0.26, 0.0, c, M('chr'), parent=el)
        k.sphere(f'CHR_Hand_{side}', 0.05, (0, 0, -0.3), c, M('chr'), parent=el, u=16, v=8)


def pose(shot):
    """대역과 현관문을 샷 시작 자세로 놓는다. 키프레임은 넣지 않는다(배치 확인용)."""
    p = POSES[shot]
    root = bpy.data.objects['CHR_Root']
    root.location = (*p['root'], 0.0)
    root.rotation_euler = (0, 0, math.radians(p['yaw']))
    for side in ('L', 'R'):
        s, e = p[side]
        bpy.data.objects[f'CHR_Shoulder_{side}'].rotation_euler = (math.radians(s), 0, 0)
        bpy.data.objects[f'CHR_Elbow_{side}'].rotation_euler = (math.radians(e), 0, 0)
    bpy.data.objects['SET_Door'].rotation_euler = (0, 0, math.radians(p['door']))


# ---------------------------------------------------------------- 조명·안개·카메라·안내선

def _lights(c):
    warm = (1.0, 0.66, 0.36)
    (x0, x1), (y0, y1) = HALL['x'], HALL['y']
    wcx = sum(WINDOW['x']) / 2
    # 거실 — 달빛은 남쪽 블라인드 창에서 라운지 러그로
    k.light('LGT_Moon', 'SUN', (wcx, y0 - 3.5, 6.5), c, (0.55, 0.68, 1.0), 2.5, target=(wcx, y0 + 1.9, 0.0), angle_deg=0.5)
    k.light('LGT_Moon_Fill', 'AREA', (wcx, y0 - 0.7, 1.7), c, (0.5, 0.62, 1.0), 60.0, target=(wcx, y0 + 4.5, 1.0), size=1.6)
    k.light('LGT_Pendant_Study', 'POINT', (0.2, 3.0, 2.34), c, warm, 120.0, radius=0.05)
    k.light('LGT_Pendant_Lounge', 'POINT', (*COFFEE, 2.34), c, warm, 90.0, radius=0.05)
    k.light('LGT_DeskLamp', 'POINT', (-1.18, 3.42, 1.08), c, warm, 25.0, radius=0.03)
    k.light('LGT_FloorLamp', 'POINT', (*FLOOR_LAMP, 1.58), c, warm, 60.0, radius=0.12)
    k.light('LGT_TableLamp', 'POINT', (-0.55, y0 + 0.25, 1.22), c, warm, 35.0, radius=0.1)
    k.light('LGT_Porch', 'POINT', (x0 - 0.9, -2.5, 2.3), c, warm, 20.0, radius=0.1)
    k.light('LGT_Fill', 'AREA', (0.0, (y0 + y1) / 2, 3.0), c, (1.0, 0.8, 0.6), 80.0, target=(0.0, (y0 + y1) / 2, 0.0), size=6.0)
    # 침실 — 머리맡 스탠드, 책상 스탠드와 지구본 위 벽등(주인공 조명), 독서 스탠드, 북쪽 창 달빛, 채움광
    (bxa, bxb), (bya, byb) = BEDROOM['x'], BEDROOM['y']
    (ex0, _), (ey0, ey1) = BED['x'], BED['y']
    (dx0, _), (_, dy1) = DESK['x'], DESK['y']
    gx, gy = GLOBE['base'][:2]
    center = (gx, gy, GLOBE['base'][2] + GLOBE['center_up'])
    k.light('LGT_Bed_Lamp_N', 'POINT', (ex0 + 0.25, ey1 + 0.3, 0.9), c, warm, 35.0, radius=0.08)
    k.light('LGT_Bed_Lamp_S', 'POINT', (ex0 + 0.25, ey0 - 0.3, 0.9), c, warm, 25.0, radius=0.08)
    k.light('LGT_Bed_DeskLamp', 'POINT', (dx0 + 0.3, dy1 - 0.27, DESK['top'] + 0.3), c, warm, 30.0, radius=0.03)
    spot = k.light('LGT_Bed_GlobeSconce', 'SPOT', (gx, byb - 0.22, 2.0), c, warm, 90.0, target=center, radius=0.05)
    spot.data.spot_size, spot.data.spot_blend = math.radians(70), 0.5
    k.light('LGT_Bed_ReadLamp', 'POINT', (3.15, 11.2, 1.58), c, warm, 40.0, radius=0.12)
    bwx = sum(BED_WINDOW['x']) / 2
    k.light('LGT_Bed_Window', 'AREA', (bwx, byb + 0.8, 1.7), c, (0.5, 0.62, 1.0), 90.0, target=(bwx, byb - 3.0, 0.8), size=1.4)
    k.light('LGT_Bed_Fill', 'AREA', ((bxa + bxb) / 2, (bya + byb) / 2, 3.0), c, (1.0, 0.8, 0.6), 60.0,
            target=((bxa + bxb) / 2, (bya + byb) / 2, 0.0), size=6.0)
    for name in ('LGT_Porch', 'LGT_Fill', 'LGT_Moon_Fill', 'LGT_TableLamp', 'LGT_Bed_Window', 'LGT_Bed_Fill',
                 'LGT_Bed_Lamp_S', 'LGT_Bed_ReadLamp'):
        bpy.data.objects[name].data.use_shadow = False


def _haze(c):
    (x0, x1), (y0, y1), h = HALL['x'], HALL['y'], HALL['h']
    k.box('FX_RoomHaze', (x0, y0, 0.0), (x1, y1, h), c, _volume_material())
    (bxa, bxb), (bya, byb) = BEDROOM['x'], BEDROOM['y']
    k.box('FX_BedroomHaze', (bxa, bya, 0.0), (bxb, byb, h), c, _volume_material())


def _pilot_path():
    center = Vector(GLOBE['base']) + Vector((0, 0, GLOBE['center_up']))
    # 책상 지구본에서 S03 카메라 쪽(남남서, 위) — 침대 위를 지나 책상 앞까지 곧게 다가간다.
    d = Vector((-0.45, -1.0, 0.25)).normalized()
    return center, center + d * 4.5, center + d * 1.0


def _cameras(c, earth, head):
    gx, gy = GLOBE['base'][:2]
    gz = GLOBE['base'][2] + GLOBE['center_up']
    k.camera('CAM_S01_Enter', S01_CAM, (-0.8, -0.9, 1.0), 18, c)
    k.camera('CAM_S02_Spin', S02_CAM, (gx - 0.05, gy - 0.05, gz), 30, c, focus=earth, fstop=2.8)
    # S03 — 캐릭터(약 3m)와 뒤쪽 책상 지구본(약 5.8m)을 함께. 조리개를 조금 닫아 지구본이 알아보이게 둔다.
    k.camera('CAM_S03_Pack', S03_CAM, (-1.55, 9.81, 1.15), 35, c, focus=head, fstop=4.0)
    k.camera('CAM_Layout_Top', (-0.3, 2.6, 26.0), None, 50, c, ortho=19.5).rotation_euler = (0, 0, 0)
    k.camera('CAM_Layout_Overview', (0.0, -15.5, 13.0), (-0.3, -0.5, 0.3), 24, c)
    # 거실 조감에서는 두 방 사이 벽이 침실을 가린다 — 거실 위 높은 곳에서 침실을 따로 내려다본다.
    k.camera('CAM_Layout_Bedroom', (0.0, 2.6, 16.5), (0.0, 8.2, 0.3), 26, c)


def _guides(paths, labels):
    z = 0.03
    s01, s01b, s02, s03 = (POSES[s]['root'] for s in ('S01', 'S01b', 'S02', 'S03'))
    k.poly_curve('GUIDE_Path_In', [(-4.6, -2.45, z), (*s01, z), (-0.8, 0.6, z), (*s01b, z), (1.6, 5.2, z),
                                   (0.6, 8.0, z), (*s02, z), (-1.2, 9.6, z), (*s03, z)], paths, M('guide_in'), 0.03)
    k.poly_curve('GUIDE_Path_Out', [(s03[0], s03[1] + 0.15, z), (-1.2, 9.35, z), (0.8, 7.5, z), (1.75, 5.2, z),
                                    (1.75, 4.0, z), (-0.6, 0.4, z), (-3.2, -2.6, z), (-4.6, -2.6, z)],
                 paths, M('guide_out'), 0.03)
    zl = 4.2
    _, start, end = _pilot_path()
    k.poly_curve('GUIDE_Cam_Pilot', [(start.x, start.y, zl), (end.x, end.y, zl)], labels, M('guide_cam'), 0.035)
    cx, cy = COFFEE
    for name, text, (x, y), size in (
            ('Door', 'FRONT DOOR  in S01 / out S04 (sound)', (-2.1, -3.45), 0.17),
            ('Hall', 'LIVING ROOM (pass through)', (-0.6, -0.4), 0.2),
            ('Board', 'TRAVEL BOARD', (-0.5, 4.15), 0.15), ('Desk', 'DESK', (-0.6, 3.35), 0.13),
            ('Lounge', 'LOUNGE', (cx, cy + 1.1), 0.15), ('Sofa', 'SOFA', (3.0, cy), 0.13),
            ('Stair', 'STAIRS -> 2F', (2.95, 1.0), 0.13), ('StairFree', '(stair entrance kept clear)', (2.4, -1.8), 0.1),
            ('Window', 'BLINDS WINDOW (moon)', (1.8, HALL['y'][0] + 0.3), 0.13),
            ('Shelves', 'BOOKCASE', (-2.85, 3.8), 0.12), ('Side', 'SIDEBOARD', (-3.0, 1.4), 0.12),
            ('BedDoor', 'BEDROOM DOOR (open)', (1.6, 3.85), 0.13), ('Bedroom', 'BEDROOM', (0.3, 6.0), 0.24),
            ('Bed', 'BED + BACKPACK (S03)', (-2.4, 7.5), 0.13), ('Globe', 'DESK GLOBE (S02)', (-0.5, 10.55), 0.12),
            ('BedWindow', 'WINDOW', (-2.3, 11.3), 0.11), ('Reading', 'READING CORNER', (2.5, 9.8), 0.1),
            ('Wardrobe', 'WARDROBE', (3.2, 6.4), 0.1), ('Dresser', 'DRESSER', (2.85, 4.95), 0.1)):
        k.label(f'TXT_{name}', text, (x, y, zl), labels, M('label'), size=size)
    for name, (x, y), dx in (('S01 cam', S01_CAM[:2], -0.6), ('S02 cam', S02_CAM[:2], 0.5),
                             ('S03 cam', S03_CAM[:2], 0.6), ('P0 start', start.xy, 0.6), ('P0 end', end.xy, 0.55)):
        k.label('TXT_Cam_' + name.replace(' ', '_'), '[' + name + ']', (x + dx, y, zl), labels, M('guide_cam'), size=0.11)


# ---------------------------------------------------------------- 빌드·파일럿·저장

GROUPS = ('SET_Room', 'SET_WallSouth', 'SET_Ceiling', 'SET_Outside', 'SET_Stair', 'SET_Study', 'SET_Lounge',
          'SET_Entry', 'SET_Bedroom', 'PROP_Globe', 'PROP_Backpack', 'CHR_Standin', 'CAM_Shots', 'LGT_Night',
          'FX_Volume', 'GUIDE_Layout', 'GUIDE_Labels')


def build(force=False):
    sc = _scene()
    _drop_old()
    top = bpy.data.collections.get(TOP)
    if top is not None and len(top.all_objects) and not force:
        print('이미 집이 있어 새로 만들지 않음. 다시 만들려면 build(force=True)')
        return sc
    top = k.coll(TOP, sc.collection)
    k.purge(top)
    g = {name: k.coll(name, top) for name in GROUPS}
    _shell(g['SET_Room'], g['SET_WallSouth'], g['SET_Outside'])
    _ceiling(g['SET_Ceiling'])
    _stairs(g['SET_Stair'])
    _study(g['SET_Study'])
    _lounge(g['SET_Lounge'])
    _entry(g['SET_Entry'])
    _bedroom(g['SET_Bedroom'], g['SET_Ceiling'], g['SET_Outside'])
    earth = _desk_globe(g['PROP_Globe'])
    _backpack(g['PROP_Backpack'])
    _standin(g['CHR_Standin'])
    pose('S03')
    _lights(g['LGT_Night'])
    _haze(g['FX_Volume'])
    _cameras(g['CAM_Shots'], earth, bpy.data.objects['CHR_Head'])
    _guides(g['GUIDE_Layout'], g['GUIDE_Labels'])
    sc.camera = bpy.data.objects['CAM_S01_Enter']
    sc['ff_intro_previs'] = f'{VERSION} 여행가의 집(거실 7×11 + 침실 7×7, 밤) — DetectiveOffice 느낌만 참고'
    print(f'집 생성({VERSION}): 객체 {len(top.all_objects)}개')
    return sc


def pilot(force=False):
    """침실 책상 지구본이 도는 동안 카메라가 5초간 4.5m → 1.0m 직선으로 다가간다(S04 접근 구간 시험)."""
    sc = bpy.data.scenes[SCENE]
    old = bpy.data.objects.get('CAM_P0_Pilot')
    if old is not None and not force:
        print('파일럿 카메라가 이미 있음. 다시 만들려면 pilot(force=True)')
        return
    if old is not None:
        bpy.data.objects.remove(old, do_unlink=True)
    earth = bpy.data.objects['GLB_Earth']
    center, start, end = _pilot_path()
    f0, f1 = PILOT_FRAMES
    cam = k.camera('CAM_P0_Pilot', start, center, 35, bpy.data.collections['CAM_Shots'], focus=earth, fstop=2.8)
    cam.keyframe_insert('location', frame=f0)
    cam.location = end
    cam.keyframe_insert('location', frame=f1)
    cam.location = start
    earth.animation_data_clear()
    earth.rotation_euler.z = 0.0
    earth.keyframe_insert('rotation_euler', index=2, frame=f0)
    earth.rotation_euler.z = math.radians(GLOBE_SPIN_DEG_PER_SEC * (f1 - f0 + 1) / FPS)
    earth.keyframe_insert('rotation_euler', index=2, frame=f1 + 1)
    earth.rotation_euler.z = 0.0
    for fc in k.fcurves(earth):
        for kp in fc.keyframe_points:
            kp.interpolation = 'LINEAR'
        fc.extrapolation = 'LINEAR'
    # 마커에 카메라를 묶지 않는다 — 묶으면 다른 카메라 렌더가 파일럿 카메라로 바뀐다.
    sc.timeline_markers.clear()
    sc.timeline_markers.new('P0_start', frame=f0)
    sc.timeline_markers.new('P0_end', frame=f1)
    sc.frame_set(f0)
    print(f'파일럿: {tuple(round(v, 3) for v in start)} → {tuple(round(v, 3) for v in end)}')


def save():
    target = str(BLEND)
    if os.path.exists(target) and os.path.normcase(bpy.data.filepath) != os.path.normcase(target):
        raise RuntimeError(f'다른 파일이 이미 있음: {target}')
    os.makedirs(BLEND.parent, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=target, relative_remap=True)
    print('저장:', target)


def show(cam_name=None, guides=False):
    """켜진 Blender 뷰포트를 카메라 시점·텍스처 색으로 맞춘다."""
    sc = bpy.data.scenes[SCENE]
    if cam_name:
        sc.camera = bpy.data.objects[cam_name]
    lcs = sc.view_layers[0].layer_collection.children[TOP].children
    for name in ('GUIDE_Layout', 'GUIDE_Labels'):
        lcs[name].hide_viewport = not guides
    lcs['FX_Volume'].hide_viewport = True
    for win in bpy.context.window_manager.windows:
        for area in win.screen.areas:
            if area.type != 'VIEW_3D':
                continue
            sp = area.spaces.active
            sp.shading.type = 'SOLID'
            sp.shading.color_type = 'TEXTURE'
            sp.overlay.show_overlays = False
            if cam_name:
                sp.region_3d.view_perspective = 'CAMERA'
                region = next(r for r in area.regions if r.type == 'WINDOW')
                with bpy.context.temp_override(window=win, area=area, region=region):
                    bpy.ops.view3d.view_center_camera()
