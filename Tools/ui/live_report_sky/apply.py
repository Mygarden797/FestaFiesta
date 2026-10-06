"""P4 하루 결산 WBP와 기존 기본 Report 자리만 적용한다. PIE/렌더는 실행하지 않는다."""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import shutil
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/LiveReportSky'
OUT.mkdir(parents=True, exist_ok=True)
PANEL = '/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelReport'
ROW = '/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_DailyReportLedgerRow'
HUD = '/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveHudScreen'
PREP_HUD = '/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar'
KEY = 'FF.LiveReportSky.Version'
REQUIRED = {
    PANEL: ('FF_LivePanelReport', {
        'Text_PanelTitle', 'Button_Close', 'Text_DayLabel', 'Text_TodayRevenue',
        'Text_TodayExpense', 'Text_TodayNetProfit', 'Text_Gold', 'Image_ReportGold',
        'Text_Refund', 'Text_LedgerNote', 'Box_EarnRows', 'Box_SpendRows',
        'Text_SimSatisfaction', 'Text_ScoreSatisfaction', 'Text_SatisfactionNote',
        'Text_WaitScore', 'Text_DensityScore', 'Text_PreferenceScore', 'Text_NeedScore',
        'Progress_WaitScore', 'Progress_DensityScore', 'Progress_PreferenceScore',
        'Progress_NeedScore', 'Text_DailyGoal', 'Text_DailyGoalState',
        'Button_Advance', 'Text_AdvanceLabel', 'Text_AdvanceReason'}),
    ROW: ('FF_DailyReportLedgerRowWidget', {'Text_Label', 'Text_Amount'}),
}
REPORT = dict(status='running', visual_check='사용자 에디터 확인 예정', compiled_widgets=[])


def asset_file(package):
    return ROOT / 'Content' / (package[6:] + '.uasset')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def defaults(package):
    widget_class = unreal.EditorAssetLibrary.load_blueprint_class(package)
    assert widget_class, '위젯 클래스 없음: ' + package
    return unreal.get_default_object(widget_class)


def widgets(blueprint):
    tree = unreal.find_object(blueprint, 'WidgetTree')
    return {widget.get_name(): widget for widget in unreal.ObjectIterator(unreal.Widget)
            if widget.get_outer() == tree}


def export_value(value):
    return value.export_text() if hasattr(value, 'export_text') else str(value)


def layouts(package):
    return {str(key): value.export_text()
            for key, value in defaults(package).get_editor_property('panel_layouts').items()}


def hud_settings():
    cdo = defaults(HUD)
    return {name: export_value(cdo.get_editor_property(name)) for name in (
        'default_panel_layout', 'bar_height', 'status_row_height', 'menu_row_height')}


def hud_tree():
    result = {}
    for name, widget in widgets(unreal.load_asset(HUD)).items():
        entry = dict(type=widget.get_class().get_name(),
                     parent=widget.get_parent().get_name() if widget.get_parent() else None)
        properties = ['visibility', 'render_opacity', 'render_transform']
        if isinstance(widget, unreal.TextBlock):
            properties += ['font', 'text', 'color_and_opacity', 'justification']
        elif isinstance(widget, unreal.Button):
            properties += ['widget_style']
        elif isinstance(widget, unreal.SizeBox):
            properties += ['height_override', 'width_override', 'min_desired_width', 'min_desired_height']
        elif isinstance(widget, unreal.Image):
            properties += ['brush', 'color_and_opacity']
        elif isinstance(widget, unreal.Border):
            properties += ['background', 'brush_color', 'padding']
        for prop in properties:
            entry[prop] = export_value(widget.get_editor_property(prop))
        slot = widget.get_editor_property('slot')
        if isinstance(slot, unreal.CanvasPanelSlot):
            entry['slot_layout'] = export_value(slot.get_editor_property('layout_data'))
        elif isinstance(slot, (unreal.HorizontalBoxSlot, unreal.VerticalBoxSlot)):
            entry['slot_size'] = export_value(slot.get_editor_property('size'))
            entry['slot_padding'] = export_value(slot.get_editor_property('padding'))
        result[name] = entry
    return result


def visible_gold_images(blueprint):
    result = []
    for name, widget in widgets(blueprint).items():
        if not isinstance(widget, unreal.Image):
            continue
        resource = widget.get_editor_property('brush').get_editor_property('resource_object')
        if not resource or 'Gold' not in resource.get_name():
            continue
        parent = widget
        while parent:
            if parent.get_visibility() in (unreal.SlateVisibility.COLLAPSED, unreal.SlateVisibility.HIDDEN):
                break
            parent = parent.get_parent()
        else:
            result.append(name)
    return sorted(result)


try:
    backup = OUT / ('Before-' + datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
    for package in (PANEL, ROW, HUD):
        source = asset_file(package)
        if source.is_file():
            destination = backup / source.relative_to(ROOT)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
    REPORT['backup'] = str(backup)
    prep_hash = digest(asset_file(PREP_HUD))
    prep_layouts = layouts(PREP_HUD)
    before_layouts = layouts(HUD)
    before_tree = hud_tree()
    before_settings = hud_settings()
    hud_bp = unreal.load_asset(HUD)
    already_layout = unreal.EditorAssetLibrary.get_metadata_tag(hud_bp, KEY + '.Layout') == '1'

    unreal.SystemLibrary.execute_console_command(None, 'FF.ApplyLiveReportSkyDesign')
    panel_bp = unreal.load_asset(PANEL)
    assert unreal.EditorAssetLibrary.get_metadata_tag(panel_bp, KEY) == '1', '결산 제작 실패'
    for package, (parent_name, required) in REQUIRED.items():
        blueprint = unreal.load_asset(package)
        assert blueprint and isinstance(defaults(package), getattr(unreal, parent_name)), 'WBP 부모 클래스 불일치: ' + package
        missing = required - set(widgets(blueprint))
        assert not missing, '필수 결산 바인딩 누락: ' + str(sorted(missing))
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        assert unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=True)
        REPORT['compiled_widgets'].append(package)
    ledger_row_class = defaults(PANEL).get_editor_property('ledger_row_class')
    assert ledger_row_class == unreal.EditorAssetLibrary.load_blueprint_class(ROW), '결산 내역 행 클래스 연결 누락'
    gold_images = visible_gold_images(panel_bp)
    assert gold_images == ['Image_ReportGold'], '보유 금화 이외의 금화 아이콘 확인 필요: ' + str(gold_images)
    REPORT['visible_currency_images'] = gold_images

    # 최초 적용에서 옛 기본 크기만 옮긴다. 사용자 지정 폭·위치와 재실행 편집은 유지한다.
    layout_changed = False
    if not already_layout:
        cdo = defaults(HUD)
        table = dict(cdo.get_editor_property('panel_layouts'))
        old = table.get(unreal.FF_LiveTab.REPORT)
        is_legacy = old is None or (
            old.dock == unreal.FF_LivePanelDock.CENTER and old.width in (500.0, 520.0)
            and old.height == 620.0 and old.offset.x == 0.0 and old.offset.y == 0.0)
        if is_legacy:
            layout = unreal.FF_LivePanelLayout()
            layout.set_editor_property('dock', unreal.FF_LivePanelDock.CENTER)
            layout.set_editor_property('width', 1140.0)
            layout.set_editor_property('height', 800.0)
            layout.set_editor_property('offset', unreal.Vector2D(0.0, 0.0))
            table[unreal.FF_LiveTab.REPORT] = layout
            cdo.set_editor_property('panel_layouts', table)
            layout_changed = True
        unreal.EditorAssetLibrary.set_metadata_tag(hud_bp, KEY + '.Layout', '1')
        unreal.BlueprintEditorLibrary.compile_blueprint(hud_bp)
        assert unreal.EditorAssetLibrary.save_loaded_asset(hud_bp, only_if_is_dirty=True)
    after_layouts = layouts(HUD)
    for key, value in before_layouts.items():
        if key != str(unreal.FF_LiveTab.REPORT) or not layout_changed:
            assert after_layouts.get(key) == value, '기존 탭 Layout 변경: ' + key
    assert hud_tree() == before_tree, '기존 HUD 위젯·행 높이·스타일 변경'
    assert hud_settings() == before_settings, '기존 HUD 높이·기본 Layout 설정 변경'
    assert digest(asset_file(PREP_HUD)) == prep_hash and layouts(PREP_HUD) == prep_layouts, '셋업 HUD 변경'
    bar_widgets = widgets(hud_bp)
    REPORT.update(status='passed', report_layout_changed=layout_changed,
                  report_layout=after_layouts.get(str(unreal.FF_LiveTab.REPORT)),
                  hud_tree_preserved=True, other_layouts_preserved=True,
                  menu_row_height=bar_widgets['SizeBox_MenuRow'].get_editor_property('height_override'),
                  status_row_height=bar_widgets['SizeBox_StatusRow'].get_editor_property('height_override'))
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
    unreal.log_error(REPORT['error'])
finally:
    (OUT / 'apply-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    exit_after = time.monotonic() + 2.0
    def finish_after_startup(delta):
        if time.monotonic() < exit_after:
            return
        unreal.unregister_slate_post_tick_callback(exit_handle)
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    exit_handle = unreal.register_slate_post_tick_callback(finish_after_startup)
