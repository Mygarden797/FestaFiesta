# -*- coding: utf-8 -*-
"""Verify coverage and technical contracts for all 29 Notion examples."""

import json
import os
import struct

import unreal


MESH_ROOT = "/Game/FestaFiesta/Zone/Test_Asset/Asset"
TEXTURE_ROOT = "/Game/FestaFiesta/Zone/Test_Asset/image"
MANIFEST_RELATIVE = os.path.join(
    "Tools", "facilities", "notion_facility_examples.json"
)
FBX_RELATIVE = os.path.join("SourceAssets", "Zone", "FacilityMeshes")
PNG_RELATIVE = os.path.join(
    "Content", "FestaFiesta", "Zone", "Test_Asset", "image"
)
REPORT_NAME = "NotionFacilityVerificationReport.json"


def project_dir():
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())


def saved_dir():
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())


def canonical_names(entry):
    definition_id = entry["definition_id"]
    return (
        "SM_FFZS_" + definition_id,
        "T_UI_BuildCatalog_" + definition_id,
    )


def png_dimensions(path):
    with open(path, "rb") as handle:
        header = handle.read(24)
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n":
        return None
    return struct.unpack(">II", header[16:24])


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
            subsystem_count = int(subsystem.get_simple_collision_count(mesh))
            if subsystem_count > 0:
                return subsystem_count
    except Exception as error:
        unreal.log_warning(
            "[FF_NOTION_VERIFY] Collision query failed for {0}: {1}".format(
                mesh.get_name(), error
            )
        )
    # Some pre-existing meshes report zero through StaticMeshEditorSubsystem in
    # a NullRHI commandlet even though their serialized BodySetup contains UCX.
    # Read the aggregate geometry as a commandlet-safe fallback.
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
            "[FF_NOTION_VERIFY] Direct collision query failed for {0}: {1}".format(
                mesh.get_name(), error
            )
        )
        return -1


def vec(value):
    return {
        "x": round(float(value.x), 3),
        "y": round(float(value.y), 3),
        "z": round(float(value.z), 3),
    }


def run():
    root = project_dir()
    with open(os.path.join(root, MANIFEST_RELATIVE), "r", encoding="utf-8") as handle:
        manifest = json.load(handle)
    existing = manifest["existing_representatives"]

    entries = []
    failures = []
    seen_meshes = set()
    seen_textures = set()
    for entry in manifest["examples"]:
        definition_id = entry["definition_id"]
        is_existing = definition_id in existing
        if is_existing:
            mesh_name = existing[definition_id]["mesh"]
            texture_name = existing[definition_id]["texture"]
        else:
            mesh_name, texture_name = canonical_names(entry)

        mesh_path = MESH_ROOT + "/" + mesh_name
        texture_path = TEXTURE_ROOT + "/" + texture_name
        fbx_path = os.path.join(root, FBX_RELATIVE, mesh_name + ".fbx")
        png_path = os.path.join(root, PNG_RELATIVE, texture_name + ".png")
        mesh = unreal.EditorAssetLibrary.load_asset(mesh_path)
        texture = unreal.EditorAssetLibrary.load_asset(texture_path)

        if not isinstance(mesh, unreal.StaticMesh):
            failures.append("Missing StaticMesh: " + mesh_path)
            continue
        if not isinstance(texture, unreal.Texture2D):
            failures.append("Missing Texture2D: " + texture_path)
            continue
        if not os.path.isfile(fbx_path):
            failures.append("Missing FBX source: " + fbx_path)
            continue
        if not os.path.isfile(png_path):
            failures.append("Missing PNG source: " + png_path)
            continue

        dimensions = png_dimensions(png_path)
        bounds = mesh.get_bounding_box()
        minimum = bounds.min
        maximum = bounds.max
        size_x = float(maximum.x - minimum.x)
        size_y = float(maximum.y - minimum.y)
        size_z = float(maximum.z - minimum.z)
        center_x = float((maximum.x + minimum.x) * 0.5)
        center_y = float((maximum.y + minimum.y) * 0.5)
        materials = material_entries(mesh)
        collisions = collision_count(mesh)
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
        mesh_identity = mesh.get_path_name()
        texture_identity = texture.get_path_name()
        result = {
            "definition_id": definition_id,
            "coverage": "existing_subcategory_representative" if is_existing else "new",
            "mesh": mesh_identity,
            "texture": texture_identity,
            "fbx": fbx_path,
            "png": png_path,
            "png_size": list(dimensions) if dimensions else None,
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
            "simple_collisions": collisions,
            "texture_settings": texture_settings,
        }
        entries.append(result)
        seen_meshes.add(mesh_identity)
        seen_textures.add(texture_identity)
        unreal.log(
            "[FF_NOTION_VERIFY] {0}".format(json.dumps(result, ensure_ascii=False))
        )

        if dimensions != (324, 260):
            failures.append(
                "Thumbnail must be 324x260: {0} is {1}".format(
                    definition_id, dimensions
                )
            )
        if abs(center_x) > 2.0 or abs(center_y) > 2.0 or abs(float(minimum.z)) > 2.0:
            failures.append(
                "Pivot/bounds origin mismatch: {0} min={1} max={2}".format(
                    definition_id, vec(minimum), vec(maximum)
                )
            )
        if size_x > 600.5 or size_y > 300.5 or float(maximum.z) > 250.5:
            failures.append(
                "Mesh exceeds 600x300x250cm: {0} size={1}".format(
                    definition_id, result["size_cm"]
                )
            )
        if size_x < 100.0 or size_y < 100.0 or size_z < 100.0:
            failures.append("Implausibly small facility mesh: " + definition_id)
        if result["vertices_lod0"] <= 0 or result["triangles_lod0"] <= 0:
            failures.append("Empty LOD0 render data: " + definition_id)
        if result["triangles_lod0"] > 50000:
            failures.append("Unexpectedly dense low-poly mesh: " + definition_id)
        if len(materials) < 3:
            failures.append("Too few material slots: " + definition_id)
        if any(not item["material"] for item in materials):
            failures.append("Unassigned material slot: " + definition_id)
        if collisions <= 0:
            failures.append("No simple collision: " + definition_id)
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
                failures.append(
                    "Texture setting mismatch: {0} {1}".format(
                        definition_id, property_name
                    )
                )

    report = {
        "source": manifest["source"],
        "expected_examples": len(manifest["examples"]),
        "covered_examples": len(entries),
        "existing_representatives": sum(
            1 for item in entries
            if item["coverage"] == "existing_subcategory_representative"
        ),
        "new_pairs": sum(1 for item in entries if item["coverage"] == "new"),
        "unique_meshes": len(seen_meshes),
        "unique_textures": len(seen_textures),
        "assets": entries,
        "failures": failures,
    }
    report_path = os.path.join(saved_dir(), REPORT_NAME)
    with open(report_path, "w", encoding="utf-8") as handle:
        json.dump(report, handle, ensure_ascii=False, indent=2)

    if failures or len(entries) != len(manifest["examples"]):
        for failure in failures:
            unreal.log_error("[FF_NOTION_VERIFY] " + failure)
        raise RuntimeError(
            "Notion facility verification failed ({0}/{1} examples, {2} failures)".format(
                len(entries), len(manifest["examples"]), len(failures)
            )
        )
    unreal.log(
        "[FF_NOTION_VERIFY] Verified {0} examples: {1} existing + {2} new".format(
            len(entries), report["existing_representatives"], report["new_pairs"]
        )
    )


run()
