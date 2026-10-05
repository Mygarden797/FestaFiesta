"""P1: 실제 PIE에서 셋업 네 패널의 화면 중앙 배치·편집값 보존을 검사한다.

일회용 렌더 가능한 에디터에서 실행한다. 에셋·설정 저장, 시설 설치, 구매, 단계 변경은 없다.
Saved/PrepPanelCenter/verify-request.json으로 cases/log_path/quit_editor를 지정할 수 있다.
PIE 창 장식과 실제 viewport 크기를 분리해 보고하며, 물리 마우스 검증으로 주장하지 않는다.
"""
from pathlib import Path
import gc
import hashlib
import json
import os
import struct
import time
import traceback
import unreal


ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = ROOT / os.environ.get('FF_PREP_PANEL_VERIFY_OUT', 'Saved/PrepPanelCenter')
OUT.mkdir(parents=True, exist_ok=True)
REQUEST = OUT / 'verify-request.json'
OPTIONS = json.loads(REQUEST.read_text(encoding='utf-8-sig')) if REQUEST.exists() else {}
LOG = Path(OPTIONS.get('log_path', str(OUT / 'verify.log')))
CASES = OPTIONS.get('cases', [
    ['small_16x9', 1280, 720], ['standard_16x9', 1920, 1080],
    ['ultrawide_21x9', 2560, 1080],
])
TABS = ['PROGRAM', 'STAFF', 'PROMOTION', 'CHECK']
HUD_PATH = '/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar'
AUTOMATION_TESTS = OPTIONS.get('automation_tests', [
    'FestaFiesta.UI.PrepHUD.Center.ViewportGeometry',
    'FestaFiesta.UI.PrepHUD.Center.ResizeAndFirstFrame',
    'FestaFiesta.UI.PrepHUD.BrightBottom.RenderedLayout',
    'FestaFiesta.UI.LiveHUD.BrightBottom.RenderedLayout',
])
LEVEL = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
STATE = dict(stage='setup', next=time.monotonic() + 1, start=time.monotonic(),
             busy=False, case_index=0, tab_index=0)
REPORT = dict(status='running', cases=[], images=[], checks=[],
              physical_input_tested=False, asset_save_requested=False,
              scope='중앙 좌표·크기·임시 위치 편집·재오픈 및 배치 Bottom/하단 HUD 회귀. 경제 동작 검증 아님.')
HANDLE = None
BEFORE = {}


def protected_paths():
    """이 작업에서 읽는 UI/맵과 프로젝트 설정의 추가·삭제·내용 변경을 모두 검사한다."""
    folders = [ROOT / 'Content/FestaFiesta/UI/PrepHUD',
               ROOT / 'Content/FestaFiesta/UI/LiveHUD',
               ROOT / 'Content/FestaFiesta/Building/FF_WBP_Building',
               ROOT / 'Config']
    paths = {p for folder in folders if folder.exists() for p in folder.rglob('*')
             if p.is_file() and p.suffix.lower() in ('.uasset', '.umap', '.ini')}
    paths.update(p for p in [ROOT / 'Content/FestaFiesta/Maps/Frontend.umap',
                 ROOT / 'Content/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas.uasset'] if p.exists())
    return sorted(paths)


def hashes():
    result = {}
    for path in protected_paths():
        digest = hashlib.sha256()
        with path.open('rb') as stream:
            for block in iter(lambda: stream.read(1024 * 1024), b''):
                digest.update(block)
        result[path.relative_to(ROOT).as_posix()] = digest.hexdigest()
    return result


def write_report():
    REPORT['stage'] = STATE['stage']
    REPORT['elapsed_seconds'] = round(time.monotonic() - STATE['start'], 2)
    (OUT / 'report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')


def advance(stage, delay=.5):
    STATE.update(stage=stage, next=time.monotonic() + delay)
    write_report()


def world():
    return unreal.EditorLevelLibrary.get_game_world()


def command(value):
    unreal.SystemLibrary.execute_console_command(world() or LEVEL.get_world(), value)


def subsystem(cls):
    instance = unreal.GameplayStatics.get_game_instance(world())
    return next((obj for obj in unreal.ObjectIterator(cls) if obj.get_outer() == instance), None)


def hud():
    return next((obj for obj in unreal.ObjectIterator(unreal.FF_PrepHudBar)
                 if obj.get_world() == world() and obj.get_parent() is not None
                 and obj.get_visibility() not in (unreal.SlateVisibility.COLLAPSED,
                                                  unreal.SlateVisibility.HIDDEN)), None)


def margin(value):
    return [float(value.left), float(value.top), float(value.right), float(value.bottom)]


def authored_widget_values(widget):
    row = dict(type=widget.get_class().get_name())
    for name in ('height_override', 'width_override'):
        try:
            row[name] = float(widget.get_editor_property(name))
        except Exception:
            pass
    try:
        row['padding'] = margin(widget.get_editor_property('padding'))
    except Exception:
        pass
    slot = widget.get_editor_property('slot')
    if slot:
        try:
            row['slot_padding'] = margin(slot.get_editor_property('padding'))
        except Exception:
            pass
    try:
        font = widget.get_editor_property('font')
        row['font'] = dict(size=font.size, typeface=str(font.typeface_font_name),
                           letter_spacing=font.letter_spacing,
                           font_object=font.font_object.get_path_name() if font.font_object else None)
    except Exception:
        pass
    return row


def read_saved_hud():
    bp = unreal.load_asset(HUD_PATH)
    names = ('SizeBox_MenuRow', 'SizeBox_StatusRow', 'Box_Cta', 'Text_CtaReason')
    rows = {}
    for widget in unreal.ObjectIterator(unreal.Widget):
        tree = widget.get_outer()
        if (widget.get_name() in names and tree and tree.get_class().get_name() == 'WidgetTree'
                and tree.get_outer() == bp):
            rows[widget.get_name()] = authored_widget_values(widget)
    assert all(name in rows for name in names), ('저장 HUD 기준 위젯 누락', list(rows))
    return dict(asset=HUD_PATH, widgets=rows,
                menu_height=rows['SizeBox_MenuRow']['height_override'],
                status_height=rows['SizeBox_StatusRow']['height_override'])


def find_runtime_child(owner, name):
    try:
        bound = owner.get_editor_property(name)
        if bound:
            return bound
    except Exception:
        pass
    return next((item for item in unreal.ObjectIterator(unreal.Widget)
                 if item.get_name() == name and item.get_outer()
                 and item.get_outer().get_class().get_name() == 'WidgetTree'
                 and item.get_outer().get_outer() == owner), None)


def geometry_diagnostics():
    rows = []
    selected = hud()
    for bar in unreal.ObjectIterator(unreal.FF_PrepHudBar):
        if bar.get_world() != world():
            continue
        row = dict(path=bar.get_path_name(), class_path=bar.get_class().get_path_name(),
                   selected=bar == selected, visibility=str(bar.get_visibility()),
                   parent=bar.get_parent().get_path_name() if bar.get_parent() else None)
        try:
            row['tick_frequency'] = str(bar.get_editor_property('tick_frequency'))
        except Exception:
            pass
        row['children'] = {}
        for name in ('Panel_PanelHost', 'SizeBox_MenuRow', 'SizeBox_StatusRow', 'Box_Cta', 'Text_CtaReason'):
            child = find_runtime_child(bar, name)
            item = dict(found=child is not None)
            if child:
                item.update(path=child.get_path_name(), visibility=str(child.get_visibility()),
                            authored_values=authored_widget_values(child))
                child_slot = child.get_editor_property('slot')
                item['slot_type'] = child_slot.get_class().get_name() if child_slot else None
                if child_slot:
                    try:
                        item['slot_layout_data'] = str(child_slot.get_editor_property('layout_data'))
                    except Exception:
                        pass
            row['children'][name] = item
        host = find_runtime_child(bar, 'Panel_PanelHost')
        row['geometry_sizes'] = {}
        for name, widget in [('hud', bar), ('host', host), ('canvas', host.get_parent() if host else None)]:
            if not widget:
                continue
            row['geometry_sizes'][name] = {}
            for kind in ('cached', 'tick_space', 'paint_space'):
                try:
                    geo = getattr(widget, 'get_' + kind + '_geometry')()
                    row['geometry_sizes'][name][kind] = pair(unreal.SlateLibrary.get_local_size(geo))
                except Exception as exc:
                    row['geometry_sizes'][name][kind] = str(exc)
        row['viewport_geometry_size'] = pair(unreal.SlateLibrary.get_local_size(
            unreal.WidgetLayoutLibrary.get_viewport_widget_geometry(bar)))
        row['dpi'] = unreal.WidgetLayoutLibrary.get_viewport_scale(bar)
        rows.append(row)
    controller = unreal.GameplayStatics.get_player_controller(world(), 0)
    return dict(candidates=rows, selected_path=selected.get_path_name() if selected else None,
                viewport_controller_pixels=list(controller.get_viewport_size()) if controller else None)


def pair(value):
    return [round(value.x, 5), round(value.y, 5)]


def vector(values):
    return unreal.Vector2D(values[0], values[1])


def assert_pair(actual, expected, message, tolerance=.1):
    assert max(abs(a - b) for a, b in zip(actual, expected)) <= tolerance, (
        message, actual, expected)


def layout_values(layout):
    return dict(dock=str(layout.get_editor_property('dock')),
                size=pair(layout.get_editor_property('size')),
                offset=pair(layout.get_editor_property('offset')))


def layout_table(bar):
    return {str(key): layout_values(value) for key, value in
            bar.get_editor_property('panel_layouts').items()}


def geometry_values(widget):
    geometry = widget.get_cached_geometry()
    size = unreal.SlateLibrary.get_local_size(geometry)
    origin = unreal.SlateLibrary.local_to_absolute(geometry, unreal.Vector2D(0, 0))
    center = unreal.SlateLibrary.local_to_absolute(geometry, unreal.Vector2D(size.x / 2, size.y / 2))
    pixel_min, _ = unreal.SlateLibrary.local_to_viewport(world(), geometry, unreal.Vector2D(0, 0))
    pixel_max, _ = unreal.SlateLibrary.local_to_viewport(world(), geometry, size)
    return dict(size=pair(size), absolute_origin=pair(origin), absolute_center=pair(center),
                pixel_bounds=[pixel_min.x, pixel_min.y, pixel_max.x, pixel_max.y])


def case_report():
    return REPORT['cases'][-1]


def inspect_center(label, expected=None):
    bar = hud()
    assert bar, 'PIE Prep HUD 인스턴스를 찾지 못함'
    host = bar.get_editor_property('Panel_PanelHost')
    assert host.get_visibility() not in (unreal.SlateVisibility.COLLAPSED,
                                         unreal.SlateVisibility.HIDDEN), '중앙 패널이 보이지 않음'
    current = layout_values(bar.get_open_panel_layout())
    if expected is not None:
        assert current == expected, ('설정 보존', current, expected)
    python_rect = geometry_values(host)
    if not case_report().get('hud_diagnostics'):
        case_report()['hud_diagnostics'] = geometry_diagnostics()
    # 이 환경에서는 Python의 FGeometry 전달값이 모두 0으로 보이는 반면 C++ 레이아웃은
    # 유효하다. 에디터 전용 읽기 probe가 실제 Slate geometry를 숫자 JSON으로 내보낸다.
    # 빈 Python 값을 대체 계산하여 통과시키지 않고 같은 기존 실측 판정을 native에 적용한다.
    tag = f"case_{STATE['case_index']}_sample_{len(case_report()['measurements'])}_{time.monotonic_ns()}"
    command(OPTIONS.get('native_probe_command', 'FF.PrepPanelCenterProbe') + ' ' + tag)
    native_path = ROOT / OPTIONS.get('native_probe_path', 'Saved/PrepPanelCenter/native-geometry.json')
    assert native_path.exists(), 'Native geometry probe 결과 파일 없음'
    native = json.loads(native_path.read_text(encoding='utf-8-sig'))
    case_report()['last_native_probe'] = native
    assert native.get('tag') == tag, ('오래된 Native probe 결과를 사용할 수 없음', tag, native.get('tag'))
    assert native.get('ok'), ('Native geometry probe 실패', native.get('error'))
    assert_pair(native['layout_size'], current['size'], 'Native와 Python의 동일 Layout Size')
    assert_pair(native['layout_offset'], current['offset'], 'Native와 Python의 동일 Layout Offset')
    rect = dict(size=native['host_size'], absolute_center=native['host_center_absolute'],
                pixel_bounds=native['host_pixel_bounds'])
    assert min(rect['size']) > 0, '실제 Native PIE geometry 없음; 렌더만으로 통과 처리하지 않음'
    assert_pair(rect['size'], current['size'], '패널 Size를 임의로 바꾸지 않음')

    viewport_local = native['viewport_size']
    assert min(viewport_local) > 0, '실제 Native viewport geometry 없음'
    # canvas_offset_absolute는 원점 좌표가 아니라 LocalToAbsolute(Offset)-LocalToAbsolute(0)의 이동량이다.
    expected_absolute = [center + delta for center, delta in zip(
        native['viewport_center_absolute'], native['canvas_offset_absolute'])]
    assert_pair(rect['absolute_center'], expected_absolute, '실제 화면 중심 + 편집 Offset', .75)

    viewport_pixels = native['controller_viewport_px']
    assert_pair(viewport_pixels, list(unreal.GameplayStatics.get_player_controller(world(), 0).get_viewport_size()),
                'Native와 Python의 동일 viewport')
    dpi = native['dpi']
    assert abs(dpi - unreal.WidgetLayoutLibrary.get_viewport_scale(bar)) < .001, 'Native와 Python의 동일 DPI'
    pixel_center = [(rect['pixel_bounds'][0] + rect['pixel_bounds'][2]) / 2,
                    (rect['pixel_bounds'][1] + rect['pixel_bounds'][3]) / 2]
    expected_pixels = [viewport_pixels[0] / 2 + current['offset'][0] * dpi,
                       viewport_pixels[1] / 2 + current['offset'][1] * dpi]
    assert_pair(pixel_center, expected_pixels, 'DPI를 한 번 적용한 실제 화면 좌표', .75)
    row = dict(label=label, layout=current, geometry=rect, viewport_local=viewport_local,
               viewport_pixels=viewport_pixels, dpi=dpi, expected_center_pixels=expected_pixels,
               actual_center_pixels=pixel_center,
               geometry_source='Native C++ Slate probe', native_probe=native,
               python_geometry_diagnostic=python_rect,
               error_pixels=[round(a-b, 5) for a, b in zip(pixel_center, expected_pixels)])
    case_report()['measurements'].append(row)
    return row


def check_hud():
    bar = hud()
    saved = REPORT['saved_hud']
    assert abs(bar.get_bottom_status_row_height() - saved['status_height']) < .1, '저장된 상태 행 높이 보존'
    assert abs(bar.get_bottom_bar_height() - (saved['menu_height'] + saved['status_height'])) < .1, '저장된 메뉴/상태 행 높이 보존'
    for name, expected in saved['widgets'].items():
        child = find_runtime_child(bar, name)
        assert child, ('저장 기준 자식의 런타임 연결 없음', name)
        assert authored_widget_values(child) == expected, ('HUD 디자이너 값 변경', name,
                                                          authored_widget_values(child), expected)
    assert bar.get_visibility() == unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE, 'HUD 루트가 월드 입력을 가림'


def check_build():
    bar = hud()
    current = layout_values(bar.get_open_panel_layout())
    assert current == case_report()['build_layout'], 'Build 레이아웃 표 변경'
    host = bar.get_editor_property('Panel_PanelHost')
    slot = unreal.WidgetLayoutLibrary.slot_as_canvas_slot(host)
    anchors = slot.get_anchors()
    assert_pair(pair(anchors.minimum), [0, 1], 'Build Bottom 최소 앵커')
    assert_pair(pair(anchors.maximum), [1, 1], 'Build Bottom 최대 앵커')
    offsets = slot.get_offsets()
    assert abs(offsets.top - (-bar.get_bottom_status_row_height() + current['offset'][1])) < .1, 'Build 현재 상태 행 간격'
    assert abs(offsets.bottom - current['size'][1]) < .1, 'Build 높이 보존'
    case_report()['build_bottom'] = dict(offsets=[offsets.left, offsets.top, offsets.right, offsets.bottom],
                                       layout=current, checked=True)


def capture(name):
    path = OUT / (name + '.png')
    command('Shot SHOWUI filename="' + path.as_posix() + '" -nosuffix')
    REPORT['images'].append(str(path))
    return path


def read_log():
    return LOG.read_text(encoding='utf-8', errors='replace') if LOG.exists() else ''


def annotate_visual_capture(report):
    """실제 PIE 크기와 다른 창을 찍은 PNG를 geometry 검증 근거로 쓰지 않는다."""
    checks = []
    for filename in report.get('images', []):
        name = Path(filename).stem
        case = next((item for item in report.get('cases', [])
                     if name.startswith(item['name'] + '-')), None)
        actual = case.get('actual_viewport') if case else None
        expected = [actual, [actual[0] + 6, actual[1] + 38]] if actual else []
        size = report.get('image_sizes', {}).get(filename)
        checks.append(dict(image=filename, png_size=size, actual_viewport=actual,
                           accepted_sizes=expected, matches_pie_size=size in expected))
    report['visual_capture_checks'] = checks
    report['visual_capture_valid'] = bool(checks) and all(item['matches_pie_size'] for item in checks)
    report['verification_scope'] = (
        '통과 여부는 실제 PIE의 Native Slate geometry·레이아웃 값·디스크 hash 검사 기준이다. '
        'PNG는 실제 viewport 또는 창 장식(+6,+38) 크기와 대조하며, 픽셀 내용이나 가독성 검증을 대신하지 않는다.')
    if not report['visual_capture_valid']:
        report['images_are_diagnostic_only'] = True
        note = ('생성 PNG 크기가 실제 PIE viewport/창 장식 크기와 다르므로 잘못된 창의 캡처다. '
                '이 PNG들은 P1 시각 검증 근거에서 제외한다. Native PIE geometry 측정 결과는 별도로 유효하다.')
        limitations = report.setdefault('limitations', [])
        if note not in limitations:
            limitations.append(note)


def finish():
    global HANDLE
    try:
        after = hashes()
        REPORT['protected_files_count'] = len(BEFORE)
        REPORT['protected_files_unchanged'] = BEFORE == after
        REPORT['changed_protected_files'] = [key for key in sorted(set(BEFORE) | set(after))
                                            if BEFORE.get(key) != after.get(key)]
        if not REPORT['protected_files_unchanged']:
            REPORT.setdefault('errors', []).append('에셋/프로젝트 설정의 디스크 hash가 달라짐')
        REPORT['pie_ended'] = not LEVEL.is_in_play_in_editor()
        if not REPORT['pie_ended']:
            REPORT.setdefault('errors', []).append('PIE 종료 대기 시간 초과')
        REPORT['actual_sizes'] = [row.get('actual_viewport') for row in REPORT['cases']]
        REPORT['requested_sizes_verified'] = len(REPORT['cases']) == len(CASES) and all(
            row.get('actual_viewport') == row['requested_viewport'] for row in REPORT['cases'])
        if not REPORT['requested_sizes_verified']:
            REPORT.setdefault('limitations', []).append('요청 크기와 실제 PIE viewport 크기가 다름. 측정된 크기만 검증됨.')
        for filename in REPORT['images']:
            path = Path(filename)
            if not path.exists():
                REPORT.setdefault('errors', []).append('캡처 파일 없음: ' + filename)
                continue
            with path.open('rb') as stream:
                header = stream.read(24)
            assert header[:8] == b'\x89PNG\r\n\x1a\n', filename
            REPORT.setdefault('image_sizes', {})[filename] = list(struct.unpack('>II', header[16:24]))
        annotate_visual_capture(REPORT)
        lines = read_log().splitlines()
        messages = [line for line in lines[STATE.get('log_start_line', 0):]
                    if ': Warning:' in line or ': Error:' in line]
        # 기존 경고는 결과와 별도 기록한다. 자동 검사 성공을 전체 로그 무경고로 오인하지 않는다.
        known = ('CreateInputActionWidget', 'no action provided', 'Cannot create action binding',
                 'deprecated', 'GetGameWorld', 'GetEditorWorld')
        REPORT['known_log_messages'] = list(dict.fromkeys(line for line in messages if any(k in line for k in known)))
        REPORT['other_log_messages'] = list(dict.fromkeys(line for line in messages if not any(k in line for k in known)))
        REPORT['status'] = 'failed' if REPORT.get('errors') else 'passed'
    except Exception:
        REPORT.setdefault('errors', []).append(traceback.format_exc())
        REPORT['status'] = 'failed'
    finally:
        STATE['stage'] = 'finished'
        write_report()
        if HANDLE is not None:
            unreal.unregister_slate_post_tick_callback(HANDLE)
            HANDLE = None
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        if OPTIONS.get('quit_editor', True):
            unreal.SystemLibrary.quit_editor()


def tick(delta):
    global BEFORE
    if STATE['busy'] or time.monotonic() < STATE['next']:
        return
    STATE['busy'] = True
    try:
        STATE['next'] = time.monotonic() + .5
        stage = STATE['stage']
        if stage not in ('stop_error', 'finish_error', 'finished'):
            assert time.monotonic() - STATE['start'] < OPTIONS.get('timeout_seconds', 480), 'P1 PIE 검증 시간 초과'
        if stage == 'setup':
            assert not LEVEL.is_in_play_in_editor(), '기존 PIE가 실행 중임'
            BEFORE = hashes()
            REPORT['saved_hud'] = read_saved_hud()
            STATE['log_start_line'] = len(read_log().splitlines())
            STATE['layout_start_line'] = STATE['log_start_line']
            if AUTOMATION_TESTS:
                command('Automation RunTests ' + '+'.join(AUTOMATION_TESTS))
            advance('layout_done', 2)
        elif stage == 'layout_done':
            lines = read_log().splitlines()[STATE['layout_start_line']:]
            results = {name: [line for line in lines if 'Test Completed. Result=' in line and name in line]
                       for name in AUTOMATION_TESTS}
            if not all(results.values()):
                return
            REPORT['automation_tests'] = {name: rows[-1] for name, rows in results.items()}
            live_name = 'FestaFiesta.UI.LiveHUD.BrightBottom.RenderedLayout'
            if live_name in REPORT['automation_tests']:
                REPORT['live_layout_test'] = REPORT['automation_tests'][live_name]
            for name, row in REPORT['automation_tests'].items():
                if 'Result={Success}' not in row:
                    REPORT.setdefault('errors', []).append('자동검사 실패: ' + name + ' — ' + row)
            atlas = unreal.load_asset('/Game/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas')
            entry = next(item for item in atlas.get_editor_property('entries')
                         if item.get_editor_property('unlocked') and
                         '다대포' in str(item.get_editor_property('festival_name')))
            STATE['festival'] = str(entry.get_editor_property('festival_id'))
            advance('load_frontend')
        elif stage == 'load_frontend':
            assert LEVEL.load_level('/Game/FestaFiesta/Maps/Frontend')
            # RenderOffscreen의 NullApplication에서 다음 PIE 창이 이전 작업영역에 잘리지 않게 한다.
            command('r.SetRes 4096x2160w')
            advance('begin_pie', 2)
        elif stage == 'begin_pie':
            name, width, height = CASES[STATE['case_index']]
            # 기존 NullApplication 측정의 창 장식 보정. 최종 판정은 아래 actual_viewport 실측이다.
            correction = OPTIONS.get('window_correction', [2, 12])
            args = [width-correction[0], height-correction[1]]
            REPORT['cases'].append(dict(name=name, requested_viewport=[width, height], begin_pie_args=args,
                                       measurements=[]))
            command(f'FF.FrontendBeginPIE {args[0]} {args[1]}')
            advance('start_festival', 5)
        elif stage == 'start_festival':
            if not world():
                return
            flow = subsystem(unreal.FF_UIFlowSubsystem)
            if not flow:
                return
            assert flow.start_festival(unreal.Name(STATE['festival'])), '축제 진입 실패'
            advance('wait_prep', 7)
        elif stage == 'wait_prep':
            bar = hud()
            if not bar:
                return
            check_hud()
            case_report()['actual_viewport'] = list(unreal.GameplayStatics.get_player_controller(world(), 0).get_viewport_size())
            case_report()['dpi'] = unreal.WidgetLayoutLibrary.get_viewport_scale(bar)
            case_report()['initial_layouts'] = layout_table(bar)
            bar.close_current_panel()
            unreal.AutomationLibrary.finish_loading_before_screenshot()
            command('DisableAllScreenMessages')
            STATE['tab_index'] = 0
            advance('open_panel')
        elif stage == 'open_panel':
            tab_name = TABS[STATE['tab_index']]
            bar = hud()
            bar.select_tab(getattr(unreal.FF_PrepHudTab, tab_name))
            case_report().setdefault('baseline_layouts', {})[tab_name] = layout_values(bar.get_open_panel_layout())
            assert_pair(case_report()['baseline_layouts'][tab_name]['offset'], [0, 0], '저장된 기본 Offset은 정중앙')
            advance('warm_panel', .5)
        elif stage == 'warm_panel':
            # 화면이 숨겨진 검증 프로세스에서도 한 번 paint를 요청한 뒤 cache를 측정한다.
            name = case_report()['name'] + '-' + TABS[STATE['tab_index']].lower()
            capture(name)
            advance('warm_panel_again', .5)
        elif stage == 'warm_panel_again':
            # 첫 paint에서 얻은 geometry로 NativeTick이 위치를 반영한 뒤 다시 그린다.
            name = case_report()['name'] + '-' + TABS[STATE['tab_index']].lower()
            capture(name)
            advance('measure_panel', 1)
        elif stage == 'measure_panel':
            tab_name = TABS[STATE['tab_index']]
            inspect_center(tab_name, case_report()['baseline_layouts'][tab_name])
            check_hud()
            STATE['tab_index'] += 1
            if STATE['tab_index'] < len(TABS):
                advance('open_panel')
            else:
                hud().select_tab(unreal.FF_PrepHudTab.STAFF)
                advance('edit_staff', .5)
        elif stage == 'edit_staff':
            bar = hud()
            layout = bar.get_open_panel_layout()
            layout.set_editor_property('offset', unreal.Vector2D(80, -40))
            layout.set_editor_property('size', unreal.Vector2D(880, 540))
            bar.set_open_panel_layout(layout)
            STATE['edited_staff'] = layout_values(layout)
            advance('measure_edit', 1)
        elif stage == 'measure_edit':
            inspect_center('STAFF 편집 Offset/Size', STATE['edited_staff'])
            assert hud().is_panel_drag_allowed(), '기존 패널 드래그 허용 보존'
            hud().nudge_panel_position(unreal.Vector2D(-25, 35))
            STATE['edited_staff']['offset'] = [55, -5]
            advance('measure_nudge', 1)
        elif stage == 'measure_nudge':
            inspect_center('STAFF 드래그 경로 이동량', STATE['edited_staff'])
            hud().close_current_panel()
            assert not hud().get_selection().get_editor_property('has_selection'), '닫기 후 선택 해제'
            advance('reopen_staff')
        elif stage == 'reopen_staff':
            hud().select_tab(unreal.FF_PrepHudTab.STAFF)
            advance('measure_reopen', 1)
        elif stage == 'measure_reopen':
            inspect_center('STAFF 닫기 후 재오픈', STATE['edited_staff'])
            current_table = layout_table(hud())
            for key, value in case_report()['initial_layouts'].items():
                if 'STAFF' not in key.upper():
                    assert current_table[key] == value, ('다른 패널 편집값은 그대로', key)
            hud().select_tab(unreal.FF_PrepHudTab.PROGRAM)
            advance('measure_isolation', 1)
        elif stage == 'measure_isolation':
            inspect_center('PROGRAM 다른 탭 편집 영향 없음', case_report()['baseline_layouts']['PROGRAM'])
            hud().select_tab(unreal.FF_PrepHudTab.BUILD)
            case_report()['build_layout'] = layout_values(hud().get_open_panel_layout())
            advance('measure_build', 1)
        elif stage == 'measure_build':
            check_build()
            check_hud()
            case_report()['passed'] = True
            hud().close_current_panel()
            LEVEL.editor_request_end_play()
            advance('between_cases', 3)
        elif stage == 'between_cases':
            if LEVEL.is_in_play_in_editor():
                return
            gc.collect()
            STATE['case_index'] += 1
            if STATE['case_index'] < len(CASES):
                advance('load_frontend')
            else:
                REPORT['checks'] = ['실제 Setup 4패널 중앙/기본 크기', 'Staff Offset/Size와 DPI 한 번 적용',
                                    'Nudge 이동량 및 닫기·재오픈 위치 보존', '탭별 설정 독립',
                                    'Build Bottom 도킹 및 저장된 HUD 메뉴/상태 높이 보존']
                REPORT['checks'] += [name for name, result in REPORT.get('automation_tests', {}).items()
                                     if 'Result={Success}' in result]
                finish()
        elif stage == 'stop_error':
            if LEVEL.is_in_play_in_editor():
                LEVEL.editor_request_end_play()
            STATE['cleanup_deadline'] = time.monotonic() + 20
            advance('finish_error', 2)
        elif stage == 'finish_error':
            if LEVEL.is_in_play_in_editor() and time.monotonic() < STATE['cleanup_deadline']:
                return
            finish()
    except Exception:
        error = traceback.format_exc()
        REPORT.setdefault('errors', []).append(error)
        unreal.log_error('PREP_PANEL_CENTER_VERIFY: ' + error)
        if STATE['stage'] in ('stop_error', 'finish_error'):
            finish()
        else:
            advance('stop_error')
    finally:
        STATE['busy'] = False


write_report()
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
HANDLE = unreal.register_slate_post_tick_callback(tick)
