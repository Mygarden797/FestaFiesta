"""음표 아이콘을 임포트하고 도크·음악 패널 아이콘 정리를 적용한다. 다른 HUD 에셋은 저장하지 않는다."""
from pathlib import Path
import time
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE = ROOT / 'SourceArt/UI/LiveHUD/LightTealParts_v1/Icons/Navigation/T_FF_LH_Icon_Music_White_256.png'
FOLDER = '/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Navigation'
NAME = 'T_FF_LH_Icon_Music_White_256'


def import_icon():
    path = FOLDER + '/' + NAME
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.log('FF_LIVE_DOCK_ICON_TEXTURE_REUSE ' + path)
        return True
    task = unreal.AssetImportTask()
    task.filename = str(SOURCE)
    task.destination_path = FOLDER
    task.destination_name = NAME
    task.automated = True
    task.replace_existing = False
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.EditorAssetLibrary.load_asset(path)
    if not isinstance(texture, unreal.Texture2D):
        unreal.log_error('FF_LIVE_DOCK_ICON_TEXTURE_FAIL ' + str(SOURCE))
        return False
    # 다른 LightTeal 아이콘과 같은 설정 (import_live_hud_light_teal.py).
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('srgb', True)
    texture.set_editor_property('never_stream', True)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    assert unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
    unreal.log('FF_LIVE_DOCK_ICON_TEXTURE_IMPORTED %s %dx%d' % (path, texture.blueprint_get_size_x(), texture.blueprint_get_size_y()))
    return True


try:
    if import_icon():
        unreal.SystemLibrary.execute_console_command(None, 'FF.ApplyLiveDockIconDesign')
except Exception as error:
    unreal.log_error('FF_LIVE_DOCK_ICON_SCRIPT_FAIL %s' % error)

exit_after = time.monotonic() + 2.0


def finish(_delta):
    if time.monotonic() < exit_after:
        return
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


handle = unreal.register_slate_post_tick_callback(finish)
