"""피날레 원본 4개를 신규 임포트하고 P1 위젯을 구성한다. PIE/렌더는 실행하지 않는다."""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import shutil
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/PrepFinaleSky'
OUT.mkdir(parents=True, exist_ok=True)
TEXTURES = '/Game/FestaFiesta/UI/PrepHUD/Finale/Textures/'
PANEL = '/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelFinale'
CARD = '/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepFinaleCard'
CONFIRM = '/Game/FestaFiesta/UI/PrepHUD/Finale/WBP_FF_FinaleConfirmDialog'
HUD = '/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar'
HASHES = {
    'C': 'b01c08633e3bff190e45082802625275f98b58379fc63e8044387eeabe718746',
    'B': '2b36657a878043c010030aa6d365711ecdeaea8f2dd348625d2be109658a0886',
    'A': '8260d4357720ea4cec0cef795cc6f319db4a709b184b9bffa4be00ac15b77430',
    'S': '5af447850db5aa45866cb66fdd27d6907ed0c8e8f689d0435688855e07e6dc87',
}
REQUIRED = {
    PANEL: ('FF_PrepPanelFinale', {
        'Text_PanelTitle', 'Button_Close', 'Box_FinaleCards', 'Text_StageNotice',
        'Text_OnceNotice', 'Text_SelectedFinale', 'Text_PurchaseCost',
        'Text_GoldAfterValue', 'Text_GoldAfterLabel', 'Text_BlockReason',
        'Text_PurchaseLabel', 'Button_Later', 'Button_Purchase'}),
    CARD: ('FF_PrepFinaleCard', {
        'Button_Root', 'Text_Label', 'Text_Badge', 'Image_Preview', 'Text_Grade',
        'Text_Description', 'Text_Cost', 'Text_Bonus', 'Border_Badge'}),
    CONFIRM: ('FF_FinaleConfirmDialog', {
        'Text_Title', 'Text_Body', 'Button_Cancel', 'Button_Confirm',
        'Image_Preview', 'Text_Grade', 'Text_Bonus', 'Text_Cost',
        'Text_GoldAfter', 'Text_BlockReason', 'Button_Close'}),
}
REPORT = dict(status='running', visual_check='사용자 에디터 확인 예정',
              imported_textures=[], preserved_textures=[], compiled_widgets=[])


def asset_file(package):
    return ROOT / 'Content' / (package[6:] + '.uasset')


def defaults(package):
    widget_class = unreal.EditorAssetLibrary.load_blueprint_class(package)
    assert widget_class, '위젯 클래스 없음: ' + package
    return unreal.get_default_object(widget_class)


def widget_names(blueprint):
    tree = unreal.find_object(blueprint, 'WidgetTree')
    return {widget.get_name() for widget in unreal.ObjectIterator(unreal.Widget)
            if widget.get_outer() == tree}


def layouts():
    return {str(key): value.export_text()
            for key, value in defaults(HUD).get_editor_property('panel_layouts').items()}


def import_missing_texture(grade):
    package = TEXTURES + 'T_FF_Finale_' + grade
    if unreal.EditorAssetLibrary.does_asset_exist(package):
        texture = unreal.load_asset(package)
        assert isinstance(texture, unreal.Texture2D), '피날레 경로에 다른 에셋 존재: ' + package
        REPORT['preserved_textures'].append(package)
        return
    source = ROOT / 'SourceArt/UI/Finale' / ('finale-' + grade + '.png')
    assert source.is_file(), '제공 원본 없음: ' + str(source)
    assert hashlib.sha256(source.read_bytes()).hexdigest() == HASHES[grade], '제공 원본과 다른 파일: ' + str(source)
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', str(source))
    task.set_editor_property('destination_path', TEXTURES.rstrip('/'))
    task.set_editor_property('destination_name', 'T_FF_Finale_' + grade)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', False)
    task.set_editor_property('save', False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(package)
    assert isinstance(texture, unreal.Texture2D), '피날레 텍스처 임포트 실패: ' + grade
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('srgb', True)
    texture.set_editor_property('never_stream', True)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    assert unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
    REPORT['imported_textures'].append(package)


try:
    # 최초 적용 전 사용자 위젯을 보관한다. 다른 맵이나 UI 에셋을 저장하지 않는다.
    backup = OUT / ('Before-' + datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
    for package in (*REQUIRED.keys(), HUD):
        source = asset_file(package)
        if source.is_file():
            destination = backup / source.relative_to(ROOT)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
    REPORT['backup'] = str(backup)
    before_layouts = layouts()
    assert 'Tab_Finale' in widget_names(unreal.load_asset(HUD)), '기존 HUD에 피날레 탭이 없습니다.'
    existing_panel = unreal.load_asset(PANEL) if unreal.EditorAssetLibrary.does_asset_exist(PANEL) else None
    had_finale_layout = bool(existing_panel and 'Box_FinaleCards' in widget_names(existing_panel))
    for grade in HASHES:
        import_missing_texture(grade)
    unreal.SystemLibrary.execute_console_command(None, 'FF.ApplyPrepFinaleSkyDesign')
    for package, (parent_name, required_names) in REQUIRED.items():
        blueprint = unreal.load_asset(package)
        assert blueprint, '피날레 WBP 제작 실패: ' + package
        assert isinstance(defaults(package), getattr(unreal, parent_name)), '부모 클래스 불일치: ' + package
        missing = required_names - widget_names(blueprint)
        assert not missing, '필수 위젯 누락: ' + package + ' ' + str(sorted(missing))
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        assert unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=True)
        REPORT['compiled_widgets'].append(package)
    panel_defaults = defaults(PANEL)
    assert panel_defaults.get_editor_property('card_widget_class'), '피날레 카드 클래스 연결 누락'
    assert panel_defaults.get_editor_property('confirm_dialog_class'), '피날레 확인창 클래스 연결 누락'
    presentations = panel_defaults.get_editor_property('tier_presentations')
    assert len(presentations) == 4, 'C/B/A/S 표시 설정 누락'
    for grade, presentation in presentations.items():
        assert presentation.get_editor_property('preview_texture'), '등급 그림 연결 누락: ' + str(grade)
    hud_defaults = defaults(HUD)
    mapped = hud_defaults.get_editor_property('panel_classes').get(unreal.FF_PrepHudTab.FINALE)
    REPORT['finale_panel_override'] = str(mapped)
    REPORT['finale_panel_folder'] = hud_defaults.get_editor_property('panel_class_folder')
    REPORT['finale_panel_name_format'] = hud_defaults.get_editor_property('panel_class_name_format')
    after_layouts = layouts()
    for key, value in before_layouts.items():
        # 최초 생성의 Finale 자리만 변경할 수 있다. 재실행은 Finale 편집값도 보존한다.
        if had_finale_layout or key != str(unreal.FF_PrepHudTab.FINALE):
            assert after_layouts.get(key) == value, '기존 패널 위치·크기 변경: ' + key
    REPORT.update(status='passed', layouts=after_layouts,
                  designer_edits_preserved=had_finale_layout)
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
    unreal.log_error(REPORT['error'])
finally:
    (OUT / 'apply-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    # 첫 시작 틱 안에서 종료하면 에디터 도구 컨텍스트 초기화와 충돌한다.
    # 저장을 마친 뒤 초기화 틱이 끝나도록 기다렸다가 정상 종료한다.
    exit_after = time.monotonic() + 2.0
    def finish_after_startup(delta):
        if time.monotonic() < exit_after:
            return
        unreal.unregister_slate_post_tick_callback(exit_handle)
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    exit_handle = unreal.register_slate_post_tick_callback(finish_after_startup)
