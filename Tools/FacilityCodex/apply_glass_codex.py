# apply_glass_codex.py
# 역할: 시설 조합 도감과 보상 표시 작업에서 지정된 대상에 변경을 적용하고 결과를 확인한다.

"""Save build typography and the fullscreen glass collection, with repeat-run checks."""
import hashlib
import json
import re
import runpy
import shutil
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Saved/FacilityCodexGlass_20260921"
CONTENT = ROOT / "Content"
MATERIAL_ROOT = "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/Materials/"
MATERIAL_NAMES = ["M_FF_CodexGlass"] + ["MI_FF_CodexGlass_" + role for role in
    ("Screen", "Header", "Card", "Selection", "Detail", "Inset")]
CODEX = {
    "Screen": "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexScreen",
    "Card": "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexCard",
}
builder = (ROOT / "Source/FestaFiestaEditor/Private/Building/Presentation/FF_BuildTypographyBuilder.cpp").read_text(encoding="utf-8")
allowlist = re.search(r"const TCHAR\* Paths\[\] = \{(.*?)\};", builder, re.S)
assert allowlist, "Typography must expose its explicit construction-only allowlist"
TYPOGRAPHY = re.findall(r'TEXT\("(/Game/[^\"]+)"\)', allowlist.group(1))
assert len(TYPOGRAPHY) >= 20 and len(TYPOGRAPHY) == len(set(TYPOGRAPHY))
COMMANDS = ("FF.ApplyBuildModeTypography", "FF.ApplyFacilityCodexGlassTheme", "FF.ApplyFacilityCodexGlassScrollbars")


def file_for(path):
    assert path.startswith("/Game/")
    return CONTENT / (path.removeprefix("/Game/") + ".uasset")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def widgets(bp):
    return {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget)
            if w.get_outer() and w.get_outer().get_class().get_name() == "WidgetTree"
            and w.get_outer().get_outer() == bp}


def verify():
    for path in TYPOGRAPHY:
        bp = unreal.load_asset(path)
        assert bp and unreal.EditorAssetLibrary.get_metadata_tag(bp, "FF.BuildMode.Typography20260921.1") == "1", path
    for role, path in CODEX.items():
        bp = unreal.load_asset(path)
        assert bp
        tree = widgets(bp)
        assert sum(1 for w in tree.values() if not w.get_parent()) == 1, "Detached widgets: " + path
        for name in ("Image_Target", "Image_Partner"):
            assert tree[name].get_editor_property("discovery_material"), role + name
        for name, widget in tree.items():
            if isinstance(widget, unreal.TextBlock) and name not in (
                    "Text_Plus", "Text_RecipePlus", "Text_RewardClaimed", "Text_ClaimReward", "Text_RewardLocked"):
                font = widget.get_editor_property("font")
                obj = font.get_editor_property("font_object")
                # Symbol-only labels may intentionally retain the icon fallback font.
                label = str(widget.get_text()).strip()
                if label and any(char.isalnum() for char in label):
                    assert obj and obj.get_path_name().startswith("/Game/Asset/Font/"), role + ":" + name
        if role == "Screen":
            assert unreal.EditorAssetLibrary.get_metadata_tag(bp, "FF.FacilityCodex.GlassScrollbars20260921.1") == "1"
            assert "BackgroundBlur_Glass" in tree
            blur = tree["BackgroundBlur_Glass"]
            assert isinstance(blur, unreal.BackgroundBlur) and blur.get_editor_property("blur_strength") > 0
            assert tree["Button_TargetCatalog"] and tree["Button_PartnerCatalog"]
            assert tree["Progress_Discovery"]
        else:
            assert tree["Size_RewardState"].get_editor_property("width_override") == 72.0
            assert tree["Size_RewardState"].get_editor_property("height_override") == 32.0
            assert tree["Overlay_RewardState"].get_children_count() == 3
            for name in ("Button_ClaimReward", "Border_RewardClaimed", "Border_RewardLocked"):
                assert tree[name].get_parent() == tree["Overlay_RewardState"]
    for name in MATERIAL_NAMES:
        asset = unreal.load_asset(MATERIAL_ROOT + name)
        assert asset and unreal.EditorAssetLibrary.get_metadata_tag(asset, "FF.FacilityCodex.GlassSurfaceVersion") == "1"


world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world and world.get_path_name().startswith("/Engine/Maps/Entry.")
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
OUT.mkdir(parents=True, exist_ok=True)
owned = {file_for(path) for path in TYPOGRAPHY + list(CODEX.values()) + [MATERIAL_ROOT + name for name in MATERIAL_NAMES]}
for path in owned:
    if path.exists():
        backup = OUT / "before_assets" / path.relative_to(CONTENT)
        backup.parent.mkdir(parents=True, exist_ok=True)
        if not backup.exists():
            shutil.copy2(path, backup)
protected = {p: digest(p) for folder in (CONTENT / "FestaFiesta", CONTENT / "Asset/Font")
             for p in folder.rglob("*.uasset") if p not in owned}
runpy.run_path(str(ROOT / "Tools/FacilityCodex/create_glass_materials.py"), run_name="__main__")
for command in COMMANDS:
    unreal.SystemLibrary.execute_console_command(world, command)
verify()
saved = {path: digest(path) for path in owned}
runpy.run_path(str(ROOT / "Tools/FacilityCodex/create_glass_materials.py"), run_name="__main__")
for command in COMMANDS:
    unreal.SystemLibrary.execute_console_command(world, command)
verify()
for path, original in saved.items():
    assert digest(path) == original, "Repeat migration changed: " + str(path)
for path, original in protected.items():
    assert digest(path) == original, "Unrelated source asset changed: " + str(path)
(OUT / "migration-result.json").write_text(json.dumps({
    "status": "PASS", "idempotent": True, "typography_widget_blueprints": len(TYPOGRAPHY),
    "protected_asset_count": len(protected),
    "asset_sha256": {str(path.relative_to(ROOT)): value for path, value in saved.items()},
}, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("FF_CODEX_GLASS_VERIFY_PASS")
