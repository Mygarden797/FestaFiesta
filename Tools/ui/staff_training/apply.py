"""P7 스태프 숙련도 WBP와 기존 두 스태프 창의 탭만 적용한다. PIE/렌더는 실행하지 않는다."""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import shutil
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/StaffTraining'
OUT.mkdir(parents=True, exist_ok=True)
PREP = '/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelStaff'
LIVE = '/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelStaff'
TRAINING = '/Game/FestaFiesta/UI/Staff/WBP_FF_StaffTraining'
ROW = '/Game/FestaFiesta/UI/Staff/WBP_FF_StaffPromotionRow'
PREP_HUD = '/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar'
LIVE_HUD = '/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveHudScreen'
KEY = 'FF.StaffTraining.Version'
HOST_BINDINGS = {
    'Button_StaffOverview': unreal.Button,
    'Button_StaffTraining': unreal.Button,
    'Switcher_StaffContent': unreal.WidgetSwitcher,
    'Widget_StaffTraining': unreal.FF_StaffTrainingWidget,
    'Indicator_StaffOverview': unreal.Border,
    'Indicator_StaffTraining': unreal.Border,
    'Text_PanelTitle': unreal.TextBlock,
    'Button_Close': unreal.Button,
}
REQUIRED = {
    ROW: ('FF_StaffPromotionRowWidget', {
        'Text_StaffName': unreal.TextBlock, 'Text_StaffKind': unreal.TextBlock,
        'Text_StaffLevel': unreal.TextBlock, 'Text_PromotionStatus': unreal.TextBlock,
        'Text_Promote': unreal.TextBlock, 'Button_Promote': unreal.Button}),
    TRAINING: ('FF_StaffTrainingWidget', {
        'Text_TotalStaff': unreal.TextBlock, 'Text_BaseStaff': unreal.TextBlock,
        'Text_HiredStaff': unreal.TextBlock, 'Text_RosterNote': unreal.TextBlock,
        'Text_RosterEmpty': unreal.TextBlock, 'Text_PromotionResult': unreal.TextBlock,
        'Box_StaffRoster': unreal.VerticalBox}),
    PREP: ('FF_PrepPanelStaff', HOST_BINDINGS),
    LIVE: ('FF_LivePanelStaff', HOST_BINDINGS),
}
REPARENTED = {PREP: {'ScrollBox_Body', 'Border_Footer'}, LIVE: {'VerticalBox_Content'}}
CDO_PROPERTIES = {
    PREP: ('allow_drag_to_move', 'daily_notice_text', 'default_selected_count',
           'staff_capacity_text_format', 'staff_breakdown_text_format',
           'selected_count_text_format', 'hire_unit_cost_text_format',
           'gold_value_text_format', 'hire_action_text_format'),
    LIVE: ('allow_drag_to_move', 'missing_value_text', 'count_text_format',
           'capacity_text_format', 'breakdown_text_format', 'idle_note_text',
           'all_busy_note_text', 'no_staff_note_text', 'checking_text',
           'inconsistent_text_format', 'response_active_text_format',
           'response_idle_text', 'response_unknown_text', 'hire_cost_text_format',
           'hire_note_text', 'assignment_guide_text', 'unassigned_guide_text_format',
           'capacity_note_text', 'capacity_unknown_text', 'breakdown_unknown_text',
           'breakdown_inconsistent_text', 'capacity_inconsistent_text',
           'response_idle_icon', 'response_active_icon', 'response_unknown_icon',
           'assignment_row_class'),
}
REPORT = dict(status='running', visual_check='사용자 에디터 확인 예정',
              command='FF.ApplyStaffTrainingDesign', compiled_widgets=[])


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
    if hasattr(value, 'export_text'):
        return value.export_text()
    return value.get_path_name() if hasattr(value, 'get_path_name') else str(value)


def layouts(package):
    return {str(key): value.export_text()
            for key, value in defaults(package).get_editor_property('panel_layouts').items()}


def settings(package):
    cdo = defaults(package)
    return {name: export_value(cdo.get_editor_property(name)) for name in CDO_PROPERTIES[package]}


def slot_values(slot):
    properties = ()
    if isinstance(slot, unreal.CanvasPanelSlot):
        properties = ('layout_data', 'auto_size', 'z_order')
    elif isinstance(slot, (unreal.VerticalBoxSlot, unreal.HorizontalBoxSlot)):
        properties = ('size', 'padding', 'horizontal_alignment', 'vertical_alignment')
    elif isinstance(slot, (unreal.BorderSlot, unreal.OverlaySlot, unreal.ScrollBoxSlot)):
        properties = ('padding', 'horizontal_alignment', 'vertical_alignment')
    return {name: export_value(slot.get_editor_property(name)) for name in properties}


def widget_values(widget_table, names, moved_names):
    result = {}
    for name in names:
        widget = widget_table[name]
        entry = dict(type=widget.get_class().get_name())
        properties = ['visibility', 'render_opacity', 'render_transform', 'render_transform_pivot']
        if isinstance(widget, unreal.TextBlock):
            properties += ['font', 'text', 'color_and_opacity', 'justification']
        elif isinstance(widget, unreal.Button):
            properties += ['widget_style']
        elif isinstance(widget, unreal.ProgressBar):
            properties += ['widget_style', 'fill_color_and_opacity', 'percent']
        elif isinstance(widget, unreal.SizeBox):
            properties += ['height_override', 'width_override', 'min_desired_width', 'min_desired_height']
        elif isinstance(widget, unreal.Image):
            properties += ['brush', 'color_and_opacity']
        elif isinstance(widget, unreal.Border):
            properties += ['background', 'brush_color', 'padding']
        for prop in properties:
            entry[prop] = export_value(widget.get_editor_property(prop))
        # 현황 탭으로 옮기는 본문/푸터만 부모가 바뀐다. 슬롯의 크기·여백은 그대로다.
        if name not in moved_names:
            entry['parent'] = widget.get_parent().get_name() if widget.get_parent() else None
        entry['slot'] = slot_values(widget.get_editor_property('slot'))
        result[name] = entry
    return result


try:
    backup = OUT / ('Before-' + datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
    for package in REQUIRED:
        source = asset_file(package)
        if source.is_file():
            destination = backup / source.relative_to(ROOT)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
    REPORT['backup'] = str(backup)
    protected = {package: digest(asset_file(package)) for package in (PREP_HUD, LIVE_HUD)}
    before_layouts = {package: layouts(package) for package in protected}
    hosts_before = {}
    for package in (PREP, LIVE):
        blueprint = unreal.load_asset(package)
        assert blueprint and isinstance(defaults(package), getattr(unreal, REQUIRED[package][0])), '기존 스태프 부모 클래스 불일치: ' + package
        table = widgets(blueprint)
        names = set(table)
        applied = unreal.EditorAssetLibrary.get_metadata_tag(blueprint, KEY) == '1'
        moved = set() if applied else REPARENTED[package]
        assert moved <= names, '이동할 기존 스태프 본문 없음: ' + package
        hosts_before[package] = dict(names=names, applied=applied, moved=moved,
                                     values=widget_values(table, names, moved), settings=settings(package))

    unreal.SystemLibrary.execute_console_command(None, REPORT['command'])
    for package, (parent_name, required) in REQUIRED.items():
        blueprint = unreal.load_asset(package)
        assert blueprint and isinstance(defaults(package), getattr(unreal, parent_name)), '스태프 WBP 제작/부모 클래스 확인 실패: ' + package
        assert unreal.EditorAssetLibrary.get_metadata_tag(blueprint, KEY) == '1', '숙련도 디자인 적용 실패: ' + package
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        table = widgets(blueprint)
        for name, widget_type in required.items():
            assert isinstance(table.get(name), widget_type), '필수 숙련도 바인딩 누락/타입 불일치: ' + package + ' ' + name
        if package in hosts_before:
            before = hosts_before[package]
            assert before['names'] <= set(table), '기존 스태프 위젯 누락: ' + package
            assert widget_values(table, before['names'], before['moved']) == before['values'], '기존 스태프 스타일·크기·슬롯 변경: ' + package
            assert settings(package) == before['settings'], '기존 스태프 CDO 설정 변경: ' + package
            for name in before['moved']:
                assert table[name].get_parent().get_name() == 'Staff_OverviewContent', '기존 본문 현황 탭 연결 누락: ' + name
            assert table['Widget_StaffTraining'].get_class() == unreal.EditorAssetLibrary.load_blueprint_class(TRAINING), '공유 숙련도 WBP 연결 누락: ' + package
        assert unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=True)
        REPORT['compiled_widgets'].append(package)
    row_class = defaults(TRAINING).get_editor_property('staff_row_class')
    assert row_class == unreal.EditorAssetLibrary.load_blueprint_class(ROW), '스태프 승급 행 클래스 연결 누락'
    for package, before in protected.items():
        assert layouts(package) == before_layouts[package], '기존 HUD 패널 위치·크기 변경: ' + package
        assert digest(asset_file(package)) == before, '기존 HUD 파일 변경: ' + package
    REPORT.update(status='passed', existing_widget_values_preserved=True,
                  existing_settings_preserved=True, hud_layouts_preserved=True,
                  protected_assets=list(protected), staff_row_class=export_value(row_class),
                  host_existing_widget_counts={package: len(before['names']) for package, before in hosts_before.items()},
                  staff_layouts={package: {key: value for key, value in values.items() if 'STAFF' in key.upper()}
                                 for package, values in before_layouts.items()})
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
