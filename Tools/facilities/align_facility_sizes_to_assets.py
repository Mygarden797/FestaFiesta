"""Match production facility dimensions to their assigned Static Mesh bounds.

Run with Tools/facilities/Run-Expansion70Editor.ps1 for a read-only audit.
Add -FFApplyAssetSizes to an unattended Python commandlet to save corrections.
The Definition remains the runtime size authority. Cardinal visual rotations,
queue spacing, capacity, upgrade multipliers and every non-layout field are
preserved. Layout corrections mirror TryResizeFootprintKeepingLayout; narrow
facilities additionally move a colliding exit outward to protect Crowd bodies.
"""

import hashlib
import json
import math
import re
import sys
from pathlib import Path

import unreal

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))
from ffzs_asset_save_transaction import AssetSaveTransaction
from ffzs_definition_source import read_facility_definitions, require_unattended_commandlet

OUT = Path(unreal.Paths.project_dir()).resolve() / "Saved/FacilityAssetSizes"
# FBX float bounds contain tiny errors around integral centimeters. This is
# 0.01 mm, matching the existing expansion importer, not a visible overhang.
BOUNDS_NOISE_CM = 0.001
# FUnitData's default radius, also used by the existing movement-target migration
# and the production content test. Native validation covers arrival tolerances.
CROWD_AGENT_RADIUS_CM = 34.0
LAYOUT_FIELDS = (
    "footprint_size_cells", "placement_obstacle_height_cm",
    "queue_reserved_rects", "queue_spline_points_cm", "anchors", "audience",
)


def source_dimensions(spec, mesh):
    if mesh is None:
        raise RuntimeError("Facility has no assigned Static Mesh")
    bounds = mesh.get_bounding_box()
    size = [float(getattr(bounds.max, axis) - getattr(bounds.min, axis)) for axis in "xyz"]
    if any(not math.isfinite(value) or value <= 0 for value in size):
        raise RuntimeError("Unusable mesh bounds: " + mesh.get_path_name())
    rotation = spec.get_editor_property("visual_rotation")
    yaw = float(rotation.yaw)
    if rotation.pitch != 0 or rotation.roll != 0 or yaw % 90 != 0:
        raise RuntimeError("Expected a cardinal visual rotation: " + mesh.get_path_name())
    if abs(round(yaw / 90)) % 2:
        size[0], size[1] = size[1], size[0]
    footprint = [max(1, math.ceil(value - BOUNDS_NOISE_CM)) for value in size[:2]]
    if any(value > 2147483647 for value in footprint):
        raise RuntimeError("Footprint exceeds the supported grid range")
    return footprint, size[2], size


def centered_coordinate(value, old_size, new_size):
    delta = (new_size - old_size) * 0.5
    return value + delta if value > 0 else value - delta if value < 0 else value


def reservation_coordinate(value, old_size, new_size):
    side = value * 2 - old_size
    result = value + new_size - old_size if side > 0 else (new_size + 1) // 2 if side == 0 else value
    if not -2147483648 <= result <= 2147483647:
        raise RuntimeError("Reservation coordinate exceeds the supported grid range")
    return result


def shift_point(point, old, new):
    return unreal.Vector(centered_coordinate(float(point.x), old[0], new[0]),
                         centered_coordinate(float(point.y), old[1], new[1]), float(point.z))


def queue_slot_points(spec):
    points = list(spec.get_editor_property("queue_spline_points_cm"))
    rules = spec.get_editor_property("queue_rules")
    slots = []
    for index in range(int(rules.get_editor_property("max_waiting_agents"))):
        remaining = index * float(rules.get_editor_property("slot_spacing_cm"))
        for first, second in zip(points, points[1:]):
            distance = math.dist((first.x, first.y, first.z), (second.x, second.y, second.z))
            if distance <= 0.0001:
                continue
            if remaining <= distance + 0.0001:
                fraction = min(1.0, remaining / distance)
                slots.append(unreal.Vector(*[getattr(first, axis) +
                    (getattr(second, axis) - getattr(first, axis)) * fraction for axis in "xyz"]))
                break
            remaining -= distance
        else:
            if points and remaining <= 0.0001:
                slots.append(points[-1])
            else:
                raise RuntimeError("Queue is too short for its capacity")
    return slots


def separate_narrow_exits(candidate, original, corrections):
    anchors = [anchor.copy() for anchor in candidate.get_editor_property("anchors")]
    original_anchors = original.get_editor_property("anchors")
    targets = queue_slot_points(candidate) + [anchor.get_editor_property("local_transform").translation
        for anchor in anchors if anchor.get_editor_property("type") != unreal.FFZSAnchorType.EXIT]
    minimum = max(CROWD_AGENT_RADIUS_CM * 2.0,
        float(candidate.get_editor_property("queue_rules").get_editor_property("slot_arrival_tolerance_cm")) * 2.0)
    for index, anchor in enumerate(anchors):
        if anchor.get_editor_property("type") != unreal.FFZSAnchorType.EXIT:
            continue
        transform = anchor.get_editor_property("local_transform").copy()
        point = transform.translation
        if any(math.hypot(point.x - target.x, point.y - target.y) < minimum for target in targets):
            before = [float(point.x), float(point.y), float(point.z)]
            original_x = original_anchors[index].get_editor_property("local_transform").translation.x
            gap = minimum + 2.0
            new_x = min(point.x, min(target.x for target in targets) - gap) if original_x <= 0 else \
                    max(point.x, max(target.x for target in targets) + gap)
            point = unreal.Vector(new_x, point.y, point.z)
            transform.set_editor_property("translation", point)
            anchor.set_editor_property("local_transform", transform)
            corrections.append({"anchor_index": index, "before_cm": before,
                                "after_cm": [float(point.x), float(point.y), float(point.z)]})
        targets.append(point)
    candidate.set_editor_property("anchors", anchors)


def resize_layout(spec, footprint, height, corrections):
    candidate = spec.copy()
    old_point = spec.get_editor_property("footprint_size_cells")
    old = [int(old_point.x), int(old_point.y)]
    if min(old) <= 0 or len(spec.get_editor_property("queue_reserved_cells")):
        raise RuntimeError("Invalid footprint or unmigrated legacy queue cells")
    candidate.set_editor_property("footprint_size_cells", unreal.IntPoint(*footprint))
    candidate.set_editor_property("placement_obstacle_height_cm", height)
    if old == footprint:
        return candidate
    rectangles = [rect.copy() for rect in spec.get_editor_property("queue_reserved_rects")]
    for rect in rectangles:
        for edge in ("min", "max"):
            point = rect.get_editor_property(edge)
            rect.set_editor_property(edge, unreal.IntPoint(
                reservation_coordinate(int(point.x), old[0], footprint[0]),
                reservation_coordinate(int(point.y), old[1], footprint[1])))
    candidate.set_editor_property("queue_reserved_rects", rectangles)
    candidate.set_editor_property("queue_spline_points_cm", [
        shift_point(point, old, footprint) for point in spec.get_editor_property("queue_spline_points_cm")])
    anchors = [anchor.copy() for anchor in spec.get_editor_property("anchors")]
    for anchor in anchors:
        transform = anchor.get_editor_property("local_transform").copy()
        transform.set_editor_property("translation", shift_point(
            transform.get_editor_property("translation"), old, footprint))
        anchor.set_editor_property("local_transform", transform)
    candidate.set_editor_property("anchors", anchors)
    audience = spec.get_editor_property("audience").copy()
    if audience.get_editor_property("enabled"):
        delta_y = (footprint[1] - old[1]) * 0.5
        for property_name in ("first_slot_local_cm", "entry_local_cm", "exit_local_cm"):
            point = audience.get_editor_property(property_name)
            audience.set_editor_property(property_name, unreal.Vector(point.x, point.y + delta_y, point.z))
        audience.set_editor_property("focus_local_cm", shift_point(
            audience.get_editor_property("focus_local_cm"), old, footprint))
        candidate.set_editor_property("audience", audience)
    separate_narrow_exits(candidate, spec, corrections)
    return candidate


def preserved_text(candidate, original):
    normalized = candidate.copy()
    for property_name in LAYOUT_FIELDS:
        normalized.set_editor_property(property_name, original.get_editor_property(property_name))
    return normalized.export_text()


def size_record(record):
    spec = record["definition_object"].get_editor_property("spec")
    footprint, height, source = source_dimensions(spec, record["visual_mesh_object"])
    current = spec.get_editor_property("footprint_size_cells")
    current_size = [int(current.x), int(current.y), float(spec.get_editor_property("placement_obstacle_height_cm"))]
    return {
        "definition": record["definition"], "id": record["facility_definition_id"],
        "name": str(spec.get_editor_property("display_name")), "mesh": record["visual_mesh"],
        "source_rotated_size_cm": source, "data_size_cm": current_size,
        "target_size_cm": footprint + [height],
        "scale": [target / actual for target, actual in zip(current_size, source)],
        "matches_source": current_size[:2] == footprint and abs(current_size[2] - height) <= BOUNDS_NOISE_CM,
        "spec_sha256": hashlib.sha256(spec.export_text().encode("utf-8")).hexdigest(),
    }


def run():
    require_unattended_commandlet()
    apply = bool(re.search(r"(?:^|\s)-FFApplyAssetSizes(?:\s|$)", unreal.SystemLibrary.get_command_line()))
    records = read_facility_definitions(require_visual_mesh=True)
    report = {"mode": "apply" if apply else "audit", "definition_count": len(records),
              "changed_count": 0, "before": [], "after": [], "unrelated_fields_preserved": True,
              "exit_clearance_corrections": []}
    candidates = []
    validation_asset = unreal.new_object(unreal.FFZSFacilityDefinition)
    for record in records:
        spec = record["spec"]
        if spec.get_editor_property("visual_sizing_mode") != unreal.FFZSVisualSizingMode.MATCH_PLACEMENT_BOUNDS:
            raise RuntimeError("Production visual sizing mode is not MatchPlacementBounds: " + record["definition"])
        row = size_record(record)
        report["before"].append(row)
        if row["matches_source"]:
            continue
        corrections = []
        candidate = resize_layout(spec, row["target_size_cm"][:2], row["target_size_cm"][2], corrections)
        if corrections:
            report["exit_clearance_corrections"].append({"id": row["id"], "corrections": corrections})
        if preserved_text(candidate, spec) != spec.export_text():
            raise RuntimeError("A non-layout property changed: " + record["definition"])
        validation_asset.set_editor_property("spec", candidate)
        errors = validation_asset.is_definition_valid()
        if errors is None or len(errors):
            raise RuntimeError("Resized definition is invalid: {0}: {1}".format(record["definition"], errors))
        candidates.append((record, candidate))
    report["needs_change_count"] = len(candidates)
    OUT.mkdir(parents=True, exist_ok=True)
    if apply and candidates:
        # No on-disk or production UObject mutation until the entire batch has
        # passed native base/upgrade validation and unrelated-field comparison.
        transaction = AssetSaveTransaction([record["definition_object"] for record, _ in candidates], "asset_sizes")
        try:
            for record, candidate in candidates:
                definition = record["definition_object"]
                definition.set_editor_property("spec", candidate)
                if not size_record(record)["matches_source"]:
                    raise RuntimeError("Requested dimensions were not applied: " + record["definition"])
                if not unreal.EditorAssetLibrary.save_loaded_asset(definition, only_if_is_dirty=False):
                    raise RuntimeError("Cannot save " + record["definition"])
                report["changed_count"] += 1
            transaction.commit()
        except Exception:
            transaction.rollback()
            raise
    report["after"] = [size_record(record) for record in records]
    report["nonmatching_count"] = sum(not row["matches_source"] for row in report["after"])
    report["status"] = "PASS" if report["nonmatching_count"] == 0 else "NEEDS_MIGRATION"
    migration_path = OUT / "migration.json"
    if not apply and migration_path.exists():
        migration = json.loads(migration_path.read_text(encoding="utf-8"))
        expected = {row["definition"]: row["spec_sha256"] for row in migration["after"]}
        report["saved_specs_match_migration"] = all(
            expected.get(row["definition"]) == row["spec_sha256"] for row in report["after"])
        if not report["saved_specs_match_migration"]:
            raise RuntimeError("Saved facility specifications differ from the migration result")
    report_path = OUT / ("migration.json" if apply else "audit.json")
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("FF_ASSET_SIZES: " + json.dumps({key: report[key] for key in
               ("mode", "status", "definition_count", "changed_count", "needs_change_count", "nonmatching_count")}))
    if apply and report["nonmatching_count"]:
        raise RuntimeError("Some facility dimensions still differ from their source assets")


run()
