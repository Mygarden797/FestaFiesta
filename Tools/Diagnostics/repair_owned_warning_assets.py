# repair_owned_warning_assets.py
# 역할: 시설·경제·UI 경고 진단 작업에서 확인된 데이터 오류만 수정한다.

"""Repair only Han Kwang Hoon's legacy Taco catalog entry and missing build guide.

Run after building FestaFiestaEditor with Unreal's PythonScript commandlet.
No maps, team UI, animation, materials, or shared settings are saved.
The legacy facility ID and specification stay intact for existing references.
"""
import hashlib
import json
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "Artifacts" / "WarningRepair"
LEGACY = "/Game/FestaFiesta/Building/FF_Data_Building/Booth/PDA_FFZS_Booth_FoodTruck_Taco"
CURRENT = "/Game/FestaFiesta/Building/FF_Data_Building/Booth/PDA_FFZS_Booth_FoodStall_Taco"
GUIDE = "/Game/FestaFiesta/Building/FF_WBP_Building/Guide/WBP_FF_BuildModeGuide"


def run():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    legacy = unreal.load_asset(LEGACY)
    current = unreal.load_asset(CURRENT)
    assert legacy and current, "Both legacy and replacement definitions must load."
    legacy_spec = legacy.get_editor_property("spec")
    current_spec = current.get_editor_property("spec")
    assert str(legacy_spec.get_editor_property("facility_definition_id")) == "Booth_FoodTruck_Taco"
    assert str(current_spec.get_editor_property("facility_definition_id")) == "Booth_FoodStall_Taco"
    assert current.get_editor_property("show_in_build_catalog"), "Replacement must remain available."
    # Same visual identity is required; queue tuning may intentionally differ.
    assert legacy_spec.get_editor_property("visual_mesh") == current_spec.get_editor_property("visual_mesh")
    report = {"changed": [], "preserved_legacy_id": "Booth_FoodTruck_Taco"}
    if legacy.get_editor_property("show_in_build_catalog"):
        asset_file = ROOT / "Content" / (LEGACY.removeprefix("/Game/") + ".uasset")
        backup = OUTPUT / "Before" / "PDA_FFZS_Booth_FoodTruck_Taco.uasset"
        backup.parent.mkdir(parents=True, exist_ok=True)
        if not backup.exists():
            backup.write_bytes(asset_file.read_bytes())
        report["legacy_before_sha256"] = hashlib.sha256(asset_file.read_bytes()).hexdigest()
        legacy.set_editor_property("show_in_build_catalog", False)
        assert unreal.EditorAssetLibrary.save_loaded_asset(legacy, only_if_is_dirty=False)
        report["changed"].append(LEGACY)
    assert str(legacy.get_editor_property("spec").get_editor_property("facility_definition_id")) == "Booth_FoodTruck_Taco"
    existed = unreal.EditorAssetLibrary.does_asset_exist(GUIDE)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world, "An editor world is required for the build guide command."
    unreal.SystemLibrary.execute_console_command(world, "FF.BuildModeGuideUMG")
    guide = unreal.load_asset(GUIDE)
    assert guide and guide.generated_class(), "Build guide must exist and have a compiled class."
    if not existed:
        report["changed"].append(GUIDE)
    report["guide_class"] = guide.generated_class().get_path_name()
    report["canonical_available"] = bool(current.get_editor_property("show_in_build_catalog"))
    (OUTPUT / "owned-asset-repair.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("FF_OWNED_WARNING_ASSETS_OK " + json.dumps(report, ensure_ascii=False))


run()
