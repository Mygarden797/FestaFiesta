"""일일 결산 비서 카드만 적용하고 본문·Footer·주변 에셋 보존을 검사한다."""
from pathlib import Path
import hashlib
import json
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
SAVED = Path(unreal.Paths.project_saved_dir()).resolve()
OUT = ROOT / 'Saved/ReportAssistantFocus'
OUT.mkdir(parents=True, exist_ok=True)
PANEL = '/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelReport'
KEY = 'FF.ReportAssistantFocus.Version'
COMMAND = 'FF.ApplyReportAssistantFocusDesign'
REPORT = {'status': 'running', 'command': COMMAND,
          'visual_check': '실제 글자 줄바꿈·카드 높이는 PIE 화면 확인 필요'}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def widgets(bp):
    tree = unreal.find_object(bp, 'WidgetTree')
    assert tree, '결산 WidgetTree 없음'
    return {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget) if w.get_outer() == tree}


def value(v):
    return v.export_text() if hasattr(v, 'export_text') else str(v)


def direct_section_child(sections, descendant):
    current = descendant
    while current:
        if current.get_parent() == sections:
            return current
        current = current.get_parent()
    raise AssertionError('구분선이 Report_Sections 아래에 없음')


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
        props += ['height_override', 'width_override', 'min_desired_height', 'max_desired_height',
                  'min_desired_width', 'max_desired_width']
    elif isinstance(widget, unreal.Image):
        props += ['brush', 'color_and_opacity']
    for prop in props:
        record[prop] = value(widget.get_editor_property(prop))
    slot = widget.get_editor_property('slot')
    if isinstance(slot, (unreal.HorizontalBoxSlot, unreal.VerticalBoxSlot)):
        for prop in ('size', 'padding', 'horizontal_alignment', 'vertical_alignment'):
            record['slot_' + prop] = value(slot.get_editor_property(prop))
    elif isinstance(slot, unreal.CanvasPanelSlot):
        record['slot_layout'] = value(slot.get_editor_property('layout_data'))
    elif isinstance(slot, (unreal.BorderSlot, unreal.ScrollBoxSlot, unreal.ButtonSlot, unreal.SizeBoxSlot)):
        for prop in ('padding', 'horizontal_alignment', 'vertical_alignment'):
            record['slot_' + prop] = value(slot.get_editor_property(prop))
    return record


panel_file = ROOT / 'Content/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelReport.uasset'
protected_files = set((ROOT / 'Content/FestaFiesta/UI').rglob('*.uasset'))
for folder in ('Shared', 'FacilityRevenue'):
    protected_files.update((ROOT / 'Content/FestaFiesta/Building/FF_WBP_Building' / folder).glob('*.uasset'))
protected_files.discard(panel_file)
protected_before = {str(p.relative_to(ROOT)): digest(p) for p in sorted(protected_files)}
binding_file = SAVED / 'ReportAssistantFocus/binding-validation.json'
previous_binding = binding_file.read_bytes() if binding_file.exists() else None
try:
    bp = unreal.load_asset(PANEL)
    assert bp, '일일 결산 에셋 없음'
    table_before = widgets(bp)
    original = {name: snapshot(w) for name, w in table_before.items()}
    before_dump = {
        'package': PANEL,
        'asset_path': bp.get_path_name(),
        'marker': unreal.EditorAssetLibrary.get_metadata_tag(bp, KEY),
        'asset_sha256': digest(panel_file),
        'widgets': original,
        'children': {name: [child.get_name() for child in widget.get_all_children()]
                     for name, widget in table_before.items()
                     if isinstance(widget, unreal.PanelWidget)},
    }
    # 명령이 사전 검증에서 중단되어도 현재 에셋의 실제 구조를 확인할 수 있어야 한다.
    before_path = OUT / 'before.json'
    before_path.write_text(json.dumps(before_dump, ensure_ascii=False, indent=2), encoding='utf-8')
    REPORT['before_snapshot'] = str(before_path)
    sections_before = [w.get_name() for w in table_before['Report_Sections'].get_all_children()]
    before_hash = digest(panel_file)
    reuse = unreal.EditorAssetLibrary.get_metadata_tag(bp, KEY) == '1'
    world = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_world()
    unreal.SystemLibrary.execute_console_command(world, COMMAND)
    assert binding_file.exists(), '실제 위젯 초기화/바인딩 검사 결과 없음'
    binding_bytes = binding_file.read_bytes()
    assert binding_bytes != previous_binding, '이번 실행의 바인딩 검사 결과 없음'
    encoding = 'utf-16' if binding_bytes.startswith((b'\xff\xfe', b'\xfe\xff')) else 'utf-8-sig'
    binding_result = json.loads(binding_bytes.decode(encoding))
    assert binding_result['status'] == 'passed' and binding_result['checked_bindings'] == 3
    assert binding_result['outside_scroll']
    assert unreal.EditorAssetLibrary.get_metadata_tag(bp, KEY) == '1', '적용 marker 없음'
    after = widgets(bp)
    assert set(original).issubset(after), '기존 결산 위젯 이름 소실'
    for name, record in original.items():
        if name == 'Text_AssistantComment' and not reuse:
            assert after[name].get_class().get_path_name() == record['class'], '기존 비서 본문 타입 변경'
        else:
            assert snapshot(after[name]) == record, '본문·Footer·기존 디자인 변경: ' + name
    sections_after = [w.get_name() for w in after['Report_Sections'].get_all_children()]
    assert [name for name in sections_after if name in sections_before] == sections_before, '기존 섹션 순서 변경'
    group = after['Group_AssistantComment']
    assert isinstance(group, unreal.Border)
    assert group.get_parent() == after['Report_Sections']
    assert sections_after.index('Report_ScrollBody') < sections_after.index('Group_AssistantComment')
    footer_section = direct_section_child(after['Report_Sections'], after['Report_FooterRule'])
    assert sections_after.index('Group_AssistantComment') + 1 == sections_after.index(footer_section.get_name())
    assert sections_after.index(footer_section.get_name()) < sections_after.index('Report_Footer')
    assert isinstance(after['Text_AssistantAdvice'], unreal.TextBlock)
    assert isinstance(after['Text_AssistantComment'], unreal.TextBlock)
    assert after['Text_AssistantAdvice'].get_parent() == after['AssistantFocus_Body']
    assert after['Text_AssistantComment'].get_parent() == after['AssistantFocus_Body']
    assert after['Text_AssistantAdvice'].get_editor_property('font').size > after['Text_AssistantComment'].get_editor_property('font').size
    assert not group.slot.get_editor_property('size').size_rule == unreal.SlateSizeRule.FILL, '고정 카드가 본문 높이를 Fill로 차지함'
    if reuse:
        assert before_hash == digest(panel_file), 'REUSE에서 에셋 변경'
    REPORT.update(status='passed', mode='reuse' if reuse else 'applied', package=PANEL,
                  preserved_widgets=len(original), preserved_body_and_footer=True,
                  native_binding_validation=binding_result,
                  backup_folder=str(SAVED / 'UIBackups/ReportAssistantFocus'))
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
finally:
    protected_after = {str(p.relative_to(ROOT)): digest(p) for p in sorted(protected_files)}
    REPORT['protected_before'] = protected_before
    REPORT['protected_after'] = protected_after
    REPORT['protected_unchanged'] = protected_before == protected_after
    if protected_before != protected_after:
        REPORT.update(status='failed', error=REPORT.get('error', '') + '\n결산 외 보호 에셋 해시 변경')
    (OUT / 'apply-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    if REPORT['status'] != 'passed':
        unreal.log_error('REPORT_ASSISTANT_FOCUS_FAIL: ' + REPORT['error'])
    else:
        unreal.log('REPORT_ASSISTANT_FOCUS_PASSED: ' + REPORT['mode'])
    if not globals().get('FF_BATCH_APPLY', False):
        unreal.SystemLibrary.quit_editor()
