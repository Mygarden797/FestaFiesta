# -*- coding: utf-8 -*-
"""Create the shared FFZS low-poly material palette and assign mesh slots."""

import json
import os
import sys

import unreal

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)
from ffzs_definition_source import (
    read_facility_definitions,
    require_unattended_commandlet,
    select_managed_visual_definitions,
)
from ffzs_asset_save_transaction import AssetSaveTransaction

ASSET_ROOT = "/Game/FestaFiesta/Zone/Test_Asset/Asset"
MATERIAL_ROOT = ASSET_ROOT + "/Materials"
REPORT_PATH = os.path.join(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
    "FacilityMeshMaterialReport.json",
)

# Values intentionally mirror the linear-color palette in build_facility_meshes.py.
# Tuple: base RGB, roughness, metallic, emissive strength.
PALETTE = {
    "FF_WoodDark": ((0.16, 0.048, 0.014), 0.82, 0.0, 0.0),
    "FF_WoodWarm": ((0.46, 0.14, 0.028), 0.72, 0.0, 0.0),
    "FF_Teal": ((0.008, 0.43, 0.50), 0.66, 0.0, 0.0),
    "FF_TealDark": ((0.006, 0.13, 0.17), 0.78, 0.05, 0.0),
    "FF_Cream": ((0.90, 0.66, 0.36), 0.78, 0.0, 0.0),
    "FF_Metal": ((0.035, 0.045, 0.048), 0.42, 0.52, 0.0),
    "FF_AmberGlow": ((1.0, 0.48, 0.025), 0.36, 0.0, 8.0),
    "FF_SunsetGlow": ((1.0, 0.32, 0.015), 0.42, 0.0, 5.0),
    "FF_Green": ((0.035, 0.36, 0.105), 0.88, 0.0, 0.0),
    "FF_Medical": ((0.90, 0.045, 0.025), 0.62, 0.0, 0.0),
    "FF_BlueGrey": ((0.14, 0.34, 0.40), 0.72, 0.05, 0.0),
    "FF_Glass": ((0.06, 0.42, 0.52), 0.24, 0.08, 0.0),
}


def linear_color(rgb, multiplier=1.0):
    return unreal.LinearColor(
        float(rgb[0]) * multiplier,
        float(rgb[1]) * multiplier,
        float(rgb[2]) * multiplier,
        1.0,
    )


def material_asset_name(slot_name):
    return "M_FFZS_" + slot_name.removeprefix("FF_")


def create_or_update_material(slot_name, values):
    rgb, roughness, metallic, emissive_strength = values
    asset_name = material_asset_name(slot_name)
    asset_path = MATERIAL_ROOT + "/" + asset_name
    material = (
        unreal.EditorAssetLibrary.load_asset(asset_path)
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path)
        else None
    )
    if material is not None:
        owner = str(
            unreal.EditorAssetLibrary.get_metadata_tag(material, "FFZS.PaletteSlot")
            or ""
        )
        if not isinstance(material, unreal.Material) or owner != slot_name:
            raise RuntimeError(
                "Refusing to overwrite unowned palette path {0} (owner={1})".format(
                    asset_path, owner or "<missing>"
                )
            )
    if not isinstance(material, unreal.Material):
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            asset_name,
            MATERIAL_ROOT,
            unreal.Material,
            unreal.MaterialFactoryNew(),
        )
    if not isinstance(material, unreal.Material):
        raise RuntimeError("Could not create Material: " + asset_path)

    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)

    base = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant3Vector, -480, -120
    )
    base.set_editor_property("constant", linear_color(rgb))
    if not unreal.MaterialEditingLibrary.connect_material_property(
        base, "", unreal.MaterialProperty.MP_BASE_COLOR
    ):
        raise RuntimeError("Could not connect BaseColor for " + asset_name)

    rough = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -480, 30
    )
    rough.set_editor_property("r", float(roughness))
    unreal.MaterialEditingLibrary.connect_material_property(
        rough, "", unreal.MaterialProperty.MP_ROUGHNESS
    )

    metal = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -480, 150
    )
    metal.set_editor_property("r", float(metallic))
    unreal.MaterialEditingLibrary.connect_material_property(
        metal, "", unreal.MaterialProperty.MP_METALLIC
    )

    if emissive_strength > 0.0:
        emissive = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionConstant3Vector, -480, 280
        )
        emissive.set_editor_property(
            "constant", linear_color(rgb, float(emissive_strength))
        )
        unreal.MaterialEditingLibrary.connect_material_property(
            emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
        )

    material.set_editor_property("two_sided", False)
    unreal.EditorAssetLibrary.set_metadata_tag(
        material, "FFZS.PaletteSlot", slot_name
    )
    unreal.MaterialEditingLibrary.recompile_material(material)
    return material


def direct_collision_count(mesh):
    """Read BodySetup aggregate geometry without an editor subsystem."""
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
            "[FF_FACILITY_MATERIAL] Direct collision query failed for {0}: {1}".format(
                mesh.get_name(), error
            )
        )
        return -1


def assign_mesh_materials(mesh, materials):
    assignments = []
    slots = mesh.get_editor_property("static_materials")
    for index, slot in enumerate(slots):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        material = materials.get(slot_name)
        if material is None:
            raise RuntimeError(
                "Unknown palette slot {0} on {1}".format(slot_name, mesh.get_name())
            )
        mesh.set_material(index, material)
        assignments.append(
            {
                "slot": slot_name,
                "material": material.get_path_name(),
            }
        )
    return assignments


def preflight_managed_meshes(definitions):
    """Prove the complete batch is owned and palette-compatible before mutation."""
    failures = []
    preflight = []
    seen_visual_packages = set()
    for record in definitions:
        mesh = record["visual_mesh_object"]
        mesh_path = record["visual_mesh"]
        visual_package_path = mesh_path.split(".", 1)[0]
        if not (
            visual_package_path == ASSET_ROOT
            or visual_package_path.startswith(ASSET_ROOT + "/")
        ):
            failures.append(
                "Refusing non-managed VisualMesh target: " + visual_package_path
            )
        if visual_package_path in seen_visual_packages:
            failures.append("Duplicate VisualMesh target: " + visual_package_path)
        seen_visual_packages.add(visual_package_path)
        if not isinstance(mesh, unreal.StaticMesh):
            failures.append("Missing StaticMesh: " + mesh_path)
            continue

        slot_names = []
        try:
            for slot in mesh.get_editor_property("static_materials"):
                slot_name = str(slot.get_editor_property("material_slot_name"))
                slot_names.append(slot_name)
                if slot_name not in PALETTE:
                    failures.append(
                        "Unknown palette slot {0} on {1}".format(
                            slot_name, mesh.get_name()
                        )
                    )
        except Exception as error:
            failures.append(
                "Could not inspect material slots on {0}: {1}".format(
                    mesh.get_name(), error
                )
            )
        preflight.append((record, mesh, slot_names))

    if failures:
        raise RuntimeError(
            "Facility material preflight failed before mutation: {0}".format(
                " | ".join(failures)
            )
        )
    return preflight


def run():
    require_unattended_commandlet()
    definitions = select_managed_visual_definitions(
        read_facility_definitions(require_visual_mesh=False), ASSET_ROOT
    )
    preflight = preflight_managed_meshes(definitions)

    palette_failures = []
    for slot_name in PALETTE:
        asset_path = MATERIAL_ROOT + "/" + material_asset_name(slot_name)
        if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            continue
        material = unreal.EditorAssetLibrary.load_asset(asset_path)
        owner = str(
            unreal.EditorAssetLibrary.get_metadata_tag(material, "FFZS.PaletteSlot")
            or ""
        ) if material else ""
        if not isinstance(material, unreal.Material) or owner != slot_name:
            palette_failures.append(
                "Unowned or mismatched palette asset {0} (owner={1})".format(
                    asset_path, owner or "<missing>"
                )
            )
    if palette_failures:
        raise RuntimeError(
            "Facility palette ownership preflight failed before mutation: "
            + " | ".join(palette_failures)
        )

    transaction = AssetSaveTransaction(
        [record["visual_mesh"] for record in definitions]
        + [MATERIAL_ROOT + "/" + material_asset_name(slot_name) for slot_name in PALETTE],
        "FacilityMaterials",
    )

    pending_report_path = REPORT_PATH + ".pending"
    try:
        if not unreal.EditorAssetLibrary.does_directory_exist(MATERIAL_ROOT):
            unreal.EditorAssetLibrary.make_directory(MATERIAL_ROOT)

        materials = {}
        for slot_name, values in PALETTE.items():
            material = create_or_update_material(slot_name, values)
            materials[slot_name] = material
            unreal.log(
                "[FF_FACILITY_MATERIAL] Configured {0}".format(material.get_path_name())
            )

        entries = []
        failures = []
        pending_mesh_saves = []
        for record, mesh, _slot_names in preflight:
            mesh_name = record["visual_mesh_name"]
            try:
                assignments = assign_mesh_materials(mesh, materials)
                pending_mesh_saves.append(mesh)
                entries.append(
                    {
                        "mesh": mesh.get_path_name(),
                        "materials": assignments,
                        "simple_collisions": direct_collision_count(mesh),
                    }
                )
                unreal.log(
                    "[FF_FACILITY_MATERIAL] Assigned {0} slots on {1}".format(
                        len(assignments), mesh_name
                    )
                )
            except Exception as error:
                failures.append("{0}: {1}".format(mesh_name, error))

        if failures or len(entries) != len(definitions):
            result = {
                "materials": len(materials),
                "assets": entries,
                "failures": failures,
            }
            with open(REPORT_PATH, "w", encoding="utf-8") as handle:
                json.dump(result, handle, ensure_ascii=False, indent=2)
            for failure in failures:
                unreal.log_error("[FF_FACILITY_MATERIAL] " + failure)
            raise RuntimeError("Facility material configuration failed")

        for material in materials.values():
            if not unreal.EditorAssetLibrary.save_loaded_asset(
                material, only_if_is_dirty=False
            ):
                raise RuntimeError("Could not save Material: " + material.get_path_name())
        for mesh in pending_mesh_saves:
            if not unreal.EditorAssetLibrary.save_loaded_asset(
                mesh, only_if_is_dirty=False
            ):
                raise RuntimeError("Could not save StaticMesh: " + mesh.get_path_name())
        result = {"materials": len(materials), "assets": entries, "failures": []}
        with open(pending_report_path, "w", encoding="utf-8") as handle:
            json.dump(result, handle, ensure_ascii=False, indent=2)
        transaction.commit()
        try:
            os.replace(pending_report_path, REPORT_PATH)
        except Exception as error:
            unreal.log_warning(
                "[FF_FACILITY_MATERIAL] Assets were committed, but the disposable "
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
                "[FF_FACILITY_MATERIAL] Could not remove pending report {0}: {1}".format(
                    pending_report_path, cleanup_error
                )
            )
        raise
    unreal.log(
        "[FF_FACILITY_MATERIAL] Configured {0} materials on {1} meshes".format(
            len(materials), len(entries)
        )
    )


run()
