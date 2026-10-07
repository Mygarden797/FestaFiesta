"""P3 기존 점검 WBP에 입장료 조절 영역만 추가한다. PIE·렌더·전체 검증은 실행하지 않는다."""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import shutil
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/PrepAdmission'
OUT.mkdir(parents=True, exist_ok=True)
CHECK = '/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelCheck'
PREP_HUD = '/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar'
LIVE_HUD = '/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveHudScreen'
METADATA_KEY = 'FF_PrepAdmissionVersion'
REQUIRED = {
    'Border_AdmissionSection', 'Slider_AdmissionFee', 'Button_AdmissionDecrease',
    'Button_AdmissionIncrease', 'Button_AdmissionApply', 'Button_AdmissionReset',
    'Text_AdmissionFeeValue', 'Text_AppliedAdmissionFee', 'Text_AdmissionFeeRange',
    'Text_AdmissionFeeState', 'Text_AdmissionPreviewDelta', 'Text_AdmissionPreviewRevenue',
    'Text_PanelTitle', 'Button_Close', 'Text_ExpectedVisitors', 'Text_Gold',
    'Button_OpenGate', 'Text_OpenGateLabel', 'Text_OpenGateReason', 'Button_ContinueBuild',
}
STYLE_TARGETS = ('Text_PanelTitle', 'Text_ExpectedVisitors', 'Text_Gold', 'Text_OpenGateLabel',
                 'Button_Close', 'Button_OpenGate', 'Button_ContinueBuild')
CDO_PROPERTIES = ('capacity_caution_ratio', 'capacity_danger_ratio', 'day_label_format',
                  'gold_value_format', 'required_facility_format', 'no_required_facilities_text')
REPORT = dict(status='running', visual_check='사용자 에디터 확인 예정',
              command='FF.ApplyPrepAdmissionDesign', compiled_widgets=[])


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


def layouts(package):
    return {str(key): value.export_text()
            for key, value in defaults(package).get_editor_property('panel_layouts').items()}


def export_value(value):
    return value.export_text() if hasattr(value, 'export_text') else str(value)


def current_settings():
    cdo = defaults(CHECK)
    return {name: export_value(cdo.get_editor_property(name)) for name in CDO_PROPERTIES}


def selected_styles(widget_table):
    result = {}
    for name in STYLE_TARGETS:
        assert name in widget_table, '기존 점검 위젯 없음: ' + name
        widget = widget_table[name]
        if isinstance(widget, unreal.TextBlock):
            properties = ('font', 'color_and_opacity', 'justification')
        else:
            assert isinstance(widget, unreal.Button), '기존 점검 위젯 타입 불일치: ' + name
            properties = ('widget_style',)
        result[name] = {prop: export_value(widget.get_editor_property(prop)) for prop in properties}
    return result


try:
    source = asset_file(CHECK)
    assert source.is_file(), '기존 점검 에셋 없음: ' + CHECK
    backup = OUT / ('Before-' + datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
    destination = backup / source.relative_to(ROOT)
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)
    REPORT['backup'] = str(backup)
    protected = {package: sha256(asset_file(package)) for package in (PREP_HUD, LIVE_HUD)}
    before_layouts = {package: layouts(package) for package in protected}
    blueprint = unreal.load_asset(CHECK)
    assert isinstance(defaults(CHECK), unreal.FF_PrepPanelCheck), '점검 부모 클래스 불일치'
    before_widgets = widgets(blueprint)
    before_names = set(before_widgets)
    before_styles = selected_styles(before_widgets)
    before_settings = current_settings()
    was_applied = unreal.EditorAssetLibrary.get_metadata_tag(blueprint, METADATA_KEY) == '1'

    unreal.SystemLibrary.execute_console_command(None, REPORT['command'])
    blueprint = unreal.load_asset(CHECK)
    assert unreal.EditorAssetLibrary.get_metadata_tag(blueprint, METADATA_KEY) == '1', '입장료 영역 구성 실패'
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    after_widgets = widgets(blueprint)
    missing = REQUIRED - set(after_widgets)
    assert not missing, '필수 입장료 위젯 누락: ' + str(sorted(missing))
    assert before_names <= set(after_widgets), '기존 점검 위젯이 사라졌습니다: ' + str(sorted(before_names - set(after_widgets)))
    assert selected_styles(after_widgets) == before_styles, '기존 제목·방문객·금화·개장 버튼 스타일 변경'
    assert current_settings() == before_settings, '기존 점검 CDO 설정 변경'
    assert unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=True)
    REPORT['compiled_widgets'].append(CHECK)
    for package, digest in protected.items():
        assert layouts(package) == before_layouts[package], '기존 HUD 탭 위치·크기 변경: ' + package
        assert sha256(asset_file(package)) == digest, '기존 HUD 파일 변경: ' + package
    REPORT.update(status='passed', required_bindings=sorted(REQUIRED),
                  existing_widget_count=len(before_names), existing_styles_preserved=True,
                  existing_settings_preserved=True, hud_layouts_preserved=True,
                  already_applied=was_applied, protected_assets=list(protected))
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
    unreal.log_error(REPORT['error'])
finally:
    (OUT / 'apply-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    # 첫 시작 틱에서 종료하지 않고 에디터 도구 컨텍스트 초기화를 마친다.
    exit_after = time.monotonic() + 2.0
    def finish_after_startup(delta):
        if time.monotonic() < exit_after:
            return
        unreal.unregister_slate_post_tick_callback(exit_handle)
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    exit_handle = unreal.register_slate_post_tick_callback(finish_after_startup)
