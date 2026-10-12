"""인트로 영상 v2 — 별빛 우주의 실사 지구에 세계 곳곳 축제 폭죽이 터지고, 터진 자리부터 우리 스타일라이즈드 지구로 물든다.

흐름(8초 30fps, `assumption`):
  0.0  검은 화면에서 별빛 우주와 실사 지구(한국·일본 쪽)가 드러난다. 지구는 동쪽으로 천천히 돈다.
  1.6~5.0  도시마다 폭죽이 올라가 터진다(부산부터). 빛이 닿은 둥근 자리부터 스타일라이즈드 지구가 드러난다.
  5.3~6.3  남은 곳이 한꺼번에 물들고, 지구 둘레로 마지막 폭죽이 터진다.
  6.1~8.0  변신 빛이 퍼지며 우주가 밝은 하늘로 바뀌고 흰 빛 정점에서 끝난다 → UE 실시간 지구가 이어받는다.

두 지구는 같은 물체 좌표를 쓴다. 스타일라이즈드 원본(FF_BrightGlobe_P05_world_v001.blend)은
경도 0° = +X, 북극 = +Z 인 표준 위경도 배치라서(UV 맞춤으로 측정), 실사 지구 텍스처도 물체 좌표에서 같은 규칙으로 입힌다.
드러남 마스크는 장면 속성 `rv0..` · `rvAll` 을 셰이더 Attribute(View Layer)로 읽는다.

blender -b --factory-startup --python intro_space_fireworks.py -- build check render
  build : 씬을 만들고 .blend 와 소리용 이벤트 표(events.json)를 저장한다
  check : 키 시점 저해상도 스틸 모음
  render: 1920×1080 30fps MP4(소리 없음)
"""
from pathlib import Path
import json
import math
import os
import random
import sys

import bpy
import bmesh
from mathutils import Vector

sys.path.insert(0, str(Path(__file__).resolve().parent))
import previs_kit as k  # noqa: E402

VERSION = 'v001'
ROOT = Path(__file__).resolve().parents[4]
ART = ROOT / 'SourceArt/UI/Intro/SpaceFireworks_v1'
BLEND = ART / f'Blender/FF_Intro_SpaceFireworks_{VERSION}.blend'
OUT = ART / 'Previs' / VERSION
EARTH = ROOT / 'SourceArt/ThirdParty/SolarSystemScope/PlanetTextures/Earth'
STYLIZED_BLEND = ROOT / 'SourceArt/UI/Atlas/BrightGlobe_v1/Blender/FF_BrightGlobe_P05_world_v001.blend'
SCENE = f'FF_Intro_SpaceFireworks_{VERSION}'
FPS = 30
END_S = 8.0
R = 0.5                          # 지구 반지름. 스타일라이즈드 원본 5000 → 0.5
STYLIZED_SCALE = R / 5000.0
RIM_SCALE = 1.06                 # 바깥으로 번지는 자리. 밝은 곳은 지구 가장자리 바로 바깥뿐이다                # 대기 두께. 실제 지구 대기처럼 얇게
TILT_DEG = 18.0                  # 북극을 카메라 쪽으로 기울여 북반구(축제 도시 대부분)를 더 보여 준다
VIEW_LON = (128.0, 18.0)         # 시작·끝에 화면 가운데 오는 경도. 한국 → 유럽·아프리카
SCREEN_FRACTION = (0.47, 0.56)   # 지구 지름 / 화면 높이. 끝 값은 UE 인트로 구도(IntroGlobeScreenHeightFraction)와 같다
LENS = 35.0

# (이름, 위도, 경도, 터지는 초, 색, 드러남 최대 반경(도))
BURSTS = [
    ('Busan', 35.1, 129.0, 1.65, 'gold', 34),
    ('Tokyo', 35.7, 139.7, 2.05, 'pink', 30),
    ('Beijing', 39.9, 116.4, 2.35, 'red', 30),
    ('Bangkok', 13.8, 100.5, 2.65, 'cyan', 32),
    ('Sydney', -33.9, 151.2, 2.95, 'green', 30),
    ('Mathura', 27.5, 77.7, 3.25, 'pink', 32),
    ('Kolkata', 22.6, 88.4, 3.5, 'gold', 26),
    ('Istanbul', 41.0, 29.0, 3.85, 'cyan', 30),
    ('Munich', 48.1, 11.6, 4.2, 'gold', 34),
    ('Valencia', 39.5, -0.4, 4.5, 'red', 30),
    ('Nairobi', -1.3, 36.8, 4.75, 'green', 34),
]
FINALE_T = 6.05                   # 지구 둘레 마지막 폭죽
FINALE_COUNT = 7
T = dict(fade_in=0.7, all0=5.3, all1=6.3, sky0=6.1, sky1=7.4, flash0=7.0, flash1=7.97, push1=7.2)
COLORS = {
    'gold': (1.0, 0.72, 0.25), 'pink': (1.0, 0.38, 0.62), 'red': (1.0, 0.32, 0.22),
    'cyan': (0.35, 0.85, 1.0), 'green': (0.45, 1.0, 0.5), 'white': (1.0, 0.95, 0.85),
}
SKY = (0.82, 0.92, 1.0)
SUN_FROM = (-4.0, -6.0, 3.0)     # 해가 있는 쪽(앞 왼쪽 위). 대기 테두리도 이쪽만 밝다
SKY_STRENGTH = 3.2              # AgX 에서 회색으로 눌리지 않게 밝게


# ---------------------------------------------------------------- 도우미

def smooth(a, b, t0, t1, t):
    if t <= t0:
        return a
    if t >= t1:
        return b
    x = (t - t0) / (t1 - t0)
    return a + (b - a) * x * x * (3 - 2 * x)


def ease_out(a, b, t0, t1, t):
    if t <= t0:
        return a
    if t >= t1:
        return b
    x = (t - t0) / (t1 - t0)
    return a + (b - a) * (1 - (1 - x) ** 3)


def lonlat(lat, lon):
    la, lo = math.radians(lat), math.radians(lon)
    return Vector((math.cos(la) * math.cos(lo), math.cos(la) * math.sin(lo), math.sin(la)))


def spin_deg(t):
    """화면 가운데 경도 L 이 보이도록 하는 자전각. 카메라는 -Y 에서 +Y 를 본다."""
    def phi(lon):
        lo = math.radians(lon)
        return math.degrees(math.atan2(-math.cos(lo), -math.sin(lo)))
    a0, a1 = phi(VIEW_LON[0]), phi(VIEW_LON[1])
    while a1 < a0:          # 동쪽 자전 = 각이 커지는 쪽
        a1 += 360.0
    return a0 + (a1 - a0) * (t / END_S)


def camera_distance(fraction):
    half_v = math.atan((36.0 * 9 / 16 / 2) / LENS)
    return R / math.sin(math.atan(fraction * math.tan(half_v)))


# ---------------------------------------------------------------- 셰이더

def _n(nt, kind, loc=(0, 0), **inputs):
    node = nt.nodes.new(kind)
    node.location = loc
    for key, value in inputs.items():
        node.inputs[key].default_value = value
    return node


def _math(nt, op, a=None, b=None, loc=(0, 0), clamp=False):
    node = nt.nodes.new('ShaderNodeMath')
    node.operation = op
    node.use_clamp = clamp
    node.location = loc
    for i, value in enumerate((a, b)):
        if value is None:
            continue
        if isinstance(value, (int, float)):
            node.inputs[i].default_value = value
        else:
            nt.links.new(value, node.inputs[i])
    return node.outputs[0]


def _mix_rgba(nt):
    node = nt.nodes.new('ShaderNodeMix')
    node.data_type = 'RGBA'
    return node


def _attr(nt, name, loc=(0, 0)):
    node = nt.nodes.new('ShaderNodeAttribute')
    node.attribute_type = 'VIEW_LAYER'
    node.attribute_name = name
    node.location = loc
    return node.outputs['Fac']


def reveal_group():
    """물체 좌표 방향으로 드러남 마스크(0~1)와 경계 빛(0~1)을 낸다. 모든 지구 재질이 같은 그룹을 쓴다."""
    g = bpy.data.node_groups.get('NG_RevealMask')
    if g:
        return g
    g = bpy.data.node_groups.new('NG_RevealMask', 'ShaderNodeTree')
    g.interface.new_socket('Mask', in_out='OUTPUT', socket_type='NodeSocketFloat')
    g.interface.new_socket('Glow', in_out='OUTPUT', socket_type='NodeSocketFloat')
    out = g.nodes.new('NodeGroupOutput')
    out.location = (1800, 0)
    tc = g.nodes.new('ShaderNodeTexCoord')
    norm = g.nodes.new('ShaderNodeVectorMath')
    norm.operation = 'NORMALIZE'
    g.links.new(tc.outputs['Object'], norm.inputs[0])
    noise = _n(g, 'ShaderNodeTexNoise', (200, -300), Scale=5.0, Detail=3.0)
    g.links.new(norm.outputs[0], noise.inputs['Vector'])
    wobble = _math(g, 'MULTIPLY_ADD', noise.outputs['Fac'], 0.16, (400, -300))
    g.nodes[-1].inputs[2].default_value = -0.08              # (노이즈-0.5)*0.16 → 경계가 둥글지 않고 일렁인다

    mask, glow = None, None
    for i, (_, lat, lon, *_rest) in enumerate(BURSTS + finale_bursts()):
        y = -400 - i * 260
        d = g.nodes.new('ShaderNodeVectorMath')
        d.operation = 'DOT_PRODUCT'
        d.location = (300, y)
        g.links.new(norm.outputs[0], d.inputs[0])
        d.inputs[1].default_value = lonlat(lat, lon)
        ang = _math(g, 'ARCCOSINE', _math(g, 'MINIMUM', d.outputs['Value'], 1.0, (450, y)), None, (600, y))
        ang = _math(g, 'ADD', ang, wobble, (750, y))
        theta = _attr(g, f'rv{i}', (600, y - 120))
        # m = clamp((theta - ang)/0.05 + 0.5)
        m = _math(g, 'MULTIPLY_ADD', _math(g, 'SUBTRACT', theta, ang, (900, y)), 20.0, (1050, y), clamp=True)
        g.nodes[-1].inputs[2].default_value = 0.5
        # 경계 빛: 1 - |ang - theta| / 0.07, 시작 전(theta=0)은 끈다
        band = _math(g, 'SUBTRACT', 1.0, _math(g, 'DIVIDE', _math(g, 'ABSOLUTE', _math(g, 'SUBTRACT', ang, theta, (900, y - 60)), None, (1000, y - 60)), 0.035, (1100, y - 60)), (1200, y - 60), clamp=True)
        band = _math(g, 'MULTIPLY', band, _attr(g, f'rg{i}', (1100, y - 140)), (1300, y - 60))
        mask = m if mask is None else _math(g, 'MAXIMUM', mask, m, (1400, y))
        glow = band if glow is None else _math(g, 'MAXIMUM', glow, band, (1400, y - 60))

    # 마지막 한꺼번에: 노이즈 문턱을 rvAll 이 넘는 곳부터
    noise2 = _n(g, 'ShaderNodeTexNoise', (200, 300), Scale=2.5, Detail=2.0)
    g.links.new(norm.outputs[0], noise2.inputs['Vector'])
    allv = _math(g, 'MULTIPLY_ADD', _attr(g, 'rvAll', (200, 500)), 1.3, (400, 500))
    g.nodes[-1].inputs[2].default_value = -0.15
    diff = _math(g, 'SUBTRACT', allv, noise2.outputs['Fac'], (600, 400))
    mg = _math(g, 'MULTIPLY_ADD', diff, 12.0, (800, 400), clamp=True)
    g.nodes[-1].inputs[2].default_value = 0.0
    bg = _math(g, 'SUBTRACT', 1.0, _math(g, 'DIVIDE', _math(g, 'ABSOLUTE', diff, None, (800, 300)), 0.06, (900, 300)), (1000, 300), clamp=True)
    alive = _math(g, 'MULTIPLY', _attr(g, 'rvAllGlow', (900, 200)), bg, (1100, 300))
    mask = _math(g, 'MAXIMUM', mask, mg, (1600, 0))
    glow = _math(g, 'MAXIMUM', glow, alive, (1600, -60))
    g.links.new(mask, out.inputs['Mask'])
    g.links.new(glow, out.inputs['Glow'])
    return g


def equirect_uv(nt, loc=(-900, 0)):
    """물체 좌표 → 위경도 UV. 이음매(경도 -60°)는 영상 내내 지구 뒤쪽에 있다."""
    tc = nt.nodes.new('ShaderNodeTexCoord')
    tc.location = loc
    norm = nt.nodes.new('ShaderNodeVectorMath')
    norm.operation = 'NORMALIZE'
    nt.links.new(tc.outputs['Object'], norm.inputs[0])
    sep = nt.nodes.new('ShaderNodeSeparateXYZ')
    nt.links.new(norm.outputs[0], sep.inputs[0])
    lon = _math(nt, 'ARCTAN2', sep.outputs['Y'], sep.outputs['X'])
    wrap = _math(nt, 'MULTIPLY', _math(nt, 'LESS_THAN', lon, math.radians(-60.0)), 2 * math.pi)
    lon = _math(nt, 'ADD', lon, wrap)
    u = _math(nt, 'MULTIPLY_ADD', lon, 1 / (2 * math.pi))
    nt.nodes[-1].inputs[2].default_value = 0.5
    v = _math(nt, 'MULTIPLY_ADD', _math(nt, 'ARCSINE', sep.outputs['Z']), 1 / math.pi)
    nt.nodes[-1].inputs[2].default_value = 0.5
    comb = nt.nodes.new('ShaderNodeCombineXYZ')
    nt.links.new(u, comb.inputs['X'])
    nt.links.new(v, comb.inputs['Y'])
    return comb.outputs[0]


def real_earth_material():
    m = bpy.data.materials.new('M_Earth_Real_Reveal')
    nt = m.node_tree
    nt.nodes.clear()
    out = _n(nt, 'ShaderNodeOutputMaterial', (900, 0))
    uv = equirect_uv(nt)
    day = nt.nodes.new('ShaderNodeTexImage')
    day.image = bpy.data.images.load(str(EARTH / '2k_earth_daymap.jpg'), check_existing=True)
    day.location = (-300, 200)
    nt.links.new(uv, day.inputs['Vector'])
    clouds = nt.nodes.new('ShaderNodeTexImage')
    clouds.image = bpy.data.images.load(str(EARTH / '2k_earth_clouds.jpg'), check_existing=True)
    clouds.image.colorspace_settings.name = 'Non-Color'
    clouds.location = (-300, -100)
    nt.links.new(uv, clouds.inputs['Vector'])
    cfac = _math(nt, 'MULTIPLY', _math(nt, 'POWER', clouds.outputs['Color'], 1.6), 0.65)
    mix = _mix_rgba(nt)
    nt.links.new(cfac, k.sock(mix, 'Factor_Float'))
    nt.links.new(day.outputs['Color'], k.sock(mix, 'A_Color'))
    k.sock(mix, 'B_Color').default_value = (1, 1, 1, 1)
    bsdf = nt.nodes.new('ShaderNodeBsdfPrincipled')
    bsdf.location = (300, 100)
    nt.links.new(k.sock(mix, 'Result_Color', outputs=True), k.sock(bsdf, 'Base Color'))
    k.sock(bsdf, 'Roughness').default_value = 0.6
    grp = nt.nodes.new('ShaderNodeGroup')
    grp.node_tree = reveal_group()
    grp.location = (300, -300)
    # 드러난 곳은 투명, 경계는 축제 빛으로 달아오른다
    glow = nt.nodes.new('ShaderNodeEmission')
    glow.inputs['Color'].default_value = (1.0, 0.55, 0.18, 1)
    glow.location = (500, -350)
    nt.links.new(_math(nt, 'MULTIPLY', grp.outputs['Glow'], 3.0), glow.inputs['Strength'])
    add = nt.nodes.new('ShaderNodeAddShader')
    nt.links.new(bsdf.outputs[0], add.inputs[0])
    nt.links.new(glow.outputs[0], add.inputs[1])
    tr = nt.nodes.new('ShaderNodeBsdfTransparent')
    cut = nt.nodes.new('ShaderNodeMixShader')
    cut.location = (700, 0)
    nt.links.new(grp.outputs['Mask'], cut.inputs[0])
    nt.links.new(add.outputs[0], cut.inputs[1])
    nt.links.new(tr.outputs[0], cut.inputs[2])
    nt.links.new(cut.outputs[0], out.inputs['Surface'])
    m.surface_render_method = 'DITHERED'
    m.use_transparent_shadow = True
    return m


def wrap_with_reveal(mat):
    """스타일라이즈드 재질: 드러나지 않은 곳은 투명. 경계에도 같은 빛을 얹는다."""
    nt = mat.node_tree
    out = next(n for n in nt.nodes if n.type == 'OUTPUT_MATERIAL' and n.is_active_output)
    link = out.inputs['Surface'].links[0]
    shader = link.from_socket
    nt.links.remove(link)
    grp = nt.nodes.new('ShaderNodeGroup')
    grp.node_tree = reveal_group()
    glow = nt.nodes.new('ShaderNodeEmission')
    glow.inputs['Color'].default_value = (1.0, 0.55, 0.18, 1)
    nt.links.new(_math(nt, 'MULTIPLY', grp.outputs['Glow'], 2.5), glow.inputs['Strength'])
    add = nt.nodes.new('ShaderNodeAddShader')
    nt.links.new(shader, add.inputs[0])
    nt.links.new(glow.outputs[0], add.inputs[1])
    tr = nt.nodes.new('ShaderNodeBsdfTransparent')
    cut = nt.nodes.new('ShaderNodeMixShader')
    nt.links.new(grp.outputs['Mask'], cut.inputs[0])
    nt.links.new(tr.outputs[0], cut.inputs[1])
    nt.links.new(add.outputs[0], cut.inputs[2])
    nt.links.new(cut.outputs[0], out.inputs['Surface'])
    mat.surface_render_method = 'DITHERED'
    mat.use_transparent_shadow = True


def rim_material():
    """얇은 대기. 지구 가장자리 바로 바깥이 가장 밝고 바깥으로 갈수록 0 으로 사라진다(딱딱한 바깥 테두리 없음).

    껍질 반지름 Rs = RIM_SCALE·R 에서 지구 가장자리를 스치는 시선의 Facing(1-|N·V|) 은 1 - cos(asin(1/RIM_SCALE)) 이다.
    그 값에서 최대, 껍질 끝(Facing 1)에서 0, 지구 앞면(작은 Facing)은 아주 옅게.
    """
    m = bpy.data.materials.new('M_Earth_Rim')
    nt = m.node_tree
    nt.nodes.clear()
    out = _n(nt, 'ShaderNodeOutputMaterial', (900, 0))
    lw = nt.nodes.new('ShaderNodeLayerWeight')
    lw.inputs['Blend'].default_value = 0.5            # Facing 을 치우침 없이 그대로
    limb = 1.0 - math.cos(math.asin(1.0 / RIM_SCALE))
    ramp = nt.nodes.new('ShaderNodeValToRGB')
    ramp.color_ramp.interpolation = 'EASE'
    els = ramp.color_ramp.elements
    # 껍질 끝(Facing→1)으로 갈수록 거리 대비 Facing 이 빨리 변하므로 바깥 정지점은 1 쪽으로 몰아 둔다 → 반지름 약 6% 에 걸쳐 옅어짐
    stops = [(0.0, 0.0), (limb * 0.6, 0.01), (limb * 0.92, 0.22), (limb, 0.8), (limb + (1 - limb) * 0.12, 0.42),
             (limb + (1 - limb) * 0.35, 0.12), (limb + (1 - limb) * 0.7, 0.02), (1.0, 0.0)]
    els[0].position, els[0].color = stops[0][0], (stops[0][1],) * 3 + (1,)
    els[1].position, els[1].color = stops[-1][0], (stops[-1][1],) * 3 + (1,)
    for pos, val in stops[1:-1]:
        e = els.new(pos)
        e.color = (val, val, val, 1)
    nt.links.new(lw.outputs['Facing'], ramp.inputs['Fac'])
    # 햇빛 받는 쪽만 밝게: 월드 법선 · 해 방향. 어두운 쪽 테두리는 거의 사라진다
    geo = nt.nodes.new('ShaderNodeNewGeometry')
    sun_dot = nt.nodes.new('ShaderNodeVectorMath')
    sun_dot.operation = 'DOT_PRODUCT'
    nt.links.new(geo.outputs['Normal'], sun_dot.inputs[0])
    sun_dot.inputs[1].default_value = Vector(SUN_FROM).normalized()
    lit = _math(nt, 'MULTIPLY_ADD', sun_dot.outputs['Value'], 0.8, clamp=True)
    nt.nodes[-1].inputs[2].default_value = 0.35
    lit = _math(nt, 'MULTIPLY_ADD', lit, 0.85)
    nt.nodes[-1].inputs[2].default_value = 0.15
    fac = _math(nt, 'MULTIPLY', _math(nt, 'MULTIPLY', ramp.outputs['Color'], lit), 1.0)
    rim_node = nt.nodes[-1]
    rim_node.name = 'RimAmount'
    em = nt.nodes.new('ShaderNodeEmission')
    em.inputs['Color'].default_value = (0.22, 0.48, 1.0, 1)
    nt.links.new(_math(nt, 'MULTIPLY', fac, 1.3), em.inputs['Strength'])
    tr = nt.nodes.new('ShaderNodeBsdfTransparent')
    mix = nt.nodes.new('ShaderNodeMixShader')
    nt.links.new(_math(nt, 'MULTIPLY', fac, 0.75), mix.inputs[0])
    nt.links.new(tr.outputs[0], mix.inputs[1])
    nt.links.new(em.outputs[0], mix.inputs[2])
    nt.links.new(mix.outputs[0], out.inputs['Surface'])
    m.surface_render_method = 'BLENDED'
    m.use_backface_culling = True
    return m


def emission_material(name, rgb, strength):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    nt = m.node_tree
    nt.nodes.clear()
    out = nt.nodes.new('ShaderNodeOutputMaterial')
    em = nt.nodes.new('ShaderNodeEmission')
    em.inputs['Color'].default_value = (*rgb, 1)
    em.inputs['Strength'].default_value = strength
    nt.links.new(em.outputs[0], out.inputs['Surface'])
    return m


# ---------------------------------------------------------------- 세계(별빛 → 밝은 하늘)

def build_world(sc):
    w = bpy.data.worlds.new('W_Space')
    sc.world = w
    nt = w.node_tree
    nt.nodes.clear()
    out = _n(nt, 'ShaderNodeOutputWorld', (1200, 0))
    tc = nt.nodes.new('ShaderNodeTexCoord')
    stars = None
    for i, (scale, thr, gain) in enumerate(((260.0, 0.09, 6.0), (120.0, 0.06, 12.0), (520.0, 0.11, 3.0))):
        vor = _n(nt, 'ShaderNodeTexVoronoi', (0, i * -250), Scale=scale)
        nt.links.new(tc.outputs['Generated'], vor.inputs['Vector'])
        dot = _math(nt, 'SUBTRACT', 1.0, _math(nt, 'DIVIDE', vor.outputs['Distance'], thr), clamp=True)
        dot = _math(nt, 'POWER', dot, 3.0)
        # 별마다 밝기를 다르게(Voronoi 색의 R)
        sep = nt.nodes.new('ShaderNodeSeparateColor')
        nt.links.new(vor.outputs['Color'], sep.inputs[0])
        lit = _math(nt, 'MULTIPLY', dot, _math(nt, 'MULTIPLY', _math(nt, 'POWER', sep.outputs['Red'], 4.0), gain))
        stars = lit if stars is None else _math(nt, 'ADD', stars, lit)
    # 옅은 성운
    neb = _n(nt, 'ShaderNodeTexNoise', (0, 400), Scale=1.6, Detail=6.0)
    nt.links.new(tc.outputs['Generated'], neb.inputs['Vector'])
    neb_amt = _math(nt, 'MULTIPLY', _math(nt, 'POWER', neb.outputs['Fac'], 4.0), 0.06)
    space = _mix_rgba(nt)
    k.sock(space, 'A_Color').default_value = (0.002, 0.003, 0.008, 1)
    k.sock(space, 'B_Color').default_value = (0.25, 0.12, 0.45, 1)
    nt.links.new(neb_amt, k.sock(space, 'Factor_Float'))
    star_rgb = _mix_rgba(nt)
    star_rgb.blend_type = 'ADD'
    k.sock(star_rgb, 'Factor_Float').default_value = 1.0
    nt.links.new(k.sock(space, 'Result_Color', outputs=True), k.sock(star_rgb, 'A_Color'))
    comb = nt.nodes.new('ShaderNodeCombineColor')
    for ch in ('Red', 'Green', 'Blue'):
        nt.links.new(stars, comb.inputs[ch])
    nt.links.new(comb.outputs[0], k.sock(star_rgb, 'B_Color'))
    to_sky = _mix_rgba(nt)
    to_sky.name = 'SkyMix'
    k.sock(to_sky, 'Factor_Float').default_value = 0.0
    nt.links.new(k.sock(star_rgb, 'Result_Color', outputs=True), k.sock(to_sky, 'A_Color'))
    k.sock(to_sky, 'B_Color').default_value = (*SKY, 1)
    bgn = nt.nodes.new('ShaderNodeBackground')
    bgn.name = 'WorldBG'
    nt.links.new(k.sock(to_sky, 'Result_Color', outputs=True), bgn.inputs['Color'])
    bgn.inputs['Strength'].default_value = 1.0
    nt.links.new(bgn.outputs[0], out.inputs['Surface'])
    return w


# ---------------------------------------------------------------- 씬 구성

def finale_bursts():
    """마지막 폭죽: 끝 무렵 화면에 보이는 반구 가장자리 쪽에 고르게."""
    lon_c = VIEW_LON[0] + (VIEW_LON[1] - VIEW_LON[0]) * (FINALE_T / END_S)
    out = []
    rng = random.Random(7)
    names = ('gold', 'pink', 'cyan', 'green', 'red', 'white', 'gold')
    for i in range(FINALE_COUNT):
        a = 2 * math.pi * i / FINALE_COUNT + 0.3
        lat = 18.0 + 48.0 * math.sin(a)
        lon = lon_c + 55.0 * math.cos(a) + rng.uniform(-6, 6)
        out.append((f'Finale{i}', lat, lon, FINALE_T + 0.07 * i, names[i % len(names)], 0))
    return out


def append_stylized(parent, c):
    names = [f'GEO-SM_BrightWorld_0{i}_{part}' for i in range(8) for part in ('Top', 'Coast')]
    names.append('GEO-BrightWorld_OceanPreview')
    with bpy.data.libraries.load(str(STYLIZED_BLEND), link=False) as (src, dst):
        dst.objects = [n for n in names if n in src.objects]
    wrapped = set()
    for ob in dst.objects:
        if ob is None:
            continue
        c.objects.link(ob)
        ob.parent = parent
        ob.location = (0, 0, 0)
        ob.rotation_euler = (0, 0, 0)
        ob.scale = (STYLIZED_SCALE,) * 3
        for slot in ob.material_slots:
            if slot.material and slot.material.name not in wrapped:
                wrap_with_reveal(slot.material)
                wrapped.add(slot.material.name)
    print('스타일라이즈드 지구:', len(dst.objects), '개 메시, 재질', sorted(wrapped))


def build_globe(top):
    c = k.coll('GLB_Globe', top)
    root = k.empty('GLB_Root', c, size=0.4)
    tilt = k.empty('GLB_Tilt', c, parent=root, rot=(math.radians(TILT_DEG), 0, 0), size=0.4)
    spin = k.empty('GLB_Spin', c, parent=tilt, size=0.4, kind='SINGLE_ARROW')
    k.sphere('GLB_RealEarth', R * 1.002, (0, 0, 0), c, real_earth_material(), parent=spin, u=160, v=80)
    rim = k.sphere('GLB_Rim', R * RIM_SCALE, (0, 0, 0), c, rim_material(), parent=root, u=128, v=64)
    rim.visible_shadow = False
    append_stylized(spin, k.coll('GLB_Stylized', c))
    return root, spin


def build_fireworks(top, spin):
    """도시마다 올라가는 불씨 하나와 터지는 불꽃 입자들. 지구와 함께 돌도록 자전 물체의 자식이다."""
    c = k.coll('FX_Fireworks', top)
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=1, radius=1.0)
    spark_mesh = bpy.data.meshes.new('M_Spark')
    bm.to_mesh(spark_mesh)
    bm.free()
    spark_mesh.materials.append(emission_material('M_Spark_Default', (1, 1, 1), 1.0))
    mats = {name: emission_material(f'M_Spark_{name}', rgb, 40.0) for name, rgb in COLORS.items()}
    launch_mat = emission_material('M_Spark_Launch', (1.0, 0.9, 0.7), 60.0)
    rng = random.Random(42)
    plan = []
    for i, (name, lat, lon, t0, color, max_deg) in enumerate(BURSTS + finale_bursts()):
        surface = lonlat(lat, lon) * R
        center = surface * (1.14 if not name.startswith('Finale') else 1.24)
        size = 0.12 if not name.startswith('Finale') else 0.16
        plan.append(dict(index=i, name=name, t=t0, color=color, center=center, max_deg=max_deg))
        sparks = []
        launch = bpy.data.objects.new(f'FX_{name}_Launch', spark_mesh)
        c.objects.link(launch)
        launch.parent = spin
        launch.material_slots[0].link = 'OBJECT'
        launch.material_slots[0].material = launch_mat
        normal = surface.normalized()
        for j in range(56):
            d = Vector((rng.gauss(0, 1), rng.gauss(0, 1), rng.gauss(0, 1))).normalized()
            if d.dot(normal) < -0.3:          # 땅속으로 파고들지 않게
                d = (d - normal * (2 * d.dot(normal))).normalized()
            ob = bpy.data.objects.new(f'FX_{name}_{j:02d}', spark_mesh)
            c.objects.link(ob)
            ob.parent = spin
            ob.material_slots[0].link = 'OBJECT'
            ob.material_slots[0].material = mats['white' if j % 9 == 0 else color]
            sparks.append((ob, d, rng.uniform(0.75, 1.15), rng.uniform(0.0, 0.08)))
        light = bpy.data.lights.new(f'LGT_{name}', 'POINT')
        light.color = COLORS[color]
        light.use_shadow = False
        light.shadow_soft_size = 0.05
        lob = bpy.data.objects.new(f'LGT_{name}', light)
        c.objects.link(lob)
        lob.parent = spin
        lob.location = center * 1.05
        _key_burst(launch, surface, center, sparks, lob, t0, size)
    return plan


def _key_burst(launch, surface, center, sparks, light, t0, size):
    rise = 0.35
    f_start = max(1, int((t0 - rise) * FPS))
    f_end = min(int(END_S * FPS) + 1, int((t0 + 1.6) * FPS))
    for f in range(f_start - 1, f_end + 2):
        t = (f - 1) / FPS
        # 올라가는 불씨
        x = smooth(0.0, 1.0, t0 - rise, t0, t)
        launch.location = surface.lerp(center, x)
        s = 0.006 if (t0 - rise) <= t < t0 else 0.0
        launch.scale = (s, s, s * 2.5)
        launch.keyframe_insert('location', frame=f)
        launch.keyframe_insert('scale', frame=f)
        tau = t - t0
        for ob, d, speed, delay in sparks:
            tt = tau - delay
            if tt <= 0:
                ob.location = center
                sc = 0.0
            else:
                spread = size * speed * (1 - math.exp(-tt / 0.16))
                sag = center.normalized() * (-0.03 * tt * tt)    # 지구 쪽으로 살짝 처진다
                ob.location = center + d * spread + sag
                twinkle = 0.75 + 0.25 * math.sin(tt * 40 + speed * 13)
                sc = 0.009 * twinkle * (1 - smooth(0, 1, 0.55, 1.25, tt))
            ob.scale = (sc, sc, sc)
            ob.keyframe_insert('location', frame=f)
            ob.keyframe_insert('scale', frame=f)
        light.data.energy = 6.0 * math.exp(-max(0.0, tau) / 0.18) if tau >= 0 else 0.0
        light.data.keyframe_insert('energy', frame=f)
    # 범위 밖에서는 보이지 않게
    for ob in [launch] + [s[0] for s in sparks]:
        ob.scale = (0, 0, 0)
        ob.keyframe_insert('scale', frame=1)
        ob.keyframe_insert('scale', frame=int(END_S * FPS))


def build_lights(top):
    c = k.coll('LGT_Lights', top)
    sun = k.light('LGT_Sun', 'SUN', SUN_FROM, c, (1.0, 0.97, 0.92), 4.5, target=(0, 0, 0), angle_deg=1.0)
    fill = k.light('LGT_SkyFill', 'SUN', (3, -4, -2), c, (0.75, 0.85, 1.0), 0.0, target=(0, 0, 0), angle_deg=20.0)
    fill.data.use_shadow = False
    return sun, fill


def build_camera(top):
    c = k.coll('CAM_Camera', top)
    cam = k.camera('CAM_Intro', (0, -camera_distance(SCREEN_FRACTION[0]), 0), (0, 0, 0), LENS, c)
    cam.data.clip_start = 0.01
    covers = {}
    for name, rgb, strength, z in (('Black', (0, 0, 0), 0.0, -0.05), ('White', (1, 1, 1), 14.0, -0.06)):
        m = bpy.data.materials.new(f'M_Cover_{name}')
        nt = m.node_tree
        nt.nodes.clear()
        out = nt.nodes.new('ShaderNodeOutputMaterial')
        em = nt.nodes.new('ShaderNodeEmission')
        em.inputs['Color'].default_value = (*rgb, 1)
        em.inputs['Strength'].default_value = strength
        tr = nt.nodes.new('ShaderNodeBsdfTransparent')
        mix = nt.nodes.new('ShaderNodeMixShader')
        mix.name = 'CoverMix'
        nt.links.new(tr.outputs[0], mix.inputs[1])
        nt.links.new(em.outputs[0], mix.inputs[2])
        nt.links.new(mix.outputs[0], out.inputs['Surface'])
        m.surface_render_method = 'BLENDED'
        bm = bmesh.new()
        bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=0.2)
        me = bpy.data.meshes.new(f'FX_Cover_{name}')
        bm.to_mesh(me)
        bm.free()
        ob = k.link(bpy.data.objects.new(f'FX_Cover_{name}', me), c, m, parent=cam, loc=(0, 0, z))
        ob.visible_shadow = False
        covers[name] = mix.inputs[0]
    return cam, covers


def key_animation(sc, spin, cam, covers, sun, fill, rim_mat, plan):
    frames = range(1, int(END_S * FPS) + 1)
    sky = k.sock(sc.world.node_tree.nodes['SkyMix'], 'Factor_Float')
    bg_strength = sc.world.node_tree.nodes['WorldBG'].inputs['Strength']
    rim = rim_mat.node_tree.nodes['RimAmount'].inputs[1]
    for p in plan:
        sc[f'rv{p["index"]}'] = 0.0
        sc[f'rg{p["index"]}'] = 0.0
    sc['rvAll'] = 0.0
    sc['rvAllGlow'] = 0.0
    for f in frames:
        t = (f - 1) / FPS
        spin.rotation_euler = (0, 0, math.radians(spin_deg(t)))
        spin.keyframe_insert('rotation_euler', frame=f)
        dist = camera_distance(smooth(SCREEN_FRACTION[0], SCREEN_FRACTION[1], 0.0, T['push1'], t))
        cam.location = (0, -dist, 0)
        cam.keyframe_insert('location', frame=f)
        for p in plan:
            sc[f'rv{p["index"]}'] = ease_out(0.0, math.radians(p['max_deg']), p['t'] + 0.04, p['t'] + 1.3, t)
            sc.keyframe_insert(f'["rv{p["index"]}"]', frame=f)
            # 경계 빛: 퍼지는 동안 켜졌다가 멈추면 꺼진다
            sc[f'rg{p["index"]}'] = 0.0 if p['max_deg'] <= 0 else smooth(0.0, 1.0, p['t'], p['t'] + 0.12, t) * (1.0 - smooth(0.0, 1.0, p['t'] + 0.8, p['t'] + 1.4, t))
            sc.keyframe_insert(f'["rg{p["index"]}"]', frame=f)
        sc['rvAll'] = smooth(0.0, 1.0, T['all0'], T['all1'], t)
        sc.keyframe_insert('["rvAll"]', frame=f)
        sc['rvAllGlow'] = 1.0 if T['all0'] < t < T['all1'] + 0.2 else 0.0
        sc.keyframe_insert('["rvAllGlow"]', frame=f)
        sky.default_value = smooth(0.0, 1.0, T['sky0'], T['sky1'], t)
        sky.keyframe_insert('default_value', frame=f)
        bg_strength.default_value = smooth(1.0, SKY_STRENGTH, T['sky0'], T['sky1'], t)
        bg_strength.keyframe_insert('default_value', frame=f)
        rim.default_value = smooth(1.0, 0.0, T['all0'], T['all1'], t)
        rim.keyframe_insert('default_value', frame=f)
        fill.data.energy = smooth(0.0, 2.2, T['sky0'], T['sky1'], t)
        fill.data.keyframe_insert('energy', frame=f)
        covers['Black'].default_value = 1.0 - smooth(0.0, 1.0, 0.0, T['fade_in'], t)
        covers['Black'].keyframe_insert('default_value', frame=f)
        covers['White'].default_value = smooth(0.0, 1.0, T['flash0'], T['flash1'], t) ** 2.2
        covers['White'].keyframe_insert('default_value', frame=f)


def linearize():
    owners = list(bpy.data.objects) + [o.data for o in bpy.data.objects if o.data] + list(bpy.data.scenes)
    owners += [m.node_tree for m in bpy.data.materials if m.node_tree] + [w.node_tree for w in bpy.data.worlds]
    for owner in owners:
        ad = getattr(owner, 'animation_data', None)
        if ad and ad.action:
            for fc in k.fcurves(owner):
                for kp in fc.keyframe_points:
                    kp.interpolation = 'LINEAR'


def compositor(sc):
    tree = bpy.data.node_groups.new('CMP_IntroGlow', 'CompositorNodeTree')
    sc.compositing_node_group = tree
    tree.interface.new_socket('Image', in_out='OUTPUT', socket_type='NodeSocketColor')
    rl = tree.nodes.new('CompositorNodeRLayers')
    glare = tree.nodes.new('CompositorNodeGlare')
    for value in ('Bloom', 'BLOOM'):
        try:
            glare.inputs['Type'].default_value = value
            break
        except (TypeError, ValueError):
            continue
    glare.inputs['Highlights Threshold'].default_value = 2.0
    glare.inputs['Strength'].default_value = 0.45
    glare.inputs['Size'].default_value = 0.6
    out = tree.nodes.new('NodeGroupOutput')
    tree.links.new(rl.outputs['Image'], glare.inputs['Image'])
    tree.links.new(glare.outputs['Image'], out.inputs[0])


def render_settings(sc):
    try:
        sc.render.engine = 'BLENDER_EEVEE'
    except TypeError:
        sc.render.engine = 'BLENDER_EEVEE_NEXT'
    sc.render.fps, sc.render.fps_base = FPS, 1.0
    sc.frame_start, sc.frame_end = 1, int(END_S * FPS)
    sc.render.resolution_x, sc.render.resolution_y = 1920, 1080
    sc.eevee.taa_render_samples = 64
    sc.eevee.shadow_pool_size = '1024'
    sc.render.use_motion_blur = True
    sc.render.motion_blur_shutter = 0.6
    sc.view_settings.view_transform = 'AgX'
    try:
        sc.view_settings.look = 'AgX - Medium High Contrast'
    except TypeError:
        pass


def export_events(sc, plan, cam):
    """소리 합성용 표. 폭죽마다 터지는 초·화면 가로 위치(좌우 이동)·화면에 보이는지."""
    from bpy_extras.object_utils import world_to_camera_view
    rows = []
    spin = bpy.data.objects['GLB_Spin']
    for p in plan:
        sc.frame_set(int(round(p['t'] * FPS)) + 1)
        world = spin.matrix_world @ p['center']
        uvz = world_to_camera_view(sc, cam, world)
        facing = (world - Vector((0, 0, 0))).normalized().dot((cam.location - Vector((0, 0, 0))).normalized())
        rows.append(dict(name=p['name'], t=round(p['t'], 3), x=round(uvz.x, 3), y=round(uvz.y, 3),
                         visible=facing > -0.15, finale=p['name'].startswith('Finale')))
    data = dict(fps=FPS, end=END_S, bursts=rows, all=[T['all0'], T['all1']], sky=[T['sky0'], T['sky1']],
                flash=[T['flash0'], T['flash1']], fade_in=T['fade_in'])
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'events.json').write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding='utf-8')
    print('이벤트 표:', OUT / 'events.json')


def build():
    for ob in list(bpy.data.objects):
        bpy.data.objects.remove(ob, do_unlink=True)
    sc = bpy.context.scene
    sc.name = SCENE
    render_settings(sc)
    top = k.coll('FF_Intro_SpaceFireworks', sc.collection)
    build_world(sc)
    root, spin = build_globe(top)
    plan = build_fireworks(top, spin)
    sun, fill = build_lights(top)
    cam, covers = build_camera(top)
    sc.camera = cam
    key_animation(sc, spin, cam, covers, sun, fill, bpy.data.materials['M_Earth_Rim'], plan)
    linearize()
    compositor(sc)
    render_settings(sc)               # 가져오기가 fps 를 바꿔도 30 으로 되돌린다
    export_events(sc, plan, cam)
    sc.frame_set(1)
    return sc


def save():
    target = str(BLEND)
    if os.path.exists(target) and os.path.normcase(bpy.data.filepath) != os.path.normcase(target):
        raise RuntimeError(f'다른 파일이 이미 있음: {target} — VERSION 을 올린다')
    os.makedirs(BLEND.parent, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=target, relative_remap=True)
    print('저장:', target)


# ---------------------------------------------------------------- 렌더

CHECK_TIMES = (0.4, 1.2, 1.9, 2.4, 3.0, 3.6, 4.3, 5.0, 5.6, 6.1, 6.5, 6.9, 7.3, 7.7, 7.9)


def _still(sc, path, frame, scale=100, samples=None):
    sc.render.resolution_percentage = scale
    if samples:
        sc.eevee.taa_render_samples = samples
    sc.frame_set(frame)
    im = sc.render.image_settings
    im.media_type = 'IMAGE'
    im.file_format = 'PNG'
    sc.render.filepath = str(path)
    bpy.ops.render.render(write_still=True, scene=sc.name)


def check(sc):
    import numpy as np
    d = OUT / 'Check'
    d.mkdir(parents=True, exist_ok=True)
    tiles = []
    for t in CHECK_TIMES:
        p = d / f'check_{t:04.2f}s.png'
        _still(sc, p, int(round(t * FPS)) + 1, scale=30, samples=24)
        im = bpy.data.images.load(str(p))
        w, h = im.size
        tiles.append(np.array(im.pixels[:], dtype=np.float32).reshape(h, w, 4))
        bpy.data.images.remove(im)
    cols = 4
    rows = (len(tiles) + cols - 1) // cols
    h, w = tiles[0].shape[:2]
    pad = 4
    sheet = np.zeros(((h + pad) * rows, (w + pad) * cols, 4), dtype=np.float32)
    sheet[..., 3] = 1
    for i, tile in enumerate(tiles):
        r, c = divmod(i, cols)
        r2 = rows - 1 - r
        sheet[r2 * (h + pad):r2 * (h + pad) + h, c * (w + pad):c * (w + pad) + w] = tile
    img = bpy.data.images.new('check_sheet', sheet.shape[1], sheet.shape[0], alpha=True)
    img.pixels = sheet.ravel()
    img.filepath_raw = str(d / 'check_sheet.png')
    img.file_format = 'PNG'
    img.save()
    sc.render.resolution_percentage = 100
    sc.eevee.taa_render_samples = 64
    print('CHECK:', d / 'check_sheet.png')


def render(sc):
    d = OUT
    d.mkdir(parents=True, exist_ok=True)
    sc.render.resolution_percentage = 100
    im = sc.render.image_settings
    im.media_type = 'VIDEO'
    im.file_format = 'FFMPEG'
    ff = sc.render.ffmpeg
    ff.format, ff.codec, ff.constant_rate_factor, ff.audio_codec = 'MPEG4', 'H264', 'HIGH', 'NONE'
    path = d / f'FF_Intro_SpaceFireworks_{VERSION}.mp4'
    before = set(d.glob('*.mp4'))
    sc.render.filepath = str(path.with_suffix(''))
    bpy.ops.render.render(animation=True, scene=sc.name)
    made = sorted(set(d.glob('*.mp4')) - before, key=lambda p: p.stat().st_mtime)
    if made and made[-1] != path:
        if path.exists():
            path.unlink()
        made[-1].rename(path)
    print('MP4:', path, path.exists())


def main(args):
    if 'build' in args:
        sc = build()
        save()
    else:
        bpy.ops.wm.open_mainfile(filepath=str(BLEND))
        sc = bpy.data.scenes[SCENE]
    if 'check' in args:
        check(sc)
    if 'render' in args:
        render(sc)


if __name__ == '__main__' and '--' in sys.argv:
    main(sys.argv[sys.argv.index('--') + 1:])
