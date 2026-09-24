# Apply the screenshot-inspired Pearl build UI to the existing production WBPs.
# Run from an idle /Engine/Maps/Entry editor with -ExecutePythonScript.
import hashlib
import json
import shutil
import time
import traceback
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Saved" / "BuildReferenceUI"
OUT.mkdir(parents=True, exist_ok=True)
PACKAGES = (
    "/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepPanelBuild",
    "/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepBuildTile",
    "/Game/FestaFiesta/Building/FF_WBP_Building/Controls/WBP_FF_BuildCategoryChoice",
    "/Game/FestaFiesta/Building/FF_WBP_Building/Controls/WBP_FF_BuildTools",
    "/Game/FestaFiesta/Building/FF_WBP_Building/Controls/WBP_FF_BuildSynergy",
    "/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityInfoPanel",
    "/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilitySynergyEntry",
    "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexLauncher",
    "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexScreen",
    "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexCard",
    "/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelProgram",
    "/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepProgramCard",
    "/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityPaidProgramPicker",
    "/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityPaidProgramCard",
    "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityRevenue/WBP_FF_FacilityRevenueRanking",
    "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityRevenue/WBP_FF_FacilityRevenueRankRow",
)
COMMANDS = (
    "FF.ApplyBuildReferenceCatalog",
    "FF.ApplyBuildReferenceCodexCatalogTab",
    "FF.ApplyBuildReferenceTools",
    "FF.ApplyBuildReferenceFacilityInfo",
    "FF.ApplyBuildModeClaritySynergy",
    "FF.ApplyBuildPearlCatalog",
    "FF.ApplyBuildPearlCodex",
    "FF.CreateBuildMultiSelectionWidget",
    "FF.ApplyProgramPearlUI",
    "FF.ApplyFacilityRevenuePearlUI",
    "FF.ApplyFacilitySynergyPearl",
)
STATE = {"step": -1, "next": time.monotonic() + 2.0, "before": {}, "commands": [], "busy": False}
BACKUP = OUT / "backups" / time.strftime("%Y%m%d-%H%M%S")


def path_for(package):
    return ROOT / "Content" / (package.removeprefix("/Game/") + ".uasset")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None


def finish(error=None):
    unreal.unregister_slate_post_tick_callback(CALLBACK)
    if not error and len(STATE["commands"]) != len(COMMANDS):
        error = "Not all WBP migration commands completed"
    result = {
        "error": error,
        "commands": STATE["commands"],
        "backup": str(BACKUP),
        "assets": [
            {"package": package, "before": STATE["before"].get(package),
             "after": digest(path_for(package))}
            for package in PACKAGES
        ],
    }
    (OUT / "apply-result.json").write_text(
        json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    if error:
        unreal.log_error("FF_BUILD_REFERENCE_UI_FAIL: " + error)
    else:
        unreal.log("FF_BUILD_REFERENCE_UI_COMMANDS_COMPLETE")
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)


def tick(_delta):
    if STATE["busy"] or time.monotonic() < STATE["next"]:
        return
    STATE["busy"] = True
    try:
        step = STATE["step"]
        if step == -1:
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            if world.get_path_name() != "/Engine/Maps/Entry.Entry":
                raise RuntimeError("Open /Engine/Maps/Entry before editing WBPs")
            for package in PACKAGES:
                path = path_for(package)
                if not path.is_file():
                    raise RuntimeError("Missing build WBP: " + package)
                STATE["before"][package] = digest(path)
                copy = BACKUP / path.relative_to(ROOT)
                copy.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, copy)
            material_script = ROOT / "Tools/BuildModeClarity/create_pearl_materials.py"
            exec(compile(material_script.read_text(encoding="utf-8"), str(material_script), "exec"), {})
        elif step < len(COMMANDS):
            command = COMMANDS[step]
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            unreal.log("FF_BUILD_REFERENCE_UI_EXECUTE: " + command)
            unreal.SystemLibrary.execute_console_command(world, command)
            STATE["commands"].append(command)
        else:
            finish()
            return
        STATE["step"] += 1
        STATE["next"] = time.monotonic() + 1.0
    except Exception:
        finish(traceback.format_exc())
    finally:
        STATE["busy"] = False


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
CALLBACK = unreal.register_slate_post_tick_callback(tick)
