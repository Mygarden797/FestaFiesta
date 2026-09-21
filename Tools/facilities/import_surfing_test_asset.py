# -*- coding: utf-8 -*-
"""Import the standalone surfing Test_Asset pair into Unreal Engine.

Run only after ``build_surfing_test_asset.py`` and only from an unattended
UnrealEditor-Cmd Python commandlet.  The importer refuses to replace packages,
uses the existing shared material palette, and rolls both packages back if the
pair cannot be configured and saved together.
"""

from __future__ import annotations

import hashlib
import json
import os
import struct
import sys

import unreal


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from ffzs_asset_save_transaction import AssetSaveTransaction
from ffzs_definition_source import require_unattended_commandlet


TEST_ASSET_ID = "Booth_ActivityGame_Surfing"
MESH_NAME = "SM_FFZS_" + TEST_ASSET_ID
TEXTURE_NAME = "T_UI_BuildCatalog_" + TEST_ASSET_ID
MESH_DESTINATION = "/Game/FestaFiesta/Zone/Test_Asset/Asset"
TEXTURE_DESTINATION = "/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/Images"
MATERIAL_DESTINATION = MESH_DESTINATION + "/Materials"
MESH_ASSET_PATH = MESH_DESTINATION + "/" + MESH_NAME
TEXTURE_ASSET_PATH = TEXTURE_DESTINATION + "/" + TEXTURE_NAME
FBX_RELATIVE = os.path.join(
    "SourceAssets", "Zone", "FacilityMeshes", MESH_NAME + ".fbx"
)
PNG_RELATIVE = os.path.join(
    "Content",
    "FestaFiesta",
    "Zone",
    "Test_Asset",
    "image",
    TEXTURE_NAME + ".png",
)
REPORT_NAME = "SurfingTestAssetImportReport.json"
EXPECTED_PNG = (324, 260, 8, 2)
EXPECTED_MATERIAL_SLOTS = {
    "FF_WoodDark",
    "FF_WoodWarm",
    "FF_Teal",
    "FF_TealDark",
    "FF_Metal",
    "FF_Cream",
    "FF_BlueGrey",
    "FF_AmberGlow",
}


def project_dir():
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())


def saved_dir():
    return unreal.Paths.convert_relative_path_to_full(
        unreal.Paths.project_saved_dir()
    )


def try_set(target, property_name, value):
    """Set a version-sensitive property without hiding required failures."""
    try:
        target.set_editor_property(property_name, value)
        return True
    except Exception as error:
        unreal.log_warning(
            "[FF_SURFING_IMPORT] Optional property {0} was not set: {1}".format(
                property_name, error
            )
        )
        return False


def png_contract(path):
    with open(path, "rb") as handle:
        header = handle.read(26)
    if len(header) != 26 or header[:8] != b"\x89PNG\r\n\x1a\n":
        raise RuntimeError("Invalid PNG header: " + path)
    width, height = struct.unpack(">II", header[16:24])
    return width, height, header[24], header[25]


def file_sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def make_fbx_options():
    options = unreal.FbxImportUI()
    try_set(options, "automated_import_should_detect_type", False)
    try_set(options, "import_mesh", True)
    try_set(options, "import_as_skeletal", False)
    try_set(options, "import_materials", False)
    try_set(options, "import_textures", False)
    try_set(options, "mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)

    data = options.get_editor_property("static_mesh_import_data")
    try_set(data, "combine_meshes", True)
    try_set(data, "generate_lightmap_u_vs", True)
    try_set(data, "auto_generate_collision", False)
    try_set(data, "import_mesh_lods", False)
    try_set(data, "convert_scene", True)
    try_set(data, "convert_scene_unit", True)
    try_set(data, "build_nanite", False)
    try_set(data, "remove_degenerates", True)
    return options


def import_asset(source, destination, asset_name, options=None, factory=None):
    before_packages = {
        str(path).split(".", 1)[0]
        for path in unreal.EditorAssetLibrary.list_assets(
            destination, recursive=False, include_folder=False
        )
    }
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", False)
    try_set(task, "replace_existing_settings", False)
    task.set_editor_property("save", False)
    if options is not None:
        task.set_editor_property("options", options)
    if factory is not None:
        task.set_editor_property("factory", factory)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    expected = destination + "/" + asset_name
    expected_object = expected + "." + asset_name
    imported_paths = list(task.get_editor_property("imported_object_paths") or [])
    normalized_paths = {
        str(path).split(":", 1)[0] for path in imported_paths
    }
    unexpected = [path for path in normalized_paths if path != expected_object]
    if unexpected:
        for object_path in unexpected:
            package_path = object_path.split(".", 1)[0]
            if (
                package_path not in before_packages
                and package_path.startswith(destination.rstrip("/") + "/")
            ):
                if not unreal.EditorAssetLibrary.delete_asset(package_path):
                    raise RuntimeError(
                        "Could not remove unexpected imported package: "
                        + package_path
                    )
        raise RuntimeError(
            "Importer created unexpected assets: "
            + ", ".join(sorted(unexpected))
        )
    if expected_object not in normalized_paths:
        raise RuntimeError(
            "Importer did not report expected asset {0} (reported={1})".format(
                expected_object,
                ", ".join(sorted(normalized_paths)) or "<none>",
            )
        )

    asset = unreal.EditorAssetLibrary.load_asset(expected)
    if not asset or asset.get_path_name() != expected_object:
        raise RuntimeError(
            "Imported asset path mismatch: expected {0}, got {1}".format(
                expected_object,
                asset.get_path_name() if asset else "<none>",
            )
        )
    return asset


def material_for_slot(slot_name):
    if not slot_name.startswith("FF_"):
        return None
    return unreal.EditorAssetLibrary.load_asset(
        MATERIAL_DESTINATION + "/M_FFZS_" + slot_name[3:]
    )


def collision_count(mesh):
    try:
        subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        if subsystem is not None:
            count = int(subsystem.get_simple_collision_count(mesh))
            if count > 0:
                return count
    except Exception as error:
        unreal.log_warning(
            "[FF_SURFING_IMPORT] Collision subsystem query failed: " + str(error)
        )
    try:
        body_setup = mesh.get_editor_property("body_setup")
        aggregate = body_setup.get_editor_property("agg_geom") if body_setup else None
        if aggregate is None:
            return 0
        count = 0
        for property_name in (
            "sphere_elems",
            "box_elems",
            "sphyl_elems",
            "convex_elems",
            "tapered_capsule_elems",
            "level_set_elems",
            "skinned_level_set_elems",
        ):
            try:
                count += len(aggregate.get_editor_property(property_name))
            except Exception:
                pass
        return count
    except Exception as error:
        unreal.log_warning(
            "[FF_SURFING_IMPORT] Direct collision query failed: " + str(error)
        )
        return -1


def configure_mesh(mesh):
    try_set(mesh, "light_map_resolution", 64)
    try_set(mesh, "light_map_coordinate_index", 1)
    try_set(mesh, "allow_cpu_access", False)
    try:
        nanite = mesh.get_editor_property("nanite_settings")
        nanite.set_editor_property("enabled", False)
        mesh.set_editor_property("nanite_settings", nanite)
    except Exception as error:
        unreal.log_warning(
            "[FF_SURFING_IMPORT] Nanite setting was not changed: " + str(error)
        )

    assignments = []
    actual_slots = set()
    for index, slot in enumerate(mesh.get_editor_property("static_materials")):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        actual_slots.add(slot_name)
        material = material_for_slot(slot_name)
        if not isinstance(material, unreal.MaterialInterface):
            raise RuntimeError("Missing shared material for slot " + slot_name)
        mesh.set_material(index, material)
        assignments.append(
            {"slot": slot_name, "material": material.get_path_name()}
        )
    if actual_slots != EXPECTED_MATERIAL_SLOTS:
        raise RuntimeError(
            "Unexpected FBX material slots: expected={0}, actual={1}".format(
                sorted(EXPECTED_MATERIAL_SLOTS), sorted(actual_slots)
            )
        )

    simple_collisions = collision_count(mesh)
    if simple_collisions == 0:
        subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        if subsystem is None:
            raise RuntimeError("StaticMeshEditorSubsystem unavailable")
        subsystem.add_simple_collisions(
            mesh, unreal.ScriptingCollisionShapeType.BOX
        )
        simple_collisions = collision_count(mesh)
    if simple_collisions <= 0:
        raise RuntimeError("StaticMesh has no simple collision")

    unreal.EditorAssetLibrary.set_metadata_tag(
        mesh,
        "FFZS.FacilityVisualContract",
        "ReferenceArtOnly;PivotGroundCenter;Front+Y;NoRuntimePlacementAuthority",
    )
    unreal.EditorAssetLibrary.set_metadata_tag(
        mesh, "FFZS.GeneratedSource", FBX_RELATIVE.replace("\\", "/")
    )
    unreal.EditorAssetLibrary.set_metadata_tag(
        mesh, "FFZS.TestAssetId", TEST_ASSET_ID
    )
    return assignments, simple_collisions


def configure_texture(texture):
    texture.set_editor_property(
        "compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON
    )
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property(
        "mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS
    )
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("never_stream", True)
    try_set(texture, "address_x", unreal.TextureAddress.TA_CLAMP)
    try_set(texture, "address_y", unreal.TextureAddress.TA_CLAMP)
    unreal.EditorAssetLibrary.set_metadata_tag(
        texture, "FFZS.GeneratedSource", PNG_RELATIVE.replace("\\", "/")
    )
    unreal.EditorAssetLibrary.set_metadata_tag(
        texture, "FFZS.TestAssetId", TEST_ASSET_ID
    )


def validate_loaded_pair(mesh, texture):
    failures = []
    if not isinstance(mesh, unreal.StaticMesh):
        failures.append("Imported mesh has the wrong class")
    if not isinstance(texture, unreal.Texture2D):
        failures.append("Imported texture has the wrong class")
    if mesh.get_path_name() != MESH_ASSET_PATH + "." + MESH_NAME:
        failures.append("StaticMesh object path mismatch")
    if texture.get_path_name() != TEXTURE_ASSET_PATH + "." + TEXTURE_NAME:
        failures.append("Texture2D object path mismatch")

    bounds = mesh.get_bounding_box()
    minimum = bounds.min
    maximum = bounds.max
    size = (
        float(maximum.x - minimum.x),
        float(maximum.y - minimum.y),
        float(maximum.z - minimum.z),
    )
    center = (
        float((maximum.x + minimum.x) * 0.5),
        float((maximum.y + minimum.y) * 0.5),
    )
    if abs(center[0]) > 2.0 or abs(center[1]) > 2.0 or abs(float(minimum.z)) > 2.0:
        failures.append("StaticMesh pivot is not at the ground-centred origin")
    if any(axis < 100.0 for axis in size):
        failures.append("StaticMesh bounds are implausibly small")
    vertices = int(mesh.get_num_vertices(0))
    triangles = int(mesh.get_num_triangles(0))
    if vertices <= 0 or triangles <= 0 or triangles > 50000:
        failures.append("StaticMesh LOD0 geometry is invalid")
    materials = mesh.get_editor_property("static_materials")
    if len(materials) < 3 or any(
        slot.get_editor_property("material_interface") is None
        for slot in materials
    ):
        failures.append("StaticMesh material assignments are incomplete")
    collisions = collision_count(mesh)
    if collisions <= 0:
        failures.append("StaticMesh has no serialized simple collision")

    required_texture_settings = (
        ("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON),
        ("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI),
        ("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS),
        ("srgb", True),
        ("never_stream", True),
        ("address_x", unreal.TextureAddress.TA_CLAMP),
        ("address_y", unreal.TextureAddress.TA_CLAMP),
    )
    for property_name, expected in required_texture_settings:
        if texture.get_editor_property(property_name) != expected:
            failures.append("Texture setting mismatch: " + property_name)
    if failures:
        raise RuntimeError(" | ".join(failures))
    return {
        "bounds_min_cm": [
            round(float(minimum.x), 3),
            round(float(minimum.y), 3),
            round(float(minimum.z), 3),
        ],
        "bounds_max_cm": [
            round(float(maximum.x), 3),
            round(float(maximum.y), 3),
            round(float(maximum.z), 3),
        ],
        "vertices_lod0": vertices,
        "triangles_lod0": triangles,
        "simple_collisions": collisions,
    }


def run():
    require_unattended_commandlet()
    unreal.SystemLibrary.execute_console_command(
        None, "Interchange.FeatureFlags.Import.FBX 0"
    )
    unreal.SystemLibrary.execute_console_command(
        None, "Interchange.FeatureFlags.Import.FBX.ToLevel 0"
    )

    root = project_dir()
    fbx_path = os.path.join(root, FBX_RELATIVE)
    png_path = os.path.join(root, PNG_RELATIVE)
    failures = []
    if not os.path.isfile(fbx_path):
        failures.append("Missing FBX source: " + fbx_path)
    if not os.path.isfile(png_path):
        failures.append("Missing PNG source: " + png_path)
    elif png_contract(png_path) != EXPECTED_PNG:
        failures.append(
            "PNG contract mismatch: expected={0}, actual={1}".format(
                EXPECTED_PNG, png_contract(png_path)
            )
        )
    for package_path in (MESH_ASSET_PATH, TEXTURE_ASSET_PATH):
        if unreal.EditorAssetLibrary.does_asset_exist(package_path):
            failures.append("Refusing to replace existing asset: " + package_path)
    for slot_name in sorted(EXPECTED_MATERIAL_SLOTS):
        material = material_for_slot(slot_name)
        if not isinstance(material, unreal.MaterialInterface):
            failures.append("Missing shared material for slot: " + slot_name)
    if failures:
        raise RuntimeError(
            "Surfing Test_Asset preflight failed before mutation: "
            + " | ".join(failures)
        )

    transaction = AssetSaveTransaction(
        [MESH_ASSET_PATH, TEXTURE_ASSET_PATH], "SurfingTestAssetImport"
    )
    try:
        mesh = import_asset(
            fbx_path,
            MESH_DESTINATION,
            MESH_NAME,
            options=make_fbx_options(),
        )
        texture = import_asset(
            png_path,
            TEXTURE_DESTINATION,
            TEXTURE_NAME,
            factory=unreal.TextureFactory(),
        )
        if not isinstance(mesh, unreal.StaticMesh):
            raise RuntimeError("FBX import did not produce a StaticMesh")
        if not isinstance(texture, unreal.Texture2D):
            raise RuntimeError("PNG import did not produce a Texture2D")

        assignments, collisions = configure_mesh(mesh)
        configure_texture(texture)
        geometry = validate_loaded_pair(mesh, texture)
        for asset in (mesh, texture):
            if not unreal.EditorAssetLibrary.save_loaded_asset(
                asset, only_if_is_dirty=False
            ):
                raise RuntimeError("Could not save imported asset " + asset.get_path_name())
        transaction.commit()
    except Exception:
        transaction.rollback()
        raise

    report = {
        "test_asset_id": TEST_ASSET_ID,
        "mesh": mesh.get_path_name(),
        "texture": texture.get_path_name(),
        "fbx_source": FBX_RELATIVE.replace("\\", "/"),
        "png_source": PNG_RELATIVE.replace("\\", "/"),
        "fbx_sha256": file_sha256(fbx_path),
        "png_sha256": file_sha256(png_path),
        "png_contract": EXPECTED_PNG,
        "materials": assignments,
        "simple_collisions": collisions,
        "geometry": geometry,
        "failures": [],
    }
    report_path = os.path.join(saved_dir(), REPORT_NAME)
    with open(report_path, "w", encoding="utf-8") as handle:
        json.dump(report, handle, ensure_ascii=False, indent=2)
    unreal.log("[FF_SURFING_IMPORT] " + json.dumps(report, ensure_ascii=False))


run()
