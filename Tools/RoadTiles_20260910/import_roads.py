"""Import six road tiles, their authored LODs and palette into UE 5.7.4.

Run through Run-RoadEditor.ps1 -Mode Import. Existing packages are never
overwritten: an occupied destination receives a fresh Run_<timestamp> child.
Verification reports contain engine measurements, not source-only estimates.
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import time
import traceback

import unreal

TOOLS = Path(__file__).resolve().parent
ROOT = TOOLS.parent.parent
SOURCE = ROOT / "SourceArt/RoadTiles_20260910"
OUT = TOOLS / "qa"
BASE = "/Game/Asset/RoadAsset_HKH/RoadTiles_20260910"
NAMES = ["SM_Road_Sand", "SM_Road_Asphalt", "SM_Road_Wood",
         "SM_Road_CreamTile", "SM_Road_MintHerringbone", "SM_Road_ConcreteDrain"]
CALIBRATION = [
    ("SM_Road_Calibration_1m", [0, 0, 50], [100, 100, 100]),
    ("SM_Road_Calibration_X", [200, 0, 50], [75, 25, 25]),
    ("SM_Road_Calibration_Y", [0, -300, 50], [25, 100, 25]),
    ("SM_Road_Calibration_Z", [0, 0, 400], [25, 25, 125]),
]


def write_report(report):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "unreal_import_report.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")


def check(condition, message):
    if not condition:
        raise RuntimeError(message)


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def vec(value):
    return [round(float(value.x), 5), round(float(value.y), 5), round(float(value.z), 5)]


def measurements(mesh):
    bounds = mesh.get_bounding_box()
    minimum, maximum = vec(bounds.min), vec(bounds.max)
    return {"min_cm": minimum, "max_cm": maximum,
            "size_cm": [round(b - a, 5) for a, b in zip(minimum, maximum)],
            "center_cm": [round((a + b) * .5, 5) for a, b in zip(minimum, maximum)]}


def options():
    result = unreal.FbxImportUI()
    for prop, value in {"automated_import_should_detect_type": False,
                        "import_mesh": True, "import_as_skeletal": False,
                        "import_materials": False, "import_textures": False,
                        "mesh_type_to_import": unreal.FBXImportType.FBXIT_STATIC_MESH}.items():
        result.set_editor_property(prop, value)
    data = result.get_editor_property("static_mesh_import_data")
    for prop, value in {"combine_meshes": True, "generate_lightmap_u_vs": False,
                        "auto_generate_collision": False, "import_mesh_lods": False,
                        "convert_scene": True, "convert_scene_unit": True,
                        "build_nanite": False, "remove_degenerates": True,
                        "transform_vertex_to_absolute": True,
                        "import_uniform_scale": 1.0,
                        "normal_import_method": unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS}.items():
        data.set_editor_property(prop, value)
    return result


def import_asset(source, folder, name, asset_class, fbx=False):
    target = folder + "/" + name
    check(not unreal.EditorAssetLibrary.does_asset_exist(target), "Refusing replacement: " + target)
    task = unreal.AssetImportTask()
    for prop, value in {"filename": str(source), "destination_path": folder,
                        "destination_name": name, "automated": True,
                        "replace_existing": False, "save": False}.items():
        task.set_editor_property(prop, value)
    if fbx:
        task.set_editor_property("options", options())
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported = [str(path) for path in task.get_editor_property("imported_object_paths")]
    expected = target + "." + name
    check(imported == [expected], "Unexpected imported paths for {}: {}".format(name, imported))
    asset = unreal.load_asset(target)
    check(isinstance(asset, asset_class), "Wrong asset type: " + target)
    unreal.EditorAssetLibrary.set_metadata_tag(asset, "RoadTiles.Source", str(source))
    return asset


def save(asset):
    check(unreal.EditorAssetLibrary.save_loaded_asset(asset, False),
          "Asset save failed: " + asset.get_path_name())


def palette_material(folder, texture):
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_RoadPalette", folder, unreal.Material, unreal.MaterialFactoryNew())
    check(isinstance(material, unreal.Material), "Material creation failed")
    material.set_editor_property("two_sided", False)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    lib = unreal.MaterialEditingLibrary
    sample = lib.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -400, 0)
    sample.set_editor_property("parameter_name", "RoadPalette")
    sample.set_editor_property("texture", texture)
    check(lib.connect_material_property(sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR), "BaseColor connection failed")
    for offset, value, destination in [(160, .78, unreal.MaterialProperty.MP_ROUGHNESS),
                                       (270, 0.0, unreal.MaterialProperty.MP_METALLIC)]:
        expression = lib.create_material_expression(material, unreal.MaterialExpressionConstant, -400, offset)
        expression.set_editor_property("r", value)
        check(lib.connect_material_property(expression, "", destination), "Material connection failed")
    lib.recompile_material(material)
    unreal.EditorAssetLibrary.set_metadata_tag(material, "RoadTiles.MaterialContract", "PaletteUV0;Roughness=0.78;Metallic=0;OneSided")
    save(material)
    return material


def verify_mesh(mesh, material, subsystem):
    result = {"asset": mesh.get_path_name(), **measurements(mesh)}
    count = int(subsystem.get_lod_count(mesh))
    result["lod_count"] = count
    result["lods"] = [
        {"lod": index, "triangles": int(mesh.get_num_triangles(index)),
         "vertices": int(mesh.get_num_vertices(index)),
         "sections": int(mesh.get_num_sections(index)),
         "uv_channels": int(subsystem.get_num_uv_channels(mesh, index))}
        for index in range(count)]
    result["screen_sizes"] = [float(value) for value in subsystem.get_lod_screen_sizes(mesh)]
    # UE's GetSimpleCollisionCount excludes ConvexElems; UCX hulls have a separate API.
    result["primitive_collision_count"] = int(subsystem.get_simple_collision_count(mesh))
    result["convex_collision_count"] = int(subsystem.get_convex_collision_count(mesh))
    result["simple_collision_count"] = result["primitive_collision_count"] + result["convex_collision_count"]
    slots = mesh.get_editor_property("static_materials")
    result["materials"] = [
        {"slot": str(slot.get_editor_property("material_slot_name")),
         "asset": slot.get_editor_property("material_interface").get_path_name()
         if slot.get_editor_property("material_interface") else None}
        for slot in slots]
    result["nanite_enabled"] = bool(subsystem.get_nanite_settings(mesh).get_editor_property("enabled"))
    failures = []
    if any(abs(value - 200.0) > .1 for value in result["size_cm"][:2]):
        failures.append("XY dimensions must be 200 x 200 cm")
    if result["size_cm"][2] <= 0:
        failures.append("Tile must have physical thickness")
    if any(abs(value) > .1 for value in result["center_cm"][:2]) or abs(result["min_cm"][2]) > .1:
        failures.append("Pivot must be floor center")
    if count != 3:
        failures.append("Expected exactly three authored LODs")
    if result["simple_collision_count"] != 1 or result["convex_collision_count"] != 1:
        failures.append("Expected one authored convex road collision")
    if len(slots) != 1 or result["materials"][0]["asset"] != material.get_path_name():
        failures.append("Expected one shared palette material")
    if result["nanite_enabled"]:
        failures.append("Nanite must remain disabled")
    for lod in result["lods"]:
        if lod["uv_channels"] < 1 or lod["triangles"] <= 0 or lod["sections"] != 1:
            failures.append("LOD{} has invalid geometry, UV0 or section count".format(lod["lod"]))
    if count == 3 and not (result["lods"][0]["triangles"] >= result["lods"][1]["triangles"] >= result["lods"][2]["triangles"]):
        failures.append("LOD triangle counts must not increase")
    result["failures"] = failures
    result["status"] = "FAIL" if failures else "PASS"
    return result


def run():
    command = str(unreal.SystemLibrary.get_command_line()).lower()
    check("-unattended" in command and "-executepythonscript=" in command,
          "Use unattended UnrealEditor-Cmd -ExecutePythonScript in the Entry map")
    report = {"status": "RUNNING", "engine_version": str(unreal.SystemLibrary.get_engine_version()),
              "source_root": str(SOURCE), "started_unix": time.time(), "entries": [],
              "calibration": [], "failures": [], "gpu_surface_review": "NOT_RUN",
              "game_performance": {"status": "NOT_RUN", "frame_time_ms": None},
              "lightmap_uv1": {"status": "NOT_APPLICABLE", "reason": "Project r.AllowStaticLighting=False; Lumen"}}
    write_report(report)
    try:
        palette = SOURCE / "Textures/T_RoadPalette.png"
        sources = [SOURCE / "Exports" / (name + suffix + ".fbx")
                   for name in NAMES for suffix in ("", "_LOD1", "_LOD2")]
        sources += [palette] + [SOURCE / "qa/calibration" / (name + ".fbx") for name, _, _ in CALIBRATION]
        check(all(path.is_file() for path in sources), "Missing source files: " + ", ".join(str(path) for path in sources if not path.is_file()))
        report["source_sha256"] = {str(path.relative_to(SOURCE)): sha256(path) for path in sources}
        folder = BASE
        if unreal.EditorAssetLibrary.does_directory_exist(folder) and unreal.EditorAssetLibrary.list_assets(folder, True, False):
            folder += "/Run_" + time.strftime("%Y%m%d_%H%M%S")
        check(not unreal.EditorAssetLibrary.list_assets(folder, True, False), "Import destination is occupied")
        report["destination"] = folder
        write_report(report)
        unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
        unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX.ToLevel 0")
        subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        check(subsystem is not None, "StaticMeshEditorSubsystem unavailable")
        texture = import_asset(palette, folder + "/Materials", "T_RoadPalette", unreal.Texture2D)
        for prop, value in {"srgb": True, "filter": unreal.TextureFilter.TF_NEAREST,
                            "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
                            "compression_settings": unreal.TextureCompressionSettings.TC_EDITOR_ICON,
                            "address_x": unreal.TextureAddress.TA_CLAMP,
                            "address_y": unreal.TextureAddress.TA_CLAMP}.items():
            texture.set_editor_property(prop, value)
        save(texture)
        material = palette_material(folder + "/Materials", texture)
        report["material"] = material.get_path_name()
        report["texture"] = texture.get_path_name()
        # The four witnesses measure source units and FBX's handedness conversion.
        for name, expected_center, expected_size in CALIBRATION:
            mesh = import_asset(SOURCE / "qa/calibration" / (name + ".fbx"), folder + "/QA_Calibration", name, unreal.StaticMesh, True)
            mesh.set_material(0, material)
            measurement = {"name": name, "asset": mesh.get_path_name(), **measurements(mesh),
                           "expected_center_cm": expected_center, "expected_size_cm": expected_size}
            ok = all(abs(a-b) < .1 for a, b in zip(measurement["center_cm"], expected_center)) and all(abs(a-b) < .1 for a, b in zip(measurement["size_cm"], expected_size))
            measurement["status"] = "PASS" if ok else "FAIL"
            report["calibration"].append(measurement)
            save(mesh)
            check(ok, "Calibration failed for " + name)
        report["axis_contract"] = {"status": "PASS", "forward": "+X", "up": "+Z", "source_y_to_unreal_y": -1,
                                   "scale": "1 Blender meter imports as 100 Unreal centimeters"}
        for name in NAMES:
            mesh = import_asset(SOURCE / "Exports" / (name + ".fbx"), folder + "/Meshes", name, unreal.StaticMesh, True)
            for index in (1, 2):
                returned = int(subsystem.import_lod(mesh, index, str(SOURCE / "Exports" / (name + "_LOD" + str(index) + ".fbx"))))
                check(returned == index, "LOD import failed for {} LOD{}".format(name, index))
            nanite = subsystem.get_nanite_settings(mesh)
            nanite.set_editor_property("enabled", False)
            subsystem.set_nanite_settings(mesh, nanite, True)
            for index in range(len(mesh.get_editor_property("static_materials"))):
                mesh.set_material(index, material)
            body = mesh.get_editor_property("body_setup")
            check(body is not None, "Missing authored body setup: " + name)
            body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
            # SetLodScreenSizes disables the private automatic-size flag internally.
            check(subsystem.set_lod_screen_sizes(mesh, [1.0, .35, .12]), "LOD screen size setup failed: " + name)
            unreal.EditorAssetLibrary.set_metadata_tag(mesh, "RoadTiles.Contract", "200x200cm;PivotFloorCenter;+XForward;+ZUp;PaletteUV0;UCX;LOD0,LOD1,LOD2")
            save(mesh)
            record = verify_mesh(mesh, material, subsystem)
            report["entries"].append(record)
            write_report(report)
            check(record["status"] == "PASS", name + ": " + "; ".join(record["failures"]))
        report["status"] = "PASS"
    except Exception as error:
        report["status"] = "FAIL"
        report["failures"].append(str(error))
        report["traceback"] = traceback.format_exc()
        unreal.log_error("ROAD_IMPORT_FAIL: " + str(error))
    finally:
        report["elapsed_seconds"] = round(time.time() - report["started_unix"], 3)
        write_report(report)
    check(report["status"] == "PASS", "Road import failed; see " + str(OUT / "unreal_import_report.json"))
    unreal.log("ROAD_IMPORT_PASS: " + report["destination"])


if __name__ == "__main__":
    try:
        run()
    finally:
        unreal.SystemLibrary.quit_editor()
