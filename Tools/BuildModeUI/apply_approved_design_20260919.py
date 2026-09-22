# apply_approved_design_20260919.py
# 역할: 승인된 건설 화면 및 데이터 이관 작업에서 지정된 대상에 변경을 적용하고 결과를 확인한다.

"""Apply the approved build UI to an explicit package allowlist, once.

Run with the freshly built Editor and -ExecutePythonScript, in /Engine/Maps/Entry.
Existing layouts are backed up. Each command has its own preservation guard.
No maps, gameplay definitions, project settings or shared HUD assets are saved.
"""
import hashlib
import json
import shutil
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Saved/BuildModeUI/20260919"
OUT.mkdir(parents=True, exist_ok=True)
BASELINE = json.loads((OUT / "wbp-baseline.json").read_text(encoding="utf-8"))
EXISTING = [
    "/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityInfoPanel",
    "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexScreen",
    "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexCard",
    "/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepPanelBuild",
]
NEW = [
    "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityRevenue/WBP_FF_FacilityRevenueRankRow",
    "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityRevenue/WBP_FF_FacilityRevenueRanking",
    "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityRevenue/WBP_FF_FacilityWorldBadge",
    "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityRevenue/PAL_FF_FacilityRevenueUI",
]
COMMANDS = [
    "FF.ApplyFacilityInfoV2UMG",
    "FF.ApplyFacilityCodexSixColumns20260919",
    "FF.ApplyBuildCatalogV2UMG",
    "FF.BuildFacilityWorldBadgeUMG",
    "FF.BuildFacilityRevenueUMG",
]


def path(package):
    return ROOT / "Content" / (package.removeprefix("/Game/") + ".uasset")


def digest(package):
    return hashlib.sha256(path(package).read_bytes()).hexdigest()


def snapshot(package):
    asset = unreal.EditorAssetLibrary.load_asset(package)
    if not asset:
        raise RuntimeError("Expected saved asset missing: " + package)
    result = {"sha256": digest(package), "widgets": {}}
    for widget in unreal.ObjectIterator(unreal.Widget):
        outer = widget.get_outer()
        if not outer or outer.get_class().get_name() != "WidgetTree" or outer.get_outer() != asset:
            continue
        row = {"class": widget.get_class().get_path_name(),
               "parent": widget.get_parent().get_name() if widget.get_parent() else None}
        slot = widget.get_editor_property("slot")
        if slot:
            row["slot_class"] = slot.get_class().get_name()
            for key in ("layout_data", "padding", "size", "horizontal_alignment", "vertical_alignment"):
                try:
                    value = slot.get_editor_property(key)
                    row[key] = value.export_text() if hasattr(value, "export_text") else str(value)
                except Exception:
                    pass
        result["widgets"][widget.get_name()] = row
    return result


world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world and world.get_path_name().startswith("/Engine/Maps/Entry."), "Use the temporary Entry world."
for package in EXISTING:
    backup = OUT / "before_assets" / path(package).relative_to(ROOT)
    if not backup.exists():
        assert digest(package) == BASELINE[package]["sha256"], "Asset changed since baseline: " + package
        backup.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path(package), backup)

protected = {package: digest(package) for package in BASELINE if package not in EXISTING}
for command in COMMANDS:
    unreal.log("FF_BUILD_UI_APPLY " + command)
    unreal.SystemLibrary.execute_console_command(world, command)

after = {package: snapshot(package) for package in EXISTING + NEW}
for package, original in protected.items():
    assert digest(package) == original == BASELINE[package]["sha256"], "Protected WBP changed: " + package

# Check the outer inspector shell and catalog/precision-placement anchor contracts.
for package, names in {
    EXISTING[0]: ["Root_Canvas", "Panel_Size", "Panel_ShadowSize", "Border_Panel"],
    EXISTING[3]: ["Border_PanelRoot", "Panel_Catalog", "Panel_PlacementCompact"],
}.items():
    for name in names:
        before_row = BASELINE[package]["widgets"].get(name)
        if before_row:
            assert after[package]["widgets"].get(name) == before_row, "Outer layout changed: " + name

# A second call must not re-save authored layouts or regenerate them.
for command in COMMANDS:
    unreal.SystemLibrary.execute_console_command(world, command)
for package, data in after.items():
    assert digest(package) == data["sha256"], "Migration is not idempotent: " + package
(OUT / "wbp-after.json").write_text(json.dumps(after, ensure_ascii=False, indent=2), encoding="utf-8")
(OUT / "migration-result.json").write_text(json.dumps({
    "status": "PASS", "commands": COMMANDS, "packages": EXISTING + NEW,
    "preserved_packages": list(protected), "idempotence": True, "outer_layout_preserved": True,
}, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("FF_BUILD_UI_APPROVED_DESIGN_PASS")
