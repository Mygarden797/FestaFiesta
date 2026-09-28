# apply_reward_affordance_materials.py
# 역할: 시설 조합 도감과 보상 표시 작업에서 지정된 대상에 변경을 적용하고 결과를 확인한다.

"""Refine the owned reward shader and author one grayscale locked-gift instance.

Run in idle Entry after the outer migration has backed up the reward base.
The four existing reward instances and the discovery shader are read-only.
Repeat runs validate without resaving; failures restore only this patch's work.
"""
import hashlib
from pathlib import Path
import runpy
import unreal


SOURCE = runpy.run_path(str(Path(__file__).with_name("create_reward_materials.py")))
FOLDER = SOURCE["FOLDER"]
BASE_PATH = FOLDER + "/" + SOURCE["BASE_NAME"]
LOCKED_NAME = "MI_FF_CodexReward_LockedGift"
LOCKED_PATH = FOLDER + "/" + LOCKED_NAME
THUMBNAIL_PATH = FOLDER + "/M_FF_Codex_DiscoveryThumbnail"
GIFT_PATH = FOLDER.rsplit("/", 1)[0] + "/Icons/T_FF_Codex_RewardGift"
VERSION_KEY = "FF.FacilityCodex.RewardAffordance20260921.1"
VERSION = "1"
PREVIOUS_SHA256 = "3c6ea010c7783919a05066e9fe101bde5cf231e1cffc538516bfe1f7e270290b"
TARGET_SHA256 = "1eb495b7926f97c531263e6af356dd9d5e4d8a330105a930f79c7afecd0d99ff"
THUMBNAIL_SHA256 = "d846893bb40e913db2bb89b4c0b9006b8b6fd1c8120d05f42d2cfbaea836079a"


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(code):
    return hashlib.sha256(code.replace("\r\n", "\n").strip().encode("utf-8")).hexdigest()


def custom_node(material, names):
    library = unreal.MaterialEditingLibrary
    require(material.get_editor_property("material_domain") == unreal.MaterialDomain.MD_UI and
            material.get_editor_property("blend_mode") == unreal.BlendMode.BLEND_TRANSLUCENT,
            "Reward/discovery base must remain a translucent UI material")
    require(library.get_num_material_expressions(material) == len(names) + 3, "Material expression count changed")
    outputs = []
    for prop, channels in ((unreal.MaterialProperty.MP_EMISSIVE_COLOR, (True, True, True, False)),
                           (unreal.MaterialProperty.MP_OPACITY, (False, False, False, True))):
        mask = library.get_material_property_input_node(material, prop)
        require(isinstance(mask, unreal.MaterialExpressionComponentMask), "Material output mask is missing")
        require(tuple(mask.get_editor_property(name) for name in ("r", "g", "b", "a")) == channels,
                "Material output channels changed")
        inputs = list(library.get_inputs_for_material_expression(material, mask))
        require(len(inputs) == 1 and isinstance(inputs[0], unreal.MaterialExpressionCustom), "Custom shader output is missing")
        outputs.append(inputs[0])
    require(outputs[0] == outputs[1] and outputs[0].get_outer() == material, "Material outputs use different shaders")
    custom = outputs[0]
    require(custom.get_editor_property("output_type") == unreal.CustomMaterialOutputType.CMOT_FLOAT4, "Custom shader output type changed")
    actual = [str(pin.get_editor_property("input_name")) for pin in custom.get_editor_property("inputs")]
    require(actual == names, "Custom shader input names changed")
    inputs = list(library.get_inputs_for_material_expression(material, custom))
    require(len(inputs) == len(names) and all(inputs), "Custom shader input wiring changed")
    require(len({node.get_path_name() for node in inputs}) == len(inputs), "Shader input sources are no longer distinct")
    return custom, inputs


def verify_locked(instance, parent, gift, texel):
    library = unreal.MaterialEditingLibrary
    require(isinstance(instance, unreal.MaterialInstanceConstant), "Locked gift must be a material instance")
    require(unreal.EditorAssetLibrary.get_metadata_tag(instance, VERSION_KEY) == VERSION, "Locked gift ownership marker is missing")
    require(instance.get_editor_property("parent") == parent, "Locked gift parent changed")
    require(library.get_material_instance_texture_parameter_value(instance, "ThumbnailTexture") == gift, "Locked gift texture changed")
    require(abs(library.get_material_instance_scalar_parameter_value(instance, "VisualState") - 1.0) < 0.0001,
            "Locked gift must use true grayscale VisualState=1")
    actual = library.get_material_instance_vector_parameter_value(instance, "TexelSize")
    require(all(abs(getattr(actual, channel) - getattr(texel, channel)) < 0.000001 for channel in ("r", "g", "b", "a")),
            "Locked gift texel size changed")


def preflight():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    require(world and world.get_path_name().startswith("/Engine/Maps/Entry."), "Reward affordance patch requires idle Entry")
    require(not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Stop PIE before the reward patch")
    require(digest(SOURCE["SHADER"]) == TARGET_SHA256, "Canonical reward shader differs from this patch version")
    assets = unreal.EditorAssetLibrary
    material = unreal.load_asset(BASE_PATH)
    require(isinstance(material, unreal.Material), "Existing reward base is missing")
    require(assets.get_metadata_tag(material, SOURCE["VERSION_KEY"]) == SOURCE["VERSION"], "Reward base ownership marker is missing")
    custom, inputs = custom_node(material, ["UV", "Clock"] + list(SOURCE["DEFAULTS"]))
    require(isinstance(inputs[0], unreal.MaterialExpressionTextureCoordinate) and
            isinstance(inputs[1], unreal.MaterialExpressionTime), "Reward UV/time sources changed")
    for name, node in zip(SOURCE["DEFAULTS"], inputs[2:]):
        require(isinstance(node, unreal.MaterialExpressionScalarParameter) and
                str(node.get_editor_property("parameter_name")) == name, "Reward scalar source changed: " + name)
        require(abs(node.get_editor_property("default_value") - SOURCE["DEFAULTS"][name]) < 0.0001, "Reward scalar default changed: " + name)
    before = custom.get_editor_property("code")
    require(digest(before) in (PREVIOUS_SHA256, TARGET_SHA256), "Refusing to replace an unknown reward shader")
    marker = assets.get_metadata_tag(material, VERSION_KEY)
    require(marker in ("", VERSION), "Unknown reward affordance version")
    require(marker != VERSION or digest(before) == TARGET_SHA256, "Reward affordance marker does not match its shader")
    for name in SOURCE["STATES"]:
        instance = unreal.load_asset(FOLDER + "/MI_FF_CodexReward_" + name)
        require(isinstance(instance, unreal.MaterialInstanceConstant) and instance.get_editor_property("parent") == material,
                "Existing reward state parent changed: " + name)
        require(assets.get_metadata_tag(instance, SOURCE["VERSION_KEY"]) == SOURCE["VERSION"], "Existing reward state is not owned: " + name)
    parent = unreal.load_asset(THUMBNAIL_PATH)
    require(isinstance(parent, unreal.Material) and
            assets.get_metadata_tag(parent, "FF.FacilityCodex.ThumbnailDiscoveryVersion") == "1", "Owned thumbnail parent is missing")
    thumbnail, inputs = custom_node(parent, ["UV", "ThumbnailTexture", "VisualState", "TexelSize"])
    require(digest(thumbnail.get_editor_property("code")) == THUMBNAIL_SHA256, "Discovery thumbnail shader changed")
    for node, cls, name in ((inputs[1], unreal.MaterialExpressionTextureObjectParameter, "ThumbnailTexture"),
                            (inputs[2], unreal.MaterialExpressionScalarParameter, "VisualState"),
                            (inputs[3], unreal.MaterialExpressionVectorParameter, "TexelSize")):
        require(isinstance(node, cls) and str(node.get_editor_property("parameter_name")) == name, "Thumbnail parameter contract changed: " + name)
    gift = unreal.load_asset(GIFT_PATH)
    require(isinstance(gift, unreal.Texture2D), "Existing transparent reward gift is missing")
    width, height = gift.blueprint_get_size_x(), gift.blueprint_get_size_y()
    require(width > 0 and height > 0, "Gift texture has invalid dimensions")
    texel = unreal.LinearColor(1.0 / width, 1.0 / height, 0.0, 0.0)
    locked = None
    if assets.does_asset_exist(LOCKED_PATH):
        locked = unreal.load_asset(LOCKED_PATH)
        verify_locked(locked, parent, gift, texel)
    return material, custom, marker, before, parent, gift, texel, locked


def apply_reward_affordance_materials():
    material, custom, marker, before, parent, gift, texel, locked = preflight()
    assets = unreal.EditorAssetLibrary
    library = unreal.MaterialEditingLibrary
    created = None
    touched = False
    try:
        if locked is None:
            created = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                LOCKED_NAME, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
            require(created is not None, "Could not create locked gift instance")
            locked = created
            library.set_material_instance_parent(locked, parent)
            library.set_material_instance_texture_parameter_value(locked, "ThumbnailTexture", gift)
            library.set_material_instance_scalar_parameter_value(locked, "VisualState", 1.0)
            library.set_material_instance_vector_parameter_value(locked, "TexelSize", texel)
            library.update_material_instance(locked)
            assets.set_metadata_tag(locked, VERSION_KEY, VERSION)
            verify_locked(locked, parent, gift, texel)
        if marker != VERSION:
            touched = True
            custom.set_editor_property("code", SOURCE["SHADER"])
            library.recompile_material(material)
            require(digest(custom.get_editor_property("code")) == TARGET_SHA256, "Reward shader update did not persist")
            assets.set_metadata_tag(material, VERSION_KEY, VERSION)
        if created:
            require(assets.save_loaded_asset(created, only_if_is_dirty=False), "Could not save locked gift instance")
        if touched:
            require(assets.save_loaded_asset(material, only_if_is_dirty=False), "Could not save reward shader")
        verify_locked(locked, parent, gift, texel)
        require(assets.get_metadata_tag(material, VERSION_KEY) == VERSION and
                digest(custom.get_editor_property("code")) == TARGET_SHA256, "Reward affordance verification failed")
    except Exception as error:
        rollback_errors = []
        if touched:
            try:
                custom.set_editor_property("code", before)
                library.recompile_material(material)
                if marker:
                    assets.set_metadata_tag(material, VERSION_KEY, marker)
                else:
                    assets.remove_metadata_tag(material, VERSION_KEY)
                require(custom.get_editor_property("code") == before, "Original reward shader did not restore")
                require(assets.save_loaded_asset(material, only_if_is_dirty=False), "Reward rollback save failed")
            except Exception as rollback_error:
                rollback_errors.append(str(rollback_error))
        if created:
            try:
                # Only the asset created by this invocation can be removed here.
                require(assets.delete_asset(LOCKED_PATH), "Could not remove this patch's new locked gift")
            except Exception as rollback_error:
                rollback_errors.append(str(rollback_error))
        if rollback_errors:
            raise RuntimeError("Reward patch failed: {}. Restore outer migration backups: {}".format(
                error, "; ".join(rollback_errors))) from error
        raise RuntimeError("Reward patch failed; touched assets were restored: " + str(error)) from error
    unreal.log("FF_CODEX_REWARD_AFFORDANCE_MATERIALS_PASS base_updated={} locked_created={}".format(int(touched), int(created is not None)))
    return {"Base": material, "LockedGift": locked}


if __name__ == "__main__":
    apply_reward_affordance_materials()
