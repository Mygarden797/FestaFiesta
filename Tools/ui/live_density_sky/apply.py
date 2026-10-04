"""P8 밀집 패널·범례 행·밀집 탭 크기를 적용한다. PIE/렌더는 실행하지 않는다."""
from datetime import datetime
from pathlib import Path
import json
import shutil
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/LiveDensitySky'
OUT.mkdir(parents=True, exist_ok=True)
PANEL = '/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelDensity'
ROW = '/Game/FestaFiesta/UI/LiveHUD/Rows/WBP_FF_LiveDensityLegendRow'
HUD = '/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveHudScreen'
KEY = 'FF.LiveDensitySky.Version'
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
    for path in (PANEL, ROW, HUD):
        source = ROOT / 'Content' / (path[6:] + '.uasset')
        if source.exists():
            destination = backup / source.relative_to(ROOT)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
        elif path != ROW:
            raise RuntimeError('기존 에셋 없음: ' + path)
    REPORT['backup'] = str(backup)
    bp, hud_bp = unreal.load_asset(PANEL), unreal.load_asset(HUD)
    before = {str(k): v.export_text() for k, v in defaults(HUD).get_editor_property('panel_layouts').items()}
    interval_before = defaults(PANEL).get_editor_property('refresh_interval_seconds')
    was_applied = unreal.EditorAssetLibrary.get_metadata_tag(bp, KEY) == '1'
    unreal.SystemLibrary.execute_console_command(None, 'FF.ApplyLiveDensitySkyDesign')
    assert unreal.EditorAssetLibrary.get_metadata_tag(bp, KEY) == '1', '밀집 패널 제작 실패'
    row_bp = unreal.load_asset(ROW)
    assert row_bp, '밀집 범례 행 WBP 없음'
    REPORT['panel_bindings'] = check_names(bp, {'Button_Close', 'Button_Toggle', 'Text_ToggleLabel',
        'Box_Legend', 'Text_Peak', 'Text_PeakGrade', 'Text_Congested', 'Text_CellSize',
        'Text_CellArea', 'Text_MissingNote', 'Text_HeatmapStatus', 'Scroll_Density'})
    REPORT['row_bindings'] = check_names(row_bp, {'Border_Row', 'Border_GradeColor',
        'Border_CurrentPeak', 'Text_Grade', 'Text_Range', 'Text_CurrentPeak'})
    if not was_applied:
        REPORT['changed_assets'] += [PANEL, ROW]
    if unreal.EditorAssetLibrary.get_metadata_tag(bp, KEY + '.Settings') != '1':
        cdo = defaults(PANEL)
        if not cdo.get_editor_property('legend_row_widget_class'):
            cdo.set_editor_property('legend_row_widget_class', unreal.EditorAssetLibrary.load_blueprint_class(ROW))
        unreal.EditorAssetLibrary.set_metadata_tag(bp, KEY + '.Settings', '1')
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
    assert defaults(PANEL).get_editor_property('legend_row_widget_class'), '범례 행 클래스 연결 없음'
    assert defaults(PANEL).get_editor_property('refresh_interval_seconds') == interval_before, '갱신 주기 변경'
    REPORT['legend_row_class'] = defaults(PANEL).get_editor_property('legend_row_widget_class').get_path_name()
    REPORT['refresh_interval_seconds'] = interval_before

    if unreal.EditorAssetLibrary.get_metadata_tag(hud_bp, KEY) != '1':
        cdo = defaults(HUD)
        table = dict(cdo.get_editor_property('panel_layouts'))
        if unreal.FF_LiveTab.DENSITY not in table:
            layout = unreal.FF_LivePanelLayout()
            layout.set_editor_property('width', 1100.0)
            layout.set_editor_property('height', 760.0)
            current = cdo.get_editor_property('default_panel_layout')
            layout.set_editor_property('dock', current.get_editor_property('dock'))
            layout.set_editor_property('offset', current.get_editor_property('offset'))
            table[unreal.FF_LiveTab.DENSITY] = layout
            cdo.set_editor_property('panel_layouts', table)
        unreal.EditorAssetLibrary.set_metadata_tag(hud_bp, KEY, '1')
        unreal.BlueprintEditorLibrary.compile_blueprint(hud_bp)
        assert unreal.EditorAssetLibrary.save_loaded_asset(hud_bp, only_if_is_dirty=False)
        REPORT['changed_assets'].append(HUD)
    after = {str(k): v.export_text() for k, v in defaults(HUD).get_editor_property('panel_layouts').items()}
    for key, value in before.items():
        assert after.get(key) == value, '기존 패널 편집값 변경: ' + key
    REPORT.update(status='passed', layouts=after)
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
    unreal.log_error(REPORT['error'])
finally:
    (OUT / 'apply-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.SystemLibrary.quit_editor()
