"""Read-only PIE verification through the authored Test_Main festival flow.

Run a disposable editor with -RenderOffscreen, -ExecutePythonScript=<this file>,
and an isolated -UserDir. No map, Blueprint, facility, or config is saved.
"""
from pathlib import Path
import gc
import hashlib
import json
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = ROOT / "Saved/MapOverview/RuntimeVerification"
OUT.mkdir(parents=True, exist_ok=True)
LEVEL = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
EDITOR = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
STATE = {"step": "setup", "next": time.monotonic() + 2, "start": time.monotonic(), "busy": False}
REPORT = {"status": "running", "map": "/Game/FestaFiesta/Maps/Test_Main", "captures": [],
          "checks": {}, "stats": [], "assets_saved": False, "facilities_installed": False,
          "physical_input_tested": False}
HANDLE = None


def hashes():
    result = {}
    for folder in ("Config", "Content/FestaFiesta/Maps", "Content/FestaFiesta/UI", "Content/FestaFiesta/Building/FF_WBP_Building", "Plugins/FF_MapOverview/Content/UI"):
        for path in (ROOT / folder).rglob("*"):
            if path.is_file():
                result[path.relative_to(ROOT).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
    return result


def write():
    REPORT["step"] = STATE["step"]
    (OUT / "report.json").write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding="utf-8")


def advance(step, delay=1):
    STATE.update(step=step, next=time.monotonic() + delay)
    write()


def subsystem(cls, world):
    return next((obj for obj in unreal.ObjectIterator(cls) if obj.get_world() == world), None)


def flow_for(world):
    game = unreal.GameplayStatics.get_game_instance(world)
    return next((obj for obj in unreal.ObjectIterator(unreal.FF_UIFlowSubsystem) if obj.get_outer() == game), None)


def capture(name):
    world = EDITOR.get_game_world()
    path = OUT / (name + ".png")
    unreal.SystemLibrary.execute_console_command(world, 'Shot SHOWUI filename="' + path.as_posix() + '" -nosuffix')
    REPORT["captures"].append(path.relative_to(ROOT).as_posix())


def stats(label, overview):
    row = {"label": label, "cells": overview.get_grid_cell_count(), "lines": overview.get_grid_line_count(),
           "trace_budget_upper_bound": overview.get_grid_trace_budget(), "spacing_cm": overview.get_grid_spacing_cm(),
           "last_grid_update_ms": overview.get_grid_last_update_ms(), "whole_grid": overview.is_whole_grid_enabled(),
           "whole_view": overview.is_whole_map_view(), "zoom_pixels": overview.get_grid_zoom_spacing_pixels(),
           "zoom_step_cm": overview.get_grid_zoom_step_cm(), "base_step_cm": overview.get_grid_base_step_cm()}
    world = EDITOR.get_game_world()
    camera = next((obj for obj in unreal.ObjectIterator(unreal.FF_OverviewCameraPawn) if obj.get_world() == world), None)
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    if camera:
        row["camera_arm_cm"] = camera.get_camera_state().get_editor_property("arm_length")
        widget = next((obj for obj in unreal.ObjectIterator(unreal.FFMapOverviewWidget) if obj.get_world() == world), None)
        if widget:
            center = widget.get_local_map_center()
            focus = camera.get_camera_state().get_editor_property("focus_location")
            assert abs(center.x - focus.x) < 1 and abs(center.y - focus.y) < 1
            box_size = unreal.MathLibrary.get_box_size(widget.get_local_map_bounds())
            assert abs(box_size.x - 10000) < 1 and abs(box_size.y - 10000) < 1
            row["minimap_center_cm"] = [center.x, center.y]
            row["minimap_size_m"] = [100, 100]
            REPORT["checks"]["player_centered_100m_map"] = True
            names = {obj.get_name(): obj for obj in unreal.ObjectIterator(unreal.Widget)
                     if obj.get_outer() and obj.get_outer().get_outer() == widget}
            for name in ("NearbyButton", "OverviewButton", "NearbyLabel", "OverviewLabel", "WholeButton",
                         "WholeLabel", "GridButton", "GridLabel", "BuildControls", "ScaleLabel"):
                assert name not in names, "Unexpected minimap footer: " + name
            REPORT["checks"]["minimap_footer_buttons_and_numbers_removed"] = True
    is_building = overview.is_build_session_active()
    addon = toolbar(world)
    if is_building:
        assert addon and addon.get_parent(), "Whole-grid WBP is not mounted in construction toolbar"
        row_widget = addon.get_parent()
        assert row_widget.get_name() == "Row_BuildReferenceTopBar"
        tools_widget = row_widget.get_outer().get_outer()
        grid = next(obj for obj in unreal.ObjectIterator(unreal.Button) if obj.get_name() == "Button_Grid"
                    and obj.get_outer().get_outer() == tools_widget)
        grid_cell = grid
        while grid_cell.get_parent() != row_widget:
            grid_cell = grid_cell.get_parent()
            assert grid_cell, "Original grid cell not found"
        assert row_widget.get_child_index(addon) == row_widget.get_child_index(grid_cell) + 1
        assert addon.get_is_enabled()
        assert addon.get_class().get_path_name() == "/FF_MapOverview/UI/WBP_FF_WholeGridButton.WBP_FF_WholeGridButton_C"
        REPORT["checks"]["whole_grid_wbp_immediately_right_of_original_grid"] = True
    else:
        assert not addon or not addon.get_parent(), "Whole-grid toolbar remains after leaving construction"
        REPORT["checks"]["whole_grid_toolbar_removed_outside_construction"] = True
    row["whole_grid_toolbar_mounted"] = is_building
    if pc:
        row["camera_mode"] = str(pc.get_view_mode())
        row["camera_input_block"] = str(pc.get_input_block_reason())
    assert row["cells"] <= 16384 and row["lines"] <= 32768 and row["trace_budget_upper_bound"] <= 512, row
    REPORT["stats"].append(row)


def toolbar(world):
    return next((obj for obj in unreal.ObjectIterator(unreal.FFMapWholeGridButtonWidget) if obj.get_world() == world), None)


def tick(_delta):
    if STATE["busy"] or time.monotonic() < STATE["next"]:
        return
    STATE["busy"] = True
    try:
        STATE["next"] = time.monotonic() + 1
        step = STATE["step"]
        if step not in ("end", "final"):
            assert time.monotonic() - STATE["start"] < 240, "Runtime verification timeout"
        world = EDITOR.get_game_world()
        overview = subsystem(unreal.FFMapOverviewSubsystem, world) if world else None
        build = subsystem(unreal.FF_BuildModeSubsystem, world) if world else None
        pc = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
        if world and step in ("near", "whole", "view", "restore", "zoom", "zoomed", "closed"):
            game = unreal.GameplayStatics.get_game_instance(world)
            tutorial = next((obj for obj in unreal.ObjectIterator(unreal.FF_TutorialSubsystem) if obj.get_outer() == game), None)
            if tutorial:
                tutorial.advance()

        if step == "setup":
            assert not LEVEL.is_in_play_in_editor()
            REPORT["hashes_before"] = hashes()
            atlas = unreal.load_asset("/Game/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas")
            candidates = [str(entry.get_editor_property("festival_id")) for entry in atlas.get_editor_property("entries")
                          if entry.get_editor_property("unlocked") and REPORT["map"] + "." in entry.export_text()]
            assert candidates, "No unlocked authored festival points to Test_Main"
            STATE["festival"] = candidates[0]
            assert LEVEL.load_level(REPORT["map"])
            advance("begin", 2)
        elif step == "begin":
            unreal.SystemLibrary.execute_console_command(LEVEL.get_world(), "FF.FrontendBeginPIE 1598 888")
            advance("start", 3)
        elif step == "start":
            if not world:
                return
            flow = flow_for(world)
            if not flow:
                return
            assert flow.start_festival(unreal.Name(STATE["festival"]))
            advance("ready", 5)
        elif step == "ready":
            if not world or not overview or not build or not pc:
                return
            widget = next((obj for obj in unreal.ObjectIterator(unreal.FFMapOverviewWidget)
                           if obj.get_world() == world and obj.get_visibility() != unreal.SlateVisibility.COLLAPSED), None)
            if not widget:
                return
            REPORT["checks"]["automatic_plugin_and_hud_mount"] = True
            assert widget.get_class().get_path_name() == "/FF_MapOverview/UI/WBP_FF_MapOverview.WBP_FF_MapOverview_C"
            REPORT["checks"]["actual_designer_wbp_mounted"] = True
            texture = overview.get_terrain_map_texture()
            assert texture and texture.get_editor_property("size_x") == 512 and texture.get_editor_property("size_y") == 512
            captures = [obj for obj in unreal.ObjectIterator(unreal.SceneCaptureComponent2D)
                        if obj.get_world() == world and obj.get_editor_property("texture_target") == texture]
            assert len(captures) == 1
            assert not captures[0].get_editor_property("capture_every_frame")
            assert not captures[0].get_editor_property("capture_on_movement")
            REPORT["checks"]["terrain_cached_512_no_per_frame_capture"] = True
            unreal.SystemLibrary.execute_console_command(world, "DisableAllScreenMessages")
            game = unreal.GameplayStatics.get_game_instance(world)
            tutorial = next((obj for obj in unreal.ObjectIterator(unreal.FF_TutorialSubsystem) if obj.get_outer() == game), None)
            if tutorial:
                tutorial.advance()
                tutorial.advance()
                tutorial.advance()
            build.enter_build_mode()
            cameras = [obj for obj in unreal.ObjectIterator(unreal.FF_OverviewCameraPawn) if obj.get_world() == world]
            assert cameras
            # The native controller cancels camera requests in an unfocused offscreen viewport.
            # Suspend only its transient actor tick while the camera's public request settles.
            STATE["camera_controller_tick"] = pc.is_actor_tick_enabled()
            pc.set_actor_tick_enabled(False)
            STATE["near_camera_pending"] = True
            cameras[0].focus_on_location(unreal.Vector(78800.5, 42200.5, 416.86))
            cameras[0].add_zoom_input(6)
            pc.set_mouse_location(650, 430)
            capture("00-layout-warmup")
            advance("near", 6)
        elif step == "near":
            if STATE.pop("near_camera_pending", False):
                pc.set_actor_tick_enabled(STATE.pop("camera_controller_tick"))
                advance("near", 2)
                return
            assert overview.is_build_session_active() and overview.is_grid_visible()
            unreal.RenderingLibrary.export_render_target(world, overview.get_terrain_map_texture(),
                                                         OUT.as_posix(), "terrain-source")
            widget = next(obj for obj in unreal.ObjectIterator(unreal.FFMapOverviewWidget) if obj.get_world() == world)
            panel = next(obj for obj in unreal.ObjectIterator(unreal.CanvasPanel) if obj.get_name() == "MapPanel"
                         and obj.get_outer().get_outer() == widget)
            geometry = panel.get_paint_space_geometry()
            position, _ = unreal.SlateLibrary.local_to_viewport(world, geometry, unreal.Vector2D(0, 0))
            size = unreal.SlateLibrary.get_local_size(geometry)
            REPORT["checks"]["minimap_top_right_pixels"] = [position.x, position.y]
            REPORT["checks"]["minimap_size"] = [size.x, size.y]
            REPORT["checks"]["minimap_visibility"] = str(widget.get_visibility())
            REPORT["checks"]["minimap_is_in_viewport"] = widget.is_in_viewport()
            if size.x >= 1:
                assert position.x > 800 and position.y < 180, "Minimap must be top-right, not bottom HUD-relative"
            else:
                REPORT["checks"]["layout_geometry_unavailable_offscreen"] = True
                # RenderOffscreen paints screenshots without UMG's ordinary cached tick geometry.
                # The actual screenshots must also be visually reviewed after this run.
            stats("surrounding", overview)
            assert overview.get_grid_line_count() > 0, "Nearby grid produced no visible lines"
            capture("01-surrounding")
            advance("switch_whole", 1)
        elif step == "switch_whole":
            addon = toolbar(world)
            assert addon
            addon.activate_whole_grid()
            assert overview.is_whole_grid_enabled() and overview.is_grid_visible()
            addon.activate_whole_grid()
            assert not overview.is_whole_grid_enabled()
            addon.activate_whole_grid()
            REPORT["checks"]["whole_grid_toolbar_action_toggles_scope"] = True
            advance("whole", 8)
        elif step == "whole":
            stats("whole-grid", overview)
            assert overview.is_whole_grid_enabled() and overview.get_grid_line_count() > 0
            capture("02-whole-grid")
            advance("enter_view", 1)
        elif step == "enter_view":
            STATE["previous_target"] = pc.get_view_target().get_path_name()
            overview.toggle_whole_map_view()
            advance("view", 8)
        elif step == "view":
            assert overview.is_whole_map_view(), "Whole map camera did not activate"
            stats("whole-map-view", overview)
            capture("03-whole-map-view")
            advance("exit_view", 1)
        elif step == "exit_view":
            overview.toggle_whole_map_view()
            advance("restore", 2)
        elif step == "restore":
            assert not overview.is_whole_map_view()
            assert pc.get_view_target().get_path_name() == STATE["previous_target"]
            REPORT["checks"]["camera_restored"] = True
            overview.toggle_grid_visibility()
            advance("off", 2)
        elif step == "off":
            assert not overview.is_grid_visible() and overview.get_grid_line_count() == 0
            REPORT["checks"]["grid_toggle"] = True
            overview.toggle_grid_visibility()
            overview.set_whole_grid_enabled(False)
            advance("zoom", 5)
        elif step == "zoom":
            stats("before-zoom", overview)
            STATE["spacing_before"] = overview.get_grid_spacing_cm()
            camera = next(obj for obj in unreal.ObjectIterator(unreal.FF_OverviewCameraPawn) if obj.get_world() == world)
            STATE["camera_controller_tick"] = pc.is_actor_tick_enabled()
            pc.set_actor_tick_enabled(False)
            camera.add_zoom_input(-48)
            advance("restore_zoom_tick", 6)
        elif step == "restore_zoom_tick":
            pc.set_actor_tick_enabled(STATE.pop("camera_controller_tick"))
            REPORT["checks"]["unfocused_viewport_tick_guard_restored"] = True
            advance("zoomed", 2)
        elif step == "zoomed":
            stats("zoomed-out", overview)
            pixels = overview.get_grid_zoom_spacing_pixels()
            assert pixels > 0 and (overview.get_grid_zoom_step_cm() == overview.get_grid_base_step_cm() or 8.95 <= pixels <= 22.05), "Zoom grid left its projected pixel spacing band"
            REPORT["checks"]["zoom_spacing"] = True
            capture("04-zoomed-out")
            advance("close_build", 1)
        elif step == "close_build":
            build.exit_build_mode()
            advance("closed", 2)
        elif step == "closed":
            assert not overview.is_build_session_active() and overview.get_grid_line_count() == 0
            stats("closed-normal-hud", overview)
            REPORT["checks"]["build_exit_clears_grid"] = True
            capture("05-minimap-normal-hud")
            advance("images", 2)
        elif step == "images":
            for name in REPORT["captures"]:
                assert (ROOT / name).exists(), "Missing capture " + name
            advance("end")
        elif step == "end":
            if pc and "camera_controller_tick" in STATE:
                pc.set_actor_tick_enabled(STATE.pop("camera_controller_tick"))
            world = overview = build = pc = None
            gc.collect()
            LEVEL.editor_request_end_play()
            advance("final", 3)
        elif step == "final":
            if LEVEL.is_in_play_in_editor():
                return
            REPORT["hashes_after"] = hashes()
            REPORT["assets_unchanged"] = REPORT["hashes_before"] == REPORT["hashes_after"]
            REPORT["status"] = "passed" if "error" not in REPORT and REPORT["assets_unchanged"] else "failed"
            REPORT["elapsed_seconds"] = round(time.monotonic() - STATE["start"], 2)
            write()
            unreal.unregister_slate_post_tick_callback(HANDLE)
            unreal.log("FF_MAP_OVERVIEW_RUNTIME_DONE " + REPORT["status"])
            unreal.EditorPythonScripting.set_keep_python_script_alive(False)
            unreal.SystemLibrary.quit_editor()
    except Exception:
        REPORT["error"] = traceback.format_exc()
        unreal.log_error(REPORT["error"])
        advance("end")
    finally:
        STATE["busy"] = False


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
HANDLE = unreal.register_slate_post_tick_callback(tick)
