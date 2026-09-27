# -*- coding: utf-8 -*-
# import_facility_meshes.py
# 역할: 시설 모델·Data Asset·썸네일 작업에서 제작 결과를 Unreal 에셋으로 가져오고 연결한다.

"""Import the generated festival facility FBXs and link them to FFZS definitions.

Run from UnrealEditor-Cmd after ``build_facility_meshes.py`` has generated the
FBX source files.  The visual contract used by the placement system is:

* Unreal centimeters at scale 1
* pivot at the footprint centre, on the ground (Z=0)
* local +Y is the customer-facing side
* target bounds come from each UFFZSFacilityDefinition Spec
"""

import json
import hashlib
import os
import sys

import unreal

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)
from ffzs_definition_source import (
    build_authoring_projection,
    calculate_projection_digest,
    read_facility_definitions,
    require_unattended_commandlet,
    select_managed_visual_definitions,
)
from ffzs_asset_save_transaction import AssetSaveTransaction

SOURCE_RELATIVE = os.path.join("SourceAssets", "Zone", "FacilityMeshes")
GENERATED_DESTINATION_ROOT = "/Game/FestaFiesta/Zone/Test_Asset/Asset"
REPORT_NAME = "FacilityMeshImportReport.json"
BUILD_MANIFEST_NAME = "FacilityMeshBuildManifest.json"


def project_dir():
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())


def saved_dir():
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())


def try_set(target, property_name, value):
    """Set a version-sensitive importer property when this UE build exposes it."""
    try:
        target.set_editor_property(property_name, value)
        return True
    except Exception as error:
        unreal.log_warning(
            "[FF_FACILITY_MESH] Optional property {0} was not set: {1}".format(
                property_name, error
            )
        )
        return False


def make_fbx_options():
    options = unreal.FbxImportUI()
    try_set(options, "automated_import_should_detect_type", False)
    try_set(options, "import_mesh", True)
    try_set(options, "import_as_skeletal", False)
    # Materials are managed by configure_facility_mesh_materials.py and must not create
    # transaction-external packages during this geometry import.
    try_set(options, "import_materials", False)
    try_set(options, "import_textures", False)
    try_set(options, "mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)

    import_data = options.get_editor_property("static_mesh_import_data")
    try_set(import_data, "combine_meshes", True)
    try_set(import_data, "generate_lightmap_u_vs", True)
    try_set(import_data, "auto_generate_collision", False)
    try_set(import_data, "import_mesh_lods", False)
    try_set(import_data, "convert_scene", True)
    try_set(import_data, "convert_scene_unit", True)
    try_set(import_data, "build_nanite", False)
    try_set(import_data, "remove_degenerates", True)
    return options


def import_mesh(source_file, asset_name, destination_path):
    before_packages = {
        str(path).split(".", 1)[0]
        for path in unreal.EditorAssetLibrary.list_assets(
            destination_path, recursive=False, include_folder=False
        )
    }
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source_file)
    task.set_editor_property("destination_path", destination_path)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    try_set(task, "replace_existing_settings", True)
    task.set_editor_property("save", False)
    task.set_editor_property("options", make_fbx_options())

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    expected_path = "{0}/{1}".format(destination_path, asset_name)
    expected_object_path = "{0}.{1}".format(expected_path, asset_name)
    imported_paths = list(task.get_editor_property("imported_object_paths") or [])
    normalized_imported_paths = {
        str(path).split(":", 1)[0] for path in imported_paths
    }
    unexpected_paths = [
        str(path)
        for path in imported_paths
        if str(path).split(":", 1)[0] != expected_object_path
    ]
    if unexpected_paths:
        for imported_path in unexpected_paths:
            package_path = imported_path.split(".", 1)[0]
            if package_path not in before_packages and package_path.startswith(
                destination_path.rstrip("/") + "/"
            ):
                if not unreal.EditorAssetLibrary.delete_asset(package_path):
                    raise RuntimeError(
                        "Could not remove unexpected imported package: " + package_path
                    )
        raise RuntimeError(
            "Importer created unexpected assets outside the transaction: {0}".format(
                ", ".join(unexpected_paths)
            )
        )
    if expected_object_path not in normalized_imported_paths:
        raise RuntimeError(
            "Importer did not report the expected StaticMesh: {0} (reported={1})".format(
                expected_object_path,
                ", ".join(sorted(normalized_imported_paths)) or "<none>",
            )
        )
    imported = unreal.EditorAssetLibrary.load_asset(expected_path)
    if not isinstance(imported, unreal.StaticMesh):
        return None
    if imported.get_path_name() != expected_object_path:
        raise RuntimeError(
            "Imported StaticMesh path mismatch: expected {0}, got {1}".format(
                expected_object_path, imported.get_path_name()
            )
        )
    return imported


def configure_mesh(mesh, definition_record):
    try_set(mesh, "light_map_resolution", 64)
    try_set(mesh, "light_map_coordinate_index", 1)
    try_set(mesh, "allow_cpu_access", False)

    try:
        nanite_settings = mesh.get_editor_property("nanite_settings")
        nanite_settings.set_editor_property("enabled", False)
        mesh.set_editor_property("nanite_settings", nanite_settings)
    except Exception as error:
        unreal.log_warning(
            "[FF_FACILITY_MESH] Nanite setting was not changed for {0}: {1}".format(
                mesh.get_name(), error
            )
        )

    # Imported collision is reused for player movement; placement and NPC navigation
    # still consume FacilitySpec geometry. Report existing UCX metadata without
    # generating another collision body or changing the shared mesh's collision mode.
    collision_count = 0
    try:
        subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        if subsystem is not None:
            collision_count = int(subsystem.get_simple_collision_count(mesh))
        else:
            body_setup = mesh.get_editor_property("body_setup")
            aggregate = body_setup.get_editor_property("agg_geom") if body_setup else None
            if aggregate is not None:
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
                        collision_count += len(
                            aggregate.get_editor_property(property_name)
                        )
                    except Exception:
                        pass
    except Exception as error:
        unreal.log_warning(
            "[FF_FACILITY_MESH] Collision setup skipped for {0}: {1}".format(
                mesh.get_name(), error
            )
        )

    target = definition_record["target_size_cm"]
    unreal.EditorAssetLibrary.set_metadata_tag(
        mesh,
        "FFZS.FacilityVisualContract",
        "Definition={0};Target={1}x{2}x{3}cm;Mode={4};PivotGroundCenter;Front+Y".format(
            definition_record["facility_definition_id"],
            target["x"],
            target["y"],
            target["z"],
            definition_record["visual_sizing_mode"],
        ),
    )
    unreal.EditorAssetLibrary.set_metadata_tag(
        mesh, "FFZS.GeneratedSource", "SourceAssets/Zone/FacilityMeshes/{0}.fbx".format(
            mesh.get_name()
        )
    )
    return collision_count


def link_definition(definition, mesh):
    spec = definition.get_editor_property("spec")
    spec.set_editor_property("visual_mesh", mesh)
    definition.set_editor_property("spec", spec)
    configured = definition.get_editor_property("spec").get_editor_property("visual_mesh")
    configured_path = configured.get_path_name() if configured else ""
    if configured_path != mesh.get_path_name():
        raise RuntimeError(
            "VisualMesh link failed: {0} -> {1}".format(
                definition.get_path_name(), configured_path
            )
        )
    return configured_path


def run():
    require_unattended_commandlet()
    # UE 5.7 Interchange's FBX import path attempts to touch Slate/Content Browser
    # state after an automated import, which is invalid in UnrealEditor-Cmd.  The
    # legacy FBX factory is commandlet-safe and consumes the FbxImportUI options
    # configured above.
    unreal.SystemLibrary.execute_console_command(
        None, "Interchange.FeatureFlags.Import.FBX 0"
    )
    unreal.SystemLibrary.execute_console_command(
        None, "Interchange.FeatureFlags.Import.FBX.ToLevel 0"
    )

    source_root = os.path.join(project_dir(), SOURCE_RELATIVE)
    definitions = select_managed_visual_definitions(
        read_facility_definitions(require_visual_mesh=False),
        GENERATED_DESTINATION_ROOT,
    )
    imported_entries = []
    failures = []

    build_manifest_path = os.path.join(saved_dir(), BUILD_MANIFEST_NAME)
    if not os.path.isfile(build_manifest_path):
        raise RuntimeError(
            "Missing completed Blender build manifest: " + build_manifest_path
        )
    with open(build_manifest_path, "r", encoding="utf-8") as handle:
        build_manifest = json.load(handle)
    if build_manifest.get("schema_version") != 1:
        raise RuntimeError("Unsupported facility mesh build manifest")
    build_records = {
        entry.get("visual_mesh_name"): entry
        for entry in build_manifest.get("entries", [])
    }

    # Fail before importing anything. This generator is intentionally limited to
    # its managed mesh root; a Definition may point at arbitrary artist content,
    # but this procedural tool must never overwrite it. Duplicate mesh targets
    # are also ambiguous because two Definitions could request different sizes.
    seen_visual_packages = set()
    preflight = []
    for facility in definitions:
        asset_name = facility["visual_mesh_name"]
        visual_package_path = facility["visual_mesh"].split(".", 1)[0]
        destination_path = visual_package_path.rsplit("/", 1)[0]
        source_file = os.path.join(source_root, asset_name + ".fbx")
        projection_digest = calculate_projection_digest(
            build_authoring_projection(facility)
        )
        build_record = build_records.get(asset_name)
        if not (
            destination_path == GENERATED_DESTINATION_ROOT
            or destination_path.startswith(GENERATED_DESTINATION_ROOT + "/")
        ):
            failures.append(
                "Refusing non-generated VisualMesh target: " + visual_package_path
            )
        if visual_package_path in seen_visual_packages:
            failures.append("Duplicate VisualMesh target: " + visual_package_path)
        seen_visual_packages.add(visual_package_path)
        if not os.path.isfile(source_file):
            failures.append("Missing FBX source: " + source_file)
        elif not build_record:
            failures.append("Missing Blender build record: " + asset_name)
        else:
            if build_record.get("projection_digest") != projection_digest:
                failures.append(
                    "Stale Blender build for current Definition projection: "
                    + asset_name
                )
            with open(source_file, "rb") as handle:
                actual_fbx_digest = hashlib.sha256(handle.read()).hexdigest()
            if build_record.get("fbx_sha256") != actual_fbx_digest:
                failures.append("FBX digest differs from completed Blender build: " + asset_name)
        preflight.append((facility, asset_name, destination_path, source_file))

    if set(build_records) != {facility[1] for facility in preflight}:
        failures.append("Blender build manifest asset set differs from current managed Definitions")

    if failures:
        raise RuntimeError(
            "Facility mesh import preflight failed before mutation: {0}".format(
                " | ".join(failures)
            )
        )

    transaction = AssetSaveTransaction(
        [facility["visual_mesh"] for facility in definitions]
        + [facility["definition"] for facility in definitions],
        "FacilityMeshImport",
    )
    report_path = os.path.join(saved_dir(), REPORT_NAME)
    pending_report_path = report_path + ".pending"
    try:
        pending_saves = []
        for facility, asset_name, destination_path, source_file in preflight:
            if not unreal.EditorAssetLibrary.does_directory_exist(destination_path):
                unreal.EditorAssetLibrary.make_directory(destination_path)

            try:
                mesh = import_mesh(source_file, asset_name, destination_path)
                if mesh is None:
                    raise RuntimeError("StaticMesh import returned no asset")
                collision_count = configure_mesh(mesh, facility)
                visual_path = link_definition(facility["definition_object"], mesh)
                pending_saves.append((mesh, facility["definition_object"]))
                imported_entries.append(
                    {
                        "source": source_file,
                        "asset": mesh.get_path_name(),
                        "definition": facility["definition"],
                        "visual_mesh": visual_path,
                        "collision_count": collision_count,
                    }
                )
                unreal.log(
                    "[FF_FACILITY_MESH] Imported and linked {0}".format(
                        mesh.get_path_name()
                    )
                )
            except Exception as error:
                failures.append("{0}: {1}".format(asset_name, error))
                unreal.log_error("[FF_FACILITY_MESH] {0}".format(failures[-1]))

        if failures or len(imported_entries) != len(definitions):
            raise RuntimeError(
                "Facility mesh import failed ({0}/{1} imported)".format(
                    len(imported_entries), len(definitions)
                )
            )

        for mesh, definition in pending_saves:
            if not unreal.EditorAssetLibrary.save_loaded_asset(
                mesh, only_if_is_dirty=False
            ):
                raise RuntimeError("Could not save imported mesh " + mesh.get_path_name())
            if not unreal.EditorAssetLibrary.save_loaded_asset(
                definition, only_if_is_dirty=False
            ):
                raise RuntimeError(
                    "Could not save facility definition " + definition.get_path_name()
                )
        report = {"assets": imported_entries, "failures": []}
        with open(pending_report_path, "w", encoding="utf-8") as handle:
            json.dump(report, handle, ensure_ascii=False, indent=2)
        transaction.commit()
        try:
            os.replace(pending_report_path, report_path)
        except Exception as error:
            unreal.log_warning(
                "[FF_FACILITY_MESH] Assets were committed, but the disposable "
                "report could not be published from {0}: {1}".format(
                    pending_report_path, error
                )
            )
    except Exception:
        transaction.rollback()
        try:
            if os.path.isfile(pending_report_path):
                os.remove(pending_report_path)
        except Exception as cleanup_error:
            unreal.log_warning(
                "[FF_FACILITY_MESH] Could not remove pending report {0}: {1}".format(
                    pending_report_path, cleanup_error
                )
            )
        raise
    unreal.log(
        "[FF_FACILITY_MESH] Imported and linked {0} facility meshes".format(
            len(imported_entries)
        )
    )


run()
