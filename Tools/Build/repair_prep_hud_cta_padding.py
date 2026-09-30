"""Inspect compact Prep HUD CTA spacing; apply only with explicit opt-in.

The older 52-unit status row allowed 5-unit vertical badge padding. Its saved
compact replacement needs 2 units, matching the other compact HUD chips.
"""
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
PACKAGE = "/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar"
TARGET = ROOT / "Content" / (PACKAGE.removeprefix("/Game/") + ".uasset")
RUN = ROOT / "Artifacts" / "Validation" / ("PrepHudCtaRepair-" + datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S-%f"))
APPLY = os.environ.get("FF_APPLY_PREP_HUD_CTA_PADDING_REPAIR") == "1"
REPORT = {"status": "RUNNING", "applyRequested": APPLY, "package": PACKAGE}

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def ui_hashes():
    return {p.relative_to(ROOT).as_posix(): digest(p)
            for folder in (ROOT / "Content/FestaFiesta/UI", ROOT / "Content/FestaFiesta/Building/FF_WBP_Building")
            for p in folder.rglob("*.uasset")}

def inspect(bp):
    tree = unreal.find_object(bp, "WidgetTree")
    badge = unreal.find_object(tree, "Box_Cta", unreal.Border)
    reason = unreal.find_object(tree, "Text_CtaReason", unreal.TextBlock)
    assert badge and reason, "The active CTA badge/text contract must exist"
    padding = badge.get_editor_property("padding")
    rows = {name: unreal.find_object(tree, name, unreal.SizeBox).get_editor_property("height_override")
            for name in ("SizeBox_MenuRow", "SizeBox_StatusRow")}
    state = {"padding": {key: getattr(padding, key) for key in ("left", "top", "right", "bottom")},
             "rows": rows, "reasonFont": str(reason.get_editor_property("font")),
             "reasonText": str(reason.get_text()),
             "reasonOverflow": str(reason.get_editor_property("text_overflow_policy"))}
    return badge, state

try:
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world and world.get_path_name().startswith("/Engine/Maps/Entry."), "Requires idle Entry editor"
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Stop PIE first"
    before_ui = ui_hashes()
    bp = unreal.load_asset(PACKAGE)
    badge, before = inspect(bp)
    REPORT.update(before=before, beforeSha256=digest(TARGET))
    changes = any(before["padding"][key] != 2.0 for key in ("top", "bottom"))
    REPORT["requiredChanges"] = changes
    if APPLY and changes:
        assert before["padding"] == {"left": 14.0, "top": 5.0, "right": 14.0, "bottom": 5.0}, "CTA styling changed; review before repairing"
        backup = RUN / "Backups" / TARGET.relative_to(ROOT)
        backup.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(TARGET, backup)
        assert digest(backup) == REPORT["beforeSha256"], "Backup bytes must match the source"
        REPORT["backup"] = str(backup)
        padding = badge.get_editor_property("padding")
        padding.top = padding.bottom = 2.0
        badge.set_padding(padding)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False), "HUD save failed"
    _, after = inspect(bp)
    REPORT["after"] = after
    assert all(after[key] == before[key] for key in before if key != "padding"), "Unrelated HUD text, font, overflow or row heights changed"
    assert all(after["padding"][key] == before["padding"][key] for key in ("left", "right")), "Horizontal padding changed"
    remaining = any(after["padding"][key] != 2.0 for key in ("top", "bottom"))
    if APPLY:
        assert not remaining, "CTA vertical padding did not persist"
    after_ui = ui_hashes()
    target_path = TARGET.relative_to(ROOT).as_posix()
    assert not [key for key in before_ui.keys() | after_ui.keys()
                if key != target_path and before_ui.get(key) != after_ui.get(key)], "Unrelated UI assets changed"
    if not APPLY:
        assert before_ui == after_ui, "Read-only inspection changed asset bytes"
    REPORT.update(status="PASS", afterSha256=digest(TARGET), remainingChanges=remaining, unrelatedUiAssetCount=len(before_ui) - 1)
except Exception:
    REPORT.update(status="FAILED", error=traceback.format_exc())
    unreal.log_error(REPORT["error"])
finally:
    RUN.mkdir(parents=True, exist_ok=True)
    result = RUN / "report.json"
    result.write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("FF_PREP_HUD_CTA_REPAIR_" + REPORT["status"] + ": " + str(result))
if REPORT["status"] != "PASS":
    raise RuntimeError("Prep HUD CTA repair failed; inspect the report and retained backup")
