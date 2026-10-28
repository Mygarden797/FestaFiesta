"""Disposable Test_Main projection evidence; no assets or existing verifier are changed."""
from pathlib import Path
import json
import math
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
VERIFIER = ROOT / "Plugins/FF_MapOverview/Tools/verify_runtime.py"
exec(compile(VERIFIER.read_text(encoding="utf-8-sig"), str(VERIFIER), "exec"), globals())
OUT = ROOT / "Saved/MapOverview/GridZoomDiagnostics"
OUT.mkdir(parents=True, exist_ok=True)
_original_stats = stats


def numbers(value):
    if hasattr(value, "z"):
        return [value.x, value.y, value.z]
    if hasattr(value, "x"):
        return [value.x, value.y]
    return str(value)


def projected(pc, point):
    value = unreal.GameplayStatics.project_world_to_screen(pc, point, False)
    if isinstance(value, (tuple, list)):
        value = next((v for v in value if isinstance(v, unreal.Vector2D)), None)
    return numbers(value) if value is not None else None


def spacing(pc, point, step):
    a = projected(pc, point)
    x = projected(pc, point + unreal.Vector(step, 0, 0))
    y = projected(pc, point + unreal.Vector(0, step, 0))
    return {"center_world": numbers(point), "center_screen": a, "x_screen": x, "y_screen": y,
            "minimum_pixels": min(math.dist(a, x), math.dist(a, y))
            if all(isinstance(v, list) for v in (a, x, y)) else None}


def stats(label, overview):
    _original_stats(label, overview)
    row = REPORT["stats"][-1]
    try:
        world = EDITOR.get_game_world()
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        build = subsystem(unreal.FF_BuildModeSubsystem, world)
        row["snap_cm"] = build.get_active_position_snap_cm()
        row["viewport"] = list(pc.get_viewport_size())
        row["view_target"] = pc.get_view_target().get_path_name()
        manager = unreal.GameplayStatics.get_player_camera_manager(world, 0)
        row["pov_location"] = numbers(manager.get_camera_location())
        row["pov_rotation"] = str(manager.get_camera_rotation())
        row["pov_fov"] = manager.get_fov_angle()
        camera = next(obj for obj in unreal.ObjectIterator(unreal.FF_OverviewCameraPawn)
                      if obj.get_world() == world)
        camera_state = camera.get_camera_state()
        focus = camera_state.get_editor_property("focus_location")
        row["native_focus"] = numbers(focus)
        row["native_pitch"] = camera_state.get_editor_property("pitch")
        row["focus_projection"] = spacing(pc, focus, row["snap_cm"])
        row["camera_components"] = [{"name": c.get_name(), "location": numbers(c.get_world_location()),
                                    "rotation": str(c.get_world_rotation()), "active": c.is_active(),
                                    "fov": c.get_editor_property("field_of_view"),
                                    "projection_mode": str(c.get_editor_property("projection_mode")),
                                    "ortho_width": c.get_editor_property("ortho_width")}
                                   for c in camera.get_components_by_class(unreal.CameraComponent)]
        row["spring_arms"] = [{"name": c.get_name(), "length": c.get_editor_property("target_arm_length"),
                              "rotation": str(c.get_world_rotation())}
                             for c in camera.get_components_by_class(unreal.SpringArmComponent)]
        if label == "surrounding":
            row["zones"] = [{"id": str(z.get_editor_property("zone_id")), "transform": str(z.get_actor_transform()),
                             "bounds_local": [numbers(p) for p in z.get_boundary_polygon_local_cm()]}
                            for z in unreal.ObjectIterator(unreal.FFZSGameplayZone) if z.get_world() == world]
        ray = unreal.GameplayStatics.deproject_screen_to_world(pc,
            unreal.Vector2D(row["viewport"][0] * 0.5, row["viewport"][1] * 0.45))
        row["deprojection"] = str(ray)
        if ray:
            origin, direction = ray
            if abs(direction.z) > 0.00001:
                plane_point = origin + direction * ((focus.z - origin.z) / direction.z)
                row["focus_plane_projection"] = spacing(pc, plane_point, row["snap_cm"])
            pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
            hit = unreal.SystemLibrary.line_trace_single(world, origin, origin + direction * 2000000.0,
                unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [pawn] if pawn else [],
                unreal.DrawDebugTrace.NONE, True)
            if isinstance(hit, (tuple, list)):
                row["center_trace_success"] = next((item for item in hit if isinstance(item, bool)), None)
                hit = next((item for item in hit if isinstance(item, unreal.HitResult)), None)
            row["center_hit"] = str(hit)
            if hit:
                parts = unreal.GameplayStatics.break_hit_result(hit)
                row["center_trace_success"] = parts[0]
                if parts[0]:
                    row["hit_projection"] = spacing(pc, parts[5], row["snap_cm"])
    except Exception:
        row["diagnostic_error"] = traceback.format_exc()
    write()

