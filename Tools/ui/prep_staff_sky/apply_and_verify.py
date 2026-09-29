"""P2 인력 패널 적용·컴파일·저장 및 좁은 범위 자동 검증. 실행 전 에디터를 닫는다."""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import shutil
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/PrepStaffSky'
OUT.mkdir(parents=True, exist_ok=True)
LOG = ROOT / 'Saved/BrightGlobe/prep_staff_sky.log'
REPORT_PATH = OUT / 'report.json'
STAFF = '/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelStaff'
HUD = '/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar'
TARGETS = [ROOT / 'Content' / (p[6:] + '.uasset') for p in (STAFF, HUD)]
PROTECTED = [p for base in ('Content/FestaFiesta/UI', 'Content/FestaFiesta/Building/FF_WBP_Building', 'Config')
             for p in (ROOT/base).rglob('*') if p.is_file() and p.suffix in ('.uasset', '.ini') and p not in TARGETS]
hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in PROTECTED}
REPORT = dict(status='running', stage='apply', protected_count=len(hashes),
              physical_input_tested=False, p3_started=False)
HANDLE = None
START = time.monotonic()

def layouts(cdo):
    return {str(k): dict(size=[v.size.x, v.size.y], offset=[v.offset.x, v.offset.y], dock=str(v.dock))
            for k, v in cdo.get_editor_property('panel_layouts').items()}

def widgets(bp):
    return {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget)
            if w.get_outer() and w.get_outer().get_class().get_name() == 'WidgetTree'
            and w.get_outer().get_outer() == bp}

def hud_tree(bp):
    result = {}
    for name, w in widgets(bp).items():
        item = dict(type=w.get_class().get_name(), parent=w.get_parent().get_name() if w.get_parent() else None)
        for prop in ('height_override', 'width_override', 'min_desired_width', 'font', 'text',
                     'padding', 'visibility', 'brush_color', 'widget_style'):
            try: item[prop] = str(w.get_editor_property(prop))
            except Exception: pass
        slot = w.get_editor_property('slot')
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
    changed = [p for p, h in hashes.items() if hashlib.sha256(Path(p).read_bytes()).hexdigest() != h]
    REPORT.update(status='failed' if error or changed else 'passed', error=error,
                  protected_changes=changed, elapsed_seconds=round(time.monotonic()-START, 2))
    REPORT_PATH.write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    if error or changed: unreal.log_error('PREP_STAFF_SKY_FAIL: ' + str(error or changed))
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()

def tick(delta):
    try:
        assert time.monotonic() - START < 240, '인력 자동화 시간 초과'
        content = LOG.read_text(encoding='utf-8', errors='replace') if LOG.exists() else ''
        results = [line for line in content.splitlines() if 'Test Completed. Result=' in line and 'FestaFiesta.UI.PrepStaff.' in line]
        if len(results) >= 5:
            REPORT['automation'] = results
            assert all('Result={Success}' in line for line in results), '인력 자동화 실패'
            finish()
    except Exception:
        finish(traceback.format_exc())

try:
    existing = unreal.load_asset(STAFF)
    already = 'Progress_StaffCapacity' in widgets(existing)
    if not already:
        backup = OUT / ('Before-' + datetime.now().strftime('%Y%m%d-%H%M%S'))
        for src in TARGETS:
            dest = backup / src.relative_to(ROOT)
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dest)
        REPORT['backup'] = str(backup)
        hud_bp = unreal.load_asset(HUD)
        hud_cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(HUD))
        before_layout = layouts(hud_cdo)
        before_tree = hud_tree(hud_bp)
        REPORT['layouts_before'] = before_layout
        command('FF.ApplyPrepStaffSkyDesign')
        staff_bp = unreal.load_asset(STAFF)
        assert 'Progress_StaffCapacity' in widgets(staff_bp), '새 인력 트리 적용 실패'
        staff_cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(STAFF))
        staff_cdo.set_editor_property('selected_count_text_format', unreal.Text('{Count}명'))
        staff_cdo.set_editor_property('daily_notice_text', unreal.Text('고용한 인력은 축제 종료까지 함께합니다.'))
        unreal.BlueprintEditorLibrary.compile_blueprint(staff_bp)
        assert unreal.EditorAssetLibrary.save_loaded_asset(staff_bp, only_if_is_dirty=False)
        mapping = hud_cdo.get_editor_property('panel_layouts')
        staff_layout = mapping[unreal.FF_PrepHudTab.STAFF]
        staff_layout.size = unreal.Vector2D(820, 470)
        mapping[unreal.FF_PrepHudTab.STAFF] = staff_layout
        hud_cdo.set_editor_property('panel_layouts', mapping)
        unreal.BlueprintEditorLibrary.compile_blueprint(hud_bp)
        after = layouts(unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(HUD)))
        expected = json.loads(json.dumps(before_layout))
        expected[str(unreal.FF_PrepHudTab.STAFF)]['size'] = [820., 470.]
        assert after == expected, '인력 Size 외 다른 패널 설정 변경'
        assert hud_tree(hud_bp) == before_tree, 'HUD 위젯/사용자 행 높이/스타일 변경'
        assert unreal.EditorAssetLibrary.save_loaded_asset(hud_bp, only_if_is_dirty=False)
        REPORT.update(layouts_after=after, hud_tree_unchanged=True)
    else:
        REPORT['designer_edits_preserved'] = True
    staff_widgets = widgets(unreal.load_asset(STAFF))
    for name in ('Text_PanelTitle', 'Button_Close', 'Text_StaffCapacity', 'Text_StaffBreakdown',
                 'Progress_StaffCapacity', 'Button_Decrease', 'Button_Increase', 'Text_SelectedCount',
                 'Text_HireUnitCost', 'Text_HireCostTotal', 'Text_GoldAfterValue', 'Text_HireSpentValue',
                 'Button_Hire', 'Text_HireAction', 'Text_HireBlockReason', 'Text_DailyNotice'):
        assert name in staff_widgets, '필수 위젯 누락: ' + name
    REPORT.update(stage='automation', required_widgets=True)
    REPORT_PATH.write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    command('Automation RunTests FestaFiesta.UI.PrepStaff.')
    HANDLE = unreal.register_slate_post_tick_callback(tick)
except Exception:
    finish(traceback.format_exc())
