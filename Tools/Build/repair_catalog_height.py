"""Inspect the saved catalog/card sizing contract; apply only with explicit opt-in.

Use an idle Entry editor with -ExecutePythonScript. The current card remains
authored at 210 units; the active catalog's row allocation and dock/default
height follow the compact card. Every application is backed up.
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
RUN = ROOT / "Artifacts" / "Validation" / (
    "CatalogHeightRepair-" + datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S-%f"))
PANEL = "/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepPanelBuild"
CARD = "/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepBuildTile"
TARGET = ROOT / "Content" / (PANEL.removeprefix("/Game/") + ".uasset")
APPLY = os.environ.get("FF_APPLY_CATALOG_HEIGHT_REPAIR") == "1"
REPORT = {"status": "RUNNING", "applyRequested": APPLY, "package": PANEL,
          "cardPackage": CARD, "sourceContract": "FF_BuildReadabilityBuilder.cpp: Catalog/Card 210, dock/default 324"}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def all_ui_hashes():
    return {path.relative_to(ROOT).as_posix(): digest(path)
            for base in (ROOT / "Content/FestaFiesta/UI", ROOT / "Content/FestaFiesta/Building/FF_WBP_Building")
            for path in base.rglob("*.uasset")}


def widgets(blueprint):
    return {widget.get_name(): widget for widget in unreal.ObjectIterator(unreal.Widget)
            if widget.get_outer() and widget.get_outer().get_class().get_name() == "WidgetTree"
            and widget.get_outer().get_outer() == blueprint}


def inspect():
    panel = unreal.load_asset(PANEL)
    card = unreal.load_asset(CARD)
    assert panel and card, "Active catalog and card WBP must exist"
    panel_widgets = widgets(panel)
    card_widgets = widgets(card)
    tiles = panel_widgets["TileView_Facilities"]
    dock = panel_widgets["Size_ClarityCatalogDock"]
    card_size = card_widgets["SizeBox_Card"]
    default = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(PANEL))
    state = {
        "entryHeight": tiles.get_editor_property("entry_height"),
        "entryWidth": tiles.get_editor_property("entry_width"),
        "dockHeight": dock.get_editor_property("height_override"),
        "catalogHeight": default.get_editor_property("catalog_height"),
        "cardHeight": card_size.get_editor_property("height_override"),
        "cardWidth": card_size.get_editor_property("width_override"),
        "designerLayout": default.get_editor_property("use_designer_catalog_layout"),
        "pagination": default.get_editor_property("use_pagination"),
        "widgetNames": sorted(panel_widgets),
    }
    return panel, tiles, dock, default, state


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world and world.get_path_name().startswith("/Engine/Maps/Entry."), "Requires idle Entry editor"
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Stop PIE first"
    before_hashes = all_ui_hashes()
    panel, tiles, dock, default, before = inspect()
    REPORT.update(before=before, beforeSha256=digest(TARGET))
    assert before["cardHeight"] == 210.0, "Authored card changed; review its new layout before repairing"
    desired = {"entryHeight": before["cardHeight"],
               "dockHeight": 324.0,
               "catalogHeight": 324.0}
    changes = {key: {"before": before[key], "after": value}
               for key, value in desired.items() if before[key] != value}
    REPORT["requiredChanges"] = changes
    if APPLY and changes:
        backup = RUN / "Backups" / TARGET.relative_to(ROOT)
        backup.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(TARGET, backup)
        assert digest(backup) == REPORT["beforeSha256"], "Backup bytes do not match the original asset"
        REPORT["backup"] = str(backup)
        tiles.set_editor_property("entry_height", desired["entryHeight"])
        dock.set_height_override(desired["dockHeight"])
        # Compile the edited template before persisting its generated defaults.
        unreal.BlueprintEditorLibrary.compile_blueprint(panel)
        default = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(PANEL))
        default.set_editor_property("catalog_height", desired["catalogHeight"])
        assert unreal.EditorAssetLibrary.save_loaded_asset(panel, only_if_is_dirty=False), "Catalog save failed"
    _, _, _, _, after = inspect()
    REPORT["after"] = after
    for key in before:
        if key not in desired:
            assert after[key] == before[key], "Unrelated catalog property changed: " + key
    if APPLY:
        assert all(after[key] == value for key, value in desired.items()), "Saved sizing does not match the contract"
    after_hashes = all_ui_hashes()
    target_path = TARGET.relative_to(ROOT).as_posix()
    unexpected = [path for path in before_hashes.keys() | after_hashes.keys()
                  if path != target_path and before_hashes.get(path) != after_hashes.get(path)]
    assert not unexpected, "Unrelated UI assets changed: " + repr(unexpected)
    if not APPLY:
        assert after_hashes == before_hashes, "Read-only inspection changed asset bytes"
    REPORT.update(status="PASS", afterSha256=digest(TARGET),
                  unrelatedUiAssetCount=len(before_hashes) - 1,
                  remainingChanges={key: {"before": after[key], "after": value}
                                    for key, value in desired.items() if after[key] != value})


try:
    main()
except Exception:
    REPORT.update(status="FAILED", error=traceback.format_exc())
    unreal.log_error(REPORT["error"])
finally:
    RUN.mkdir(parents=True, exist_ok=True)
    report_path = RUN / "report.json"
    report_path.write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("FF_CATALOG_HEIGHT_REPAIR_" + REPORT["status"] + ": " + str(report_path))
if REPORT["status"] != "PASS":
    raise RuntimeError("Catalog height repair failed; inspect report.json and retained backup")
