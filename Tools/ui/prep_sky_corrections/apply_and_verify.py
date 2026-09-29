"""홍보 확인창·닫기 아이콘만 보정하고 사용자 프로그램 높이950을 유지한다."""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import shutil
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/PrepSkyCorrections'
OUT.mkdir(parents=True, exist_ok=True)
LOG = ROOT / 'Saved/BrightGlobe/prep_sky_corrections.log'
REPORT_PATH = OUT / 'report.json'
BASE = '/Game/FestaFiesta/UI/PrepHUD/'
PANELS = [BASE + 'Panels/WBP_FF_PrepPanel' + name for name in ('Program', 'Staff', 'Promotion')]
HUD = '/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar'
DIALOG = BASE + 'WBP_FF_PrepConfirmSky'
ROW = BASE + 'WBP_FF_PrepConfirmSkyRow'
NEW_NAMES = ['WBP_FF_PrepConfirmSky', 'WBP_FF_PrepConfirmSkyRow',
             'WBP_FF_PrepConfirmSkyCancel', 'WBP_FF_PrepConfirmSkyAccept',
             'Styles/CBS_FF_PrepConfirmSkyCancel', 'Styles/CBS_FF_PrepConfirmSkyAccept',
             'Styles/CTS_FF_PrepConfirmSkyInk', 'Styles/CTS_FF_PrepConfirmSkyLight']
TARGETS = [ROOT / 'Content' / (p[6:] + '.uasset') for p in PANELS + [HUD] + [BASE + n for n in NEW_NAMES]]
PROTECTED = [p for base in ('Content/FestaFiesta/UI', 'Content/FestaFiesta/Building/FF_WBP_Building', 'Config')
             for p in (ROOT/base).rglob('*') if p.is_file() and p.suffix in ('.uasset', '.ini') and p not in TARGETS]
HASHES = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in PROTECTED}
TESTS = ['FestaFiesta.UI.PrepSkyCorrections.RenderedLayout', 'FestaFiesta.UI.PrepPromotion.ConfirmTitle']
REPORT = dict(status='running', protected_count=len(HASHES), physical_input_tested=False)
START = time.monotonic()
HANDLE = None

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
    changed = [p for p,h in HASHES.items() if hashlib.sha256(Path(p).read_bytes()).hexdigest() != h]
    REPORT.update(status='failed' if error or changed else 'passed', error=error, protected_changes=changed,
                  elapsed_seconds=round(time.monotonic()-START, 2))
    REPORT_PATH.write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    if error or changed: unreal.log_error('PREP_SKY_CORRECTION_FAIL: ' + str(error or changed))
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()

def tick(delta):
    try:
        assert time.monotonic()-START < 300, '확인창 검사 시간 초과'
        lines = LOG.read_text(encoding='utf-8', errors='replace').splitlines()
        results = [line for line in lines if 'Test Completed. Result=' in line and any(t+'}' in line for t in TESTS)]
        if all(any(t+'}' in line for line in results) for t in TESTS):
            REPORT['automation'] = results
            assert all('Result={Success}' in line for line in results), '확인창/닫기 검사 실패'
            assert not any('Ensure condition failed:' in line and 'WidgetBlueprintCompiler.cpp' in line for line in lines), '위젯 GUID 컴파일 경고'
            finish()
    except Exception:
        finish(traceback.format_exc())

try:
    backup = OUT / ('Before-' + datetime.now().strftime('%Y%m%d-%H%M%S'))
    for src in TARGETS:
        if src.exists():
            dest = backup / src.relative_to(ROOT)
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dest)
    REPORT['backup'] = str(backup)
    hud_bp = unreal.load_asset(HUD)
    hud_cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(HUD))
    before_layout = layouts(hud_cdo)
    before_hud_tree = hud_tree(hud_bp)
    # 기존 패널의 닫기 자손 외 Designer 수정은 그대로 유지한다.
    ignore = {'Text_Close', 'SizeBox_CloseIcon', 'Image_Close'}
    before_panels = {p: {k:v for k,v in hud_tree(unreal.load_asset(p)).items() if k not in ignore} for p in PANELS}
    command('FF.FixPrepSkyCloseIcons')
    for path in PANELS:
        bp = unreal.load_asset(path)
        assert 'Image_Close' in widgets(bp), path + ' 닫기 아이콘 없음'
        after = {k:v for k,v in hud_tree(bp).items() if k not in ignore}
        assert before_panels[path] == after, path + ' 닫기 외 패널 편집값 변경'
    command('FF.ApplyPrepConfirmSkyDesign')
    dialog_bp = unreal.load_asset(DIALOG)
    assert dialog_bp and 'SizeBox_Dialog' in widgets(dialog_bp), '밝은 확인창 생성 실패'
    dialog_cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(DIALOG))
    row_class = unreal.EditorAssetLibrary.load_blueprint_class(ROW)
    if dialog_cdo.get_editor_property('row_widget_class') != row_class:
        dialog_cdo.set_editor_property('row_widget_class', row_class)
        unreal.BlueprintEditorLibrary.compile_blueprint(dialog_bp)
        assert unreal.EditorAssetLibrary.save_loaded_asset(dialog_bp, only_if_is_dirty=False)
    promotion_bp = unreal.load_asset(PANELS[2])
    promotion_cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(PANELS[2]))
    promotion_cdo.set_editor_property('confirm_dialog_path', DIALOG)
    unreal.BlueprintEditorLibrary.compile_blueprint(promotion_bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(promotion_bp, only_if_is_dirty=False)
    mapping = hud_cdo.get_editor_property('panel_layouts')
    program = mapping[unreal.FF_PrepHudTab.PROGRAM]
    program.size = unreal.Vector2D(program.size.x, 950)
    mapping[unreal.FF_PrepHudTab.PROGRAM] = program
    hud_cdo.set_editor_property('panel_layouts', mapping)
    unreal.BlueprintEditorLibrary.compile_blueprint(hud_bp)
    expected = json.loads(json.dumps(before_layout))
    expected[str(unreal.FF_PrepHudTab.PROGRAM)]['size'][1] = 950.
    after_layout = layouts(unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(HUD)))
    assert after_layout == expected, '프로그램 높이 외 패널 설정 변경'
    assert hud_tree(hud_bp) == before_hud_tree, 'HUD 트리/치수 변경'
    assert unreal.EditorAssetLibrary.save_loaded_asset(hud_bp, only_if_is_dirty=False)
    REPORT.update(stage='automation', layouts_before=before_layout, layouts_after=after_layout,
                  panel_edits_preserved=True, hud_tree_unchanged=True, confirm_path=DIALOG)
    REPORT_PATH.write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    command('Automation RunTests ' + '+'.join(TESTS))
    HANDLE = unreal.register_slate_post_tick_callback(tick)
except Exception:
    finish(traceback.format_exc())


