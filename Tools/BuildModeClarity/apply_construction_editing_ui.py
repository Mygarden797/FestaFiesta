"""Save the editable construction controls and verify repeat runs preserve the assets.

Run with a freshly built editor, /Engine/Maps/Entry and -ExecutePythonScript.
Only the three listed WBP assets may change; existing assets are backed up first.
"""
from datetime import datetime
import hashlib
import json
from pathlib import Path
import shutil
import traceback

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Saved/ConstructionEditingUI"
RUN = OUT / datetime.now().strftime("%Y%m%d-%H%M%S-%f")
FOLDER = "/Game/FestaFiesta/Building/FF_WBP_Building/Controls/"
TOOLS = FOLDER + "WBP_FF_BuildTools"
SELECTION = FOLDER + "WBP_FF_BuildMultiSelection"
CARD = FOLDER + "WBP_FF_BuildSelectedFacilityCard"
PACKAGES = (TOOLS, SELECTION, CARD)
VERSIONS = {
    TOOLS: "FF.BuildReferenceTools.QueueShortcutV11",
    SELECTION: "FF.MultiSelection.EditableWorkspaceVersion",
}
COMMANDS = ("FF.ApplyBuildReferenceTools", "FF.UpdateBuildMultiSelectionWidget")
UI_ROOTS = (ROOT / "Content/FestaFiesta/UI", ROOT / "Content/FestaFiesta/Building/FF_WBP_Building")
REPORT = {"status": "RUNNING", "commands": COMMANDS, "before": {}, "after": {},
          "backup": str(RUN / "before_assets"), "idempotent": False}


def asset_path(package):
    return ROOT / "Content" / (package.removeprefix("/Game/") + ".uasset")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def ui_hashes():
    return {p.relative_to(ROOT).as_posix(): digest(p)
            for folder in UI_ROOTS for p in sorted(folder.rglob("*.uasset"))}


def verify_assets():
    for package in PACKAGES:
        bp = unreal.load_asset(package)
        assert bp and unreal.EditorAssetLibrary.load_blueprint_class(package), "Missing WBP/class: " + package
        if package in VERSIONS:
            key = VERSIONS[package]
            assert unreal.EditorAssetLibrary.get_metadata_tag(bp, key) == "1", "Missing migration marker: " + key
    return {package: digest(asset_path(package)) for package in PACKAGES}


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world and world.get_path_name().startswith("/Engine/Maps/Entry."), "Use an idle Entry world"
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Stop PIE first"
    before = ui_hashes()
    targets = {asset_path(p).relative_to(ROOT).as_posix() for p in PACKAGES}
    protected = {p: h for p, h in before.items() if p not in targets}
    REPORT["protected_ui_count"] = len(protected)
    for package in PACKAGES:
        source = asset_path(package)
        if not source.is_file():
            assert package == CARD, "Missing existing production WBP: " + package
            REPORT["before"][package] = None
            continue
        REPORT["before"][package] = digest(source)
        backup = RUN / "before_assets" / source.relative_to(ROOT)
        backup.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, backup)

    for command in COMMANDS:
        unreal.SystemLibrary.execute_console_command(world, command)
    REPORT["after"] = verify_assets()
    for command in COMMANDS:
        unreal.SystemLibrary.execute_console_command(world, command)
    REPORT["repeat"] = verify_assets()
    assert REPORT["after"] == REPORT["repeat"], "Second run changed saved Designer assets"
    REPORT["idempotent"] = True
    after = ui_hashes()
    current_protected = {p: h for p, h in after.items() if p not in targets}
    REPORT["unexpected_ui_changes"] = sorted(p for p in protected.keys() | current_protected.keys()
                                              if protected.get(p) != current_protected.get(p))
    assert not REPORT["unexpected_ui_changes"], "Unrelated UI changed: " + str(REPORT["unexpected_ui_changes"])
    REPORT["status"] = "PASS"


try:
    main()
except Exception:
    REPORT["status"] = "FAIL"
    REPORT["error"] = traceback.format_exc()
    unreal.log_error("FF_CONSTRUCTION_UI_APPLY_FAIL: " + REPORT["error"])
    raise
finally:
    RUN.mkdir(parents=True, exist_ok=True)
    serialized = json.dumps(REPORT, ensure_ascii=False, indent=2)
    (RUN / "report.json").write_text(serialized, encoding="utf-8")
    (OUT / "latest-result.json").write_text(serialized, encoding="utf-8")

unreal.log("FF_CONSTRUCTION_UI_APPLY_PASS: " + str(RUN / "report.json"))
