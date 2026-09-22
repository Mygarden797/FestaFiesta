# capture_roads.py
# 역할: 도로·타일 제작 원본과 납품 작업에서 실행 화면이나 모델을 촬영해 시각 검증 자료를 만든다.

"""Capture actual UE road materials/LODs and save an isolated six-tile gallery."""
import json
from pathlib import Path
import sys
import time
import traceback

import unreal

TOOLS = Path(__file__).resolve().parent
OUT = TOOLS / 'qa'
RENDERS = OUT / 'unreal_renders'
RENDERS.mkdir(parents=True, exist_ok=True)


def start():
    imported = json.loads((OUT / 'unreal_import_report.json').read_text(encoding='utf-8'))
    if imported['status'] != 'PASS':
        raise RuntimeError('Import must pass before GPU capture')
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world or not world.get_path_name().startswith('/Engine/Maps/Entry.'):
        raise RuntimeError('Launch in /Engine/Maps/Entry; do not alter user levels')
    map_path = imported['destination'] + '/Preview/L_RoadTiles_' + time.strftime('%H%M%S')
    if unreal.EditorAssetLibrary.does_asset_exist(map_path):
        raise RuntimeError('Gallery path already exists')
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not levels.new_level(map_path):
        raise RuntimeError('Could not create isolated gallery')
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    models, components, positions, phases = {}, {}, {}, []
    state = {'status': 'RUNNING', 'gallery_map': map_path, 'map_saved': False,
             'started': time.time(), 'phase': 0, 'frame': 0, 'captures': [],
             'surface_visual_review': 'NOT_RUN',
             'game_performance': {'status': 'NOT_RUN', 'frame_time_ms': None}}

    def write():
        public = {key: value for key, value in state.items() if key != 'callback'}
        (OUT / 'unreal_capture_report.json').write_text(json.dumps(public, indent=2), encoding='utf-8')

    def spawn(cls, location, label, rotation=unreal.Rotator(0, 0, 0)):
        actor = actors.spawn_actor_from_class(cls, unreal.Vector(*location), rotation)
        if not actor:
            raise RuntimeError('Spawn failed: ' + label)
        actor.set_actor_label(label)
        return actor

    for index, record in enumerate(imported['entries']):
        mesh = unreal.load_asset(record['asset'])
        name = mesh.get_name()
        # Two rows and three columns. Gallery coordinates do not imply adjacency.
        position = [(index // 3 - .5) * 310, (index % 3 - 1) * 310, 0]
        model = spawn(unreal.StaticMeshActor, position, name)
        model.set_folder_path('ROAD_TILES')
        component = model.get_component_by_class(unreal.StaticMeshComponent)
        component.set_mobility(unreal.ComponentMobility.MOVABLE)
        component.set_static_mesh(mesh)
        component.set_forced_lod_model(1)
        models[name], components[name], positions[name] = model, component, position
        target = [position[0], position[1], record['size_cm'][2] * .5]
        for lod in (0, 1, 2):
            phases.append({'name': name + '_LOD' + str(lod), 'fixture': name, 'lod': lod + 1,
                           'resolution': [900, 900], 'camera': [position[0] + 280, position[1] - 320, 390],
                           'target': target})
        phases.append({'name': name + '_Top', 'fixture': name, 'lod': 1,
                       'resolution': [900, 900], 'camera': [position[0] + .1, position[1], 550], 'target': target})

    gallery = {'name': '00_RoadGallery', 'fixture': None, 'lod': 1, 'resolution': [1800, 1200],
               'camera': [950, -800, 1400], 'target': [0, 0, 3]}
    phases.insert(0, gallery)
    state['expected_captures'] = len(phases)
    ground = spawn(unreal.StaticMeshActor, [0, 0, -12], 'PreviewGround')
    ground.set_folder_path('PREVIEW_ONLY')
    ground.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
    ground.static_mesh_component.set_material(0, unreal.load_asset('/Engine/BasicShapes/BasicShapeMaterial'))
    ground.set_actor_scale3d(unreal.Vector(35, 35, .2))

    def directional(label, rotation, intensity):
        light = spawn(unreal.DirectionalLight, [0, 0, 1500], label, rotation)
        light.set_folder_path('PREVIEW_ONLY')
        component = light.get_component_by_class(unreal.DirectionalLightComponent)
        component.set_mobility(unreal.ComponentMobility.MOVABLE)
        component.set_editor_property('cast_shadows', False)
        component.set_intensity(intensity)
        return light

    directional('PreviewKey', unreal.Rotator(-45, -135, 0), 3.0)
    fill = directional('PreviewFill', unreal.Rotator(-60, 45, 0), 1.2)
    camera_actor = spawn(unreal.SceneCapture2D, [0, 0, 0], 'Temporary_GPU_QA_Camera')
    capture = camera_actor.get_component_by_class(unreal.SceneCaptureComponent2D)
    targets = {tuple(size): unreal.RenderingLibrary.create_render_target2d(
        world, *size, unreal.TextureRenderTargetFormat.RTF_RGBA8)
        for size in ([900, 900], [1800, 1200])}
    capture.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    capture.set_editor_property('fov_angle', 52.0)
    capture.set_editor_property('capture_every_frame', False)
    capture.set_editor_property('capture_on_movement', False)
    capture.set_editor_property('always_persist_rendering_state', True)
    capture.set_editor_property('primitive_render_mode', unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
    post = capture.get_editor_property('post_process_settings')
    for prop, value in [('override_auto_exposure_min_brightness', True), ('override_auto_exposure_max_brightness', True),
                        ('auto_exposure_min_brightness', 1.0), ('auto_exposure_max_brightness', 1.0),
                        ('override_bloom_intensity', True), ('bloom_intensity', 0.0),
                        ('override_vignette_intensity', True), ('vignette_intensity', 0.0)]:
        post.set_editor_property(prop, value)
    capture.set_editor_property('post_process_settings', post)
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    write()

    def finish():
        callback = state.pop('callback', None)
        if callback is not None:
            unreal.unregister_slate_post_tick_callback(callback)
        for component in components.values():
            component.set_forced_lod_model(0)
        actors.destroy_actor(camera_actor)
        current = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        if not current.get_path_name().startswith(map_path + '.'):
            raise RuntimeError('Refusing save outside gallery')
        location = unreal.Vector(*gallery['camera'])
        rotation = unreal.MathLibrary.find_look_at_rotation(location, unreal.Vector(*gallery['target']))
        unreal.EditorLevelLibrary.set_level_viewport_camera_info(location, rotation)
        state['map_saved'] = bool(levels.save_current_level())
        state['status'] = 'PASS' if ('error' not in state and state['map_saved']
                                    and len(state['captures']) == len(phases)) else 'FAIL'
        state['elapsed_seconds'] = round(time.time() - state['started'], 3)
        write()
        if state['status'] == 'FAIL':
            unreal.log_error('ROAD_CAPTURE_FAIL: see unreal_capture_report.json')
        else:
            unreal.log('ROAD_CAPTURE_PASS: ' + map_path)
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()

    def tick(delta):
        if state.get('busy'):
            return
        state['busy'] = True
        try:
            if time.time() - state['started'] > 900:
                raise TimeoutError('GPU capture exceeded 15 minutes')
            if state['phase'] >= len(phases):
                finish()
                return
            phase = phases[state['phase']]
            if state['frame'] == 0:
                capture.set_editor_property('texture_target', targets[tuple(phase['resolution'])])
                location = unreal.Vector(*phase['camera'])
                rotation = unreal.MathLibrary.find_look_at_rotation(location, unreal.Vector(*phase['target']))
                camera_actor.set_actor_location_and_rotation(location, rotation, False, False)
                fill.set_actor_rotation(rotation, False)
                capture.clear_show_only_components()
                capture.show_only_actor_components(ground)
                for name, component in components.items():
                    component.set_forced_lod_model(phase['lod'])
                    if phase['fixture'] is None or phase['fixture'] == name:
                        capture.show_only_actor_components(models[name])
                capture.set_editor_property('camera_cut_this_frame', True)
                state['phase_started'] = time.time()
            state['frame'] += 1
            delay = 20 if state['phase'] == 0 else 2.0
            if state['frame'] >= 60 and time.time() - state['phase_started'] >= delay and 'render_requested' not in state:
                capture.capture_scene()
                state['render_requested'] = time.time()
                state['render_frame'] = state['frame']
                return
            if 'render_requested' in state and state['frame'] > state['render_frame'] + 10 and time.time() - state['render_requested'] > .6:
                filename = phase['name'] + '.png'
                unreal.RenderingLibrary.export_render_target(world, targets[tuple(phase['resolution'])], str(RENDERS), filename)
                path = RENDERS / filename
                if not path.is_file() or path.stat().st_size < 1000:
                    raise RuntimeError('Missing GPU capture: ' + str(path))
                state['captures'].append({**phase, 'path': str(path), 'bytes': path.stat().st_size, 'visual_review': 'NOT_RUN'})
                state['phase'] += 1
                state['frame'] = 0
                state.pop('render_requested')
                state.pop('render_frame')
                write()
        except Exception:
            state['error'] = traceback.format_exc()
            unreal.log_error(state['error'])
            finish()
        finally:
            state['busy'] = False
    state['callback'] = unreal.register_slate_post_tick_callback(tick)


start()
