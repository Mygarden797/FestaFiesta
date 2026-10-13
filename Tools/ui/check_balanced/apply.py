"""점검 시안의 두 WBP만 적용하고 바인딩·주변 에셋·재실행 보존을 확인한다."""
from pathlib import Path
from datetime import datetime
import hashlib
import json
import shutil
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/CheckBalanced'
OUT.mkdir(parents=True, exist_ok=True)
PANEL = '/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelCheck'
ROW = '/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepRecommendedFacilityRow'
HUD = '/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar'
PACKAGES = (PANEL, ROW)
COMMAND = 'FF.ApplyPrepCheckBalancedDesign'
REPORT = {'status': 'running', 'command': COMMAND}
PROTECTED = {}
HEIGHTS = ('height_override', 'min_desired_height', 'max_desired_height',
           'override_height_override', 'override_min_desired_height', 'override_max_desired_height')


def asset_file(package):
    return ROOT / 'Content' / (package[6:] + '.uasset')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def widgets(package):
    tree = unreal.find_object(unreal.load_asset(package), 'WidgetTree')
    assert tree, 'WidgetTree 누락'
    return {widget.get_name(): widget for widget in unreal.ObjectIterator(unreal.Widget) if widget.get_outer() == tree}


def properties(widget, names):
    result = {}
    for name in names:
        value = widget.get_editor_property(name)
        result[name] = value.export_text() if hasattr(value, 'export_text') else str(value)
    return result


def execute():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.SystemLibrary.execute_console_command(world, COMMAND)


def verify(original, row_data, heights):
    current = {package: widgets(package) for package in PACKAGES}
    for package in PACKAGES:
        assert unreal.EditorAssetLibrary.load_blueprint_class(package), '컴파일 클래스 누락'
        assert set(original[package]).issubset(current[package]), '기존 위젯 이름 누락'
    panel, row = current[PANEL], current[ROW]
    defaults = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(PANEL))
    assert defaults.get_editor_property('use_balanced_layout'), '새 레이아웃 옵션 누락'
    metrics = panel['HorizontalBox_BalancedMetrics']
    assert [child.get_name() for child in metrics.get_all_children()] == [
        'SizeBox_WeatherCard', 'SizeBox_DailyGoalCard', 'SizeBox_ExpectedVisitorsCard'], '상단 세 카드 순서 오류'
    for child in metrics.get_all_children():
        assert not child.get_editor_property('override_width_override'), '상단 고정 폭이 남음'
        assert child.slot.get_editor_property('size').size_rule == unreal.SlateSizeRule.FILL, '동일 비율 채우기 누락'
    for name in ('WrapBox_Metrics', 'SizeBox_CapacitySummary', 'SizeBox_GoldCard',
                 'VerticalBox_AppliedAdmission', 'VerticalBox_AdmissionPreview'):
        assert panel[name].get_visibility() == unreal.SlateVisibility.COLLAPSED, '중복 영역 표시: ' + name
    for name in ('Text_AdmissionFeeMin', 'Text_AdmissionFeeMax', 'Text_CapacityRisk',
                 'Text_CapacityDetail', 'Text_AdmissionFeeState', 'Text_AdmissionPreviewDelta'):
        assert panel[name].get_parent(), '상태 표시 바인딩 분리됨: ' + name
    for name, data in row_data.items():
        assert properties(panel[name], tuple(data)) == data, '시설 원본 데이터 변경: ' + name
    assert len(panel['WrapBox_RecommendedRows'].get_all_children()) == 8, '시설 종류 누락'
    assert row['Button_Place'].get_content(), '권장 시설 바로가기 콘텐츠 누락'
    for name, data in heights.items():
        assert properties(panel[name], HEIGHTS) == data, '헤더·하단 버튼 높이 변경: ' + name
    return {'panel_widgets': len(panel), 'row_widgets': len(row), 'original_names_preserved': True,
            'header_footer_heights_preserved': True, 'recommended_kinds': 8}


try:
    assert not unreal.EditorLevelLibrary.get_pie_worlds(False), 'PIE 중 적용 불가'
    targets = {asset_file(package) for package in PACKAGES}
    protected = set((ROOT / 'Content/FestaFiesta/UI/PrepHUD').rglob('*.uasset')) - targets
    protected.add(asset_file(HUD))
    PROTECTED = {path: digest(path) for path in protected}
    original = {package: widgets(package) for package in PACKAGES}
    row_data = {name: properties(widget, ('kind', 'enabled', 'title_text', 'description_text', 'action_text', 'icon_brush'))
                for name, widget in original[PANEL].items() if name.startswith('RecommendedRow_')}
    heights = {name: properties(original[PANEL][name], HEIGHTS)
               for name in ('SizeBox_Header', 'SizeBox_ContinueBuild', 'SizeBox_OpenGate')}
    backup = OUT / 'Backups' / datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    backup.mkdir(parents=True)
    for path in targets:
        shutil.copy2(path, backup / path.name)
    REPORT['backup'] = str(backup)
    execute()
    REPORT['applied'] = verify(original, row_data, heights)
    hashes = {package: digest(asset_file(package)) for package in PACKAGES}
    execute()
    REPORT['repeated'] = verify(original, row_data, heights)
    assert hashes == {package: digest(asset_file(package)) for package in PACKAGES}, '재실행 시 에셋 변경'
    REPORT.update(status='passed', hashes=hashes, repeat_unchanged=True)
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
finally:
    REPORT['protected_changes'] = [str(path) for path, value in PROTECTED.items() if digest(path) != value]
    if REPORT['protected_changes']:
        REPORT.update(status='failed', error='점검 두 에셋 외 변경 발생')
    REPORT['protected_count'] = len(PROTECTED)
    (OUT / 'apply-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    if REPORT['status'] == 'passed':
        unreal.log('CHECK_BALANCED_APPLY_PASSED')
    else:
        unreal.log_error('CHECK_BALANCED_FAIL: ' + REPORT.get('error', ''))
    unreal.SystemLibrary.quit_editor()
