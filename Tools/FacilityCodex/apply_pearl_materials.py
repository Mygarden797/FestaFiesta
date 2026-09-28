# apply_pearl_materials.py
# 역할: 시설 조합 도감과 보상 표시 작업에서 지정된 대상에 변경을 적용하고 결과를 확인한다.

"""Update only the owned codex glass base's known shader to the pearl palette.

Run in an idle Entry editor after the outer migration has backed up the base.
All material-instance parameters, graph wiring and reward materials remain
untouched. Repeat runs validate the saved version without saving it again.
"""
import hashlib
from pathlib import Path
import runpy
import unreal


SOURCE = runpy.run_path(str(Path(__file__).with_name("create_glass_materials.py")))
BASE_PATH = SOURCE["FOLDER"] + "/" + SOURCE["BASE_NAME"]
OWNER_KEY = SOURCE["VERSION_KEY"]
OWNER_VERSION = SOURCE["VERSION"]
VERSION_KEY = "FF.FacilityCodex.Pearl20260921.1"
VERSION = "1"
# The contrast migration changed MI scalar values only; its base has this same code.
ORIGINAL_SHADER_SHA256 = "86b9c40812124596e8aa4f892daa4b230b06bf78ada73113ed7a2b9489139961"
PEARL_SHADER_SHA256 = "7b4f550deb5b88531c7e85b3b87692f3174e45911fa7a6fcf239213027b3706e"


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def shader_digest(code):
    return hashlib.sha256(code.replace("\r\n", "\n").strip().encode("utf-8")).hexdigest()


def validate_graph(material):
    """Follow the saved output wiring, without opening a material editor."""
    library = unreal.MaterialEditingLibrary
    require(library.get_num_material_expressions(material) == 11, "Glass expression count changed")
    nodes = []
    for prop, channels in ((unreal.MaterialProperty.MP_EMISSIVE_COLOR, (True, True, True, False)),
                           (unreal.MaterialProperty.MP_OPACITY, (False, False, False, True))):
        mask = library.get_material_property_input_node(material, prop)
        require(isinstance(mask, unreal.MaterialExpressionComponentMask), "Glass output mask is missing")
        require(tuple(mask.get_editor_property(name) for name in ("r", "g", "b", "a")) == channels,
                "Glass output channels changed")
        inputs = list(library.get_inputs_for_material_expression(material, mask))
        require(len(inputs) == 1 and isinstance(inputs[0], unreal.MaterialExpressionCustom),
                "Glass output no longer uses its custom shader")
        nodes.append(inputs[0])
    require(nodes[0] == nodes[1] and nodes[0].get_outer() == material, "Glass outputs use different shader nodes")
    custom = nodes[0]
    require(custom.get_editor_property("output_type") == unreal.CustomMaterialOutputType.CMOT_FLOAT4,
            "Glass custom shader output type changed")
    names = [str(pin.get_editor_property("input_name")) for pin in custom.get_editor_property("inputs")]
    require(names == ["UV", "Clock"] + list(SOURCE["DEFAULTS"]), "Glass shader input names changed")
    inputs = list(library.get_inputs_for_material_expression(material, custom))
    require(len(inputs) == len(names) and all(inputs), "Glass shader input wiring changed")
    require(len({node.get_path_name() for node in inputs}) == len(inputs), "Glass shader inputs are no longer distinct")
    require(isinstance(inputs[0], unreal.MaterialExpressionTextureCoordinate) and
            isinstance(inputs[1], unreal.MaterialExpressionTime), "Glass UV/time sources changed")
    for name, node in zip(names[2:], inputs[2:]):
        require(isinstance(node, unreal.MaterialExpressionScalarParameter) and
                str(node.get_editor_property("parameter_name")) == name, "Glass scalar source changed: " + name)
        require(abs(node.get_editor_property("default_value") - SOURCE["DEFAULTS"][name]) < 0.0001,
                "Glass default scalar changed: " + name)
    return custom


def preflight():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    require(world and world.get_path_name().startswith("/Engine/Maps/Entry."), "Pearl patch requires an idle Entry editor")
    require(not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Stop PIE before the pearl patch")
    require(shader_digest(SOURCE["SHADER"]) == PEARL_SHADER_SHA256, "Canonical pearl shader differs from this bounded patch version")
    material = unreal.load_asset(BASE_PATH)
    require(isinstance(material, unreal.Material), "Existing glass base material is missing")
    require(unreal.EditorAssetLibrary.get_metadata_tag(material, OWNER_KEY) == OWNER_VERSION,
            "Glass base ownership marker is missing")
    require(material.get_editor_property("material_domain") == unreal.MaterialDomain.MD_UI and
            material.get_editor_property("blend_mode") == unreal.BlendMode.BLEND_TRANSLUCENT,
            "Glass base must remain a translucent UI material")
    marker = unreal.EditorAssetLibrary.get_metadata_tag(material, VERSION_KEY)
    require(marker in ("", VERSION), "Unknown pearl migration version")
    custom = validate_graph(material)
    code = custom.get_editor_property("code")
    digest = shader_digest(code)
    require(digest in (ORIGINAL_SHADER_SHA256, PEARL_SHADER_SHA256),
            "Refusing to replace an unknown glass shader: " + digest)
    if marker == VERSION:
        require(digest == PEARL_SHADER_SHA256, "Saved pearl marker does not match its shader")
    return material, custom, marker, code


def apply_pearl_materials():
    material, custom, marker, before = preflight()
    if marker == VERSION:
        unreal.log("FF_CODEX_PEARL_MATERIALS_PASS updated=0 instances_unchanged=true")
        return material
    library = unreal.MaterialEditingLibrary
    assets = unreal.EditorAssetLibrary
    try:
        custom.set_editor_property("code", SOURCE["SHADER"])
        library.recompile_material(material)
        require(shader_digest(validate_graph(material).get_editor_property("code")) == PEARL_SHADER_SHA256,
                "Pearl shader verification failed")
        assets.set_metadata_tag(material, VERSION_KEY, VERSION)
        require(assets.save_loaded_asset(material, only_if_is_dirty=False), "Could not save pearl glass base")
        require(assets.get_metadata_tag(material, OWNER_KEY) == OWNER_VERSION and
                assets.get_metadata_tag(material, VERSION_KEY) == VERSION, "Glass version metadata did not persist")
    except Exception as error:
        try:
            custom.set_editor_property("code", before)
            library.recompile_material(material)
            if marker:
                assets.set_metadata_tag(material, VERSION_KEY, marker)
            else:
                assets.remove_metadata_tag(material, VERSION_KEY)
            require(custom.get_editor_property("code") == before, "Original shader did not restore")
            require(assets.save_loaded_asset(material, only_if_is_dirty=False), "Rollback save failed")
        except Exception as rollback_error:
            raise RuntimeError("Pearl update failed: {}. Restore the outer migration's glass base backup: {}".format(
                error, rollback_error)) from error
        raise RuntimeError("Pearl update failed; original glass base was restored: " + str(error)) from error
    unreal.log("FF_CODEX_PEARL_MATERIALS_PASS updated=1 instances_unchanged=true")
    return material


if __name__ == "__main__":
    apply_pearl_materials()
