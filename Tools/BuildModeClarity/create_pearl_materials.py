"""Create seven static, resolution-aware UMG pearl surfaces for the build interface."""
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
folder = '/Game/FestaFiesta/Building/FF_WBP_Building/Controls/Pearl'
code = (root / 'Tools/BuildModeClarity/shaders/BuildPearl.ush').read_text(encoding='utf-8')
lib = unreal.MaterialEditingLibrary
assets = unreal.AssetToolsHelpers.get_asset_tools()
unreal.EditorAssetLibrary.make_directory(folder)
material = unreal.load_asset(folder + '/M_UI_BuildPearl')
if not material:
    material = assets.create_asset('M_UI_BuildPearl', folder, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    uv = lib.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -500, 0)
    role = lib.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -500, 100)
    role.set_editor_property('parameter_name', 'SurfaceRole')
    custom = lib.create_material_expression(material, unreal.MaterialExpressionCustom, -200, 0)
    custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    pins = []
    for name in ('UV', 'Role'):
        pin = unreal.CustomInput(); pin.set_editor_property('input_name', name); pins.append(pin)
    custom.set_editor_property('inputs', pins)
    custom.set_editor_property('code', code)
    assert lib.connect_material_expressions(uv, '', custom, 'UV')
    assert lib.connect_material_expressions(role, '', custom, 'Role')
    for channel, output in ((False, unreal.MaterialProperty.MP_EMISSIVE_COLOR), (True, unreal.MaterialProperty.MP_OPACITY)):
        mask = lib.create_material_expression(material, unreal.MaterialExpressionComponentMask, 100, 100 if channel else 0)
        for key in ('r', 'g', 'b'): mask.set_editor_property(key, not channel)
        mask.set_editor_property('a', channel)
        assert lib.connect_material_expressions(custom, '', mask, '')
        assert lib.connect_material_property(mask, '', output)
custom = next(n for n in unreal.ObjectIterator(unreal.MaterialExpressionCustom) if n.get_outer() == material)
custom.set_editor_property('code', code)
lib.recompile_material(material)
unreal.EditorAssetLibrary.save_loaded_asset(material)
for index, name in enumerate(('Panel', 'Card', 'Dark', 'Hero', 'Rim', 'Stamp', 'Sea')):
    asset_name = 'MI_UI_BuildPearl_' + name
    instance = unreal.load_asset(folder + '/' + asset_name)
    if not instance:
        instance = assets.create_asset(asset_name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    lib.set_material_instance_parent(instance, material)
    lib.set_material_instance_scalar_parameter_value(instance, 'SurfaceRole', float(index))
    unreal.EditorAssetLibrary.save_loaded_asset(instance)
unreal.log('FF_BUILD_PEARL_MATERIALS_PASS')
