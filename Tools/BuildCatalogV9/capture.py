"""Capture the saved catalog in the production Test_Main PIE flow, without installing anything.

Run a disposable editor with -RenderOffscreen, -SaveToUserDir and an isolated -UserDir.
No asset or map save is requested. Gameplay actions only open the catalog and a preview.
"""
from pathlib import Path
import gc
import hashlib
import json
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = ROOT / 'Saved/BuildCatalogV9' / time.strftime('runtime-%Y%m%d-%H%M%S')
OUT.mkdir(parents=True, exist_ok=True)
LEVEL = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
STATE = {'stage': 'setup', 'next': time.monotonic() + 2, 'started': time.monotonic()}
REPORT = {'status': 'RUNNING', 'physical_mouse_input_tested': False,
          'facilities_committed': False, 'asset_save_requested': False, 'captures': {}}
HANDLE = None

def hashes():
    paths = list((ROOT / 'Content/FestaFiesta/Building/FF_WBP_Building').rglob('*.uasset'))
    paths += list((ROOT / 'Saved/SaveGames').glob('*')) if (ROOT / 'Saved/SaveGames').exists() else []
    return {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in paths if p.is_file()}

def write():
    REPORT['stage'] = STATE['stage']
    (OUT / 'report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    (ROOT / 'Saved/BuildCatalogV9/runtime-latest.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')

def advance(stage, delay=2):
    STATE.update(stage=stage, next=time.monotonic() + delay)
    write()

def game_world():
    return unreal.EditorLevelLibrary.get_game_world()

def gi_subsystem(cls, world):
    gi = unreal.GameplayStatics.get_game_instance(world)
    return next((x for x in unreal.ObjectIterator(cls) if x.get_outer() == gi), None)

def live(cls):
    world = game_world()
    return next((x for x in unreal.ObjectIterator(cls) if x.get_world() == world
                 and x.get_visibility() not in (unreal.SlateVisibility.COLLAPSED, unreal.SlateVisibility.HIDDEN)), None)

def mode():
    world = game_world()
    return next((x for x in unreal.ObjectIterator(unreal.FF_BuildModeSubsystem) if x.get_world() == world), None)

def child(owner, name):
    found = [x for x in unreal.ObjectIterator(unreal.Widget) if x.get_name() == name and x.get_outer()
             and x.get_outer().get_class().get_name() == 'WidgetTree' and x.get_outer().get_outer() == owner]
    assert len(found) == 1, name
    return found[0]

def bounds(widget):
    geometry = widget.get_cached_geometry()
    size = unreal.SlateLibrary.get_local_size(geometry)
    if size.x <= 0 or size.y <= 0:
        return None  # Offscreen windows may not publish Python geometry; native Slate tests cover it.
    start, _ = unreal.SlateLibrary.local_to_viewport(game_world(), geometry, unreal.Vector2D(0, 0))
    end, _ = unreal.SlateLibrary.local_to_viewport(game_world(), geometry, size)
    return [start.x, start.y, end.x, end.y]

def shot(name):
    path = OUT / (name + '.png')
    assert not path.exists(), 'Use a fresh capture output directory: ' + str(path)
    unreal.SystemLibrary.execute_console_command(game_world(), 'Shot SHOWUI filename="' + path.as_posix() + '" -nosuffix')
    REPORT['captures'][name] = {'path': str(path), 'viewport': list(unreal.GameplayStatics.get_player_controller(game_world(), 0).get_viewport_size())}

def tick(delta):
    if STATE.get('busy') or time.monotonic() < STATE['next']:
        return
    STATE['busy'] = True
    try:
        stage = STATE['stage']
        if stage not in ('end', 'finish'):
            assert time.monotonic() - STATE['started'] < 240, 'Runtime capture timeout'
        if stage == 'setup':
            assert not LEVEL.is_in_play_in_editor()
            REPORT['hashes_before'] = hashes()
            atlas = unreal.load_asset('/Game/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas')
            festival = next(x for x in atlas.get_editor_property('entries')
                            if x.get_editor_property('unlocked') and '/Game/FestaFiesta/Maps/Test_Main.' in x.export_text())
            STATE['festival_id'] = str(festival.get_editor_property('festival_id'))
            assert LEVEL.load_level('/Game/FestaFiesta/Maps/Test_Main')
            unreal.SystemLibrary.execute_console_command(LEVEL.get_world(), 'r.SetRes 4096x2160w')
            advance('begin')
        elif stage == 'begin':
            unreal.SystemLibrary.execute_console_command(LEVEL.get_world(), 'FF.FrontendBeginPIE 1918 1068')
            advance('festival', 4)
        elif stage == 'festival':
            world = game_world()
            if not world:
                return
            flow = gi_subsystem(unreal.FF_UIFlowSubsystem, world)
            if not flow:
                return
            assert flow.start_festival(unreal.Name(STATE['festival_id']))
            advance('prep', 5)
        elif stage == 'prep':
            world = game_world()
            if not world or unreal.GameplayStatics.get_current_level_name(world, True) != 'Test_Main':
                return
            bar = live(unreal.FF_PrepHudBar)
            if not bar:
                return
            tutorial = next((x for x in unreal.ObjectIterator(unreal.FF_TutorialSubsystem)
                             if x.get_world() == world), None)
            if tutorial:
                tutorial.stop_script()
            REPORT['dialogue_stopped_for_capture'] = bool(tutorial)
            unreal.AutomationLibrary.finish_loading_before_screenshot()
            unreal.SystemLibrary.execute_console_command(world, 'DisableAllScreenMessages')
            bar.select_tab(unreal.FF_PrepHudTab.BUILD)
            advance('catalog', 4)
        elif stage == 'catalog':
            panel = live(unreal.FF_PrepPanelBuild)
            if not panel or not panel.get_catalog_page_items():
                return
            catalog = gi_subsystem(unreal.FF_BuildCatalogSubsystem, game_world())
            if catalog.get_thumbnail_load_state() in (unreal.FF_BuildCatalogThumbnailLoadState.LOADING,
                                                     unreal.FF_BuildCatalogThumbnailLoadState.WAITING_FOR_CATALOG):
                return
            shot('01-catalog')
            items = panel.get_catalog_page_items()
            choice = next((x for x in items if x.is_affordable() and '서핑' in str(x.get_display_name())), None)
            choice = choice or next(x for x in items if x.is_affordable() and not x.is_required_facility())
            STATE['candidate'] = str(choice.get_facility_definition_id())
            REPORT['candidate_name'] = str(choice.get_display_name())
            REPORT['candidate_cost'] = choice.get_install_cost()
            REPORT['entry_count'] = len(catalog.get_entries())
            REPORT['catalog_bounds'] = bounds(child(panel, 'Size_ClarityCatalogDock'))
            REPORT['list_bounds'] = bounds(child(panel, 'TileView_Facilities'))
            REPORT['search_bounds'] = bounds(child(panel, 'Size_PearlSearch'))
            # SHOWUI is rendered at the end of this frame. Keep the unselected
            # catalog intact until its queued screenshot has been processed.
            advance('begin_placement', 2)
        elif stage == 'begin_placement':
            panel = live(unreal.FF_PrepPanelBuild)
            assert panel
            assert panel.activate_catalog_facility(unreal.Name(STATE['candidate']))
            advance('placement', 4)
        elif stage == 'placement':
            assert mode().get_build_mode_state() == unreal.FF_BuildModeState.PLACING
            controller = unreal.GameplayStatics.get_player_controller(game_world(), 0)
            width, height = controller.get_viewport_size()
            controller.set_mouse_location(round(width * .54), round(height * .43))
            advance('selected', 3)
        elif stage == 'selected':
            tools = live(unreal.FF_BuildToolsWidget)
            assert tools
            shot('02-selected-facility')
            REPORT['inspector_bounds'] = bounds(child(tools, 'Size_PlacementInfo'))
            REPORT['selected_name'] = str(child(tools, 'Text_FacilityName').get_text())
            REPORT['selected_cost'] = str(child(tools, 'Text_Cost').get_text())
            advance('wait_images', 3)
        elif stage == 'wait_images':
            if not all(Path(x['path']).is_file() and Path(x['path']).stat().st_size > 1000
                       for x in REPORT['captures'].values()):
                return
            mode().return_to_catalog()
            advance('end')
        elif stage == 'end':
            # Let wrappers from the previous callback go before PIE teardown.
            gc.collect()
            LEVEL.editor_request_end_play()
            advance('finish', 3)
        elif stage == 'finish':
            if LEVEL.is_in_play_in_editor():
                return
            REPORT['assets_and_player_saves_unchanged'] = REPORT.get('hashes_before') == hashes()
            REPORT['status'] = 'PASS' if 'error' not in REPORT and REPORT['assets_and_player_saves_unchanged'] else 'FAIL'
            REPORT['elapsed_seconds'] = round(time.monotonic() - STATE['started'], 2)
            write()
            unreal.log('FF_CATALOG_V9_CAPTURE_' + REPORT['status'])
            unreal.unregister_slate_post_tick_callback(HANDLE)
            unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    except Exception:
        REPORT['error'] = traceback.format_exc()
        unreal.log_error(REPORT['error'])
        advance('end')
    finally:
        STATE['busy'] = False

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
HANDLE = unreal.register_slate_post_tick_callback(tick)
