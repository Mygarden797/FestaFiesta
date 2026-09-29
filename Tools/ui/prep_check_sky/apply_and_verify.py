"""P5 점검 화면만 적용한다. 사용자 편집한 다른 패널·HUD 트리를 보존한다."""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import re
import shutil
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/PrepCheckSky'
OUT.mkdir(parents=True, exist_ok=True)
LOG = ROOT / 'Saved/BrightGlobe/prep_check_sky.log'
REPORT_PATH = OUT / 'report.json'
CHECK = '/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelCheck'
CHOICE = '/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepCheckSkyWeatherChoice'
HUD = '/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar'
TARGETS = [ROOT / 'Content' / (p[6:] + '.uasset') for p in (CHECK, CHOICE, HUD)]
PROTECTED = [p for base in ('Content/FestaFiesta/UI', 'Content/FestaFiesta/Building/FF_WBP_Building', 'Config')
             for p in (ROOT/base).rglob('*') if p.is_file() and p.suffix in ('.uasset', '.ini') and p not in TARGETS]
HASHES = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in PROTECTED}
REPORT = dict(status='running', stage='apply', protected_count=len(HASHES), physical_input_tested=False)
HANDLE = None
START = time.monotonic()
# 실제 등록된 점검 검사를 수집하므로 일부 검사만 끝난 것을 전체 성공으로 처리하지 않는다.
TESTS = set()
for source in (ROOT / 'Source/FestaFiesta/Private/UI/PrepHUD/Panels').glob('*Tests.cpp'):
    TESTS.update(re.findall(r'"(FestaFiesta\.UI\.PrepCheck\.[^"\s]+)"', source.read_text(encoding='utf-8-sig')))
assert TESTS, '점검 자동검사 목록이 비어 있다'


def layouts(cdo):
    return {str(k): dict(size=[v.size.x, v.size.y], offset=[v.offset.x, v.offset.y], dock=str(v.dock))
            for k, v in cdo.get_editor_property('panel_layouts').items()}


def widgets(bp):
    return {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget)
            if w.get_outer() and w.get_outer().get_class().get_name() == 'WidgetTree'
            and w.get_outer().get_name() == 'WidgetTree' and w.get_outer().get_outer() == bp}


def hud_tree(bp):
    result = {}
    for name, widget in widgets(bp).items():
        item = dict(type=widget.get_class().get_name(), parent=widget.get_parent().get_name() if widget.get_parent() else None)
        for prop in ('height_override', 'width_override', 'min_desired_width', 'font', 'text',
                     'padding', 'visibility', 'brush_color', 'widget_style'):
            try: item[prop] = str(widget.get_editor_property(prop))
            except Exception: pass
        slot = widget.get_editor_property('slot')
        if slot:
            for prop in ('layout_data', 'padding', 'size', 'horizontal_alignment', 'vertical_alignment'):
                try: item['slot_'+prop] = str(slot.get_editor_property(prop))
                except Exception: pass
        result[name] = item
    return result


def command(value):
    unreal.SystemLibrary.execute_console_command(unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_world(), value)


def finish(error=None):
    global HANDLE
    if HANDLE is not None:
        unreal.unregister_slate_post_tick_callback(HANDLE)
        HANDLE = None
    changed = [p for p, digest in HASHES.items() if hashlib.sha256(Path(p).read_bytes()).hexdigest() != digest]
    REPORT.update(status='failed' if error or changed else 'passed', error=error,
                  protected_changes=changed, elapsed_seconds=round(time.monotonic()-START, 2))
    REPORT_PATH.write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    if error or changed:
        unreal.log_error('PREP_CHECK_SKY_FAIL: ' + str(error or changed))
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()


def tick(delta):
    try:
        assert time.monotonic()-START < 360, '점검 자동검사 시간 초과'
        lines = LOG.read_text(encoding='utf-8', errors='replace').splitlines() if LOG.exists() else []
        results = [line for line in lines if 'Test Completed. Result=' in line and 'FestaFiesta.UI.PrepCheck.' in line]
        if all(any(name+'}' in line for line in results) for name in TESTS):
            REPORT['automation'] = results
            assert all('Result={Success}' in line for line in results), '점검 자동검사 실패'
            finish()
    except Exception:
        finish(traceback.format_exc())


try:
    hud_bp = unreal.load_asset(HUD)
    hud_cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(HUD))
    before_layout = layouts(hud_cdo)
    before_tree = hud_tree(hud_bp)
    REPORT['layouts_before'] = before_layout
    already = 'Text_RequiredFacilities' in widgets(unreal.load_asset(CHECK))
    if not already:
        backup = OUT / ('Before-' + datetime.now().strftime('%Y%m%d-%H%M%S'))
        for src in TARGETS:
            if src.exists():
                dest = backup / src.relative_to(ROOT)
                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(src, dest)
        REPORT['backup'] = str(backup)
        command('FF.ApplyPrepCheckSkyDesign')
        assert 'Text_RequiredFacilities' in widgets(unreal.load_asset(CHECK)), '점검 새 트리 생성 실패'
        mapping = hud_cdo.get_editor_property('panel_layouts')
        check_layout = mapping[unreal.FF_PrepHudTab.CHECK]
        check_layout.size = unreal.Vector2D(1140, 700)
        mapping[unreal.FF_PrepHudTab.CHECK] = check_layout
        hud_cdo.set_editor_property('panel_layouts', mapping)
        unreal.BlueprintEditorLibrary.compile_blueprint(hud_bp)
        after = layouts(unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(HUD)))
        expected = json.loads(json.dumps(before_layout))
        expected[str(unreal.FF_PrepHudTab.CHECK)]['size'] = [1140., 700.]
        assert after == expected, '점검 Size 외 다른 패널 설정 변경'
        assert hud_tree(hud_bp) == before_tree, 'HUD 위젯/행 높이/스타일 변경'
        assert unreal.EditorAssetLibrary.save_loaded_asset(hud_bp, only_if_is_dirty=False)
        REPORT.update(layouts_after=after, hud_tree_unchanged=True)
    else:
        REPORT.update(designer_edits_preserved=True, layouts_after=before_layout)
    saved_widgets = widgets(unreal.load_asset(CHECK))
    for name in ('Text_PanelTitle', 'Text_DayLabel', 'Button_Close', 'Image_Close', 'ScrollBox_Body',
                 'Text_Weather', 'Choice_Clear', 'Choice_Rain', 'Choice_Windy', 'Text_DailyGoalValue',
                 'Text_DailyGoalBonus', 'Text_ExpectedVisitors', 'Text_Gold', 'Text_CapacityRisk',
                 'Text_CapacityDetail', 'Text_StaffSummary', 'Text_ProgramSummary', 'Text_RequiredFacilities',
                 'Text_StaffWageCost', 'Text_ExpectedAdmission', 'Text_ExpectedGoalBonus',
                 'Text_OpenGateReason', 'Button_ContinueBuild', 'Button_OpenGate', 'Text_OpenGateLabel'):
        assert name in saved_widgets, '점검 위젯 누락: ' + name
    REPORT.update(stage='automation', required_widgets=True, expected_tests=sorted(TESTS))
    REPORT_PATH.write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    command('Automation RunTests FestaFiesta.UI.PrepCheck.')
    HANDLE = unreal.register_slate_post_tick_callback(tick)
except Exception:
    finish(traceback.format_exc())
