"""P11 실제 PIE 타이틀 UI의 화면비·배치·대비 확인용 캡처.

별도 일회용 에디터에서 실행한다. 에셋을 저장하지 않는다.
PIE 창 위치 설정은 엔진 종료 때 자동 저장되므로 실행 전 별도로 백업한다.
SHOWUI는 창 장식까지 포함할 수 있어 PNG 크기와 실제 뷰포트를 따로 기록한다.
"""
from pathlib import Path
import gc
import hashlib
import json
import struct
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
REQUEST = ROOT / 'Saved/BrightGlobe/title_ui_request.json'
config = json.loads(REQUEST.read_text(encoding='utf-8-sig')) if REQUEST.exists() else {}
OUT = ROOT / 'Saved/BrightGlobe' / config.get('output', 'title_ui_v001')
OUT.mkdir(parents=True, exist_ok=True)
CASES = [('small_16x9', 960, 540), ('small_4x3', 960, 720),
         ('standard_16x9', 1600, 900), ('ultrawide_21x9', 1680, 720)]
CASES = config.get('cases', CASES)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
state = dict(stage='setup', index=0, next=time.monotonic()+2, started=time.monotonic())
report = dict(status='running', cases=[], ui_included=True, asset_save_requested=False,
              physical_input_tested=False, visual_review_complete=False)
handle = None


def hashes():
    paths = [ROOT/'Content/FestaFiesta/Maps/Frontend.umap',
             ROOT/'Content/FestaFiesta/UI/Title/GlassOrbit/WBP_TitleMain_MyCasual.uasset',
             ROOT/'Content/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas.uasset']
    paths += list((ROOT/'Content/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1/P10').glob('*.uasset'))
    return {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}


def write():
    report['stage'] = state['stage']
    (OUT/'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')


def advance(stage, delay=1):
    state.update(stage=stage, next=time.monotonic()+delay)
    write()


def find_widget(screen, name):
    tree = unreal.find_object(screen, 'WidgetTree')
    if tree is None:
        trees = {}
        for candidate in unreal.ObjectIterator(unreal.Widget):
            outer = candidate.get_outer()
            if outer and outer.get_class().get_name() == 'WidgetTree' and outer.get_outer() == screen:
                trees[outer.get_path_name()] = outer
        assert len(trees) == 1, '타이틀 WidgetTree 식별 실패'
        tree = next(iter(trees.values()))
    assert tree
    widget = unreal.find_object(tree, name, unreal.Widget)
    assert widget, name
    return widget


def bounds(world, widget):
    # 비표시 창은 tick geometry가 0인 채 SHOWUI에서 paint만 실행될 수 있다.
    geometry = widget.get_paint_space_geometry()
    size = unreal.SlateLibrary.get_local_size(geometry)
    a, _ = unreal.SlateLibrary.local_to_viewport(world, geometry, unreal.Vector2D(0, 0))
    b, _ = unreal.SlateLibrary.local_to_viewport(world, geometry, size)
    return [a.x, a.y, b.x, b.y]


def inspect():
    world = state['world']
    controller = unreal.GameplayStatics.get_player_controller(world, 0)
    title = next(w for w in unreal.ObjectIterator(unreal.FF_TitleScreen)
                 if w.get_owning_player() == controller and w.is_activated())
    actual = list(controller.get_viewport_size())
    name, width, height = CASES[state['index']]
    row = dict(name=name, requested=[width, height], viewport=actual, title=title.get_path_name(),
               viewport_matches_requested=actual == [width, height], widgets={})
    for widget_name in ('Image_BrandMark', 'Image_Wordmark', 'Btn_Play', 'Btn_Options', 'Btn_Quit'):
        widget = find_widget(title, widget_name)
        rect = bounds(world, widget)
        visible = widget.get_visibility() not in (unreal.SlateVisibility.COLLAPSED, unreal.SlateVisibility.HIDDEN)
        row['widgets'][widget_name] = dict(bounds_pixels=rect, visible=visible,
            object_path=widget.get_path_name(), geometry_available=rect[2]>rect[0] and rect[3]>rect[1],
            contained=rect[2]>rect[0] and rect[3]>rect[1] and rect[0]>=-1 and rect[1]>=-1 and rect[2]<=actual[0]+1 and rect[3]<=actual[1]+1)
        if not row['widgets'][widget_name]['geometry_available']:
            row['widgets'][widget_name]['contained'] = None
        if widget_name.startswith('Btn_'):
            bound = title.get_editor_property(widget_name)
            row['widgets'][widget_name]['bound_path'] = bound.get_path_name()
            row['widgets'][widget_name]['bound_paint_bounds'] = bounds(world, bound)
            label = widget.get_editor_property('Text_Label')
            color = label.get_editor_property('color_and_opacity').get_editor_property('specified_color')
            row['widgets'][widget_name].update(text=str(label.get_text()), label_bounds_pixels=bounds(world,label),
                                               color_linear=[color.r,color.g,color.b,color.a])
    row['outer_panel_collapsed'] = all(find_widget(title,n).get_visibility()==unreal.SlateVisibility.COLLAPSED
        for n in ('Border_LeftPanel', 'Border_LeftPanelShadow'))
    buttons = [row['widgets'][n]['bounds_pixels'] for n in ('Btn_Play','Btn_Options','Btn_Quit')]
    row['buttons_do_not_overlap'] = all(a[3]<=b[1]+1 for a,b in zip(buttons,buttons[1:]))
    if not all(row['widgets'][n]['geometry_available'] for n in ('Btn_Play','Btn_Options','Btn_Quit')):
        row['buttons_do_not_overlap'] = None
        row['geometry_limitation'] = '비표시 PIE의 widget geometry 캐시가 비어 있음. 배치는 실제 SHOWUI PNG로 판정한다.'
    image = OUT/(name+'.png')
    assert not image.exists(), '기존 캡처 덮어쓰기 방지: 다른 output 이름 사용'
    row['image'] = image.relative_to(ROOT).as_posix()
    report['cases'].append(row)
    unreal.SystemLibrary.execute_console_command(world, 'Shot SHOWUI filename="'+image.as_posix()+'" -nosuffix')
    advance('wait_image', 2)


def cleanup(error=None):
    if error:
        report['error'] = error
        unreal.log_error('BRIGHT_GLOBE_FAIL '+error)
    state.pop('world', None)
    gc.collect()
    level.editor_request_end_play()
    advance('final', 3)


def tick(delta):
    try:
        if time.monotonic()<state['next']: return
        state['next'] = time.monotonic()+300
        assert time.monotonic()-state['started']<240, 'P11 캡처 제한 시간 초과'
        stage = state['stage']
        if stage == 'setup':
            assert not level.is_in_play_in_editor()
            report['hashes_before'] = hashes()
            assert level.load_level('/Game/FestaFiesta/Maps/Frontend')
            advance('prepare_window')
        elif stage == 'prepare_window':
            # RenderOffscreen의 NullApplication은 r.SetRes를 가상 작업영역으로 쓴다.
            # 이전 PIE 크기로 다음 창을 잘라내지 않도록 생성 전에 넓힌다.
            unreal.SystemLibrary.execute_console_command(level.get_world(), 'r.SetRes 4096x2160w')
            advance('begin', 2)
        elif stage == 'begin':
            assert not level.is_in_play_in_editor()
            _, width, height = CASES[state['index']]
            # NullApplication 새 창의 실제 viewport 보정(P11 실측). 아래 실측으로 검증한다.
            unreal.SystemLibrary.execute_console_command(level.get_world(), f'FF.FrontendBeginPIE {width-2} {height-12}')
            advance('wait_pie', 2)
        elif stage == 'wait_pie':
            world = unreal.EditorLevelLibrary.get_game_world()
            if not world: state['next']=time.monotonic()+1; return
            state['world'] = world
            globe = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.FF_AtlasGlobeActor)[0]
            globe.set_auto_spin_paused(True)
            if 'orientation' not in state:
                state['orientation'] = globe.get_orientation().export_text()
            else:
                orientation = globe.get_orientation()
                orientation.import_text(state['orientation'])
                globe.set_orientation(orientation)
            unreal.AutomationLibrary.finish_loading_before_screenshot()
            unreal.SystemLibrary.execute_console_command(world, 'DisableAllScreenMessages')
            state['warm_count'] = 0
            advance('warm', 6)
        elif stage == 'warm':
            name, _, _ = CASES[state['index']]
            state['warm_count'] += 1
            warm = OUT/(name+f'_warm{state["warm_count"]}.png')
            unreal.SystemLibrary.execute_console_command(state['world'], 'Shot SHOWUI filename="'+warm.as_posix()+'" -nosuffix')
            advance('warm_wait', 2)
        elif stage == 'warm_wait':
            if state['warm_count']<2: advance('warm')
            else: advance('inspect')
        elif stage == 'inspect': inspect()
        elif stage == 'wait_image':
            row = report['cases'][-1]
            image = ROOT/row['image']
            if not image.exists(): state['next']=time.monotonic()+1; return
            header = image.read_bytes()[:24]
            assert header[:8] == b'\x89PNG\r\n\x1a\n'
            row['png_size'] = list(struct.unpack('>II',header[16:24]))
            row['png_bytes'] = image.stat().st_size
            state.pop('world')
            gc.collect()
            level.editor_request_end_play()
            advance('between', 3)
        elif stage == 'between':
            if level.is_in_play_in_editor(): state['next']=time.monotonic()+1; return
            state['index'] += 1
            if state['index']<len(CASES): advance('prepare_window')
            else: advance('final')
        elif stage == 'final':
            if level.is_in_play_in_editor(): state['next']=time.monotonic()+1; return
            report['hashes_after'] = hashes()
            report['assets_unchanged'] = report.get('hashes_before') == report['hashes_after']
            report['elapsed_seconds'] = time.monotonic()-state['started']
            report['pie_ended'] = True
            report['status'] = 'failed' if 'error' in report else 'captured'
            write()
            unreal.unregister_slate_post_tick_callback(handle)
            gc.collect()
            unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    except Exception: cleanup(traceback.format_exc())


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle = unreal.register_slate_post_tick_callback(tick)
