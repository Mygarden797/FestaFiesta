# apply_reward_bounds.py
# 역할: 시설 조합 도감과 보상 표시 작업에서 지정된 대상에 변경을 적용하고 결과를 확인한다.

"""Give available/claimed reward surfaces the same saved layout allocation."""
import hashlib
import json
import shutil
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Saved/FacilityCodexRewardBounds_20260921"
UI = ROOT / "Content/FestaFiesta/Building/FF_WBP_Building"
UI_ROOTS = (ROOT / "Content/FestaFiesta/UI", UI)
CARD = "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexCard"
CARD_FILE = UI / "FacilityCodex/WBP_FF_FacilityCodexCard.uasset"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify():
    bp = unreal.load_asset(CARD)
    owned = {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget)
             if w.get_outer() and w.get_outer().get_class().get_name() == "WidgetTree"
             and w.get_outer().get_outer() == bp}
    assert unreal.EditorAssetLibrary.get_metadata_tag(bp, "FF.FacilityCodex.RewardBounds20260921.1") == "1"
    bounds = owned["Size_RewardState"]
    states = owned["Overlay_RewardState"]
    assert bounds.get_editor_property("width_override") == 72.0
    assert bounds.get_editor_property("height_override") == 32.0
    assert bounds.get_visibility() == unreal.SlateVisibility.COLLAPSED
    assert bounds.get_content() == states
    assert owned["Horizontal_RewardActions"].get_child_at(1) == bounds
    names = ["Button_ClaimReward", "Border_RewardClaimed"]
    if "Border_RewardLocked" in owned:
        names.append("Border_RewardLocked")
    assert states.get_children_count() == len(names)
    for name in names:
        widget = owned[name]
        assert widget.get_parent() == states
        slot = widget.get_editor_property("slot")
        assert slot.get_editor_property("horizontal_alignment") == unreal.HorizontalAlignment.H_ALIGN_FILL
        assert slot.get_editor_property("vertical_alignment") == unreal.VerticalAlignment.V_ALIGN_FILL
    assert len([w for w in owned.values() if not w.get_parent()]) == 1, "Detached widget"


world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world and world.get_path_name().startswith("/Engine/Maps/Entry."), "Use idle Entry map"
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
OUT.mkdir(parents=True, exist_ok=True)
backup = OUT / CARD_FILE.name
if not backup.exists():
    shutil.copy2(CARD_FILE, backup)
protected = {p: digest(p) for base in UI_ROOTS for p in base.rglob("*.uasset") if p != CARD_FILE}
unreal.SystemLibrary.execute_console_command(world, "FF.ApplyFacilityCodexRewardBounds")
verify()
saved_hash = digest(CARD_FILE)
# The earlier migration still recognizes the new hierarchy and preserves it.
unreal.SystemLibrary.execute_console_command(world, "FF.ApplyFacilityCodexActions")
unreal.SystemLibrary.execute_console_command(world, "FF.ApplyFacilityCodexRewardBounds")
verify()
assert digest(CARD_FILE) == saved_hash, "Repeated migration changed the saved card"
for path, original in protected.items():
    assert digest(path) == original, "Unexpected UI edit: " + str(path)
(OUT / "migration-result.json").write_text(json.dumps({
    "status": "PASS", "card_sha256": saved_hash, "reward_size": [72, 32],
    "preserved_other_ui_count": len(protected), "idempotent": True,
}, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("FF_CODEX_REWARD_BOUNDS_VERIFY_PASS")
