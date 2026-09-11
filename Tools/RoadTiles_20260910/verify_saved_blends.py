"""Independently reopen the final editable .blend deliverables without saving.

Run with Blender --background --factory-startup --python this_file.py.
Only the JSON report is written; source files and the live MCP session are untouched.
"""

import json
import os
from pathlib import Path
import traceback
from datetime import datetime, timezone

import bpy


PROJECT = Path(__file__).resolve().parents[2]
ROOT = PROJECT / "SourceArt/RoadTiles_20260910"
OUTPUT = ROOT / "qa/saved_blend_verification.json"
NAMES = ("Sand", "Asphalt", "Wood", "CreamTile", "MintHerringbone", "ConcreteDrain")


def check(record, label, condition, detail=None):
    item = {"check": label, "status": "PASS" if condition else "FAIL"}
    if detail is not None:
        item["detail"] = detail
    record["checks"].append(item)
    return condition


def object_hidden(obj, scene):
    return {layer.name: bool(obj.hide_get(view_layer=layer)) for layer in scene.view_layers}


def image_info(image):
    packed = bool(image.packed_file) or len(image.packed_files) > 0
    resolved = bpy.path.abspath(image.filepath, library=image.library) if image.filepath else ""
    exists = bool(resolved) and Path(resolved).is_file()
    external_required = image.source in {"FILE", "MOVIE", "SEQUENCE", "TILED"}
    available = packed or exists or not external_required
    return {"name": image.name, "source": image.source,
            "packed": packed, "size": list(image.size),
            "filepath": image.filepath, "resolved_filepath": resolved,
            "external_file_exists": exists, "available": available}


def inspect(name):
    path = ROOT / "Blends" / ("Road_" + name + ".blend")
    record = {"name": name, "file": str(path), "checks": []}
    if not check(record, "final_file_exists", path.is_file()):
        record["status"] = "FAIL"
        return record
    before = {"size": path.stat().st_size, "mtime_ns": path.stat().st_mtime_ns}
    expected_scene = "ROAD_" + name + "_SOURCE"
    with bpy.data.libraries.load(str(path), link=False) as (data_from, data_to):
        # Read the blend's saved datablock directory without appending anything.
        record["saved_scene_directory"] = list(data_from.scenes)
    check(record, "exactly_one_expected_source_scene_in_file",
          record["saved_scene_directory"] == [expected_scene],
          record["saved_scene_directory"])

    result = bpy.ops.wm.open_mainfile(filepath=str(path), load_ui=False)
    record["open_operator_result"] = sorted(result)
    check(record, "open_mainfile_finished", "FINISHED" in result)
    scenes = list(bpy.data.scenes)
    check(record, "exactly_one_expected_source_scene_after_open",
          len(scenes) == 1 and scenes[0].name == expected_scene,
          [scene.name for scene in scenes])
    scene = bpy.data.scenes.get(expected_scene)
    if scene is None:
        record["status"] = "FAIL"
        return record
    initial_scene = bpy.context.scene.name if bpy.context.scene else None
    if bpy.context.window:
        bpy.context.window.scene = scene
    bpy.context.view_layer.update()
    record["initial_active_scene"] = initial_scene
    record["selected_active_scene"] = bpy.context.scene.name if bpy.context.scene else None
    check(record, "source_scene_can_be_selected", bpy.context.scene == scene,
          record["selected_active_scene"])

    editable = bpy.data.collections.get("EDITABLE_" + name)
    exports = bpy.data.collections.get("EXPORT_" + name)
    check(record, "editable_collection_exists", editable is not None)
    check(record, "export_collection_exists", exports is not None)
    parts = list(editable.objects) if editable else []
    record["source_part_count"] = len(parts)
    record["source_parts"] = [
        {"name": obj.name, "type": obj.type, "mesh": obj.data.name if obj.type == "MESH" else None,
         "vertices": len(obj.data.vertices) if obj.type == "MESH" else None,
         "faces": len(obj.data.polygons) if obj.type == "MESH" else None,
         "part_label": obj.get("part_label"), "detail": obj.get("detail"),
         "palette_key": obj.get("palette_key"),
         "modifier_types": [modifier.type for modifier in obj.modifiers],
         "viewport_hidden": object_hidden(obj, scene), "hide_render": bool(obj.hide_render)}
        for obj in sorted(parts, key=lambda obj: obj.name)
    ]
    check(record, "source_is_separate_editable_mesh_parts",
          len(parts) > 1 and all(obj.type == "MESH" for obj in parts)
          and len({obj.data.as_pointer() for obj in parts}) == len(parts))
    check(record, "source_parts_have_edit_labels_and_palette_keys",
          bool(parts) and all(obj.get("part_label") and obj.get("palette_key") for obj in parts))
    check(record, "source_has_editable_bevel_modifiers",
          any(modifier.type == "BEVEL" for obj in parts for modifier in obj.modifiers))
    check(record, "source_parts_visible",
          bool(parts) and all(not obj.hide_render and not any(object_hidden(obj, scene).values())
                              for obj in parts))
    declared = ROOT / "qa" / ("validation_" + name + ".json")
    if declared.is_file():
        expected_count = json.loads(declared.read_text(encoding="utf-8"))["editable_part_count"]
        check(record, "actual_part_count_matches_build_manifest", len(parts) == expected_count,
              {"expected": expected_count, "actual": len(parts)})

    export_objects = list(exports.objects) if exports else []
    lods = sorted([obj for obj in export_objects if obj.get("LOD") is not None],
                  key=lambda obj: obj["LOD"])
    expected_lod_names = ["SM_Road_" + name, "SM_Road_" + name + "_LOD1",
                          "SM_Road_" + name + "_LOD2"]
    check(record, "exactly_three_export_lods",
          [obj.name for obj in lods] == expected_lod_names
          and [obj.get("LOD") for obj in lods] == [0, 1, 2],
          [obj.name for obj in lods])
    record["export_lods"] = [
        {"name": obj.name, "level": obj["LOD"], "vertices": len(obj.data.vertices),
         "triangles": sum(len(poly.vertices) - 2 for poly in obj.data.polygons),
         "viewport_hidden": object_hidden(obj, scene), "hide_render": bool(obj.hide_render),
         "material_slots": len(obj.data.materials),
         "uv_maps": [layer.name for layer in obj.data.uv_layers]}
        for obj in lods]
    check(record, "export_lods_hidden_in_all_view_layers_and_render",
          len(lods) == 3 and all(obj.hide_render and all(object_hidden(obj, scene).values())
                                for obj in lods))
    check(record, "export_lods_are_triangulated_meshes",
          len(lods) == 3 and all(obj.type == "MESH" and len(obj.data.polygons) > 0
                                and all(len(poly.vertices) == 3 for poly in obj.data.polygons)
                                for obj in lods))

    collisions = [obj for obj in scene.objects if obj.name.startswith("UCX_")]
    check(record, "one_expected_ucx_collision",
          len(collisions) == 1 and collisions[0].name == "UCX_SM_Road_" + name + "_00",
          [obj.name for obj in collisions])
    record["collision"] = [{"name": obj.name, "viewport_hidden": object_hidden(obj, scene),
                             "hide_render": bool(obj.hide_render)} for obj in collisions]
    check(record, "collision_hidden_in_all_view_layers_and_render",
          len(collisions) == 1 and all(obj.hide_render and all(object_hidden(obj, scene).values())
                                      for obj in collisions))
    check(record, "only_source_export_lods_and_collision_in_scene",
          len(scene.objects) == len(parts) + 4,
          {"scene_objects": len(scene.objects), "expected": len(parts) + 4})

    bad_scales = [{"name": obj.name, "scale": list(obj.scale)} for obj in scene.objects
                  if any(value <= 0 or abs(value - 1.0) > 1e-6 for value in obj.scale)]
    check(record, "all_object_scales_positive_unit", not bad_scales, bad_scales)
    reflected = [obj.name for obj in scene.objects if obj.matrix_world.to_3x3().determinant() <= 0]
    check(record, "no_reflected_or_singular_object_transforms", not reflected, reflected)
    record["images"] = [image_info(image) for image in bpy.data.images]
    palettes = [entry for entry in record["images"] if entry["name"].startswith("T_RoadPalette")]
    check(record, "palette_image_packed", len(palettes) == 1 and palettes[0]["packed"])
    check(record, "palette_image_has_pixels",
          len(palettes) == 1 and palettes[0]["size"] == [512, 512])
    check(record, "no_missing_images", all(entry["available"] for entry in record["images"]),
          [entry["name"] for entry in record["images"] if not entry["available"]])
    used_palette_images = []
    for obj in lods:
        for material in obj.data.materials:
            if material and material.use_nodes:
                used_palette_images.extend(node.image.name for node in material.node_tree.nodes
                                           if node.type == "TEX_IMAGE" and node.image)
    check(record, "each_lod_material_links_to_packed_palette",
          len(used_palette_images) == 3
          and all(image_name.startswith("T_RoadPalette") for image_name in used_palette_images),
          used_palette_images)
    after = {"size": path.stat().st_size, "mtime_ns": path.stat().st_mtime_ns}
    check(record, "source_blend_not_modified", before == after)
    record["status"] = "PASS" if all(c["status"] == "PASS" for c in record["checks"]) else "FAIL"
    return record


def main():
    report = {"status": "RUNNING", "blender_version": bpy.app.version_string,
              "fresh_background_process": True, "process_id": os.getpid(),
              "timestamp_utc": datetime.now(timezone.utc).isoformat(),
              "method": "Read datablock directory, then open_mainfile(load_ui=False) per final file; never save",
              "assets": []}
    for name in NAMES:
        try:
            record = inspect(name)
        except Exception:
            record = {"name": name, "status": "FAIL", "exception": traceback.format_exc()}
        report["assets"].append(record)
        print("SAVED_BLEND_VERIFICATION", name, record["status"], flush=True)
    report["status"] = "PASS" if all(asset["status"] == "PASS" for asset in report["assets"]) else "FAIL"
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("SAVED_BLEND_VERIFICATION_RESULT", report["status"], str(OUTPUT), flush=True)
    if report["status"] != "PASS":
        raise RuntimeError("Independent saved .blend verification failed; inspect JSON report")


if __name__ == "__main__":
    main()
