# create_materials.py
# 역할: 건설 화면의 배치·색·가독성 작업에서 필요한 에셋 또는 설정을 생성한다.

"""Small, static UI materials: matte sage glass, thin-film rim and a sampled SDF wheel.

Run in UE 5.7. This writes only the new Building/Clarity assets. It does not change
the global palette, font settings, world materials, or existing Aurora assets.
"""
import math
import struct
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
FOLDER = '/Game/FestaFiesta/Building/FF_WBP_Building/Controls/Clarity'
shader_code = (ROOT / 'Tools/BuildModeClarity/shaders/BuildSurface.ush').read_text(encoding='utf-8')
SOURCE = ROOT / 'SourceAssets/UI/Building/Clarity/T_UI_BuildCircleSDF.tga'
SOURCE.parent.mkdir(parents=True, exist_ok=True)
SIZE = 128
if not SOURCE.exists():
    # Linear signed distance, 0.5 at the circle edge. RGB storage avoids alpha compression.
    data = bytearray()
    for y in range(SIZE):
        for x in range(SIZE):
            distance = math.hypot((x + .5) / SIZE - .5, (y + .5) / SIZE - .5) - .40
            value = round(255 * max(0., min(1., .5 + distance * 2.)))
            data.extend((value, value, value))
    header = struct.pack('<BBBHHBHHHHBB', 0, 0, 2, 0, 0, 0, 0, 0, SIZE, SIZE, 24, 0x20)
    SOURCE.write_bytes(header + data)

lib = unreal.MaterialEditingLibrary
assets = unreal.AssetToolsHelpers.get_asset_tools()
unreal.EditorAssetLibrary.make_directory(FOLDER)
texture = unreal.load_asset(FOLDER + '/T_UI_BuildCircleSDF')
if not texture:
    task = unreal.AssetImportTask()
    task.filename = str(SOURCE)
    task.destination_path = FOLDER
    task.automated = True
    task.save = False
    assets.import_asset_tasks([task])
    texture = unreal.load_asset(FOLDER + '/T_UI_BuildCircleSDF')
    assert texture
    texture.set_editor_property('srgb', False)
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_GRAYSCALE)
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)

material = unreal.load_asset(FOLDER + '/M_UI_BuildClarity')
if not material:
    material = assets.create_asset('M_UI_BuildClarity', FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    uv = lib.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -600, 0)
    sdf = lib.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -600, 140)
    sdf.set_editor_property('parameter_name', 'ShapeSDF')
    sdf.set_editor_property('texture', texture)
    sdf.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
    role = lib.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -600, 320)
    role.set_editor_property('parameter_name', 'SurfaceRole')
    aspect = lib.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -600, 420)
    aspect.set_editor_property('parameter_name', 'Aspect')
    aspect.set_editor_property('default_value', 4.0)
    node = lib.create_material_expression(material, unreal.MaterialExpressionCustom, -180, 0)
    node.set_editor_property('description', 'Clarity: static optical thin-film rim / SDF antialiasing')
    node.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    pins = []
    for name in ('UV', 'SDF', 'Role', 'Aspect'):
        pin = unreal.CustomInput(); pin.set_editor_property('input_name', name); pins.append(pin)
    node.set_editor_property('inputs', pins)
    node.set_editor_property('code', shader_code)
    for expression, output, name in ((uv, '', 'UV'), (sdf, 'RGB', 'SDF'), (role, '', 'Role'), (aspect, '', 'Aspect')):
        assert lib.connect_material_expressions(expression, output, node, name)
    rgb = lib.create_material_expression(material, unreal.MaterialExpressionComponentMask, 100, 0)
    alpha = lib.create_material_expression(material, unreal.MaterialExpressionComponentMask, 100, 100)
    for name in ('r', 'g', 'b'):
        rgb.set_editor_property(name, True); alpha.set_editor_property(name, False)
    rgb.set_editor_property('a', False); alpha.set_editor_property('a', True)
    assert lib.connect_material_expressions(node, '', rgb, '')
    assert lib.connect_material_expressions(node, '', alpha, '')
    assert lib.connect_material_property(rgb, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert lib.connect_material_property(alpha, '', unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)

# Update only our own material when its authored shader differs; repeat runs do not resave it.
custom_nodes = [n for n in unreal.ObjectIterator(unreal.MaterialExpressionCustom) if n.get_outer() == material]
assert len(custom_nodes) == 1, 'Expected the single Clarity UI shader'
if custom_nodes[0].get_editor_property('code') != shader_code:
    custom_nodes[0].set_editor_property('code', shader_code)
    lib.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)

for name, role, aspect in [('Paper',0,2),('Header',1,6),('Action',3,3),('Dark',4,3),('Gold',5,4),('Ring',6,1),('Program',7,2),('Required',8,2)]:
    asset_name = 'MI_UI_BuildClarity_' + name
    if not unreal.EditorAssetLibrary.does_asset_exist(FOLDER + '/' + asset_name):
        instance = assets.create_asset(asset_name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        lib.set_material_instance_parent(instance, material)
        lib.set_material_instance_scalar_parameter_value(instance, 'SurfaceRole', float(role))
        lib.set_material_instance_scalar_parameter_value(instance, 'Aspect', float(aspect))
        unreal.EditorAssetLibrary.save_loaded_asset(instance)
unreal.log('FF_BUILD_CLARITY_MATERIALS_PASS')
