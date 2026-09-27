# -*- coding: utf-8 -*-
# import_notion_facility_examples.py
# 역할: 시설 모델·Data Asset·썸네일 작업에서 제작 결과를 Unreal 에셋으로 가져오고 연결한다.

"""Import only missing Notion reference-art meshes and thumbnails.

Run only from an unattended UnrealEditor-Cmd Python commandlet. Existing
packages and the shared material palette are never replaced. The whole batch
is preflighted and saved through a recoverable package transaction.

These assets live under Test_Asset and are visual references only.  They never
define placement, queue, or obstacle dimensions; UFFZSFacilityDefinition::Spec
is the sole source for those runtime values.
"""

import json
import os
import re
import struct
import sys

import unreal

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from ffzs_asset_save_transaction import AssetSaveTransaction
from ffzs_definition_source import require_unattended_commandlet


MESH_DESTINATION = "/Game/FestaFiesta/Zone/Test_Asset/Asset"
TEXTURE_DESTINATION = "/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/Images"
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
        # An explicit native factory keeps commandlet texture imports on the
        # legacy, headless-safe path instead of UE 5.7 Interchange/Slate.
        task.set_editor_property("factory", factory)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    expected = destination + "/" + asset_name
    expected_object = expected + "." + asset_name
    imported_paths = list(task.get_editor_property("imported_object_paths") or [])
    normalized_paths = {str(path).split(":", 1)[0] for path in imported_paths}
    unexpected = [path for path in normalized_paths if path != expected_object]
    if unexpected:
        for object_path in unexpected:
            package_path = object_path.split(".", 1)[0]
            if package_path not in before_packages and package_path.startswith(
                destination.rstrip("/") + "/"
            ):
                if not unreal.EditorAssetLibrary.delete_asset(package_path):
                    raise RuntimeError(
                        "Could not remove unexpected imported package: " + package_path
                    )
        raise RuntimeError(
            "Importer created unexpected assets: " + ", ".join(sorted(unexpected))
        )
    if expected_object not in normalized_paths:
        raise RuntimeError(
            "Importer did not report expected asset {0} (reported={1})".format(
                expected_object, ", ".join(sorted(normalized_paths)) or "<none>"
            )
        )
    asset = unreal.EditorAssetLibrary.load_asset(expected)
    if not asset or asset.get_path_name() != expected_object:
        raise RuntimeError(
            "Imported asset path mismatch: expected {0}, got {1}".format(
                expected_object, asset.get_path_name() if asset else "<none>"
            )
        )
    return asset


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
        "ReferenceArtOnly;PivotGroundCenter;Front+Y;NoRuntimePlacementAuthority",
    )
    unreal.EditorAssetLibrary.set_metadata_tag(
        mesh, "FFZS.GeneratedSource", source_rel.replace("\\", "/")
    )
    unreal.EditorAssetLibrary.set_metadata_tag(
        mesh, "FFZS.NotionFacilityDefinitionId", definition_id
    )
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


def metadata_value(asset, key):
    return str(unreal.EditorAssetLibrary.get_metadata_tag(asset, key) or "")


def validate_existing_mesh(mesh, definition_id, source_rel):
    expected_contract = (
        "ReferenceArtOnly;PivotGroundCenter;Front+Y;NoRuntimePlacementAuthority"
    )
    actual_contract = metadata_value(mesh, "FFZS.FacilityVisualContract")
    legacy_reference_contract = bool(
        re.fullmatch(
            r"[0-9]+(?:\.[0-9]+)?x[0-9]+(?:\.[0-9]+)?x[0-9]+(?:\.[0-9]+)?cm;PivotGroundCenter;Front\+Y",
            actual_contract,
        )
    )
    if actual_contract != expected_contract and not legacy_reference_contract:
        raise RuntimeError("Existing reference mesh ownership contract mismatch")
    if metadata_value(mesh, "FFZS.GeneratedSource").replace("\\", "/") != source_rel.replace("\\", "/"):
        raise RuntimeError("Existing reference mesh source marker mismatch")
    if metadata_value(mesh, "FFZS.NotionFacilityDefinitionId") != definition_id:
        raise RuntimeError("Existing reference mesh Definition marker mismatch")


def validate_existing_texture(texture, definition_id, source_rel):
    if metadata_value(texture, "FFZS.GeneratedSource").replace("\\", "/") != source_rel.replace("\\", "/"):
        raise RuntimeError("Existing reference texture source marker mismatch")
    if metadata_value(texture, "FFZS.NotionFacilityDefinitionId") != definition_id:
        raise RuntimeError("Existing reference texture Definition marker mismatch")


def png_dimensions(path):
    with open(path, "rb") as handle:
        header = handle.read(24)
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n":
        raise RuntimeError("Invalid PNG: " + path)
    return struct.unpack(">II", header[16:24])


def run():
    require_unattended_commandlet()
    # UE 5.7 commandlets need the commandlet-safe legacy FBX importer.
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

    ready_entries = []
    skipped_representatives = []
    failures = []
    preflight = []
    for entry in manifest["examples"]:
        definition_id = entry["definition_id"]
        if definition_id in existing:
            skipped_representatives.append(
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

        if not os.path.isfile(fbx_path):
            failures.append("Missing FBX source: " + fbx_path)
        if not os.path.isfile(png_path):
            failures.append("Missing PNG source: " + png_path)
        elif png_dimensions(png_path) != (324, 260):
            failures.append("Thumbnail is not 324x260: " + png_path)

        mesh_exists = unreal.EditorAssetLibrary.does_asset_exist(mesh_asset_path)
        texture_exists = unreal.EditorAssetLibrary.does_asset_exist(texture_asset_path)
        if mesh_exists:
            mesh = unreal.EditorAssetLibrary.load_asset(mesh_asset_path)
            if not isinstance(mesh, unreal.StaticMesh):
                failures.append("Existing asset is not a StaticMesh: " + mesh_asset_path)
            else:
                try:
                    validate_existing_mesh(mesh, definition_id, fbx_rel)
                except Exception as error:
                    failures.append("{0}: {1}".format(definition_id, error))
        if texture_exists:
            texture = unreal.EditorAssetLibrary.load_asset(texture_asset_path)
            if not isinstance(texture, unreal.Texture2D):
                failures.append("Existing asset is not a Texture2D: " + texture_asset_path)
            else:
                try:
                    validate_existing_texture(texture, definition_id, png_rel)
                except Exception as error:
                    failures.append("{0}: {1}".format(definition_id, error))
        preflight.append(
            {
                "definition_id": definition_id,
                "mesh_name": mesh_name,
                "texture_name": texture_name,
                "mesh_asset_path": mesh_asset_path,
                "texture_asset_path": texture_asset_path,
                "fbx_path": fbx_path,
                "png_path": png_path,
                "fbx_rel": fbx_rel,
                "png_rel": png_rel,
                "mesh_exists": mesh_exists,
                "texture_exists": texture_exists,
            }
        )

    if failures:
        raise RuntimeError(
            "Notion facility import preflight failed before mutation: "
            + " | ".join(failures)
        )

    transaction_targets = []
    for item in preflight:
        if not item["mesh_exists"]:
            transaction_targets.append(item["mesh_asset_path"])
        if not item["texture_exists"]:
            transaction_targets.append(item["texture_asset_path"])
    transaction = (
        AssetSaveTransaction(transaction_targets, "NotionFacilityImport")
        if transaction_targets
        else None
    )
    report_path = os.path.join(saved_dir(), REPORT_NAME)
    pending_report_path = report_path + ".pending"
    try:
        pending_saves = []
        for item in preflight:
            definition_id = item["definition_id"]
            mesh_created = not item["mesh_exists"]
            texture_created = not item["texture_exists"]
            mesh = (
                unreal.EditorAssetLibrary.load_asset(item["mesh_asset_path"])
                if item["mesh_exists"]
                else import_asset(
                    item["fbx_path"],
                    MESH_DESTINATION,
                    item["mesh_name"],
                    make_fbx_options(),
                )
            )
            texture = (
                unreal.EditorAssetLibrary.load_asset(item["texture_asset_path"])
                if item["texture_exists"]
                else import_asset(
                    item["png_path"],
                    TEXTURE_DESTINATION,
                    item["texture_name"],
                    factory=unreal.TextureFactory(),
                )
            )
            if not isinstance(mesh, unreal.StaticMesh):
                raise RuntimeError("StaticMesh import failed: " + item["mesh_asset_path"])
            if not isinstance(texture, unreal.Texture2D):
                raise RuntimeError("Texture2D import failed: " + item["texture_asset_path"])

            assignments = []
            simple_collisions = collision_count(mesh)
            if mesh_created:
                assignments, simple_collisions = configure_mesh(
                    mesh, definition_id, item["fbx_rel"]
                )
                pending_saves.append(mesh)
            if texture_created:
                configure_texture(texture, definition_id, item["png_rel"])
                pending_saves.append(texture)
            ready_entries.append(
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

        for asset in pending_saves:
            if not unreal.EditorAssetLibrary.save_loaded_asset(
                asset, only_if_is_dirty=False
            ):
                raise RuntimeError("Could not save imported asset " + asset.get_path_name())

        report = {
            "source": manifest["source"],
            "expected_examples": len(manifest["examples"]),
            "existing_representatives": skipped_representatives,
            "reference_assets": ready_entries,
            "failures": [],
        }
        with open(pending_report_path, "w", encoding="utf-8") as handle:
            json.dump(report, handle, ensure_ascii=False, indent=2)
        if transaction:
            transaction.commit()
        try:
            os.replace(pending_report_path, report_path)
        except Exception as error:
            unreal.log_warning(
                "[FF_NOTION_IMPORT] Assets were committed, but the disposable "
                "report could not be published from {0}: {1}".format(
                    pending_report_path, error
                )
            )
    except Exception:
        if transaction:
            transaction.rollback()
        try:
            if os.path.isfile(pending_report_path):
                os.remove(pending_report_path)
        except Exception as cleanup_error:
            unreal.log_warning(
                "[FF_NOTION_IMPORT] Could not remove pending report {0}: {1}".format(
                    pending_report_path, cleanup_error
                )
            )
        raise

    unreal.log(
        "[FF_NOTION_IMPORT] Complete: {0} existing representatives, {1} reference pairs".format(
            len(skipped_representatives), len(ready_entries)
        )
    )


run()
