# -*- coding: utf-8 -*-
"""Import the generated festival facility FBXs and link them to FFZS definitions.

Run from UnrealEditor-Cmd after ``build_facility_meshes.py`` has generated the
FBX source files.  The visual contract used by the placement system is:

* Unreal centimeters at scale 1
* pivot at the footprint centre, on the ground (Z=0)
* local +Y is the customer-facing side
* all geometry remains inside the 600 x 300 x 250 cm facility bounds
"""

import json
import os

import unreal


DESTINATION = "/Game/FestaFiesta/Zone/Test_Asset/Asset"
SOURCE_RELATIVE = os.path.join("SourceAssets", "Zone", "FacilityMeshes")
REPORT_NAME = "FacilityMeshImportReport.json"

FACILITIES = [
    {
        "name": "SM_FFZS_DadaPochaFoodTruck",
        "definition": (
            "/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Booth/"
            "PDA_FFZS_Booth_FoodStall_DadaPochaFoodTruck"
        ),
    },
    {
        "name": "SM_FFZS_MainInformation",
        "definition": (
            "/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Info/"
            "PDA_FFZS_Info_GeneralInfo_MainInformation"
        ),
    },
    {
        "name": "SM_FFZS_SunsetMainStage",
        "definition": (
            "/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Performance/"
            "PDA_FFZS_Performance_MainStage_SunsetMainStage"
        ),
    },
    {
        "name": "SM_FFZS_SunsetLandmark",
        "definition": (
            "/Game/FestaFiesta/Zone/Data/FacilityDefinitions/PhotoSpot/"
            "PDA_FFZS_PhotoSpot_Landmark_SunsetLandmark"
        ),
    },
    {
        "name": "SM_FFZS_MainRestArea",
        "definition": (
            "/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Rest/"
            "PDA_FFZS_Rest_Seating_MainRestArea"
        ),
    },
    {
        "name": "SM_FFZS_MedicalTent",
        "definition": (
            "/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Safety/"
            "PDA_FFZS_Safety_FirstAid_MedicalTent"
        ),
    },
    {
        "name": "SM_FFZS_MainRestroom",
        "definition": (
            "/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Toilet/"
            "PDA_FFZS_Toilet_Standard_MainRestroom"
        ),
    },
]


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
    try_set(options, "import_materials", True)
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


def import_mesh(source_file, asset_name):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source_file)
    task.set_editor_property("destination_path", DESTINATION)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    try_set(task, "replace_existing_settings", True)
    task.set_editor_property("save", False)
    task.set_editor_property("options", make_fbx_options())

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    for imported_path in task.get_editor_property("imported_object_paths") or []:
        imported = unreal.EditorAssetLibrary.load_asset(imported_path)
        if isinstance(imported, unreal.StaticMesh):
            return imported

    expected_path = "{0}/{1}".format(DESTINATION, asset_name)
    imported = unreal.EditorAssetLibrary.load_asset(expected_path)
    return imported if isinstance(imported, unreal.StaticMesh) else None


def configure_mesh(mesh):
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

    # Imported UCX collision is preferred.  Fall back to one deterministic box;
    # FFZS runtime components use NoCollision, so this is editor/reuse metadata.
    collision_count = 0
    try:
        subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        if subsystem is not None:
            collision_count = int(subsystem.get_simple_collision_count(mesh))
            if collision_count == 0:
                subsystem.add_simple_collisions(
                    mesh, unreal.ScriptingCollisionShapeType.BOX
                )
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

    unreal.EditorAssetLibrary.set_metadata_tag(
        mesh, "FFZS.FacilityVisualContract", "600x300x250cm;PivotGroundCenter;Front+Y"
    )
    unreal.EditorAssetLibrary.set_metadata_tag(
        mesh, "FFZS.GeneratedSource", "SourceAssets/Zone/FacilityMeshes/{0}.fbx".format(
            mesh.get_name()
        )
    )
    unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
    return collision_count


def link_definition(definition_path, mesh):
    definition = unreal.EditorAssetLibrary.load_asset(definition_path)
    if definition is None:
        raise RuntimeError("Missing facility definition: " + definition_path)

    spec = definition.get_editor_property("spec")
    spec.set_editor_property("visual_mesh", mesh)
    definition.set_editor_property("spec", spec)
    unreal.EditorAssetLibrary.save_loaded_asset(definition, only_if_is_dirty=False)

    configured = definition.get_editor_property("spec").get_editor_property("visual_mesh")
    configured_path = configured.get_path_name() if configured else ""
    if configured_path != mesh.get_path_name():
        raise RuntimeError(
            "VisualMesh link failed: {0} -> {1}".format(
                definition_path, configured_path
            )
        )
    return configured_path


def run():
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
    imported_entries = []
    failures = []

    if not unreal.EditorAssetLibrary.does_directory_exist(DESTINATION):
        unreal.EditorAssetLibrary.make_directory(DESTINATION)

    for facility in FACILITIES:
        asset_name = facility["name"]
        source_file = os.path.join(source_root, asset_name + ".fbx")
        if not os.path.isfile(source_file):
            failures.append("Missing FBX source: " + source_file)
            continue

        try:
            mesh = import_mesh(source_file, asset_name)
            if mesh is None:
                raise RuntimeError("StaticMesh import returned no asset")
            collision_count = configure_mesh(mesh)
            visual_path = link_definition(facility["definition"], mesh)
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
                "[FF_FACILITY_MESH] Imported and linked {0}".format(mesh.get_path_name())
            )
        except Exception as error:
            failures.append("{0}: {1}".format(asset_name, error))
            unreal.log_error("[FF_FACILITY_MESH] {0}".format(failures[-1]))

    report = {"assets": imported_entries, "failures": failures}
    report_path = os.path.join(saved_dir(), REPORT_NAME)
    with open(report_path, "w", encoding="utf-8") as handle:
        json.dump(report, handle, ensure_ascii=False, indent=2)

    if failures or len(imported_entries) != len(FACILITIES):
        raise RuntimeError(
            "Facility mesh import failed ({0}/{1} imported)".format(
                len(imported_entries), len(FACILITIES)
            )
        )
    unreal.log(
        "[FF_FACILITY_MESH] Imported and linked {0} facility meshes".format(
            len(imported_entries)
        )
    )


run()
