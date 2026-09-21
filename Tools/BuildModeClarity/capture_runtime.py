"""Capture Clarity build UI through the production Test_Main PIE game flow.

Run in an otherwise idle, disposable UE editor with rendering enabled. No actors,
widgets, facility records or assets are authored by this script. The game's own
placement preview is opened and cancelled, never committed. It captures the catalog,
required/program cards, dismissible default help, direct handles, real invalid
placement, bounded nearby assistance, removal of radial tools, codex and ranking.
Optional Saved/BuildModeClarity/runtime-capture-request.json accepts
output (a child-directory name), width, height and timeout_seconds.

Run with the editor's -ExecutePythonScript argument and rendering enabled, after
the explicit Clarity migrations have completed. No editor is started here.
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
BASE = ROOT / 'Saved/BuildModeClarity'
REQUEST = BASE / 'runtime-capture-request.json'
CONFIG = json.loads(REQUEST.read_text(encoding='utf-8-sig')) if REQUEST.exists() else {}
OUTPUT_NAME = CONFIG.get('output', time.strftime('runtime-%Y%m%d-%H%M%S'))
assert Path(OUTPUT_NAME).name == OUTPUT_NAME, 'output must be one directory name'
assert OUTPUT_NAME.startswith('runtime-'), 'output must start with runtime-'
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
              physical_input_tested=False, visual_review_complete=False, actions=[], captures=[],
              cursor_position_method='PlayerController.SetMouseLocation / GetMousePosition',
              limitations=['Public game UI actions drive this capture; physical clicking and dragging are not tested.'])
HANDLE = None


def write_report():
    REPORT['stage'] = STATE['stage']
    (OUT / 'report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')


def advance(stage, delay=1):
    STATE.update(stage=stage, next=time.monotonic() + delay)
    write_report()


def action(name):
    REPORT['actions'].append(name)
    unreal.log('FF_CLARITY_RUNTIME_CAPTURE: ' + name)


def hash_files():
    # Include all maps, game definitions, UI assets and project configuration.
    # These are read-only checks: no save or editor transaction is performed.
    paths = set()
    for folder in ('Content/FestaFiesta/Maps', 'Content/FestaFiesta/Data',
                   'Content/FestaFiesta/UI', 'Config'):
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
    assert child, 'Production widget missing: ' + name
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


def widget_state(screen, names):
    world = game_world()
    result = {}
    for name in names:
        child = find_child(screen, name)
        geometry = child.get_paint_space_geometry()
        size = unreal.SlateLibrary.get_local_size(geometry)
        p0, _ = unreal.SlateLibrary.local_to_viewport(world, geometry, unreal.Vector2D(0, 0))
        p1, _ = unreal.SlateLibrary.local_to_viewport(world, geometry, size)
        result[name] = dict(visible=child.get_visibility() not in
            (unreal.SlateVisibility.COLLAPSED, unreal.SlateVisibility.HIDDEN),
            enabled=child.get_is_enabled(), geometry_available=size.x > 0 and size.y > 0,
            bounds_pixels=[p0.x, p0.y, p1.x, p1.y] if size.x > 0 and size.y > 0 else None)
        if isinstance(child, unreal.TextBlock):
            result[name]['text'] = str(child.get_text())
    return result


def record_capture(key, name, row):
    row['image'] = screenshot(name).relative_to(ROOT).as_posix()
    REPORT[key] = row
    REPORT['captures'].append(key)
    write_report()


def inspect_catalog(key, name):
    world = game_world()
    panel = live_widget(unreal.FF_PrepPanelBuild, world)
    assert panel, 'Production PrepHUD build panel is not visible'
    build = world_subsystem(unreal.FF_BuildModeSubsystem, world)
    assert build.is_external_catalog_presentation_active(), 'Expected the production PrepHUD catalog'
    assert build.get_build_mode_state() == unreal.FF_BuildModeState.CATALOG_OPEN
    items = panel.get_catalog_page_items()
    assert items, 'No production facilities are displayed'
    controller = unreal.GameplayStatics.get_player_controller(world, 0)
    row = dict(world=world.get_path_name(), map=unreal.GameplayStatics.get_current_level_name(world, True),
               viewport=list(controller.get_viewport_size()), panel=panel.get_path_name(),
               build_state=str(build.get_build_mode_state()), page_count=panel.get_catalog_page_count(),
               selected_facility=str(panel.get_selected_catalog_facility_id()), widgets={},
               visible_facilities=[dict(id=str(x.get_facility_definition_id()), name=str(x.get_display_name()),
                                      cost=x.get_install_cost(), program_capable=x.has_compatible_programs(),
                                      required=x.is_required_facility(), required_satisfied=x.is_required_facility_satisfied(),
                                      state=str(x.get_tile_state())) for x in items])
    assert row['map'] == 'Test_Main', 'Capture must show the requested real map'
    row['widgets'] = widget_state(panel, ('Border_PanelRoot', 'Size_ClarityCatalogDock',
        'TileView_Facilities', 'Text_Gold', 'Text_Status', 'Button_RevenueRanking'))
    if not all(item['geometry_available'] for item in row['widgets'].values()):
        row['geometry_limitation'] = 'RenderOffscreen has no cached child geometry; review the actual SHOWUI PNG for layout.'
    record_capture(key, name, row)


def inspect_tools(key, name):
    world = game_world()
    build = world_subsystem(unreal.FF_BuildModeSubsystem, world)
    tools = live_widget(unreal.FF_BuildToolsWidget, world)
    assert tools and build.get_build_mode_state() == unreal.FF_BuildModeState.PLACING
    controller = unreal.GameplayStatics.get_player_controller(world, 0)
    data = tools.get_tools_view_data()
    row = dict(world=world.get_path_name(), map=unreal.GameplayStatics.get_current_level_name(world, True),
        build_state=str(build.get_build_mode_state()), selected_facility=str(build.get_selected_facility_definition_id()),
        viewport=list(controller.get_viewport_size()), actual_cursor=controller.get_mouse_position(),
        tool_view_data=data.export_text(),
        widgets=widget_state(tools, ('Border_PlacementInfo', 'Border_Tools',
            'Button_Confirm', 'Text_FacilityName', 'Text_Cost', 'Text_Legend', 'Border_ContextPing',
            'Border_PlacementHelp', 'Label_Help', 'Text_PlacementAdvice', 'Text_PlacementObstruction',
            'Text_AssistanceMessage', 'Button_DirectAdjustment', 'Button_FindNearby', 'Button_NextPlacementProblem')))
    assert row['map'] == 'Test_Main'
    owned_names = {widget.get_name() for widget in unreal.ObjectIterator(unreal.Widget)
                   if widget.get_outer() and widget.get_outer().get_class().get_name() == 'WidgetTree'
                   and widget.get_outer().get_outer() == tools}
    assert not {'Button_Radial', 'Panel_Radial', 'Panel_Details'} & owned_names, 'Removed tools are still in the live widget'
    row['radial_tools_removed'] = True
    record_capture(key, name, row)


def inspect_codex():
    world = game_world()
    codex = live_widget(unreal.FF_FacilityCodexScreen, world)
    assert codex and codex.get_visible_entry_count() > 0, 'Production codex has no entries'
    row = dict(world=world.get_path_name(), map=unreal.GameplayStatics.get_current_level_name(world, True),
        visible_entry_count=codex.get_visible_entry_count(), selected_rule=str(codex.get_selected_rule_id()),
        widgets=widget_state(codex, ('Size_Dialog', 'TileView_Entries', 'Size_Detail', 'Text_Summary')))
    assert row['map'] == 'Test_Main'
    def effectively_visible(widget):
        while widget:
            if widget.get_visibility() in (unreal.SlateVisibility.COLLAPSED, unreal.SlateVisibility.HIDDEN):
                return False
            widget = widget.get_parent()
        return True
    row['catalog_visible_behind_modal'] = any(effectively_visible(panel)
        for panel in unreal.ObjectIterator(unreal.FF_PrepPanelBuild) if panel.get_world() == world)
    assert not row['catalog_visible_behind_modal'], 'Construction catalog must be hidden while codex is open'
    record_capture('codex', '06-combination-codex-Test_Main', row)


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
            advance('warm_one', 2)
        elif stage == 'select_card':
            panel = live_widget(unreal.FF_PrepPanelBuild, game_world())
            items = panel.get_catalog_page_items()
            item = next((x for x in items if x.is_affordable() and not x.is_required_facility()), items[0])
            assert item.is_affordable(), 'A production affordable candidate is needed for placement capture'
            STATE['candidate_id'] = str(item.get_facility_definition_id())
            # Program compatibility is projected for the active festival into the
            # presentation model; the raw catalog definition cache has no such quote.
            program_id, required_id = None, None
            for page_index in range(panel.get_catalog_page_count()):
                panel.set_catalog_page(page_index)
                page_items = panel.get_catalog_page_items()
                program = next((entry for entry in page_items
                    if entry.has_compatible_programs()), None)
                required = next((entry for entry in page_items if entry.is_required_facility()), None)
                if program and program_id is None: program_id = program.get_facility_definition_id()
                if required and required_id is None: required_id = required.get_facility_definition_id()
                if program_id is not None and required_id is not None: break
            assert program_id is not None, 'Production festival must expose at least one program-capable facility'
            assert required_id is not None, 'Production festival must expose a first-day required facility'
            STATE['program_id'] = str(program_id)
            panel.request_facility_focus(required_id)
            action('Focused the first-day required facility to inspect its red pearl surface and thumbnail badge')
            advance('capture_required_catalog', 3)
        elif stage == 'capture_required_catalog':
            inspect_catalog('required_catalog', '02a-required-catalog-Test_Main')
            assert any(row['required'] for row in REPORT['required_catalog']['visible_facilities'])
            advance('focus_program', 2)
        elif stage == 'focus_program':
            panel = live_widget(unreal.FF_PrepPanelBuild, game_world())
            panel.request_facility_focus(unreal.Name(STATE['program_id']))
            action('Focused an authored program facility to inspect its lavender pearl category treatment')
            advance('capture_program_catalog', 3)
        elif stage == 'warm_one':
            screenshot('warm-01')
            advance('warm_two', 3)
        elif stage == 'warm_two':
            screenshot('warm-02')
            advance('capture', 3)
        elif stage == 'capture':
            inspect_catalog('catalog', '01-catalog-Test_Main')
            assert REPORT['catalog']['visible_facilities'][0]['required'], 'All facilities must lead with the required facility'
            assert not REPORT['catalog']['visible_facilities'][0]['required_satisfied']
            REPORT['required_facility_first_on_all_catalog'] = True
            advance('select_card', 3)
        elif stage == 'capture_program_catalog':
            inspect_catalog('program_catalog', '02-program-catalog-Test_Main')
            assert any(row['program_capable'] for row in REPORT['program_catalog']['visible_facilities'])
            # SHOWUI is resolved at the end of the frame. Preserve the displayed
            # program category until its queued screenshot has actually rendered.
            advance('restore_candidate', 2)
        elif stage == 'restore_candidate':
            panel = live_widget(unreal.FF_PrepPanelBuild, game_world())
            panel.request_facility_focus(unreal.Name(STATE['candidate_id']))
            advance('begin_placement', 3)
        elif stage == 'begin_placement':
            world = game_world()
            panel = live_widget(unreal.FF_PrepPanelBuild, world)
            assert panel
            # The card's single activation starts the preview; no second placement-start button.
            assert panel.activate_catalog_facility(unreal.Name(STATE['candidate_id']))
            build = world_subsystem(unreal.FF_BuildModeSubsystem, world)
            assert build.get_build_mode_state() == unreal.FF_BuildModeState.PLACING
            controller = unreal.GameplayStatics.get_player_controller(world, 0)
            width, height = controller.get_viewport_size()
            target = [round(width * 0.54), round(height * 0.43)]
            # This engine UFUNCTION warps the game viewport cursor. It does not inject a click.
            controller.set_mouse_location(*target)
            REPORT['placement_cursor_requested'] = target
            action('Activated one catalog card directly into placement preview; no separate start or CommitPlacement call')
            advance('capture_placement', 4)
        elif stage == 'capture_placement':
            inspect_tools('placement', '03-placement-default-help-Test_Main')
            assert REPORT['placement']['widgets']['Border_PlacementHelp']['visible'], 'New placement must open its instructions'
            assert REPORT['placement']['widgets']['Label_Help']['text'] == '안내 닫기'
            if not REPORT['placement']['widgets']['Border_ContextPing']['visible']:
                REPORT['limitations'].append('Placement preview had no visible contextual ping at the requested cursor position; no valid target is fabricated.')
            advance('close_help', 3)
        elif stage == 'close_help':
            tools = live_widget(unreal.FF_BuildToolsWidget, game_world())
            tools.toggle_help()
            tools.refresh_tools()
            action('Dismissed default instructions and refreshed placement; dismissal must persist')
            advance('capture_closed_help', 2)
        elif stage == 'capture_closed_help':
            inspect_tools('closed_help', '03a-placement-help-closed-Test_Main')
            assert not REPORT['closed_help']['widgets']['Border_PlacementHelp']['visible']
            assert REPORT['closed_help']['widgets']['Label_Help']['text'] == '조작 안내'
            advance('open_help', 2)
        elif stage == 'open_help':
            live_widget(unreal.FF_BuildToolsWidget, game_world()).toggle_help()
            advance('capture_help', 2)
        elif stage == 'capture_help':
            inspect_tools('left_help', '04-left-placement-help-Test_Main')
            assert REPORT['left_help']['widgets']['Border_PlacementHelp']['visible']
            advance('begin_direct_adjustment', 3)
        elif stage == 'begin_direct_adjustment':
            build = world_subsystem(unreal.FF_BuildModeSubsystem, game_world())
            assert build.get_placement_guidance().problem_count == 0
            build.toggle_direct_placement_adjustment()
            assert build.is_direct_placement_adjustment()
            zones = world_subsystem(unreal.FFZSWorldSubsystem, game_world())
            STATE['assistance_revision'] = zones.get_world_revision()
            STATE['nudge_count'] = 0
            action('Pinned the real facility preview in direct adjustment mode; no facility committed')
            advance('capture_direct_adjustment', 2)
        elif stage == 'capture_direct_adjustment':
            inspect_tools('direct_adjustment', '04a-direct-adjustment-Test_Main')
            advance('find_placement_problem', 2)
        elif stage == 'find_placement_problem':
            build = world_subsystem(unreal.FF_BuildModeSubsystem, game_world())
            for _ in range(4):
                guidance = build.get_placement_guidance()
                if guidance.problem_count > 0:
                    REPORT['assistance_problem'] = dict(title=str(guidance.title), advice=str(guidance.advice),
                        obstruction=str(guidance.obstruction), count=guidance.problem_count,
                        can_find_nearby=guidance.can_find_nearby, nudge_count=STATE['nudge_count'])
                    assert guidance.can_find_nearby, 'A spatial problem is required to verify nearby assistance'
                    # The first real obstruction may be outside the initial camera. Use the
                    # game's own focus command to inspect it; do not fake a closer blocker.
                    world = game_world()
                    previews = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.FFZSPlacementPreviewActor)
                    assert len(previews) == 1
                    location = previews[0].get_actor_location()
                    controller = unreal.GameplayStatics.get_player_controller(world, 0)
                    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
                    assert isinstance(pawn, unreal.FF_OverviewCameraPawn)
                    pawn.focus_on_location(location)
                    REPORT['assistance_camera_focus'] = [location.x, location.y, location.z]
                    action('Focused the production camera on the actual obstruction before inspecting its overlay')
                    advance('capture_placement_problem', 4)
                    return
                assert STATE['nudge_count'] < 160, 'No real placement boundary found within the bounded trace'
                build.nudge_placement(unreal.IntPoint(1, 0))
                STATE['nudge_count'] += 1
            advance('find_placement_problem', .1)
        elif stage == 'capture_placement_problem':
            inspect_tools('placement_problem', '04b-placement-problem-Test_Main')
            advance('find_nearby', 2)
        elif stage == 'find_nearby':
            build = world_subsystem(unreal.FF_BuildModeSubsystem, game_world())
            assert build.find_nearby_placement()
            action('Requested bounded nearby position assistance after the real validator rejected a nudged preview')
            advance('wait_nearby', .2)
        elif stage == 'wait_nearby':
            build = world_subsystem(unreal.FF_BuildModeSubsystem, game_world())
            if build.is_finding_nearby_placement(): return
            assert build.get_placement_guidance().problem_count == 0, 'Nearby candidate must pass full validation'
            assert build.is_direct_placement_adjustment(), 'Suggested position must remain pinned for explicit confirmation'
            zones = world_subsystem(unreal.FFZSWorldSubsystem, game_world())
            assert zones.get_world_revision() == STATE['assistance_revision'], 'Assistance must not install or mutate facilities'
            REPORT['nearby_assistance_preserved_world_revision'] = True
            advance('capture_nearby', 2)
        elif stage == 'capture_nearby':
            inspect_tools('nearby_placement', '04c-nearby-placement-Test_Main')
            advance('resume_cursor_placement', 2)
        elif stage == 'resume_cursor_placement':
            build = world_subsystem(unreal.FF_BuildModeSubsystem, game_world())
            build.toggle_direct_placement_adjustment()
            assert not build.is_direct_placement_adjustment()
            advance('verify_removed_radial', 2)
        elif stage == 'verify_removed_radial':
            tools = live_widget(unreal.FF_BuildToolsWidget, game_world())
            assert tools
            tools.toggle_radial_menu()
            action('Confirmed the legacy radial action cannot restore the removed R menu')
            advance('capture_without_radial', 2)
        elif stage == 'capture_without_radial':
            inspect_tools('without_radial', '05-placement-without-tools-Test_Main')
            advance('open_codex', 3)
        elif stage == 'open_codex':
            world = game_world()
            build = world_subsystem(unreal.FF_BuildModeSubsystem, world)
            tools = live_widget(unreal.FF_BuildToolsWidget, world)
            if widget_state(tools, ('Border_PlacementHelp',))['Border_PlacementHelp']['visible']:
                tools.toggle_help()
            build.return_to_catalog()
            assert build.get_build_mode_state() == unreal.FF_BuildModeState.CATALOG_OPEN
            presenter = world_subsystem(unreal.FF_FacilityCodexPresenterSubsystem, world)
            assert presenter and presenter.open_codex(unreal.GameplayStatics.get_player_controller(world, 0), unreal.Name('None'))
            action('Cancelled placement and opened full production combination codex')
            advance('capture_codex', 3)
        elif stage == 'capture_codex':
            inspect_codex()
            advance('close_codex', 3)
        elif stage == 'close_codex':
            presenter = world_subsystem(unreal.FF_FacilityCodexPresenterSubsystem, game_world())
            presenter.close_codex()
            action('Closed codex and restored the prior construction catalog presentation')
            advance('capture_restored_catalog', 3)
        elif stage == 'capture_restored_catalog':
            inspect_catalog('restored_catalog', '07-catalog-restored-Test_Main')
            assert REPORT['restored_catalog']['selected_facility'] == STATE['candidate_id']
            advance('verify_next_preview_help', 2)
        elif stage == 'verify_next_preview_help':
            world = game_world()
            panel = live_widget(unreal.FF_PrepPanelBuild, world)
            assert panel.activate_catalog_facility(unreal.Name(STATE['candidate_id']))
            tools = live_widget(unreal.FF_BuildToolsWidget, world)
            assert widget_state(tools, ('Border_PlacementHelp',))['Border_PlacementHelp']['visible']
            tools.toggle_help()
            world_subsystem(unreal.FF_BuildModeSubsystem, world).return_to_catalog()
            REPORT['default_help_reopened_on_next_placement'] = True
            action('A new placement reopened its default instructions without committing a facility')
            advance('open_ranking', 2)
        elif stage == 'open_ranking':
            ranking = world_subsystem(unreal.FF_FacilityRevenueRankingSubsystem, game_world())
            ranking.open()
            assert ranking.is_open()
            action('Opened the real facility revenue ranking')
            advance('capture_ranking', 3)
        elif stage == 'capture_ranking':
            world = game_world()
            panel = live_widget(unreal.FF_FacilityRevenueRankingPanel, world)
            assert panel
            row = dict(world=world.get_path_name(), widgets=widget_state(panel,
                ('Size_ClarityFrame', 'Border_Panel', 'Text_Total', 'Text_Count', 'Button_Today', 'Button_Total')))
            record_capture('ranking', '08-revenue-ranking-Test_Main', row)
            advance('wait_images', 3)
        elif stage == 'wait_images':
            for key in REPORT['captures']:
                path = ROOT / REPORT[key]['image']
                if not path.exists():
                    return
                header = path.read_bytes()[:24]
                assert header[:8] == b'\x89PNG\r\n\x1a\n', 'Screenshot is not PNG: ' + key
                REPORT[key]['png_size'] = list(struct.unpack('>II', header[16:24]))
                REPORT[key]['png_bytes'] = path.stat().st_size
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
            unreal.log('FF_CLARITY_RUNTIME_CAPTURE_DONE ' + str(OUT / 'report.json'))
            unreal.unregister_slate_post_tick_callback(HANDLE)
            unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    except Exception:
        REPORT['error'] = traceback.format_exc()
        unreal.log_error('FF_CLARITY_RUNTIME_CAPTURE_FAIL ' + REPORT['error'])
        advance('end_pie')
    finally:
        STATE['busy'] = False


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
HANDLE = unreal.register_slate_post_tick_callback(tick)
