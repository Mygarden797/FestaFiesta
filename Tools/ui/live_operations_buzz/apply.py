"""P6 기존 운영 패널에 화제성 카드만 추가한다. PIE·렌더·전체 검증은 실행하지 않는다."""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import shutil
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/LiveOperationsBuzz'
OUT.mkdir(parents=True, exist_ok=True)
PANEL = '/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelOperations'
PREP_HUD = '/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar'
LIVE_HUD = '/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveHudScreen'
KEY = 'FF.LiveOperationsBuzz.Version'
REQUIRED = {
    'Ops_ProgressColumns': unreal.HorizontalBox,
    'Ops_GoalBorder': unreal.Border,
    'Ops_BuzzBorder': unreal.Border,
    'Text_BuzzValue': unreal.TextBlock,
    'Text_BuzzChange': unreal.TextBlock,
    'Text_BuzzNote': unreal.TextBlock,
    'Bar_Buzz': unreal.ProgressBar,
    'Text_PanelTitle': unreal.TextBlock,
    'Button_Close': unreal.Button,
    'Text_GoalProgress': unreal.TextBlock,
    'Bar_GoalProgress': unreal.ProgressBar,
}
CDO_PROPERTIES = (
    'allow_drag_to_move', 'missing_value_text', 'day_text_format',
    'goal_progress_text_format', 'goal_percent_text_format',
    'goal_remaining_text_format', 'goal_remaining_no_bonus_text_format',
    'goal_reached_text_format', 'goal_bonus_paid_text_format',
    'goal_reached_no_bonus_text', 'goal_unknown_text',
    'satisfaction_note_text', 'footer_note_text',
    'buzz_value_text_format', 'buzz_increase_text_format', 'buzz_decrease_text_format',
    'buzz_unchanged_text', 'buzz_note_text', 'buzz_unavailable_text', 'buzz_interp_speed',
)
REPORT = dict(status='running', visual_check='사용자 에디터 확인 예정',
              command='FF.ApplyLiveOperationsBuzzDesign', compiled_widgets=[])


def asset_file(package):
    return ROOT / 'Content' / (package[6:] + '.uasset')


def sha256(path):
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


def current_settings():
    cdo = defaults(PANEL)
    return {name: export_value(cdo.get_editor_property(name)) for name in CDO_PROPERTIES}


def slot_values(slot):
    properties = ()
    if isinstance(slot, unreal.CanvasPanelSlot):
        properties = ('layout_data', 'auto_size', 'z_order')
    elif isinstance(slot, (unreal.VerticalBoxSlot, unreal.HorizontalBoxSlot)):
        properties = ('size', 'padding', 'horizontal_alignment', 'vertical_alignment')
    elif isinstance(slot, (unreal.BorderSlot, unreal.OverlaySlot)):
        properties = ('padding', 'horizontal_alignment', 'vertical_alignment')
    return {name: export_value(slot.get_editor_property(name)) for name in properties}


def existing_widget_values(widget_table, names, ignore_goal_slot):
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
        # 새 가로 행으로 옮기는 목표 카드의 직접 슬롯만 달라진다.
        if not (ignore_goal_slot and name == 'Ops_GoalBorder'):
            entry['parent'] = widget.get_parent().get_name() if widget.get_parent() else None
            entry['slot'] = slot_values(widget.get_editor_property('slot'))
        result[name] = entry
    return result


try:
    source = asset_file(PANEL)
    assert source.is_file(), '기존 운영 에셋 없음: ' + PANEL
    backup = OUT / ('Before-' + datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
    destination = backup / source.relative_to(ROOT)
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)
    REPORT['backup'] = str(backup)
    protected = {package: sha256(asset_file(package)) for package in (PREP_HUD, LIVE_HUD)}
    before_layouts = {package: layouts(package) for package in protected}
    blueprint = unreal.load_asset(PANEL)
    assert isinstance(defaults(PANEL), unreal.FF_LivePanelOperations), '운영 부모 클래스 불일치'
    before_widgets = widgets(blueprint)
    before_names = set(before_widgets)
    assert 'Ops_GoalBorder' in before_names, '기존 일일 목표 카드 없음'
    was_applied = unreal.EditorAssetLibrary.get_metadata_tag(blueprint, KEY) == '1'
    before_values = existing_widget_values(before_widgets, before_names, not was_applied)
    before_settings = current_settings()
    old_goal_slot = slot_values(before_widgets['Ops_GoalBorder'].get_editor_property('slot'))

    unreal.SystemLibrary.execute_console_command(None, REPORT['command'])
    blueprint = unreal.load_asset(PANEL)
    assert unreal.EditorAssetLibrary.get_metadata_tag(blueprint, KEY) == '1', '화제성 영역 구성 실패'
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    after_widgets = widgets(blueprint)
    for name, widget_type in REQUIRED.items():
        assert isinstance(after_widgets.get(name), widget_type), '필수 운영 바인딩 누락/타입 불일치: ' + name
    assert before_names <= set(after_widgets), '기존 운영 위젯이 사라졌습니다: ' + str(sorted(before_names - set(after_widgets)))
    assert existing_widget_values(after_widgets, before_names, not was_applied) == before_values, '기존 운영 위젯 스타일·크기·위치 변경'
    assert current_settings() == before_settings, '기존 운영 CDO 설정 변경'
    if not was_applied:
        assert after_widgets['Ops_GoalBorder'].get_parent() == after_widgets['Ops_ProgressColumns'], '목표 카드의 가로 배치 연결 누락'
        assert slot_values(after_widgets['Ops_ProgressColumns'].get_editor_property('slot')) == old_goal_slot, '기존 목표 카드 바깥 여백·정렬 변경'
    assert unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=True)
    REPORT['compiled_widgets'].append(PANEL)
    for package, before in protected.items():
        assert layouts(package) == before_layouts[package], '기존 HUD 탭 위치·크기 변경: ' + package
        assert sha256(asset_file(package)) == before, '기존 HUD 파일 변경: ' + package
    REPORT.update(status='passed', required_bindings=sorted(REQUIRED),
                  existing_widget_count=len(before_names), existing_widget_values_preserved=True,
                  existing_settings_preserved=True, hud_layouts_preserved=True,
                  already_applied=was_applied, protected_assets=list(protected),
                  buzz_defaults={name: value for name, value in current_settings().items() if name.startswith('buzz_')})
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
