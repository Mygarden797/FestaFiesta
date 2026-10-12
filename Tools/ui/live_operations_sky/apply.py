"""P2 운영 화면만 제작·저장. PIE/렌더/전체 회귀검사는 실행하지 않는다."""
from datetime import datetime
from pathlib import Path
import json
import shutil
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/LiveOperationsSky'
OUT.mkdir(parents=True, exist_ok=True)
PANEL = '/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelOperations'
HUD = '/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveHudScreen'
KEY = 'FF.LiveOperationsSky.Version'
REPORT = dict(status='running', visual_check='사용자 에디터 확인 예정', changed_assets=[])


def defaults(path):
    return unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(path))


try:
    backup = OUT / ('Before-' + datetime.now().strftime('%Y%m%d-%H%M%S'))
    for path in (PANEL, HUD):
        source = ROOT / 'Content' / (path[6:] + '.uasset')
        destination = backup / source.relative_to(ROOT)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
    REPORT['backup'] = str(backup)
    bp = unreal.load_asset(PANEL)
    hud_bp = unreal.load_asset(HUD)
    before = {str(k): v.export_text() for k, v in defaults(HUD).get_editor_property('panel_layouts').items()}
    was_applied = unreal.EditorAssetLibrary.get_metadata_tag(bp, KEY) == '1'
    unreal.SystemLibrary.execute_console_command(None, 'FF.ApplyLiveOperationsSkyDesign')
    assert unreal.EditorAssetLibrary.get_metadata_tag(bp, KEY) == '1', '운영 패널 제작 실패'
    # 네이티브 제작기의 SaveNewBlueprint가 컴파일 오류를 검사한 뒤 저장한다.
    # WidgetBlueprint.Status는 Python에서 읽을 수 없는 보호 속성이다.
    tree = unreal.find_object(bp, 'WidgetTree')
    names = {w.get_name() for w in unreal.ObjectIterator(unreal.Widget) if w.get_outer() == tree}
    required = {'Text_Day', 'Text_GoalPercent', 'Text_SatisfactionNote', 'Text_TodayRevenue',
                'Text_Gold', 'Text_Satisfaction', 'Text_Safety', 'Text_GoalProgress',
                'Text_GoalDetail', 'Bar_GoalProgress', 'Button_Close'}
    assert required <= names, '필수 바인딩 누락: ' + str(required - names)
    if not was_applied:
        REPORT['changed_assets'].append(PANEL)

    # 운영만 크기를 추가한다. 기존 개별값이 있다면 디자이너 편집으로 보고 보존한다.
    if unreal.EditorAssetLibrary.get_metadata_tag(hud_bp, KEY) != '1':
        cdo = defaults(HUD)
        table = dict(cdo.get_editor_property('panel_layouts'))
        if unreal.FF_LiveTab.OPERATIONS not in table:
            layout = unreal.FF_LivePanelLayout()
            layout.set_editor_property('width', 1100.0)
            layout.set_editor_property('height', 620.0)
            current = cdo.get_editor_property('default_panel_layout')
            layout.set_editor_property('dock', current.get_editor_property('dock'))
            layout.set_editor_property('offset', current.get_editor_property('offset'))
            table[unreal.FF_LiveTab.OPERATIONS] = layout
            cdo.set_editor_property('panel_layouts', table)
        unreal.EditorAssetLibrary.set_metadata_tag(hud_bp, KEY, '1')
        unreal.BlueprintEditorLibrary.compile_blueprint(hud_bp)
        assert unreal.EditorAssetLibrary.save_loaded_asset(hud_bp, only_if_is_dirty=False)
        REPORT['changed_assets'].append(HUD)
    after = {str(k): v.export_text() for k, v in defaults(HUD).get_editor_property('panel_layouts').items()}
    for key, value in before.items():
        assert after.get(key) == value, '기존 패널 편집값 변경: ' + key
    REPORT.update(status='passed', layouts=after, bindings_present=sorted(required))
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
    unreal.log_error(REPORT['error'])
finally:
    (OUT / 'apply-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.SystemLibrary.quit_editor()
