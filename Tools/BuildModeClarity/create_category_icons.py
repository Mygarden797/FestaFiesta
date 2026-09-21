"""Create original SDF pictograms without modifying any existing icon artwork.

Run before FF.ApplyBuildModeClarityCatalog. The project already always-cooks
/Game/FestaFiesta/UI; all new assets stay in its owned Building/Clarity folder.
"""
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
FOLDER = '/Game/FestaFiesta/UI/Building/Clarity'
SOURCE = ROOT / 'Tools/BuildModeClarity/shaders/CategoryPictograms.ush'
GLYPHS = ('All', 'Booth', 'Toilet', 'Info', 'Rest', 'Performance', 'Safety',
          'PhotoSpot', 'Lighting', 'Road', 'Tile')
lib = unreal.MaterialEditingLibrary
assets = unreal.AssetToolsHelpers.get_asset_tools()
unreal.EditorAssetLibrary.make_directory(FOLDER)
code = SOURCE.read_text(encoding='utf-8')
material_path = FOLDER + '/M_UI_CategoryClarity'
material = unreal.load_asset(material_path) if unreal.EditorAssetLibrary.does_asset_exist(material_path) else None
if not material:
    material = assets.create_asset('M_UI_CategoryClarity', FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    uv = lib.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -500, 0)
    glyph = lib.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -500, 160)
    glyph.set_editor_property('parameter_name', 'Glyph')
    node = lib.create_material_expression(material, unreal.MaterialExpressionCustom, -160, 0)
    node.set_editor_property('description', 'Original festival schematic pictograms / SDF line coverage')
    node.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    pins = []
    for name in ('UV', 'Glyph'):
        pin = unreal.CustomInput()
        pin.set_editor_property('input_name', name)
        pins.append(pin)
    node.set_editor_property('inputs', pins)
    node.set_editor_property('code', code)
    white = lib.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -160, 220)
    white.set_editor_property('constant', unreal.LinearColor(1., 1., 1., 1.))
    assert lib.connect_material_expressions(uv, '', node, 'UV')
    assert lib.connect_material_expressions(glyph, '', node, 'Glyph')
    assert lib.connect_material_property(node, '', unreal.MaterialProperty.MP_OPACITY)
    assert lib.connect_material_property(white, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)

nodes = [node for node in unreal.ObjectIterator(unreal.MaterialExpressionCustom) if node.get_outer() == material]
assert len(nodes) == 1, 'Expected the single category pictogram shader'
if nodes[0].get_editor_property('code') != code:
    nodes[0].set_editor_property('code', code)
    lib.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)

for index, name in enumerate(GLYPHS):
    asset_name = 'MI_UI_CategoryClarity_' + name
    if not unreal.EditorAssetLibrary.does_asset_exist(FOLDER + '/' + asset_name):
        instance = assets.create_asset(asset_name, FOLDER, unreal.MaterialInstanceConstant,
                                       unreal.MaterialInstanceConstantFactoryNew())
        lib.set_material_instance_parent(instance, material)
        lib.set_material_instance_scalar_parameter_value(instance, 'Glyph', float(index))
        unreal.EditorAssetLibrary.save_loaded_asset(instance)
unreal.log('FF_BUILD_CLARITY_CATEGORY_ICONS_PASS')
