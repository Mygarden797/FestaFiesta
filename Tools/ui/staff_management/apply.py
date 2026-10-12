"""스태프 관리 WBP를 한 번 적용하고 기존 고용·승급 검사 두 개만 실행한다."""
from pathlib import Path
import json
import re
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/StaffManagement'
OUT.mkdir(parents=True, exist_ok=True)
STAFF = '/Game/FestaFiesta/UI/Staff/'
PREP = '/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelStaff'
LIVE = '/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelStaff'
TRAINING = STAFF + 'WBP_FF_StaffTraining'
PROMOTION = STAFF + 'WBP_FF_StaffPromotionRow'
MANAGEMENT = STAFF + 'WBP_FF_StaffManagement'
FACILITY_ROW = STAFF + 'WBP_FF_StaffFacilityRow'
PREP_HUD = '/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar'
LIVE_HUD = '/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveHudScreen'
PACKAGES = [PREP, LIVE, TRAINING, PROMOTION, MANAGEMENT, FACILITY_ROW, PREP_HUD, LIVE_HUD]
VERSION = 'FF.StaffManagement.Version'
REPORT = {'status': 'running', 'visual_check': '배정·위치 보기와 실제 화면은 사용자 확인 예정',
          'command': 'FF.ApplyStaffManagementDesign'}


def widgets(bp):
    tree = unreal.find_object(bp, 'WidgetTree')
    return {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget) if w.get_outer() == tree}


def defaults(package):
    cls = unreal.EditorAssetLibrary.load_blueprint_class(package)
    assert cls, '위젯 클래스 없음: ' + package
    return unreal.get_default_object(cls)


def layouts(package):
    return {str(key): value.export_text()
            for key, value in defaults(package).get_editor_property('panel_layouts').items()}


def require(table, names, cls, package):
    for name in names.split():
        assert isinstance(table.get(name), cls), '바인딩 누락/타입 불일치: ' + package + '.' + name


try:
    existing = [PREP, LIVE, TRAINING, PROMOTION]
    before = {p: {name: w.get_class().get_path_name() for name, w in widgets(unreal.load_asset(p)).items()}
              for p in existing}
    before_layouts = {p: layouts(p) for p in (PREP_HUD, LIVE_HUD)}
    world = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_world()
    unreal.SystemLibrary.execute_console_command(world, REPORT['command'])
    assets = {p: unreal.load_asset(p) for p in PACKAGES}
    for package, bp in assets.items():
        assert bp and unreal.EditorAssetLibrary.get_metadata_tag(bp, VERSION) == '1', '적용 실패: ' + package
    tables = {p: widgets(assets[p]) for p in PACKAGES[:6]}
    preserved = 0
    for package, original in before.items():
        table = tables[package]
        for name, cls in original.items():
            assert name in table and table[name].get_class().get_path_name() == cls, '기존 위젯 소실: ' + package + '.' + name
            preserved += 1
    for package in (PREP, LIVE):
        table = tables[package]
        assert table['Management_Legacy'].get_visibility() == unreal.SlateVisibility.COLLAPSED
        assert table['Switcher_StaffContent'].get_parent() == table['Management_Legacy']
        assert table['Widget_StaffManagement'].get_class() == unreal.EditorAssetLibrary.load_blueprint_class(MANAGEMENT)
        require(table, 'Button_Close', unreal.Button, package)
    require(tables[PREP], 'Button_Hire Button_Decrease Button_Increase', unreal.Button, PREP)
    require(tables[PREP], 'Text_CurrentGold Text_HireCostTotal Text_GoldAfterValue Text_SelectedCount', unreal.TextBlock, PREP)
    assert tables[PREP]['Management_CurrentGoldIcon'].get_editor_property('brush').resource_object
    management = tables[MANAGEMENT]
    require(management, '''Text_Phase Text_Subtitle Text_TotalStaff Text_StaffBreakdown
        Text_SecondLabel Text_SecondValue Text_SecondNote Text_ThirdLabel Text_ThirdValue Text_ThirdNote
        Text_FourthLabel Text_FourthValue Text_FourthNote Text_FacilitiesEmpty Text_AssignmentGuide
        Text_AssignmentResult Text_IncidentSummary Text_ManagementNote''', unreal.TextBlock, MANAGEMENT)
    require(management, 'Button_FillVacancies Button_ViewIncident', unreal.Button, MANAGEMENT)
    require(management, 'Box_Facilities', unreal.VerticalBox, MANAGEMENT)
    require(management, 'Group_Incident', unreal.Widget, MANAGEMENT)
    assert management['Widget_StaffTraining'].get_class() == unreal.EditorAssetLibrary.load_blueprint_class(TRAINING)
    require(tables[FACILITY_ROW], 'Text_FacilityName Text_AssignedCapacity Text_FacilityStatus', unreal.TextBlock, FACILITY_ROW)
    require(tables[FACILITY_ROW], 'Button_Assign Button_Unassign Button_Locate', unreal.Button, FACILITY_ROW)
    require(tables[TRAINING], 'Button_AllStaff Button_UnassignedStaff', unreal.Button, TRAINING)
    require(tables[TRAINING], 'Text_AllStaffFilter Text_UnassignedStaffFilter', unreal.TextBlock, TRAINING)
    require(tables[PROMOTION], 'Text_AssignmentFacility Text_AssignmentStatus', unreal.TextBlock, PROMOTION)
    require(tables[PROMOTION], 'Button_Promote', unreal.Button, PROMOTION)
    assert defaults(TRAINING).get_editor_property('staff_row_class') == unreal.EditorAssetLibrary.load_blueprint_class(PROMOTION)
    assert defaults(MANAGEMENT).get_editor_property('facility_row_class') == unreal.EditorAssetLibrary.load_blueprint_class(FACILITY_ROW)
    assert defaults(PROMOTION).get_editor_property('compact_management_row')
    for package, tab in ((PREP_HUD, unreal.FF_PrepHudTab.STAFF), (LIVE_HUD, unreal.FF_LiveTab.STAFF)):
        after = layouts(package)
        before_other = {k: v for k, v in before_layouts[package].items() if k != str(tab)}
        after_other = {k: v for k, v in after.items() if k != str(tab)}
        assert before_other == after_other, '다른 HUD 탭의 배치가 변경됨: ' + package
        layout = defaults(package).get_editor_property('panel_layouts')[tab]
        size = layout.get_editor_property('size') if package == PREP_HUD else None
        actual = (size.x, size.y) if size is not None else (layout.get_editor_property('width'), layout.get_editor_property('height'))
        assert actual == (1300.0, 800.0), '스태프 패널 크기 불일치: ' + package
    REPORT.update(status='passed', packages=PACKAGES, preserved_widgets=preserved,
                  binding_check='고용·승급·배정·사건 버튼 및 행 클래스 연결 유지',
                  unchanged_other_hud_layouts=True,
                  backup_folder=str(ROOT / 'Saved/UIBackups/StaffManagement'))
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
    unreal.log_error('STAFF_MANAGEMENT_FAIL: ' + REPORT['error'])

expected = {'FestaFiesta.UI.ViewModel.EconomyRelay.StaffPromotion', 'FestaFiesta.UI.PrepStaff.QuoteClamps'}
started = time.monotonic()
log_path = ROOT / 'Saved/BrightGlobe/staff_management_apply.log'
if REPORT['status'] == 'passed' and not globals().get('FF_BATCH_APPLY', False):
    unreal.SystemLibrary.execute_console_command(world, 'Automation RunTests ' + '+'.join(sorted(expected)))


def finish(delta_time):
    elapsed = time.monotonic() - started
    if elapsed < 10:
        return
    if REPORT['status'] == 'passed':
        log = log_path.read_text(encoding='utf-8', errors='replace') if log_path.exists() else ''
        matches = re.findall(r'Test Completed\. Result=\{([^}]+)\}.*?Path=\{([^}]+)\}', log)
        results = {name: state for state, name in matches if name in expected}
        if not expected.issubset(results) and elapsed < 90:
            return
        REPORT['tests'] = results
        if not expected.issubset(results) or any(state != 'Success' for state in results.values()):
            REPORT.update(status='failed', error='기존 고용/승급 검사 실패 또는 시간 초과')
            unreal.log_error('STAFF_MANAGEMENT_FAIL: ' + REPORT['error'])
    (OUT / 'apply-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(exit_handle)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()


if globals().get('FF_BATCH_APPLY', False):
    REPORT['tests'] = '통합 스크립트에서 기존 검사 두 개 실행'
    (OUT / 'apply-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
else:
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    exit_handle = unreal.register_slate_post_tick_callback(finish)
