"""Apply the staff navigation and catalog lighting UI after an Editor build.

Run in the idle Entry editor with -ExecutePythonScript and -RenderOffscreen.
Backups and verification results are kept in Saved/StaffCatalogUX.
"""

from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import runpy
import shutil
import traceback

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Saved/StaffCatalogUX"
RUN = OUT / datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S-%f")
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
    for package in (INSPECTOR, CHECK):
        path = asset_file(package)
        shutil.copy2(path, RUN / path.name)
    commands = ("FF.MigrateFacilityStaffTab", "FF.MigratePrepCheckStaffLinks")
    for command in commands:
        unreal.SystemLibrary.execute_console_command(world, command)
    inspector = widgets(INSPECTOR)
    REPORT["navigation"] = {name: {"class": widget.get_class().get_name(),
                                  "parent": widget.get_parent().get_name() if widget.get_parent() else None,
                                  "parent_class": widget.get_parent().get_class().get_name() if widget.get_parent() else None}
                            for name, widget in inspector.items()
                            if name.startswith("Button_Tab") or name in ("Clarity_SecondaryTabs", "Panel_InspectorTools", "Panel_Tabs", "Panel_Navigation")}
    tabs = inspector["Button_TabStaff"].get_parent()
    ancestry = []
    ancestor = tabs
    while ancestor:
        ancestry.append([ancestor.get_name(), ancestor.get_class().get_name()])
        ancestor = ancestor.get_parent()
    REPORT["tab_ancestors"] = ancestry
    tab_names = [tabs.get_child_at(i).get_name() for i in range(tabs.get_children_count())]
    REPORT["tab_names"] = tab_names
    assert tab_names == ["Button_TabOverview", "Button_TabSynergy", "Button_TabOperation", "Button_TabStaff"], str(tab_names)
    assert inspector["Panel_StaffSection"].get_parent() == inspector["Panel_SynergySection"].get_parent()
    check = widgets(CHECK)
    assert check["Text_RequiredFacilities"].get_parent() == check["Button_RequiredFacilities"]
    reason_row = check["HorizontalBox_OpenGateReason"]
    assert check["Text_OpenGateReason"].get_parent() == reason_row
    assert check["Button_OpenGateReason"].get_parent() == reason_row
    assert reason_row.get_child_at(0) == check["Text_OpenGateReason"]
    assert reason_row.get_child_at(1) == check["Button_OpenGateReason"]
    assert check["Button_OpenGateReason"].get_content() == check["Text_OpenGateStaffLink"]
    assert str(check["Text_OpenGateStaffLink"].get_text()) == "스태프 배정 →"
    default = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(CHECK))
    assert str(default.get_editor_property("required_facility_format")) == "{Name} · {Required}개 설치 · {State}"
    first_hashes = {package: digest(asset_file(package)) for package in (INSPECTOR, CHECK)}
    for command in commands:
        unreal.SystemLibrary.execute_console_command(world, command)
    assert first_hashes == {package: digest(asset_file(package)) for package in first_hashes}, "Migration is not idempotent"
    runpy.run_path(str(ROOT / "Content/FestaFiesta/Building/FF_WBP_Building/WebUI/sync_to_umg.py"), run_name="__main__")
    sync = json.loads((ROOT / "Saved/BuildingWebUI20261008/latest-sync.json").read_text(encoding="utf-8"))
    assert sync["status"] == "PASS", "Catalog HTML synchronization failed"
    REPORT.update(status="PASS", migrations=list(commands), idempotent=True,
                  staff_tabs=4, check_links=2, catalog_sync=sync["status"], backup=str(RUN))


try:
    main()
except Exception:
    REPORT.update(status="FAIL", error=traceback.format_exc())
    unreal.log_error(REPORT["error"])
finally:
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "latest-result.json").write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("FF_STAFF_CATALOG_UX_RESULT: " + REPORT["status"])
if REPORT["status"] != "PASS":
    raise RuntimeError("Staff/catalog UX application failed; see Saved/StaffCatalogUX/latest-result.json")
