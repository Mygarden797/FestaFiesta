"""라이브 전용 복제 WBP 적용·계약 검사. 원본과 사용자 HUD는 저장하지 않는다."""
from pathlib import Path
import hashlib
import json
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
SAVED = Path(unreal.Paths.project_saved_dir()).resolve()
OUT = ROOT / 'Saved/LiveStaffCompact'
OUT.mkdir(parents=True, exist_ok=True)
STAFF = '/Game/FestaFiesta/UI/Staff/'
ORIGINALS = [STAFF + 'WBP_FF_StaffManagement', STAFF + 'WBP_FF_StaffTraining',
             STAFF + 'WBP_FF_StaffPromotionRow']
MANAGEMENT, TRAINING, PROMOTION = [STAFF + 'WBP_FF_LiveStaff' + suffix
                                   for suffix in ('Management', 'Training', 'PromotionRow')]
LIVE = '/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelStaff'
FACILITY = STAFF + 'WBP_FF_StaffFacilityRow'
PREP = '/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelStaff'
PREP_HUD = '/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar'
LIVE_HUD = '/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveHudScreen'
TARGETS = [MANAGEMENT, TRAINING, PROMOTION, LIVE]
VERSION = 'FF.LiveStaffCompact.Version'
REPORT = {'status': 'running', 'command': 'FF.ApplyLiveStaffCompactDesign',
          'visual_check': '직원 5행·시설 3개 및 실제 글자 잘림은 PIE 화면 확인 필요'}


def filename(package):
    return ROOT / 'Content' / (package.removeprefix('/Game/') + '.uasset')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def hashes(paths):
    return {str(path.relative_to(ROOT)): digest(path) for path in paths if path.exists()}


def widgets(bp):
    tree = unreal.find_object(bp, 'WidgetTree')
    assert tree, 'WidgetTree 누락: ' + bp.get_path_name()
    return {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget) if w.get_outer() == tree}


def require(table, names, cls, package):
    for name in names.split():
        assert isinstance(table.get(name), cls), '위젯 누락/타입: ' + package + '.' + name


def generated(package):
    cls = unreal.EditorAssetLibrary.load_blueprint_class(package)
    assert cls, '클래스 누락: ' + package
    return cls


def defaults(package):
    return unreal.get_default_object(generated(package))


def slot_signature(slot):
    if not slot:
        return None
    result = {'class': slot.get_class().get_path_name()}
    for name in ('padding', 'size', 'horizontal_alignment', 'vertical_alignment',
                 'layout_data', 'auto_size', 'z_order', 'row', 'column', 'row_span', 'column_span',
                 'layer', 'nudge'):
        try:
            value = slot.get_editor_property(name)
        except Exception:
            continue
        result[name] = value.export_text() if hasattr(value, 'export_text') else str(value)
    return result


def describe(table):
    return {name: {'class': w.get_class().get_path_name(),
                   'parent': w.get_parent().get_name() if w.get_parent() else None,
                   'slot': slot_signature(w.get_editor_property('slot'))}
            for name, w in table.items()}


def verify_preserved(original, table, package, changed_classes=()):
    for name, info in original.items():
        assert name in table, '기존 위젯 이름 소실: ' + package + '.' + name
        if name not in changed_classes:
            assert table[name].get_class().get_path_name() == info['class'], '기존 위젯 타입 변경: ' + package + '.' + name


# 보호 대상은 디스크 해시만 읽는다. 사용자 미저장 HUD를 로드·컴파일·저장하지 않는다.
protected = [filename(p) for p in ORIGINALS + [FACILITY, PREP, PREP_HUD, LIVE_HUD]]
protected += list((ROOT / 'Content/FestaFiesta/Building/FF_WBP_Building/FacilityRevenue').glob('*.uasset'))
protected.append(ROOT / 'Content/FestaFiesta/Data/Economy/DT_FacilityRevenues.uasset')
protected = sorted(set(protected))
before_hashes = hashes(protected)
before_targets = hashes([filename(p) for p in TARGETS])
binding_report_path = SAVED / 'LiveStaffCompact/binding-validation.json'
previous_binding_report = binding_report_path.read_bytes() if binding_report_path.exists() else None
try:
    assert len(before_hashes) == len(protected), '보호 에셋 파일 누락'
    sources = {p: unreal.load_asset(p) for p in ORIGINALS + [LIVE]}
    assert all(sources.values()), '원본 스태프 에셋 누락'
    before = {p: describe(widgets(bp)) for p, bp in sources.items()}
    source_fonts = {p: {name: {'font': w.get_editor_property('font').export_text(),
                              'color': w.get_editor_property('color_and_opacity').export_text()}
                        for name, w in widgets(bp).items() if isinstance(w, unreal.TextBlock)}
                    for p, bp in sources.items()}
    reuse = all(filename(p).exists() for p in TARGETS) and all(
        unreal.EditorAssetLibrary.get_metadata_tag(unreal.load_asset(p), VERSION) == '1' for p in TARGETS)
    world = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_world()
    unreal.SystemLibrary.execute_console_command(world, REPORT['command'])
    assert binding_report_path.exists(), '실제 native 바인딩 검사 결과 없음'
    binding_report_bytes = binding_report_path.read_bytes()
    assert binding_report_bytes != previous_binding_report, '이번 실행의 native 바인딩 검사 결과 없음'
    # FFileHelper 기본 인코딩은 내용에 따라 달라질 수 있으므로 BOM을 함께 처리한다.
    binding_encoding = 'utf-16' if binding_report_bytes.startswith((b'\xff\xfe', b'\xfe\xff')) else 'utf-8-sig'
    binding_report = json.loads(binding_report_bytes.decode(binding_encoding))
    assert binding_report['status'] == 'passed' and binding_report['checked_bindings'] == 12
    assert binding_report['initialized_widgets'] == 4
    REPORT['native_binding_validation'] = binding_report
    REPORT['native_binding_report'] = str(binding_report_path)
    assets = {p: unreal.load_asset(p) for p in TARGETS}
    for package, bp in assets.items():
        assert bp and unreal.EditorAssetLibrary.get_metadata_tag(bp, VERSION) == '1', '적용 실패/충돌: ' + package
    tables = {p: widgets(bp) for p, bp in assets.items()}
    preserved_count = 0
    for original, copy in zip(ORIGINALS, [MANAGEMENT, TRAINING, PROMOTION]):
        changed = ('Widget_StaffTraining',) if copy == MANAGEMENT else ()
        verify_preserved(before[original], tables[copy], copy, changed)
        preserved_count += len(before[original])
        # 기존 글꼴과 색은 원본에서 그대로 복제한다. 새 설명/배지는 인접 라벨 스타일을 따른다.
        if not reuse:
            for name, style in source_fonts[original].items():
                assert tables[copy][name].get_editor_property('font').export_text() == style['font'], '폰트 변경: ' + copy + '.' + name
                assert tables[copy][name].get_editor_property('color_and_opacity').export_text() == style['color'], '색 변경: ' + copy + '.' + name
    verify_preserved(before[LIVE], tables[LIVE], LIVE, ('Widget_StaffManagement',))
    after_host = describe(tables[LIVE])
    for name, info in before[LIVE].items():
        assert info['parent'] == after_host[name]['parent'], '라이브 외곽 계층 변경: ' + name
        assert info['slot'] == after_host[name]['slot'], '라이브 외곽 슬롯 변경: ' + name
    management, training, promotion = [tables[p] for p in (MANAGEMENT, TRAINING, PROMOTION)]
    require(management, '''Text_Phase Text_TotalStaff Text_SecondLabel Text_SecondValue Text_ThirdLabel Text_ThirdValue
        Text_FourthLabel Text_FourthValue Text_Vacancies Text_FacilitiesEmpty Text_AssignmentGuide
        Text_AssignmentResult Text_IncidentTitle Text_IncidentSummary''', unreal.TextBlock, MANAGEMENT)
    require(management, 'Button_FillVacancies Button_ViewIncident', unreal.Button, MANAGEMENT)
    require(management, 'Box_Facilities', unreal.VerticalBox, MANAGEMENT)
    require(management, 'Group_Incident Border_AssignmentGuide', unreal.Border, MANAGEMENT)
    require(training, 'Text_TrainingHelp Compact_Text_StatusHeader', unreal.TextBlock, TRAINING)
    require(training, 'Button_AllStaff Button_UnassignedStaff', unreal.Button, TRAINING)
    require(training, 'Box_StaffRoster', unreal.VerticalBox, TRAINING)
    require(promotion, 'Text_StaffName Text_StaffLevel Text_AssignmentFacility Text_AssignmentStatus Text_PromotionStatus', unreal.TextBlock, PROMOTION)
    require(promotion, 'Button_Promote', unreal.Button, PROMOTION)
    # bIsVariable와 BindWidgetOptional 전용 필드는 Python 읽기가 금지되어 있다.
    # 위 C++ 검사는 실제 Initialize 결과를 FObjectProperty로 읽고 인스턴스 트리 포인터와 대조한다.
    assert tables[LIVE]['Widget_StaffManagement'].get_class() == generated(MANAGEMENT)
    assert management['Widget_StaffTraining'].get_class() == generated(TRAINING)
    assert defaults(MANAGEMENT).get_editor_property('use_compact_live_layout')
    assert defaults(MANAGEMENT).get_editor_property('facility_row_class') == generated(FACILITY)
    assert defaults(TRAINING).get_editor_property('compact_live_layout')
    assert defaults(TRAINING).get_editor_property('staff_row_class') == generated(PROMOTION)
    assert management['Widget_StaffTraining'].get_editor_property('staff_row_class') == generated(PROMOTION)
    assert defaults(PROMOTION).get_editor_property('compact_live_layout')
    assert defaults(PROMOTION).get_editor_property('compact_management_row')
    assert promotion['Text_AssignmentStatus'].get_parent() == promotion['Compact_Size_Status']
    assert promotion['Text_AssignmentFacility'].get_parent() == promotion['Management_StaffAssignment']
    assert promotion['Compact_Size_Status'].get_parent() == promotion['HorizontalBox_StaffRow']
    assert training['Compact_Size_StatusHeader'].get_parent() == training['HorizontalBox_RosterHeader']
    assert promotion['SizeBox_StaffRow'].get_editor_property('height_override') == 56.0
    assert training['SizeBox_StaffRoster'].get_editor_property('min_desired_height') >= 310.0
    for name in ('Text_Subtitle', 'Text_StaffBreakdown', 'Text_SecondNote', 'Text_ThirdNote', 'Text_FourthNote', 'Text_ManagementNote'):
        assert management[name].get_visibility() == unreal.SlateVisibility.COLLAPSED, '반복 안내 접힘 실패: ' + name
    if reuse:
        assert before_targets == hashes([filename(p) for p in TARGETS]), '재실행 시 저장된 디자인이 바뀜'
    REPORT.update(status='passed', mode='reuse' if reuse else 'applied', packages=TARGETS,
                  preserved_widgets=preserved_count + len(before[LIVE]), host_slots_unchanged=True,
                  original_fonts_and_colors_preserved=not reuse,
                  staff_columns=['직원', '숙련도', '배정 시설', '상태', '승급'],
                  backup_folder=str(SAVED / 'UIBackups/LiveStaffCompact'))
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
finally:
    after_hashes = hashes(protected)
    REPORT['protected_before'] = before_hashes
    REPORT['protected_after'] = after_hashes
    REPORT['protected_unchanged'] = before_hashes == after_hashes
    if before_hashes != after_hashes:
        REPORT.update(status='failed', error=REPORT.get('error', '') + '\n원본·HUD·매출 보호 에셋 해시가 변경되었습니다.')
    (OUT / 'apply-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    if REPORT['status'] == 'failed':
        unreal.log_error('LIVE_STAFF_COMPACT_FAIL: ' + REPORT['error'])
    else:
        unreal.log('LIVE_STAFF_COMPACT_PASSED: ' + REPORT['mode'])
    if not globals().get('FF_BATCH_APPLY', False):
        unreal.SystemLibrary.quit_editor()
