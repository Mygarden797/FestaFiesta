"""P6: 저장된 32/32 HUD의 CTA 세로 여백만 5에서 2로 보정한다.

일회용 UE 에디터에서 실행한다. 재생성 명령이나 런타임 강제 스타일은 사용하지 않는다.
적용 후 별도로 FestaFiesta.UI.PrepHUD.BrightBottom.RenderedLayout을 실행해야 한다.
"""
from copy import deepcopy
from datetime import datetime
from pathlib import Path
import hashlib
import json
import shutil
import traceback
import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/PrepPanelsIntegration'
HUD_PATH = '/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar'
SOURCE = ROOT / 'Content' / (HUD_PATH[6:] + '.uasset')
REPORT_PATH = OUT / 'hud-cta-fix-report.json'

# 사용자 편집값을 비교하는 읽기 전용 목록이다. 실제 쓰기는 BorderSlot.Padding 한 곳뿐이다.
WIDGET_PROPERTIES = (
    'height_override', 'width_override', 'min_desired_width', 'min_desired_height',
    'max_desired_width', 'max_desired_height', 'font', 'text', 'padding', 'visibility',
    'brush', 'brush_color', 'widget_style', 'style', 'color_and_opacity',
    'content_color_and_opacity', 'background_color', 'foreground_color',
    'horizontal_alignment', 'vertical_alignment', 'render_opacity', 'render_transform',
    'render_transform_pivot', 'clipping', 'is_enabled', 'is_focusable',
    'auto_wrap_text', 'wrap_text_at', 'justification', 'text_overflow_policy',
    'display_label_override', 'designer_icon',
)
SLOT_PROPERTIES = ('layout_data', 'padding', 'size', 'horizontal_alignment',
                   'vertical_alignment', 'auto_size', 'z_order')


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def margin(value):
    return {key: float(getattr(value, key)) for key in ('left', 'top', 'right', 'bottom')}


def value_snapshot(value):
    if isinstance(value, unreal.Margin):
        return margin(value)
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    if hasattr(value, 'export_text'):
        return value.export_text()
    return str(value)


def properties(obj, names):
    result = {}
    for name in names:
        try:
            value = obj.get_editor_property(name)
        except Exception:
            continue  # 이 위젯/슬롯 타입이 갖지 않은 속성만 생략한다.
        result[name] = value_snapshot(value)
    return result


def widgets(bp):
    tree = unreal.find_object(bp, 'WidgetTree')
    assert tree, '저장된 HUD WidgetTree 없음'
    return {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget)
            if w.get_outer() == tree}


def tree_snapshot(bp):
    result = {}
    for name, widget in widgets(bp).items():
        row = dict(type=widget.get_class().get_name(),
                   parent=widget.get_parent().get_name() if widget.get_parent() else None,
                   properties=properties(widget, WIDGET_PROPERTIES))
        if isinstance(widget, unreal.PanelWidget):
            row['children'] = [widget.get_child_at(i).get_name()
                               for i in range(widget.get_children_count())]
        slot = widget.get_editor_property('slot')
        row['slot'] = None if slot is None else dict(
            type=slot.get_class().get_name(), properties=properties(slot, SLOT_PROPERTIES))
        result[name] = row
    return result


def layout_snapshot():
    cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(HUD_PATH))
    return {str(key): dict(dock=str(value.dock), size=[float(value.size.x), float(value.size.y)],
                           offset=[float(value.offset.x), float(value.offset.y)])
            for key, value in cdo.get_editor_property('panel_layouts').items()}


def protected_hashes():
    return {str(path): sha256(path)
            for folder in ('Content/FestaFiesta/UI', 'Content/FestaFiesta/Building/FF_WBP_Building', 'Config')
            for path in (ROOT / folder).rglob('*')
            if path.is_file() and path.suffix in ('.uasset', '.ini') and path != SOURCE}


def apply():
    OUT.mkdir(parents=True, exist_ok=True)
    report = dict(status='running', asset=HUD_PATH, asset_saved=False,
                  rendered_layout_verified=False,
                  scope='Box_Cta 콘텐츠 BorderSlot의 상하 패딩 5→2; 행 높이·전체 PanelLayouts·나머지 HUD 보존')
    hashes = protected_hashes()
    report['protected_files_count'] = len(hashes)
    try:
        assert SOURCE.is_file(), '저장된 HUD 에셋 없음'
        report['source_sha256_before'] = sha256(SOURCE)
        bp = unreal.load_asset(HUD_PATH)
        assert bp, 'HUD 에셋 로드 실패'
        dirty = unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
        assert bp.get_outer() not in dirty, '저장하지 않은 HUD 편집이 있어 적용 중단'
        items = widgets(bp)
        for name in ('SizeBox_MenuRow', 'SizeBox_StatusRow'):
            assert items[name].get_editor_property('height_override') == 32., name + ' 높이가 기준 32와 다름'
        box = items['Box_Cta']
        content = items['HorizontalBox_CtaReason']
        assert isinstance(box, unreal.Border) and content.get_parent() == box, 'CTA 콘텐츠 구조가 기준과 다름'
        slot = content.get_editor_property('slot')
        assert isinstance(slot, unreal.BorderSlot), 'CTA 콘텐츠가 BorderSlot이 아님'
        before_padding = margin(box.get_editor_property('padding'))
        assert margin(slot.get_editor_property('padding')) == before_padding, 'CTA Border와 콘텐츠 슬롯 패딩 불일치'
        assert before_padding['top'] == before_padding['bottom'], 'CTA 상하 여백이 비대칭이므로 적용 중단'
        assert before_padding['top'] in (2., 5.), 'CTA 세로 여백이 알려진 5 또는 적용값 2와 다름'
        before_layouts = layout_snapshot()
        before_tree = tree_snapshot(bp)
        report.update(before_padding=before_padding, before_layouts=before_layouts, before_tree=before_tree)

        if before_padding['top'] == 2.:
            report.update(status='already_applied', after_padding=before_padding,
                          panel_layouts_preserved=True, other_widget_properties_preserved=True)
        else:
            backup = OUT / ('HudCtaBefore-' + datetime.now().strftime('%Y%m%d-%H%M%S-%f')) / SOURCE.relative_to(ROOT)
            backup.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(SOURCE, backup)
            assert sha256(backup) == report['source_sha256_before'], 'HUD 백업 해시 불일치'
            report['backup'] = str(backup)
            after_padding = dict(before_padding, top=2., bottom=2.)
            expected_tree = deepcopy(before_tree)
            expected_tree['Box_Cta']['properties']['padding'] = after_padding
            expected_tree['HorizontalBox_CtaReason']['slot']['properties']['padding'] = after_padding
            # UE5.7 BorderSlot.SetPadding은 슬롯과 부모 Border를 함께 갱신한다.
            # Border.SetPadding만 쓰면 저장된 자식 슬롯의 5가 남아 재동기화 시 되돌아갈 수 있다.
            padding = slot.get_editor_property('padding')
            padding.set_editor_property('top', 2.)
            padding.set_editor_property('bottom', 2.)
            slot.set_padding(padding)
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
            assert tree_snapshot(bp) == expected_tree, '허용한 CTA 패딩 외 HUD 속성 변경'
            assert layout_snapshot() == before_layouts, '기존 PanelLayouts 변경'
            assert protected_hashes() == hashes, '저장 전 보호 파일 변경'
            assert sha256(SOURCE) == report['source_sha256_before'], '적용 중 원본 HUD 디스크가 외부에서 변경됨'
            assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False), 'HUD 저장 실패'
            report.update(status='applied', asset_saved=True, after_padding=after_padding,
                          after_layouts=layout_snapshot(), after_tree=tree_snapshot(bp),
                          panel_layouts_preserved=True, other_widget_properties_preserved=True)
        report['source_sha256_after'] = sha256(SOURCE)
        current = protected_hashes()
        changed = sorted(path for path in set(hashes) | set(current) if hashes.get(path) != current.get(path))
        report.update(protected_files_unchanged=not changed, changed_protected_files=changed)
        assert not changed, '대상 HUD 이외 보호 파일 변경'
    except Exception:
        report.update(status='failed', error=traceback.format_exc())
        unreal.log_error(report['error'])
        raise
    finally:
        REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('PREP_HUD_CTA_FIX: ' + report['status'] + ' / ' + str(REPORT_PATH))
    return report


if __name__ == '__main__':
    try:
        apply()
    finally:
        unreal.SystemLibrary.quit_editor()
