"""점검 패널·권장 시설 행만 부분 적용한다. PIE·렌더·자동화 테스트는 실행하지 않는다."""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import shutil
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/CheckCompact'
OUT.mkdir(parents=True, exist_ok=True)
PANEL = '/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelCheck'
ROW = '/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepRecommendedFacilityRow'
HUD = '/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar'
PACKAGES = (PANEL, ROW)
COMMAND = 'FF.ApplyPrepCheckCompactDesign'
KINDS = ('Lighting', 'Food', 'Toilet', 'Entertainment', 'Rest', 'Drink', 'FirstAid', 'FireSafety')
TITLES = ('조명 없음', '음식 없음', '화장실 없음', '즐길거리 없음', '휴게 없음', '음료 없음', '응급 없음', '소방 없음')
HIDDEN = ('SizeBox_GoldCard', 'Text_RequiredFacilitiesCaption', 'Button_RequiredFacilities',
          'Box_GoalBonusRow', 'Box_GoalBonusRow_Rule_Size', 'Text_EstimateNotice', 'Border_AdmissionPreviewRevenue')
HEIGHT_FIELDS = ('height_override', 'min_desired_height', 'max_desired_height',
                 'override_height_override', 'override_min_desired_height', 'override_max_desired_height')
PROTECTED = {}
REPORT = {'status': 'running', 'command': COMMAND, 'saved_assets': [],
          'verification': '위젯 계층·원본 데이터·치수·재실행 보존 확인. 실제 화면은 사용자 에디터 확인 예정.'}


def asset_file(package):
    return ROOT / 'Content' / (package[6:] + '.uasset')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


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


def row_data(widget):
    return snapshot(widget, ('kind', 'enabled', 'title_text', 'description_text', 'action_text', 'icon_brush'))


def execute():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.SystemLibrary.execute_console_command(world, COMMAND)


def verify(original, original_rows, heights):
    current = {}
    for package in PACKAGES:
        compiled = unreal.EditorAssetLibrary.load_blueprint_class(package)
        assert compiled, '컴파일 클래스 누락: ' + package
        current[package] = widgets(unreal.load_asset(package))
        for name, widget in original[package].items():
            assert name in current[package], '기존 위젯 이름 누락: ' + name
            # 행 BP 컴파일은 패널에 들어 있는 행 인스턴스를 재생성할 수 있다.
            if package == PANEL and name in original_rows:
                assert row_data(current[package][name]) == original_rows[name], '행 원본 데이터 변경: ' + name
            else:
                assert current[package][name] == widget, '기존 위젯 객체 변경: ' + name
    panel, row = current[PANEL], current[ROW]
    defaults = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(PANEL))
    assert defaults.get_editor_property('use_compact_layout'), '패널 compact 옵션 누락'
    for name in HIDDEN:
        assert panel[name].get_visibility() == unreal.SlateVisibility.COLLAPSED, '중복 그룹 표시: ' + name
    wrap = panel['WrapBox_RecommendedRows']
    children = wrap.get_all_children()
    assert isinstance(wrap, unreal.WrapBox) and len(children) == 8, '권장 카드 컨테이너 오류'
    assert [item.get_name() for item in children] == ['RecommendedRow_' + kind for kind in KINDS], '권장 카드 종류·순서 오류'
    assert len({str(item.get_editor_property('kind')) for item in children}) == 8, '권장 카드 종류 중복'
    for index, item in enumerate(children):
        assert item.get_editor_property('compact_presentation'), '카드 compact 옵션 누락'
        assert str(item.get_editor_property('compact_title_text')) == TITLES[index], '짧은 제목 불일치'
        assert str(item.get_editor_property('compact_action_text')) == '배치하기 ›', '배치 동작 라벨 불일치'
    old_rows = panel['VerticalBox_RecommendedRows']
    assert not old_rows.get_all_children() and old_rows.get_visibility() == unreal.SlateVisibility.COLLAPSED
    assert row['Border_CompactCard'].get_parent() == row['SizeBox_Row'], '카드 프레임 계층 오류'
    card_body = row['VerticalBox_CompactCard']
    assert [item.get_name() for item in card_body.get_all_children()] == ['HorizontalBox_Content', 'SizeBox_Action']
    assert row['Text_Title'].get_parent() == row['HorizontalBox_Content'], '카드 제목 위치 오류'
    assert row['Button_Place'].get_parent() == row['SizeBox_Action'], '배치 버튼 누락'
    for name in ('SizeBox_Separator', 'VerticalBox_Copy', 'Text_Description'):
        assert row[name].get_visibility() == unreal.SlateVisibility.COLLAPSED, '행 중복 설명 표시: ' + name
    for name, value in heights.items():
        assert snapshot(panel[name], HEIGHT_FIELDS) == value, '헤더·하단 버튼 높이 변경: ' + name
    return {'panel_widget_count': len(panel), 'row_widget_count': len(row), 'recommended_count': len(children),
            'existing_widget_names_preserved': sum(len(items) for items in original.values()),
            'row_data_preserved': True, 'header_footer_heights_preserved': True}


try:
    assert not unreal.EditorLevelLibrary.get_pie_worlds(False), 'PIE 중에는 적용하지 않습니다.'
    targets = {asset_file(package) for package in PACKAGES}
    assert all(path.is_file() for path in targets), '기존 점검 에셋 누락'
    neighbors = set((ROOT / 'Content/FestaFiesta/UI/PrepHUD').rglob('*.uasset')) - targets
    neighbors.add(asset_file(HUD))
    PROTECTED = {path: digest(path) for path in neighbors}
    dirty = {package.get_name() for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    assert not dirty.intersection(PACKAGES), '점검 에셋의 미저장 변경을 먼저 저장해야 합니다.'
    original = {package: widgets(unreal.load_asset(package)) for package in PACKAGES}
    original_rows = {name: row_data(widget) for name, widget in original[PANEL].items() if name.startswith('RecommendedRow_')}
    assert len(original_rows) == 8, '기존 권장 시설 8종 누락'
    heights = {name: snapshot(original[PANEL][name], HEIGHT_FIELDS)
               for name in ('SizeBox_Header', 'SizeBox_ContinueBuild', 'SizeBox_OpenGate')}
    backup = OUT / 'Backups' / datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    backup.mkdir(parents=True, exist_ok=False)
    hashes_before = {}
    for package in PACKAGES:
        source = asset_file(package)
        hashes_before[package] = digest(source)
        shutil.copy2(source, backup / source.name)
        assert digest(backup / source.name) == hashes_before[package], '백업 해시 불일치'
    REPORT.update(backup=str(backup), protected_count=len(PROTECTED), heights_before=heights,
                  original_rows=original_rows, hashes_before=hashes_before)
    execute()
    REPORT['applied'] = verify(original, original_rows, heights)
    hashes_after = {package: digest(asset_file(package)) for package in PACKAGES}
    REPORT['saved_assets'] = [package for package in PACKAGES if hashes_after[package] != hashes_before[package]]
    execute()
    REPORT['repeated'] = verify(original, original_rows, heights)
    assert {package: digest(asset_file(package)) for package in PACKAGES} == hashes_after, '재실행 시 파일 변경'
    REPORT.update(status='passed', hashes_after=hashes_after, repeat_file_hashes_unchanged=True)
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
finally:
    changed = [str(path.relative_to(ROOT)) for path, value in PROTECTED.items() if not path.is_file() or digest(path) != value]
    REPORT['protected_changes'] = changed
    if changed:
        REPORT.update(status='failed', protected_error='점검 두 에셋 외 보호 파일 변경')
    (OUT / 'apply-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    if REPORT['status'] == 'passed':
        unreal.log('CHECK_COMPACT_APPLY_PASSED')
    else:
        unreal.log_error('CHECK_COMPACT_FAIL: ' + str(REPORT.get('error') or REPORT.get('protected_error')))

started = time.monotonic()


def finish(delta_time):
    if time.monotonic() - started < 2:
        return
    unreal.unregister_slate_post_tick_callback(exit_handle)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
exit_handle = unreal.register_slate_post_tick_callback(finish)
