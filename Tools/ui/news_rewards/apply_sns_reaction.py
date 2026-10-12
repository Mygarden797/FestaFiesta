"""소식 행에 공감 UI만 덧붙이고 기존 위젯·주변 에셋 편집을 보존한다."""
from pathlib import Path
import hashlib
import json
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/NewsRewards'
OUT.mkdir(parents=True, exist_ok=True)
ROW = '/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LiveSnsFlyoutPostRow'
PROTECTED = (
    '/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LiveSnsFlyout',
    '/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LiveSnsPostEntry',
    '/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveUtilityDock',
)
FIELDS = (
    'visibility', 'is_enabled', 'render_opacity', 'render_transform', 'render_transform_pivot',
    'clipping', 'tool_tip_text', 'font', 'text', 'color_and_opacity', 'auto_wrap_text',
    'wrap_text_at', 'justification', 'shadow_offset', 'shadow_color_and_opacity',
    'min_desired_width', 'line_height_percentage', 'apply_line_height_to_bottom_line',
    'wrapping_policy', 'text_transform_policy', 'text_overflow_policy', 'brush',
    'background', 'brush_color', 'widget_style', 'padding', 'width_override',
    'height_override', 'min_desired_height', 'max_desired_width', 'max_desired_height',
    'override_width_override', 'override_height_override', 'override_min_desired_width',
    'override_min_desired_height', 'override_max_desired_width', 'override_max_desired_height',
)
SLOT_FIELDS = ('size', 'padding', 'horizontal_alignment', 'vertical_alignment', 'layout_data', 'auto_size', 'z_order')
REPORT = {'status': 'running', 'visual_check': '실제 화면은 사용자 확인 예정'}


def digest(package):
    return hashlib.sha256((ROOT / 'Content' / (package[6:] + '.uasset')).read_bytes()).hexdigest()


def properties(obj, names):
    values = {}
    for name in names:
        try:
            value = obj.get_editor_property(name)
            values[name] = value.export_text() if hasattr(value, 'export_text') else str(value)
        except Exception:
            continue
    return values


def widgets(bp):
    tree = unreal.find_object(bp, 'WidgetTree')
    assert tree, '기존 WidgetTree 없음'
    return {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget) if w.get_outer() == tree}


def snapshot(all_widgets):
    result = {}
    for name, widget in all_widgets.items():
        parent = widget.get_parent()
        record = {'class': widget.get_class().get_path_name(),
                  'parent': parent.get_name() if parent else None,
                  'properties': properties(widget, FIELDS)}
        slot = widget.get_editor_property('slot')
        if slot:
            record['slot_class'] = slot.get_class().get_path_name()
            record['slot'] = properties(slot, SLOT_FIELDS)
        if isinstance(widget, unreal.PanelWidget):
            record['children'] = [w.get_name() for w in widget.get_all_children()]
        result[name] = record
    return result


try:
    assert not unreal.EditorLevelLibrary.get_pie_worlds(False), 'PIE 중에는 적용하지 않습니다.'
    asset = unreal.load_asset(ROW)
    assert asset, '기존 SNS 행 에셋 없음'
    protected = {p: digest(p) for p in PROTECTED}
    before = snapshot(widgets(asset))
    REPORT['before'] = before
    unreal.SystemLibrary.execute_console_command(None, 'FF.ApplySnsReactionRow')
    assert unreal.EditorAssetLibrary.get_metadata_tag(asset, 'FF_SnsReactionVersion') == '1', '공감 행 적용 실패'
    current = widgets(asset)
    expected_types = {
        'Box_Reaction': unreal.HorizontalBox,
        'Button_React': unreal.Button,
        'Text_ReactLabel': unreal.TextBlock,
        'Text_ReactStatus': unreal.TextBlock,
    }
    for name, widget_type in expected_types.items():
        assert isinstance(current.get(name), widget_type), '공감 바인딩 누락: ' + name
    assert current['Box_Reaction'].get_parent() == current['VerticalBox_Text'], '공감 행 부모 불일치'
    assert current['Button_React'].get_parent() == current['Box_Reaction'], '공감 버튼 부모 불일치'
    assert current['Text_ReactLabel'].get_parent() == current['Button_React'], '공감 버튼 문구 연결 불일치'
    assert current['Text_ReactStatus'].get_parent() == current['Box_Reaction'], '보상 문구 부모 불일치'
    after = snapshot(current)
    original_order = before['VerticalBox_Text']['children']
    expected_order = original_order if 'Box_Reaction' in original_order else original_order + ['Box_Reaction']
    assert after['VerticalBox_Text']['children'] == expected_order, '기존 행 순서 변경'
    for name, original in before.items():
        assert name in after, '기존 위젯 누락: ' + name
        expected = dict(original)
        if name == 'VerticalBox_Text':
            expected['children'] = expected_order
        assert after[name] == expected, '기존 위젯 스타일·배치 변경: ' + name
    assert unreal.EditorAssetLibrary.load_blueprint_class(ROW), '컴파일 클래스 없음'
    assert current['Button_React'].get_editor_property('click_method') == unreal.ButtonClickMethod.PRECISE_CLICK
    assert current['Button_React'].get_editor_property('touch_method') == unreal.ButtonTouchMethod.PRECISE_TAP
    for package, sha256 in protected.items():
        assert digest(package) == sha256, '주변 에셋 변경: ' + package
    first_hash = digest(ROW)
    unreal.SystemLibrary.execute_console_command(None, 'FF.ApplySnsReactionRow')
    assert snapshot(widgets(asset)) == after and digest(ROW) == first_hash, '재실행으로 공감 행 편집 변경'
    REPORT.update(status='passed', saved_asset=ROW, existing_widgets_preserved=len(before),
                  protected_assets=list(PROTECTED), idempotent=True, after=after)
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
    unreal.log_error('SNS_REACTION_FAIL: ' + REPORT['error'])
    raise
finally:
    (OUT / 'sns-reaction-apply-report.json').write_text(
        json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')

# 에디터 종료와 개인 설정·세이브 복원은 상위 적용 도구가 담당한다.
