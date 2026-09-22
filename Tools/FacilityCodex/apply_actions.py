"""Add authored codex actions and verify layout preservation and repeatability.

Run a freshly built UE 5.7 editor on /Engine/Maps/Entry with
-ExecutePythonScript=<absolute path to this file>. No PIE session may be active.
Only the existing codex Screen and Card packages are saved by this migration.
"""
import hashlib
import json
import shutil
import traceback
from datetime import datetime, timezone
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
UI_ROOTS = (ROOT / "Content/FestaFiesta/UI", ROOT / "Content/FestaFiesta/Building/FF_WBP_Building")
OUT = ROOT / "Saved/FacilityCodex/Actions20260921"
COMMAND = "FF.ApplyFacilityCodexActions"
VERSION = "FF.FacilityCodex.Actions20260921.1"
ASSETS = {
    role: "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodex" + role
    for role in ("Screen", "Card")
}
ADDITIONS = {
    "Screen": {"Button_TargetCatalog", "Button_PartnerCatalog"},
    "Card": {"Scale_RewardActions", "Horizontal_RewardActions", "Button_ClaimReward", "Text_ClaimReward"},
}
REPARENTED = {"Screen": {"Vertical_Target", "Vertical_Partner"}, "Card": {"Text_Bonus"}}


def asset_file(path):
    return ROOT / "Content" / (path.removeprefix("/Game/") + ".uasset")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def widgets(bp):
    owned = [widget for widget in unreal.ObjectIterator(unreal.Widget)
             if widget.get_outer() and widget.get_outer().get_class().get_name() == "WidgetTree"
             and widget.get_outer().get_outer() == bp]
    roots = [widget.get_name() for widget in owned if not widget.get_parent()]
    assert len(roots) == 1, bp.get_name() + ": detached widgets: " + str(roots)
    result = {widget.get_name(): widget for widget in owned}
    assert len(result) == len(owned), bp.get_name() + ": duplicate widget names"
    return result


def snapshot(bp):
    owned = widgets(bp)
    return {
        "tree": {name: {"class": widget.get_class().get_name(),
                        "parent": widget.get_parent().get_name() if widget.get_parent() else None}
                 for name, widget in owned.items()},
        "bounds": {name: [widget.get_editor_property("width_override"),
                           widget.get_editor_property("height_override")]
                   for name, widget in owned.items() if isinstance(widget, unreal.SizeBox)},
    }


def verify_contract(role, bp):
    parent = unreal.load_class(None, "/Script/FestaFiesta.FF_FacilityCodex" + role)
    assert parent and bp.generated_class() and unreal.MathLibrary.class_is_child_of(bp.generated_class(), parent), role + ": wrong parent"
    assert unreal.EditorAssetLibrary.get_metadata_tag(bp, VERSION) == "1", role + ": migration not saved"
    owned = widgets(bp)
    assert ADDITIONS[role] <= owned.keys(), role + ": missing action widgets"
    if role == "Screen":
        for side in ("Target", "Partner"):
            button = owned["Button_" + side + "Catalog"]
            column = owned["Vertical_" + side]
            assert isinstance(button, unreal.Button) and button.get_content() == column
            assert button.get_parent() == owned["Horizontal_Ingredients"]
            assert owned["Size_Thumb" + side].get_parent() == column
            assert owned["Text_" + side + "Name"].get_parent() == column
            assert str(button.get_editor_property("tool_tip_text")) == "건축 카탈로그에서 확인"
        assert owned["TileView_Entries"].get_editor_property("column_count") == 6
    else:
        button = owned["Button_ClaimReward"]
        row = owned["Horizontal_RewardActions"]
        scale = owned["Scale_RewardActions"]
        assert isinstance(button, unreal.Button)
        assert button.get_content() == owned.get("Overlay_ClaimReward", owned["Text_ClaimReward"])
        assert row.get_child_at(0) == owned["Text_Bonus"] and row.get_child_at(1) == owned.get("Size_RewardState", button)
        assert scale.get_content() == row and scale.get_parent() == owned["Vertical_Card"]
        assert button.get_visibility() == unreal.SlateVisibility.COLLAPSED
        assert not owned["Text_ClaimReward"].get_editor_property("auto_wrap_text")
    return sorted(owned)


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world and world.get_path_name().startswith("/Engine/Maps/Entry."), "Use /Engine/Maps/Entry"
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Stop PIE before migration"
    paths = {role: asset_file(path) for role, path in ASSETS.items()}
    assert all(path.is_file() for path in paths.values()), "Existing codex assets are required"
    protected = {path: digest(path) for base in UI_ROOTS for path in base.rglob("*.uasset") if path not in paths.values()}
    backup_dir = OUT / "backups" / datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    backup_dir.mkdir(parents=True, exist_ok=False)
    for path in paths.values():
        shutil.copy2(path, backup_dir / path.name)
    bps = {role: unreal.load_asset(path) for role, path in ASSETS.items()}
    assert all(bps.values()), "Failed to load codex assets"
    before = {role: snapshot(bp) for role, bp in bps.items()}
    before_hashes = {role: digest(path) for role, path in paths.items()}

    unreal.SystemLibrary.execute_console_command(world, COMMAND)
    names = {role: verify_contract(role, bp) for role, bp in bps.items()}
    for role, bp in bps.items():
        after = snapshot(bp)
        assert before[role]["bounds"] == after["bounds"], role + ": existing size bounds changed"
        assert set(after["tree"]) - set(before[role]["tree"]) <= ADDITIONS[role], role + ": unexpected new widgets"
        for name, original in before[role]["tree"].items():
            assert name in after["tree"], role + ": original widget removed: " + name
            current = after["tree"][name]
            assert original["class"] == current["class"], role + ": widget type changed: " + name
            if name not in REPARENTED[role]:
                assert original["parent"] == current["parent"], role + ": unrelated widget reparented: " + name
    after_hashes = {role: digest(path) for role, path in paths.items()}
    unreal.SystemLibrary.execute_console_command(world, COMMAND)
    for role, bp in bps.items():
        verify_contract(role, bp)
        assert digest(paths[role]) == after_hashes[role], role + ": repeated command changed saved bytes"
    current_protected = {path: digest(path) for base in UI_ROOTS for path in base.rglob("*.uasset") if path not in paths.values()}
    assert protected == current_protected, "Unexpected change outside codex Screen/Card"
    return {"status": "PASS", "command": COMMAND, "version": VERSION,
            "backup_directory": str(backup_dir), "before_sha256": before_hashes,
            "after_sha256": after_hashes, "preserved_other_ui_count": len(protected),
            "existing_widget_bounds_preserved": True, "rerun_preserved_saved_bytes": True,
            "widgets": names}


OUT.mkdir(parents=True, exist_ok=True)
try:
    report = main()
except Exception:
    report = {"status": "FAIL", "command": COMMAND, "error": traceback.format_exc()}
    (OUT / "migration-result.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log_error("FF_FACILITY_CODEX_ACTIONS_VERIFY_FAIL: " + report["error"])
    raise
else:
    (OUT / "migration-result.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("FF_FACILITY_CODEX_ACTIONS_VERIFY_PASS")
