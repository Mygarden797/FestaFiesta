"""매출 순위 닫기 아이콘만 교체하고 사용자가 정한 패널 치수와 다른 위젯은 보존한다."""
from pathlib import Path
import hashlib
import json
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/RevenueCloseIcon'
OUT.mkdir(parents=True, exist_ok=True)
PACKAGE = '/Game/FestaFiesta/Building/FF_WBP_Building/FacilityRevenue/WBP_FF_FacilityRevenueRanking'
REPORT = {'status': 'running', 'command': 'FF.FixFacilityRevenueCloseIcon'}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def widgets(bp):
    tree = unreal.find_object(bp, 'WidgetTree')
    return {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget) if w.get_outer() == tree}


def snapshot(widget):
    record = {'class': widget.get_class().get_path_name(),
              'parent': widget.get_parent().get_name() if widget.get_parent() else None}
    props = ['visibility', 'render_opacity', 'render_transform']
    if isinstance(widget, unreal.TextBlock):
        props += ['font', 'text', 'color_and_opacity', 'justification']
    elif isinstance(widget, unreal.Border):
        props += ['background', 'brush_color', 'padding']
    elif isinstance(widget, unreal.Button):
        props += ['widget_style']
    elif isinstance(widget, unreal.SizeBox):
        props += ['height_override', 'width_override', 'min_desired_height', 'max_desired_height']
    for prop in props:
        value = widget.get_editor_property(prop)
        record[prop] = value.export_text() if hasattr(value, 'export_text') else str(value)
    slot = widget.get_editor_property('slot')
    if isinstance(slot, unreal.CanvasPanelSlot):
        record['slot'] = slot.get_editor_property('layout_data').export_text()
    elif isinstance(slot, (unreal.HorizontalBoxSlot, unreal.VerticalBoxSlot, unreal.ButtonSlot, unreal.SizeBoxSlot)):
        for prop in ('padding', 'horizontal_alignment', 'vertical_alignment'):
            value = slot.get_editor_property(prop)
            record['slot_' + prop] = value.export_text() if hasattr(value, 'export_text') else str(value)
    return record


try:
    bp = unreal.load_asset(PACKAGE)
    assert bp, '매출 순위 에셋 없음'
    table = widgets(bp)
    before = {name: snapshot(w) for name, w in table.items()}
    before_size = [table['Size_ClarityFrame'].get_editor_property(p) for p in ('width_override', 'height_override')]
    (OUT / 'before.json').write_text(json.dumps(before, ensure_ascii=False, indent=2), encoding='utf-8')
    asset_file = ROOT / (PACKAGE.replace('/Game/', 'Content/') + '.uasset')
    protected = list((ROOT / 'Content/FestaFiesta/UI').rglob('*.uasset'))
    protected += list((ROOT / 'Content/FestaFiesta/Building/FF_WBP_Building').rglob('*.uasset'))
    hashes = {str(p.relative_to(ROOT)): digest(p) for p in protected if p != asset_file}
    world = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_world()
    unreal.SystemLibrary.execute_console_command(world, REPORT['command'])
    after = widgets(bp)
    for name, state in before.items():
        if name in ('Button_Close', 'Button_Close_Label'):
            continue
        assert name in after and snapshot(after[name]) == state, '기존 위젯 변경: ' + name
    close = after['Button_Close']
    icon_box = after['SizeBox_CloseIcon']
    icon = after['Image_Close']
    assert close.get_content() == icon_box and icon.get_parent() == icon_box
    assert icon_box.get_editor_property('width_override') == icon_box.get_editor_property('height_override') == 18
    slot = icon_box.get_editor_property('slot')
    assert slot.get_editor_property('horizontal_alignment') == unreal.HorizontalAlignment.H_ALIGN_CENTER
    assert slot.get_editor_property('vertical_alignment') == unreal.VerticalAlignment.V_ALIGN_CENTER
    padding = slot.get_editor_property('padding')
    assert all(getattr(padding, p) == 0 for p in ('left', 'top', 'right', 'bottom'))
    assert icon.get_editor_property('brush').get_editor_property('resource_object'), '닫기 아이콘 텍스처 없음'
    after_size = [after['Size_ClarityFrame'].get_editor_property(p) for p in ('width_override', 'height_override')]
    assert before_size == after_size, '사용자 패널 크기 변경'
    assert all(digest(ROOT / p) == h for p, h in hashes.items()), '주변 에셋 변경'
    # 재실행이 다시 저장하거나 사용자 편집을 덮지 않는지 확인한다.
    saved_hash = digest(asset_file)
    unreal.SystemLibrary.execute_console_command(world, REPORT['command'])
    assert digest(asset_file) == saved_hash, '재실행 에셋 변경'
    REPORT.update(status='passed', size_before=before_size, size_after=after_size,
                  preserved_widgets=len(before) - 2, protected_unchanged=True,
                  protected_assets=len(hashes), repeat_unchanged=True,
                  icon='공통 닫기 아이콘 18×18 · 중앙 정렬')
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
    unreal.log_error('REVENUE_CLOSE_ICON_FAIL: ' + REPORT['error'])
finally:
    (OUT / 'apply-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.SystemLibrary.quit_editor()
