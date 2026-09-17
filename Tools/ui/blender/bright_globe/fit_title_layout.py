"""P11 타이틀의 기존 1920×1080 배치를 좁은 창 안에 균등 축소한다.

UE 5.7 에디터 Python 전용. 기존 위젯·버튼·그래프를 재생성하지 않는다.
SafeZone → ScaleBox → SizeBox → 기존 Canvas 경로만 추가한다.
완료본 재실행은 파일 해시와 구조를 읽어 검사하고 다시 저장하지 않는다.
중간 실패는 자동 재적용하지 않는다. 보고서와 바이트 백업부터 확인한다.
"""
from datetime import datetime, timezone
from pathlib import Path
import copy
import hashlib
import json
import shutil
import traceback

import unreal


ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
REPORT = ROOT / 'Saved/BrightGlobe/p11_title_layout.json'
TITLE = '/Game/FestaFiesta/UI/Title/GlassOrbit/WBP_TitleMain_MyCasual'
TITLE_FILE = Path('Content/FestaFiesta/UI/Title/GlassOrbit/WBP_TitleMain_MyCasual.uasset')
SCALE_NAME = 'ScaleBox_TitleDesignFit'
FRAME_NAME = 'SizeBox_TitleDesignFrame'
CANVAS_NAME = 'CanvasPanel_Content'
SAFE_SLOT_KEYS = ('is_title_safe', 'safe_area_scale', 'h_align', 'v_align', 'padding')


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def canonical(value):
    if value is None or isinstance(value, (str, int, float, bool)):
        return value
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    if hasattr(value, 'export_text'):
        return value.export_text()
    return str(value)


def props(obj, keys):
    return {key: canonical(obj.get_editor_property(key)) for key in keys}


def write_report(report):
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    temporary = REPORT.with_suffix('.writing.json')
    temporary.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    temporary.replace(REPORT)


def inventory():
    """다른 Content 패키지의 생성·삭제·수정 여부를 파일 크기/mtime으로 감시한다."""
    result = {}
    for path in (ROOT / 'Content').rglob('*'):
        if path.is_file() and path.suffix.lower() in ('.uasset', '.umap', '.uexp', '.ubulk'):
            relative = path.relative_to(ROOT)
            if relative == TITLE_FILE:
                continue
            stat = path.stat()
            result[relative.as_posix()] = [stat.st_size, stat.st_mtime_ns]
    return result


def protected_hashes():
    paths = [ROOT / 'Content/FestaFiesta/Maps/Frontend.umap',
             ROOT / 'Content/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas.uasset']
    folder = ROOT / 'Content/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1/P10'
    paths += sorted(folder.glob('*.uasset'))
    return {path.relative_to(ROOT).as_posix(): digest(path) for path in paths}


def get_tree(bp):
    trees = {obj.get_outer() for obj in unreal.ObjectIterator(unreal.Widget)
             if obj.get_outer() and obj.get_outer().get_class().get_name() == 'WidgetTree'
             and obj.get_outer().get_outer() == bp}
    assert len(trees) == 1, f'원본 WidgetTree 식별 실패: {len(trees)}'
    return next(iter(trees))


def get_widgets(tree):
    result = {obj.get_name(): obj for obj in unreal.ObjectIterator(unreal.Widget)
              if obj.get_outer() == tree}
    for name in ('Overlay_Root', 'SafeZone', CANVAS_NAME, 'Image_BrandMark',
                 'Image_Wordmark', 'VerticalBox_Menu', 'Btn_Play', 'Btn_Options', 'Btn_Quit'):
        assert name in result, f'기존 타이틀 필수 위젯 없음: {name}'
    return result


def snapshot(widgets):
    result = {}
    for name, widget in widgets.items():
        parent = widget.get_parent()
        row = {'class': widget.get_class().get_path_name(),
               'parent': parent.get_name() if parent else None,
               'properties': props(widget, ('visibility', 'render_opacity', 'is_enabled',
                                            'render_transform', 'render_transform_pivot'))}
        if isinstance(widget, unreal.PanelWidget):
            row['children'] = [widget.get_child_at(i).get_name()
                               for i in range(widget.get_children_count())]
        slot = widget.get_editor_property('slot')
        if slot:
            row['slot_class'] = slot.get_class().get_path_name()
            if isinstance(slot, unreal.CanvasPanelSlot):
                row['slot'] = props(slot, ('layout_data', 'auto_size', 'z_order'))
            elif isinstance(slot, unreal.VerticalBoxSlot):
                row['slot'] = props(slot, ('size', 'padding', 'horizontal_alignment', 'vertical_alignment'))
            elif isinstance(slot, (unreal.OverlaySlot, unreal.SizeBoxSlot)):
                row['slot'] = props(slot, ('padding', 'horizontal_alignment', 'vertical_alignment'))
            elif isinstance(slot, unreal.ScaleBoxSlot):
                row['slot'] = props(slot, ('horizontal_alignment', 'vertical_alignment'))
            elif isinstance(slot, unreal.SafeZoneSlot):
                row['slot'] = props(slot, SAFE_SLOT_KEYS)
        if isinstance(widget, (unreal.Image, unreal.Border)):
            row['brush'] = canonical(widget.get_editor_property(
                'brush' if isinstance(widget, unreal.Image) else 'background'))
        if isinstance(widget, unreal.FF_CommonButtonBase):
            row['button'] = props(widget, ('button_label', 'icon_brush', 'button_size_override',
                'normal_content_color', 'highlighted_content_color', 'disabled_content_color',
                'tint_label', 'disabled_opacity', 'focus_on_hover', 'show_chevron_only_when_highlighted'))
        if isinstance(widget, unreal.ScaleBox):
            row['scale_box'] = props(widget, ('stretch', 'stretch_direction', 'ignore_inherited_scale'))
        if isinstance(widget, unreal.SizeBox):
            row['size_box'] = props(widget, ('width_override', 'height_override',
                                             'override_width_override', 'override_height_override'))
        result[name] = row
    return result


def event_objects(bp):
    """직접 건드리지 않는 원본 EventGraph 및 노드의 이름·클래스를 대조한다."""
    graph = unreal.BlueprintEditorLibrary.find_event_graph(bp)
    if graph is None:
        return None
    nodes = sorted((obj.get_name(), obj.get_class().get_path_name())
                   for obj in unreal.ObjectIterator(unreal.Object) if obj.get_outer() == graph)
    return {'graph': graph.get_path_name(), 'objects': [list(row) for row in nodes]}


def validate_structure(widgets):
    safe, scale, frame, canvas = [widgets[name] for name in ('SafeZone', SCALE_NAME, FRAME_NAME, CANVAS_NAME)]
    assert isinstance(scale, unreal.ScaleBox) and isinstance(frame, unreal.SizeBox)
    assert safe.get_children_count() == scale.get_children_count() == frame.get_children_count() == 1
    assert safe.get_child_at(0) == scale and scale.get_child_at(0) == frame and frame.get_child_at(0) == canvas
    assert scale.get_editor_property('stretch') == unreal.Stretch.SCALE_TO_FIT
    assert scale.get_editor_property('stretch_direction') == unreal.StretchDirection.DOWN_ONLY
    assert not scale.get_editor_property('ignore_inherited_scale')
    assert frame.get_editor_property('override_width_override')
    assert frame.get_editor_property('override_height_override')
    assert abs(frame.get_editor_property('width_override') - 1920.) < .001
    assert abs(frame.get_editor_property('height_override') - 1080.) < .001
    scale_slot = frame.get_editor_property('slot')
    assert scale_slot.get_editor_property('horizontal_alignment') == unreal.HorizontalAlignment.H_ALIGN_LEFT
    assert scale_slot.get_editor_property('vertical_alignment') == unreal.VerticalAlignment.V_ALIGN_CENTER
    for name in ('Border_LeftPanel', 'Border_LeftPanelShadow'):
        assert widgets[name].get_visibility() == unreal.SlateVisibility.COLLAPSED


def assert_preserved(before, after):
    assert set(after) == set(before) | {SCALE_NAME, FRAME_NAME}
    comparable = {name: copy.deepcopy(after[name]) for name in before}
    # 허용 변경: SafeZone의 자식 한 개와 Canvas의 부모/슬롯만 바뀐다.
    comparable['SafeZone']['children'] = before['SafeZone']['children']
    for key in ('parent', 'slot_class', 'slot'):
        comparable[CANVAS_NAME][key] = before[CANVAS_NAME][key]
    assert comparable == before, '허용한 컨테이너 경로 외에 기존 위젯 속성이 바뀜'
    assert after[SCALE_NAME]['slot'] == before[CANVAS_NAME]['slot'], 'SafeZone 슬롯 설정 보존 실패'


def run():
    previous = json.loads(REPORT.read_text(encoding='utf-8')) if REPORT.exists() else None
    report = {'status': 'preflight', 'saved_assets': [], 'mutation_started': False,
              'design_size': [1920, 1080], 'shrink_only': True,
              'alignment': 'left_center', 'separate_process_verified': False,
              'started_at': datetime.now(timezone.utc).isoformat()}
    try:
        level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        assert not level.is_in_play_in_editor(), 'PIE 실행 중에는 적용하지 않음'
        assert not previous or previous.get('status') == 'passed', '이전 미완료 보고서/백업을 먼저 확인해야 함'
        bp = unreal.load_asset(TITLE)
        assert isinstance(bp, unreal.WidgetBlueprint), '타이틀 WBP 로드 실패'
        tree = get_tree(bp)
        widgets = get_widgets(tree)
        if previous:
            assert digest(ROOT / TITLE_FILE) == previous['title_sha256_after'], '완료 후 타이틀이 수정되어 자동 재적용하지 않음'
            validate_structure(widgets)
            assert snapshot(widgets) == previous['after'], '완료 보고서와 타이틀 구조가 다름'
            unreal.log('P11_TITLE_LAYOUT_ALREADY_VERIFIED: 저장 없이 기존 적용 상태 확인')
            return previous
        assert SCALE_NAME not in widgets and FRAME_NAME not in widgets, '보고서 없는 기존 컨테이너 발견: 수동 확인 필요'
        safe, canvas = widgets['SafeZone'], widgets[CANVAS_NAME]
        assert isinstance(safe, unreal.SafeZone) and isinstance(canvas, unreal.CanvasPanel)
        assert safe.get_children_count() == 1 and safe.get_child_at(0) == canvas
        assert safe.get_parent() == widgets['Overlay_Root']
        old_slot = canvas.get_editor_property('slot')
        assert isinstance(old_slot, unreal.SafeZoneSlot)
        assert old_slot.get_editor_property('h_align') == unreal.HorizontalAlignment.H_ALIGN_FILL
        assert old_slot.get_editor_property('v_align') == unreal.VerticalAlignment.V_ALIGN_FILL
        safe_settings = {key: old_slot.get_editor_property(key) for key in SAFE_SLOT_KEYS}
        report['before'] = snapshot(widgets)
        report['event_objects_before'] = event_objects(bp)
        report['protected_hashes_before'] = protected_hashes()
        files_before = inventory()
        report['other_package_count'] = len(files_before)
        original = ROOT / TITLE_FILE
        report['title_sha256_before'] = digest(original)
        backup = ROOT / 'Saved/BrightGlobeBefore' / ('P11-' + datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
        backup_file = backup / TITLE_FILE
        backup_file.parent.mkdir(parents=True, exist_ok=False)
        shutil.copy2(original, backup_file)
        assert digest(backup_file) == report['title_sha256_before']
        report['backup_file'] = str(backup_file)
        report.update(status='applying', mutation_started=True)
        write_report(report)
        for obj in (bp, tree, safe, canvas):
            obj.modify()
        # UE 5.7 UWidgetTree::ConstructWidget도 같은 Outer를 쓰는 NewObject 경로다.
        scale = unreal.new_object(unreal.ScaleBox, outer=tree, name=SCALE_NAME)
        frame = unreal.new_object(unreal.SizeBox, outer=tree, name=FRAME_NAME)
        assert scale and frame and scale.get_outer() == frame.get_outer() == tree
        for obj in (scale, frame):
            obj.modify()
            obj.set_visibility(unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
        scale.set_stretch(unreal.Stretch.SCALE_TO_FIT)
        scale.set_stretch_direction(unreal.StretchDirection.DOWN_ONLY)
        scale.set_ignore_inherited_scale(False)
        frame.set_width_override(1920.)
        frame.set_height_override(1080.)
        assert safe.remove_child(canvas)
        safe_slot = safe.add_child(scale)
        assert isinstance(safe_slot, unreal.SafeZoneSlot)
        for key, value in safe_settings.items():
            safe_slot.set_editor_property(key, value)
        scale_slot = scale.add_child(frame)
        scale_slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_LEFT)
        scale_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        frame_slot = frame.add_child(canvas)
        frame_slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        frame_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
        frame_slot.set_padding(unreal.Margin(0., 0., 0., 0.))
        current = get_widgets(tree)
        assert all(current[name] == widget for name, widget in widgets.items()), '기존 위젯 인스턴스가 교체됨'
        validate_structure(current)
        assert_preserved(report['before'], snapshot(current))
        assert event_objects(bp) == report['event_objects_before']
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        compiled_widgets = get_widgets(get_tree(bp))
        validate_structure(compiled_widgets)
        report['after'] = snapshot(compiled_widgets)
        assert_preserved(report['before'], report['after'])
        assert event_objects(bp) == report['event_objects_before'], '컴파일 후 원본 그래프 개체 변경'
        assert protected_hashes() == report['protected_hashes_before']
        assert inventory() == files_before
        write_report(report)
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp, False), '타이틀 WBP 저장 실패'
        report['saved_assets'] = [TITLE]
        report['title_sha256_after'] = digest(original)
        report['protected_hashes_after'] = protected_hashes()
        assert report['protected_hashes_after'] == report['protected_hashes_before']
        assert inventory() == files_before, '타이틀 외 Content 패키지 파일이 변경됨'
        report.update(status='passed', other_packages_unchanged=True,
                      finished_at=datetime.now(timezone.utc).isoformat())
        write_report(report)
        unreal.log('P11_TITLE_LAYOUT_PASSED: 기존 타이틀 배치를 좁은 창에서 균등 축소, 왼쪽/세로 중앙 정렬')
        return report
    except Exception:
        if not previous or previous.get('status') != 'passed':
            report.update(status='failed', error=traceback.format_exc())
            write_report(report)
        unreal.log_error('BRIGHT_GLOBE_FAIL ' + traceback.format_exc())
        raise


if __name__ == '__main__':
    run()
