"""P6 SNS 패널·기존 글 카드·SNS 탭 크기만 적용. PIE/렌더를 실행하지 않는다."""
from datetime import datetime
from pathlib import Path
import json
import shutil
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/LiveSnsSky'
OUT.mkdir(parents=True, exist_ok=True)
PANEL = '/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelSNS'
ENTRY = '/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LiveSnsPostEntry'
HUD = '/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveHudScreen'
KEY = 'FF.LiveSnsSky.Version'
REPORT = dict(status='running', visual_check='사용자 에디터 확인 예정', changed_assets=[])


def defaults(path):
    return unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(path))


def check_names(bp, required):
    tree = unreal.find_object(bp, 'WidgetTree')
    names = {w.get_name() for w in unreal.ObjectIterator(unreal.Widget) if w.get_outer() == tree}
    assert required <= names, '필수 위젯 누락: ' + str(required - names)
    return sorted(required)


try:
    backup = OUT / ('Before-' + datetime.now().strftime('%Y%m%d-%H%M%S'))
    for path in (PANEL, ENTRY, HUD):
        source = ROOT / 'Content' / (path[6:] + '.uasset')
        assert source.exists(), '기존 에셋 없음: ' + path
        destination = backup / source.relative_to(ROOT)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
    REPORT['backup'] = str(backup)
    bp, entry_bp, hud_bp = [unreal.load_asset(path) for path in (PANEL, ENTRY, HUD)]
    before = {str(k): v.export_text() for k, v in defaults(HUD).get_editor_property('panel_layouts').items()}
    display_limit = defaults(PANEL).get_editor_property('display_limit')
    entry_class = defaults(PANEL).get_editor_property('post_entry_class')
    entry_class_path = entry_class.get_path_name() if entry_class else None
    pending_assets = [path for path, asset in ((PANEL, bp), (ENTRY, entry_bp))
        if unreal.EditorAssetLibrary.get_metadata_tag(asset, KEY) != '1']
    unreal.SystemLibrary.execute_console_command(None, 'FF.ApplyLiveSnsSkyDesign')
    for asset in (bp, entry_bp):
        assert unreal.EditorAssetLibrary.get_metadata_tag(asset, KEY) == '1', 'SNS 제작 실패: ' + asset.get_name()
    REPORT['changed_assets'] += pending_assets
    REPORT['panel_bindings'] = check_names(bp, {'Button_Close', 'Box_Posts', 'Scroll_Posts',
        'Button_FilterAll', 'Button_FilterPositive', 'Button_FilterNegative',
        'Text_FilterAll', 'Text_FilterPositive', 'Text_FilterNegative', 'Button_NewPosts',
        'Text_NewPosts', 'Text_Count', 'Text_Empty', 'Text_MissingNote'})
    REPORT['entry_bindings'] = check_names(entry_bp, {'Border_Card', 'Border_Sentiment',
        'Text_Author', 'Text_Body', 'Text_Sentiment', 'Text_SentimentSymbol'})
    if not defaults(PANEL).get_editor_property('post_entry_class'):
        defaults(PANEL).set_editor_property('post_entry_class', unreal.EditorAssetLibrary.load_blueprint_class(ENTRY))
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
    assert defaults(PANEL).get_editor_property('display_limit') == display_limit, '표시 한도 변경'
    if entry_class_path:
        assert defaults(PANEL).get_editor_property('post_entry_class').get_path_name() == entry_class_path, '기존 글 카드 클래스 변경'

    if unreal.EditorAssetLibrary.get_metadata_tag(hud_bp, KEY) != '1':
        cdo = defaults(HUD)
        table = dict(cdo.get_editor_property('panel_layouts'))
        if unreal.FF_LiveTab.SNS not in table:
            layout = unreal.FF_LivePanelLayout()
            layout.set_editor_property('width', 960.0)
            layout.set_editor_property('height', 700.0)
            current = cdo.get_editor_property('default_panel_layout')
            layout.set_editor_property('dock', current.get_editor_property('dock'))
            layout.set_editor_property('offset', current.get_editor_property('offset'))
            table[unreal.FF_LiveTab.SNS] = layout
            cdo.set_editor_property('panel_layouts', table)
        unreal.EditorAssetLibrary.set_metadata_tag(hud_bp, KEY, '1')
        unreal.BlueprintEditorLibrary.compile_blueprint(hud_bp)
        assert unreal.EditorAssetLibrary.save_loaded_asset(hud_bp, only_if_is_dirty=False)
        REPORT['changed_assets'].append(HUD)
    after = {str(k): v.export_text() for k, v in defaults(HUD).get_editor_property('panel_layouts').items()}
    for key, value in before.items():
        assert after.get(key) == value, '기존 패널 편집값 변경: ' + key
    REPORT.update(status='passed', layouts=after, display_limit=display_limit,
        post_entry_class=defaults(PANEL).get_editor_property('post_entry_class').get_path_name())
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
    unreal.log_error(REPORT['error'])
finally:
    (OUT / 'apply-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.SystemLibrary.quit_editor()
