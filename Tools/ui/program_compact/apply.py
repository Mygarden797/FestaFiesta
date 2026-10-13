"""프로그램 패널·시설 바로가기·오늘 편성 행만 적용한다. PIE/렌더 검증은 실행하지 않는다."""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import shutil
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/ProgramCompact'
OUT.mkdir(parents=True, exist_ok=True)
PANEL = '/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelProgram'
CHOICE = '/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepProgramCatalogChoice'
ROW = '/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepScheduledProgramRow'
HUD = '/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar'
PACKAGES = (PANEL, CHOICE, ROW)
COMMAND = 'FF.ApplyPrepProgramCompactDesign'
HIDDEN = ('Text_SelectedLabel', 'Text_SelectedProgram', 'Text_SummaryStatus',
          'HorizontalBox_ProgramCost', 'Text_RevenuePreview', 'Text_FacilitySelectionHint',
          'Text_FacilityNotice', 'Text_Instruction', 'Text_ProgramCompletion')
HEIGHT_FIELDS = ('height_override', 'min_desired_height', 'max_desired_height',
                 'override_height_override', 'override_min_desired_height', 'override_max_desired_height')
PROTECTED = {}
REPORT = {'status': 'running', 'command': COMMAND, 'saved_assets': [],
          'verification': '위젯 계층·바인딩·기존 치수·재실행 보존 확인. 실제 화면과 편성 조작은 사용자 에디터 확인 예정.'}


def asset_file(package):
    return ROOT / 'Content' / (package[6:] + '.uasset')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None


def widgets(bp):
    tree = unreal.find_object(bp, 'WidgetTree')
    assert tree, 'WidgetTree 누락: ' + bp.get_name()
    return {widget.get_name(): widget for widget in unreal.ObjectIterator(unreal.Widget) if widget.get_outer() == tree}


def snapshot(widget, fields):
    result = {}
    for name in fields:
        value = widget.get_editor_property(name)
        result[name] = value.export_text() if hasattr(value, 'export_text') else str(value)
    return result


def execute():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.SystemLibrary.execute_console_command(world, COMMAND)


def verify(original, heights):
    current = {}
    for package in PACKAGES:
        compiled = unreal.EditorAssetLibrary.load_blueprint_class(package)
        assert compiled, '컴파일 클래스 누락: ' + package
        current[package] = widgets(unreal.load_asset(package))
        for name, widget in original.get(package, {}).items():
            assert current[package].get(name) == widget, '기존 위젯 객체 변경/누락: ' + name
    panel, choice, row = current[PANEL], current[CHOICE], current[ROW]
    defaults = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(PANEL))
    assert defaults.get_editor_property('use_compact_layout'), '패널 compact 옵션 누락'
    assert defaults.get_editor_property('summary_row_class') == unreal.EditorAssetLibrary.load_blueprint_class(ROW), '오늘 편성 행 클래스 누락'
    for name in HIDDEN:
        assert panel[name].get_visibility() == unreal.SlateVisibility.COLLAPSED, '중복 그룹 표시: ' + name
    assert str(panel['Text_SummaryTitle'].get_text()) == '오늘 편성', '오른쪽 제목 불일치'
    assert isinstance(panel['Text_ScheduledCount'], unreal.TextBlock), '총 건수 바인딩 누락'
    assert isinstance(panel['VerticalBox_ScheduledPrograms'], unreal.VerticalBox), '오늘 편성 목록 바인딩 누락'
    assert panel['VerticalBox_ScheduledPrograms'].get_parent() == panel['ScrollBox_ScheduledPrograms'], '오늘 편성 스크롤 계층 오류'
    assert isinstance(panel['Text_ScheduledEmpty'], unreal.TextBlock), '미편성 안내 바인딩 누락'
    assert panel['Box_FacilityRow'].get_parent() == panel['VerticalBox_Catalog'], '운영 시설 선택이 카드 아래에 없음'
    assert panel['VerticalBox_Catalog'].get_child_index(panel['Box_FacilityRow']) > panel['VerticalBox_Catalog'].get_child_index(panel['Box_ProgramCards'])
    assert panel['VerticalBox_FacilityInstances'].get_parent() == panel['ScrollBox_FacilityInstances'], '기존 시설 선택 목록 변경'
    assert panel['Text_GoldAfter'].get_parent() == panel['HorizontalBox_Actions'], '잔액 표시가 하단에 없음'
    assert panel['Text_DailyNotice'].get_parent() == panel['Border_DailyRule'], '하루 운영 배지 계층 오류'
    assert panel['Border_DailyRule'].get_parent() == panel['HorizontalBox_Instruction'], '하루 운영 배지가 날짜 옆에 없음'
    assert str(panel['Text_DailyNotice'].get_text()) == '오늘만 운영', '하루 규칙 라벨 오류'
    assert str(panel['Text_DailyNotice'].get_editor_property('tool_tip_text')), '하루 규칙 상세 툴팁 누락'
    assert choice['Text_Subtitle'].get_visibility() == unreal.SlateVisibility.COLLAPSED, '반복 건축목록 설명이 표시됨'
    assert str(choice['Button_Root'].get_editor_property('tool_tip_text')) == '건축목록에서 보기', '바로가기 툴팁 누락'
    for name in ('Text_ProgramName', 'Text_FacilityName', 'Text_TimeRange'):
        assert isinstance(row[name], unreal.TextBlock), '오늘 편성 행 바인딩 누락: ' + name
    for name, value in heights.items():
        assert snapshot(panel[name], HEIGHT_FIELDS) == value, '헤더·하단 버튼 높이 변경: ' + name
    return {'panel_widget_count': len(panel), 'choice_widget_count': len(choice), 'scheduled_row_widget_count': len(row),
            'existing_widgets_preserved': sum(len(items) for items in original.values()),
            'header_footer_heights_preserved': True, 'facility_selection_widgets_preserved': True}


try:
    assert not unreal.EditorLevelLibrary.get_pie_worlds(False), 'PIE 중에는 적용하지 않습니다.'
    assert asset_file(PANEL).is_file() and asset_file(CHOICE).is_file(), '기존 프로그램 에셋 누락'
    targets = {asset_file(package) for package in PACKAGES}
    neighbors = set((ROOT / 'Content/FestaFiesta/UI/PrepHUD').rglob('*.uasset')) - targets
    neighbors.add(asset_file(HUD))
    PROTECTED = {path: digest(path) for path in neighbors}
    dirty = {package.get_name() for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    assert not dirty.intersection(PACKAGES), '프로그램 에셋의 미저장 변경을 먼저 저장해야 합니다.'
    original = {package: widgets(unreal.load_asset(package)) for package in PACKAGES if asset_file(package).is_file()}
    heights = {name: snapshot(original[PANEL][name], HEIGHT_FIELDS)
               for name in ('SizeBox_Header', 'SizeBox_ScheduleAction')}
    backup = OUT / 'Backups' / datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    backup.mkdir(parents=True, exist_ok=False)
    hashes_before = {}
    for package in PACKAGES:
        source = asset_file(package)
        hashes_before[package] = digest(source)
        if source.is_file():
            shutil.copy2(source, backup / source.name)
            assert digest(backup / source.name) == hashes_before[package], '백업 해시 불일치'
    REPORT.update(backup=str(backup), protected_count=len(PROTECTED), heights_before=heights, hashes_before=hashes_before)
    execute()
    REPORT['applied'] = verify(original, heights)
    hashes_after = {package: digest(asset_file(package)) for package in PACKAGES}
    assert all(hashes_after.values()), '대상 에셋 저장 누락'
    REPORT['saved_assets'] = [package for package in PACKAGES if hashes_after[package] != hashes_before[package]]
    execute()
    REPORT['repeated'] = verify(original, heights)
    assert {package: digest(asset_file(package)) for package in PACKAGES} == hashes_after, '재실행 시 파일 변경'
    REPORT.update(status='passed', hashes_after=hashes_after, repeat_file_hashes_unchanged=True)
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
finally:
    changed = [str(path.relative_to(ROOT)) for path, value in PROTECTED.items() if digest(path) != value]
    REPORT['protected_changes'] = changed
    if changed:
        REPORT.update(status='failed', protected_error='프로그램 대상 세 에셋 외 보호 파일 변경')
    (OUT / 'apply-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    if REPORT['status'] == 'passed':
        unreal.log('PROGRAM_COMPACT_APPLY_PASSED')
    else:
        unreal.log_error('PROGRAM_COMPACT_FAIL: ' + str(REPORT.get('error') or REPORT.get('protected_error')))

started = time.monotonic()


def finish(delta_time):
    if time.monotonic() - started < 2:
        return
    unreal.unregister_slate_post_tick_callback(exit_handle)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
exit_handle = unreal.register_slate_post_tick_callback(finish)
