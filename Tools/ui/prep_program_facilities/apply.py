"""설치 시설 목록과 위치 선택 행을 추가하고 기존 프로그램 창 편집 값을 확인한다."""
from pathlib import Path
from datetime import datetime
import hashlib
import json
import shutil
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/PrepProgramFacilities'
OUT.mkdir(parents=True, exist_ok=True)
PANEL = '/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelProgram'
CHOICE = '/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepProgramCatalogChoice'
ROW = '/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepProgramFacilityRow'
VERSION = 'FF_PrepProgramFacilitiesVersion'
PROTECTED = (
    ROOT / 'Content/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar.uasset',
    ROOT / 'Content/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelCheck.uasset',
    ROOT / 'Content/FestaFiesta/UI/PrepHUD/WBP_FF_PrepProgramCard.uasset',
)
REPORT = {'status': 'running', 'visual_check': '사용자 에디터 확인 예정'}


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
            pass
    return result


def widgets(bp):
    tree = unreal.find_object(bp, 'WidgetTree')
    return {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget) if w.get_outer() == tree}


def brush_shape(brush, preserve_resource=True, preserve_colors=False):
    names = ['draw_as', 'tiling', 'mirroring', 'image_size', 'margin']
    if preserve_resource:
        names.append('resource_object')
    if preserve_colors:
        names.append('tint_color')
    result = fields(brush, names)
    outline = brush.get_editor_property('outline_settings')
    outline_names = ['corner_radii', 'rounding_type', 'width', 'use_brush_transparency']
    if preserve_colors:
        outline_names.append('color')
    result['outline'] = fields(outline, outline_names)
    return result


def button_shape(widget):
    style = widget.get_editor_property('widget_style')
    result = fields(style, ('normal_padding', 'pressed_padding', 'hovered_slate_sound', 'pressed_slate_sound'))
    for state in ('normal', 'hovered', 'pressed', 'disabled'):
        result[state] = brush_shape(style.get_editor_property(state), preserve_colors=(state == 'disabled'))
    return result


def properties(widget):
    parent = widget.get_parent()
    result = {'type': widget.get_class().get_name(), 'parent': parent.get_name() if parent else None}
    result.update(fields(widget, (
        'font', 'text', 'brush', 'background', 'brush_color', 'color_and_opacity', 'widget_style',
        'padding', 'visibility', 'is_enabled', 'render_opacity', 'render_transform', 'render_transform_pivot',
        'clipping', 'height_override', 'width_override', 'min_desired_height', 'min_desired_width',
        'max_desired_height', 'max_desired_width', 'auto_wrap_text', 'wrap_text_at', 'justification',
        'shadow_offset', 'shadow_color_and_opacity', 'is_variable',
    )))
    slot = widget.get_editor_property('slot')
    if slot:
        result.update({'slot_' + name: value for name, value in fields(slot, (
            'padding', 'size', 'horizontal_alignment', 'vertical_alignment',
            'layout_data', 'auto_size', 'z_order', 'row', 'column',
        )).items()})
    return result


ALLOWED_PANEL = {
    'Border_FacilityGuide': {'background', 'brush'},
    'Text_FacilityGuideTitle': {'text', 'color_and_opacity'},
    'Image_FacilityGuideWarning': {'brush', 'color_and_opacity', 'is_variable'},
    'Text_FacilityLabel': {'is_variable'},
    'Combo_Facilities': {'visibility'},
}
ALLOWED_CHOICE = {
    'Button_Root': {'widget_style'},
    'Text_Chevron': {'color_and_opacity'},
}


def compare_existing(before, after, allowed, choice=False):
    changed = {}
    for name, old in before.items():
        assert name in after, '원본 위젯 사라짐: ' + name
        new = properties(after[name])
        for key in set(old) | set(new):
            if old.get(key) == new.get(key):
                continue
            if key in allowed.get(name, set()):
                continue
            if choice and name == 'Text_Label' and (key == 'parent' or key.startswith('slot_')):
                continue  # 시설명은 같은 복사 그룹 안에서 설치 상태 칩과 나란히 놓는다.
            changed.setdefault(name, []).append(key)
    return changed


try:
    backup = OUT / ('Before-' + datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
    backup.mkdir(parents=True, exist_ok=True)
    for package in (PANEL, CHOICE, ROW):
        source = ROOT / 'Content' / (package[6:] + '.uasset')
        if source.exists():
            shutil.copy2(source, backup / source.name)
    REPORT['backup'] = str(backup)
    protected_hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in PROTECTED}
    panel, choice = unreal.load_asset(PANEL), unreal.load_asset(CHOICE)
    assert panel and choice, '프로그램 패널 또는 시설 종류 WBP 없음'
    panel_widgets, choice_widgets = widgets(panel), widgets(choice)
    panel_before = {name: properties(w) for name, w in panel_widgets.items()}
    choice_before = {name: properties(w) for name, w in choice_widgets.items()}
    panel_already_applied = unreal.EditorAssetLibrary.get_metadata_tag(panel, VERSION) == '1'
    choice_already_applied = unreal.EditorAssetLibrary.get_metadata_tag(choice, VERSION) == '1'
    guide_shape = brush_shape(panel_widgets['Border_FacilityGuide'].get_editor_property('background'))
    guide_icon_shape = brush_shape(panel_widgets['Image_FacilityGuideWarning'].get_editor_property('brush'), preserve_resource=False)
    choice_button_shape = button_shape(choice_widgets['Button_Root'])

    world = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_world()
    unreal.SystemLibrary.execute_console_command(world, 'FF.ApplyPrepProgramFacilities')
    row = unreal.load_asset(ROW)
    assert row, '설치 시설 선택 행 WBP 없음'
    for bp in (panel, choice, row):
        assert unreal.EditorAssetLibrary.get_metadata_tag(bp, VERSION) == '1', '적용 버전 없음: ' + bp.get_name()
    panel_after, choice_after, row_after = widgets(panel), widgets(choice), widgets(row)
    changes = compare_existing(panel_before, panel_after, {} if panel_already_applied else ALLOWED_PANEL)
    changes.update({'choice:' + name: keys for name, keys in compare_existing(
        choice_before, choice_after, {} if choice_already_applied else ALLOWED_CHOICE,
        choice=not choice_already_applied).items()})
    assert not changes, '기존 편집 값 변경: ' + str(changes)
    assert guide_shape == brush_shape(panel_after['Border_FacilityGuide'].get_editor_property('background')), '안내 배경 치수 변경'
    assert guide_icon_shape == brush_shape(panel_after['Image_FacilityGuideWarning'].get_editor_property('brush'), preserve_resource=False), '안내 아이콘 치수 변경'
    assert choice_button_shape == button_shape(choice_after['Button_Root']), '시설 종류 버튼 치수 변경'
    if not choice_already_applied:
        assert properties(choice_after['HorizontalBox_InstallationHeading']).get('slot_padding') == choice_before['Text_Label'].get('slot_padding'), '시설명 원래 슬롯 여백 미보존'
    for path, digest in protected_hashes.items():
        assert hashlib.sha256(Path(path).read_bytes()).hexdigest() == digest, '관련 없는 에셋 변경: ' + path

    required = {
        PANEL: (panel_after, ('Border_FacilityGuide', 'Text_FacilityGuideTitle', 'Image_FacilityGuideWarning',
            'Box_CatalogChoices', 'Combo_Facilities', 'Text_FacilityLabel', 'Text_FacilityEmpty',
            'ScrollBox_FacilityInstances', 'VerticalBox_FacilityInstances')),
        CHOICE: (choice_after, ('Button_Root', 'Text_Label', 'Text_Subtitle', 'Border_Attention',
            'Border_InstallationStatus', 'Text_InstallationStatus')),
        ROW: (row_after, ('Border_Selection', 'Button_Select', 'Button_Locate', 'Text_SelectionMark',
            'Text_FacilityName', 'Text_Location', 'Text_Status')),
    }
    for package, (all_widgets, names) in required.items():
        assert all(name in all_widgets for name in names), '바인딩 누락: ' + package
        for name in names:
            try:
                is_variable = all_widgets[name].get_editor_property('is_variable')
            except Exception:
                continue
            assert is_variable, '변수 표시 없음: ' + package + ':' + name
    assert panel_after['Combo_Facilities'].get_visibility() == unreal.SlateVisibility.COLLAPSED, '이전 시설 선택 상자가 표시됨'
    assert panel_after['VerticalBox_FacilityInstances'].get_parent() == panel_after['ScrollBox_FacilityInstances'], '시설 목록 스크롤 연결 누락'
    assert row_after['Button_Select'].get_parent() == row_after['Button_Locate'].get_parent().get_parent(), '시설 선택과 위치 보기 버튼 분리 구조 불일치'
    settings_class = unreal.load_class(None, '/Script/FestaFiesta.FF_AudioSettings')
    assert settings_class, '오디오 설정 클래스 없음'
    z_order = unreal.get_default_object(settings_class).get_editor_property('playlist_player_z_order')
    assert z_order == 98, '음악 UI 화면 순서 설정 불일치: ' + str(z_order)
    REPORT.update(status='passed', preserved_panel_widgets=len(panel_before),
        preserved_choice_widgets=len(choice_before), protected_assets=list(protected_hashes),
        playlist_player_z_order=z_order, required_widget_bindings={package: list(data[1]) for package, data in required.items()})
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
    unreal.log_error('PREP_PROGRAM_FACILITIES_FAIL: ' + REPORT['error'])
finally:
    (OUT / 'apply-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    exit_after = time.monotonic() + 10.0

    def finish_after_startup(delta_time):
        if time.monotonic() < exit_after:
            return
        unreal.unregister_slate_post_tick_callback(exit_handle)
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()

    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    exit_handle = unreal.register_slate_post_tick_callback(finish_after_startup)
