"""P3 프로그램 WBP 적용·저장과 프로그램 범위 검증. 사용자 인력/HUD 편집 보존."""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import shutil
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/PrepProgramSky'
OUT.mkdir(parents=True, exist_ok=True)
LOG = ROOT / 'Saved/BrightGlobe/prep_program_sky.log'
REPORT_PATH = OUT / 'report.json'
PROGRAM = '/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelProgram'
CARD = '/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepProgramCard'
CHOICE = '/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepProgramFacilityChoice'
HUD = '/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar'
TARGETS = [ROOT / 'Content' / (p[6:] + '.uasset') for p in (PROGRAM, CARD, CHOICE, HUD)]
PROTECTED = [p for base in ('Content/FestaFiesta/UI', 'Content/FestaFiesta/Building/FF_WBP_Building', 'Config')
             for p in (ROOT/base).rglob('*') if p.is_file() and p.suffix in ('.uasset', '.ini') and p not in TARGETS]
hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in PROTECTED}
REPORT = dict(status='running', stage='apply', protected_count=len(hashes), physical_input_tested=False, p4_started=False)
HANDLE = None
START = time.monotonic()
EXPECTED_TESTS = {
    'BlockReason', 'ReasonOrder', 'Quote', 'FacilityNavigation', 'SkyRenderedLayout',
    'FacilityGuide.Compatibility', 'FacilityGuide.Unrestricted',
    'FacilityGuide.ReadableNotice', 'FacilityGuide.MissingCatalogAndDisplayName',
}

def layouts(cdo):
    return {str(k): dict(size=[v.size.x, v.size.y], offset=[v.offset.x, v.offset.y], dock=str(v.dock))
            for k, v in cdo.get_editor_property('panel_layouts').items()}

def widgets(bp):
    return {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget)
            if w.get_outer() and w.get_outer().get_class().get_name() == 'WidgetTree'
            and w.get_outer().get_name() == 'WidgetTree' and w.get_outer().get_outer() == bp}

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
    if error or changed: unreal.log_error('PREP_PROGRAM_SKY_FAIL: ' + str(error or changed))
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()

def tick(delta):
    try:
        assert time.monotonic() - START < 300, '프로그램 자동화 시간 초과'
        content = LOG.read_text(encoding='utf-8', errors='replace') if LOG.exists() else ''
        results = [line for line in content.splitlines() if 'Test Completed. Result=' in line and 'FestaFiesta.UI.PrepProgram.' in line]
        if all(any('FestaFiesta.UI.PrepProgram.'+name+'}' in line for line in results) for name in EXPECTED_TESTS):
            REPORT['automation'] = results
            assert all('Result={Success}' in line for line in results), '프로그램 자동화 실패'
            finish()
    except Exception:
        finish(traceback.format_exc())

try:
    hud_bp = unreal.load_asset(HUD)
    hud_cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(HUD))
    before_layout = layouts(hud_cdo)
    before_tree = hud_tree(hud_bp)
    REPORT['layouts_before'] = before_layout
    existing = unreal.load_asset(PROGRAM)
    already = 'Combo_Facilities' in widgets(existing)
    if not already:
        backup = OUT / ('Before-' + datetime.now().strftime('%Y%m%d-%H%M%S'))
        for src in TARGETS:
            if src.exists():
                dest = backup / src.relative_to(ROOT)
                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(src, dest)
        REPORT['backup'] = str(backup)
        command('FF.ApplyPrepProgramSkyDesign')
        program_bp = unreal.load_asset(PROGRAM)
        assert 'Combo_Facilities' in widgets(program_bp), '새 프로그램 트리 적용 실패'
        program_cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(PROGRAM))
        program_cdo.set_editor_property('card_widget_class', unreal.EditorAssetLibrary.load_blueprint_class(CARD))
        program_cdo.set_editor_property('facility_choice_class', unreal.EditorAssetLibrary.load_blueprint_class(CHOICE))
        program_cdo.set_editor_property('facility_choice_fallback_path', CHOICE)
        program_cdo.set_editor_property('daily_notice_text', unreal.Text('프로그램은 하루 동안 운영되며, 다음 날 다시 편성합니다.'))
        unreal.BlueprintEditorLibrary.compile_blueprint(program_bp)
        assert unreal.EditorAssetLibrary.save_loaded_asset(program_bp, only_if_is_dirty=False)
        mapping = hud_cdo.get_editor_property('panel_layouts')
        program_layout = mapping[unreal.FF_PrepHudTab.PROGRAM]
        program_layout.size = unreal.Vector2D(1120, 600)
        mapping[unreal.FF_PrepHudTab.PROGRAM] = program_layout
        hud_cdo.set_editor_property('panel_layouts', mapping)
        unreal.BlueprintEditorLibrary.compile_blueprint(hud_bp)
        after = layouts(unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(HUD)))
        expected = json.loads(json.dumps(before_layout))
        expected[str(unreal.FF_PrepHudTab.PROGRAM)]['size'] = [1120., 600.]
        assert after == expected, '프로그램 Size 외 다른 패널 설정 변경'
        assert hud_tree(hud_bp) == before_tree, 'HUD 위젯/사용자 행 높이/스타일 변경'
        assert unreal.EditorAssetLibrary.save_loaded_asset(hud_bp, only_if_is_dirty=False)
        REPORT.update(layouts_after=after, hud_tree_unchanged=True)
    else:
        REPORT.update(designer_edits_preserved=True, layouts_after=before_layout)
    program_widgets = widgets(unreal.load_asset(PROGRAM))
    for name in ('Text_PanelTitle', 'Button_Close', 'Box_ProgramCards', 'Text_SelectedProgram',
                 'Text_Day', 'Text_EmptyCatalog', 'Text_CostPreview', 'Text_GoldAfter', 'Text_RevenuePreview',
                 'Box_FacilityRow', 'Combo_Facilities', 'Text_FacilityNotice', 'Box_CatalogChoices',
                 'Button_Schedule', 'Text_BlockReason', 'Text_DailyNotice'):
        assert name in program_widgets, '필수 위젯 누락: ' + name
    REPORT.update(stage='automation', required_widgets=True)
    REPORT_PATH.write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    command('Automation RunTests FestaFiesta.UI.PrepProgram.')
    HANDLE = unreal.register_slate_post_tick_callback(tick)
except Exception:
    finish(traceback.format_exc())
