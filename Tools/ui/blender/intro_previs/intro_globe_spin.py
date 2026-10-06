"""인트로 「지구 돌리기」 — 흰 스튜디오 무대에서 캐릭터가 실사 지구를 쓱 돌리고 쓰러지면
지구가 빨라지며 빛나다 흰 빛으로 펑. 영상은 흰 화면에서 끝나고 그 뒤는 UE 실시간 지구가 맡는다.

흐름: UE 시작 화면(로고·팀 이름·안내) → 이 영상 → UE 가운데 지구 + 「아무 키」 → 프론트 메뉴.
영상 처음도 흰 화면에서 열려 시작 화면과 이어진다.

blender -b --factory-startup --python intro_globe_spin.py -- build check render
  build : 씬을 새로 만들고 .blend 저장(이미 있으면 덮어쓰지 않고 실패한다 — 버전을 올린다)
  check : 키 시점 EEVEE 저해상도 스틸과 한 장 모음
  render: 1920×1080 MP4(소리 없음)와 시작·끝 PNG
build 없이 check/render만 주면 저장된 .blend를 연다.

좌표: 카메라가 +Y를 본다. 캐릭터는 휴지 자세에서 -Y를 보고 왼쪽이 +X(Mixamo 뼈대).
모든 시간·위치는 프리비즈 `assumption`이다.
"""
from pathlib import Path
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, str(Path(__file__).resolve().parent))
import previs_kit as k  # noqa: E402

VERSION = 'v001'
ROOT = Path(__file__).resolve().parents[4]
ART = ROOT / 'SourceArt/UI/Intro/GlobeSpin_v1'
BLEND = ART / f'Blender/FF_Intro_GlobeSpin_{VERSION}.blend'
OUT = ART / 'Previs' / VERSION
CHAR_FBX = ART / 'Character/tripo_convert_27b6b2ec-b920-4a5b-bb0b-619e544eb480.fbx'
EARTH = ROOT / 'SourceArt/ThirdParty/SolarSystemScope/PlanetTextures/Earth'
SCENE = f'FF_Intro_GlobeSpin_{VERSION}'
FPS = 30
END_S = 7.5

GLOBE_C = Vector((0.35, 0.0, 0.62))
GLOBE_R = 0.5                      # 지름 1m = 캐릭터 키
TILT_DEG = 23.5
GLOBE_PHASE_DEG = 240.0           # 시작할 때 아프리카·유럽이 정면
CONTACT_DIR = Vector((-0.8, -0.6, 0.0)).normalized()   # 지구 중심 → 캐릭터(앞 왼쪽)
STAND = (GLOBE_C + CONTACT_DIR * 0.74).to_2d()
START = Vector((-2.55, -0.3))
FALL_AT = Vector((-0.42, -0.98))

# 시간표(초)
T = dict(fade_in=0.35, walk0=0.25, walk1=2.45, turn=2.75, reach=3.0, swipe=3.25, spin=4.05,
         sit=4.25, lie=4.5, glow0=4.55, push0=4.4, push1=6.9, flash0=6.85, flash1=7.2)

CAM_WIDE = (Vector((-0.05, -4.1, 0.95)), Vector((-0.05, 0.0, 0.5)))
CAM_GLOBE = (Vector((0.35, -2.75, 0.8)), Vector((0.35, 0.0, 0.62)))

PALETTE = {
    'stage': ((0.86, 0.88, 0.91), 0.85),
}


# ---------------------------------------------------------------- 시간 도우미

def smooth(a, b, t0, t1, t):
    if t <= t0:
        return a
    if t >= t1:
        return b
    x = (t - t0) / (t1 - t0)
    x = x * x * (3 - 2 * x)
    return a + (b - a) * x


def ease_out(a, b, t0, t1, t):
    if t <= t0:
        return a
    if t >= t1:
        return b
    x = (t - t0) / (t1 - t0)
    return a + (b - a) * (1 - (1 - x) ** 3)


def window(t, t0, t1):
    return t0 <= t < t1


# ---------------------------------------------------------------- 지구 회전(서→동, 위에서 보아 반시계)

def globe_speed(t):
    """초당 각도. 손에 밀린 순간 크게 뛰고, 쓰러진 뒤 점점 빨라진다."""
    if t < T['reach'] + 0.05:
        return 30.0
    if t < T['swipe']:
        return smooth(30.0, 460.0, T['reach'] + 0.05, T['swipe'], t)
    if t < T['glow0']:
        return smooth(460.0, 320.0, T['swipe'], T['glow0'], t)
    return 320.0 * math.exp(min(t, T['flash1']) - T['glow0'])     # 펑 직전 약 3,000°/s


def globe_angles(frames):
    out, a = {}, GLOBE_PHASE_DEG
    for f in frames:
        out[f] = a
        a += globe_speed((f - 1) / FPS) / FPS
    return out


# ---------------------------------------------------------------- 씬

def _reset():
    for ob in list(bpy.data.objects):
        bpy.data.objects.remove(ob, do_unlink=True)
    sc = bpy.context.scene
    sc.name = SCENE
    for c in list(sc.collection.children):
        k.purge(c)
        bpy.data.collections.remove(c)
    return sc


def _stage(c):
    """무한 배경(바닥이 뒤에서 둥글게 올라가는 흰 벽)."""
    import bmesh
    prof = [(y, 0.0) for y in (-9.0, -4.0, 0.0, 1.5)]
    cy, cz, r = 1.5, 2.6, 2.6
    for i in range(1, 13):
        a = -math.pi / 2 + (math.pi / 2) * i / 12
        prof.append((cy + r * math.cos(a), cz + r * math.sin(a)))
    prof.append((cy + r, 12.0))
    bm = bmesh.new()
    rows = [[bm.verts.new((x, y, z)) for x in (-14.0, 14.0)] for y, z in prof]
    for a, b in zip(rows, rows[1:]):
        bm.faces.new((a[0], a[1], b[1], b[0]))
    me = bpy.data.meshes.new('SET_Cyc')
    bm.to_mesh(me)
    bm.free()
    me.shade_smooth()
    rgb, rough = PALETTE['stage']
    k.link(bpy.data.objects.new('SET_Cyc', me), c, k.principled('M_Stage_White', rgb, rough))


def _mix_alpha_material(name, alpha_from, shader_from, method='BLENDED'):
    """투명 ↔ 셰이더를 섞는 머티리얼. alpha_from(nt)는 섞음 비율 소켓, shader_from(nt)는 셰이더 소켓을 돌려준다."""
    m = bpy.data.materials.new(name)
    nt = m.node_tree
    nt.nodes.clear()
    outn = nt.nodes.new('ShaderNodeOutputMaterial')
    mix = nt.nodes.new('ShaderNodeMixShader')
    tr = nt.nodes.new('ShaderNodeBsdfTransparent')
    nt.links.new(tr.outputs[0], mix.inputs[1])
    nt.links.new(shader_from(nt), mix.inputs[2])
    fac = alpha_from(nt)
    if isinstance(fac, float):
        mix.inputs[0].default_value = fac
    else:
        nt.links.new(fac, mix.inputs[0])
    nt.links.new(mix.outputs[0], outn.inputs[0])
    m.surface_render_method = method
    m.use_backface_culling = True
    return m, mix


def _image_node(nt, path, non_color=False):
    tex = nt.nodes.new('ShaderNodeTexImage')
    tex.image = bpy.data.images.load(str(path), check_existing=True)
    if non_color:
        tex.image.colorspace_settings.name = 'Non-Color'
    return tex


def _globe(c):
    root = k.empty('GLB_Root', c, loc=GLOBE_C, size=0.3)
    tilt = k.empty('GLB_Tilt', c, parent=root, rot=(0, math.radians(TILT_DEG), 0), size=0.3)
    spin = k.empty('GLB_Spin', c, parent=tilt, size=0.3, kind='SINGLE_ARROW')

    earth_mat = k.image_material('M_Earth_Real', EARTH / '2k_earth_daymap.jpg', (0.2, 0.4, 0.7), rough=0.55)
    nt = earth_mat.node_tree
    bsdf = next(n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED')
    tex = next(n for n in nt.nodes if n.type == 'TEX_IMAGE')
    nt.links.new(k.sock(tex, 'Color', outputs=True), k.sock(bsdf, 'Emission Color'))
    k.sock(bsdf, 'Emission Strength').default_value = 0.0
    k.sphere('GLB_Earth', GLOBE_R, (0, 0, 0), c, earth_mat, parent=spin, u=96, v=48, uvs=True)

    def cloud_alpha(nt):
        t = _image_node(nt, EARTH / '2k_earth_clouds.jpg', non_color=True)
        mul = nt.nodes.new('ShaderNodeMath')
        mul.operation = 'MULTIPLY'
        mul.inputs[1].default_value = 0.7
        pw = nt.nodes.new('ShaderNodeMath')
        pw.operation = 'POWER'
        pw.inputs[1].default_value = 1.8          # 옅은 구름 안개를 걷어 대륙이 보이게
        nt.links.new(t.outputs['Color'], pw.inputs[0])
        nt.links.new(pw.outputs[0], mul.inputs[0])
        return mul.outputs[0]

    def cloud_shader(nt):
        b = nt.nodes.new('ShaderNodeBsdfPrincipled')
        k.sock(b, 'Base Color').default_value = (1, 1, 1, 1)
        k.sock(b, 'Roughness').default_value = 0.9
        return b.outputs[0]

    cloud_mat, _ = _mix_alpha_material('M_Earth_Clouds', cloud_alpha, cloud_shader, method='DITHERED')
    clouds = k.sphere('GLB_Clouds', GLOBE_R * 1.012, (0, 0, 0), c, cloud_mat, parent=spin, u=96, v=48, uvs=True)
    clouds.visible_shadow = False

    def rim_alpha(nt):
        lw = nt.nodes.new('ShaderNodeLayerWeight')
        lw.inputs['Blend'].default_value = 0.35
        pw = nt.nodes.new('ShaderNodeMath')
        pw.operation = 'POWER'
        pw.inputs[1].default_value = 2.2
        nt.links.new(lw.outputs['Facing'], pw.inputs[0])
        return pw.outputs[0]

    def rim_shader(nt):
        e = nt.nodes.new('ShaderNodeEmission')
        e.name = 'RimEmission'
        e.inputs['Color'].default_value = (0.55, 0.8, 1.0, 1)
        e.inputs['Strength'].default_value = 2.0
        return e.outputs[0]

    rim_mat, _ = _mix_alpha_material('M_Earth_Rim', rim_alpha, rim_shader)
    rim = k.sphere('GLB_Rim', GLOBE_R * 1.07, (0, 0, 0), c, rim_mat, parent=root, u=64, v=32)
    rim.visible_shadow = False

    glow = k.light('LGT_GlobeGlow', 'POINT', GLOBE_C + Vector((0, -0.9, 0.05)), c, (0.8, 0.92, 1.0), 0.0, radius=0.4)
    glow.data.use_shadow = False
    return root, spin, earth_mat, rim_mat, glow


def _character(c):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(CHAR_FBX))
    new = [o for o in bpy.data.objects if o not in before]
    arm = next(o for o in new if o.type == 'ARMATURE')
    arm.name = 'CHR_Armature'
    for o in new:
        for uc in list(o.users_collection):
            uc.objects.unlink(o)
        c.objects.link(o)
        if o.type == 'MESH':
            o.name = 'CHR_Body'
    root = k.empty('CHR_Root', c, size=0.3, kind='ARROWS')
    arm.parent = root
    for pb in arm.pose.bones:
        pb.rotation_mode = 'QUATERNION'
    return root, arm


def _lights(c):
    sun = k.light('LGT_Key', 'SUN', (-3, -4, 6), c, (1.0, 0.97, 0.92), 5.0, target=(0.2, 0, 0), angle_deg=6)
    fill = k.light('LGT_Fill', 'AREA', (3.5, -3.5, 2.2), c, (0.85, 0.92, 1.0), 260, target=(0, 0, 0.5), size=3.0)
    fill.data.use_shadow = False
    back = k.light('LGT_Back', 'AREA', (0.4, 3.0, 3.2), c, (1.0, 1.0, 1.0), 300, target=(0.3, 0, 0.6), size=2.0)
    back.data.use_shadow = False
    return sun


def _world(sc):
    w = sc.world or bpy.data.worlds.new('W_Intro')
    sc.world = w
    bg = next(n for n in w.node_tree.nodes if n.type == 'BACKGROUND')
    bg.inputs['Color'].default_value = (0.78, 0.84, 0.92, 1)
    bg.inputs['Strength'].default_value = 1.05


def _camera(c):
    cam = k.camera('CAM_Intro', CAM_WIDE[0], CAM_WIDE[1], 35, c)
    def flash_alpha(nt):
        return 1.0

    def flash_shader(nt):
        e = nt.nodes.new('ShaderNodeEmission')
        e.inputs['Color'].default_value = (1, 1, 1, 1)
        e.inputs['Strength'].default_value = 40.0
        return e.outputs[0]

    mat, mix = _mix_alpha_material('M_Flash_White', flash_alpha, flash_shader)
    mat.use_backface_culling = False
    import bmesh
    bm = bmesh.new()
    bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=0.3)
    me = bpy.data.meshes.new('FX_Flash')
    bm.to_mesh(me)
    bm.free()
    flash = k.link(bpy.data.objects.new('FX_Flash', me), c, mat, parent=cam, loc=(0, 0, -0.1))
    flash.visible_shadow = False
    return cam, mix


# ---------------------------------------------------------------- 캐릭터 자세

def R(axis, deg):
    return Matrix.Rotation(math.radians(deg), 3, axis)


def chain(*rots):
    """먼저 적용할 회전을 오른쪽에. chain(R2, R1) = R2 @ R1."""
    m = Matrix.Identity(3)
    for r in rots:
        m = m @ r
    return m


ARM_DOWN = 68.0


def arm(side, down=ARM_DOWN, fwd=0.0, across=0.0, out=0.0):
    """side 'L'/'R'. down: T자에서 내린 각, fwd: 앞으로 든 각, across: 몸 쪽(왼팔은 오른쪽으로) 돌림."""
    s = 1 if side == 'L' else -1
    return chain(R('Z', s * -across), R('X', -fwd), R('Y', s * (down - out)))


def elbow(side, deg):
    return R('Z', (-1 if side == 'L' else 1) * deg)


def walk_pose(t, period, amp=1.0):
    p = 2 * math.pi * t / period
    sp, cp = math.sin(p), math.cos(p)
    A, K, B = 30 * amp, 45 * amp, 28 * amp
    pose = {
        'LeftUpLeg': R('X', -A * sp), 'RightUpLeg': R('X', A * sp),
        'LeftLeg': R('X', K * max(0.0, cp)), 'RightLeg': R('X', K * max(0.0, -cp)),
        'LeftArm': chain(R('X', B * sp), arm('L')), 'RightArm': chain(R('X', -B * sp), arm('R')),
        'LeftForeArm': elbow('L', 25), 'RightForeArm': elbow('R', 25),
        'Spine': chain(R('Z', 7 * amp * sp), R('X', 6)),
        'Head': R('X', -3 * math.cos(2 * p)),
    }
    bob = 0.022 * amp * math.cos(2 * p)
    return pose, Vector((0, 0, bob))


def idle_pose():
    return {'LeftArm': arm('L'), 'RightArm': arm('R'),
            'LeftForeArm': elbow('L', 12), 'RightForeArm': elbow('R', 12)}, Vector()


def blend(pa, pb, x):
    """두 자세 사전을 회전 행렬 단위로 섞는다(쿼터니언 slerp)."""
    out = {}
    for name in set(pa[0]) | set(pb[0]):
        qa = pa[0].get(name, Matrix.Identity(3)).to_quaternion()
        qb = pb[0].get(name, Matrix.Identity(3)).to_quaternion()
        out[name] = qa.slerp(qb, x).to_matrix()
    return out, pa[1].lerp(pb[1], x)


def character_state(t):
    """(root_xy, yaw_deg, (pose, hips_offset)) — yaw는 휴지 정면(-Y)을 몇 도 돌렸는지."""
    def yaw_to(d):
        return math.degrees(math.atan2(d.x, -d.y))

    walk_dir = (STAND - START).normalized()
    yaw_walk = yaw_to(walk_dir)
    yaw_globe = yaw_to(-CONTACT_DIR.to_2d())
    yaw_fallen = yaw_globe - 540.0

    if t < T['walk1']:
        pos = START.lerp(STAND, ease_out(0.0, 1.0, T['walk0'], T['walk1'], t) * 0.35
                         + smooth(0.0, 1.0, T['walk0'], T['walk1'], t) * 0.65)
        amp = smooth(1.0, 0.4, T['walk1'] - 0.35, T['walk1'], t)
        return pos, yaw_walk, walk_pose(t, 0.44, amp)

    if t < T['turn']:
        x = smooth(0, 1, T['walk1'], T['turn'], t)
        return STAND, smooth(yaw_walk, yaw_globe, T['walk1'], T['turn'], t), blend(walk_pose(t, 0.44, 0.4), idle_pose(), x)

    wind = ({
        'Spine': chain(R('Z', 22), R('X', 10)), 'Spine2': R('Z', 8),
        'RightArm': chain(R('Z', 38), arm('R', fwd=82)), 'RightForeArm': elbow('R', 12),
        'LeftArm': arm('L', down=55, fwd=-15), 'LeftForeArm': elbow('L', 20),
        'Head': R('X', 8),
        'LeftUpLeg': R('X', -8), 'RightUpLeg': R('X', 10), 'RightLeg': R('X', 12),
    }, Vector((0, 0, -0.015)))
    swiped = ({
        'Spine': chain(R('Z', -32), R('X', 8)), 'Spine2': R('Z', -12), 'Hips': R('Z', -14),
        'RightArm': chain(R('Z', -78), arm('R', fwd=70)), 'RightForeArm': elbow('R', 6),
        'LeftArm': arm('L', down=30, fwd=20), 'LeftForeArm': elbow('L', 35),
        'Head': R('Z', -10),
        'LeftUpLeg': R('X', 6), 'RightUpLeg': R('X', -6),
    }, Vector((0, 0, -0.01)))

    if t < T['reach']:
        return STAND, yaw_globe, blend(idle_pose(), wind, smooth(0, 1, T['turn'], T['reach'], t))
    if t < T['swipe']:
        x = ease_out(0, 1, T['reach'], T['swipe'], t)
        return STAND, yaw_globe - 18 * x, blend(wind, swiped, x)

    if t < T['spin']:
        x = smooth(0, 1, T['swipe'], T['spin'], t)
        u = (t - T['swipe']) / (T['spin'] - T['swipe'])
        pos = STAND.lerp(FALL_AT, x)
        flail = math.sin(u * math.pi * 6)
        step = walk_pose(t, 0.28, 0.8)[0]
        dizzy = ({
            'Spine': chain(R('Y', 10 * flail), R('X', -6)),
            'LeftArm': arm('L', down=10 + 15 * flail, fwd=10), 'RightArm': arm('R', down=10 - 15 * flail, fwd=-10),
            'LeftForeArm': elbow('L', 30), 'RightForeArm': elbow('R', 30),
            'Head': chain(R('Y', -14 * flail), R('X', -8)),
            'LeftUpLeg': step['LeftUpLeg'], 'RightUpLeg': step['RightUpLeg'],
            'LeftLeg': step['LeftLeg'], 'RightLeg': step['RightLeg'],
        }, Vector((0, 0, 0.02 * abs(flail))))
        mix = blend(swiped, dizzy, smooth(0, 1, T['swipe'], T['swipe'] + 0.2, t))
        return pos, smooth(yaw_globe - 18, yaw_fallen, T['swipe'], T['spin'], t), mix

    sit = ({
        'Hips': R('X', -12),
        'Spine': R('X', 14), 'Head': R('X', 10),
        'LeftUpLeg': R('X', -82), 'RightUpLeg': R('X', -76), 'LeftLeg': R('X', 18), 'RightLeg': R('X', 24),
        'LeftArm': arm('L', down=40, fwd=-35), 'RightArm': arm('R', down=40, fwd=-35),
        'LeftForeArm': elbow('L', 10), 'RightForeArm': elbow('R', 10),
    }, Vector((0, 0.04, -0.36)))
    lie = ({
        'Hips': R('X', -84),
        'Spine': R('X', -4), 'Head': R('X', -6),
        'LeftUpLeg': R('X', -28), 'RightUpLeg': R('X', -18), 'LeftLeg': R('X', 30), 'RightLeg': R('X', 20),
        'LeftArm': arm('L', down=8, fwd=-20), 'RightArm': arm('R', down=8, fwd=-20),
        'LeftForeArm': elbow('L', 20), 'RightForeArm': elbow('R', 20),
    }, Vector((0, 0.10, -0.40)))
    rest = ({**lie[0], 'LeftUpLeg': R('X', -8), 'RightUpLeg': R('X', -4), 'LeftLeg': R('X', 6), 'RightLeg': R('X', 4)},
            Vector((0, 0.10, -0.415)))

    if t < T['sit']:
        prev = character_state(T['spin'] - 1e-4)[2]
        return FALL_AT, yaw_fallen, blend(prev, sit, smooth(0, 1, T['spin'], T['sit'], t))
    if t < T['lie']:
        return FALL_AT, yaw_fallen, blend(sit, lie, ease_out(0, 1, T['sit'], T['lie'], t))
    x = smooth(0, 1, T['lie'], T['lie'] + 0.9, t)
    pose, hip = blend(lie, rest, x)
    bounce = 0.025 * math.exp(-6 * (t - T['lie'])) * abs(math.sin((t - T['lie']) * 16))
    wob = math.sin((t - T['lie']) * 7)
    pose['Head'] = chain(R('Y', 10 * wob), pose['Head'])
    return FALL_AT, yaw_fallen, (pose, hip + Vector((0, 0, bounce)))


# ---------------------------------------------------------------- 키프레임

def _key_character(root, arm_ob, frames):
    rest = {pb.name: pb.bone.matrix_local.to_3x3() for pb in arm_ob.pose.bones}
    prev = {}
    for f in frames:
        t = (f - 1) / FPS
        xy, yaw, (pose, hip) = character_state(t)
        root.location = (xy.x, xy.y, 0.0)
        root.rotation_euler = (0, 0, math.radians(yaw))
        root.keyframe_insert('location', frame=f)
        root.keyframe_insert('rotation_euler', frame=f)
        for pb in arm_ob.pose.bones:
            short = pb.name.split(':')[-1]
            rb = rest[pb.name]
            d = pose.get(short, Matrix.Identity(3))
            q = (rb.inverted() @ d @ rb).to_quaternion()
            if pb.name in prev and prev[pb.name].dot(q) < 0:
                q.negate()
            prev[pb.name] = q
            pb.rotation_quaternion = q
            if short == 'Hips':
                pb.location = rb.inverted() @ hip
                pb.keyframe_insert('location', frame=f)
            if short in pose or f == frames[0]:
                pb.keyframe_insert('rotation_quaternion', frame=f)


def _key_globe(spin, earth_mat, rim_mat, glow, root, frames):
    ang = globe_angles(frames)
    bsdf = next(n for n in earth_mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
    es = k.sock(bsdf, 'Emission Strength')
    rim = rim_mat.node_tree.nodes['RimEmission'].inputs['Strength']
    for f in frames:
        t = (f - 1) / FPS
        spin.rotation_euler = (0, 0, math.radians(ang[f]))
        spin.keyframe_insert('rotation_euler', frame=f)
        g = smooth(0.0, 1.0, T['glow0'], T['flash1'], t) ** 2
        es.default_value = 7.0 * g
        es.keyframe_insert('default_value', frame=f)
        rim.default_value = 2.0 + 40.0 * g
        rim.keyframe_insert('default_value', frame=f)
        glow.data.energy = 900.0 * g
        glow.data.keyframe_insert('energy', frame=f)
        pulse = 1.0 + 0.06 * smooth(0, 1, T['flash0'] - 0.15, T['flash1'], t)
        root.scale = (pulse, pulse, pulse)
        root.keyframe_insert('scale', frame=f)


def _key_camera(cam, flash_mix, frames):
    fac = flash_mix.inputs[0]
    for f in frames:
        t = (f - 1) / FPS
        x = smooth(0, 1, T['push0'], T['push1'], t)
        loc = CAM_WIDE[0].lerp(CAM_GLOBE[0], x)
        tgt = CAM_WIDE[1].lerp(CAM_GLOBE[1], x)
        shake = smooth(0, 1, T['glow0'] + 1.2, T['flash1'], t) * 0.006
        loc = loc + Vector((shake * math.sin(t * 61), 0, shake * math.cos(t * 47)))
        cam.location = loc
        k.look_at(cam, tgt)
        cam.keyframe_insert('location', frame=f)
        cam.keyframe_insert('rotation_euler', frame=f)
        a = 1.0 - smooth(0, 1, 0.0, T['fade_in'], t)
        a = max(a, smooth(0, 1, T['flash0'], T['flash1'], t))
        fac.default_value = a
        fac.keyframe_insert('default_value', frame=f)


def _linearize():
    for ob in bpy.data.objects:
        for owner in (ob, ob.data):
            ad = getattr(owner, 'animation_data', None)
            if ad and ad.action:
                for fc in k.fcurves(owner):
                    for kp in fc.keyframe_points:
                        kp.interpolation = 'LINEAR'
    for m in bpy.data.materials:
        ad = m.node_tree.animation_data if m.node_tree else None
        if ad and ad.action:
            for fc in k.fcurves(m.node_tree):
                for kp in fc.keyframe_points:
                    kp.interpolation = 'LINEAR'


def build():
    sc = _reset()
    sc.render.fps = FPS
    sc.frame_start, sc.frame_end = 1, int(round(END_S * FPS))
    sc.render.resolution_x, sc.render.resolution_y = 1920, 1080
    top = k.coll('FF_Intro_GlobeSpin', sc.collection)
    _stage(k.coll('SET_Stage', top))
    root, spin, earth_mat, rim_mat, glow = _globe(k.coll('GLB_Globe', top))
    chr_root, arm_ob = _character(k.coll('CHR_Character', top))
    # FBX 가져오기가 장면 프레임 속도를 파일 값(24)으로 바꾼다. 그대로 두면 MP4가 24fps로 태그돼 1.25배 느려진다.
    sc.render.fps, sc.render.fps_base = FPS, 1.0
    _lights(k.coll('LGT_Lights', top))
    _world(sc)
    cam, flash_mix = _camera(k.coll('CAM_Camera', top))
    sc.camera = cam
    frames = list(range(sc.frame_start, sc.frame_end + 1))
    _key_character(chr_root, arm_ob, frames)
    _key_globe(spin, earth_mat, rim_mat, glow, root, frames)
    _key_camera(cam, flash_mix, frames)
    _linearize()
    _render_settings(sc)
    sc.frame_set(1)
    return sc


def _render_settings(sc):
    try:
        sc.render.engine = 'BLENDER_EEVEE'
    except TypeError:
        sc.render.engine = 'BLENDER_EEVEE_NEXT'
    sc.eevee.taa_render_samples = 48
    sc.eevee.shadow_pool_size = '1024'
    sc.render.use_motion_blur = True
    sc.render.motion_blur_shutter = 0.5
    vs = sc.view_settings
    vs.view_transform = 'AgX'
    try:                                  # look은 동적 열거라 RNA 목록이 실제 값을 다 보여 주지 않는다
        vs.look = 'AgX - Base Contrast'
    except TypeError:
        pass


def save():
    target = str(BLEND)
    if os.path.exists(target) and os.path.normcase(bpy.data.filepath) != os.path.normcase(target):
        raise RuntimeError(f'다른 파일이 이미 있음: {target} — VERSION을 올린다')
    os.makedirs(BLEND.parent, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=target, relative_remap=True)
    print('저장:', target)


# ---------------------------------------------------------------- 렌더

CHECK_TIMES = (0.2, 0.9, 1.7, 2.45, 2.95, 3.15, 3.45, 3.8, 4.15, 4.45, 5.2, 6.0, 6.6, 6.95, 7.1, 7.4)


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
        _still(sc, p, int(round(t * FPS)) + 1, scale=25, samples=16)
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
    for i, t in enumerate(tiles):
        r, c = divmod(i, cols)
        r2 = rows - 1 - r
        sheet[r2 * (h + pad):r2 * (h + pad) + h, c * (w + pad):c * (w + pad) + w] = t
    img = bpy.data.images.new('check_sheet', sheet.shape[1], sheet.shape[0], alpha=True)
    img.pixels = sheet.ravel()
    img.filepath_raw = str(d / 'check_sheet.png')
    img.file_format = 'PNG'
    img.save()
    print('CHECK:', d / 'check_sheet.png', [f'{t}s' for t in CHECK_TIMES])


def render(sc):
    d = OUT
    d.mkdir(parents=True, exist_ok=True)
    _still(sc, d / 'intro_start.png', int(T['fade_in'] * FPS) + 2, samples=48)
    _still(sc, d / 'intro_before_flash.png', int(T['flash0'] * FPS), samples=48)
    sc.render.resolution_percentage = 100
    im = sc.render.image_settings
    im.media_type = 'VIDEO'
    im.file_format = 'FFMPEG'
    ff = sc.render.ffmpeg
    ff.format, ff.codec, ff.constant_rate_factor, ff.audio_codec = 'MPEG4', 'H264', 'HIGH', 'NONE'
    path = d / f'FF_Intro_GlobeSpin_{VERSION}.mp4'
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
