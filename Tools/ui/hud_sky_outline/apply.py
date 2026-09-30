"""흰 HUD 탭의 하늘색 테두리만 적용한다. 기존 트리·치수·문구는 보존한다."""
from datetime import datetime
from pathlib import Path
import json
import re
import shutil
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/HudSkyOutline'
OUT.mkdir(parents=True, exist_ok=True)
REPORT_PATH = OUT / 'apply-report.json'
PAIRS = [
    ('/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudTabButton',
     '/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar'),
    ('/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveTabButton',
     '/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveHudScreen'),
]
REPORT = {'status': 'running', 'assets': []}


def text(value):
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    if hasattr(value, 'export_text'):
        return value.export_text()
    return re.sub(r'0x[0-9A-Fa-f]+', '<address>', str(value))


def snapshot(bp):
    # 스타일 이외의 디자이너 수정값이 컴파일 후에도 유지되는지 확인한다.
    result = {}
    for widget in unreal.ObjectIterator(unreal.Widget):
        tree = widget.get_outer()
        if not tree or tree.get_class().get_name() != 'WidgetTree' or tree.get_outer() != bp:
            continue
        row = {'class': widget.get_class().get_name(),
               'parent': widget.get_parent().get_name() if widget.get_parent() else None}
        for key in ('visibility', 'text', 'font', 'height_override', 'width_override',
                    'min_desired_width', 'min_desired_height', 'max_desired_width',
                    'max_desired_height', 'override_height_override', 'override_width_override',
                    'padding', 'display_label_override', 'designer_icon'):
            try:
                row[key] = text(widget.get_editor_property(key))
            except Exception:
                pass
        slot = widget.get_editor_property('slot')
        if slot:
            for key in ('layout_data', 'padding', 'size', 'horizontal_alignment', 'vertical_alignment'):
                try:
                    row['slot_' + key] = text(slot.get_editor_property(key))
                except Exception:
                    pass
        result[widget.get_name()] = row
    return result


def sky_color(hex_rgb):
    rgb = [int(hex_rgb[i:i+2], 16) / 255. for i in (0, 2, 4)]
    linear = [c / 12.92 if c <= .04045 else ((c + .055) / 1.055) ** 2.4 for c in rgb]
    return unreal.SlateColor(specified_color=unreal.LinearColor(*linear, 1.))


try:
    if REPORT_PATH.exists():
        assert json.loads(REPORT_PATH.read_text(encoding='utf-8')).get('status') != 'passed', \
            '이미 적용했다. 이후에는 Designer에서 수정한다.'
    backup = OUT / ('Before-' + datetime.now().strftime('%Y%m%d-%H%M%S'))
    for package in [p for pair in PAIRS for p in pair]:
        src = ROOT / 'Content' / (package[6:] + '.uasset')
        dst = backup / src.relative_to(ROOT)
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
    REPORT['backup'] = str(backup)

    for tab_path, hud_path in PAIRS:
        bp = unreal.load_asset(tab_path)
        hud = unreal.load_asset(hud_path)
        before_tab, before_hud = snapshot(bp), snapshot(hud)
        tree = unreal.find_object(bp, 'WidgetTree')
        button = unreal.find_object(tree, 'Button_Root', unreal.Button)
        assert button, tab_path
        style = button.get_editor_property('widget_style')
        fill = style.get_editor_property('normal').get_editor_property('tint_color')
        pads = [text(style.get_editor_property(p)) for p in ('normal_padding', 'pressed_padding')]
        # 채움·모서리·패딩·아이콘 크기는 기존값을 유지하고 테두리만 교체한다.
        for state, color, width in [('normal', 'A5CDDF', 1.25), ('hovered', '80BDD5', 1.75),
                                     ('pressed', '65ABC9', 2.5), ('disabled', 'C9DCE4', 1.)]:
            brush = style.get_editor_property(state)
            brush.set_editor_property('draw_as', unreal.SlateBrushDrawType.ROUNDED_BOX)
            brush.set_editor_property('tint_color', fill)
            outline = brush.get_editor_property('outline_settings')
            outline.set_editor_property('color', sky_color(color))
            outline.set_editor_property('width', width)
            outline.set_editor_property('rounding_type', unreal.SlateBrushRoundingType.FIXED_RADIUS)
            brush.set_editor_property('outline_settings', outline)
            style.set_editor_property(state, brush)
        button.set_style(style)
        cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(tab_path))
        ink = cdo.get_editor_property('normal_content_color')
        cdo.set_editor_property('selected_content_color', ink)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(tab_path))
        assert text(cdo.get_editor_property('selected_content_color')) == text(ink)
        assert snapshot(bp) == before_tab, '탭 치수 또는 내용 변경'
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)

        # 부모에 저장된 자식 인스턴스에도 선택 글자색을 반영한다. 위치·크기는 건드리지 않는다.
        hud_tree = unreal.find_object(hud, 'WidgetTree')
        instances = []
        for item in unreal.ObjectIterator(unreal.FF_HudTabButtonBase):
            if item.get_outer() == hud_tree:
                item.set_editor_property('selected_content_color', item.get_editor_property('normal_content_color'))
                instances.append(item.get_name())
        assert instances, hud_path
        unreal.BlueprintEditorLibrary.compile_blueprint(hud)
        assert snapshot(hud) == before_hud, 'HUD 치수 또는 내용 변경'
        assert unreal.EditorAssetLibrary.save_loaded_asset(hud, only_if_is_dirty=False)
        button = unreal.find_object(unreal.find_object(bp, 'WidgetTree'), 'Button_Root', unreal.Button)
        saved_style = button.get_editor_property('widget_style')
        assert pads == [text(saved_style.get_editor_property(p)) for p in ('normal_padding', 'pressed_padding')]
        row = {'tab': tab_path, 'hud': hud_path, 'tab_instances': instances, 'layout_preserved': True,
               'styles': {s: text(saved_style.get_editor_property(s)) for s in ('normal', 'hovered', 'pressed', 'disabled')},
               'tab_width': before_tab['SizeBox_Tab']['min_desired_width'],
               'menu_height': before_hud['SizeBox_MenuRow']['height_override'],
               'status_height': before_hud['SizeBox_StatusRow']['height_override']}
        REPORT['assets'].append(row)
    REPORT['status'] = 'passed'
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
    unreal.log_error(REPORT['error'])
finally:
    REPORT_PATH.write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('FF_HUD_SKY_OUTLINE_' + REPORT['status'])
    unreal.SystemLibrary.quit_editor()
