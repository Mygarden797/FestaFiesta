# apply_reward_iridescence.py
# 역할: 시설 조합 도감과 보상 표시 작업에서 지정된 대상에 변경을 적용하고 결과를 확인한다.

"""Save the game reward shader and authored WBP styles in an idle Entry editor."""
import hashlib
import json
import runpy
import shutil
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Saved/FacilityCodexRewardIridescence_20260921"
UI = ROOT / "Content/FestaFiesta/Building/FF_WBP_Building"
UI_ROOTS = (ROOT / "Content/FestaFiesta/UI", UI)
CARD = "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexCard"
CARD_FILE = UI / "FacilityCodex/WBP_FF_FacilityCodexCard.uasset"
MATERIAL_DIR = UI / "FacilityCodex/Materials"
MATERIAL_PATH = "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/Materials/"
OWNED_MATERIAL_FILES = [MATERIAL_DIR / (name + ".uasset") for name in
                        ("M_FF_CodexRewardIridescent", "MI_FF_CodexReward_Ready", "MI_FF_CodexReward_Hover",
                         "MI_FF_CodexReward_Pressed", "MI_FF_CodexReward_Claimed")]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify():
    bp = unreal.load_asset(CARD)
    owned = {widget.get_name(): widget for widget in unreal.ObjectIterator(unreal.Widget)
             if widget.get_outer() and widget.get_outer().get_class().get_name() == "WidgetTree"
             and widget.get_outer().get_outer() == bp}
    assert unreal.EditorAssetLibrary.get_metadata_tag(bp, "FF.FacilityCodex.RewardIridescence20260921.1") == "1"
    style = owned["Button_ClaimReward"].get_editor_property("widget_style")
    for brush_name, state in (("normal", "Ready"), ("hovered", "Hover"), ("pressed", "Pressed"), ("disabled", "Claimed")):
        resource = style.get_editor_property(brush_name).get_editor_property("resource_object")
        assert resource == unreal.load_asset(MATERIAL_PATH + "MI_FF_CodexReward_" + state), brush_name
    claimed = unreal.load_asset(MATERIAL_PATH + "MI_FF_CodexReward_Claimed")
    assert owned["Border_RewardClaimed"].get_editor_property("background").get_editor_property("resource_object") == claimed
    assert unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(claimed, "MotionStrength") == 0.0
    assert str(owned["Text_ClaimReward"].get_text()) == "받기"
    assert str(owned["Text_RewardClaimed"].get_text()) == "완료"
    assert owned["Border_RewardClaimed"].get_visibility() == unreal.SlateVisibility.COLLAPSED
    assert owned["Border_RewardClaimed"].get_is_enabled()


world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world and world.get_path_name().startswith("/Engine/Maps/Entry."), "Use idle Entry map"
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
OUT.mkdir(parents=True, exist_ok=True)
targets_before = [CARD_FILE] + [p for p in OWNED_MATERIAL_FILES if p.exists()]
for target in targets_before:
    backup = OUT / "before_assets" / target.relative_to(UI)
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(target, backup)
protected = {p: digest(p) for base in UI_ROOTS for p in base.rglob("*.uasset") if p != CARD_FILE and p not in OWNED_MATERIAL_FILES}
runpy.run_path(str(ROOT / "Tools/FacilityCodex/create_reward_materials.py"), run_name="__main__")
unreal.SystemLibrary.execute_console_command(world, "FF.ApplyFacilityCodexRewardIridescence")
verify()
saved = {p: digest(p) for p in [CARD_FILE] + OWNED_MATERIAL_FILES}
assert len(saved) == 6, "Expected one card, one base shader and four state instances"
runpy.run_path(str(ROOT / "Tools/FacilityCodex/create_reward_materials.py"), run_name="__main__")
unreal.SystemLibrary.execute_console_command(world, "FF.ApplyFacilityCodexRewardIridescence")
verify()
for path, original in saved.items():
    assert digest(path) == original, "Repeated migration changed " + str(path)
for path, original in protected.items():
    assert digest(path) == original, "Unexpected UI edit: " + str(path)
(OUT / "migration-result.json").write_text(json.dumps({
    "status": "PASS", "asset_sha256": {str(p.relative_to(ROOT)): value for p, value in saved.items()},
    "preserved_other_ui_count": len(protected), "idempotent": True,
}, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("FF_CODEX_REWARD_IRIDESCENCE_VERIFY_PASS")
