"""P9: 현재 저장된 연결만 읽는다. 에셋 저장/컴파일/PIE/렌더/게임 진행 없음."""
from pathlib import Path
import hashlib
import json
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/LivePanelsSky'
OUT.mkdir(parents=True, exist_ok=True)
LIVE = '/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveHudScreen'
PREP = '/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar'
PREFIX = '/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanel'
STAGES = [('Operations', 'Operations'), ('Facilities', 'Facilities'), ('Guests', 'Guests'),
          ('Queues', 'Queues'), ('SNS', 'Sns'), ('Staff', 'Staff'), ('Density', 'Density')]
ROWS = {'Facilities': ('row_widget_class', 'Rows/WBP_FF_LiveFacilityRow'),
        'Guests': ('row_widget_class', 'Rows/WBP_FF_LiveNeedRow'),
        'Queues': ('row_widget_class', 'Rows/WBP_FF_LiveQueueRow'),
        'SNS': ('post_entry_class', 'Panels/WBP_FF_LiveSnsPostEntry'),
        'Density': ('legend_row_widget_class', 'Rows/WBP_FF_LiveDensityLegendRow')}
REPORT = dict(status='running', visual_check='사용자 확인 예정', changed_assets=[], panels={})
READ_HASHES = {}


def load(path):
    file = ROOT / 'Content' / (path[6:] + '.uasset')
    READ_HASHES[file] = hashlib.sha256(file.read_bytes()).hexdigest()
    bp = unreal.load_asset(path)
    assert bp, '에셋 없음: ' + path
    cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
    assert cls, '생성 클래스 없음: ' + path
    return bp, unreal.get_default_object(cls)


def widgets(bp):
    tree = unreal.find_object(bp, 'WidgetTree')
    assert tree, '위젯 트리 없음: ' + bp.get_path_name()
    return {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget) if w.get_outer() == tree}


def path_of(value):
    return value.get_path_name() if isinstance(value, unreal.Object) else str(value)


try:
    live_bp, live = load(LIVE)
    classes = live.get_editor_property('panel_classes')
    layouts = live.get_editor_property('panel_layouts')
    live_widgets = widgets(live_bp)
    for panel, stage in STAGES:
        prior = json.loads((ROOT / ('Saved/Live' + stage + 'Sky/apply-report.json')).read_text(encoding='utf-8-sig'))
        assert prior['status'] == 'passed', '이전 적용 미완료: ' + panel
        bp, cdo = load(PREFIX + panel)
        tab = getattr(unreal.FF_LiveTab, panel.upper())
        # PanelClasses는 예외 표다. 보통은 HUD의 폴더/이름 규칙으로 조회한다.
        mapped = classes.get(tab)
        mapped_path = path_of(mapped) if mapped else ''
        if mapped_path and mapped_path not in ('None', 'null'):
            assert ('WBP_FF_LivePanel' + panel) in mapped_path, '탭 클래스 연결: ' + panel
            route = mapped_path
        else:
            folder = live.get_editor_property('panel_class_folder')
            asset_name = live.get_editor_property('panel_class_name_format').format(panel)
            route = folder + '/' + asset_name
            assert route == PREFIX + panel, '패널 이름 규칙: ' + route
        assert tab in layouts, '패널 Layout 누락: ' + panel
        assert 'Tab_' + panel in live_widgets, 'HUD 탭 위젯 누락: ' + panel
        layout = layouts[tab]
        assert layout.get_editor_property('width') > 0 and layout.get_editor_property('height') > 0
        names = widgets(bp)
        required = set(prior.get('panel_bindings', prior.get('bindings_present', []))) | {'Button_Close', 'Sky_SizeBox_Header', 'Sky_SizeBox_CloseIcon'}
        assert required <= set(names), '패널 위젯 누락: ' + panel + str(required - set(names))
        record = dict(asset=PREFIX + panel, route=route, layout=layout.export_text(), bindings=len(required),
                      allow_drag_to_move=cdo.get_editor_property('allow_drag_to_move'))
        if panel in ROWS:
            prop, suffix = ROWS[panel]
            expected = '/Game/FestaFiesta/UI/LiveHUD/' + suffix
            ref = cdo.get_editor_property(prop)
            assert ref and path_of(ref).startswith(expected + '.'), '행 클래스 연결: ' + panel
            row_bp, _ = load(expected)
            required_row = set(prior.get('row_bindings', prior.get('entry_bindings', [])))
            assert required_row <= set(widgets(row_bp)), '행 바인딩 누락: ' + panel
            record.update(row_class=path_of(ref), row_bindings=len(required_row))
        REPORT['panels'][panel] = record
    REPORT['bottom_hud'] = {}
    for path in (PREP, LIVE):
        bp, cdo = load(path)
        names = widgets(bp)
        fmt = str(cdo.get_editor_property('gold_text_format'))
        assert fmt == '{Amount}', '하단 금액 포맷: ' + path
        icon = names.get('Image_Text_Gold') or names.get('Image_Gold')
        if not icon:
            matches = [w for name, w in names.items() if isinstance(w, unreal.Image) and 'Gold' in name]
            assert len(matches) == 1, '금화 아이콘을 하나로 특정할 수 없음: ' + path
            icon = matches[0]
        assert icon, '금화 아이콘 없음: ' + path
        brush = icon.get_editor_property('brush')
        texture = brush.get_editor_property('resource_object')
        assert texture, '금화 텍스처 없음'
        REPORT['bottom_hud'][path] = dict(format=fmt, icon=texture.get_path_name(),
            tint=icon.get_editor_property('color_and_opacity').export_text(),
            menu_height=names['SizeBox_MenuRow'].get_editor_property('height_override'),
            status_height=names['SizeBox_StatusRow'].get_editor_property('height_override'))
    assert len({v['icon'] for v in REPORT['bottom_hud'].values()}) == 1, '하단 금화 아이콘 불일치'
    assert len({v['tint'] for v in REPORT['bottom_hud'].values()}) == 1, '하단 금화 Tint 불일치'
    changed = [str(file.relative_to(ROOT)) for file, digest in READ_HASHES.items()
               if hashlib.sha256(file.read_bytes()).hexdigest() != digest]
    assert not changed, '읽기 전용 확인 중 파일 변경: ' + str(changed)
    REPORT.update(status='passed', unchanged_assets=len(READ_HASHES),
                  latest_build='Saved/LiveDensitySky/build.log',
                  pie_run=False, render_run=False, asset_save_run=False)
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
    unreal.log_error(REPORT['error'])
finally:
    (OUT / 'finish-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.SystemLibrary.quit_editor()
