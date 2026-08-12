# -*- coding: utf-8 -*-
"""Import only missing Notion facility example meshes and thumbnails.

Run inside Unreal Editor 5.7 (preferred while the project is already open) or
from UnrealEditor-Cmd with the editor closed.  Existing packages and the shared
material palette are never replaced.
"""

import json
import os
import struct

import unreal


MESH_DESTINATION = "/Game/FestaFiesta/Zone/Test_Asset/Asset"
TEXTURE_DESTINATION = "/Game/FestaFiesta/Zone/Test_Asset/image"
MANIFEST_RELATIVE = os.path.join(
    "Tools", "facilities", "notion_facility_examples.json"
)
FBX_RELATIVE = os.path.join("SourceAssets", "Zone", "FacilityMeshes")
PNG_RELATIVE = os.path.join(
    "Content", "FestaFiesta", "Zone", "Test_Asset", "image"
)
REPORT_NAME = "NotionFacilityImportReport.json"


def project_dir():
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())


def saved_dir():
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())


def try_set(target, property_name, value):
    try:
        target.set_editor_property(property_name, value)
        return True
    except Exception as error:
        unreal.log_warning(
            "[FF_NOTION_IMPORT] Optional property {0} was not set: {1}".format(
                property_name, error
            )
        )
        return False


def canonical_names(entry):
    definition_id = entry["definition_id"]
    return (
        "SM_FFZS_" + definition_id,
        "T_UI_BuildCatalog_" + definition_id,
    )


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
        # An explicit native factory keeps commandlet texture imports on the
        # legacy, headless-safe path instead of UE 5.7 Interchange/Slate.
        task.set_editor_property("factory", factory)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    expected = destination + "/" + asset_name
    return unreal.EditorAssetLibrary.load_asset(expected)


def material_for_slot(slot_name):
    if not slot_name.startswith("FF_"):
        return None
    asset_name = "M_FFZS_" + slot_name[3:]
    return unreal.EditorAssetLibrary.load_asset(
        MESH_DESTINATION + "/Materials/" + asset_name
    )


def collision_count(mesh):
    try:
        subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        if subsystem is not None:
            return int(subsystem.get_simple_collision_count(mesh))
    except Exception as error:
        unreal.log_warning(
            "[FF_NOTION_IMPORT] Collision query failed for {0}: {1}".format(
                mesh.get_name(), error
            )
        )
    return 0


def configure_mesh(mesh, definition_id, source_rel):
    try_set(mesh, "light_map_resolution", 64)
    try_set(mesh, "light_map_coordinate_index", 1)
    try_set(mesh, "allow_cpu_access", False)
    try:
        nanite = mesh.get_editor_property("nanite_settings")
        nanite.set_editor_property("enabled", False)
        mesh.set_editor_property("nanite_settings", nanite)
    except Exception as error:
        unreal.log_warning(
            "[FF_NOTION_IMPORT] Nanite setting skipped for {0}: {1}".format(
                mesh.get_name(), error
            )
        )

    assignments = []
    slots = mesh.get_editor_property("static_materials")
    for index, slot in enumerate(slots):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        material = material_for_slot(slot_name)
        if not isinstance(material, unreal.MaterialInterface):
            raise RuntimeError(
                "Missing shared material for slot {0} on {1}".format(
                    slot_name, mesh.get_name()
                )
            )
        mesh.set_material(index, material)
        assignments.append(
            {"slot": slot_name, "material": material.get_path_name()}
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

    unreal.EditorAssetLibrary.set_metadata_tag(
        mesh,
        "FFZS.FacilityVisualContract",
        "600x300x250cm;PivotGroundCenter;Front+Y",
    )
    unreal.EditorAssetLibrary.set_metadata_tag(
        mesh, "FFZS.GeneratedSource", source_rel.replace("\\", "/")
    )
    unreal.EditorAssetLibrary.set_metadata_tag(
        mesh, "FFZS.NotionFacilityDefinitionId", definition_id
    )
    unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
    return assignments, simple_collisions


def configure_texture(texture, definition_id, source_rel):
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
        texture, "FFZS.GeneratedSource", source_rel.replace("\\", "/")
    )
    unreal.EditorAssetLibrary.set_metadata_tag(
        texture, "FFZS.NotionFacilityDefinitionId", definition_id
    )
    unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)


def png_dimensions(path):
    with open(path, "rb") as handle:
        header = handle.read(24)
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n":
        raise RuntimeError("Invalid PNG: " + path)
    return struct.unpack(">II", header[16:24])


def run():
    # UE 5.7 commandlets need the commandlet-safe legacy FBX importer.  These
    # flags are harmless when this script is executed inside the open editor.
    unreal.SystemLibrary.execute_console_command(
        None, "Interchange.FeatureFlags.Import.FBX 0"
    )
    unreal.SystemLibrary.execute_console_command(
        None, "Interchange.FeatureFlags.Import.FBX.ToLevel 0"
    )

    root = project_dir()
    with open(os.path.join(root, MANIFEST_RELATIVE), "r", encoding="utf-8") as handle:
        manifest = json.load(handle)
    existing = manifest["existing_representatives"]

    imported = []
    skipped = []
    failures = []
    for entry in manifest["examples"]:
        definition_id = entry["definition_id"]
        if definition_id in existing:
            skipped.append(
                {
                    "definition_id": definition_id,
                    "reason": "existing_subcategory_representative",
                    **existing[definition_id]
                }
            )
            continue

        mesh_name, texture_name = canonical_names(entry)
        mesh_asset_path = MESH_DESTINATION + "/" + mesh_name
        texture_asset_path = TEXTURE_DESTINATION + "/" + texture_name
        fbx_rel = os.path.join(FBX_RELATIVE, mesh_name + ".fbx")
        png_rel = os.path.join(PNG_RELATIVE, texture_name + ".png")
        fbx_path = os.path.join(root, fbx_rel)
        png_path = os.path.join(root, png_rel)

        try:
            if not os.path.isfile(fbx_path):
                raise RuntimeError("Missing FBX source: " + fbx_path)
            if not os.path.isfile(png_path):
                raise RuntimeError("Missing PNG source: " + png_path)
            if png_dimensions(png_path) != (324, 260):
                raise RuntimeError("Thumbnail is not 324x260: " + png_path)

            mesh_created = False
            if unreal.EditorAssetLibrary.does_asset_exist(mesh_asset_path):
                mesh = unreal.EditorAssetLibrary.load_asset(mesh_asset_path)
            else:
                mesh = import_asset(
                    fbx_path, MESH_DESTINATION, mesh_name, make_fbx_options()
                )
                mesh_created = True
            if not isinstance(mesh, unreal.StaticMesh):
                raise RuntimeError("StaticMesh import failed: " + mesh_asset_path)

            texture_created = False
            if unreal.EditorAssetLibrary.does_asset_exist(texture_asset_path):
                texture = unreal.EditorAssetLibrary.load_asset(texture_asset_path)
            else:
                texture = import_asset(
                    png_path,
                    TEXTURE_DESTINATION,
                    texture_name,
                    factory=unreal.TextureFactory(),
                )
                texture_created = True
            if not isinstance(texture, unreal.Texture2D):
                raise RuntimeError("Texture2D import failed: " + texture_asset_path)

            assignments = []
            simple_collisions = collision_count(mesh)
            if mesh_created:
                assignments, simple_collisions = configure_mesh(
                    mesh, definition_id, fbx_rel
                )
            if texture_created:
                configure_texture(texture, definition_id, png_rel)

            imported.append(
                {
                    "definition_id": definition_id,
                    "mesh": mesh.get_path_name(),
                    "texture": texture.get_path_name(),
                    "mesh_created": mesh_created,
                    "texture_created": texture_created,
                    "materials": assignments,
                    "simple_collisions": simple_collisions,
                }
            )
            unreal.log(
                "[FF_NOTION_IMPORT] Ready {0} mesh_new={1} texture_new={2}".format(
                    definition_id, mesh_created, texture_created
                )
            )
        except Exception as error:
            failure = "{0}: {1}".format(definition_id, error)
            failures.append(failure)
            unreal.log_error("[FF_NOTION_IMPORT] " + failure)

    report = {
        "source": manifest["source"],
        "expected_examples": len(manifest["examples"]),
        "existing_representatives": skipped,
        "new_assets": imported,
        "failures": failures,
    }
    report_path = os.path.join(saved_dir(), REPORT_NAME)
    with open(report_path, "w", encoding="utf-8") as handle:
        json.dump(report, handle, ensure_ascii=False, indent=2)

    expected_new = len(manifest["examples"]) - len(existing)
    if failures or len(imported) != expected_new:
        raise RuntimeError(
            "Notion facility import failed ({0}/{1} new pairs, {2} failures)".format(
                len(imported), expected_new, len(failures)
            )
        )
    unreal.log(
        "[FF_NOTION_IMPORT] Complete: {0} existing representatives, {1} new pairs".format(
            len(skipped), len(imported)
        )
    )


run()
