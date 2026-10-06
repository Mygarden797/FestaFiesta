"""P5 최종 결산 WBP만 구성한다. 기존 HUD·맵 저장과 PIE/렌더는 실행하지 않는다."""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import shutil
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/FinalReportSky'
OUT.mkdir(parents=True, exist_ok=True)
SCREEN = '/Game/FestaFiesta/UI/Report/WBP_FF_FinalReportScreen'
ROW = '/Game/FestaFiesta/UI/Report/WBP_FF_FinalReportDayRow'
KEY = 'FF.FinalReportSky.Version'
PREP_HUD = '/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar'
LIVE_HUD = '/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveHudScreen'
PROTECTED = (
    PREP_HUD, LIVE_HUD,
    '/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelReport',
    '/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_DailyReportLedgerRow',
    '/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveUtilityDock',
    '/Game/FestaFiesta/UI/Flow/WBP_FF_FlowStub',
)
REQUIRED = {
    SCREEN: ('FF_ReportScreen', {
        'Text_Title', 'Text_FestivalSummary', 'Text_Grade', 'Text_TotalScore',
        'Text_Headline', 'Text_WeakestAxisComment', 'Text_SatisfactionScore',
        'Text_VisitorScore', 'Text_FinanceScore', 'Text_BuzzScore',
        'Progress_SatisfactionScore', 'Progress_VisitorScore',
        'Progress_FinanceScore', 'Progress_BuzzScore',
        'Text_TotalAdmittedVisitors', 'Text_TotalRevenue', 'Text_TotalExpense',
        'Text_NetProfit', 'Text_DaysSummary', 'Box_DayRows', 'Text_Gold',
        'Text_Refund', 'Text_VisitorHistoryNote', 'Text_Status',
        'Button_Lobby', 'Text_LobbyLabel', 'Image_FinalGold'}),
    ROW: ('FF_FinalReportDayRowWidget', {
        'Text_Day', 'Text_Revenue', 'Text_Expense', 'Text_NetProfit',
        'Text_AdmittedVisitors'}),
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


def layouts(package):
    return {str(key): value.export_text()
            for key, value in defaults(package).get_editor_property('panel_layouts').items()}


def object_path(value):
    return value.get_path_name() if hasattr(value, 'get_path_name') else str(value)


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
    for package in REQUIRED:
        source = asset_file(package)
        if source.is_file():
            destination = backup / source.relative_to(ROOT)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
    REPORT['backup'] = str(backup)
    protected = {}
    for package in PROTECTED:
        source = asset_file(package)
        assert source.is_file(), '기존 에셋 없음: ' + package
        protected[package] = digest(source)
    before_layouts = {package: layouts(package) for package in (PREP_HUD, LIVE_HUD)}

    unreal.SystemLibrary.execute_console_command(None, 'FF.ApplyFinalReportSkyDesign')
    for package, (parent_name, required_names) in REQUIRED.items():
        blueprint = unreal.load_asset(package)
        assert blueprint, '최종 결산 WBP 제작 실패: ' + package
        assert unreal.EditorAssetLibrary.get_metadata_tag(blueprint, KEY) == '1', '최종 결산 디자인 적용 실패: ' + package
        assert isinstance(defaults(package), getattr(unreal, parent_name)), 'WBP 부모 클래스 불일치: ' + package
        missing = required_names - set(widgets(blueprint))
        assert not missing, '필수 최종 결산 바인딩 누락: ' + str(sorted(missing))
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        assert unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=True)
        REPORT['compiled_widgets'].append(package)
    day_row_class = defaults(SCREEN).get_editor_property('day_row_class')
    assert day_row_class == unreal.EditorAssetLibrary.load_blueprint_class(ROW), '일별 결산 행 클래스 연결 누락'
    gold_images = visible_gold_images(unreal.load_asset(SCREEN))
    assert gold_images == ['Image_FinalGold'], '최종 보유 금화 이외의 금화 아이콘 확인 필요: ' + str(gold_images)
    assert not visible_gold_images(unreal.load_asset(ROW)), '일별 결산 행에는 금화 아이콘을 쓰지 않습니다.'
    settings = unreal.get_default_object(unreal.FF_UIFlowSettings)
    configured_screen = object_path(settings.get_editor_property('final_report_screen_class'))
    expected_class = SCREEN + '.WBP_FF_FinalReportScreen_C'
    assert expected_class in configured_screen, 'UI Flow의 Final Report Screen Class 연결 누락'

    for package, before in before_layouts.items():
        assert layouts(package) == before, '기존 HUD 탭 위치·크기 변경: ' + package
    for package, before in protected.items():
        assert digest(asset_file(package)) == before, '기존 HUD/다른 WBP 파일 변경: ' + package
    REPORT.update(status='passed', protected_assets=list(protected),
                  hud_layouts_preserved=True, visible_currency_images=gold_images,
                  configured_screen=configured_screen, day_row_class=object_path(day_row_class))
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
    unreal.log_error(REPORT['error'])
finally:
    (OUT / 'apply-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    # 에디터 시작 컨텍스트의 초기화를 마친 후 종료한다.
    exit_after = time.monotonic() + 2.0
    def finish_after_startup(delta):
        if time.monotonic() < exit_after:
            return
        unreal.unregister_slate_post_tick_callback(exit_handle)
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    exit_handle = unreal.register_slate_post_tick_callback(finish_after_startup)
