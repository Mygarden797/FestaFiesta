"""Add and verify SHIFT/ALT keycaps in the saved BuildTools guide.

Run with UnrealEditor-Cmd -ExecutePythonScript in an idle Entry editor.
The native command performs the narrow migration; this script backs up the WBP,
checks existing widgets and UI assets, and verifies that repeating it saves no
changes. Backups and reports are retained in Saved/CharacterShortcuts.
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
OUT = ROOT / "Saved/CharacterShortcuts"
RUN = OUT / datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S-%f")
ADDRESSES = (ROOT / "Source/FestaFiesta/Public/Building/Presentation/FF_BuildUIAssetPaths.h").read_text(encoding="utf-8-sig")
PACKAGE_MATCH = re.search(r'BuildToolsPackage\s*=\s*TEXT\("([^\"]+)"\)', ADDRESSES)
assert PACKAGE_MATCH, "BuildToolsPackage must be declared in FF_BuildUIAssetPaths.h"
PACKAGE = PACKAGE_MATCH.group(1)
TARGET = ROOT / "Content" / (PACKAGE.removeprefix("/Game/") + ".uasset")
COMMAND = "FF.ApplyBuildCharacterShortcuts"
VERSION = "FF.BuildCharacterShortcuts.20261013.v1"
COLUMN = "Column_BuildIconHelp"
ROWS = (
    ("ShiftLock", "ShiftLock", "SHIFT", "캐릭터 시점 고정"),
    ("Sprint", "SprintAlt", "ALT", "달리기"),
)
NEW_ROWS = {"Row_BuildHelp" + stem for stem, _, _, _ in ROWS}
NEW_WIDGETS = {
    name
    for stem, key, _, _ in ROWS
    for name in (
        "Row_BuildHelp" + stem, "Size_BuildHelp" + stem + "Icon",
        "Row_BuildHelp" + stem + "Keys", "Text_BuildHelp" + stem,
        *[kind + "_BuildHelpKey_" + key for kind in ("Size", "Border", "Face", "Text")],
    )
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
    return widget.get_editor_property("font").export_text()


def slot_presentation(widget):
    slot = widget.get_editor_property("slot")
    if not slot:
        return None
    kind = slot.get_class().get_name()
    properties = {
        # UCanvasPanelSlot exposes LayoutData; GetLayout is its getter name.
        "CanvasPanelSlot": ("layout_data", "auto_size", "z_order"),
        "HorizontalBoxSlot": ("padding", "size", "horizontal_alignment", "vertical_alignment"),
        "VerticalBoxSlot": ("padding", "size", "horizontal_alignment", "vertical_alignment"),
        "OverlaySlot": ("padding", "horizontal_alignment", "vertical_alignment"),
        "SizeBoxSlot": ("padding", "horizontal_alignment", "vertical_alignment"),
        "BorderSlot": ("padding", "horizontal_alignment", "vertical_alignment"),
        "ButtonSlot": ("padding", "horizontal_alignment", "vertical_alignment"),
    }.get(kind, ())
    result = {"class": kind}
    for name in properties:
        value = slot.get_editor_property(name)
        result[name] = value.export_text() if hasattr(value, "export_text") else str(value)
    return result


def presentation(widget):
    state = {"visibility": str(widget.get_visibility()), "slot": slot_presentation(widget)}
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
    column = widget_map[COLUMN]
    state = {"widgets": topology(widget_map),
             "version": unreal.EditorAssetLibrary.get_metadata_tag(blueprint, VERSION),
             "presentation": {name: presentation(widget) for name, widget in widget_map.items()},
             "columnChildren": [column.get_child_at(index).get_name()
                                for index in range(column.get_children_count())]}
    return blueprint, widget_map, state


def verify(state):
    assert state["version"] == "1", "C++ migration did not finish successfully"
    tree, appearance = state["widgets"], state["presentation"]
    assert NEW_WIDGETS <= tree.keys(), "Character shortcut branches are incomplete"
    children = state["columnChildren"]
    start = children.index("Row_BuildHelpViewToggle") + 1
    assert children[start:start + 3] == [
        "Row_BuildHelpShiftLock", "Row_BuildHelpSprint", "Button_BuildHelpUndo"
    ], "SHIFT and ALT rows must follow TAB and immediately precede Ctrl+Z"
    for stem, key, key_text, caption_text in ROWS:
        row = "Row_BuildHelp" + stem
        lane = "Size_BuildHelp" + stem + "Icon"
        keys = "Row_BuildHelp" + stem + "Keys"
        caption = "Text_BuildHelp" + stem
        branch = ["Size_BuildHelpKey_" + key, "Border_BuildHelpKey_" + key,
                  "Face_BuildHelpKey_" + key, "Text_BuildHelpKey_" + key]
        assert tree[row]["parent"] == COLUMN, "Shortcut row must use the existing guide column: " + row
        assert appearance[row]["visibility"] == str(unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE), "Shortcut row must be visible: " + row
        assert tree[row]["children"] == [lane, caption], "Unexpected row content: " + row
        for parent, child in zip([lane, keys, *branch[:-1]], [keys, *branch]):
            assert tree[parent]["children"] == [child], "Unexpected keycap content: " + parent
        assert appearance[lane]["width_override"] == 124.0 and appearance[lane]["height_override"] == 30.0, "Key lane must match the guide: " + lane
        assert appearance[lane]["override_width_override"] and appearance[lane]["override_height_override"], "Key lane overrides must be enabled: " + lane
        assert appearance[branch[0]]["height_override"] == 28.0 and appearance[branch[0]]["override_height_override"], "Keycap must be 28 pixels tall: " + key
        assert appearance[branch[-1]]["text"] == key_text, "Physical key label does not match: " + key
        assert appearance[caption]["text"] == caption_text, "Action caption does not match: " + caption
        assert appearance[caption]["font"] == appearance["Text_BuildHelpUndo"]["font"], "Action caption font must match the saved guide: " + caption
        for kind in ("Size", "Border", "Face", "Text"):
            actual = dict(appearance[kind + "_BuildHelpKey_" + key])
            template = dict(appearance[kind + "_BuildHelpKey_UndoCtrl"])
            if kind == "Text":
                actual.pop("text")
                template.pop("text")
            assert actual == template, "Keycap must match the Ctrl+Z design: " + key + "/" + kind


def verify_preserved(before, after, original_widgets, current_widgets, original_slots):
    for name, original in before["widgets"].items():
        current = after["widgets"].get(name)
        assert current, "Existing widget was removed: " + name
        assert current_widgets[name] == original_widgets[name], "Existing widget identity changed: " + name
        assert current_widgets[name].get_editor_property("slot") == original_slots[name], "Existing slot identity changed: " + name
        if name == COLUMN:
            original, current = dict(original), dict(current)
            original["children"] = [child for child in original["children"] if child not in NEW_ROWS]
            current["children"] = [child for child in current["children"] if child not in NEW_ROWS]
        assert current == original, "Existing widget topology changed: " + name
        original_appearance = dict(before["presentation"][name])
        current_appearance = dict(after["presentation"][name])
        if name in {"Text_BuildHelpShiftLock", "Text_BuildHelpSprint"}:
            # The command can also update earlier captions while preserving their styling.
            original_appearance.pop("text")
            current_appearance.pop("text")
        assert current_appearance == original_appearance, "Existing widget or slot presentation changed: " + name
    assert after["widgets"].keys() - before["widgets"].keys() <= NEW_WIDGETS, "Unexpected widgets were added"


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
    verify_preserved(before, after, original_widgets, current_widgets, original_slots)
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
    REPORT.update(status="PASS", afterSha256=digest(TARGET),
                  keycapsMatchCtrlZ=True, preservedWidgets=len(original_widgets),
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
    unreal.log("FF_BUILD_CHARACTER_SHORTCUTS_" + REPORT["status"] + ": " + str(RUN))
if REPORT["status"] != "PASS":
    raise RuntimeError("Character shortcut migration failed; inspect latest-result.json and retained backup")
