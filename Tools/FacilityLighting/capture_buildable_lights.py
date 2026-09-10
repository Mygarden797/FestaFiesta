"""Capture actual buildable fixtures, update six catalog thumbnails, and QA controls.

Launch in /Engine/Maps/Entry with a real RHI and -RenderOffscreen. The transient
actors are never saved into a map. Only generated thumbnails, their six PDA
references and the existing lighting-caption migration are saved.
"""
import json
import math
import sys
import time
import traceback
from pathlib import Path
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from create_buildable_lights import ROOT, BASE, DEFINITIONS, FIXTURES, backup, save

OUT = ROOT / 'Saved/FacilityLighting_20260910/buildable_renders'
OUT.mkdir(parents=True, exist_ok=True)


def write_report(state):
    public = {key: value for key, value in state.items() if key != 'callback'}
    (OUT / 'capture_report.json').write_text(json.dumps(public, ensure_ascii=False, indent=2), encoding='utf-8')


def start():
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world and world.get_path_name().startswith('/Engine/Maps/Entry.'), 'Run only in /Engine/Maps/Entry; no user level is modified.'
    spawned, records, phases = [], {}, []
    state = {'status': 'RUNNING', 'started': time.time(), 'phase': 0, 'frame': 0,
             'captures': [], 'thumbnails': [], 'source_geometry_modified': False,
             'map_saved': False, 'visual_review': 'NOT_REVIEWED'}

    def spawn(cls, location, rotation=unreal.Rotator(0, 0, 0)):
        actor = actors.spawn_actor_from_class(cls, unreal.Vector(*location), rotation)
        assert actor, str(cls)
        spawned.append(actor)
        return actor

    # Place authored meshes at their original scale, with the lower bounds at ground level.
    cursor, maximum_height, maximum_depth = 0.0, 0.0, 0.0
    for name, minor, display in FIXTURES:
        definition_path = DEFINITIONS + '/PDA_FFZS_Lighting_' + minor + '_' + name
        definition = unreal.load_asset(definition_path)
        assert definition, definition_path
        folder = BASE + ('/Lighting/FestivalStreetlamp/Run_b31f2d864298' if name == 'FestivalStreetlamp' else '/' + name)
        mesh_path = folder + ('/SM_FestivalStreetlamp' if name == 'FestivalStreetlamp' else '/Meshes/SM_FF_' + name)
        mesh = unreal.load_asset(mesh_path)
        rig_bp = unreal.load_asset(folder + '/BP_FF_' + name + '_Lighting')
        assert mesh and rig_bp and rig_bp.generated_class(), name
        bounds = mesh.get_bounding_box()
        size = bounds.max - bounds.min
        width, height, depth = float(size.y), float(size.z), float(size.x)
        center_y = cursor + width * .5
        location = [-float(bounds.min.x + bounds.max.x) * .5,
                    center_y - float(bounds.min.y + bounds.max.y) * .5, -float(bounds.min.z)]
        model = spawn(unreal.StaticMeshActor, location)
        model.set_actor_label('Capture_' + name)
        component = model.get_component_by_class(unreal.StaticMeshComponent)
        component.set_mobility(unreal.ComponentMobility.MOVABLE)
        component.set_static_mesh(mesh)
        component.set_collision_profile_name('NoCollision')
        component.set_forced_lod_model(1)
        rig = spawn(rig_bp.generated_class(), location)
        rig.set_actor_label('CaptureRig_' + name)
        for preview in rig.get_components_by_class(unreal.StaticMeshComponent):
            preview.set_visibility(False)
            preview.set_editor_property('hidden_in_game', True)
        records[name] = dict(model=model, component=component, rig=rig, definition=definition,
                             width=width, height=height, depth=depth, center_y=center_y)
        distance = max(height * 1.72, width * 1.72, 480.0) + depth * .4
        phases.append(dict(name='Thumbnail_' + name, fixture=name, mode='ON', resolution=[512, 512],
                           camera=[distance, center_y - width * .40, height * .66],
                           target=[0, center_y, height * .48], key=1.3, fill=.7))
        cursor += width + 300.0
        maximum_height, maximum_depth = max(maximum_height, height), max(maximum_depth, depth)

    total_width = cursor - 300.0
    center_y = total_width * .5
    # The gallery uses one fixed camera, exposure and illumination for all three states.
    gallery_distance = max(total_width / (2.0 * math.tan(math.radians(27.0))) * 1.15,
                           maximum_height * 2.0)
    overview = dict(fixture=None, resolution=[1800, 800],
                    camera=[gallery_distance, center_y - total_width * .065, maximum_height * 1.6],
                    target=[0, center_y, maximum_height * .38], key=.28, fill=.16)
    phases += [dict(name='Gallery_ON', mode='ON', **overview),
               dict(name='Gallery_OFF', mode='OFF', **overview),
               dict(name='Gallery_BLUE', mode='BLUE', **overview)]
    state['fixture_order'] = [name for name, _, _ in FIXTURES]
    state['expected_capture_count'] = len(phases)

    ground = spawn(unreal.StaticMeshActor, [0, center_y, -10])
    ground.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
    ground.static_mesh_component.set_material(0, unreal.load_asset('/Engine/BasicShapes/BasicShapeMaterial'))
    ground.set_actor_scale3d(unreal.Vector(max(gallery_distance * 1.5, 5000) / 100.0,
                                          (total_width + 6000) / 100.0, .2))

    def direction(label, rotation):
        actor = spawn(unreal.DirectionalLight, [0, center_y, 1500], rotation)
        actor.set_actor_label(label)
        component = actor.get_component_by_class(unreal.DirectionalLightComponent)
        component.set_mobility(unreal.ComponentMobility.MOVABLE)
        component.set_editor_property('cast_shadows', False)
        return actor, component

    key, key_component = direction('CaptureKey', unreal.Rotator(-45, -135, 0))
    fill, fill_component = direction('CaptureFill', unreal.Rotator(-20, 140, 0))
    capture_actor = spawn(unreal.SceneCapture2D, [0, 0, 0])
    capture = capture_actor.get_component_by_class(unreal.SceneCaptureComponent2D)
    targets = {tuple(size): unreal.RenderingLibrary.create_render_target2d(
        world, *size, unreal.TextureRenderTargetFormat.RTF_RGBA8) for size in ([512, 512], [1800, 800])}
    capture.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    capture.set_editor_property('projection_type', unreal.CameraProjectionMode.PERSPECTIVE)
    capture.set_editor_property('fov_angle', 54.0)
    capture.set_editor_property('capture_every_frame', False)
    capture.set_editor_property('capture_on_movement', False)
    capture.set_editor_property('always_persist_rendering_state', True)
    capture.set_editor_property('primitive_render_mode', unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
    pp = capture.get_editor_property('post_process_settings')
    for prop, value in [('override_auto_exposure_min_brightness', True), ('override_auto_exposure_max_brightness', True),
                        ('auto_exposure_min_brightness', 1.0), ('auto_exposure_max_brightness', 1.0),
                        ('override_bloom_intensity', True), ('bloom_intensity', .22),
                        ('override_vignette_intensity', True), ('vignette_intensity', 0.0)]:
        pp.set_editor_property(prop, value)
    capture.set_editor_property('post_process_settings', pp)

    # Reuse the versioned WBP migration; this preserves custom designer captions.
    lighting_wbp = unreal.load_asset('/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityLightingControls')
    if lighting_wbp:
        backup(lighting_wbp)
    unreal.SystemLibrary.execute_console_command(world, 'FF.BuildFacilityInfoUMG')
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    write_report(state)

    def finish():
        handle = state.pop('callback', None)
        if handle is not None:
            unreal.unregister_slate_post_tick_callback(handle)
        for actor in reversed(spawned):
            actors.destroy_actor(actor)
        state['elapsed_seconds'] = time.time() - state['started']
        state['status'] = 'PASS' if ('error' not in state and len(state['captures']) == len(phases)
                                   and len(state['thumbnails']) == len(FIXTURES)) else 'FAIL'
        write_report(state)
        unreal.log('BUILDABLE_LIGHTING_CAPTURES_' + state['status'])
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()

    def apply_phase(phase):
        capture.set_editor_property('texture_target', targets[tuple(phase['resolution'])])
        location = unreal.Vector(*phase['camera'])
        rotation = unreal.MathLibrary.find_look_at_rotation(location, unreal.Vector(*phase['target']))
        capture_actor.set_actor_location_and_rotation(location, rotation, False, False)
        fill.set_actor_rotation(rotation, False)
        key_component.set_intensity(phase['key'])
        fill_component.set_intensity(phase['fill'])
        capture.clear_show_only_components()
        capture.show_only_actor_components(ground)
        lighting = unreal.FFZSFacilityLightingState()
        lighting.set_editor_property('enabled', phase['mode'] != 'OFF')
        lighting.set_editor_property('brightness_multiplier', 1.0)
        lighting.set_editor_property('override_color', phase['mode'] == 'BLUE')
        lighting.set_editor_property('color', unreal.LinearColor(.015, .18, 1.0, 1.0))
        light_readback = {}
        for name, record in records.items():
            visible = phase['fixture'] is None or phase['fixture'] == name
            if visible:
                capture.show_only_actor_components(record['model'])
            # Call production runtime code: this controls actual lamps AND per-instance bulb materials.
            record['rig'].apply_lighting_state(lighting, visible, record['component'])
            lights = record['rig'].get_components_by_class(unreal.LightComponent)
            assert lights, name + ' has no actual light components'
            intensities = [float(light.get_editor_property('intensity')) for light in lights]
            if visible:
                assert (any(value > 0 for value in intensities) if phase['mode'] != 'OFF'
                        else all(value == 0 for value in intensities)), name + ': light state mismatch'
                light_readback[name] = {'count': len(lights), 'intensities': intensities,
                                       'temperature_enabled': [bool(light.get_editor_property('use_temperature')) for light in lights]}
        phase['light_readback'] = light_readback
        capture.set_editor_property('camera_cut_this_frame', True)
        state['phase_started'] = time.time()

    def save_thumbnail(name, render_target):
        path = DEFINITIONS + '/Thumbnails/T_UI_Lighting_' + name
        texture = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
        if texture:
            backup(texture)
            unreal.RenderingLibrary.convert_render_target_to_texture2d_editor_only(world, render_target, texture)
        else:
            texture = unreal.RenderingLibrary.render_target_create_static_texture2d_editor_only(
                render_target, path, unreal.TextureCompressionSettings.TC_EDITOR_ICON,
                unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        assert texture and texture.get_path_name().split('.')[0] == path, path
        texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
        texture.set_editor_property('srgb', True)
        texture.set_editor_property('never_stream', True)
        save(texture)
        definition = records[name]['definition']
        backup(definition)
        definition.set_editor_property('catalog_thumbnail', texture)
        save(definition)
        state['thumbnails'].append({'fixture': name, 'texture': path,
                                    'definition': definition.get_path_name().split('.')[0]})

    def tick(dt):
        # Texture creation / package saving can pump Slate and re-enter this callback.
        if state.get('busy'):
            return
        state['busy'] = True
        try:
            if time.time() - state['started'] > 600:
                raise TimeoutError('Buildable fixture capture exceeded ten minutes')
            if state['phase'] >= len(phases):
                finish()
                return
            phase = phases[state['phase']]
            if state['frame'] == 0:
                apply_phase(phase)
            state['frame'] += 1
            delay = 20.0 if state['phase'] == 0 else 3.0
            if state['frame'] >= 70 and time.time() - state['phase_started'] >= delay and 'render_requested' not in state:
                capture.capture_scene()
                state['render_requested'] = time.time()
                state['render_frame'] = state['frame']
                return
            if ('render_requested' in state and state['frame'] > state['render_frame'] + 10
                    and time.time() - state['render_requested'] > .6):
                target = targets[tuple(phase['resolution'])]
                filename = phase['name'] + '.png'
                unreal.RenderingLibrary.export_render_target(world, target, str(OUT), filename)
                path = OUT / filename
                assert path.is_file() and path.stat().st_size > 1000, 'Missing or empty capture: ' + str(path)
                if phase['fixture']:
                    save_thumbnail(phase['fixture'], target)
                state['captures'].append({**phase, 'path': str(path), 'bytes': path.stat().st_size,
                                          'visual_review': 'NOT_REVIEWED'})
                state['phase'] += 1
                state['frame'] = 0
                state.pop('render_requested')
                state.pop('render_frame')
                write_report(state)
        except Exception:
            state['error'] = traceback.format_exc()
            unreal.log_error(state['error'])
            finish()
        finally:
            state['busy'] = False

    state['callback'] = unreal.register_slate_post_tick_callback(tick)


if __name__ == '__main__':
    try:
        start()
    except Exception:
        error = traceback.format_exc()
        (OUT / 'capture_error.txt').write_text(error, encoding='utf-8')
        unreal.log_error(error)
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()
