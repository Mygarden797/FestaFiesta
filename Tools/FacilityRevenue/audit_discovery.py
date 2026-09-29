"""Read-only Test_Main facility discovery/revenue audit; never saves a map or slot.

Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>.
The production map supplies zone links/navigation; day1 supplies a saved example
layout, when present. C++ automation tests verify candidate selection and payout.
"""
import json
import hashlib
from collections import deque
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Saved/FacilityQueueUI_20260929/discovery-audit.json"
SOURCE_ZONE = "GZ_TestMain_08"


def connected_zones(graph, start):
    visited = {start}
    pending = deque([start])
    while pending:
        for neighbor in graph.get(pending.popleft(), []):
            if neighbor in graph and neighbor not in visited:
                visited.add(neighbor)
                pending.append(neighbor)
    return visited


world = unreal.EditorLoadingAndSavingUtils.load_map("/Game/FestaFiesta/Maps/Test_Main")
assert world, "The production map must load"
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
zones = {str(actor.get_editor_property("zone_id")): actor for actor in actors
         if isinstance(actor, unreal.FFZSGameplayZone)}
graph = {key: [str(value) for value in actor.get_editor_property("neighbor_zone_ids")]
         for key, actor in zones.items()}
assert SOURCE_ZONE in zones, "The zone from the captured gameplay log must exist"
connected = connected_zones(graph, SOURCE_ZONE)
local = {SOURCE_ZONE, *graph[SOURCE_ZONE]}
report = {
    "map": world.get_path_name(), "source_zone": SOURCE_ZONE,
    "authored_zone_neighbors": graph, "old_local_zone_ids": sorted(local),
    "expanded_connected_zone_ids": sorted(connected), "map_saved": False,
    "walk_distance_enabled_by_default": bool(unreal.get_default_object(
        unreal.FF_CrowdDecisionSubsystem).get_editor_property("bEnableWalkDistances")),
    "map_sha256": hashlib.sha256((ROOT / "Content/FestaFiesta/Maps/Test_Main.umap").read_bytes()).hexdigest(),
}
layout = unreal.GameplayStatics.load_game_from_slot("day1", 0)
if layout:
    report["example_layout_slot"] = "day1"
    try:
        snapshot = layout.get_editor_property("snapshot")
        report["example_layout_facilities"] = [
            {"definition_id": str(item.get_editor_property("facility_spec").get_editor_property("facility_definition_id")),
             "zone_id": str(item.get_editor_property("zone_id")),
             "in_old_local_search": str(item.get_editor_property("zone_id")) in local,
             "in_connected_search": str(item.get_editor_property("zone_id")) in connected}
            for item in snapshot.get_editor_property("facilities")]
    except Exception as error:
        # SaveGame-only fields can be protected from Python. This optional example
        # is not a reason to discard the production map/table/navigation audit.
        report["example_layout_unavailable"] = str(error)

table = unreal.load_asset("/Game/FestaFiesta/Data/Economy/DT_FacilityRevenues")
rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
report["revenue_row_count"] = len(rows)
report["paid_facilities"] = [{"id": row["Name"], "base_gold": row["RevenuePerUseGold"],
                              "commission_rate": row["CommissionRate"]}
                             for row in rows if row["RevenuePerUseGold"] > 0]
report["free_facility_count"] = sum(row["RevenuePerUseGold"] == 0 for row in rows)
report["status"] = "MAP_AND_REVENUE_READ"
OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")

assert unreal.FFZSNavigationEditorLibrary.prepare_world_for_navigation(world), "World geometry is not ready"
navs = [actor for actor in actors if isinstance(actor, unreal.RecastNavMesh)]
assert navs, "The production map must have navigation data"
nav = navs[0]
anchors = {}
anchor_records = {}
def vec(value):
    return [value.x, value.y, value.z] if value is not None else None

for key, actor in zones.items():
    authored = actor.get_editor_property("entry_exit_anchors")
    location = actor.get_actor_transform().transform_location(authored[0].translation) if authored else actor.get_actor_location()
    anchors[key] = unreal.NavigationSystemV1.project_point_to_navigation(
        world, location, nav, None, unreal.Vector(300, 300, 2000))
    anchor_records[key] = {"requested": vec(location), "projected": vec(anchors[key]), "authored": bool(authored)}
paths = {}
path_details = {}
for key, point in anchors.items():
    if key == SOURCE_ZONE:
        continue
    path = unreal.NavigationSystemV1.find_path_to_location_synchronously(
        world, anchors[SOURCE_ZONE], point, nav) if anchors[SOURCE_ZONE] is not None and point is not None else None
    paths[key] = bool(path and path.is_valid() and not path.is_partial() and path.path_points)
    path_details[key] = {"source_projected": anchors[SOURCE_ZONE] is not None, "target_projected": point is not None,
                         "valid": bool(path and path.is_valid()), "partial": bool(path and path.is_partial()),
                         "point_count": len(path.path_points) if path else 0,
                         "last_point": vec(path.path_points[-1]) if path and path.path_points else None}
report["navigation_paths_from_source_zone"] = paths
report["navigation_anchor_records"] = anchor_records
report["navigation_path_details"] = path_details

# A zone anchor is a distance proxy, not every visitor's actual position. Check
# multiple interior positions and the production spawn separately before blaming
# the map's overall reachability on one projected anchor.
source_actor = zones[SOURCE_ZONE]
polygon = source_actor.get_boundary_polygon_local_cm()
source_samples = {}
if polygon:
    min_x, max_x = min(p.x for p in polygon), max(p.x for p in polygon)
    min_y, max_y = min(p.y for p in polygon), max(p.y for p in polygon)
    for ix, fx in enumerate((.2, .5, .8)):
        for iy, fy in enumerate((.2, .5, .8)):
            point = source_actor.get_actor_transform().transform_location(unreal.Vector(
                min_x + (max_x-min_x)*fx, min_y + (max_y-min_y)*fy, 0))
            if source_actor.contains_world_point(point):
                source_samples[f"source_zone_interior_{ix}_{iy}"] = point
for actor in actors:
    if isinstance(actor, unreal.FF_SpawnActor):
        override = actor.get_editor_property("SpawnAchor")
        source_samples["production_spawn"] = override.get_actor_location() if override else actor.get_actor_location()
sample_records = {}
for label, requested in source_samples.items():
    point = unreal.NavigationSystemV1.project_point_to_navigation(world, requested, nav, None, unreal.Vector(300, 300, 2000))
    reachable = []
    for key, target in anchors.items():
        path = unreal.NavigationSystemV1.find_path_to_location_synchronously(world, point, target, nav) if point is not None and target is not None else None
        if path and path.is_valid() and not path.is_partial() and path.path_points:
            reachable.append(key)
    sample_records[label] = {"requested": vec(requested), "projected": vec(point),
                             "reachable_anchor_count": len(reachable), "reachable_anchor_zones": sorted(reachable)}
report["source_position_comparison"] = sample_records
report["navigation_complete_count"] = sum(paths.values())
report["navigation_incomplete_count"] = sum(not value for value in paths.values())
report["status"] = "AUDIT_COMPLETE_WITH_PATH_GAPS" if not all(paths.values()) else "PASS"
OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("FACILITY_DISCOVERY_AUDIT: " + str(OUT))
