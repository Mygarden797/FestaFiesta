# 역할: 건축 도움말·시설 조합 화면 네 개를 백업한 뒤 수정하고, 재실행 결과와 다른 UI의 보존 여부를 확인한다.
"""Apply only the build help and facility combination fixes in an idle Entry editor.

Run with a freshly built UE editor and -ExecutePythonScript. Each run backs up the
four existing production WBPs and records isolation and second-run hash checks.
"""
from datetime import datetime
import hashlib
import json
from pathlib import Path
import shutil
import traceback

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Saved/BuildModeClarity/ui-fixes"
RUN = OUT / datetime.now().strftime("%Y%m%d-%H%M%S-%f")
BACKUP = RUN / "before_assets"
BASE_PACKAGE = "/Game/FestaFiesta/Building/FF_WBP_Building/"
PACKAGES = (
    BASE_PACKAGE + "Controls/WBP_FF_BuildTools",
    BASE_PACKAGE + "Facility/WBP_FF_FacilityInfoPanel",
    BASE_PACKAGE + "Facility/WBP_FF_FacilitySynergyEntry",
    BASE_PACKAGE + "FacilityCodex/WBP_FF_FacilityCodexLauncher",
)
COMMANDS = ("FF.ApplyBuildReferenceTools", "FF.ApplyFacilitySynergyPearl")
VERSIONS = {
    PACKAGES[0]: "FF.BuildReferenceTools.KeyboardHelpV9",
    PACKAGES[1]: "FF.FacilitySynergy.Pearl.20260924.v2",
    PACKAGES[2]: "FF.FacilitySynergy.Pearl.20260924.v2",
    PACKAGES[3]: "FF.FacilitySynergy.Pearl.20260924.v2",
}
UI_ROOTS = (
    ROOT / "Content/FestaFiesta/UI",
    ROOT / "Content/FestaFiesta/Building/FF_WBP_Building",
)
REPORT = {
    "status": "RUNNING", "commands": list(COMMANDS), "executed": [],
    "backup": str(BACKUP.relative_to(ROOT)), "before": {}, "after": {},
    "metadata": {}, "idempotent": False, "unrelated_ui_unchanged": False,
}


def path_for(package):
    return ROOT / "Content" / (package.removeprefix("/Game/") + ".uasset")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def ui_hashes():
    return {path.relative_to(ROOT).as_posix(): digest(path)
            for folder in UI_ROOTS for path in sorted(folder.rglob("*.uasset"))}


def verify_metadata():
    result = {}
    for package, version in VERSIONS.items():
        blueprint = unreal.load_asset(package)
        assert blueprint, "Missing production WBP: " + package
        value = unreal.EditorAssetLibrary.get_metadata_tag(blueprint, version)
        assert value == "1", "Migration marker missing: " + package + " / " + version
        assert unreal.EditorAssetLibrary.load_blueprint_class(package), "Missing generated class: " + package
        result[package] = {version: value}
    return result


def execute(world, run):
    for command in COMMANDS:
        unreal.log("FF_BUILD_UI_FIXES_EXECUTE: " + command)
        unreal.SystemLibrary.execute_console_command(world, command)
        REPORT["executed"].append({"run": run, "command": command})


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world and world.get_path_name().startswith("/Engine/Maps/Entry."), "Use an idle Entry world"
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Stop PIE first"
    REPORT["world"] = world.get_path_name()
    targets = {path_for(package).relative_to(ROOT).as_posix() for package in PACKAGES}
    before = ui_hashes()
    protected = {path: sha for path, sha in before.items() if path not in targets}
    REPORT["protected_ui_count"] = len(protected)
    for package in PACKAGES:
        path = path_for(package)
        assert path.is_file(), "Missing production WBP: " + package
        REPORT["before"][package] = digest(path)
        backup = BACKUP / path.relative_to(ROOT)
        backup.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, backup)

    # 첫 실행 결과를 기록한 뒤 다시 실행해 중복 적용으로 에셋이 계속 변하지 않는지 검사한다.
    execute(world, 1)
    REPORT["metadata"] = verify_metadata()
    REPORT["after"] = {package: digest(path_for(package)) for package in PACKAGES}
    execute(world, 2)
    assert verify_metadata() == REPORT["metadata"], "Migration changed metadata on its second run"
    repeat = {package: digest(path_for(package)) for package in PACKAGES}
    REPORT["repeat"] = repeat
    assert repeat == REPORT["after"], "Migration rewrote a target WBP on its second run"
    REPORT["idempotent"] = True

    after = ui_hashes()
    current_protected = {path: sha for path, sha in after.items() if path not in targets}
    REPORT["unexpected_ui_changes"] = sorted(
        path for path in protected.keys() | current_protected.keys()
        if protected.get(path) != current_protected.get(path))
    assert not REPORT["unexpected_ui_changes"], "Unexpected UI edit: " + str(REPORT["unexpected_ui_changes"])
    REPORT["unrelated_ui_unchanged"] = True
    REPORT["status"] = "PASS"


try:
    main()
except Exception:
    REPORT["status"] = "FAIL"
    REPORT["error"] = traceback.format_exc()
    unreal.log_error("FF_BUILD_UI_FIXES_APPLY_VERIFY_FAIL: " + REPORT["error"])
    raise
finally:
    RUN.mkdir(parents=True, exist_ok=True)
    serialized = json.dumps(REPORT, ensure_ascii=False, indent=2)
    (RUN / "report.json").write_text(serialized, encoding="utf-8")
    (OUT / "latest-result.json").write_text(serialized, encoding="utf-8")

unreal.log("FF_BUILD_UI_FIXES_APPLY_VERIFY_PASS: " + str(RUN / "report.json"))
