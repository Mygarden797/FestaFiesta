"""Capture the production V7 catalog over the real Test_Main PIE world.

Run in an otherwise idle, disposable UE editor with rendering enabled. No actors,
widgets, facility records or assets are authored by this script. It follows the
real festival flow, opens the real PrepHUD panel, and changes only its selection.
Optional Saved/FacilityAurora_20260920/runtime-capture-request.json accepts
output (a child-directory name), width, height and timeout_seconds.
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
BASE = ROOT / 'Saved/FacilityAurora_20260920'
REQUEST = BASE / 'runtime-capture-request.json'
CONFIG = json.loads(REQUEST.read_text(encoding='utf-8-sig')) if REQUEST.exists() else {}
OUTPUT_NAME = CONFIG.get('output', time.strftime('runtime-%Y%m%d-%H%M%S'))
assert Path(OUTPUT_NAME).name == OUTPUT_NAME, 'output must be one directory name'
OUT = BASE / OUTPUT_NAME
assert not (OUT / 'report.json').exists(), 'Use a new output name; captures are not overwritten'
OUT.mkdir(parents=True, exist_ok=True)
WIDTH, HEIGHT = int(CONFIG.get('width', 1600)), int(CONFIG.get('height', 900))
assert 960 <= WIDTH <= 3840 and 540 <= HEIGHT <= 2160
TARGET_MAP = '/Game/FestaFiesta/Maps/Test_Main'
ATLAS = '/Game/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas'
LEVEL = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
STATE = dict(stage='setup', next=time.monotonic() + 2, started=time.monotonic())
REPORT = dict(status='running', target_map=TARGET_MAP, requested_size=[WIDTH, HEIGHT],
              production_game_flow=True, asset_save_requested=False,
              fixture_actors_created=False, facilities_installed=False,
              physical_input_tested=False, visual_review_complete=False, actions=[])
HANDLE = None


def write_report():
    REPORT['stage'] = STATE['stage']
    (OUT / 'report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')


def advance(stage, delay=1):
    STATE.update(stage=stage, next=time.monotonic() + delay)
    write_report()


def action(name):
    REPORT['actions'].append(name)
    unreal.log('FF_AURORA_RUNTIME_CAPTURE: ' + name)


def hash_files():
    # Include all maps, game definitions, UI assets and project configuration.
    # These are read-only checks: no save or editor transaction is performed.
    paths = set()
    for folder in ('Content/FestaFiesta/Maps', 'Content/FestaFiesta/Data',
                   'Content/FestaFiesta/UI', 'Content/FestaFiesta/Building/FF_WBP_Building',
                   'Content/FestaFiesta/Building/FF_Data_Building', 'Config'):
        paths.update(p for p in (ROOT / folder).rglob('*') if p.is_file())
    result = {}
    for path in sorted(paths):
        digest = hashlib.sha256()
        with path.open('rb') as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b''):
                digest.update(chunk)
        result[path.relative_to(ROOT).as_posix()] = digest.hexdigest()
    return result


def game_world():
    return unreal.EditorLevelLibrary.get_game_world()


def gi_subsystem(cls, world):
    instance = unreal.GameplayStatics.get_game_instance(world)
    return next((obj for obj in unreal.ObjectIterator(cls) if obj.get_outer() == instance), None)


def world_subsystem(cls, world):
    return next((obj for obj in unreal.ObjectIterator(cls) if obj.get_world() == world), None)


def live_widget(cls, world):
    return next((obj for obj in unreal.ObjectIterator(cls)
                 if obj.get_world() == world and obj.get_visibility() not in
                 (unreal.SlateVisibility.COLLAPSED, unreal.SlateVisibility.HIDDEN)), None)


def find_child(screen, name):
    tree = unreal.find_object(screen, 'WidgetTree')
    if tree is None:
        # Instanced nested WBP trees are commonly named WidgetTree_0. Identify
        # the actual owning tree through its direct children, never by suffix.
        trees = {}
        for candidate in unreal.ObjectIterator(unreal.Widget):
            outer = candidate.get_outer()
            if outer and outer.get_class().get_name() == 'WidgetTree' and outer.get_outer() == screen:
                trees[outer.get_path_name()] = outer
        assert len(trees) == 1, 'Production WidgetTree could not be uniquely identified: ' + screen.get_path_name()
        tree = next(iter(trees.values()))
    child = unreal.find_object(tree, name, unreal.Widget)
    assert child, 'V7 widget missing: ' + name
    return child


def choose_real_festival():
    catalog = unreal.load_asset(ATLAS)
    assert catalog, 'Production festival atlas not found'
    candidates = []
    for entry in catalog.get_editor_property('entries'):
        # export_text retains the soft object path without depending on Python's
        # soft-reference conversion or synchronously loading unrelated levels.
        if entry.get_editor_property('unlocked') and TARGET_MAP + '.' in entry.export_text():
            candidates.append(str(entry.get_editor_property('festival_id')))
    assert candidates, 'No unlocked production atlas entry points to Test_Main'
    REPORT['matching_festivals'] = candidates
    STATE['festival_id'] = candidates[0]


def screenshot(name):
    path = OUT / (name + '.png')
    assert not path.exists(), 'Refusing to overwrite ' + str(path)
    unreal.SystemLibrary.execute_console_command(game_world(),
        'Shot SHOWUI filename="' + path.as_posix() + '" -nosuffix')
    return path


def inspect_catalog():
    world = game_world()
    panel = live_widget(unreal.FF_PrepPanelBuild, world)
    assert panel, 'Production PrepHUD build panel is not visible'
    build = world_subsystem(unreal.FF_BuildModeSubsystem, world)
    assert build.is_external_catalog_presentation_active(), 'Legacy catalog is not the V7 panel'
    assert build.get_build_mode_state() == unreal.FF_BuildModeState.CATALOG_OPEN
    items = panel.get_catalog_page_items()
    assert items, 'No production facilities are displayed'
    controller = unreal.GameplayStatics.get_player_controller(world, 0)
    row = dict(world=world.get_path_name(), map=unreal.GameplayStatics.get_current_level_name(world, True),
               viewport=list(controller.get_viewport_size()), panel=panel.get_path_name(),
               build_state=str(build.get_build_mode_state()), page_count=panel.get_catalog_page_count(),
               selected_facility=str(panel.get_selected_catalog_facility_id()), widgets={},
               visible_facilities=[dict(id=str(x.get_facility_definition_id()), name=str(x.get_display_name()),
                                      cost=x.get_install_cost(), state=str(x.get_tile_state())) for x in items])
    assert row['map'] == 'Test_Main', 'Capture must show the requested real map'
    for name in ('Button_PlaceSelected', 'Text_Gold', 'Text_SelectedCost',
                 'Text_AfterPlacementGold', 'Border_AuroraSelection'):
        child = find_child(panel, name)
        geometry = child.get_paint_space_geometry()
        size = unreal.SlateLibrary.get_local_size(geometry)
        p0, _ = unreal.SlateLibrary.local_to_viewport(world, geometry, unreal.Vector2D(0, 0))
        p1, _ = unreal.SlateLibrary.local_to_viewport(world, geometry, size)
        row['widgets'][name] = dict(visible=child.get_visibility() not in
            (unreal.SlateVisibility.COLLAPSED, unreal.SlateVisibility.HIDDEN),
            enabled=child.get_is_enabled(), geometry_available=size.x > 0 and size.y > 0,
            bounds_pixels=[p0.x, p0.y, p1.x, p1.y] if size.x > 0 and size.y > 0 else None)
        if isinstance(child, unreal.TextBlock):
            row['widgets'][name]['text'] = str(child.get_text())
    REPORT['catalog'] = row
    if not all(item['geometry_available'] for item in row['widgets'].values()):
        row['geometry_limitation'] = 'RenderOffscreen has no cached child geometry; review the actual SHOWUI PNG for layout.'
    REPORT['catalog']['image'] = screenshot('01-catalog-Test_Main').relative_to(ROOT).as_posix()
    advance('wait_image', 3)


def tick(delta):
    # Loading/screenshot helpers pump Slate internally; nested callbacks must not
    # replay a toggle while the outer callback is still finishing that action.
    if STATE.get('busy') or time.monotonic() < STATE['next']:
        return
    STATE['busy'] = True
    try:
        STATE['next'] = time.monotonic() + 1
        stage = STATE['stage']
        if stage not in ('end_pie', 'final'):
            assert time.monotonic() - STATE['started'] < int(CONFIG.get('timeout_seconds', 480)), 'Capture timed out'
        if stage == 'setup':
            assert not LEVEL.is_in_play_in_editor(), 'Run with no existing PIE session'
            REPORT['hashes_before'] = hash_files()
            choose_real_festival()
            assert LEVEL.load_level(TARGET_MAP)
            action('Loaded the authored Test_Main map without saving')
            unreal.SystemLibrary.execute_console_command(LEVEL.get_world(), 'r.SetRes 4096x2160w')
            advance('begin_pie', 2)
        elif stage == 'begin_pie':
            # Existing RenderOffscreen helper uses a transient duplicate of play
            # settings. The window decoration correction is verified in report.
            unreal.SystemLibrary.execute_console_command(LEVEL.get_world(),
                f'FF.FrontendBeginPIE {WIDTH - 2} {HEIGHT - 12}')
            action('Requested real PIE with FF.FrontendBeginPIE')
            advance('start_festival', 3)
        elif stage == 'start_festival':
            world = game_world()
            if not world:
                return
            flow = gi_subsystem(unreal.FF_UIFlowSubsystem, world)
            if not flow:
                return
            assert flow.start_festival(unreal.Name(STATE['festival_id'])), 'Production StartFestival refused entry'
            action('Started authored festival ' + STATE['festival_id'] + ' through UIFlow.StartFestival')
            advance('wait_prep', 4)
        elif stage == 'wait_prep':
            world = game_world()
            if not world or unreal.GameplayStatics.get_current_level_name(world, True) != 'Test_Main':
                return
            flow = gi_subsystem(unreal.FF_UIFlowSubsystem, world)
            bar = live_widget(unreal.FF_PrepHudBar, world)
            if not flow or flow.get_mode() != unreal.FF_UIFlowMode.FESTIVAL or not bar:
                return
            unreal.AutomationLibrary.finish_loading_before_screenshot()
            unreal.SystemLibrary.execute_console_command(world, 'DisableAllScreenMessages')
            bar.select_tab(unreal.FF_PrepHudTab.BUILD)
            action('Opened production PrepHUD Build tab using its public action')
            advance('wait_catalog', 3)
        elif stage == 'wait_catalog':
            world = game_world()
            panel = live_widget(unreal.FF_PrepPanelBuild, world)
            catalog = gi_subsystem(unreal.FF_BuildCatalogSubsystem, world)
            if not panel or not catalog or not catalog.is_catalog_ready() or not panel.get_catalog_page_items():
                return
            if catalog.get_thumbnail_load_state() in (unreal.FF_BuildCatalogThumbnailLoadState.LOADING,
                    unreal.FF_BuildCatalogThumbnailLoadState.WAITING_FOR_CATALOG):
                return
            REPORT['catalog_entry_count'] = len(catalog.get_entries())
            REPORT['thumbnail_state'] = str(catalog.get_thumbnail_load_state())
            build = world_subsystem(unreal.FF_BuildModeSubsystem, world)
            assert build and build.is_external_catalog_presentation_active()
            build.toggle_build_mode()
            assert build.get_build_mode_state() == unreal.FF_BuildModeState.BUILD_MODE
            action('ToggleBuildMode closed catalog into inspection mode')
            advance('reopen_catalog')
        elif stage == 'reopen_catalog':
            build = world_subsystem(unreal.FF_BuildModeSubsystem, game_world())
            build.toggle_build_mode()
            assert build.get_build_mode_state() == unreal.FF_BuildModeState.CATALOG_OPEN
            action('ToggleBuildMode reopened the production catalog')
            advance('select_card', 2)
        elif stage == 'select_card':
            panel = live_widget(unreal.FF_PrepPanelBuild, game_world())
            items = panel.get_catalog_page_items()
            item = next((x for x in items if x.is_affordable()), items[0])
            panel.request_facility_focus(item.get_facility_definition_id())
            action('Selected existing catalog facility ' + str(item.get_facility_definition_id()) + '; no installation')
            advance('warm_one', 4)
        elif stage == 'warm_one':
            screenshot('warm-01')
            advance('warm_two', 3)
        elif stage == 'warm_two':
            screenshot('warm-02')
            advance('capture', 3)
        elif stage == 'capture':
            inspect_catalog()
        elif stage == 'wait_image':
            path = ROOT / REPORT['catalog']['image']
            if not path.exists():
                return
            header = path.read_bytes()[:24]
            assert header[:8] == b'\x89PNG\r\n\x1a\n', 'Screenshot is not PNG'
            REPORT['catalog']['png_size'] = list(struct.unpack('>II', header[16:24]))
            REPORT['catalog']['png_bytes'] = path.stat().st_size
            advance('end_pie')
        elif stage == 'end_pie':
            # No world/widget references are retained in STATE. The previous
            # callback must return before requesting teardown and garbage collect.
            gc.collect()
            LEVEL.editor_request_end_play()
            advance('final', 3)
        elif stage == 'final':
            if LEVEL.is_in_play_in_editor():
                return
            REPORT['hashes_after'] = hash_files()
            REPORT['assets_unchanged'] = REPORT.get('hashes_before') == REPORT['hashes_after']
            REPORT['elapsed_seconds'] = round(time.monotonic() - STATE['started'], 2)
            REPORT['status'] = 'captured' if 'error' not in REPORT and REPORT['assets_unchanged'] else 'failed'
            write_report()
            unreal.log('FF_AURORA_RUNTIME_CAPTURE_DONE ' + str(OUT / 'report.json'))
            unreal.unregister_slate_post_tick_callback(HANDLE)
            unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    except Exception:
        REPORT['error'] = traceback.format_exc()
        unreal.log_error('FF_AURORA_RUNTIME_CAPTURE_FAIL ' + REPORT['error'])
        advance('end_pie')
    finally:
        STATE['busy'] = False


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
HANDLE = unreal.register_slate_post_tick_callback(tick)
