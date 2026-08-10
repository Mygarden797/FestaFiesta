# -*- coding: utf-8 -*-
"""Verify all generated FFZS facility StaticMeshes and VisualMesh links."""

import json
import os

import unreal


ASSET_ROOT = "/Game/FestaFiesta/Zone/Test_Asset/Asset"
REPORT_PATH = os.path.join(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
    "FacilityMeshVerificationReport.json",
)

FACILITIES = [
    (
        "SM_FFZS_DadaPochaFoodTruck",
        "/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Booth/"
        "PDA_FFZS_Booth_FoodStall_DadaPochaFoodTruck",
    ),
    (
        "SM_FFZS_MainInformation",
        "/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Info/"
        "PDA_FFZS_Info_GeneralInfo_MainInformation",
    ),
    (
        "SM_FFZS_SunsetMainStage",
        "/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Performance/"
        "PDA_FFZS_Performance_MainStage_SunsetMainStage",
    ),
    (
        "SM_FFZS_SunsetLandmark",
        "/Game/FestaFiesta/Zone/Data/FacilityDefinitions/PhotoSpot/"
        "PDA_FFZS_PhotoSpot_Landmark_SunsetLandmark",
    ),
    (
        "SM_FFZS_MainRestArea",
        "/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Rest/"
        "PDA_FFZS_Rest_Seating_MainRestArea",
    ),
    (
        "SM_FFZS_MedicalTent",
        "/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Safety/"
        "PDA_FFZS_Safety_FirstAid_MedicalTent",
    ),
    (
        "SM_FFZS_MainRestroom",
        "/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Toilet/"
        "PDA_FFZS_Toilet_Standard_MainRestroom",
    ),
]


def vec(value):
    return {
        "x": round(float(value.x), 3),
        "y": round(float(value.y), 3),
        "z": round(float(value.z), 3),
    }


def material_entries(mesh):
    result = []
    for slot in mesh.get_editor_property("static_materials"):
        material = slot.get_editor_property("material_interface")
        result.append(
            {
                "slot": str(slot.get_editor_property("material_slot_name")),
                "material": material.get_path_name() if material else "",
            }
        )
    return result


def collision_count(mesh):
    try:
        subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        if subsystem is not None:
            return int(subsystem.get_simple_collision_count(mesh))
    except Exception as error:
        unreal.log_warning(
            "[FF_FACILITY_MESH_VERIFY] Collision query failed for {0}: {1}".format(
                mesh.get_name(), error
            )
        )
    try:
        body_setup = mesh.get_editor_property("body_setup")
        if body_setup is None:
            return 0
        aggregate = body_setup.get_editor_property("agg_geom")
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
            "[FF_FACILITY_MESH_VERIFY] Direct collision query failed for {0}: {1}".format(
                mesh.get_name(), error
            )
        )
        return -1


def run():
    entries = []
    failures = []

    for asset_name, definition_path in FACILITIES:
        asset_path = "{0}/{1}".format(ASSET_ROOT, asset_name)
        mesh = unreal.EditorAssetLibrary.load_asset(asset_path)
        definition = unreal.EditorAssetLibrary.load_asset(definition_path)
        if not isinstance(mesh, unreal.StaticMesh):
            failures.append("Missing StaticMesh: " + asset_path)
            continue
        if definition is None:
            failures.append("Missing facility definition: " + definition_path)
            continue

        bounds = mesh.get_bounding_box()
        minimum = bounds.min
        maximum = bounds.max
        size_x = float(maximum.x - minimum.x)
        size_y = float(maximum.y - minimum.y)
        size_z = float(maximum.z - minimum.z)
        center_x = float((maximum.x + minimum.x) * 0.5)
        center_y = float((maximum.y + minimum.y) * 0.5)

        configured = definition.get_editor_property("spec").get_editor_property(
            "visual_mesh"
        )
        configured_path = configured.get_path_name() if configured else ""
        materials = material_entries(mesh)
        simple_collisions = collision_count(mesh)
        entry = {
            "asset": mesh.get_path_name(),
            "class": mesh.get_class().get_path_name(),
            "definition": definition_path,
            "visual_mesh": configured_path,
            "bounds_min_cm": vec(minimum),
            "bounds_max_cm": vec(maximum),
            "size_cm": {
                "x": round(size_x, 3),
                "y": round(size_y, 3),
                "z": round(size_z, 3),
            },
            "vertices_lod0": int(mesh.get_num_vertices(0)),
            "triangles_lod0": int(mesh.get_num_triangles(0)),
            "sections_lod0": int(mesh.get_num_sections(0)),
            "materials": materials,
            "simple_collisions": simple_collisions,
        }
        entries.append(entry)
        unreal.log("[FF_FACILITY_MESH_VERIFY] {0}".format(json.dumps(entry)))

        if configured_path != mesh.get_path_name():
            failures.append(
                "VisualMesh mismatch: {0} -> {1}, expected {2}".format(
                    definition_path, configured_path, mesh.get_path_name()
                )
            )
        if abs(center_x) > 2.0 or abs(center_y) > 2.0 or abs(float(minimum.z)) > 2.0:
            failures.append(
                "Pivot/bounds origin mismatch for {0}: min={1}, max={2}".format(
                    asset_name, vec(minimum), vec(maximum)
                )
            )
        if size_x > 600.5 or size_y > 300.5 or float(maximum.z) > 250.5:
            failures.append(
                "Mesh exceeds 600x300x250cm contract: {0} size={1}".format(
                    asset_name, entry["size_cm"]
                )
            )
        if size_x < 100.0 or size_y < 100.0 or size_z < 100.0:
            failures.append("Implausibly small facility mesh: " + asset_name)
        if entry["vertices_lod0"] <= 0 or entry["triangles_lod0"] <= 0:
            failures.append("Empty LOD0 render data: " + asset_name)
        if entry["triangles_lod0"] > 50000:
            failures.append("Unexpectedly dense low-poly mesh: " + asset_name)
        if len(materials) < 3:
            failures.append("Too few material slots for representative mesh: " + asset_name)
        if any(not item["material"] for item in materials):
            failures.append("Unassigned material slot: " + asset_name)
        if simple_collisions == 0:
            failures.append("No simple collision: " + asset_name)

    result = {"assets": entries, "failures": failures}
    with open(REPORT_PATH, "w", encoding="utf-8") as handle:
        json.dump(result, handle, ensure_ascii=False, indent=2)

    if failures or len(entries) != len(FACILITIES):
        for failure in failures:
            unreal.log_error("[FF_FACILITY_MESH_VERIFY] " + failure)
        raise RuntimeError(
            "Facility mesh verification failed ({0} assets, {1} failures)".format(
                len(entries), len(failures)
            )
        )
    unreal.log(
        "[FF_FACILITY_MESH_VERIFY] Verified {0} facility meshes".format(len(entries))
    )


run()
