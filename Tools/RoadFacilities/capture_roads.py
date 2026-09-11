"""Render imported road surfaces and an existing facility above a tiled road.

Uses a transient Entry world and real GPU SceneCapture. This visual fixture is
separate from the command/placement automation tests; it never saves a level.
"""
import json
import math
import sys
import time
import traceback
from pathlib import Path
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from import_roads import ROOT, BASE, ROADS

OUT = ROOT / 'Saved/Roads_20260910/renders'
OUT.mkdir(parents=True, exist_ok=True)


def start():
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world and world.get_path_name().startswith('/Engine/Maps/Entry.'), 'Run only in the transient Entry map'
    spawned, gallery, example = [], [], []
    state = dict(status='RUNNING', started=time.time(), phase=0, frame=0, captures=[],
                 map_saved=False, visual_review='NOT_REVIEWED',
                 placement_verification='See FF_ZoneSystem.Road command automation tests')

    def report():
        (OUT / 'capture_report.json').write_text(json.dumps(
            {key: value for key, value in state.items() if key != 'callback'},
            ensure_ascii=False, indent=2), encoding='utf-8')

    def spawn(cls, location, rotation=unreal.Rotator(0, 0, 0)):
        actor = actors.spawn_actor_from_class(cls, unreal.Vector(*location), rotation)
        assert actor
        spawned.append(actor)
        return actor

    def model(mesh, location, name, target):
        actor = spawn(unreal.StaticMeshActor, location)
        actor.set_actor_label(name)
        component = actor.static_mesh_component
        component.set_mobility(unreal.ComponentMobility.MOVABLE)
        component.set_static_mesh(mesh)
        component.set_collision_profile_name('NoCollision')
        component.set_forced_lod_model(1)
        target.append(actor)
        return actor

    # A 3-by-2 arrangement fits the square modular shapes without scaling them.
    for index, (name, label) in enumerate(ROADS):
        mesh = unreal.load_asset(BASE + '/SM_Road_' + name)
        assert mesh, name
        model(mesh, [(index % 3 - 1) * 540, (index // 3 - .5) * 550, 0], label, gallery)

    # Keep the existing facility at its authoritative size and position the floor
    # at the road's 8 cm support plane. The tile platform expands to fit the spec.
    definition = unreal.load_asset('/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Booth/PDA_FFZS_Booth_FoodStall_IceCream')
    assert definition
    spec = definition.get_editor_property('spec')
    mesh = spec.get_editor_property('visual_mesh')
    assert mesh
    footprint = spec.get_editor_property('footprint_size_cells')
    yaw = spec.get_editor_property('visual_rotation').yaw
    size = mesh.get_bounding_box().max - mesh.get_bounding_box().min
    swap = abs(math.sin(math.radians(yaw))) > .5
    target_x, target_y = (footprint.y, footprint.x) if swap else (footprint.x, footprint.y)
    scale = unreal.Vector(target_x / size.x, target_y / size.y,
                          spec.get_editor_property('placement_obstacle_height_cm') / size.z)
    bounds = mesh.get_bounding_box()
    center = (bounds.min + bounds.max) * .5
    angle = math.radians(yaw)
    cx, cy = center.x * scale.x, center.y * scale.y
    facility = model(mesh, [-cx * math.cos(angle) + cy * math.sin(angle),
                            -cx * math.sin(angle) - cy * math.cos(angle),
                            8 - bounds.min.z * scale.z], 'ExistingIceCreamFacility_OnRoad', example)
    facility.set_actor_scale3d(scale)
    facility.set_actor_rotation(unreal.Rotator(pitch=0, yaw=yaw, roll=0), False)
    platform_x = max(2, math.ceil(footprint.x / 400.0) + 1)
    platform_y = max(2, math.ceil(footprint.y / 400.0) + 1)
    stone = unreal.load_asset(BASE + '/SM_Road_Stone')
    for x in range(platform_x):
        for y in range(platform_y):
            model(stone, [(x - (platform_x - 1) * .5) * 400,
                          (y - (platform_y - 1) * .5) * 400, 0], 'SupportRoad_%s_%s' % (x, y), example)
    state['facility_visual_fixture'] = dict(definition=definition.get_path_name(),
        facility_mesh=mesh.get_path_name(), support_height_cm=8,
        footprint_cm=[footprint.x, footprint.y], road_tile_count=platform_x * platform_y)

    ground = model(unreal.load_asset('/Engine/BasicShapes/Cube'), [0, 0, -12], 'PreviewGround', [])
    ground.set_actor_scale3d(unreal.Vector(45, 45, .2))
    ground.static_mesh_component.set_material(0, unreal.load_asset('/Engine/BasicShapes/BasicShapeMaterial'))
    for label, rotation, intensity in [('Key', unreal.Rotator(-50, -120, 0), 2.2),
                                        ('Fill', unreal.Rotator(-35, 35, 0), 1.25)]:
        light = spawn(unreal.DirectionalLight, [0, 0, 2000], rotation)
        light.set_actor_label('RoadCapture_' + label)
        component = light.get_component_by_class(unreal.DirectionalLightComponent)
        component.set_mobility(unreal.ComponentMobility.MOVABLE)
        component.set_editor_property('cast_shadows', label == 'Key')
        component.set_intensity(intensity)

    capture_actor = spawn(unreal.SceneCapture2D, [0, 0, 0])
    capture = capture_actor.get_component_by_class(unreal.SceneCaptureComponent2D)
    target = unreal.RenderingLibrary.create_render_target2d(world, 1800, 1200, unreal.TextureRenderTargetFormat.RTF_RGBA8)
    capture.set_editor_property('texture_target', target)
    capture.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    capture.set_editor_property('projection_type', unreal.CameraProjectionMode.PERSPECTIVE)
    capture.set_editor_property('fov_angle', 48.0)
    capture.set_editor_property('capture_every_frame', False)
    capture.set_editor_property('capture_on_movement', False)
    capture.set_editor_property('always_persist_rendering_state', True)
    capture.set_editor_property('primitive_render_mode', unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
    pp = capture.get_editor_property('post_process_settings')
    for key, value in dict(override_auto_exposure_min_brightness=True, override_auto_exposure_max_brightness=True,
                           auto_exposure_min_brightness=1.0, auto_exposure_max_brightness=1.0,
                           override_bloom_intensity=True, bloom_intensity=0.0,
                           override_vignette_intensity=True, vignette_intensity=0.0).items():
        pp.set_editor_property(key, value)
    capture.set_editor_property('post_process_settings', pp)
    distance = max(platform_x * 400, platform_y * 400, spec.get_editor_property('placement_obstacle_height_cm')) * 1.65
    phases = [dict(name='RoadGallery_LOD' + str(lod), mode='gallery', lod=lod,
                   camera=[-200, -1150, 2350], look_at=[0, 0, 0]) for lod in range(3)]
    phases.append(dict(name='FacilityOnRoads', mode='example', lod=0,
                       camera=[distance, distance * .7, distance * .72],
                       look_at=[0, 0, spec.get_editor_property('placement_obstacle_height_cm') * .3]))
    state['gallery_order'] = [label for _, label in ROADS]
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)

    def finish():
        handle = state.pop('callback', None)
        if handle is not None:
            unreal.unregister_slate_post_tick_callback(handle)
        for actor in reversed(spawned):
            actors.destroy_actor(actor)
        state['status'] = 'PASS' if 'error' not in state and len(state['captures']) == len(phases) else 'FAIL'
        state['elapsed_seconds'] = time.time() - state['started']
        report()
        unreal.log('ROAD_CAPTURE_' + state['status'])
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()

    def tick(dt):
        if state.get('busy'):
            return
        state['busy'] = True
        try:
            if time.time() - state['started'] > 600:
                raise TimeoutError('Road capture exceeded ten minutes')
            if state['phase'] >= len(phases):
                finish()
                return
            phase = phases[state['phase']]
            if state['frame'] == 0:
                location = unreal.Vector(*phase['camera'])
                rotation = unreal.MathLibrary.find_look_at_rotation(location, unreal.Vector(*phase['look_at']))
                capture_actor.set_actor_location_and_rotation(location, rotation, False, False)
                capture.clear_show_only_components()
                capture.show_only_actor_components(ground)
                for actor in gallery + example:
                    visible = actor in (gallery if phase['mode'] == 'gallery' else example)
                    actor.set_actor_hidden_in_game(not visible)
                    if visible:
                        actor.static_mesh_component.set_forced_lod_model(phase['lod'] + 1)
                        capture.show_only_actor_components(actor)
                capture.set_editor_property('camera_cut_this_frame', True)
                state['phase_started'] = time.time()
            state['frame'] += 1
            if state['frame'] >= 70 and time.time() - state['phase_started'] >= (20 if state['phase'] == 0 else 3) and 'requested' not in state:
                capture.capture_scene()
                state['requested'] = time.time()
                state['render_frame'] = state['frame']
                return
            if ('requested' in state and state['frame'] > state['render_frame'] + 10 and time.time() - state['requested'] > .6):
                filename = phase['name'] + '.png'
                unreal.RenderingLibrary.export_render_target(world, target, str(OUT), filename)
                path = OUT / filename
                assert path.is_file() and path.stat().st_size > 1000, str(path)
                state['captures'].append(dict(**phase, path=str(path), visual_review='NOT_REVIEWED'))
                state['phase'] += 1
                state['frame'] = 0
                state.pop('requested')
                state.pop('render_frame')
                report()
        except Exception:
            state['error'] = traceback.format_exc()
            unreal.log_error(state['error'])
            finish()
        finally:
            state['busy'] = False

    state['callback'] = unreal.register_slate_post_tick_callback(tick)
    report()


if __name__ == '__main__':
    try:
        start()
    except Exception:
        (OUT / 'capture_error.txt').write_text(traceback.format_exc(), encoding='utf-8')
        unreal.log_error(traceback.format_exc())
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()
