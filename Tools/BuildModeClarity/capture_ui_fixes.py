"""Capture catalog/help, Pause Hub/options, keycaps and an existing facility's combos.

Run in a freshly built, idle Entry editor with -ExecutePythonScript and
-RenderOffscreen (never -nullrhi). This reuses capture_runtime.py's production
festival startup, screenshots, file hashes and teardown without changing it.
The same Saved/BuildModeClarity/runtime-capture-request.json accepts a fresh
runtime-* output name, width, height and timeout_seconds.

No facility is installed and no asset is saved. Festival startup can autosave;
all existing Saved/SaveGames/*.sav files are backed up before PIE and restored
after normal or failed capture teardown. New direct .sav files from the run
are removed only after checking their exact paths and captured hashes. An
abrupt process termination retains the backup and its manifest for recovery.
If the production map has no installed facility, the report records that limitation; use the real-RHI automation
FestaFiesta.UI.FacilityInfo.SynergyTab for its authored combo/empty-state PNGs.
Public game actions are exercised; this is not physical keyboard input testing.
"""
from pathlib import Path
import hashlib
import json
import runpy
import shutil
import time
import traceback

import unreal


ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
_module = runpy.run_path(str(ROOT / "Tools/BuildModeClarity/capture_runtime.py"))
BASE_TICK = _module["tick"]
# runpy returns a namespace copy; callbacks retain their original globals.
BASE = BASE_TICK.__globals__
unreal.unregister_slate_post_tick_callback(BASE["HANDLE"])
STATE, REPORT = BASE["STATE"], BASE["REPORT"]
advance = BASE["advance"]
game_world = BASE["game_world"]
live_widget = BASE["live_widget"]
find_child = BASE["find_child"]
widget_state = BASE["widget_state"]
record_capture = BASE["record_capture"]
REPORT["capture_scope"] = "20260924 build UI overlap and keyboard help fixes"
REPORT["limitations"] = ["Public UI actions are used; physical keyboard input is not tested."]
REPORT["synergy_automation_fallback"] = {
    "test": "FestaFiesta.UI.FacilityInfo.SynergyTab",
    "images": ["Saved/Automation/FacilitySynergy/FacilitySynergyTab720.png",
               "Saved/Automation/FacilitySynergy/FacilitySynergyTabEmpty720.png"],
}
SAVE_DIR = (ROOT / "Saved/SaveGames").resolve()
assert SAVE_DIR.parent == (ROOT / "Saved").resolve(), "SaveGames must be directly inside this project's Saved directory"
SAVE_BACKUP = BASE["OUT"] / ("savegames-before-" + time.strftime("%Y%m%d-%H%M%S"))
SAVE_SNAPSHOT = {}
SAVE_RESTORE_ATTEMPTED = False


def checked_save_path(name):
    assert Path(name).name == name and Path(name).suffix.lower() == ".sav", "Only direct .sav filenames are permitted"
    path = SAVE_DIR / name
    assert path.resolve().parent == SAVE_DIR and not path.is_symlink(), "Save path escapes the verified SaveGames directory"
    return path


def save_file_state(path):
    stat = path.stat()
    return {"sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "bytes": stat.st_size, "mtime_ns": stat.st_mtime_ns}


def scan_save_games():
    if not SAVE_DIR.exists():
        return {}
    return {path.name: save_file_state(checked_save_path(path.name))
            for path in sorted(SAVE_DIR.iterdir())
            if path.is_file() and path.suffix.lower() == ".sav"}


def snapshot_save_games():
    assert not BASE["LEVEL"].is_in_play_in_editor(), "Save snapshot must precede PIE startup"
    SAVE_BACKUP.mkdir(parents=True, exist_ok=False)
    snapshot = scan_save_games()
    for name, expected in snapshot.items():
        shutil.copy2(checked_save_path(name), SAVE_BACKUP / name)
        assert save_file_state(SAVE_BACKUP / name) == expected, "Save backup changed while being copied: " + name
    (SAVE_BACKUP / "manifest.json").write_text(json.dumps({
        "save_directory": str(SAVE_DIR), "files": snapshot,
    }, indent=2), encoding="utf-8")
    REPORT["savegames_backup"] = str(SAVE_BACKUP.relative_to(ROOT))
    REPORT["savegames_before"] = snapshot
    REPORT["savegames_restored"] = False
    BASE["write_report"]()
    return snapshot


def restore_save_games():
    assert not BASE["LEVEL"].is_in_play_in_editor(), "Save restoration requires completed PIE teardown"
    current = scan_save_games()
    REPORT["savegames_after_capture"] = current
    # Validate every backup before replacing any original. Keep the backup and
    # manifest after recovery so interrupted runs can be inspected manually.
    for name, expected in SAVE_SNAPSHOT.items():
        assert save_file_state(SAVE_BACKUP / name) == expected, "Save backup is no longer intact: " + name
    SAVE_DIR.mkdir(parents=True, exist_ok=True)
    for name in SAVE_SNAPSHOT:
        target = checked_save_path(name)
        if name in current:
            assert save_file_state(target) == current[name], "Save changed after teardown; refusing to overwrite: " + name
        shutil.copy2(SAVE_BACKUP / name, target)
    created = sorted(set(current) - set(SAVE_SNAPSHOT))
    for name in created:
        target = checked_save_path(name)
        assert save_file_state(target) == current[name], "New save changed after teardown; refusing to remove: " + name
        # Exact verified direct .sav only; no recursive deletion or glob delete.
        target.unlink()
    REPORT["savegames_removed_created_slots"] = created
    REPORT["savegames_after_restore"] = scan_save_games()
    REPORT["savegames_restored"] = REPORT["savegames_after_restore"] == SAVE_SNAPSHOT
    assert REPORT["savegames_restored"], "SaveGames did not return to the original bytes and timestamps"


def finish_save_restoration():
    global SAVE_RESTORE_ATTEMPTED
    if SAVE_RESTORE_ATTEMPTED:
        return
    SAVE_RESTORE_ATTEMPTED = True
    try:
        restore_save_games()
        unreal.log("FF_UI_FIXES_SAVEGAMES_RESTORED " + str(SAVE_BACKUP))
    except Exception:
        REPORT["savegames_restored"] = False
        REPORT["savegames_restore_error"] = traceback.format_exc()
        REPORT["status"] = "failed"
        unreal.log_error("FF_UI_FIXES_SAVEGAMES_RESTORE_FAIL " + REPORT["savegames_restore_error"])
    BASE["write_report"]()


def build_system():
    return BASE["world_subsystem"](unreal.FF_BuildModeSubsystem, game_world())


def settings_system():
    return BASE["gi_subsystem"](unreal.FF_FestivalSettingsSubsystem, game_world())


def tools_widget():
    # Pause deliberately collapses the top-level widget, so live_widget cannot
    # be used to check that the retained production instance stays hidden.
    world = game_world()
    tools = next((obj for obj in unreal.ObjectIterator(unreal.FF_BuildToolsWidget)
                  if obj.get_world() == world), None)
    assert tools, "Production build tools were not created"
    return tools


def effectively_visible(widget):
    while widget:
        if widget.get_visibility() in (unreal.SlateVisibility.COLLAPSED,
                                       unreal.SlateVisibility.HIDDEN):
            return False
        widget = widget.get_parent()
    return True


def help_visible():
    tools = tools_widget()
    return effectively_visible(tools) and effectively_visible(find_child(tools, "Border_PlacementHelp"))


def capture_help(key, name, with_catalog=False):
    tools = tools_widget()
    row = {"build_state": str(build_system().get_build_mode_state()),
           "help_visible": help_visible(),
           "widgets": widget_state(tools, ("Border_BuildTopBar", "Border_PlacementHelp",
                                          "Text_BuildHelpPrimary", "Text_BuildHelpRotate",
                                          "Text_BuildHelpMove", "Text_BuildHelpPrecision"))}
    assert row["help_visible"], "The opened control guide must be visible"
    if with_catalog:
        panel = live_widget(unreal.FF_PrepPanelBuild, game_world())
        assert panel, "The production catalog must remain visible beside the help"
        row["catalog"] = widget_state(panel, ("Size_ClarityCatalogDock",))
        help_rect = row["widgets"]["Border_PlacementHelp"]["bounds_pixels"]
        catalog_rect = row["catalog"]["Size_ClarityCatalogDock"]["bounds_pixels"]
        if help_rect and catalog_rect:
            row["help_overlaps_catalog"] = (
                min(help_rect[2], catalog_rect[2]) > max(help_rect[0], catalog_rect[0]) + 1
                and min(help_rect[3], catalog_rect[3]) > max(help_rect[1], catalog_rect[1]) + 1)
            assert not row["help_overlaps_catalog"], "Control guide overlaps the catalog"
        else:
            row["geometry_limitation"] = "Review SHOWUI PNG; offscreen child geometry is unavailable."
    record_capture(key, name, row)


def capture_pause(key, name, screen_class):
    tools = tools_widget()
    screen = live_widget(screen_class, game_world())
    assert screen, "The requested pause/options screen is not visible"
    row = {"screen": screen.get_path_name(),
           "build_state": str(build_system().get_build_mode_state()),
           "build_input_suspended": build_system().is_input_suspended_for_pause_menu(),
           "build_tools_visibility": str(tools.get_visibility()),
           "build_tools_visible": effectively_visible(tools),
           "toolbar_visible": effectively_visible(tools)
               and effectively_visible(find_child(tools, "Border_BuildTopBar"))}
    assert row["build_input_suspended"], "Pause must retain its build-input lease"
    assert not row["build_tools_visible"] and not row["toolbar_visible"], "Build HUD leaks above pause/options"
    record_capture(key, name, row)


def tick(delta):
    stage = STATE["stage"]
    if stage != "capture" and not stage.startswith("audit_"):
        BASE_TICK(delta)
        if stage == "final" and REPORT["status"] in ("captured", "failed"):
            finish_save_restoration()
        return
    if STATE.get("busy") or time.monotonic() < STATE["next"]:
        return
    STATE["busy"] = True
    try:
        assert time.monotonic() - STATE["started"] < int(BASE["CONFIG"].get("timeout_seconds", 480)), "Capture timed out"
        STATE["next"] = time.monotonic() + 1
        world = game_world()
        controller = unreal.GameplayStatics.get_player_controller(world, 0)
        build = build_system()
        if stage == "capture":
            BASE["inspect_catalog"]("catalog", "01-catalog-Test_Main")
            if not help_visible():
                tools_widget().toggle_help()
            advance("audit_catalog_help", 3)
        elif stage == "audit_catalog_help":
            capture_help("catalog_help", "02-catalog-with-keyboard-help", True)
            advance("audit_open_pause", 2)
        elif stage == "audit_open_pause":
            STATE["pre_pause_build_state"] = str(build.get_build_mode_state())
            assert settings_system().open_pause_menu(controller), "Pause Hub refused to open"
            REPORT["pause_immediately_hides_build_tools"] = not effectively_visible(tools_widget())
            assert REPORT["pause_immediately_hides_build_tools"], "Build HUD must hide in the same pause call"
            advance("audit_pause", 3)
        elif stage == "audit_pause":
            capture_pause("pause", "03-pause-with-build-hud-hidden", unreal.FF_PauseHubScreen)
            advance("audit_open_options", 2)
        elif stage == "audit_open_options":
            hub = live_widget(unreal.FF_PauseHubScreen, world)
            assert hub and hub.request_action(unreal.FF_PauseHubAction.SYSTEM_SETTINGS), "System Settings action failed"
            advance("audit_options", 3)
        elif stage == "audit_options":
            capture_pause("options", "04-options-with-build-hud-hidden", unreal.FF_FestivalSettingsScreen)
            assert settings_system().is_festival_settings_open()
            advance("audit_close_options", 2)
        elif stage == "audit_close_options":
            # Offscreen startup may normalize an unsupported resolution into
            # an unsaved draft. The first public Back action then arms discard
            # confirmation and returns false; a later Back discards it safely.
            if not settings_system().is_festival_settings_open():
                advance("audit_close_pause", 2)
            else:
                attempts = STATE.get("options_back_attempts", 0) + 1
                assert attempts <= 3, "Options did not close through the bounded public Back flow"
                STATE["options_back_attempts"] = attempts
                closed = settings_system().close_festival_settings()
                REPORT["options_back_attempts"] = attempts
                advance("audit_close_pause" if closed else "audit_close_options", 2)
        elif stage == "audit_close_pause":
            assert not settings_system().is_festival_settings_open(), "Options must return to Pause Hub first"
            assert settings_system().close_pause_menu()
            advance("audit_restored", 3)
        elif stage == "audit_restored":
            assert not settings_system().is_pause_menu_open()
            assert not build.is_input_suspended_for_pause_menu()
            assert str(build.get_build_mode_state()) == STATE["pre_pause_build_state"]
            capture_help("restored", "05-restored-catalog-and-help", True)
            advance("audit_begin_placement", 2)
        elif stage == "audit_begin_placement":
            panel = live_widget(unreal.FF_PrepPanelBuild, world)
            item = next((item for item in panel.get_catalog_page_items() if item.is_affordable()), None)
            assert item, "The production catalog has no affordable preview candidate"
            assert panel.activate_catalog_facility(item.get_facility_definition_id())
            width, height = controller.get_viewport_size()
            controller.set_mouse_location(round(width * .54), round(height * .43))
            advance("audit_placement", 4)
        elif stage == "audit_placement":
            assert build.get_build_mode_state() == unreal.FF_BuildModeState.PLACING
            capture_help("placement_help", "06-placement-keyboard-help")
            advance("audit_direct", 2)
        elif stage == "audit_direct":
            build.toggle_direct_placement_adjustment()
            advance("audit_direct_help", 3)
        elif stage == "audit_direct_help":
            capture_help("direct_help", "07-direct-adjustment-keyboard-help")
            REPORT["direct_help"]["direct_adjustment_active"] = build.is_direct_placement_adjustment()
            if not REPORT["direct_help"]["direct_adjustment_active"]:
                REPORT["limitations"].append("Direct adjustment was unavailable at the real viewport cursor; no target was fabricated.")
            advance("audit_return_catalog", 2)
        elif stage == "audit_return_catalog":
            build.return_to_catalog()
            advance("audit_find_facility", 3)
        elif stage == "audit_find_facility":
            zones = BASE["world_subsystem"](unreal.FFZSWorldSubsystem, world)
            facilities = [obj for obj in unreal.ObjectIterator(unreal.FFZSFacilityActor)
                          if obj.get_world() == world
                          and zones.find_facility(obj.get_facility_instance_id()) == obj]
            facilities.sort(key=lambda obj: obj.get_path_name())
            facility = next((obj for obj in facilities if build.inspect_facility(obj)), None)
            if facility:
                STATE["inspected_facility_path"] = facility.get_path_name()
                advance("audit_open_synergy", 3)
            else:
                REPORT["limitations"].append("No existing inspectable facility was present; runtime synergy capture was skipped without installing a facility. Run the listed SynergyTab render test.")
                advance("wait_images", 3)
        elif stage == "audit_open_synergy":
            panel = live_widget(unreal.FF_FacilityInfoPanel, world)
            assert panel, "InspectFacility did not open the production information panel"
            panel.set_info_page(unreal.FF_FacilityInfoPage.SYNERGY)
            advance("audit_synergy", 3)
        elif stage == "audit_synergy":
            panel = live_widget(unreal.FF_FacilityInfoPanel, world)
            row = {"facility": STATE["inspected_facility_path"],
                   "widgets": widget_state(panel, ("Border_Panel", "Panel_SynergySection",
                                                   "Text_SynergySummary", "VerticalBox_SynergyEntries"))}
            assert row["widgets"]["Panel_SynergySection"]["visible"]
            record_capture("facility_synergy", "08-facility-information-combinations", row)
            advance("wait_images", 3)
    except Exception:
        REPORT["error"] = traceback.format_exc()
        unreal.log_error("FF_UI_FIXES_CAPTURE_FAIL " + REPORT["error"])
        advance("end_pie")
    finally:
        STATE["busy"] = False


# Existing teardown unregisters this replacement callback and releases Python.
try:
    SAVE_SNAPSHOT = snapshot_save_games()
except Exception:
    REPORT["status"] = "failed"
    REPORT["error"] = "SaveGames snapshot failed before PIE startup:\n" + traceback.format_exc()
    BASE["write_report"]()
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    raise
BASE["HANDLE"] = unreal.register_slate_post_tick_callback(tick)
