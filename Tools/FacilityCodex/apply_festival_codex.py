"""Save the festival codex, thumbnail shaders and catalog destination highlight."""
import hashlib
import json
import runpy
import shutil
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Saved/FacilityCodexFestival_20260921"
UI = ROOT / "Content/FestaFiesta/Building/FF_WBP_Building"
UI_ROOTS = (ROOT / "Content/FestaFiesta/UI", UI)
ASSETS = {
    "Card": "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexCard",
    "Screen": "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexScreen",
    "CatalogTile": "/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepBuildTile",
}
MATERIAL_NAMES = [
    "M_FF_Codex_DiscoveryThumbnail", "M_FF_CodexFestivalSurface",
    "MI_FF_CodexFestival_Header", "MI_FF_CodexFestival_Paper", "MI_FF_CodexFestival_Pass", "MI_FF_CodexFestival_Selection",
    "M_FF_CodexCatalogFocus", "MI_FF_CodexCatalogFocus_Rim", "MI_FF_CodexCatalogFocus_Badge",
]
MATERIAL_ROOT = "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/Materials/"
SCRIPTS = ("create_thumbnail_materials.py", "create_festival_materials.py", "create_catalog_focus_materials.py")
COMMANDS = ("FF.ApplyFacilityCodexFestivalTheme", "FF.ApplyCodexCatalogFocus")


def asset_file(path):
    return ROOT / "Content" / (path.removeprefix("/Game/") + ".uasset")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def widgets(bp):
    result = {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget)
              if w.get_outer() and w.get_outer().get_class().get_name() == "WidgetTree"
              and w.get_outer().get_outer() == bp}
    assert len([w for w in result.values() if not w.get_parent()]) == 1, "Detached widgets: " + bp.get_name()
    return result


def verify():
    bps = {role: unreal.load_asset(path) for role, path in ASSETS.items()}
    tree = {role: widgets(bp) for role, bp in bps.items()}
    discovery = unreal.load_asset(MATERIAL_ROOT + "M_FF_Codex_DiscoveryThumbnail")
    assert discovery and discovery.get_editor_property("material_domain") == unreal.MaterialDomain.MD_UI
    for role in ("Card", "Screen"):
        assert unreal.EditorAssetLibrary.get_metadata_tag(bps[role], "FF.FacilityCodex.FestivalTheme20260921.1") == "1"
        for side in ("Target", "Partner"):
            assert tree[role]["Image_" + side].get_editor_property("discovery_material") == discovery
    screen = tree["Screen"]
    assert str(screen["Text_Title"].get_text()) == "축제 조합 도감"
    assert isinstance(screen["Progress_Discovery"], unreal.ProgressBar)
    assert "미발견" in str(screen["Text_VisualLegend"].get_text())
    assert screen["Button_TargetCatalog"].get_parent() == screen["Horizontal_Ingredients"]
    assert screen["Button_PartnerCatalog"].get_parent() == screen["Horizontal_Ingredients"]
    card = tree["Card"]
    assert card["Size_RewardState"].get_editor_property("width_override") == 72.0
    assert card["Size_RewardState"].get_editor_property("height_override") == 32.0
    assert card["Overlay_RewardState"].get_children_count() == 3
    for name in ("Button_ClaimReward", "Border_RewardClaimed", "Border_RewardLocked"):
        assert card[name].get_parent() == card["Overlay_RewardState"]
    tile = tree["CatalogTile"]
    assert unreal.EditorAssetLibrary.get_metadata_tag(bps["CatalogTile"], "FF.FacilityCodex.CatalogFocus20260921.1") == "1"
    assert tile["Overlay_CodexCatalogFocus"].get_children_count() == 3
    for name in ("Image_CodexFocusRim", "Border_CodexFocusBadge"):
        assert tile[name].get_visibility() == unreal.SlateVisibility.COLLAPSED
    assert str(tile["Text_CodexFocusBadge"].get_text()) == "도감에서 찾은 시설"
    for name in MATERIAL_NAMES:
        assert unreal.load_asset(MATERIAL_ROOT + name), name


world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world and world.get_path_name().startswith("/Engine/Maps/Entry."), "Use idle Entry map"
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
OUT.mkdir(parents=True, exist_ok=True)
owned = [asset_file(p) for p in ASSETS.values()] + [asset_file(MATERIAL_ROOT + name) for name in MATERIAL_NAMES]
for path in owned:
    if path.exists():
        backup = OUT / "before_assets" / path.relative_to(UI)
        backup.parent.mkdir(parents=True, exist_ok=True)
        if not backup.exists():
            shutil.copy2(path, backup)
protected = {p: digest(p) for base in UI_ROOTS for p in base.rglob("*.uasset") if p not in owned}
for script in SCRIPTS:
    runpy.run_path(str(ROOT / "Tools/FacilityCodex" / script), run_name="__main__")
for command in COMMANDS:
    unreal.SystemLibrary.execute_console_command(world, command)
verify()
saved = {p: digest(p) for p in owned}
for script in SCRIPTS:
    runpy.run_path(str(ROOT / "Tools/FacilityCodex" / script), run_name="__main__")
for command in COMMANDS:
    unreal.SystemLibrary.execute_console_command(world, command)
verify()
for path, original in saved.items():
    assert digest(path) == original, "Repeated migration changed: " + str(path)
for path, original in protected.items():
    assert digest(path) == original, "Unexpected UI edit: " + str(path)
(OUT / "migration-result.json").write_text(json.dumps({
    "status": "PASS", "asset_sha256": {str(p.relative_to(ROOT)): h for p, h in saved.items()},
    "preserved_other_ui_count": len(protected), "idempotent": True,
}, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("FF_CODEX_FESTIVAL_VERIFY_PASS")
