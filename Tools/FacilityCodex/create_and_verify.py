# create_and_verify.py
# 역할: 시설 조합 도감과 보상 표시 작업에서 필요한 에셋 또는 설정을 생성한다.

"""Create the codex WBP assets and add its launcher without replacing existing UI.

Run in a freshly built UnrealEditor-Cmd with -run=pythonscript -script=<this file>.
Re-running must preserve all saved assets byte for byte.
"""
import hashlib
import json
import traceback
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "Saved/FacilityCodex"
ASSETS = {
    "Screen": "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexScreen",
    "Card": "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexCard",
    "Launcher": "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexLauncher",
    "CatalogLauncher": "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexCatalogLauncher",
    "Inspector": "/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityInfoPanel",
    "CatalogPanel": "/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepPanelBuild",
}
PARENTS = {
    "CatalogLauncher": "FF_FacilityCodexLauncher",
    "Inspector": "FF_FacilityInfoPanel",
    "CatalogPanel": "FF_PrepPanelBuild",
}


def asset_file(path):
    return ROOT / "Content" / (path.removeprefix("/Game/") + ".uasset")


def digest(path):
    return hashlib.sha256(asset_file(path).read_bytes()).hexdigest()


def main():
    prior = {key: digest(path) for key, path in ASSETS.items() if asset_file(path).exists()}
    unreal.SystemLibrary.execute_console_command(None, "FF.BuildFacilityCodexUMG")
    unreal.SystemLibrary.execute_console_command(None, "FF.MigrateFacilityCodexInspector")
    unreal.SystemLibrary.execute_console_command(None, "FF.MigrateFacilityCodexCatalog")
    for key, path in ASSETS.items():
        asset = unreal.load_asset(path)
        if not asset or not asset.generated_class() or not asset_file(path).exists():
            raise RuntimeError("Missing or uncompiled codex asset: " + path)
        parent_path = "/Script/FestaFiesta." + PARENTS.get(key, "FF_FacilityCodex" + key)
        parent = unreal.load_class(None, parent_path)
        if not parent or not unreal.MathLibrary.class_is_child_of(asset.generated_class(), parent):
            raise RuntimeError("Wrong native widget contract: " + path)
        if key in prior and key not in ("Inspector", "CatalogPanel") and digest(path) != prior[key]:
            raise RuntimeError("Existing Designer-owned codex asset was rewritten: " + path)
    first = {key: digest(path) for key, path in ASSETS.items()}
    unreal.SystemLibrary.execute_console_command(None, "FF.BuildFacilityCodexUMG")
    unreal.SystemLibrary.execute_console_command(None, "FF.MigrateFacilityCodexInspector")
    unreal.SystemLibrary.execute_console_command(None, "FF.MigrateFacilityCodexCatalog")
    second = {key: digest(path) for key, path in ASSETS.items()}
    if first != second:
        raise RuntimeError("Repeated authoring commands changed saved WBP bytes")
    return {"status": "PASS", "assets": ASSETS, "sha256": second,
            "rerun_preserved_saved_bytes": True,
            "existing_assets_before_run": prior}


OUT.mkdir(parents=True, exist_ok=True)
try:
    report = main()
except Exception:
    report = {"status": "FAIL", "error": traceback.format_exc()}
    (OUT / "asset_verification.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log_error("FACILITY_CODEX_ASSETS_FAIL: " + report["error"])
    raise
else:
    (OUT / "asset_verification.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("FACILITY_CODEX_ASSETS_PASS: Designer edits preserved on repeated authoring commands")
