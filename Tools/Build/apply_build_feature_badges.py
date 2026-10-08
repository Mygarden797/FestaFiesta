"""Add first-use badges to the existing toolbar in an idle Entry editor.

Backs up only WBP_FF_BuildTools, retains all authored widgets and slots, and
checks badge geometry plus no-op behavior on a second migration.
"""

from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil
import traceback

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Saved/BuildFeatureBadges"
RUN = OUT / datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S-%f")
TOOLS = "/Game/FestaFiesta/Building/FF_WBP_Building/Controls/WBP_FF_BuildTools"
VERSION = "FF.BuildFeatureBadges.20261008.v1"
COMMAND = "FF.MigrateBuildFeatureBadges"
TOPICS = ("MultiSelection", "BuildGrid", "BuildSnap", "FacilityRanges",
          "BuildSettings", "BuildCatalog", "BuildControls")
REPORT = {"status": "RUNNING"}


def widgets(blueprint):
    return {widget.get_name(): widget for widget in unreal.ObjectIterator(unreal.Widget)
            if widget.get_outer() and widget.get_outer().get_class().get_name() == "WidgetTree"
            and widget.get_outer().get_outer() == blueprint}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world and world.get_path_name().startswith("/Engine/Maps/Entry."), "Requires idle Entry map"
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Stop PIE first"
    blueprint = unreal.load_asset(TOOLS)
    assert blueprint, TOOLS
    toolbar_file = ROOT / "Content" / (TOOLS.removeprefix("/Game/") + ".uasset")
    RUN.mkdir(parents=True, exist_ok=True)
    shutil.copy2(toolbar_file, RUN / toolbar_file.name)

    original = widgets(blueprint)
    original_parents = {name: widget.get_parent() for name, widget in original.items()}
    original_slots = {name: widget.get_editor_property("slot") for name, widget in original.items()}
    original_art = {name: widget.get_editor_property("background").export_text()
                    for name, widget in original.items() if isinstance(widget, unreal.Border)}
    original_text = {name: (str(widget.get_text()), widget.get_editor_property("color_and_opacity").export_text())
                     for name, widget in original.items() if isinstance(widget, unreal.TextBlock)}
    unreal.SystemLibrary.execute_console_command(world, COMMAND)

    toolbar = widgets(blueprint)
    assert unreal.EditorAssetLibrary.get_metadata_tag(blueprint, VERSION) == "1", "Badge migration did not complete"
    for name, widget in original.items():
        assert toolbar.get(name) == widget, f"Original widget replaced: {name}"
        assert widget.get_parent() == original_parents[name], f"Original widget reparented: {name}"
        assert widget.get_editor_property("slot") == original_slots[name], f"Original slot replaced: {name}"
    for name, brush in original_art.items():
        assert toolbar[name].get_editor_property("background").export_text() == brush, f"Original artwork changed: {name}"
    for name, appearance in original_text.items():
        widget = toolbar[name]
        assert (str(widget.get_text()), widget.get_editor_property("color_and_opacity").export_text()) == appearance, f"Original text changed: {name}"

    badge_names = []
    for index, topic in enumerate(TOPICS, 1):
        badge = toolbar.get(f"Badge_{topic}")
        mark = toolbar.get(f"Text_Badge_{topic}")
        assert isinstance(badge, unreal.Border), topic
        assert isinstance(mark, unreal.TextBlock) and str(mark.get_text()) == "!", topic
        assert badge.get_content() == mark, topic
        canvas = toolbar[f"Size_BuildToolbarLine{index}"].get_content()
        assert isinstance(canvas, unreal.CanvasPanel) and badge.get_parent() == canvas, topic
        assert badge.get_visibility() == unreal.SlateVisibility.COLLAPSED, topic
        assert mark.get_visibility() == unreal.SlateVisibility.HIT_TEST_INVISIBLE, topic
        slot = badge.get_editor_property("slot")
        assert isinstance(slot, unreal.CanvasPanelSlot), topic
        position, size = slot.get_position(), slot.get_size()
        assert (position.x, position.y) == (32.0, -3.0), topic
        assert (size.x, size.y) == (18.0, 18.0) and slot.get_z_order() >= 10, topic
        badge_names.append(badge.get_name())

    first_hash = digest(toolbar_file)
    unreal.SystemLibrary.execute_console_command(world, COMMAND)
    assert first_hash == digest(toolbar_file), "Second badge migration rewrote the toolbar"
    assert set(toolbar) == set(widgets(blueprint)), "Second badge migration duplicated widgets"
    REPORT.update(status="PASS", migration=COMMAND, idempotent=True, badges=badge_names,
                  preserved_widgets=len(original), toolbar_hash=first_hash, backup=str(RUN))


try:
    main()
except Exception:
    REPORT.update(status="FAIL", error=traceback.format_exc())
    unreal.log_error(REPORT["error"])
finally:
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "latest-result.json").write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("FF_BUILD_FEATURE_BADGES_RESULT: " + REPORT["status"])
if REPORT["status"] != "PASS":
    raise RuntimeError("Build feature badge migration failed; see Saved/BuildFeatureBadges/latest-result.json")
