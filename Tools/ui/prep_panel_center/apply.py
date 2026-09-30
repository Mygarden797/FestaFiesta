"""P1: 셋업 네 패널의 저장된 기본 오프셋만 중앙(0,0)으로 맞춘다."""
from datetime import datetime
from pathlib import Path
import json
import shutil
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/PrepPanelCenter'
OUT.mkdir(parents=True, exist_ok=True)
REPORT_PATH = OUT / 'apply-report.json'
HUD_PATH = '/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar'
TARGETS = (unreal.FF_PrepHudTab.PROGRAM, unreal.FF_PrepHudTab.STAFF,
           unreal.FF_PrepHudTab.PROMOTION, unreal.FF_PrepHudTab.CHECK)

def vector(v): return [float(v.x), float(v.y)]

def layout_snapshot(cdo):
    return {str(k): dict(dock=str(v.dock), size=vector(v.size), offset=vector(v.offset))
            for k, v in cdo.get_editor_property('panel_layouts').items()}

def tree_snapshot(bp):
    """사용자가 정한 32/36, 탭 크기, 폰트와 슬롯은 변경하지 않는다."""
    result = {}
    for widget in unreal.ObjectIterator(unreal.Widget):
        tree = widget.get_outer()
        if not tree or tree.get_class().get_name() != 'WidgetTree' or tree.get_outer() != bp:
            continue
        row = dict(type=widget.get_class().get_name(),
                   parent=widget.get_parent().get_name() if widget.get_parent() else None)
        for prop in ('height_override', 'width_override', 'min_desired_width', 'font', 'text',
                     'padding', 'visibility', 'brush_color', 'widget_style'):
            try: row[prop] = str(widget.get_editor_property(prop))
            except Exception: pass
        slot = widget.get_editor_property('slot')
        if slot:
            for prop in ('layout_data', 'padding', 'size', 'horizontal_alignment', 'vertical_alignment'):
                try: row['slot_'+prop] = str(slot.get_editor_property(prop))
                except Exception: pass
        result[widget.get_name()] = row
    return result

report = dict(status='running', asset=HUD_PATH)
try:
    if REPORT_PATH.exists() and json.loads(REPORT_PATH.read_text(encoding='utf-8')).get('status') == 'passed':
        unreal.log('P1 기본값은 이미 적용했다. 이후 위치/크기는 WBP의 Panel Layouts에서 편집한다.')
    else:
        source = ROOT / 'Content' / (HUD_PATH[6:] + '.uasset')
        backup = OUT / ('Before-' + datetime.now().strftime('%Y%m%d-%H%M%S')) / source.relative_to(ROOT)
        backup.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, backup)
        report['backup'] = str(backup)
        bp = unreal.load_asset(HUD_PATH)
        cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(HUD_PATH))
        before = layout_snapshot(cdo)
        before_tree = tree_snapshot(bp)
        report['before'] = before
        expected = json.loads(json.dumps(before))
        layouts = cdo.get_editor_property('panel_layouts')
        for tab in TARGETS:
            assert tab in layouts, '기존 탭 레이아웃 없음: ' + str(tab)
            item = layouts[tab]
            assert item.dock == unreal.FF_PrepPanelDock.CENTER, '기존 중앙 도킹 여부 확인 필요'
            item.offset = unreal.Vector2D(0, 0)
            layouts[tab] = item
            expected[str(tab)]['offset'] = [0., 0.]
        cdo.set_editor_property('panel_layouts', layouts)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        compiled = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(HUD_PATH))
        assert layout_snapshot(compiled) == expected, '대상 Offset 외 레이아웃 변경'
        assert tree_snapshot(bp) == before_tree, '기존 HUD 위젯/스타일 변경'
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
        report.update(status='passed', after=layout_snapshot(compiled),
                      widget_tree_unchanged=True, sizes_preserved=True,
                      build_and_finale_unchanged=True)
        REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
except Exception:
    report.update(status='failed', error=traceback.format_exc())
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log_error(report['error'])
finally:
    unreal.SystemLibrary.quit_editor()
