# apply_contrast_materials.py
# 역할: 시설 조합 도감과 보상 표시 작업에서 지정된 대상에 변경을 적용하고 결과를 확인한다.

"""Refine only the six existing codex glass material instances.

The base shader, art palette, motion, discovery materials and reward materials
are unchanged. Run in an idle Entry editor. The outer contrast migration owns
filesystem backups; this bounded patch also restores touched instance values
and metadata if mutation, verification or saving fails.
"""
import math
import unreal


FOLDER = "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/Materials"
OWNER_KEY = "FF.FacilityCodex.GlassSurfaceVersion"
OWNER_VERSION = "1"
VERSION_KEY = "FF.FacilityCodex.Contrast20260921.1"
VERSION = "1"
BASE_PATH = FOLDER + "/M_FF_CodexGlass"
OVERRIDES = {
    "Screen": {"SurfaceOpacity": 0.86},
    "Header": {"SurfaceOpacity": 0.96, "EdgeWidthPixels": 1.5},
    "Card": {"SurfaceOpacity": 0.90, "RimStrength": 0.34},
    "Selection": {"SurfaceOpacity": 0.0, "RimStrength": 1.0, "EdgeWidthPixels": 3.2},
    "Detail": {"SurfaceOpacity": 0.94, "EdgeWidthPixels": 1.5},
    "Inset": {"SurfaceOpacity": 0.82},
}
PARAMETERS = ("SurfaceRole", "SurfaceOpacity", "CornerRadiusPixels", "EdgeWidthPixels", "RimStrength", "MotionStrength")
TOLERANCE = 0.0001


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def scalar_values(asset):
    return {name: unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(asset, name)
            for name in PARAMETERS}


def verify_values(role, asset, preserved=None):
    values = scalar_values(asset)
    require(all(math.isfinite(value) for value in values.values()), "Non-finite glass parameter: " + role)
    for name, expected in OVERRIDES[role].items():
        require(abs(values[name] - expected) < TOLERANCE,
                "Contrast parameter mismatch: {}.{} = {} (expected {})".format(role, name, values[name], expected))
    if preserved is not None:
        for name, expected in preserved.items():
            if name not in OVERRIDES[role]:
                require(abs(values[name] - expected) < TOLERANCE,
                        "Unrelated glass parameter changed: {}.{}".format(role, name))


def preflight():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    require(world and world.get_path_name().startswith("/Engine/Maps/Entry."), "Contrast patch requires an idle Entry editor")
    require(not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Stop PIE before the contrast patch")
    base = unreal.load_asset(BASE_PATH)
    require(isinstance(base, unreal.Material), "Existing glass base material is missing")
    require(unreal.EditorAssetLibrary.get_metadata_tag(base, OWNER_KEY) == OWNER_VERSION, "Glass base ownership marker is missing")
    require(base.get_editor_property("material_domain") == unreal.MaterialDomain.MD_UI and
            base.get_editor_property("blend_mode") == unreal.BlendMode.BLEND_TRANSLUCENT,
            "Glass base must remain a translucent UI material")
    result = {}
    # Validate every destination before changing any instance.
    for index, role in enumerate(OVERRIDES):
        path = FOLDER + "/MI_FF_CodexGlass_" + role
        asset = unreal.load_asset(path)
        require(isinstance(asset, unreal.MaterialInstanceConstant), "Existing glass instance is missing: " + path)
        require(unreal.EditorAssetLibrary.get_metadata_tag(asset, OWNER_KEY) == OWNER_VERSION,
                "Refusing to edit an unowned glass instance: " + path)
        require(asset.get_editor_property("parent") == base, "Glass instance has an unexpected parent: " + path)
        marker = unreal.EditorAssetLibrary.get_metadata_tag(asset, VERSION_KEY)
        require(marker in ("", VERSION), "Unknown contrast migration version: " + path)
        values = scalar_values(asset)
        require(all(math.isfinite(value) for value in values.values()), "Non-finite source glass parameter: " + role)
        require(abs(values["SurfaceRole"] - index) < TOLERANCE, "Glass instance has an unexpected role: " + role)
        if marker == VERSION:
            verify_values(role, asset)
        result[role] = (asset, marker, values)
    return result


def apply_contrast_materials():
    instances = preflight()
    touched = []
    library = unreal.MaterialEditingLibrary
    try:
        for role, (asset, marker, before) in instances.items():
            if marker == VERSION:
                continue
            # Struct.copy() preserves the entire scalar override, including expression GUIDs
            # and editor-only atlas data, rather than reconstructing only its float value.
            original_overrides = [value.copy() for value in asset.get_editor_property("scalar_parameter_values")]
            touched.append((role, asset, marker, original_overrides, before))
            for parameter, value in OVERRIDES[role].items():
                # UE's Python setter returns None; verify the resulting value, not a return flag.
                library.set_material_instance_scalar_parameter_value(asset, parameter, float(value))
            library.update_material_instance(asset)
            verify_values(role, asset, before)
            unreal.EditorAssetLibrary.set_metadata_tag(asset, VERSION_KEY, VERSION)
        # Save after all in-memory changes have passed validation.
        for role, asset, _, _, before in touched:
            require(unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False),
                    "Could not save contrast instance: " + role)
            verify_values(role, asset, before)
    except Exception as error:
        rollback_errors = []
        for role, asset, marker, original_overrides, before in reversed(touched):
            try:
                asset.set_editor_property("scalar_parameter_values", original_overrides)
                library.update_material_instance(asset)
                if marker:
                    unreal.EditorAssetLibrary.set_metadata_tag(asset, VERSION_KEY, marker)
                else:
                    unreal.EditorAssetLibrary.remove_metadata_tag(asset, VERSION_KEY)
                require(all(abs(scalar_values(asset)[name] - value) < TOLERANCE for name, value in before.items()),
                        "Original scalar values did not restore")
                require(unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False), "Rollback save failed")
            except Exception as rollback_error:
                rollback_errors.append("{}: {}".format(role, rollback_error))
        if rollback_errors:
            raise RuntimeError("Contrast update failed: {}. Restore outer migration backups for: {}".format(
                error, "; ".join(rollback_errors))) from error
        raise RuntimeError("Contrast update failed; touched instances were restored: " + str(error)) from error

    for role, (asset, _, before) in instances.items():
        require(unreal.EditorAssetLibrary.get_metadata_tag(asset, VERSION_KEY) == VERSION, "Contrast marker missing: " + role)
        verify_values(role, asset, before)
    unreal.log("FF_CODEX_CONTRAST_MATERIALS_PASS updated={} preserved_motion=true".format(len(touched)))
    return {role: asset for role, (asset, _, _) in instances.items()}


if __name__ == "__main__":
    apply_contrast_materials()
