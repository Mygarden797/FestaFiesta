# apply_approved_build_ui.py
# 역할: UI 이미지·화면 에셋 작업에서 지정된 대상에 변경을 적용하고 결과를 확인한다.

"""Apply the approved build/inspection UI in an unattended full Unreal Editor process.

Run with -ExecutePythonScript, not the Python commandlet: WBP compilation needs editor
initialization. Each native migration is versioned and preserves subsequent Designer edits.
Only the six listed build/inspection assets are in scope; the teammate HUD is not rebuilt.
"""

import hashlib
import json
import shutil
import time
import traceback
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Artifacts" / "Validation" / "BuildUIApproved"
OUT.mkdir(parents=True, exist_ok=True)
PACKAGES = (
    "/Game/FestaFiesta/Building/FF_WBP_Building/Controls/WBP_FF_BuildCategoryChoice",
    "/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepBuildTile",
    "/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepPanelBuild",
    "/Game/FestaFiesta/Building/FF_WBP_Building/Controls/WBP_FF_BuildTools",
    "/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityInfoPanel",
    "/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityPaidProgramPicker",
)
COMMANDS = (
    "FF.RefreshBuildStudioUMG",
    "FF.RefreshBuildToolsUMG",
    "FF.MigrateFacilityInspectorUMG",
)
_state = {"index": 0, "next": time.monotonic() + 3.0, "commands": [], "before": {}}
_backup = OUT / "Backups" / time.strftime("%Y%m%d-%H%M%S")


def asset_file(package):
    return ROOT / "Content" / (package.removeprefix("/Game/") + ".uasset")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None


def finish(error=None):
    unreal.unregister_slate_post_tick_callback(_callback)
    result = {
        "commands": _state["commands"],
        "error": error,
        "assets": [
            {"package": p, "before": _state["before"].get(p), "after": digest(asset_file(p))}
            for p in PACKAGES
        ],
        "backup": str(_backup),
        "verification": "Run the UI automation suite after restarting the editor.",
    }
    (OUT / "asset-migration.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    if error:
        unreal.log_error("FF_APPROVED_BUILD_UI_ERROR: " + error)
    else:
        unreal.log("FF_APPROVED_BUILD_UI_COMMANDS_COMPLETE")
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)


def tick(_delta):
    if time.monotonic() < _state["next"]:
        return
    try:
        index = _state["index"]
        if index == 0:
            for package in PACKAGES:
                path = asset_file(package)
                if not path.is_file():
                    raise RuntimeError("Required WBP is missing: " + package)
                _state["before"][package] = digest(path)
                target = _backup / path.relative_to(ROOT)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, target)
        if index == len(COMMANDS):
            finish()
            return
        command = COMMANDS[index]
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        unreal.log("FF_APPROVED_BUILD_UI_EXECUTE: " + command)
        unreal.SystemLibrary.execute_console_command(world, command)
        _state["commands"].append(command)
        _state["index"] += 1
        _state["next"] = time.monotonic() + 1.0
    except Exception:
        finish(traceback.format_exc())


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
_callback = unreal.register_slate_post_tick_callback(tick)
