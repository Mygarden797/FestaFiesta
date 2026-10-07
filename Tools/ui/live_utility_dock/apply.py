"""P2 SNS·음악 도크의 새 WBP만 구성한다. 기존 HUD·맵 저장과 PIE/렌더는 실행하지 않는다."""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import shutil
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/LiveUtilityDock'
OUT.mkdir(parents=True, exist_ok=True)
SNS = '/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LiveSnsFlyout'
MUSIC = '/Game/FestaFiesta/UI/Audio/WBP_FF_PlaylistFlyout'
ROW = '/Game/FestaFiesta/UI/Audio/WBP_FF_PlaylistTrackRow'
DOCK = '/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveUtilityDock'
SNS_ENTRY = '/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LiveSnsPostEntry'
PREP_HUD = '/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar'
LIVE_HUD = '/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveHudScreen'
OLD_MUSIC = '/Game/FestaFiesta/UI/Audio/WBP_FF_PlaylistPlayer'
OLD_SNS = '/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelSNS'
REQUIRED = {
    SNS: ('FF_LivePanelSNS', {
        'Text_PanelTitle', 'Button_Close', 'Box_Posts', 'Scroll_Posts',
        'Button_FilterAll', 'Button_FilterPositive', 'Button_FilterNegative',
        'Text_FilterAll', 'Text_FilterPositive', 'Text_FilterNegative',
        'Button_NewPosts', 'Text_NewPosts', 'Text_Count', 'Text_Empty', 'Text_MissingNote'}),
    ROW: ('FF_PlaylistTrackRowWidget', {
        'Button_Track', 'Text_Index', 'Text_Title', 'Text_State'}),
    MUSIC: ('FF_PlaylistPlayerWidget', {
        'Panel_Player', 'Panel_List', 'Button_Close', 'Button_Prev', 'Button_PlayPause',
        'Button_Next', 'Button_AutoAdvance', 'Button_Shuffle', 'Text_TrackTitle',
        'Text_PlaybackState', 'Text_TrackIndex', 'Text_Elapsed', 'Text_Duration',
        'Image_Shuffle', 'Image_Repeat', 'Image_RepeatOne', 'Image_Play', 'Image_Pause',
        'ProgressBar_Track', 'ScrollBox_Tracks'}),
    DOCK: ('FF_LiveUtilityDock', {
        'Button_SNS', 'Button_Music', 'Panel_Dock', 'Panel_Flyout', 'SizeBox_SNS',
        'SizeBox_Music', 'Text_Unread', 'Text_SNSSelected', 'Text_MusicSelected'}),
}
COMMANDS = ('FF.LiveSnsFlyoutSky', 'FF.PlaylistFlyoutSky', 'FF.LiveUtilityDockSky')
REPORT = dict(status='running', visual_check='사용자 에디터 확인 예정',
              commands=[], compiled_widgets=[], protected_assets=[])


def asset_file(package):
    return ROOT / 'Content' / (package[6:] + '.uasset')


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def defaults(package):
    widget_class = unreal.EditorAssetLibrary.load_blueprint_class(package)
    assert widget_class, '위젯 클래스 없음: ' + package
    return unreal.get_default_object(widget_class)


def widget_names(blueprint):
    tree = unreal.find_object(blueprint, 'WidgetTree')
    return {widget.get_name() for widget in unreal.ObjectIterator(unreal.Widget)
            if widget.get_outer() == tree}


def layouts(package):
    return {str(key): value.export_text()
            for key, value in defaults(package).get_editor_property('panel_layouts').items()}


def object_path(value):
    return value.get_path_name() if hasattr(value, 'get_path_name') else str(value)


try:
    backup = OUT / ('Before-' + datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
    for package in REQUIRED:
        source = asset_file(package)
        if source.is_file():
            destination = backup / source.relative_to(ROOT)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
    REPORT['backup'] = str(backup)
    protected = {}
    for package in (PREP_HUD, LIVE_HUD, OLD_MUSIC, OLD_SNS, SNS_ENTRY):
        source = asset_file(package)
        assert source.is_file(), '기존 에셋 없음: ' + package
        protected[package] = sha256(source)
    before_layouts = {package: layouts(package) for package in (PREP_HUD, LIVE_HUD)}
    for command in COMMANDS:
        unreal.SystemLibrary.execute_console_command(None, command)
        REPORT['commands'].append(command)
    for package, (parent_name, required_names) in REQUIRED.items():
        blueprint = unreal.load_asset(package)
        assert blueprint, 'P2 WBP 제작 실패: ' + package
        assert isinstance(defaults(package), getattr(unreal, parent_name)), '부모 클래스 불일치: ' + package
        missing = required_names - widget_names(blueprint)
        assert not missing, '필수 위젯 누락: ' + package + ' ' + str(sorted(missing))
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        assert unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=True)
        REPORT['compiled_widgets'].append(package)

    sns_defaults = defaults(SNS)
    assert sns_defaults.get_editor_property('preserve_feed_while_closed'), 'SNS 닫힌 동안 새 글 보관 설정 누락'
    assert not sns_defaults.get_editor_property('allow_drag_to_move'), 'SNS 도크 패널은 부모 위치 설정을 사용해야 합니다.'
    entry_class = sns_defaults.get_editor_property('post_entry_class')
    assert entry_class, 'SNS 글 카드 클래스 연결 누락'
    REPORT['sns_post_entry_class'] = object_path(entry_class)
    REPORT['sns_display_limit'] = sns_defaults.get_editor_property('display_limit')

    music_defaults = defaults(MUSIC)
    assert music_defaults.get_editor_property('hosted_by_hud'), '음악 패널이 도크용으로 설정되지 않았습니다.'
    row_class = music_defaults.get_editor_property('track_row_widget_class')
    assert row_class, '음악 목록 행 클래스 연결 누락'
    REPORT['music_row_class'] = object_path(row_class)

    dock_defaults = defaults(DOCK)
    sns_panel_class = dock_defaults.get_editor_property('sns_panel_class')
    music_panel_class = dock_defaults.get_editor_property('music_panel_class')
    assert sns_panel_class, '도크의 SNS 패널 클래스 연결 누락'
    assert music_panel_class, '도크의 음악 패널 클래스 연결 누락'
    REPORT['dock_panel_classes'] = dict(sns=object_path(sns_panel_class), music=object_path(music_panel_class))
    REPORT['dock_layout'] = {name: str(dock_defaults.get_editor_property(name)) for name in (
        'dock_position', 'button_diameter', 'button_gap', 'panel_offset',
        'sns_panel_size', 'music_panel_size', 'reserved_bottom_space')}
    audio_settings = unreal.get_default_object(unreal.FF_AudioSettings)
    configured_player = object_path(audio_settings.get_editor_property('playlist_player_widget_class'))
    assert 'WBP_FF_LiveUtilityDock_C' in configured_player, 'Audio Settings의 Playlist Player Widget Class 도크 연결 누락'
    REPORT['configured_playlist_widget'] = configured_player

    for package, before in before_layouts.items():
        assert layouts(package) == before, '기존 HUD 탭 위치·크기 변경: ' + package
    for package, digest in protected.items():
        assert sha256(asset_file(package)) == digest, '기존 HUD/플레이어/글 카드 파일 변경: ' + package
    REPORT.update(status='passed', protected_assets=list(protected),
                  hud_layouts_preserved=True)
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
    unreal.log_error(REPORT['error'])
finally:
    (OUT / 'apply-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    # 첫 시작 틱에서 바로 종료하지 않고 에디터 도구 컨텍스트 초기화를 마친다.
    exit_after = time.monotonic() + 2.0
    def finish_after_startup(delta):
        if time.monotonic() < exit_after:
            return
        unreal.unregister_slate_post_tick_callback(exit_handle)
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    exit_handle = unreal.register_slate_post_tick_callback(finish_after_startup)
