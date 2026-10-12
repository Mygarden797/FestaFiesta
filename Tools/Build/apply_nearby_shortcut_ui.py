"""Apply and verify the narrow N-shortcut migration in an idle Entry editor.

Run with UnrealEditor-Cmd -ExecutePythonScript using this script's absolute path.
Only the active BuildTools WBP is backed up and edited. The second command call
must leave saved bytes unchanged. Reports are retained in Saved/NearbyShortcut.
"""

from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil
import traceback

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
RUN = ROOT / "Saved/NearbyShortcut" / datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S-%f")
PACKAGE = "/Game/FestaFiesta/Building/FF_WBP_Building/Controls/WBP_FF_BuildTools"
TARGET = ROOT / "Content" / (PACKAGE.removeprefix("/Game/") + ".uasset")
COMMAND = "FF.ApplyBuildNearbyShortcut"
CAPTION = "가까운 빈자리로 이동"
BUTTON_CAPTION = "[N] 가까운 빈자리"
TOOLTIP = "N 또는 클릭으로 가까운 빈자리로 이동합니다. Enter로 배치를 확정하세요."
NEW_WIDGETS = {
    "Row_BuildHelpNearby", "Size_BuildHelpNearbyIcon", "Row_BuildHelpNearbyKeys",
    "Size_BuildHelpKey_N", "Border_BuildHelpKey_N", "Face_BuildHelpKey_N",
    "Text_BuildHelpKey_N", "Text_BuildHelpNearby",
}
REPORT = {"status": "RUNNING", "package": PACKAGE, "command": COMMAND}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def ui_hashes():
    return {path.relative_to(ROOT).as_posix(): digest(path)
            for base in (ROOT / "Content/FestaFiesta/UI", ROOT / "Content/FestaFiesta/Building/FF_WBP_Building")
            for path in base.rglob("*.uasset")}


def widgets(blueprint):
    return {widget.get_name(): widget for widget in unreal.ObjectIterator(unreal.Widget)
            if widget.get_outer() and widget.get_outer().get_class().get_name() == "WidgetTree"
            and widget.get_outer().get_outer() == blueprint}


def text_font(widget):
    font = widget.get_editor_property("font")
    return {"size": font.size, "typeface": str(font.typeface_font_name),
            "fontObject": font.font_object.get_path_name() if font.font_object else None}


def topology(widget_map):
    result = {}
    for name, widget in widget_map.items():
        parent = widget.get_parent()
        entry = {"class": widget.get_class().get_name(),
                 "parent": parent.get_name() if parent else None}
        if isinstance(widget, unreal.PanelWidget):
            entry["children"] = [widget.get_child_at(index).get_name()
                                 for index in range(widget.get_children_count())]
        result[name] = entry
    return result


def inspect():
    blueprint = unreal.load_asset(PACKAGE)
    assert blueprint, "Active BuildTools WBP must exist"
    widget_map = widgets(blueprint)
    column = widget_map["Column_BuildIconHelp"]
    button = widget_map["Button_FindNearby"]
    button_label = button.get_content()
    assert isinstance(button_label, unreal.TextBlock), "Nearby button must retain its authored text content"
    state = {"widgets": topology(widget_map),
             "buttonClass": button.get_class().get_name(),
             "buttonLabelName": button_label.get_name(),
             "buttonLabelFont": text_font(button_label),
             "buttonStyle": str(button.get_editor_property("widget_style")),
             "buttonCaption": str(button_label.get_text()),
             "buttonTooltip": str(button.get_editor_property("tool_tip_text")),
             "columnChildren": [column.get_child_at(index).get_name()
                                for index in range(column.get_children_count())]}
    if "Row_BuildHelpNearby" in widget_map:
        row = widget_map["Row_BuildHelpNearby"]
        lane = widget_map["Size_BuildHelpNearbyIcon"]
        key = widget_map["Text_BuildHelpKey_N"]
        caption = widget_map["Text_BuildHelpNearby"]
        state.update(rowVisibility=str(row.get_visibility()),
                     rowParent=row.get_parent().get_name(),
                     keyLaneWidth=lane.get_editor_property("width_override"),
                     keyLaneHeight=lane.get_editor_property("height_override"),
                     keyCaption=str(key.get_text()), keyFont=text_font(key),
                     helpCaption=str(caption.get_text()), helpFont=text_font(caption))
    return blueprint, widget_map, state


def verify(state):
    assert NEW_WIDGETS <= state["widgets"].keys(), "Nearby shortcut branch is incomplete"
    assert state["rowParent"] == "Column_BuildIconHelp", "Nearby row must use the existing guide column"
    children = state["columnChildren"]
    assert children.index("Row_BuildHelpNearby") == children.index("Row_BuildHelpRotate") + 1, \
        "Nearby row must immediately follow facility rotation"
    assert state["rowVisibility"] == str(unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE), "Saved row must be visible"
    # bHiddenInDesigner is not reflected to Python; the console command's C++
    # Verify checks that flag before reporting success or saving the Blueprint.
    assert state["keyLaneWidth"] == 124.0 and state["keyLaneHeight"] == 30.0, "Key lane must match the guide"
    assert state["keyCaption"] == "N" and state["keyFont"]["size"] == 14, "Physical N keycap is missing"
    assert state["helpCaption"] == CAPTION, "Nearby action caption does not match"
    assert state["buttonCaption"] == BUTTON_CAPTION, "Nearby button must teach the N shortcut"
    assert state["buttonTooltip"] == TOOLTIP, "Nearby button tooltip must explain movement and confirmation"


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world and world.get_path_name().startswith("/Engine/Maps/Entry."), "Requires idle Entry editor"
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Stop PIE first"
    before_hashes = ui_hashes()
    _, _, before = inspect()
    REPORT.update(before=before, beforeSha256=digest(TARGET))
    backup = RUN / "Backups" / TARGET.relative_to(ROOT)
    backup.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(TARGET, backup)
    assert digest(backup) == REPORT["beforeSha256"], "Backup bytes do not match the original WBP"
    REPORT["backup"] = str(backup)

    unreal.SystemLibrary.execute_console_command(world, COMMAND)
    _, _, after = inspect()
    verify(after)
    for name, original in before["widgets"].items():
        current = after["widgets"].get(name)
        assert current, "Existing widget was removed: " + name
        if name == "Column_BuildIconHelp":
            current = dict(current)
            current["children"] = [child for child in current["children"]
                                   if child != "Row_BuildHelpNearby"]
            original = dict(original)
            original["children"] = [child for child in original["children"]
                                    if child != "Row_BuildHelpNearby"]
        assert current == original, "Existing widget topology changed: " + name
    assert after["widgets"].keys() - before["widgets"].keys() <= NEW_WIDGETS, "Unexpected widgets were added"
    for key in ("buttonClass", "buttonLabelName", "buttonLabelFont", "buttonStyle"):
        assert after[key] == before[key], "Existing nearby button presentation changed: " + key

    after_hashes = ui_hashes()
    target_path = TARGET.relative_to(ROOT).as_posix()
    unexpected = [path for path in before_hashes.keys() | after_hashes.keys()
                  if path != target_path and before_hashes.get(path) != after_hashes.get(path)]
    assert not unexpected, "Unrelated UI assets changed: " + repr(unexpected)

    # An already migrated WBP must not be compiled or saved by the command.
    unreal.SystemLibrary.execute_console_command(world, COMMAND)
    _, _, repeated = inspect()
    verify(repeated)
    assert repeated == after, "Repeat command changed the saved guide state"
    assert ui_hashes() == after_hashes, "Repeat command changed saved asset bytes"
    REPORT.update(status="PASS", after=after, afterSha256=digest(TARGET),
                  unrelatedUiUnchanged=True, unrelatedUiAssetCount=len(before_hashes) - 1,
                  repeatSavedChanges=False,
                  changedAssets=[target_path] if before_hashes[target_path] != after_hashes[target_path] else [])


try:
    main()
except Exception:
    REPORT.update(status="FAILED", error=traceback.format_exc())
    unreal.log_error(REPORT["error"])
finally:
    RUN.mkdir(parents=True, exist_ok=True)
    result = json.dumps(REPORT, ensure_ascii=False, indent=2)
    (RUN / "report.json").write_text(result, encoding="utf-8")
    (ROOT / "Saved/NearbyShortcut/latest-result.json").write_text(result, encoding="utf-8")
    unreal.log("FF_BUILD_NEARBY_SHORTCUT_" + REPORT["status"] + ": " + str(RUN))
if REPORT["status"] != "PASS":
    raise RuntimeError("Nearby shortcut migration failed; inspect latest-result.json and retained backup")
