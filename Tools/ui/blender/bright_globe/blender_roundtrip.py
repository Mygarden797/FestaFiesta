"""Blender 5.2 P2: 원본 좌표와 UV를 변경하지 않는 별도 씬 왕복."""
from pathlib import Path
import json
import math
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[4]
ART = ROOT / 'SourceArt/UI/Atlas/BrightGlobe_v1'
INPUT = ART / 'P02/Inputs'
EXPORT = ART / 'P02/Exports'
OUT = ROOT / 'Saved/BrightGlobe'
SCENE = 'FF_BrightGlobe_P02_Roundtrip'


def checked_enum(owner, prop, value):
    values = [i.identifier for i in owner.bl_rna.properties[prop].enum_items]
    assert value in values, (prop, value, values)
    setattr(owner, prop, value)


def import_scene():
    assert (OUT / 'blender_session_before.json').exists(), '사용자 씬 백업 필요'
    assert SCENE not in bpy.data.scenes, '기존 시험 씬을 덮어쓰지 않음'
    scene = bpy.data.scenes.new(SCENE)
    bpy.context.window.scene = scene
    scene.unit_settings.scale_length = 0.01  # 좌표 1 == UE 1 cm, OBJ 자체는 단위 없음
    checked_enum(scene.unit_settings, 'system', 'METRIC')
    props = bpy.ops.wm.obj_import.get_rna_type().properties
    for prop, value in [('forward_axis', 'Y'), ('up_axis', 'Z')]:
        assert value in [i.identifier for i in props[prop].enum_items]
    objects = []
    for path in sorted(INPUT.glob('*.obj')):
        before = set(scene.objects)
        bpy.ops.wm.obj_import(filepath=str(path), forward_axis='Y', up_axis='Z',
                              global_scale=1.0, validate_meshes=False)
        imported = set(scene.objects) - before
        assert len(imported) == 1, path
        obj = imported.pop()
        obj.name = 'GEO-' + path.stem
        obj['bright_globe_source'] = path.name
        obj['radius_uu'] = 5000.0
        obj['base_land_thickness_uu'] = 100.0
        obj['base_mountain_height_uu'] = 350.0
        assert obj.data.uv_layers.active is not None
        assert all(abs(obj.location[a]) < 1e-6 for a in range(3))
        mat = bpy.data.materials.get('MAT-BrightP02-' + path.stem.rsplit('_', 1)[1])
        if mat is None:
            mat = bpy.data.materials.new('MAT-BrightP02-' + path.stem.rsplit('_', 1)[1])
            shader = next(n for n in mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
            if path.stem.endswith('Top'):
                tex = mat.node_tree.nodes.new('ShaderNodeTexImage')
                tex.image = bpy.data.images.load(str(INPUT / 'T_Diorama_Color.png'), check_existing=True)
                mat.node_tree.links.new(tex.outputs['Color'], shader.inputs['Base Color'])
            else:
                shader.inputs['Base Color'].default_value = (.65, .49, .22, 1)
            shader.inputs['Roughness'].default_value = .7
        obj.data.materials.append(mat)
        objects.append({'name': obj.name, 'vertices': len(obj.data.vertices),
                        'triangles': len(obj.data.polygons), 'uv_layers': len(obj.data.uv_layers)})
    view_direction = Vector((math.cos(.65)*math.cos(.2), math.cos(.65)*math.sin(.2), math.sin(.65)))
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            area.spaces.active.clip_end = 200000
            area.spaces.active.region_3d.view_location = (0, 0, 0)
            area.spaces.active.region_3d.view_distance = 13000
            area.spaces.active.region_3d.view_rotation = view_direction.to_track_quat('Z', 'Y')
    for obj in scene.objects:
        obj.select_set(False)
    scene['contract'] = 'P2 unchanged geometry. Top UV=equirectangular; Coast OBJ UV=(height,1-weight).'
    (OUT / 'blender_p02_import.json').write_text(json.dumps({'version': bpy.app.version_string,
        'scene': SCENE, 'objects': objects, 'axis': 'Forward Y / Up Z; scale 1'}, indent=2), encoding='utf-8')
    print('P02_IMPORT_OK', len(objects), sum(o['triangles'] for o in objects))


def export_scene():
    assert bpy.context.scene.name == SCENE
    EXPORT.mkdir(parents=True, exist_ok=True)
    props = bpy.ops.wm.obj_export.get_rna_type().properties
    for prop, value in [('forward_axis', 'Y'), ('up_axis', 'Z')]:
        assert value in [i.identifier for i in props[prop].enum_items]
    outputs = []
    for obj in bpy.context.scene.objects:
        if 'bright_globe_source' not in obj:
            continue
        for other in bpy.context.scene.objects:
            other.select_set(False)
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        path = EXPORT / obj['bright_globe_source']
        assert not path.exists(), '기존 출력은 덮어쓰기 전에 확인해야 함'
        bpy.ops.wm.obj_export(filepath=str(path), export_selected_objects=True,
            forward_axis='Y', up_axis='Z', global_scale=1., apply_modifiers=False,
            export_uv=True, export_normals=True, export_materials=False,
            export_triangulated_mesh=False, check_existing=False)
        outputs.append(str(path))
    blend = ART / 'Blender/FF_BrightGlobe_P02_roundtrip_v001.blend'
    blend.parent.mkdir(parents=True, exist_ok=True)
    assert not blend.exists()
    bpy.ops.file.pack_all()
    bpy.ops.wm.save_as_mainfile(filepath=str(blend), check_existing=False)
    (OUT / 'blender_p02_export.json').write_text(json.dumps({'blend': str(blend), 'outputs': outputs,
        'geometry_changed': False, 'modifiers_applied': False, 'axis': 'Y/Z', 'global_scale': 1}, indent=2), encoding='utf-8')
    print('P02_EXPORT_OK', len(outputs), str(blend))
