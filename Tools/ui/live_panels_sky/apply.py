"""라이브 P1: 저장값 조사 또는 범위가 제한된 스타일/금화 적용. 전체 HUD 재생성 없음."""
from copy import deepcopy
from datetime import datetime
from pathlib import Path
import hashlib
import json
import os
import shutil
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/LivePanelsSky'
MODE = os.environ.get('FF_LIVE_SKY_MODE', 'Inspect')
OUT.mkdir(parents=True, exist_ok=True)
PANELS = ['/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanel' + name
          for name in ('Operations', 'Facilities', 'Guests', 'Queues', 'SNS', 'Staff', 'Density')]
PREP = '/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar'
LIVE = '/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveHudScreen'
ASSETS = [PREP, LIVE] + PANELS
PROPERTIES = ('height_override', 'width_override', 'min_desired_width', 'min_desired_height',
              'max_desired_width', 'max_desired_height', 'font', 'text', 'padding', 'visibility',
              'brush', 'brush_color', 'widget_style', 'color_and_opacity', 'render_opacity',
              'content_color_and_opacity', 'background_color', 'foreground_color',
              'render_transform', 'clipping', 'auto_wrap_text', 'justification', 'is_enabled',
              'display_label_override', 'designer_icon')
SLOTS = ('layout_data', 'padding', 'size', 'horizontal_alignment', 'vertical_alignment', 'auto_size', 'z_order')


def file_for(asset):
    return ROOT / 'Content' / (asset[6:] + '.uasset')


def hashes():
    return {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
            for folder in ('Content/FestaFiesta/UI', 'Content/FestaFiesta/Building/FF_WBP_Building', 'Config')
            for p in (ROOT / folder).rglob('*') if p.is_file() and p.suffix in ('.uasset', '.umap', '.ini')}


def widgets(bp):
    tree = unreal.find_object(bp, 'WidgetTree')
    assert tree, 'WidgetTree 없음: ' + bp.get_path_name()
    return {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget) if w.get_outer() == tree}


def props(obj, names):
    values = {}
    for name in names:
        try:
            value = obj.get_editor_property(name)
        except Exception:
            continue
        if isinstance(value, unreal.Object):
            values[name] = value.get_path_name()
        elif hasattr(value, 'export_text'):
            values[name] = value.export_text()
        else:
            values[name] = str(value)
    return values


def tree_snapshot(bp):
    result = {}
    for name, w in widgets(bp).items():
        row = dict(type=w.get_class().get_name(), parent=w.get_parent().get_name() if w.get_parent() else None,
                   properties=props(w, PROPERTIES))
        if isinstance(w, unreal.PanelWidget):
            row['children'] = [w.get_child_at(i).get_name() for i in range(w.get_children_count())]
        slot = w.get_editor_property('slot')
        row['slot'] = None if slot is None else dict(type=slot.get_class().get_name(), properties=props(slot, SLOTS))
        result[name] = row
    return result


def cdo_for(path):
    return unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(path))


def layouts(cdo):
    return {str(key): value.export_text() for key, value in cdo.get_editor_property('panel_layouts').items()}


REPORT = dict(status='running', mode=MODE, changed_assets=[], physical_input_tested=False)
BEFORE_HASHES = hashes()
try:
    assert MODE in ('Inspect', 'Apply')
    for path in ASSETS:
        assert file_for(path).is_file(), path + ' 에셋 파일 없음'
    loaded = {path: unreal.load_asset(path) for path in ASSETS}
    dirty = set(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())
    assert not any(bp.get_outer() in dirty for bp in loaded.values()), '저장하지 않은 대상 패키지 있음'
    REPORT['before_trees'] = {p: tree_snapshot(bp) for p, bp in loaded.items()}
    REPORT['before_layouts'] = {p: layouts(cdo_for(p)) for p in (PREP, LIVE)}
    REPORT['live_default_layout'] = cdo_for(LIVE).get_editor_property('default_panel_layout').export_text()
    REPORT['before_gold_formats'] = {p: str(cdo_for(p).get_editor_property('gold_text_format')) for p in (PREP, LIVE)}
    if MODE == 'Apply':
        backup = OUT / ('AssetsBefore-' + datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
        for path in ASSETS:
            src = file_for(path)
            dest = backup / src.relative_to(ROOT)
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dest)
            assert hashlib.sha256(src.read_bytes()).digest() == hashlib.sha256(dest.read_bytes()).digest()
        REPORT['backup'] = str(backup)
        # 공통 패널은 한 번만 이행하는 네이티브 명령. 사용자 편집값은 후속 실행에서 보존한다.
        world = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_world()
        unreal.SystemLibrary.execute_console_command(world, 'FF.ApplyLivePanelSkyDesign')
        for path in PANELS:
            bp = loaded[path]
            assert unreal.EditorAssetLibrary.get_metadata_tag(bp, 'FF.LivePanelSky.FrameVersion') == '1', '공통 틀 적용 실패: ' + path
            before, after = REPORT['before_trees'][path], tree_snapshot(bp)
            assert not (set(before) - set(after)), '기존 위젯 유실: ' + path
            for name, row in before.items():
                assert after[name]['type'] == row['type'], '기존 위젯 형식 변경: ' + name
                if name not in ('Text_PanelTitle', 'Text_CloseGlyph'):
                    assert after[name]['properties'].get('text') == row['properties'].get('text'), '본문 문구 변경: ' + name
            assert 'Sky_SizeBox_CloseIcon' in after and 'Sky_SizeBox_Header' in after
            for scroll_name in ('Scroll_Posts', 'Scroll_Density'):
                if scroll_name in before:
                    assert after[scroll_name] == before[scroll_name], '기존 스크롤 속성/부모 변경: ' + path
                    assert after['VerticalBox_Content']['parent'] == 'Sky_VerticalBox_Sections', '이중 스크롤: ' + path
        for path in (PREP, LIVE):
            bp = loaded[path]
            items = widgets(bp)
            before = tree_snapshot(bp)
            cdo = cdo_for(path)
            current_format = str(cdo.get_editor_property('gold_text_format'))
            assert current_format in ('{Amount} Gold', '{Amount}'), '사용자 금화 포맷이 기준과 달라 자동 변경 중단: ' + current_format
            # 기본 CDO 변경은 이미 저장된 BP override를 수정하지 않으므로 에셋도 명시적으로 이행한다.
            cdo.set_editor_property('gold_text_format', unreal.Text('{Amount}'))
            icon = items.get('Image_Text_Gold') or items.get('Image_Gold')
            if not icon:
                matches = [w for name, w in items.items() if isinstance(w, unreal.Image) and 'Gold' in name]
                assert len(matches) == 1, '금화 아이콘을 하나로 특정할 수 없음'
                icon = matches[0]
            icon.set_color_and_opacity(unreal.LinearColor(1, 1, 1, 1))
            text = items['Text_Gold']
            text.set_text(unreal.Text(str(text.get_text()).removesuffix(' Gold')))
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
            assert layouts(cdo_for(path)) == REPORT['before_layouts'][path], '기존 Layout 저장값 변경'
            after = tree_snapshot(bp)
            allowed = {icon.get_name(): {'color_and_opacity'}, 'Text_Gold': {'text'}}
            comparable_before, comparable_after = deepcopy(before), deepcopy(after)
            for name, names in allowed.items():
                for prop in names:
                    comparable_before[name]['properties'].pop(prop, None)
                    comparable_after[name]['properties'].pop(prop, None)
            assert comparable_before == comparable_after, '허용 금화 표시 외 HUD 트리 변경: ' + path
            assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False), 'HUD 저장 실패'
        REPORT['after_trees'] = {p: tree_snapshot(bp) for p, bp in loaded.items()}
        REPORT['after_layouts'] = {p: layouts(cdo_for(p)) for p in (PREP, LIVE)}
        assert REPORT['after_layouts'] == REPORT['before_layouts'], '대상 외 레이아웃 변경'
        assert cdo_for(LIVE).get_editor_property('default_panel_layout').export_text() == REPORT['live_default_layout']
        REPORT['after_gold_formats'] = {p: str(cdo_for(p).get_editor_property('gold_text_format')) for p in (PREP, LIVE)}
        assert set(REPORT['after_gold_formats'].values()) == {'{Amount}'}
    after_hashes = hashes()
    changes = [p for p in set(BEFORE_HASHES) | set(after_hashes) if BEFORE_HASHES.get(p) != after_hashes.get(p)]
    allowed_paths = {str(file_for(p).relative_to(ROOT)) for p in ASSETS} if MODE == 'Apply' else set()
    assert not (set(changes) - allowed_paths), '허용 범위 밖 파일 변경: ' + str(set(changes) - allowed_paths)
    REPORT.update(status='passed', changed_assets=changes, protected_files_count=len(BEFORE_HASHES)-len(allowed_paths))
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
    unreal.log_error('LIVE_PANELS_SKY_FAILED: ' + REPORT['error'])
finally:
    (OUT / (MODE.lower() + '-report.json')).write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.SystemLibrary.quit_editor()
