# -*- coding: utf-8 -*-
"""Reload and verify the surfing Test_Asset pair in a fresh commandlet."""

from __future__ import annotations

import json
import os
import struct
import sys

import unreal


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from ffzs_definition_source import require_unattended_commandlet


TEST_ASSET_ID = "Booth_ActivityGame_Surfing"
MESH_NAME = "SM_FFZS_" + TEST_ASSET_ID
TEXTURE_NAME = "T_UI_BuildCatalog_" + TEST_ASSET_ID
MESH_PATH = "/Game/FestaFiesta/Zone/Test_Asset/Asset/" + MESH_NAME
TEXTURE_PATH = "/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/Images/" + TEXTURE_NAME
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
REPORT_NAME = "SurfingTestAssetVerificationReport.json"
EXPECTED_PNG = (324, 260, 8, 2)
EXPECTED_CONTRACT = (
    "ReferenceArtOnly;PivotGroundCenter;Front+Y;NoRuntimePlacementAuthority"
)
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


def png_contract(path):
    with open(path, "rb") as handle:
        header = handle.read(26)
    if len(header) != 26 or header[:8] != b"\x89PNG\r\n\x1a\n":
        return None
    width, height = struct.unpack(">II", header[16:24])
    return width, height, header[24], header[25]


def metadata_value(asset, key):
    return str(unreal.EditorAssetLibrary.get_metadata_tag(asset, key) or "")


def collision_count(mesh):
    try:
        subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        if subsystem is not None:
            count = int(subsystem.get_simple_collision_count(mesh))
            if count > 0:
                return count
    except Exception as error:
        unreal.log_warning(
            "[FF_SURFING_VERIFY] Collision subsystem query failed: " + str(error)
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
            "[FF_SURFING_VERIFY] Direct collision query failed: " + str(error)
        )
        return -1


def vector(value):
    return [
        round(float(value.x), 3),
        round(float(value.y), 3),
        round(float(value.z), 3),
    ]


def run():
    require_unattended_commandlet()
    root = project_dir()
    fbx_path = os.path.join(root, FBX_RELATIVE)
    png_path = os.path.join(root, PNG_RELATIVE)
    mesh = unreal.EditorAssetLibrary.load_asset(MESH_PATH)
    texture = unreal.EditorAssetLibrary.load_asset(TEXTURE_PATH)
    failures = []

    if not os.path.isfile(fbx_path):
        failures.append("Missing FBX source: " + fbx_path)
    if not os.path.isfile(png_path):
        failures.append("Missing PNG source: " + png_path)
        png_spec = None
    else:
        png_spec = png_contract(png_path)
        if png_spec != EXPECTED_PNG:
            failures.append(
                "PNG contract mismatch: expected={0}, actual={1}".format(
                    EXPECTED_PNG, png_spec
                )
            )

    if not isinstance(mesh, unreal.StaticMesh):
        failures.append("Missing StaticMesh: " + MESH_PATH)
    if not isinstance(texture, unreal.Texture2D):
        failures.append("Missing Texture2D: " + TEXTURE_PATH)

    if failures:
        report = {"test_asset_id": TEST_ASSET_ID, "failures": failures}
    else:
        bounds = mesh.get_bounding_box()
        minimum = bounds.min
        maximum = bounds.max
        size = [
            float(maximum.x - minimum.x),
            float(maximum.y - minimum.y),
            float(maximum.z - minimum.z),
        ]
        center_xy = [
            float((maximum.x + minimum.x) * 0.5),
            float((maximum.y + minimum.y) * 0.5),
        ]
        vertices = int(mesh.get_num_vertices(0))
        triangles = int(mesh.get_num_triangles(0))
        sections = int(mesh.get_num_sections(0))
        collisions = collision_count(mesh)

        materials = []
        actual_slots = set()
        for slot in mesh.get_editor_property("static_materials"):
            slot_name = str(slot.get_editor_property("material_slot_name"))
            material = slot.get_editor_property("material_interface")
            material_path = material.get_path_name() if material else ""
            actual_slots.add(slot_name)
            materials.append({"slot": slot_name, "material": material_path})
            expected_material = (
                "/Game/FestaFiesta/Zone/Test_Asset/Asset/Materials/M_FFZS_"
                + slot_name[3:]
                + ".M_FFZS_"
                + slot_name[3:]
                if slot_name.startswith("FF_")
                else ""
            )
            if material_path != expected_material:
                failures.append(
                    "Shared material mismatch for slot {0}: {1}".format(
                        slot_name, material_path
                    )
                )

        texture_settings = {
            "compression_settings": str(
                texture.get_editor_property("compression_settings")
            ),
            "lod_group": str(texture.get_editor_property("lod_group")),
            "mip_gen_settings": str(
                texture.get_editor_property("mip_gen_settings")
            ),
            "srgb": bool(texture.get_editor_property("srgb")),
            "never_stream": bool(texture.get_editor_property("never_stream")),
            "address_x": str(texture.get_editor_property("address_x")),
            "address_y": str(texture.get_editor_property("address_y")),
        }

        if mesh.get_path_name() != MESH_PATH + "." + MESH_NAME:
            failures.append("StaticMesh object path mismatch")
        if texture.get_path_name() != TEXTURE_PATH + "." + TEXTURE_NAME:
            failures.append("Texture2D object path mismatch")
        if abs(center_xy[0]) > 2.0 or abs(center_xy[1]) > 2.0:
            failures.append("StaticMesh footprint is not centred on the pivot")
        if abs(float(minimum.z)) > 2.0:
            failures.append("StaticMesh pivot is not grounded")
        if any(axis < 100.0 for axis in size):
            failures.append("StaticMesh bounds are implausibly small")
        if vertices <= 0 or triangles <= 0 or triangles > 50000:
            failures.append("StaticMesh LOD0 geometry is invalid")
        if sections <= 0:
            failures.append("StaticMesh LOD0 has no sections")
        if actual_slots != EXPECTED_MATERIAL_SLOTS:
            failures.append(
                "Material slot mismatch: expected={0}, actual={1}".format(
                    sorted(EXPECTED_MATERIAL_SLOTS), sorted(actual_slots)
                )
            )
        if any(not item["material"] for item in materials):
            failures.append("StaticMesh contains an unassigned material")
        if collisions <= 0:
            failures.append("StaticMesh has no simple collision")

        required_texture_settings = (
            (
                "compression_settings",
                unreal.TextureCompressionSettings.TC_EDITOR_ICON,
            ),
            ("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI),
            (
                "mip_gen_settings",
                unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
            ),
            ("srgb", True),
            ("never_stream", True),
            ("address_x", unreal.TextureAddress.TA_CLAMP),
            ("address_y", unreal.TextureAddress.TA_CLAMP),
        )
        for property_name, expected in required_texture_settings:
            if texture.get_editor_property(property_name) != expected:
                failures.append("Texture setting mismatch: " + property_name)

        expected_mesh_source = FBX_RELATIVE.replace("\\", "/")
        expected_texture_source = PNG_RELATIVE.replace("\\", "/")
        if metadata_value(mesh, "FFZS.FacilityVisualContract") != EXPECTED_CONTRACT:
            failures.append("StaticMesh reference-art contract mismatch")
        if metadata_value(mesh, "FFZS.GeneratedSource") != expected_mesh_source:
            failures.append("StaticMesh generated-source metadata mismatch")
        if metadata_value(texture, "FFZS.GeneratedSource") != expected_texture_source:
            failures.append("Texture2D generated-source metadata mismatch")
        for asset, label in ((mesh, "StaticMesh"), (texture, "Texture2D")):
            if metadata_value(asset, "FFZS.TestAssetId") != TEST_ASSET_ID:
                failures.append(label + " TestAssetId metadata mismatch")

        report = {
            "test_asset_id": TEST_ASSET_ID,
            "mesh": mesh.get_path_name(),
            "texture": texture.get_path_name(),
            "fbx_source": fbx_path,
            "png_source": png_path,
            "png_contract": list(png_spec),
            "bounds_min_cm": vector(minimum),
            "bounds_max_cm": vector(maximum),
            "size_cm": [round(value, 3) for value in size],
            "vertices_lod0": vertices,
            "triangles_lod0": triangles,
            "sections_lod0": sections,
            "simple_collisions": collisions,
            "materials": materials,
            "texture_settings": texture_settings,
            "failures": failures,
        }

    report_path = os.path.join(saved_dir(), REPORT_NAME)
    with open(report_path, "w", encoding="utf-8") as handle:
        json.dump(report, handle, ensure_ascii=False, indent=2)
    unreal.log("[FF_SURFING_VERIFY] " + json.dumps(report, ensure_ascii=False))
    if failures:
        for failure in failures:
            unreal.log_error("[FF_SURFING_VERIFY] " + failure)
        raise RuntimeError(
            "Surfing Test_Asset verification failed with {0} issue(s)".format(
                len(failures)
            )
        )


run()
