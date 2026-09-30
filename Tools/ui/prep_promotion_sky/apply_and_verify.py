"""P4 홍보 WBP 적용·저장과 홍보 범위 검증. 사용자 인력/HUD 편집 보존."""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import shutil
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/PrepPromotionSky'
OUT.mkdir(parents=True, exist_ok=True)
LOG = ROOT / 'Saved/BrightGlobe/prep_promotion_sky.log'
REPORT_PATH = OUT / 'report.json'
PROMOTION = '/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelPromotion'
CARD = '/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepPromotionCard'
HUD = '/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar'
TARGETS = [ROOT / 'Content' / (p[6:] + '.uasset') for p in (PROMOTION, CARD, HUD)]
PROTECTED = [p for base in ('Content/FestaFiesta/UI', 'Content/FestaFiesta/Building/FF_WBP_Building', 'Config')
             for p in (ROOT/base).rglob('*') if p.is_file() and p.suffix in ('.uasset', '.ini') and p not in TARGETS]
hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in PROTECTED}
REPORT = dict(status='running', stage='apply', protected_count=len(hashes), physical_input_tested=False, p5_started=False)
HANDLE = None
START = time.monotonic()
EXPECTED_TESTS = {'BlockReason', 'ReasonOrder', 'Quote', 'SummaryBalance', 'SkyRenderedLayout'}

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
    if error or changed: unreal.log_error('PREP_PROMOTION_SKY_FAIL: ' + str(error or changed))
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()

def tick(delta):
    try:
        assert time.monotonic() - START < 300, '홍보 자동화 시간 초과'
        content = LOG.read_text(encoding='utf-8', errors='replace') if LOG.exists() else ''
        results = [line for line in content.splitlines() if 'Test Completed. Result=' in line and 'FestaFiesta.UI.PrepPromotion.' in line]
        if all(any('FestaFiesta.UI.PrepPromotion.'+name+'}' in line for line in results) for name in EXPECTED_TESTS):
            REPORT['automation'] = results
            assert all('Result={Success}' in line for line in results), '홍보 자동화 실패'
            finish()
    except Exception:
        finish(traceback.format_exc())

try:
    hud_bp = unreal.load_asset(HUD)
    hud_cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(HUD))
    before_layout = layouts(hud_cdo)
    before_tree = hud_tree(hud_bp)
    REPORT['layouts_before'] = before_layout
    existing = unreal.load_asset(PROMOTION)
    already = 'Border_PurchaseSummary' in widgets(existing)
    if not already:
        backup = OUT / ('Before-' + datetime.now().strftime('%Y%m%d-%H%M%S'))
        for src in TARGETS:
            if src.exists():
                dest = backup / src.relative_to(ROOT)
                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(src, dest)
        REPORT['backup'] = str(backup)
        command('FF.ApplyPrepPromotionSkyDesign')
        card_bp = unreal.load_asset(CARD)
        card_cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(CARD))
        icon_names = {
            unreal.FF_PromotionChoice.NONE: 'Actions/T_FF_LH_Icon_NoData_White_256',
            unreal.FF_PromotionChoice.BASIC: 'Navigation/T_FF_LH_Icon_SNS_White_256',
            unreal.FF_PromotionChoice.FOCUSED: 'Metrics/T_FF_LH_Icon_Visitors_White_256',
            unreal.FF_PromotionChoice.INFLUENCER: 'AudienceGroups/T_FF_LH_Icon_GroupSolo_White_256',
        }
        icon_brushes = {}
        for choice, name in icon_names.items():
            texture = unreal.load_asset('/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/' + name)
            assert texture, '기존 홍보 아이콘 누락: ' + name
            brush = unreal.SlateBrush()
            brush.set_editor_property('resource_object', texture)
            # 표시 크기는 WBP의 SizeBox_PromotionIcon에서 편집한다.
            icon_brushes[choice] = brush
        card_cdo.set_editor_property('choice_icon_brushes', icon_brushes)
        card_cdo.set_editor_property('disabled_content_color', unreal.LinearColor(0.1329, 0.2122, 0.1779, 1.0))
        unreal.BlueprintEditorLibrary.compile_blueprint(card_bp)
        assert unreal.EditorAssetLibrary.save_loaded_asset(card_bp, only_if_is_dirty=False)
        promotion_bp = unreal.load_asset(PROMOTION)
        assert 'Border_PurchaseSummary' in widgets(promotion_bp), '새 홍보 트리 적용 실패'
        promotion_cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(PROMOTION))
        promotion_cdo.set_editor_property('card_widget_class', unreal.EditorAssetLibrary.load_blueprint_class(CARD))
        unreal.BlueprintEditorLibrary.compile_blueprint(promotion_bp)
        assert unreal.EditorAssetLibrary.save_loaded_asset(promotion_bp, only_if_is_dirty=False)
        mapping = hud_cdo.get_editor_property('panel_layouts')
        promotion_layout = mapping[unreal.FF_PrepHudTab.PROMOTION]
        promotion_layout.size = unreal.Vector2D(1140, 620)
        mapping[unreal.FF_PrepHudTab.PROMOTION] = promotion_layout
        hud_cdo.set_editor_property('panel_layouts', mapping)
        unreal.BlueprintEditorLibrary.compile_blueprint(hud_bp)
        after = layouts(unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(HUD)))
        expected = json.loads(json.dumps(before_layout))
        expected[str(unreal.FF_PrepHudTab.PROMOTION)]['size'] = [1140., 620.]
        assert after == expected, '홍보 Size 외 다른 패널 설정 변경'
        assert hud_tree(hud_bp) == before_tree, 'HUD 위젯/사용자 행 높이/스타일 변경'
        assert unreal.EditorAssetLibrary.save_loaded_asset(hud_bp, only_if_is_dirty=False)
        REPORT.update(layouts_after=after, hud_tree_unchanged=True)
    else:
        REPORT.update(designer_edits_preserved=True, layouts_after=before_layout)
    promotion_widgets = widgets(unreal.load_asset(PROMOTION))
    for name in ('Text_PanelTitle', 'Button_Close', 'Box_PromotionCards', 'Text_SelectedPromotion',
                 'Text_PurchaseCost', 'Text_GoldAfterValue', 'Text_GoldAfterLabel', 'Border_PurchaseSummary',
                 'Text_NotAppliedNotice', 'Text_OnceNotice', 'Button_Purchase', 'Text_BlockReason'):
        assert name in promotion_widgets, '필수 위젯 누락: ' + name
    REPORT.update(stage='automation', required_widgets=True)
    REPORT_PATH.write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    command('Automation RunTests FestaFiesta.UI.PrepPromotion.')
    HANDLE = unreal.register_slate_post_tick_callback(tick)
except Exception:
    finish(traceback.format_exc())

