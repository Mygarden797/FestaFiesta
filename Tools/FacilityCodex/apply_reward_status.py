# apply_reward_status.py
# 역할: 시설 조합 도감과 보상 표시 작업에서 지정된 대상에 변경을 적용하고 결과를 확인한다.

"""Apply distinct available/claimed reward presentation in an idle Entry editor."""
import hashlib
import json
import shutil
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
UI_ROOTS = (ROOT / "Content/FestaFiesta/UI", ROOT / "Content/FestaFiesta/Building/FF_WBP_Building")
OUT = ROOT / "Saved/FacilityCodexRewardStatus_20260921"
CARD = "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexCard"
CARD_FILE = ROOT / "Content/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexCard.uasset"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify():
    bp = unreal.load_asset(CARD)
    owned = {widget.get_name(): widget for widget in unreal.ObjectIterator(unreal.Widget)
             if widget.get_outer() and widget.get_outer().get_class().get_name() == "WidgetTree"
             and widget.get_outer().get_outer() == bp}
    assert unreal.EditorAssetLibrary.get_metadata_tag(bp, "FF.FacilityCodex.RewardStatus20260921.1") == "1"
    assert str(owned["Text_ClaimReward"].get_text()) == "받기"
    assert str(owned["Text_RewardClaimed"].get_text()) == "완료"
    assert owned["Text_ClaimReward"].get_parent() == owned["Horizontal_RewardContent"]
    assert owned["Border_RewardClaimed"].get_parent() == owned.get("Overlay_RewardState", owned["Horizontal_RewardActions"])
    assert owned["Image_RewardClaimed"].get_editor_property("brush").get_editor_property("resource_object")
    assert owned["Border_RewardClaimed"].get_visibility() == unreal.SlateVisibility.COLLAPSED
    assert owned["Border_RewardClaimed"].get_is_enabled()


world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world and world.get_path_name().startswith("/Engine/Maps/Entry."), "Use idle Entry map"
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
OUT.mkdir(parents=True, exist_ok=True)
backup = OUT / CARD_FILE.name
if not backup.exists():
    shutil.copy2(CARD_FILE, backup)
protected = {path: digest(path) for base in UI_ROOTS for path in base.rglob("*.uasset") if path != CARD_FILE}
unreal.SystemLibrary.execute_console_command(world, "FF.ApplyFacilityCodexRewardStatus")
verify()
saved_hash = digest(CARD_FILE)
unreal.SystemLibrary.execute_console_command(world, "FF.ApplyFacilityCodexRewardStatus")
verify()
assert digest(CARD_FILE) == saved_hash, "Repeated migration changed saved card"
for path, original in protected.items():
    assert digest(path) == original, "Unexpected UI edit: " + str(path)
(OUT / "migration-result.json").write_text(json.dumps({
    "status": "PASS", "card_sha256": saved_hash,
    "preserved_other_ui_count": len(protected), "idempotent": True,
}, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("FF_CODEX_REWARD_STATUS_VERIFY_PASS")
