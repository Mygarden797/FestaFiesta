"""Back up and finalize the current editable WBP trees; preserve all unrelated UI."""
from datetime import datetime
import hashlib
import json
from pathlib import Path
import shutil
import traceback

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Saved/BuildModeClarity/designer-ui"
RUN = OUT / datetime.now().strftime("%Y%m%d-%H%M%S-%f")
BASE = "/Game/FestaFiesta/Building/FF_WBP_Building/"
GROUPS = (
    ("FF.ApplyBuildCatalogDesigner", "FF.BuildCatalogDesigner.20260929.v3", (
        "Catalog/WBP_FF_PrepPanelBuild", "Catalog/WBP_FF_PrepBuildTile", "Controls/WBP_FF_BuildCategoryChoice")),
    ("FF.FinalizeBuildToolsInfoDesigner", "FF.ToolsInfoDesigner.20260929.v3", (
        "Controls/WBP_FF_BuildTools", "Facility/WBP_FF_FacilityInfoPanel")),
    ("FF.ApplyCodexNavigation", "FF.Codex.NavigationRewards.20260929.v4", (
        "FacilityCodex/WBP_FF_FacilityCodexScreen", "FacilityCodex/WBP_FF_FacilityCodexCard",
        "FacilityCodex/Controls/WBP_FF_CodexCollectionReward", "FacilityCodex/WBP_FF_FacilityCodexCatalogLauncher")),
)
PACKAGES = [BASE + suffix for _, _, suffixes in GROUPS for suffix in suffixes]
REPORT = {"status": "RUNNING", "packages": PACKAGES, "commands": [group[0] for group in GROUPS],
          "backup": str(RUN / "before_assets")}


def path_for(package):
    return ROOT / "Content" / (package.removeprefix("/Game/") + ".uasset")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def ui_hashes():
    return {p.relative_to(ROOT).as_posix(): digest(p)
            for folder in (ROOT / "Content/FestaFiesta/UI", ROOT / "Content/FestaFiesta/Building/FF_WBP_Building")
            for p in folder.rglob("*.uasset")}


def widget_names(package):
    blueprint = unreal.load_asset(package)
    return sorted(w.get_name() for w in unreal.ObjectIterator(unreal.Widget)
                  if w.get_outer() and w.get_outer().get_class().get_name() == "WidgetTree"
                  and w.get_outer().get_outer() == blueprint)


def verify():
    for _, version, suffixes in GROUPS:
        for suffix in suffixes:
            package = BASE + suffix
            bp = unreal.load_asset(package)
            assert bp and unreal.EditorAssetLibrary.load_blueprint_class(package), "Missing WBP/class: " + package
            assert unreal.EditorAssetLibrary.get_metadata_tag(bp, version) == "1", "Incomplete migration: " + package
    return {package: digest(path_for(package)) for package in PACKAGES}


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world and world.get_path_name().startswith("/Engine/Maps/Entry."), "Requires idle Entry map"
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Stop PIE first"
    before = ui_hashes()
    REPORT["all_ui_before"] = before
    for package in PACKAGES:
        source = path_for(package)
        backup = RUN / "before_assets" / source.relative_to(ROOT)
        backup.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, backup)
    REPORT["widgets_before"] = {package: widget_names(package) for package in PACKAGES}
    for command, _, _ in GROUPS:
        unreal.SystemLibrary.execute_console_command(world, command)
    REPORT["after"] = verify()
    REPORT["widgets_after"] = {package: widget_names(package) for package in PACKAGES}
    for command, _, _ in GROUPS:
        unreal.SystemLibrary.execute_console_command(world, command)
    assert REPORT["after"] == verify(), "Repeat application changed saved WBP assets"
    target_paths = {path_for(p).relative_to(ROOT).as_posix() for p in PACKAGES}
    after = ui_hashes()
    unexpected = [p for p in before.keys() | after.keys()
                  if p not in target_paths and before.get(p) != after.get(p)]
    assert not unexpected, "Unrelated UI changed: " + repr(unexpected)
    REPORT["removed_widgets"] = {p: sorted(set(REPORT["widgets_before"][p]) - set(REPORT["widgets_after"][p])) for p in PACKAGES}
    REPORT.update(status="PASS", idempotent=True, unrelated_ui_unchanged=True)


try:
    main()
except Exception:
    REPORT.update(status="FAILED", error=traceback.format_exc())
    unreal.log_error(REPORT["error"])
finally:
    RUN.mkdir(parents=True, exist_ok=True)
    serialized = json.dumps(REPORT, ensure_ascii=False, indent=2)
    (RUN / "report.json").write_text(serialized, encoding="utf-8")
    (OUT / "latest-result.json").write_text(serialized, encoding="utf-8")
    unreal.log("FF_DESIGNER_UI_RESULT: " + REPORT["status"])
if REPORT["status"] != "PASS":
    raise RuntimeError("Designer migration failed; read latest-result.json")
