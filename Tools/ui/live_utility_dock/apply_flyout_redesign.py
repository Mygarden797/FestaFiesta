"""SNS·음악 펼침 패널 승인안(2026-10-07) 적용. 새 아이콘 8종을 임포트하고 `FF.ApplyLiveFlyoutRedesign` 을 실행한다.

도크 버튼·하단 HUD·다른 패널은 저장하지 않는다. 이미 새 구조인 WBP 는 건너뛴다(Designer 편집 보존).
"""
from pathlib import Path
import time
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE = ROOT / 'SourceArt/UI/LiveHUD/LightTealParts_v1/Icons/Actions'
FOLDER = '/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Actions'
ICONS = ['Shuffle', 'Repeat', 'RepeatOne', 'Equalizer', 'PlayFill', 'PauseFill', 'NextFill', 'PreviousFill']


def import_icon(key):
    name = 'T_FF_LH_Icon_%s_White_256' % key
    path = FOLDER + '/' + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.log('FF_LIVE_FLYOUT_ICON_REUSE ' + path)
        return True
    task = unreal.AssetImportTask()
    task.filename = str(SOURCE / (name + '.png'))
    task.destination_path = FOLDER
    task.destination_name = name
    task.automated = True
    task.replace_existing = False
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.EditorAssetLibrary.load_asset(path)
    if not isinstance(texture, unreal.Texture2D):
        unreal.log_error('FF_LIVE_FLYOUT_ICON_FAIL ' + task.filename)
        return False
    # 다른 LightTeal 아이콘과 같은 설정.
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('srgb', True)
    texture.set_editor_property('never_stream', True)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    assert unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
    unreal.log('FF_LIVE_FLYOUT_ICON_IMPORTED ' + path)
    return True


try:
    if all(import_icon(key) for key in ICONS):
        unreal.SystemLibrary.execute_console_command(None, 'FF.ApplyLiveFlyoutRedesign')
except Exception as error:
    unreal.log_error('FF_LIVE_FLYOUT_SCRIPT_FAIL %s' % error)

exit_after = time.monotonic() + 2.0


def finish(_delta):
    if time.monotonic() < exit_after:
        return
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


handle = unreal.register_slate_post_tick_callback(finish)
