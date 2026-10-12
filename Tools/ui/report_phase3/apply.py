"""기존 최종 결산에 만족도 열만 추가하고 날짜 기록 회귀 한 건을 확인한다."""
from pathlib import Path
import hashlib
import json
import re
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/ReportPhase3'
OUT.mkdir(parents=True, exist_ok=True)
SCREEN = '/Game/FestaFiesta/UI/Report/WBP_FF_FinalReportScreen'
ROW = '/Game/FestaFiesta/UI/Report/WBP_FF_FinalReportDayRow'
DAILY = '/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelReport'
LEDGER = '/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_DailyReportLedgerRow'
TARGETS = {
    ROW: ('HorizontalBox_DayCells', 'Text_Satisfaction', 'Text_AdmittedVisitors'),
    SCREEN: ('HorizontalBox_TableHeader', 'Text_SatisfactionHeader', 'Text_VisitorsHeader'),
}
FIELDS = (
    'visibility', 'is_enabled', 'render_opacity', 'render_transform', 'render_transform_pivot',
    'clipping', 'tool_tip_text', 'font', 'text', 'color_and_opacity', 'auto_wrap_text',
    'wrap_text_at', 'justification', 'shadow_offset', 'shadow_color_and_opacity',
    'min_desired_width', 'line_height_percentage', 'apply_line_height_to_bottom_line',
    'wrapping_policy', 'text_transform_policy', 'text_overflow_policy', 'brush',
    'background', 'brush_color', 'widget_style', 'padding', 'width_override',
    'height_override', 'min_desired_height', 'max_desired_width', 'max_desired_height',
    'override_width_override', 'override_height_override', 'override_min_desired_width',
    'override_min_desired_height', 'override_max_desired_width', 'override_max_desired_height',
)
SLOT_FIELDS = ('size', 'padding', 'horizontal_alignment', 'vertical_alignment', 'layout_data', 'auto_size', 'z_order')
TEST = 'FestaFiesta.UI.ViewModel.EconomyRelay.RecordedDailySatisfaction'
REPORT = {'status': 'running', 'visual_check': '실제 화면은 사용자 확인 예정'}


def digest(package):
    return hashlib.sha256((ROOT / 'Content' / (package[6:] + '.uasset')).read_bytes()).hexdigest()


def properties(obj, names):
    values = {}
    for name in names:
        try:
            value = obj.get_editor_property(name)
            values[name] = value.export_text() if hasattr(value, 'export_text') else str(value)
        except Exception:
            continue  # 위젯 종류에 실제로 있는 속성만 비교한다.
    return values


def widgets(bp):
    tree = unreal.find_object(bp, 'WidgetTree')
    assert tree, '기존 WidgetTree 없음'
    return {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget) if w.get_outer() == tree}


def snapshot(all_widgets):
    result = {}
    for name, widget in all_widgets.items():
        parent = widget.get_parent()
        record = {'class': widget.get_class().get_path_name(),
                  'parent': parent.get_name() if parent else None,
                  'properties': properties(widget, FIELDS)}
        slot = widget.get_editor_property('slot')
        if slot:
            record['slot_class'] = slot.get_class().get_path_name()
            record['slot'] = properties(slot, SLOT_FIELDS)
        if isinstance(widget, unreal.PanelWidget):
            record['children'] = [w.get_name() for w in widget.get_all_children()]
        result[name] = record
    return result


def gold_images(bp):
    visible = []
    for name, widget in widgets(bp).items():
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
            visible.append(name)
    return sorted(visible)


def write_report():
    (OUT / 'apply-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')


try:
    assert not unreal.EditorLevelLibrary.get_pie_worlds(False), 'PIE 중에는 적용하지 않습니다.'
    assets = {package: unreal.load_asset(package) for package in (ROW, SCREEN, DAILY, LEDGER)}
    assert all(assets.values()), '결산 원본 에셋 누락'
    protected = {package: digest(package) for package in (DAILY, LEDGER)}
    before = {package: snapshot(widgets(assets[package])) for package in TARGETS}
    before_row_class = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(SCREEN)).get_editor_property('day_row_class')
    REPORT['before'] = before
    unreal.SystemLibrary.execute_console_command(None, 'FF.ApplyFinalReportSatisfactionColumn')
    preserved = 0
    for package, (container, new_name, style_name) in TARGETS.items():
        current = widgets(assets[package])
        after = snapshot(current)
        assert isinstance(current.get(new_name), unreal.TextBlock), '만족도 열 생성 실패: ' + new_name
        assert current[new_name].get_parent() == current[container], '만족도 부모 불일치'
        original_order = [n for n in before[package][container]['children'] if n != new_name]
        assert after[container]['children'] == original_order + [new_name], '6열 순서 불일치'
        assert len(original_order) == 5, '원본 5열 불일치'
        for name, original in before[package].items():
            assert name in after, '기존 위젯 누락: ' + name
            expected = dict(original)
            if name == container:
                expected['children'] = original_order + [new_name]
            assert after[name] == expected, '기존 위젯 스타일·배치 변경: ' + name
            preserved += 1
        if new_name not in before[package]:
            assert after[new_name]['slot'] == after[style_name]['slot'], '새 열 슬롯 불일치'
            for field in ('font', 'color_and_opacity', 'justification'):
                assert after[new_name]['properties'][field] == after[style_name]['properties'][field], '새 열 스타일 불일치: ' + field
        unreal.BlueprintEditorLibrary.compile_blueprint(assets[package])
        assert unreal.EditorAssetLibrary.load_blueprint_class(package), '컴파일 클래스 없음: ' + package
        assert unreal.EditorAssetLibrary.save_loaded_asset(assets[package], only_if_is_dirty=True), '저장 실패: ' + package
    row_class = unreal.EditorAssetLibrary.load_blueprint_class(ROW)
    final_defaults = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(SCREEN))
    assert final_defaults.get_editor_property('day_row_class') == before_row_class == row_class, '일별 행 클래스 연결 변경'
    assert str(widgets(assets[SCREEN])['Text_SatisfactionHeader'].get_editor_property('text')) == '만족도'
    visible = {package: gold_images(bp) for package, bp in assets.items()}
    assert visible == {ROW: [], SCREEN: ['Image_FinalGold'], DAILY: ['Image_ReportGold'], LEDGER: []}, '보유 금화 외의 금액 아이콘 확인 필요'
    for package, sha256 in protected.items():
        assert digest(package) == sha256, '읽기 전용 일일 결산 에셋 변경: ' + package
    REPORT.update(status='passed', saved_assets=list(TARGETS), existing_widgets_preserved=preserved,
                  visible_gold_images=visible, day_row_class=row_class.get_path_name(),
                  after={p: snapshot(widgets(assets[p])) for p in TARGETS})
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
    unreal.log_error('REPORT_PHASE3_FAIL: ' + REPORT['error'])
finally:
    write_report()

# 날짜 누락·0점·최종일 매핑을 확인하는 단일 회귀만 같은 프로세스에서 실행한다.
started = time.monotonic()
log_path = ROOT / 'Saved/BrightGlobe/report_phase3_apply.log'
if REPORT['status'] == 'passed':
    world = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_world()
    unreal.SystemLibrary.execute_console_command(world, 'Automation RunTests ' + TEST)


def finish(delta_time):
    elapsed = time.monotonic() - started
    if elapsed < 10:
        return
    if REPORT['status'] == 'passed':
        text = log_path.read_text(encoding='utf-8', errors='replace') if log_path.exists() else ''
        matches = re.findall(r'Test Completed\. Result=\{([^}]+)\}.*?Path=\{([^}]+)\}', text)
        results = {name: state for state, name in matches if name == TEST}
        if TEST not in results and elapsed < 90:
            return
        REPORT['tests'] = results
        if results.get(TEST) != 'Success':
            REPORT.update(status='failed', error='날짜별 만족도 회귀 실패 또는 90초 시간 초과')
            unreal.log_error('REPORT_PHASE3_FAIL: ' + REPORT['error'])
        write_report()
    unreal.unregister_slate_post_tick_callback(exit_handle)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
exit_handle = unreal.register_slate_post_tick_callback(finish)
