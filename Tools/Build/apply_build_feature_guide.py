"""Migrate the authored toolbar guide and staffing shortcuts in an idle Entry editor.

Preserves the current assets in Saved/BuildFeatureGuide before updating them and
checks the exact widget contract plus no-op behavior on a second migration.
"""

from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil
import traceback

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Saved/BuildFeatureGuide"
RUN = OUT / datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S-%f")
TOOLS = "/Game/FestaFiesta/Building/FF_WBP_Building/Controls/WBP_FF_BuildTools"
INSPECTOR = "/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityInfoPanel"
CHECK = "/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelCheck"
REPORT = {"status": "RUNNING"}


def widgets(package):
    blueprint = unreal.load_asset(package)
    assert blueprint, package
    return {widget.get_name(): widget for widget in unreal.ObjectIterator(unreal.Widget)
            if widget.get_outer() and widget.get_outer().get_class().get_name() == "WidgetTree"
            and widget.get_outer().get_outer() == blueprint}


def asset_file(package):
    return ROOT / "Content" / (package.removeprefix("/Game/") + ".uasset")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world and world.get_path_name().startswith("/Engine/Maps/Entry."), "Requires idle Entry map"
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Stop PIE first"
    RUN.mkdir(parents=True, exist_ok=True)
    packages = (TOOLS, INSPECTOR, CHECK)
    for package in packages:
        shutil.copy2(asset_file(package), RUN / asset_file(package).name)
    commands = ("FF.MigrateFacilityStaffTab", "FF.MigratePrepCheckStaffLinks", "FF.MigrateBuildFeatureGuide")
    for command in commands:
        unreal.SystemLibrary.execute_console_command(world, command)

    toolbar = widgets(TOOLS)
    for name, expected_class in {
        "Button_FeatureGuide": unreal.Button,
        "Panel_FeatureGuide": unreal.Border,
        "Combo_FeatureGuideTopic": unreal.ComboBoxString,
        "Text_FeatureGuideTitle": unreal.TextBlock,
        "Text_FeatureGuideBody": unreal.TextBlock,
        "Text_FeatureGuideHint": unreal.TextBlock,
        "Button_FeatureGuideConfirm": unreal.Button,
        "Button_FeatureGuideClose": unreal.Button,
    }.items():
        assert isinstance(toolbar.get(name), expected_class), name
    row = toolbar["Row_BuildReferenceTopBar"]
    row_names = [row.get_child_at(i).get_name() for i in range(row.get_children_count())]
    help_index = row_names.index("Size_Button_ToolbarHelp")
    guide_entry = toolbar["Button_FeatureGuide"]
    while guide_entry.get_parent() != row:
        guide_entry = guide_entry.get_parent()
        assert guide_entry, "Guide button must belong to the top toolbar"
    assert row_names[help_index + 1] == guide_entry.get_name(), row_names
    assert toolbar["Panel_FeatureGuide"].get_visibility() == unreal.SlateVisibility.COLLAPSED

    check = widgets(CHECK)
    reason_row = check["HorizontalBox_OpenGateReason"]
    assert reason_row.get_child_at(0) == check["Text_OpenGateReason"]
    assert reason_row.get_child_at(1) == check["Button_OpenGateReason"]
    assert str(check["Text_OpenGateStaffLink"].get_text()) == "스태프 배정 →"
    inspector = widgets(INSPECTOR)
    tabs = inspector["Button_TabStaff"].get_parent()
    tab_names = [tabs.get_child_at(i).get_name() for i in range(tabs.get_children_count())]
    assert tab_names == ["Button_TabOverview", "Button_TabSynergy", "Button_TabOperation", "Button_TabStaff"]

    first_hashes = {package: digest(asset_file(package)) for package in packages}
    for command in commands:
        unreal.SystemLibrary.execute_console_command(world, command)
    assert first_hashes == {package: digest(asset_file(package)) for package in packages}, "Migration is not idempotent"
    REPORT.update(status="PASS", migrations=list(commands), idempotent=True,
                  toolbar=row_names, inspector_tabs=tab_names, hashes=first_hashes, backup=str(RUN))


try:
    main()
except Exception:
    REPORT.update(status="FAIL", error=traceback.format_exc())
    unreal.log_error(REPORT["error"])
finally:
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "latest-result.json").write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("FF_BUILD_FEATURE_GUIDE_RESULT: " + REPORT["status"])
if REPORT["status"] != "PASS":
    raise RuntimeError("Build feature guide migration failed; see Saved/BuildFeatureGuide/latest-result.json")
