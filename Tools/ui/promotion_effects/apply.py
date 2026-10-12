"""기존 홍보 카드에 화제성·예상 방문객 행만 추가한다. UE 5.7 전용."""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import shutil
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/PromotionEffects'
OUT.mkdir(parents=True, exist_ok=True)
PACKAGE = '/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepPromotionCard'
SOURCE = ROOT / 'Content' / (PACKAGE[6:] + '.uasset')
NEW_ROWS = {
    'Text_Buzz': ('화제성 +0', 8.0),
    'Text_Forecast': ('예상 방문객 확인 중\n현재 입장료 기준', 12.0),
}
SLOT_FIELDS = ('size', 'padding', 'horizontal_alignment', 'vertical_alignment')
WIDGET_FIELDS = (
    'visibility', 'is_enabled', 'render_opacity', 'render_transform',
    'render_transform_pivot', 'clipping', 'tool_tip_text', 'navigation',
    'font', 'text', 'color_and_opacity', 'auto_wrap_text', 'wrap_text_at',
    'justification', 'shadow_offset', 'shadow_color_and_opacity',
    'min_desired_width', 'line_height_percentage', 'apply_line_height_to_bottom_line',
    'wrapping_policy', 'text_transform_policy', 'text_overflow_policy',
    'brush', 'background', 'brush_color', 'widget_style', 'padding',
    'width_override', 'height_override', 'min_desired_height',
    'max_desired_width', 'max_desired_height', 'min_aspect_ratio', 'max_aspect_ratio',
    'override_width_override', 'override_height_override', 'override_min_desired_width',
    'override_min_desired_height', 'override_max_desired_width', 'override_max_desired_height',
)
REPORT = {'status': 'running', 'asset': PACKAGE, 'created_rows': [], 'saved_assets': [],
          'visual_check': '사용자 플레이 화면 확인 예정'}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def value_text(value):
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    return value.export_text() if hasattr(value, 'export_text') else str(value)


def fields(obj, names):
    result = {}
    for name in names:
        try:
            result[name] = value_text(obj.get_editor_property(name))
        except Exception:
            continue  # 위젯·슬롯 종류에 존재하는 속성만 비교한다.
    return result


def widgets(bp):
    tree = unreal.find_object(bp, 'WidgetTree')
    assert tree, '기존 WidgetTree를 찾지 못했습니다.'
    found = [w for w in unreal.ObjectIterator(unreal.Widget) if w.get_outer() == tree]
    result = {w.get_name(): w for w in found}
    assert len(result) == len(found), '동일한 이름의 위젯이 중복되었습니다.'
    return tree, result


def snapshot(all_widgets):
    result = {}
    for name, widget in all_widgets.items():
        parent = widget.get_parent()
        record = {'class': widget.get_class().get_path_name(),
                  'parent': parent.get_name() if parent else None,
                  'properties': fields(widget, WIDGET_FIELDS)}
        slot = widget.get_editor_property('slot')
        if slot:
            record['slot_class'] = slot.get_class().get_path_name()
            record['slot'] = fields(slot, SLOT_FIELDS + ('layout_data', 'auto_size', 'z_order'))
        if isinstance(widget, unreal.PanelWidget):
            record['children'] = [child.get_name() for child in widget.get_all_children()]
        result[name] = record
    return result


def verify_preserved(before, current):
    after = snapshot(current)
    for name, original in before.items():
        assert name in after, '기존 위젯 누락: ' + name
        actual = after[name]
        expected = dict(original)
        if name == 'SizeBox_Card':
            expected['properties'] = dict(original['properties'])
            for key in ('min_desired_height', 'override_min_desired_height'):
                if key in actual['properties']:
                    expected['properties'][key] = actual['properties'][key]
        if name == 'VerticalBox_Card':
            actual = dict(actual)
            expected['children'] = [n for n in original['children'] if n not in NEW_ROWS]
            actual['children'] = [n for n in actual['children'] if n not in NEW_ROWS]
        assert actual == expected, '기존 위젯의 스타일·배치 변경: ' + name


def verify_rows(current, expected_order, expected_height, source_style):
    column = current['VerticalBox_Card']
    assert [w.get_name() for w in column.get_all_children()] == expected_order, '행 순서 불일치'
    assert float(current['SizeBox_Card'].get_editor_property('min_desired_height')) == expected_height
    for name in NEW_ROWS:
        widget = current.get(name)
        assert isinstance(widget, unreal.TextBlock) and widget.get_parent() == column, '정보 행 누락: ' + name
        assert isinstance(widget.get_editor_property('slot'), unreal.VerticalBoxSlot)
    # 기존 추가 행의 사용자 편집은 재실행 시 유지한다. 새로 생성한 행만 초기 규격과 비교한다.
    for name in REPORT['created_rows']:
        widget = current[name]
        font = widget.get_editor_property('font').copy()
        assert float(font.get_editor_property('size')) == 15.0, '글자 크기 불일치: ' + name
        font.set_editor_property('size', source_style['font_size'])
        assert font.export_text() == source_style['font'], '원본 글꼴 스타일 미보존: ' + name
        assert widget.get_editor_property('color_and_opacity').export_text() == source_style['color']
        assert widget.get_editor_property('justification') == unreal.TextJustify.CENTER
        assert str(widget.get_editor_property('text')) == NEW_ROWS[name][0]
        padding = widget.get_editor_property('slot').get_editor_property('padding')
        assert (padding.left, padding.top, padding.right, padding.bottom) == (0.0, 0.0, 0.0, NEW_ROWS[name][1])


try:
    assert not unreal.EditorLevelLibrary.get_pie_worlds(False), 'PIE 중에는 적용하지 않습니다.'
    bp = unreal.load_asset(PACKAGE)
    assert bp and SOURCE.is_file(), '기존 홍보 카드 에셋이 없습니다.'
    dirty = {p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    assert PACKAGE not in dirty, '홍보 카드의 미저장 변경을 먼저 저장해야 합니다.'
    tree, original_widgets = widgets(bp)
    for name, expected_type in {
        'VerticalBox_Card': unreal.VerticalBox, 'SizeBox_Card': unreal.SizeBox,
        'Text_Effect': unreal.TextBlock, 'Text_Badge': unreal.TextBlock,
    }.items():
        assert isinstance(original_widgets.get(name), expected_type), '기존 구조 불일치: ' + name
    column = original_widgets['VerticalBox_Card']
    frame = original_widgets['SizeBox_Card']
    effect = original_widgets['Text_Effect']
    children = list(column.get_all_children())
    base_children = [w for w in children if w.get_name() not in NEW_ROWS]
    base_names = [w.get_name() for w in base_children]
    effect_index = base_names.index('Text_Effect')
    assert base_names.index('Text_Badge') > effect_index, '효과·배지 순서가 예상과 다릅니다.'
    for name in NEW_ROWS:
        existing = original_widgets.get(name)
        assert existing is None or (isinstance(existing, unreal.TextBlock) and existing.get_parent() == column), \
            '동명 위젯의 타입 또는 부모가 다릅니다: ' + name
    expected_order = base_names[:effect_index + 1] + list(NEW_ROWS) + base_names[effect_index + 1:]
    expected_height = max(320.0, float(frame.get_editor_property('min_desired_height')))
    before = snapshot(original_widgets)
    effect_font = effect.get_editor_property('font')
    source_style = {'font': effect_font.export_text(), 'font_size': effect_font.get_editor_property('size'),
                    'color': effect.get_editor_property('color_and_opacity').export_text()}
    backup = ROOT / 'Saved/UIBackups/PromotionEffects' / datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    backup.mkdir(parents=True, exist_ok=False)
    backup_file = backup / SOURCE.name
    shutil.copy2(SOURCE, backup_file)
    REPORT.update(backup=str(backup_file), sha256_before=digest(SOURCE), before=before)
    assert digest(backup_file) == REPORT['sha256_before'], '백업 해시 불일치'

    bp.modify()
    tree.modify()
    column.modify()
    for name, (preview, bottom_padding) in NEW_ROWS.items():
        if name in original_widgets:
            continue
        widget = unreal.new_object(unreal.TextBlock, outer=tree, name=name)
        assert widget and widget.get_outer() == tree, '정보 행 생성 실패: ' + name
        widget.modify()
        font = effect.get_editor_property('font').copy()
        font.set_editor_property('size', 15)
        widget.set_font(font)
        widget.set_color_and_opacity(effect.get_editor_property('color_and_opacity'))
        widget.set_auto_wrap_text(True)
        widget.set_editor_property('justification', unreal.TextJustify.CENTER)
        widget.set_text(unreal.Text(preview))
        widget.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        slot = column.add_child_to_vertical_box(widget)
        assert slot, '정보 행 연결 실패: ' + name
        slot.set_padding(unreal.Margin(0.0, 0.0, 0.0, bottom_padding))
        slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        REPORT['created_rows'].append(name)

    # InsertChildAt/ShiftChild는 UE 5.7 Python 비노출. tail만 재연결하고 슬롯의 4개 편집 속성을 복원한다.
    _, current = widgets(bp)
    if [w.get_name() for w in column.get_all_children()] != expected_order:
        all_children = list(column.get_all_children())
        first_changed = next(i for i, child in enumerate(all_children) if child.get_name() != expected_order[i])
        tail = all_children[first_changed:]
        saved_slots = {}
        for widget in tail:
            slot = widget.get_editor_property('slot')
            assert isinstance(slot, unreal.VerticalBoxSlot), '기존 행 슬롯 타입 불일치'
            saved_slots[widget.get_name()] = {key: slot.get_editor_property(key) for key in SLOT_FIELDS}
        for widget in tail:
            widget.modify()
            assert column.remove_child(widget), '행 분리 실패: ' + widget.get_name()
        for name in expected_order[first_changed:]:
            slot = column.add_child_to_vertical_box(current[name])
            for key, value in saved_slots[name].items():
                slot.set_editor_property(key, value)
    frame.modify()
    frame.set_min_desired_height(expected_height)
    _, current = widgets(bp)
    assert all(current[name] == widget for name, widget in original_widgets.items()), '기존 위젯 인스턴스 교체'
    verify_preserved(before, current)
    verify_rows(current, expected_order, expected_height, source_style)

    # bIsVariable을 설정하지 않는다. C++ BindWidgetOptional 멤버가 이름 기반 런타임 연결 대상이다.
    # 위젯 GUID 등록은 UE 5.7의 비공개 Python 속성이므로 대상 한정 C++ 편집기 명령을 쓴다.
    unreal.SystemLibrary.execute_console_command(None, 'FF.RegisterPrepPromotionEffectWidgets')
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    _, current = widgets(bp)
    verify_preserved(before, current)
    verify_rows(current, expected_order, expected_height, source_style)

    # 컴파일된 클래스와 템플릿 이름을 확인한다. 실제 화면은 사용자 플레이에서 확인한다.
    card_class = unreal.EditorAssetLibrary.load_blueprint_class(PACKAGE)
    assert card_class, '홍보 카드 컴파일 클래스가 없습니다.'
    REPORT['compiled_class'] = card_class.get_path_name()
    REPORT['binding_widgets'] = list(NEW_ROWS)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False), '홍보 카드 저장 실패'
    REPORT['saved_assets'] = [PACKAGE]
    _, current = widgets(unreal.load_asset(PACKAGE))
    verify_preserved(before, current)
    verify_rows(current, expected_order, expected_height, source_style)
    REPORT.update(status='passed', sha256_after=digest(SOURCE), after=snapshot(current),
                  existing_widgets_preserved=len(before), expected_order=expected_order,
                  min_desired_height=expected_height,
                  binding_policy='C++ BindWidgetOptional의 동일 이름 멤버 연결, bIsVariable 불필요',
                  verification='컴파일·저장 후 로드된 에셋 재조회. 디스크 재로드는 새 프로세스 재실행으로 확인.')
    unreal.log('PROMOTION_EFFECTS_APPLY_PASSED')
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
    unreal.log_error('PROMOTION_EFFECTS_FAIL: ' + REPORT['error'])
finally:
    (OUT / 'apply-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')

started = time.monotonic()


def finish(delta_time):
    if time.monotonic() - started < 2:
        return
    unreal.unregister_slate_post_tick_callback(exit_handle)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
exit_handle = unreal.register_slate_post_tick_callback(finish)
