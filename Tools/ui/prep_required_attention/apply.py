"""점검 WBP 한 개의 필수 안내 위치만 변경하고 기존 위젯/치수/바인딩 보존을 확인한다."""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import re
import shutil
import traceback
import uuid
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
command_line = unreal.SystemLibrary.get_command_line()
output_arg = re.search(r'-PrepRequiredAttentionOutput=(?:"([^"]+)"|(\S+))', command_line)
OUT = Path(output_arg.group(1) or output_arg.group(2)).resolve() if output_arg else (
    ROOT / 'Saved/PrepRequiredAttention' / ('Manual-' + uuid.uuid4().hex))
assert OUT.is_relative_to(ROOT / 'Saved/PrepRequiredAttention'), '프로젝트 적용 결과 폴더 밖의 출력 차단'
OUT.mkdir(parents=True, exist_ok=True)
PANEL = '/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelCheck'
COMMAND = 'FF.ApplyPrepRequiredAttentionDesign'
VERSION_KEY = 'FF_PrepRequiredAttentionVersion'
REPORT = {'status': 'running', 'command': COMMAND, 'target': PANEL, 'started': datetime.now().isoformat()}
PROTECTED = {}
BINDING_FILE = Path(unreal.Paths.project_saved_dir()).resolve() / 'PrepRequiredAttention/binding-validation.json'
MOVED = {'HorizontalBox_OpenGateReason', 'Button_OpenGateReason'}
NEW = {'Border_RequiredAttention', 'HorizontalBox_RequiredAttentionContent',
       'VerticalBox_RequiredAttentionCopy', 'Text_RequiredAttentionTitle'}
PROPERTIES = (
    'visibility', 'is_enabled', 'render_opacity', 'render_transform',
    'render_transform_pivot', 'clipping', 'tool_tip_text', 'font', 'text', 'color_and_opacity',
    'widget_style', 'brush', 'background', 'brush_color', 'padding', 'auto_wrap_text',
    'wrap_text_at', 'justification', 'shadow_offset', 'shadow_color_and_opacity',
    'height_override', 'width_override', 'min_desired_height', 'min_desired_width',
    'max_desired_height', 'max_desired_width', 'override_height_override',
    'override_width_override', 'override_min_desired_height', 'override_min_desired_width',
    'override_max_desired_height', 'override_max_desired_width', 'min_aspect_ratio',
    'max_aspect_ratio', 'override_min_aspect_ratio', 'override_max_aspect_ratio',
    'inner_slot_padding', 'explicit_wrap_size', 'kind', 'enabled', 'title_text',
    'description_text', 'action_text', 'icon_brush',
)
SLOT_PROPERTIES = ('padding', 'size', 'horizontal_alignment', 'vertical_alignment',
                   'layout_data', 'auto_size', 'z_order', 'row', 'column', 'row_span',
                   'column_span', 'layer', 'nudge', 'fill_empty_space', 'fill_span_when_less_than')
DEFAULT_PROPERTIES = (
    'use_compact_layout', 'use_balanced_layout', 'day_label_format', 'daily_goal_value_format',
    'daily_goal_bonus_format', 'gold_value_format', 'held_gold_value_format',
    'required_facility_format', 'no_required_facilities_text', 'open_gate_ready_text',
    'capacity_caution_ratio', 'capacity_danger_ratio', 'estimate_notice_text', 'weather_labels',
    'recommended_facility_rules', 'recommended_count_format', 'recommended_summary_format',
    'recommended_unknown_text', 'required_ready_with_warnings_text', 'open_with_warnings_format',
    'recommended_catalog_unavailable_text', 'recommended_navigation_failed_text',
    'admission_value_format', 'admission_range_format',
)


def asset_file(package):
    return ROOT / 'Content' / (package[6:] + '.uasset')


def digest(path):
    with path.open('rb') as stream:
        result = hashlib.sha256()
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(chunk)
        return result.hexdigest()


def text(value):
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    return value.export_text() if hasattr(value, 'export_text') else str(value)


def fields(obj, names):
    result = {}
    for name in names:
        try:
            value = obj.get_editor_property(name)
        except Exception:
            continue  # 위젯 종류에 없는 속성만 생략한다.
        result[name] = text(value)
    return result


def widgets(bp):
    tree = unreal.find_object(bp, 'WidgetTree')
    assert tree, 'WidgetTree 누락'
    return {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget) if w.get_outer() == tree}


def snapshot(table):
    result = {}
    for name, widget in table.items():
        parent = widget.get_parent()
        slot = widget.get_editor_property('slot')
        result[name] = {
            'class': widget.get_class().get_path_name(),
            'parent': parent.get_name() if parent else None,
            'properties': fields(widget, PROPERTIES),
            'slot': fields(slot, SLOT_PROPERTIES) if slot else None,
        }
        if isinstance(widget, unreal.PanelWidget):
            result[name]['children'] = [child.get_name() for child in widget.get_all_children()]
    return result


def defaults():
    cls = unreal.EditorAssetLibrary.load_blueprint_class(PANEL)
    assert cls, '컴파일 클래스 누락'
    return fields(unreal.get_default_object(cls), DEFAULT_PROPERTIES)


def within(widget, ancestor):
    while widget:
        if widget == ancestor:
            return True
        widget = widget.get_parent()
    return False


def verify(bp, before, native_validation, original_defaults):
    table = widgets(bp)
    current = snapshot(table)
    assert set(before).issubset(current), '기존 위젯 이름 소실'
    assert NEW.issubset(table), '필수 안내 위젯 누락'
    assert unreal.EditorAssetLibrary.get_metadata_tag(bp, VERSION_KEY) == '1', '적용 버전 누락'
    assert native_validation['original_bindings_preserved'], '기존 Blueprint 속성 바인딩 변경'
    assert native_validation['original_variables_preserved'], '기존 위젯 변수 표시 변경'
    assert defaults() == original_defaults, '기존 점검 설정 변경'
    for name, prior in before.items():
        now = current[name]
        assert now['class'] == prior['class'], '기존 위젯 타입 변경: ' + name
        if name not in MOVED:
            assert now['parent'] == prior['parent'], '대상 외 위젯 부모 변경: ' + name
            assert now['slot'] == prior['slot'], '기존 슬롯 치수/여백 변경: ' + name
        for key, value in prior['properties'].items():
            if key == 'visibility' and now.get('children') == [] and any(child in MOVED for child in prior.get('children', [])):
                assert table[name].get_visibility() == unreal.SlateVisibility.COLLAPSED, '빈 기존 컨테이너 공간이 남음'
                continue
            assert now['properties'].get(key) == value, '기존 속성 변경: ' + name + '.' + key
        if 'children' in prior:
            expected = [child for child in prior['children'] if child not in MOVED and child not in NEW]
            actual = [child for child in now['children'] if child not in MOVED and child not in NEW]
            assert actual == expected, '기존 형제 순서 변경: ' + name
    sections = table['VerticalBox_Sections']
    order = [child.get_name() for child in sections.get_all_children()]
    assert order.index('SizeBox_Header') < order.index('Border_RequiredAttention'), '헤더 위에 안내가 배치됨'
    assert order.index('Border_RequiredAttention') + 1 == order.index('ScrollBox_Body'), '스크롤 바로 앞 고정 안내가 아님'
    strip = table['Border_RequiredAttention']
    assert strip.slot.get_editor_property('size').size_rule == unreal.SlateSizeRule.AUTOMATIC, '안내띠가 본문 Fill 공간을 점유함'
    assert table['ScrollBox_Body'].slot.get_editor_property('size').size_rule == unreal.SlateSizeRule.FILL, '본문 Fill 손실'
    for name in MOVED | {'Text_OpenGateReason', 'Text_OpenGateStaffLink', 'Text_RequiredAttentionTitle'}:
        assert within(table[name], strip), '안내 위젯이 고정 영역 밖에 있음: ' + name
        assert not within(table[name], table['VerticalBox_Footer']), '하단 경고가 중복됨: ' + name
    assert table['Button_OpenGateReason'].get_content() == table['Text_OpenGateStaffLink'], '기존 바로가기 라벨 분리됨'
    # protected Bindings/bIsVariable은 Python에서 읽지 않는다.
    # C++의 실제 Initialize + reflection 결과를 필수 성공 조건으로 확인한다.
    assert native_validation['initialized_bindings_valid'] and native_validation['checked_bindings'] == 8
    # 입장료 초안/적용/되돌리기는 기존 부모/속성/가시성을 그대로 검증한다.
    for name in ('Text_AdmissionFeeState', 'Button_AdmissionApply', 'Button_AdmissionReset'):
        assert name in table and table[name].get_parent(), '입장료 조작/상태 안내 누락: ' + name
        assert current[name] == before[name], '입장료 조작/상태 안내 변경: ' + name
    return {'original_widget_count': len(before), 'current_widget_count': len(table),
            'original_widgets_preserved': True, 'original_bindings_preserved': True,
            'original_binding_count': native_validation['original_binding_count'], 'original_settings_preserved': True,
            'native_binding_validation': native_validation,
            'header_body_footer_slots_preserved': True, 'button_sizes_preserved': True,
            'admission_draft_controls_preserved': True, 'fixed_attention_before_scroll': True,
            'footer_reason_moved': True}


def execute(report_name):
    previous = BINDING_FILE.read_bytes() if BINDING_FILE.exists() else None
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.SystemLibrary.execute_console_command(world, COMMAND)
    assert BINDING_FILE.exists(), '실제 위젯 초기화/바인딩 검증 보고서 없음'
    result_bytes = BINDING_FILE.read_bytes()
    assert result_bytes != previous, '이번 명령의 새 바인딩 검증 보고서 없음'
    encoding = 'utf-16' if result_bytes.startswith((b'\xff\xfe', b'\xfe\xff')) else 'utf-8-sig'
    result = json.loads(result_bytes.decode(encoding))
    assert result['status'] == 'passed' and result['checked_bindings'] == 8, '실제 바인딩 검증 실패'
    assert result['initialized_bindings_valid'] and result['original_bindings_preserved'] and result['original_variables_preserved']
    # 명령별 token이 포함된 검증 결과를 실행별 폴더에도 보존한다.
    (OUT / report_name).write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    return result


try:
    assert not unreal.EditorLevelLibrary.get_pie_worlds(False), 'PIE 중 적용 불가'
    target = asset_file(PANEL)
    protected_paths = set((ROOT / 'Content/FestaFiesta/UI').rglob('*.uasset'))
    protected_paths.update((ROOT / 'Content/FestaFiesta/Building/FF_WBP_Building').rglob('*.uasset'))
    protected_paths.discard(target)
    PROTECTED = {path: digest(path) for path in protected_paths}
    bp = unreal.load_asset(PANEL)
    assert bp, '점검 WBP 누락'
    before = snapshot(widgets(bp))
    original_defaults = defaults()
    initial_hash = digest(target)
    backup = OUT / 'Backup'
    backup.mkdir()
    shutil.copy2(target, backup / target.name)
    REPORT['backup'] = str(backup)
    (OUT / 'before.json').write_text(json.dumps({'widgets': before,
        'defaults': original_defaults}, ensure_ascii=False, indent=2), encoding='utf-8')
    applied_validation = execute('binding-applied.json')
    REPORT['applied'] = verify(bp, before, applied_validation, original_defaults)
    applied_hash = digest(target)
    if not NEW.issubset(before):
        assert initial_hash != applied_hash, '적용 후 점검 에셋이 저장되지 않음'
    # 두 번째 호출은 디자이너 편집을 덮어쓰지 않는 REUSE 경로여야 한다.
    after_first = snapshot(widgets(bp))
    repeated_validation = execute('binding-repeated.json')
    assert repeated_validation['mode'] == 'reuse', '두 번째 실행이 REUSE가 아님'
    REPORT['repeated'] = verify(bp, before, repeated_validation, original_defaults)
    assert after_first == snapshot(widgets(bp)), '재실행 시 메모리 위젯/편집값 변경'
    assert applied_hash == digest(target), '재실행 시 에셋 파일 변경'
    (OUT / 'after.json').write_text(json.dumps({'widgets': after_first, 'native_binding_validation': repeated_validation,
        'defaults': defaults()}, ensure_ascii=False, indent=2), encoding='utf-8')
    REPORT.update(status='passed', before_sha256=initial_hash, after_sha256=applied_hash, repeat_unchanged=True)
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
finally:
    REPORT['protected_changes'] = [str(path) for path, value in PROTECTED.items()
                                   if not path.exists() or digest(path) != value]
    REPORT['protected_count'] = len(PROTECTED)
    REPORT['protected_unchanged'] = not REPORT['protected_changes']
    if REPORT['protected_changes']:
        REPORT.update(status='failed', error='점검 WBP 외 UI 에셋 변경 발생')
    (OUT / 'apply-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    if REPORT['status'] == 'passed':
        unreal.log('PREP_REQUIRED_ATTENTION_APPLY_PASSED report=' + str(OUT / 'apply-report.json'))
    else:
        unreal.log_error('PREP_REQUIRED_ATTENTION_FAIL: ' + REPORT.get('error', ''))
    unreal.SystemLibrary.quit_editor()
