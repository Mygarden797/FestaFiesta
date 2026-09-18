"""P3 지역 시험: 원본 해안 고정, 낮은 육지, 둥근 해안 단면, 고도 강조.

MCP에서 build() 후 export()를 별도로 실행한다. P02 씬과 파일은 보존한다.
"""
from pathlib import Path
import json
import math
import hashlib
import bpy
import bmesh
import numpy as np
from mathutils import Vector
from prepare_roundtrip import parse_obj

ROOT = Path(__file__).resolve().parents[4]
ART = ROOT / 'SourceArt/UI/Atlas/BrightGlobe_v1'
OUT = ROOT / 'Saved/BrightGlobe'
NAME = 'FF_BrightGlobe_P03_Pilot'
CONFIG = {'radius': 5000., 'land_thickness': 40., 'mountain_height': 650.,
          'height_floor': .008, 'height_power': 1.15, 'round_depth': 12.,
          'inset_factor_requested': 2., 'coast_round_segments': 4,
          'scope_lon_lat': [-20, 55, 20, 65]}


def shape_height(h):
    return np.maximum(0., (np.asarray(h) - CONFIG['height_floor']) / (1. - CONFIG['height_floor'])) ** CONFIG['height_power']


def mesh_object(scene, name, vertices, faces, corner_uv):
    mesh = bpy.data.meshes.new(name + '_Mesh')
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    layer = mesh.uv_layers.new(name='UVMap')
    layer.data.foreach_set('uv', np.asarray(corner_uv, dtype=np.float32).ravel())
    for poly in mesh.polygons:
        poly.use_smooth = True
    obj = bpy.data.objects.new('GEO-' + name, mesh)
    scene.collection.objects.link(obj)
    obj['bright_p03_export'] = name
    return obj


def build():
    assert NAME not in bpy.data.scenes, '기존 P03 씬은 먼저 확인'
    assert json.loads((OUT / 'p02_ue_import.json').read_text())['status'] == 'passed', 'UE 왕복 확인 필요'
    scene = bpy.data.scenes.new(NAME)
    bpy.context.window.scene = scene
    scene.unit_settings.scale_length = .01
    report = {'status': 'complete', 'config': CONFIG, 'tiles': [], 'world_expanded': False,
              'limits': '지역 절단면 열림; 전세계 완성본 아님. 새 해안선·산맥 위치를 그리지 않음.'}
    inputdir = ART / 'P02/Inputs'
    for tile in (3, 4, 5):
        stem = f'SM_Diorama_{tile:02d}'
        top_path = inputdir / (stem + '_Top.obj')
        tv, tuv, tf = parse_obj(top_path)
        pos = np.asarray(tv)
        uv = np.asarray(tuv)
        fs = np.asarray([[v for v, u in f] for f in tf])
        radii = np.linalg.norm(pos, axis=1)
        direction = pos / radii[:, None]
        h = np.maximum(0., (radii - 5100.) / 350.)
        target_r = CONFIG['radius'] + CONFIG['land_thickness'] + CONFIG['mountain_height'] * shape_height(h)
        by_position = {tuple(p): i for i, p in enumerate(tv)}
        full_path = ROOT / 'SourceArt/UI/Atlas/DioramaWorld_v1' / (stem + '_Coast.obj')
        cv, cu, _ = parse_obj(full_path)
        assert len(cv) % 8 == 0
        edges = []
        outer_for_top = {}
        for start in range(0, len(cv), 8):
            a = by_position.get(cv[start+7])
            b = by_position.get(cv[start+6])
            if a is None or b is None:
                continue
            # 원본 8정점 = body quad + bevel quad. 기존 outer 해안선은 고정한다.
            oa = np.asarray(cv[start]); oa /= np.linalg.norm(oa)
            ob = np.asarray(cv[start+1]); ob /= np.linalg.norm(ob)
            edges.append((a, b, oa, ob))
            outer_for_top[a] = oa
            outer_for_top[b] = ob
        factor = CONFIG['inset_factor_requested']
        while True:
            moved = direction.copy()
            for index, outer in outer_for_top.items():
                moved[index] = outer + (direction[index] - outer) * factor
                moved[index] /= np.linalg.norm(moved[index])
            vertices = moved * target_r[:, None]
            normal = np.cross(vertices[fs[:,1]]-vertices[fs[:,0]], vertices[fs[:,2]]-vertices[fs[:,0]])
            sign = np.sum(normal * vertices[fs[:,0]], axis=1)
            if np.min(sign) > 0:
                break
            assert factor > 1., '원본 토폴로지 방향 문제'
            factor = max(1., factor * .75)
        top = mesh_object(scene, f'SM_BrightP03_{tile:02d}_Top', vertices.tolist(), fs.tolist(),
                          [tuv[u] for f in tf for v, u in f])
        coast_v, coast_uv, coast_f = [], [], []
        # 어깨에서 윗면까지 1/4 타원 단면. body는 기존 높은 절벽보다 낮아진다.
        ring = [(0., 0., 0.)] + [(1., 1.-math.cos(t), CONFIG['round_depth']*(1.-math.sin(t)))
                  for t in np.linspace(0., math.pi/2, CONFIG['coast_round_segments']+1)]
        for a, b, oa, ob in edges:
            base = len(coast_v)
            for weight, blend, depth in ring:
                for index, outer in ((a, oa), (b, ob)):
                    d = outer*(1.-blend) + moved[index]*blend
                    d /= np.linalg.norm(d)
                    radius = 4998. if weight == 0 else target_r[index]-depth
                    coast_v.append(tuple(d*radius))
                    # OBJ/Blender V는 반전값. UE에서는 (height, weight)가 된다.
                    coast_uv.append((float(h[index]), 1.-weight))
            for j in range(len(ring)-1):
                n = base + j*2
                coast_f.extend([(n, n+3, n+1), (n, n+2, n+3)])
        coast = mesh_object(scene, f'SM_BrightP03_{tile:02d}_Coast', coast_v, coast_f,
                            [coast_uv[v] for face in coast_f for v in face])
        # 같은 좌표의 경계만 병합한다. UV는 면 코너에 보존한다.
        bm = bmesh.new(); bm.from_mesh(coast.data)
        bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=.0005)
        bm.to_mesh(coast.data); bm.free(); coast.data.update()
        bv, bu, bf = parse_obj(inputdir / (stem + '_Bottom.obj'))
        bottom = mesh_object(scene, f'SM_BrightP03_{tile:02d}_Bottom', bv,
            [[v for v, u in face] for face in bf], [bu[u] for face in bf for v, u in face])
        for obj in (top, coast, bottom):
            kind = 'Coast' if obj == coast else 'Top' if obj == top else 'Bottom'
            mat = bpy.data.materials.get('MAT-BrightP02-' + kind)
            if mat:
                obj.data.materials.append(mat)
        report['tiles'].append({'tile': tile, 'inset_factor': factor, 'triangles': len(fs)+len(coast_f)+len(bf),
            'coast_edges': len(edges), 'minimum_top_outward_dot': float(sign.min()),
            'source_top_sha256': hashlib.sha256(top_path.read_bytes()).hexdigest(),
            'height_above_sea_range': [float(target_r.min()-5000), float(target_r.max()-5000)],
            'maximum_inward_shift_uu': float(np.max(np.linalg.norm(moved-direction, axis=1))*5000)})
    scene['bright_globe_config'] = json.dumps(CONFIG)
    direction = Vector((.78, .15, .6))
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            area.spaces.active.clip_end = 200000
            area.spaces.active.region_3d.view_location = (0, 0, 0)
            area.spaces.active.region_3d.view_distance = 12000
            area.spaces.active.region_3d.view_rotation = direction.to_track_quat('Z', 'Y')
    (OUT / 'p03_geometry.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print('P03_BUILD_OK', sum(t['triangles'] for t in report['tiles']))


def export():
    assert bpy.context.scene.name == NAME
    exportdir = ART / 'P03/Exports'; exportdir.mkdir(parents=True, exist_ok=True)
    for key, value in [('forward_axis','Y'), ('up_axis','Z')]:
        assert value in [i.identifier for i in bpy.ops.wm.obj_export.get_rna_type().properties[key].enum_items]
    for obj in bpy.context.scene.objects:
        if 'bright_p03_export' not in obj:
            continue
        for other in bpy.context.scene.objects:
            other.select_set(False)
        obj.select_set(True); bpy.context.view_layer.objects.active = obj
        path = exportdir / (obj['bright_p03_export']+'.obj')
        assert not path.exists()
        bpy.ops.wm.obj_export(filepath=str(path), export_selected_objects=True, forward_axis='Y', up_axis='Z',
            global_scale=1., apply_modifiers=False, export_uv=True, export_normals=True, export_materials=False,
            export_triangulated_mesh=False, check_existing=False)
    path = ART / 'Blender/FF_BrightGlobe_P03_pilot_v001.blend'
    assert not path.exists()
    bpy.ops.file.pack_all()
    bpy.ops.wm.save_as_mainfile(filepath=str(path), check_existing=False)
    print('P03_EXPORTED', str(path))
