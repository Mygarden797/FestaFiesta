"""Create the isolated, non-authoritative placement overlay material in UE 5.7.

Only this new asset is written. Source facility materials, maps and post-process
settings are untouched. Repeat invocation does not save an existing completed asset.
"""
import unreal

FOLDER = '/Game/FestaFiesta/Building/FF_WBP_Building/Controls/Clarity'
NAME = 'M_BuildPlacementFeedback'
PATH = FOLDER + '/' + NAME
VERSION_KEY = 'FF.BuildPlacementFeedback.Version'
material = unreal.load_asset(PATH) if unreal.EditorAssetLibrary.does_asset_exist(PATH) else None
if material and unreal.EditorAssetLibrary.get_metadata_tag(material, VERSION_KEY) == '1':
    unreal.log('FF_BUILD_PLACEMENT_FEEDBACK_ALREADY_APPLIED')
else:
    if material:
        raise RuntimeError('Existing placement feedback material lacks the expected ownership marker: ' + PATH)
    unreal.EditorAssetLibrary.make_directory(FOLDER)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    assert material
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property('two_sided', True)
    material.set_editor_property('disable_depth_test', True)
    lib = unreal.MaterialEditingLibrary
    color = lib.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -240, 0)
    color.set_editor_property('parameter_name', 'StatusColor')
    color.set_editor_property('default_value', unreal.LinearColor(0.04, 0.65, 0.25, 1.0))
    opacity = lib.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -240, 160)
    opacity.set_editor_property('parameter_name', 'Opacity')
    opacity.set_editor_property('default_value', 0.24)
    assert lib.connect_material_property(color, 'RGB', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert lib.connect_material_property(opacity, '', unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(material)
    unreal.EditorAssetLibrary.set_metadata_tag(material, VERSION_KEY, '1')
    assert unreal.EditorAssetLibrary.save_loaded_asset(material)
    unreal.log('FF_BUILD_PLACEMENT_FEEDBACK_PASS')
