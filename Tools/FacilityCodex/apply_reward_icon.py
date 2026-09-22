"""Import the transparent gift and migrate only the codex reward button in idle Entry."""
import hashlib
import json
import shutil
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
UI_ROOTS = (ROOT / "Content/FestaFiesta/UI", ROOT / "Content/FestaFiesta/Building/FF_WBP_Building")
OUT = ROOT / "Saved/FacilityCodexRewardIcon_20260921"
SOURCE = ROOT / "Design/UXUI/FacilityCodex/Generated/reward_gift_v1.png"
CARD = "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexCard"
ICON = "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/Icons/T_FF_Codex_RewardGift"
CARD_FILE = ROOT / "Content/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexCard.uasset"
ICON_FILE = ROOT / "Content/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/Icons/T_FF_Codex_RewardGift.uasset"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify():
    bp = unreal.load_asset(CARD)
    widgets = {widget.get_name(): widget for widget in unreal.ObjectIterator(unreal.Widget)
               if widget.get_outer() and widget.get_outer().get_class().get_name() == "WidgetTree"
               and widget.get_outer().get_outer() == bp}
    assert unreal.EditorAssetLibrary.get_metadata_tag(bp, "FF.FacilityCodex.RewardIcon20260921.1") == "1"
    has_status = "Border_RewardClaimed" in widgets
    assert widgets["Text_ClaimReward"].get_visibility() == (
        unreal.SlateVisibility.HIT_TEST_INVISIBLE if has_status else unreal.SlateVisibility.COLLAPSED)
    assert widgets["Button_ClaimReward"].get_content() == widgets["Overlay_ClaimReward"]
    assert widgets["Image_ClaimReward"].get_editor_property("brush").get_editor_property("resource_object") == unreal.load_asset(ICON)
    assert widgets["Size_ClaimRewardIcon"].get_editor_property("width_override") == (24.0 if has_status else 28.0)
    assert widgets["Size_ClaimRewardIcon"].get_editor_property("height_override") == (24.0 if has_status else 28.0)


world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world and world.get_path_name().startswith("/Engine/Maps/Entry."), "Use idle Entry map"
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
assert SOURCE.is_file()
OUT.mkdir(parents=True, exist_ok=True)
backup = OUT / CARD_FILE.name
if not backup.exists():
    shutil.copy2(CARD_FILE, backup)
protected = {path: digest(path) for base in UI_ROOTS for path in base.rglob("*.uasset")
             if path not in (CARD_FILE, ICON_FILE)}
if not unreal.EditorAssetLibrary.does_asset_exist(ICON):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(SOURCE))
    task.set_editor_property("destination_path", ICON.rsplit("/", 1)[0])
    task.set_editor_property("destination_name", ICON.rsplit("/", 1)[1])
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(ICON)
    assert isinstance(texture, unreal.Texture2D)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("max_texture_size", 128)
    assert unreal.EditorAssetLibrary.save_loaded_asset(texture)
unreal.SystemLibrary.execute_console_command(world, "FF.ApplyFacilityCodexRewardIcon")
verify()
saved = digest(CARD_FILE)
unreal.SystemLibrary.execute_console_command(world, "FF.ApplyFacilityCodexRewardIcon")
verify()
assert digest(CARD_FILE) == saved, "Second migration changed card bytes"
for path, original in protected.items():
    assert digest(path) == original, "Unexpected UI asset change: " + str(path)
(OUT / "migration-result.json").write_text(json.dumps({
    "status": "PASS", "source": str(SOURCE), "source_sha256": digest(SOURCE),
    "card_sha256": saved, "icon_sha256": digest(ICON_FILE),
    "preserved_other_ui_count": len(protected), "idempotent": True,
}, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("FF_CODEX_REWARD_ICON_VERIFY_PASS")
