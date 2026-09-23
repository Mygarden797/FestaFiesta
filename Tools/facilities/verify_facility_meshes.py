# -*- coding: utf-8 -*-
# verify_facility_meshes.py
# 역할: 시설 모델·Data Asset·썸네일 작업에서 저장된 결과가 요구 조건에 맞는지 검증한다.

"""Verify the procedurally managed representative facility meshes.

This is an art-pipeline profile, not the UFFZSFacilityDefinition validity gate.
Definitions may legally omit VisualMesh or reference other artist content; use
the C++ DataValidation/content automation tests to validate all Definitions.
"""

import json
import os
import sys

import unreal

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)
from ffzs_definition_source import (
    read_facility_definitions,
    select_managed_visual_definitions,
)

REPORT_PATH = os.path.join(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
    "FacilityMeshVerificationReport.json",
)
GENERATED_MESH_ROOT = "/Game/FestaFiesta/Zone/Test_Asset/Asset/"


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

    definitions = read_facility_definitions(require_visual_mesh=False)
    managed_definitions = select_managed_visual_definitions(
        definitions, GENERATED_MESH_ROOT.rstrip("/")
    )
    seen_mesh_paths = set()
    for record in managed_definitions:
        asset_name = record["visual_mesh_name"]
        asset_path = record["visual_mesh"]
        definition_path = record["definition"]
        mesh = record["visual_mesh_object"]
        definition = record["definition_object"]
        if not isinstance(mesh, unreal.StaticMesh):
            failures.append("Missing StaticMesh: " + asset_path)
            continue
        if asset_path in seen_mesh_paths:
            failures.append("Duplicate managed representative mesh: " + asset_path)
            continue
        seen_mesh_paths.add(asset_path)
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
            "facility_definition_id": record["facility_definition_id"],
            "visual_mesh": configured_path,
            "target_size_cm": record["target_size_cm"],
            "visual_sizing_mode": record["visual_sizing_mode"],
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
        target = record["target_size_cm"]
        source_size_mode = (
            record["visual_sizing_mode"].replace("_", "").lower()
            == "sourcemeshsize"
        )
        if source_size_mode and (
            size_x > float(target["x"]) + 0.5
            or size_y > float(target["y"]) + 0.5
            or float(maximum.z) > float(target["z"]) + 0.5
        ):
            failures.append(
                "SourceMeshSize exceeds Definition target for {0}: size={1}, target={2}".format(
                    asset_name, entry["size_cm"], target
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
        # Simple collision is optional. Runtime placement and queue authority come
        # from the Definition Spec; the VisualMesh component is always NoCollision.

    result = {"assets": entries, "failures": failures}
    with open(REPORT_PATH, "w", encoding="utf-8") as handle:
        json.dump(result, handle, ensure_ascii=False, indent=2)

    if failures or len(entries) != len(managed_definitions):
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
