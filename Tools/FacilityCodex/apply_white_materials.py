"""Apply the white codex palette to two existing bases and the Screen opacity.

The outer migration backs up these three assets before running in idle Entry.
This patch preserves all other instance values and restores touched assets on
failure. Repeated calls validate the existing result without saving it again.
"""
import math
from pathlib import Path
import runpy
import unreal


DIRECTORY = Path(__file__).parent
PEARL_HELPERS = runpy.run_path(str(DIRECTORY / "apply_pearl_materials.py"))
require = PEARL_HELPERS["require"]
shader_digest = PEARL_HELPERS["shader_digest"]
GLASS = PEARL_HELPERS["SOURCE"]
SHOWCASE = runpy.run_path(str(DIRECTORY / "create_showcase_materials.py"))
VERSION_KEY = "FF.FacilityCodex.White20260921.1"
VERSION = "1"
SHADERS = (
    (GLASS, "7b4f550deb5b88531c7e85b3b87692f3174e45911fa7a6fcf239213027b3706e",
     "cfa2ddd96ddf5f392a6e7b8a70fc24de3126a08d27d3ba5812736fa42388d0d4"),
    (SHOWCASE, "9bc30f44868344d141a13593bc4a6e4aa8a72847bf10aeda77bc010f26a0067e",
     "daa2e6673cd89e14fdee21fd27c5d6b3bddd668efc325ef3cb647cddd25edbd0"),
)


def validate_graph(material, source):
    if source is GLASS:
        return PEARL_HELPERS["validate_graph"](material)
    # Same proven graph contract as glass, with the showcase's eight scalars.
    library = unreal.MaterialEditingLibrary
    source["validate_base"](material, library)
    nodes = []
    for prop, channels in ((unreal.MaterialProperty.MP_EMISSIVE_COLOR, (True, True, True, False)),
                           (unreal.MaterialProperty.MP_OPACITY, (False, False, False, True))):
        mask = library.get_material_property_input_node(material, prop)
        require(isinstance(mask, unreal.MaterialExpressionComponentMask), "Showcase output mask is missing")
        require(tuple(mask.get_editor_property(name) for name in ("r", "g", "b", "a")) == channels,
                "Showcase output channels changed")
        inputs = list(library.get_inputs_for_material_expression(material, mask))
        require(len(inputs) == 1 and isinstance(inputs[0], unreal.MaterialExpressionCustom),
                "Showcase output no longer uses its custom shader")
        nodes.append(inputs[0])
    require(nodes[0] == nodes[1] and nodes[0].get_outer() == material, "Showcase outputs use different shader nodes")
    custom = nodes[0]
    require(custom.get_editor_property("output_type") == unreal.CustomMaterialOutputType.CMOT_FLOAT4,
            "Showcase custom shader output type changed")
    names = [str(pin.get_editor_property("input_name")) for pin in custom.get_editor_property("inputs")]
    require(names == ["UV", "Clock"] + list(source["DEFAULTS"]), "Showcase shader input names changed")
    inputs = list(library.get_inputs_for_material_expression(material, custom))
    require(len(inputs) == len(names) and all(inputs), "Showcase shader input wiring changed")
    require(len({node.get_path_name() for node in inputs}) == len(inputs), "Showcase shader inputs are no longer distinct")
    require(isinstance(inputs[0], unreal.MaterialExpressionTextureCoordinate) and
            isinstance(inputs[1], unreal.MaterialExpressionTime), "Showcase UV/time sources changed")
    for name, node in zip(names[2:], inputs[2:]):
        require(isinstance(node, unreal.MaterialExpressionScalarParameter) and
                str(node.get_editor_property("parameter_name")) == name, "Showcase scalar source changed: " + name)
        require(abs(node.get_editor_property("default_value") - source["DEFAULTS"][name]) < 0.0001,
                "Showcase default scalar changed: " + name)
    return custom


def scalar_values(instance):
    return {name: unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(instance, name)
            for name in GLASS["DEFAULTS"]}


def verify_record(record):
    asset = record["asset"]
    assets = unreal.EditorAssetLibrary
    require(assets.get_metadata_tag(asset, record["source"]["VERSION_KEY"]) == record["source"]["VERSION"],
            "Material ownership marker changed: " + asset.get_name())
    if record["kind"] == "base":
        require(shader_digest(validate_graph(asset, record["source"]).get_editor_property("code")) == record["target"],
                "White shader verification failed: " + asset.get_name())
    else:
        values = scalar_values(asset)
        for name, before in record["values"].items():
            expected = 0.96 if name == "SurfaceOpacity" else before
            require(abs(values[name] - expected) < 0.0001, "Screen parameter changed unexpectedly: " + name)


def preflight():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    require(world and world.get_path_name().startswith("/Engine/Maps/Entry."), "White patch requires an idle Entry editor")
    require(not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Stop PIE before the white patch")
    assets = unreal.EditorAssetLibrary
    records = []
    for source, previous, target in SHADERS:
        require(shader_digest(source["SHADER"]) == target, "Canonical white shader differs from this patch version")
        path = source["FOLDER"] + "/" + source["BASE_NAME"]
        material = unreal.load_asset(path)
        require(isinstance(material, unreal.Material), "Existing white-theme base is missing: " + path)
        require(assets.get_metadata_tag(material, source["VERSION_KEY"]) == source["VERSION"],
                "Material ownership marker is missing: " + path)
        require(material.get_editor_property("material_domain") == unreal.MaterialDomain.MD_UI and
                material.get_editor_property("blend_mode") == unreal.BlendMode.BLEND_TRANSLUCENT,
                "White-theme base must remain a translucent UI material")
        custom = validate_graph(material, source)
        before = custom.get_editor_property("code")
        require(shader_digest(before) in (previous, target), "Refusing to replace an unknown shader: " + path)
        records.append({"kind": "base", "asset": material, "custom": custom, "source": source,
                        "before": before, "target": target})
    screen_path = GLASS["FOLDER"] + "/MI_FF_CodexGlass_Screen"
    screen = unreal.load_asset(screen_path)
    require(isinstance(screen, unreal.MaterialInstanceConstant), "Existing Screen instance is missing")
    require(assets.get_metadata_tag(screen, GLASS["VERSION_KEY"]) == GLASS["VERSION"], "Screen ownership marker is missing")
    require(screen.get_editor_property("parent") == records[0]["asset"], "Screen parent changed")
    values = scalar_values(screen)
    require(all(math.isfinite(value) for value in values.values()) and abs(values["SurfaceRole"]) < 0.0001,
            "Screen has invalid scalar values or role")
    require(any(abs(values["SurfaceOpacity"] - expected) < 0.0001 for expected in (0.86, 0.96)),
            "Screen opacity is not the expected contrast/white version")
    records.append({"kind": "instance", "asset": screen, "source": GLASS, "values": values,
                    "before": [value.copy() for value in screen.get_editor_property("scalar_parameter_values")]})
    for record in records:
        record["marker"] = assets.get_metadata_tag(record["asset"], VERSION_KEY)
        require(record["marker"] in ("", VERSION), "Unknown white migration version: " + record["asset"].get_name())
        if record["marker"] == VERSION:
            verify_record(record)
    return records


def apply_white_materials():
    records = preflight()
    assets = unreal.EditorAssetLibrary
    library = unreal.MaterialEditingLibrary
    touched = []
    try:
        for record in records:
            if record["marker"] == VERSION:
                continue
            touched.append(record)
            if record["kind"] == "base":
                record["custom"].set_editor_property("code", record["source"]["SHADER"])
                library.recompile_material(record["asset"])
            else:
                library.set_material_instance_scalar_parameter_value(record["asset"], "SurfaceOpacity", 0.96)
                library.update_material_instance(record["asset"])
            verify_record(record)
            assets.set_metadata_tag(record["asset"], VERSION_KEY, VERSION)
        for record in touched:
            require(assets.save_loaded_asset(record["asset"], only_if_is_dirty=False),
                    "Could not save white material: " + record["asset"].get_name())
        for record in records:
            verify_record(record)
            require(assets.get_metadata_tag(record["asset"], VERSION_KEY) == VERSION, "White marker did not persist")
    except Exception as error:
        rollback_errors = []
        for record in reversed(touched):
            try:
                asset = record["asset"]
                if record["kind"] == "base":
                    record["custom"].set_editor_property("code", record["before"])
                    library.recompile_material(asset)
                    require(record["custom"].get_editor_property("code") == record["before"], "Original shader did not restore")
                else:
                    asset.set_editor_property("scalar_parameter_values", record["before"])
                    library.update_material_instance(asset)
                    require(all(abs(scalar_values(asset)[name] - value) < 0.0001 for name, value in record["values"].items()),
                            "Original Screen scalars did not restore")
                if record["marker"]:
                    assets.set_metadata_tag(asset, VERSION_KEY, record["marker"])
                else:
                    assets.remove_metadata_tag(asset, VERSION_KEY)
                require(assets.save_loaded_asset(asset, only_if_is_dirty=False), "Rollback save failed")
            except Exception as rollback_error:
                rollback_errors.append(record["asset"].get_name() + ": " + str(rollback_error))
        if rollback_errors:
            raise RuntimeError("White update failed: {}. Restore outer migration backups: {}".format(
                error, "; ".join(rollback_errors))) from error
        raise RuntimeError("White update failed; touched materials were restored: " + str(error)) from error
    unreal.log("FF_CODEX_WHITE_MATERIALS_PASS updated={} screen_opacity=0.96".format(len(touched)))
    return [record["asset"] for record in records]


if __name__ == "__main__":
    apply_white_materials()
