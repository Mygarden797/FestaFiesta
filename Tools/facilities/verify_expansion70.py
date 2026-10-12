# -*- coding: utf-8 -*-
# verify_expansion70.py
# 역할: 시설 모델·Data Asset·썸네일 작업에서 저장된 결과가 요구 조건에 맞는지 검증한다.

"""Read back saved Facility 70 native assets in a fresh Unreal process.

Use the same FF70_NUMBERS pilot filter as import_expansion70.py. No packages are
modified or saved. Checks native Spec validation, actual FBX scale/collision,
materials, thumbnails, unique identities, queue body protection and light rigs.
"""
from __future__ import annotations

import json
import math
import os
from pathlib import Path
import struct
import sys
import traceback

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
import import_expansion70 as pipeline


def check(condition, message):
    pipeline.check(condition, message)


def point(value):
    return [float(value.x), float(value.y), float(value.z)]


def close(actual, expected, tolerance=.01):
    return all(abs(float(a) - float(b)) <= tolerance for a, b in zip(actual, expected))


def png_size(path):
    with Path(path).open("rb") as handle:
        header = handle.read(24)
    check(header[:8] == b"\x89PNG\r\n\x1a\n", "Invalid source PNG: " + str(path))
    return struct.unpack(">II", header[16:24])


def saved_package(path):
    native_file = pipeline.ROOT / "Content" / (path.removeprefix("/Game/") + ".uasset")
    check(native_file.is_file() and native_file.stat().st_size > 128, "Missing native package: " + str(native_file))
    with native_file.open("rb") as handle:
        tag = handle.read(4)
    check(tag in (b"\xc1\x83\x2a\x9e", b"\x9e\x2a\x83\xc1"), "Not an Unreal package: " + str(native_file))
    return str(native_file)


def identity_inventory():
    by_id = {}
    for path in unreal.EditorAssetLibrary.list_assets(pipeline.DEFINITIONS, True, False):
        if "/PDA_FFZS_" not in path:
            continue
        definition = unreal.load_asset(path)
        if not isinstance(definition, unreal.FFZSFacilityDefinition):
            continue
        spec = definition.get_editor_property("spec")
        identity = str(spec.get_editor_property("facility_definition_id"))
        check(identity not in by_id, "Duplicate facility ID: " + identity)
        by_id[identity] = pipeline.package(definition)
    return by_id


def verify_queue(spec, entry):
    rules = spec.get_editor_property("queue_rules")
    anchors = list(spec.get_editor_property("anchors"))
    queue_points = list(spec.get_editor_property("queue_spline_points_cm"))
    rects = list(spec.get_editor_property("queue_reserved_rects"))
    count = int(rules.get_editor_property("max_waiting_agents"))
    capacity = int(rules.get_editor_property("service_capacity"))
    if entry["category"] in pipeline.NO_SERVICE:
        check(count == capacity == 0 and not anchors and not queue_points and not rects,
              "Nonservice facility has queue data")
        check(not spec.get_editor_property("queue_reserved_cells"), "Nonservice facility has legacy queue cells")
        check(spec.get_editor_property("clearance_cells") == 0, "Nonservice facility reserves extra clearance")
        check(not spec.get_editor_property("audience").get_editor_property("enabled"), "Nonservice audience enabled")
        return dict(waiting=0, service_capacity=0, anchors=[], reserved_rects=[])
    check(count > 0 and capacity == 1 and len(queue_points) >= 2, "Service queue missing")
    check(len(spec.get_editor_property("queue_reserved_cells")) == 0, "Deprecated cell reservation used")
    spacing = float(rules.get_editor_property("slot_spacing_cm"))
    distances = [math.dist(point(a), point(b)) for a, b in zip(queue_points, queue_points[1:])]
    check(sum(distances) + .001 >= (count - 1) * spacing, "Queue too short for capacity")
    slots = []
    for index in range(count):
        remaining = index * spacing
        for segment, length in enumerate(distances):
            if remaining <= length + .001:
                ratio = remaining / length if length else 0
                a, b = point(queue_points[segment]), point(queue_points[segment + 1])
                slots.append([x + (y - x) * ratio for x, y in zip(a, b)])
                break
            remaining -= length
    check(len(slots) == count, "Queue slot sampling failed")
    anchor_data = [{"type": str(anchor.get_editor_property("type")), "index": int(anchor.get_editor_property("index")),
                    "position_cm": point(anchor.get_editor_property("local_transform").translation)} for anchor in anchors]
    positions = slots + [item["position_cm"] for item in anchor_data]
    minimum = max(68.0, 2 * float(rules.get_editor_property("slot_arrival_tolerance_cm")))
    for i, first in enumerate(positions):
        for second in positions[i + 1:]:
            check(math.dist(first[:2], second[:2]) + .01 >= minimum,
                  "Movement targets overlap Crowd bodies/arrival tolerances")
    footprint = spec.get_editor_property("footprint_size_cells")
    def reserved(location):
        x, y = location[0] + footprint.x * .5, location[1] + footprint.y * .5
        return any(rect.min.x <= x - 34 and rect.min.y <= y - 34
                   and rect.max.x > x + 34 and rect.max.y > y + 34 for rect in rects)
    for location in slots:
        check(reserved(location), "Waiting Crowd body escapes queue reservation")
    for anchor in anchors:
        if anchor.get_editor_property("type") == unreal.FFZSAnchorType.ENTRY:
            check(reserved(point(anchor.get_editor_property("local_transform").translation)), "Entry Crowd body is unprotected")
    return dict(waiting=count, service_capacity=capacity, slot_spacing_cm=spacing, anchors=anchor_data,
                reserved_rects=[dict(min=[r.min.x, r.min.y], max=[r.max.x, r.max.y]) for r in rects])


def verify_lighting(spec, entry):
    settings = spec.get_editor_property("lighting_settings")
    rig_class = settings.get_editor_property("lighting_rig_class")
    if entry["category"] != "Lighting":
        return None
    check(rig_class, "Lighting facility has no rig class")
    check(settings.get_editor_property("enabled"), "Lighting is disabled by default")
    rig_path = pipeline.paths(entry)["rig"]
    saved_package(rig_path)
    bp = pipeline.owned_existing(rig_path, entry["id"])
    check(isinstance(bp, unreal.Blueprint) and bp.generated_class(), "Missing lighting Blueprint")
    check(rig_class.get_path_name() == bp.generated_class().get_path_name(), "Definition points to another rig")
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    fn = unreal.SubobjectDataBlueprintFunctionLibrary
    lights = []
    for handle in sub.k2_gather_subobject_data_for_blueprint(bp):
        component = fn.get_object_for_blueprint(fn.get_data(handle), bp)
        if isinstance(component, unreal.LocalLightComponent):
            check(component.get_editor_property("mobility") == unreal.ComponentMobility.MOVABLE, "Static runtime light")
            check(float(component.get_editor_property("intensity")) > 0, "Light has no intensity")
            lights.append(dict(name=component.get_name(), position_cm=point(component.get_editor_property("relative_location")),
                               intensity=float(component.get_editor_property("intensity"))))
    check(lights, "Lighting rig has no local light components")
    expected = entry.get("light_positions_m") or [[0, 0, max(.1, entry["bounds_m"][2] * .85)]]
    ordered = sorted(lights, key=lambda item: item["name"])
    check(len(ordered) == min(12, len(expected)), "Lighting source/component count mismatch")
    for actual, position in zip(ordered, expected[:12]):
        check(close(actual["position_cm"], pipeline.source_point_to_unreal_cm(entry, position)), "Light position differs from imported globe")
    return dict(rig=rig_path, lights=lights)


def verify_entry(entry, palette, inventory, cost_rows):
    p = pipeline.paths(entry)
    assets = {}
    for key, cls in [("mesh", unreal.StaticMesh), ("texture", unreal.Texture2D), ("definition", unreal.FFZSFacilityDefinition)]:
        saved_package(p[key])
        assets[key] = pipeline.owned_existing(p[key], entry["id"])
        check(isinstance(assets[key], cls), "Wrong native asset class: " + p[key])
    mesh, texture, definition = assets["mesh"], assets["texture"], assets["definition"]
    spec = definition.get_editor_property("spec")
    check(str(spec.get_editor_property("facility_definition_id")) == entry["id"], "Definition ID mismatch")
    check(inventory.get(entry["id"]) == p["definition"], "Definition inventory mismatch")
    check(str(spec.get_editor_property("display_name")) == entry["name_ko"], "Korean display name mismatch")
    check(spec.get_editor_property("category") == pipeline.category_enum(entry["category"]), "Category mismatch")
    check(pipeline.package(spec.get_editor_property("visual_mesh")) == p["mesh"], "Incorrect visual mesh reference")
    check(pipeline.package(definition.get_editor_property("catalog_thumbnail")) == p["texture"], "Incorrect catalog thumbnail")
    footprint = spec.get_editor_property("footprint_size_cells")
    final_footprint, final_height = pipeline.authored_dimensions(entry)
    check([int(footprint.x), int(footprint.y)] == final_footprint, "Footprint mismatch")
    check(abs(float(spec.get_editor_property("placement_obstacle_height_cm")) - final_height) < .01, "Height mismatch")
    check(spec.get_editor_property("visual_sizing_mode") == unreal.FFZSVisualSizingMode.MATCH_PLACEMENT_BOUNDS, "Unexpected visual sizing mode")
    check(close(point(spec.get_editor_property("visual_offset_cm")), [0, 0, 0]), "Nonzero visual offset")
    rotation = spec.get_editor_property("visual_rotation")
    check(close([rotation.pitch, rotation.yaw, rotation.roll], [0, 0, 0]), "Axis correction must be baked into raw mesh")
    import_data = mesh.get_editor_property("asset_import_data")
    import_rotation = import_data.get_editor_property("import_rotation")
    check(close([import_rotation.pitch, import_rotation.yaw, import_rotation.roll], [0, pipeline.import_yaw(entry), 0]),
          "FBX import rotation does not preserve customer front +Y")
    check(pipeline.metadata(mesh, "FF70.ImportYaw") == str(pipeline.import_yaw(entry)), "Missing raw mesh axis provenance")
    check(definition.get_editor_property("show_in_build_catalog"), "Facility hidden from catalog")
    check(not definition.get_editor_property("procedurally_managed_visual_mesh"), "Legacy generator ownership must remain off")
    check(definition.get_editor_property("enable_drag_placement") == (entry["category"] in {"Road", "Tile"}), "Drag placement policy mismatch")
    check(spec.get_editor_property("bAllowFacilityPlacementOnTop") == (entry["category"] == "Tile"), "Ground stacking policy mismatch")
    check(pipeline.metadata(definition, "FF70.DescriptionKO") == entry.get("description_ko", ""), "Description metadata mismatch")
    errors = definition.is_definition_valid()
    check(errors is not None and len(errors) == 0, "Native IsDefinitionValid failed: " + str(errors))
    geometry = pipeline.validate_mesh(mesh, entry)
    check(not mesh.get_editor_property("nanite_settings").get_editor_property("enabled"), "Nanite unexpectedly enabled")
    assignments = []
    for slot in mesh.get_editor_property("static_materials"):
        name = str(slot.get_editor_property("material_slot_name"))
        mat = slot.get_editor_property("material_interface")
        check(name in palette and isinstance(mat, unreal.Material), "Invalid palette material slot: " + name)
        check(pipeline.package(mat) == pipeline.ART + "/Materials/" + name, "Wrong material assignment")
        check(pipeline.metadata(mat, "FF70.Owner") == pipeline.OWNER, "Unowned palette material")
        if float(palette[name].get("emissive_strength", 0)) > 0:
            scalar = {str(value) for value in unreal.MaterialEditingLibrary.get_scalar_parameter_names(mat)}
            vector = {str(value) for value in unreal.MaterialEditingLibrary.get_vector_parameter_names(mat)}
            check({"FF_LightingBrightness", "FF_LightingUseColorOverride"} <= scalar and "FF_LightingColor" in vector,
                  "Emissive material lacks runtime lighting controls")
        assignments.append(name)
    check(set(assignments) == set(entry["material_names"]), "FBX material slots differ from build manifest")
    expected_size = png_size(pipeline.source_path(entry["preview"]))
    check((int(texture.blueprint_get_size_x()), int(texture.blueprint_get_size_y())) == expected_size, "Thumbnail dimensions mismatch")
    for key, value in dict(compression_settings=unreal.TextureCompressionSettings.TC_EDITOR_ICON,
                           lod_group=unreal.TextureGroup.TEXTUREGROUP_UI,
                           mip_gen_settings=unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
                           srgb=True, never_stream=True).items():
        check(texture.get_editor_property(key) == value, "Thumbnail setting mismatch: " + key)
    cost_key = str(spec.get_editor_property("install_cost_key"))
    candidates = ([cost_key] if cost_key not in {"None", ""} else
                  ["Cost." + entry["id"].replace("_", "."), "Cost." + entry["category"] + "." + entry["subtype"]])
    candidates.append("Cost." + entry["category"] + ".Default")
    cost_names = {key.casefold(): key for key in cost_rows}
    resolved = next((cost_names[key.casefold()] for key in candidates if key.casefold() in cost_names), None)
    check(resolved and int(cost_rows[resolved]["InstallCost"]) >= 0, "Unresolved install cost; tried=" + str(candidates))
    front_probe = None
    semantic_material = {"Booth_FoodStall_CafeTruck": "M_F70_Wood",
                         "Booth_ActivityGame_RhythmGameStall": "M_F70_Screen"}.get(entry["id"])
    if semantic_material:
        index = assignments.index(semantic_material)
        vertices = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh, 0, index)[0]
        check(vertices and min(v.y for v in vertices) > 0,
              "Asymmetric service surface faces away from +Y queue: " + entry["id"])
        front_probe = dict(material=semantic_material, min_y_cm=min(v.y for v in vertices),
                           max_y_cm=max(v.y for v in vertices), service_side="+Y")
    return dict(number=entry["number"], id=entry["id"], definition=p["definition"], mesh=p["mesh"],
                thumbnail=p["texture"], footprint_cm=final_footprint, height_cm=final_height,
                requested_footprint_cm=entry["footprint_cm"], requested_height_cm=entry["height_cm"],
                import_yaw_degrees=pipeline.import_yaw(entry), customer_front="+X" if entry["category"] in {"Road", "Tile"} else "+Y",
                visual_rotation_degrees=[0, 0, 0], asymmetric_front_probe=front_probe,
                geometry=geometry, material_slots=assignments, thumbnail_dimensions=list(expected_size),
                queue=verify_queue(spec, entry), lighting=verify_lighting(spec, entry),
                install_cost_key=resolved, install_cost=int(cost_rows[resolved]["InstallCost"]), native_validation="PASS")


def run():
    pipeline.OUT.mkdir(parents=True, exist_ok=True)
    report = dict(status="RUNNING", entries=[], failures=[])
    try:
        manifest, entries, palette = pipeline.load_inputs()
        inventory = identity_inventory()
        table = unreal.load_asset("/Game/FestaFiesta/Data/Economy/DT_FacilityCosts")
        check(isinstance(table, unreal.DataTable), "Missing facility cost table")
        costs = {row["Name"]: row for row in json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))}
        for entry in entries:
            try:
                report["entries"].append(verify_entry(entry, palette, inventory, costs))
                unreal.log("FF70_VERIFY_READY " + entry["id"])
            except Exception as error:
                report["failures"].append(entry["id"] + ": " + str(error))
        report.update(status="FAIL" if report["failures"] else "PASS", selected_count=len(entries),
                      verified_count=len(report["entries"]), total_definitions=len(inventory),
                      live_navmesh_and_render_review="Not covered by readback validation")
        check(not report["failures"], "Facility70 verification failed: " + " | ".join(report["failures"]))
        unreal.log("FF70_VERIFY_PASS count=" + str(len(entries)))
    except Exception:
        report["status"] = "FAIL"
        if not report["failures"]:
            report["failures"].append(traceback.format_exc())
        raise
    finally:
        label = (os.environ.get("FF70_NUMBERS") or "all").replace(",", "_")
        (pipeline.OUT / ("verify_" + label + ".json")).write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")


if __name__ == "__main__":
    run()
