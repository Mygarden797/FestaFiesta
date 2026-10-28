"""Apply the TAB keycap to the saved BuildTools guide in an idle Entry editor.

Run with UnrealEditor-Cmd -ExecutePythonScript using this script's absolute path.
Back up the active BuildTools WBP, check its existing widgets and other UI assets,
and verify the new keycap matches Ctrl+Z and a repeat command leaves bytes intact.
Reports and the backup are retained in Saved/ViewToggleShortcut.
"""

from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import shutil
import traceback

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Saved/ViewToggleShortcut"
RUN = OUT / datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S-%f")
ADDRESSES = (ROOT / "Source/FestaFiesta/Public/Building/Presentation/FF_BuildUIAssetPaths.h").read_text(encoding="utf-8-sig")
PACKAGE = re.search(r'BuildToolsPackage\s*=\s*TEXT\("([^\"]+)"\)', ADDRESSES).group(1)
TARGET = ROOT / "Content" / (PACKAGE.removeprefix("/Game/") + ".uasset")
COMMAND = "FF.ApplyBuildViewToggleShortcut"
VERSION = "FF.BuildViewToggleShortcut.20261013.v1"
ROW = "Row_BuildHelpViewToggle"
HINT = "Text_BuildHelpKeyboardHint"
NEW_WIDGETS = {
    ROW, "Size_BuildHelpViewToggleIcon", "Row_BuildHelpViewToggleKeys",
    "Size_BuildHelpKey_Tab", "Border_BuildHelpKey_Tab", "Face_BuildHelpKey_Tab",
    "Text_BuildHelpKey_Tab", "Text_BuildHelpViewToggle",
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


def text_font(widget):
    font = widget.get_editor_property("font")
    return {"size": font.size, "typeface": str(font.typeface_font_name),
            "fontObject": font.font_object.get_path_name() if font.font_object else None,
            "fullFont": font.export_text()}


def presentation(widget):
    state = {"visibility": str(widget.get_visibility())}
    if isinstance(widget, unreal.TextBlock):
        state.update(text=str(widget.get_text()), font=text_font(widget),
                     color=widget.get_editor_property("color_and_opacity").export_text(),
                     autoWrap=widget.get_editor_property("auto_wrap_text"))
    elif isinstance(widget, unreal.Border):
        state.update(brush=widget.get_editor_property("background").export_text(),
                     padding=widget.get_editor_property("padding").export_text(),
                     horizontalAlignment=str(widget.get_editor_property("horizontal_alignment")),
                     verticalAlignment=str(widget.get_editor_property("vertical_alignment")))
    elif isinstance(widget, unreal.SizeBox):
        state.update({name: widget.get_editor_property(name) for name in (
            "width_override", "height_override", "min_desired_width", "min_desired_height",
            "max_desired_width", "max_desired_height", "override_width_override", "override_height_override",
            "override_min_desired_width", "override_min_desired_height",
            "override_max_desired_width", "override_max_desired_height")})
    elif isinstance(widget, unreal.Button):
        state.update(style=widget.get_editor_property("widget_style").export_text(),
                     tooltip=str(widget.get_editor_property("tool_tip_text")))
    return state


def inspect():
    blueprint = unreal.load_asset(PACKAGE)
    assert blueprint, "Active BuildTools WBP must exist"
    widget_map = widgets(blueprint)
    column = widget_map["Column_BuildIconHelp"]
    state = {"widgets": topology(widget_map),
             "version": unreal.EditorAssetLibrary.get_metadata_tag(blueprint, VERSION),
             "presentation": {name: presentation(widget) for name, widget in widget_map.items()},
             "columnChildren": [column.get_child_at(index).get_name()
                                for index in range(column.get_children_count())]}
    return blueprint, widget_map, state


def verify(state):
    assert state["version"] == "1", "C++ migration did not finish successfully"
    assert NEW_WIDGETS <= state["widgets"].keys(), "View-toggle keycap branch is incomplete"
    tree = state["widgets"]
    appearance = state["presentation"]
    children = state["columnChildren"]
    assert tree[ROW]["parent"] == "Column_BuildIconHelp", "TAB row must use the existing guide column"
    assert children.index(ROW) == children.index(HINT) + 1, "TAB row must immediately follow the guide hint"
    following = children[children.index(ROW) + 1:children.index("Button_BuildHelpUndo")]
    assert following in ([], ["Row_BuildHelpShiftLock", "Row_BuildHelpSprint"]), "TAB and character shortcuts must precede Ctrl+Z"
    assert appearance[ROW]["visibility"] == str(unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE), "TAB row must be visible"
    # bHiddenInDesigner is not reflected to Python; the new native row uses its visible default.
    assert tree[ROW]["children"] == ["Size_BuildHelpViewToggleIcon", "Text_BuildHelpViewToggle"]
    assert tree["Size_BuildHelpViewToggleIcon"]["children"] == ["Row_BuildHelpViewToggleKeys"]
    assert tree["Row_BuildHelpViewToggleKeys"]["children"] == ["Size_BuildHelpKey_Tab"]
    assert tree["Size_BuildHelpKey_Tab"]["children"] == ["Border_BuildHelpKey_Tab"]
    assert tree["Border_BuildHelpKey_Tab"]["children"] == ["Face_BuildHelpKey_Tab"]
    assert tree["Face_BuildHelpKey_Tab"]["children"] == ["Text_BuildHelpKey_Tab"]
    lane = appearance["Size_BuildHelpViewToggleIcon"]
    assert lane["width_override"] == 124.0 and lane["height_override"] == 30.0, "TAB key lane must match the guide"
    key = appearance["Text_BuildHelpKey_Tab"]
    assert key["text"] == "TAB" and key["font"]["size"] == 14, "Physical TAB keycap is missing"
    caption = appearance["Text_BuildHelpViewToggle"]
    assert caption["text"] == "시점 전환", "View-toggle action caption does not match"
    assert caption["font"] == appearance["Text_BuildHelpUndo"]["font"], "Action captions must share the saved guide font"
    assert appearance[HINT]["text"] == "표시된 키를 누르면 실행됩니다", "The hint must not duplicate the TAB shortcut"
    for kind in ("Size", "Border", "Face", "Text"):
        actual = dict(appearance[f"{kind}_BuildHelpKey_Tab"])
        template = dict(appearance[f"{kind}_BuildHelpKey_UndoCtrl"])
        if kind == "Text":
            actual.pop("text")
            template.pop("text")
        assert actual == template, "TAB keycap must match the Ctrl+Z keycap design: " + kind


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world and world.get_path_name().startswith("/Engine/Maps/Entry."), "Requires idle Entry editor"
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Stop PIE first"
    before_hashes = ui_hashes()
    _, original_widgets, before = inspect()
    original_slots = {name: widget.get_editor_property("slot") for name, widget in original_widgets.items()}
    REPORT.update(before=before, beforeSha256=digest(TARGET))
    backup = RUN / "Backups" / TARGET.relative_to(ROOT)
    backup.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(TARGET, backup)
    assert digest(backup) == REPORT["beforeSha256"], "Backup bytes differ from the original WBP"
    REPORT["backup"] = str(backup)

    unreal.SystemLibrary.execute_console_command(world, COMMAND)
    _, current_widgets, after = inspect()
    REPORT["after"] = after
    verify(after)
    assert unreal.EditorAssetLibrary.load_blueprint_class(PACKAGE), "Migrated WBP has no generated class"
    for name, original in before["widgets"].items():
        current = after["widgets"].get(name)
        assert current, "Existing widget was removed: " + name
        assert current_widgets[name] == original_widgets[name], "Existing widget identity changed: " + name
        assert current_widgets[name].get_editor_property("slot") == original_slots[name], "Existing slot changed: " + name
        if name == "Column_BuildIconHelp":
            current = dict(current)
            current["children"] = [child for child in current["children"] if child != ROW]
            original = dict(original)
            original["children"] = [child for child in original["children"] if child != ROW]
        assert current == original, "Existing widget topology changed: " + name
        current_appearance = dict(after["presentation"][name])
        original_appearance = dict(before["presentation"][name])
        if name == HINT:
            current_appearance.pop("text")
            original_appearance.pop("text")
        assert current_appearance == original_appearance, "Existing widget presentation changed: " + name
    assert after["widgets"].keys() - before["widgets"].keys() <= NEW_WIDGETS, "Unexpected widgets were added"

    after_hashes = ui_hashes()
    target_path = TARGET.relative_to(ROOT).as_posix()
    unexpected = [path for path in before_hashes.keys() | after_hashes.keys()
                  if path != target_path and before_hashes.get(path) != after_hashes.get(path)]
    assert not unexpected, "Unrelated UI assets changed: " + repr(unexpected)

    unreal.SystemLibrary.execute_console_command(world, COMMAND)
    _, _, repeated = inspect()
    verify(repeated)
    assert repeated == after, "Repeated migration changed the authored guide"
    assert ui_hashes() == after_hashes, "Repeated migration changed saved bytes"
    REPORT.update(status="PASS", after=after, afterSha256=digest(TARGET),
                  keycapMatchesCtrlZ=True, preservedWidgets=len(original_widgets),
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
    (OUT / "latest-result.json").write_text(result, encoding="utf-8")
    unreal.log("FF_BUILD_VIEW_TOGGLE_SHORTCUT_" + REPORT["status"] + ": " + str(RUN))
if REPORT["status"] != "PASS":
    raise RuntimeError("View-toggle shortcut migration failed; inspect latest-result.json and retained backup")
