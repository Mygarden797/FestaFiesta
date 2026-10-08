"""Expose required placement space in the one active BuildTools WBP.

Run with a freshly compiled UnrealEditor-Cmd -ExecutePythonScript in Entry.
The original asset is backed up before the migration. Unrelated UI bytes,
existing widget identities and repeated-run idempotence are checked afterwards.
Reports and the backup are retained in Saved/PlacementSpace.
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
RUN = ROOT / "Saved/PlacementSpace" / datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S-%f")
ADDRESSES = (ROOT / "Source/FestaFiesta/Public/Building/Presentation/FF_BuildUIAssetPaths.h").read_text(encoding="utf-8-sig")
PACKAGE = re.search(r'BuildToolsPackage\s*=\s*TEXT\("([^\"]+)"\)', ADDRESSES).group(1)
TARGET = ROOT / "Content" / (PACKAGE.removeprefix("/Game/") + ".uasset")
COMMAND = "FF.ApplyBuildPlacementSpace"
VERSION = "FF.BuildPlacementSpace.20261007.v1"
NEW_WIDGETS = {
    "Border_PlacementSpace", "Column_PlacementSpace", "Text_PlacementSpaceTitle",
    "Row_PlacementSpaceLegend", "Text_Audience", "Row_PlacementBoundaryLegend",
    *(f"{kind}_PlacementSpaceLegend{suffix}" for kind in ("Border", "Text")
      for suffix in ("Facility", "Queue", "Audience", "Boundary", "Blocked")),
}
CHANGED_TOPOLOGY = {
    "Column_ClaritySummary", "Panel_PlacementFacts", "Text_Balance",
    "Text_Footprint", "Text_Queue", "Text_RangeLegend",
    "Column_PlacementSpace", "Row_PlacementSpaceLegend", "Border_PlacementSpaceLegendBlocked",
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


def text_state(widget):
    font = widget.get_editor_property("font")
    return {"text": str(widget.get_text()), "fontSize": font.size,
            "fontObject": font.font_object.get_path_name() if font.font_object else None,
            "typeface": str(font.typeface_font_name), "visibility": str(widget.get_visibility())}


def inspect():
    blueprint = unreal.load_asset(PACKAGE)
    assert blueprint, "Active BuildTools WBP must exist"
    widget_map = widgets(blueprint)
    state = {"widgets": topology(widget_map),
             "version": unreal.EditorAssetLibrary.get_metadata_tag(blueprint, VERSION),
             "texts": {name: text_state(widget_map[name]) for name in (
                 "Text_Footprint", "Text_Queue", "Text_Audience", "Text_RangeLegend", "Text_Balance", "Label_Facts")
                       if name in widget_map},
             "factsVisibility": str(widget_map["Panel_PlacementFacts"].get_visibility()),
             "thumbnailWidth": widget_map["Size_PlacementFacilityThumb"].get_editor_property("width_override"),
             "thumbnailHeight": widget_map["Size_PlacementFacilityThumb"].get_editor_property("height_override"),
             "buttons": {name: {"class": widget.get_class().get_name(),
                                "content": widget.get_content().get_name() if widget.get_content() else None}
                         for name, widget in widget_map.items() if isinstance(widget, unreal.Button)}}
    if "Border_PlacementSpace" in widget_map:
        state.update(spaceVisibility=str(widget_map["Border_PlacementSpace"].get_visibility()),
                     title=text_state(widget_map["Text_PlacementSpaceTitle"]),
                     legends={suffix: text_state(widget_map["Text_PlacementSpaceLegend" + suffix])
                              for suffix in ("Facility", "Queue", "Audience", "Boundary", "Blocked")
                              if "Text_PlacementSpaceLegend" + suffix in widget_map},
                     legacyClearanceVisibility={name: str(widget_map[name].get_visibility())
                                                for name in ("Border_PlacementSpaceLegendClearance",
                                                             "Text_PlacementSpaceLegendClearance")
                                                if name in widget_map})
    return blueprint, widget_map, state


def verify(state):
    assert state["version"] == "2", "C++ migration did not finish successfully"
    assert NEW_WIDGETS <= state["widgets"].keys(), "Required space branch is incomplete"
    tree = state["widgets"]
    assert tree["Border_PlacementSpace"]["parent"] == "Column_ClaritySummary", "Space belongs inside the existing inspector"
    assert tree["Border_PlacementSpace"]["children"] == ["Column_PlacementSpace"]
    assert tree["Column_PlacementSpace"]["children"] == [
        "Text_PlacementSpaceTitle", "Text_Footprint", "Text_Queue", "Text_Audience",
        "Row_PlacementSpaceLegend", "Row_PlacementBoundaryLegend", "Text_RangeLegend"]
    assert tree["Panel_PlacementFacts"]["children"] == ["Text_Balance"], "Only cost details remain folded"
    assert tree["Text_Balance"]["parent"] == "Panel_PlacementFacts"
    children = tree["Column_ClaritySummary"]["children"]
    assert children.index("Border_PlacementSpace") == children.index("Text_Legend") + 1
    assert state["spaceVisibility"] == str(unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
    assert state["factsVisibility"] == str(unreal.SlateVisibility.COLLAPSED)
    assert state["title"]["text"] == "필요한 공간" and state["title"]["fontSize"] == 16
    for name in ("Text_Footprint", "Text_Queue", "Text_Audience", "Text_RangeLegend"):
        assert tree[name]["parent"] == "Column_PlacementSpace", "Measurement remains hidden: " + name
        assert state["texts"][name]["visibility"] == str(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    assert state["texts"]["Text_Footprint"]["fontSize"] == 16
    assert state["texts"]["Text_Queue"]["fontSize"] == 16
    assert state["texts"]["Text_Audience"]["fontSize"] == 16
    assert "관객석 범위" in state["texts"]["Text_Audience"]["text"] and "정원" in state["texts"]["Text_Audience"]["text"]
    assert state["texts"]["Text_RangeLegend"]["text"] == "각 범위는 회전 전 가로 × 세로 · 단순 합산 아님"
    assert state["texts"]["Label_Facts"]["text"] == "비용 상세"
    assert {key: value["text"] for key, value in state["legends"].items()} == {
        "Facility": "시설", "Queue": "대기열", "Audience": "관객석", "Boundary": "건축 경계", "Blocked": "문제 영역"}
    assert tree["Border_PlacementSpaceLegendAudience"]["parent"] == "Row_PlacementSpaceLegend"
    assert tree["Row_PlacementBoundaryLegend"]["children"] == [
        "Border_PlacementSpaceLegendBoundary", "Border_PlacementSpaceLegendBlocked"]
    assert all(value == str(unreal.SlateVisibility.COLLAPSED)
               for value in state["legacyClearanceVisibility"].values()), "Retired clearance legend is visible"
    assert state["thumbnailWidth"] == 56.0 and state["thumbnailHeight"] == 48.0


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
    assert digest(backup) == REPORT["beforeSha256"], "Backup bytes differ from the original WBP"
    REPORT["backup"] = str(backup)

    unreal.SystemLibrary.execute_console_command(world, COMMAND)
    blueprint, _, after = inspect()
    verify(after)
    assert unreal.EditorAssetLibrary.load_blueprint_class(PACKAGE), "Migrated WBP has no generated class"
    for name, original in before["widgets"].items():
        current = after["widgets"].get(name)
        assert current, "Existing widget was removed: " + name
        assert current["class"] == original["class"], "Existing widget class changed: " + name
        if name not in CHANGED_TOPOLOGY:
            assert current == original, "Unrelated widget topology changed: " + name
    assert after["widgets"].keys() - before["widgets"].keys() <= NEW_WIDGETS, "Unexpected widgets were added"
    assert after["buttons"] == before["buttons"], "Existing action identity/content changed"
    for name in ("Text_Footprint", "Text_Queue", "Text_RangeLegend", "Text_Balance"):
        for key in ("fontObject", "typeface"):
            assert after["texts"][name][key] == before["texts"][name][key], "Bound text typography changed: " + name

    after_hashes = ui_hashes()
    target_path = TARGET.relative_to(ROOT).as_posix()
    unexpected = [path for path in before_hashes.keys() | after_hashes.keys()
                  if path != target_path and before_hashes.get(path) != after_hashes.get(path)]
    assert not unexpected, "Unrelated UI assets changed: " + repr(unexpected)

    unreal.SystemLibrary.execute_console_command(world, COMMAND)
    _, _, repeated = inspect()
    verify(repeated)
    assert repeated == after, "Repeated migration changed the authored widget state"
    assert ui_hashes() == after_hashes, "Repeated migration changed saved bytes"
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
    (ROOT / "Saved/PlacementSpace/latest-result.json").write_text(result, encoding="utf-8")
    unreal.log("FF_BUILD_PLACEMENT_SPACE_" + REPORT["status"] + ": " + str(RUN))
if REPORT["status"] != "PASS":
    raise RuntimeError("Placement space migration failed; inspect latest-result.json and retained backup")
