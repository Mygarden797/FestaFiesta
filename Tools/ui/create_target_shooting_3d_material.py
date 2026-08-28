"""Create the authored lit material used by the TargetShooting UMG 3D viewport."""

import unreal


ASSET_PATH = "/Game/FestaFiesta/UI/Minigame/UIUX_v2"
ASSET_NAME = "M_MG_TargetShooting_Arena3D"
OBJECT_PATH = f"{ASSET_PATH}/{ASSET_NAME}.{ASSET_NAME}"


def create_or_rebuild_material() -> unreal.Material:
    material = unreal.load_asset(OBJECT_PATH)
    if material is None:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            ASSET_NAME,
            ASSET_PATH,
            unreal.Material,
            unreal.MaterialFactoryNew(),
        )
    if material is None:
        raise RuntimeError(f"Failed to create {OBJECT_PATH}")

    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("two_sided", False)

    color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVectorParameter, -520, -80
    )
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(0.04, 0.12, 0.18, 1.0))

    roughness = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, -520, 160
    )
    roughness.set_editor_property("parameter_name", "Roughness")
    roughness.set_editor_property("default_value", 0.34)

    metallic = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, -520, 270
    )
    metallic.set_editor_property("parameter_name", "Metallic")
    metallic.set_editor_property("default_value", 0.18)

    emissive_strength = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, -520, 40
    )
    emissive_strength.set_editor_property("parameter_name", "EmissiveStrength")
    emissive_strength.set_editor_property("default_value", 0.0)

    emissive = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -250, 20
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(color, "", emissive, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(
        emissive_strength, "", emissive, "B"
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        color, "", unreal.MaterialProperty.MP_BASE_COLOR
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness, "", unreal.MaterialProperty.MP_ROUGHNESS
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        metallic, "", unreal.MaterialProperty.MP_METALLIC
    )

    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    material.modify()
    unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    return material


created = create_or_rebuild_material()
unreal.log(f"FF_TARGET_SHOOTING_3D_MATERIAL_OK {created.get_path_name()}")

