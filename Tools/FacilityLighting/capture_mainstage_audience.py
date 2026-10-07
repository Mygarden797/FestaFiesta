"""Read-only GPU comparison of MainStage lighting at authored audience seats.

Run in /Engine/Maps/Entry with -RenderOffscreen and a real RHI. The screenshot
uses 63 sampled audience positions, not a running crowd simulation. No assets or
maps are saved. BEFORE disables only the two added AudienceWash spotlights after
the production ApplyLightingState call; AFTER restores the production state.
"""
import hashlib
import json
import math
import time
import traceback
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'Saved/MainStageAudienceLighting'
OUT.mkdir(parents=True, exist_ok=True)
PDA = '/Game/FestaFiesta/Building/FF_Data_Building/Performance/PDA_FFZS_Performance_MainStage_SunsetMainStage'
RIG = '/Game/Asset/FacilitiesAsset_HKH/Performance/MainStage_Grand_20260909/BP_MainStage_Grand_Lighting'
CROWD = '/Game/Level/Demo_Matereal/Mesh/anim/Biped_Import_20260929/CH_01_Male/SK_CH_01_Male'
IDLE = '/Game/Level/Demo_Matereal/Mesh/anim/Biped_Import_20260929/CH_01_Male/Animations/A_CH_01_Male_idle'
WASH_NAMES = {'AudienceWash_Left_Spot', 'AudienceWash_Right_Spot'}


def xyz(vector):
    return [float(vector.x), float(vector.y), float(vector.z)]


def source_hash(asset):
    package = asset.get_path_name().split('.')[0]
    path = ROOT / 'Content' / (package.removeprefix('/Game/') + '.uasset')
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fitted_mesh_transform(spec, mesh):
    """Mirror FFZSVisualGeometry for the current cardinal MatchPlacementBounds PDA."""
    assert spec.visual_sizing_mode == unreal.FFZSVisualSizingMode.MATCH_PLACEMENT_BOUNDS
    rotation = spec.visual_rotation
    assert abs(rotation.pitch) < 0.001 and abs(rotation.roll) < 0.001
    turns = round(float(rotation.yaw) / 90.0)
    assert abs(float(rotation.yaw) - turns * 90.0) < 0.001
    bounds = mesh.get_bounding_box()
    size = bounds.max - bounds.min
    target = [float(spec.footprint_size_cells.x), float(spec.footprint_size_cells.y),
              float(spec.placement_obstacle_height_cm)]
    scale = unreal.Vector((target[1] if turns % 2 else target[0]) / size.x,
                          (target[0] if turns % 2 else target[1]) / size.y,
                          target[2] / size.z)
    # The production transform centres XY after scaling/rotation and grounds Z.
    radians = math.radians(float(rotation.yaw))
    centre = (bounds.min + bounds.max) * 0.5
    scaled_x, scaled_y = centre.x * scale.x, centre.y * scale.y
    location = unreal.Vector(-scaled_x * math.cos(radians) + scaled_y * math.sin(radians),
                             -scaled_x * math.sin(radians) - scaled_y * math.cos(radians),
                             -bounds.min.z * scale.z) + spec.visual_offset_cm
    return location, rotation, scale


def start():
    editor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world and world.get_path_name().startswith('/Engine/Maps/Entry.'), 'Only the transient Entry world is allowed.'
    spawned, visible_actors, loaded_assets = [], [], []
    state = {'status': 'RUNNING', 'started': time.time(), 'phase': 0, 'frame': 0,
             'captures': [], 'assets_saved': False, 'map_saved': False,
             'visual_review': 'NOT_REVIEWED', 'comparison': 'Only AudienceWash lights change.'}

    def report():
        public = {key: value for key, value in state.items() if key != 'callback'}
        (OUT / 'capture_report.json').write_text(json.dumps(public, ensure_ascii=False, indent=2), encoding='utf-8')

    def load(path):
        asset = unreal.load_asset(path)
        assert asset, 'Missing source asset: ' + path
        loaded_assets.append(asset)
        return asset

    def spawn(cls, location, rotation=unreal.Rotator()):
        actor = editor.spawn_actor_from_class(cls, location, rotation)
        assert actor, str(cls)
        spawned.append(actor)
        return actor

    pda, rig_bp, crowd_mesh, idle = load(PDA), load(RIG), load(CROWD), load(IDLE)
    spec = pda.get_editor_property('spec')
    mesh = spec.get_editor_property('visual_mesh')
    assert isinstance(mesh, unreal.StaticMesh), 'The production PDA mesh must resolve.'
    loaded_assets.append(mesh)
    location, rotation, scale = fitted_mesh_transform(spec, mesh)
    model = spawn(unreal.StaticMeshActor, location, rotation)
    model.set_actor_label('AudienceLightingQA_Stage')
    model.set_actor_scale3d(scale)
    component = model.static_mesh_component
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_static_mesh(mesh)
    component.set_collision_profile_name('NoCollision')
    component.set_forced_lod_model(1)
    visible_actors.append(model)
    rig = spawn(rig_bp.generated_class(), location, rotation)
    rig.set_actor_label('AudienceLightingQA_Rig')
    rig.set_actor_scale3d(scale)
    for preview in rig.get_components_by_class(unreal.StaticMeshComponent):
        preview.set_visibility(False)
        preview.set_editor_property('hidden_in_game', True)

    audience = spec.audience
    assert audience.enabled, 'The production PDA must enable its audience layout.'
    snapshot = unreal.FFZSAudienceSnapshot()
    snapshot.set_editor_property('layout', audience)
    snapshot.set_editor_property('capacity', audience.columns * audience.rows)
    snapshot.set_editor_property('facility_transform', unreal.Transform())
    snapshot.set_editor_property('focus_world_cm', audience.focus_local_cm)
    # Samples include both extreme rows/columns and positions surrounding the aisle.
    columns = sorted({round(i * (audience.columns - 1) / 8) for i in range(9)})
    rows = sorted({round(i * (audience.rows - 1) / 6) for i in range(7)})
    slots = []
    for row in rows:
        for column in columns:
            index = row * audience.columns + column
            transform = unreal.FFZSAudienceLibrary.get_audience_slot_transform(snapshot, index)
            assert transform is not None, 'Native audience slot lookup failed: ' + str(index)
            seat_rotation = transform.rotation.rotator()
            actor = spawn(unreal.SkeletalMeshActor, transform.translation, seat_rotation)
            actor.set_actor_label('AudienceLightingQA_SampledSeat_' + str(index))
            skeletal = actor.get_component_by_class(unreal.SkeletalMeshComponent)
            skeletal.set_skeletal_mesh_asset(crowd_mesh)
            skeletal.set_collision_profile_name('NoCollision')
            skeletal.set_editor_property('cast_shadow', False)
            skeletal.set_editor_property('visibility_based_anim_tick_option',
                unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
            skeletal.set_update_animation_in_editor(True)
            skeletal.play_animation(idle, True)
            skeletal.set_position(0.0, False)
            skeletal.set_play_rate(0.0)
            visible_actors.append(actor)
            slots.append({'index': index, 'position_cm': xyz(transform.translation),
                          'yaw_degrees': float(seat_rotation.yaw)})
    state['audience_capacity'] = int(snapshot.capacity)
    state['sampled_audience_count'] = len(slots)
    state['sampled_slots'] = slots
    state['crowd_asset'] = CROWD
    state['crowd_pose'] = IDLE + ' at time 0; frozen for both captures'
    state['capture_limitation'] = 'Sampled authored seats in Entry; not a running game or crowd simulation.'
    state['stage_transform'] = {'location': xyz(location), 'scale': xyz(scale), 'yaw': float(rotation.yaw)}

    ground = spawn(unreal.StaticMeshActor, unreal.Vector(0, 1500, -12))
    ground.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
    ground.static_mesh_component.set_material(0, unreal.load_asset('/Engine/BasicShapes/BasicShapeMaterial'))
    ground.set_actor_scale3d(unreal.Vector(100, 110, 0.2))
    visible_actors.append(ground)
    # Constant low fill keeps unlit silhouettes legible without obscuring the wash.
    for label, light_rotation, intensity in [('Key', unreal.Rotator(pitch=-45, yaw=-130, roll=0), 0.3),
                                              ('Fill', unreal.Rotator(pitch=-20, yaw=45, roll=0), 0.08)]:
        light_actor = spawn(unreal.DirectionalLight, unreal.Vector(0, 0, 2000), light_rotation)
        light_actor.set_actor_label('AudienceLightingQA_' + label)
        light = light_actor.get_component_by_class(unreal.DirectionalLightComponent)
        light.set_mobility(unreal.ComponentMobility.MOVABLE)
        light.set_editor_property('cast_shadows', False)
        light.set_intensity(intensity)

    camera = unreal.Vector(4800, -900, 2300)
    target = unreal.Vector(0, 1550, 250)
    capture_actor = spawn(unreal.SceneCapture2D, camera,
                          unreal.MathLibrary.find_look_at_rotation(camera, target))
    capture = capture_actor.get_component_by_class(unreal.SceneCaptureComponent2D)
    texture = unreal.RenderingLibrary.create_render_target2d(world, 1800, 1080,
                                                              unreal.TextureRenderTargetFormat.RTF_RGBA8)
    capture.set_editor_property('texture_target', texture)
    capture.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    capture.set_editor_property('projection_type', unreal.CameraProjectionMode.PERSPECTIVE)
    capture.set_editor_property('fov_angle', 58.0)
    capture.set_editor_property('capture_every_frame', False)
    capture.set_editor_property('capture_on_movement', False)
    capture.set_editor_property('always_persist_rendering_state', True)
    capture.set_editor_property('primitive_render_mode', unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
    for actor in visible_actors:
        capture.show_only_actor_components(actor)
    pp = capture.get_editor_property('post_process_settings')
    for prop, value in [('override_auto_exposure_min_brightness', True), ('override_auto_exposure_max_brightness', True),
                        ('auto_exposure_min_brightness', 4.0), ('auto_exposure_max_brightness', 4.0),
                        ('override_bloom_intensity', True), ('bloom_intensity', 0.15),
                        ('override_vignette_intensity', True), ('vignette_intensity', 0.0)]:
        pp.set_editor_property(prop, value)
    capture.set_editor_property('post_process_settings', pp)
    state['camera'] = {'position': xyz(camera), 'target': xyz(target), 'fov_degrees': 58,
                       'resolution': [1800, 1080], 'fixed_exposure_ev100': 4}
    state['source_hashes_before'] = {asset.get_path_name(): source_hash(asset) for asset in loaded_assets}
    lighting = unreal.FFZSFacilityLightingState()
    for key in ('enabled', 'brightness_multiplier', 'override_color', 'color'):
        lighting.set_editor_property(key, spec.lighting_settings.get_editor_property(key))
    assert lighting.enabled
    phases = ['before', 'after']

    def finish():
        handle = state.pop('callback', None)
        if handle is not None:
            unreal.unregister_slate_post_tick_callback(handle)
        state['source_hashes_after'] = {asset.get_path_name(): source_hash(asset) for asset in loaded_assets}
        if state['source_hashes_after'] != state['source_hashes_before']:
            state['error'] = 'Source asset hashes changed during the capture.'
        for actor in reversed(spawned):
            editor.destroy_actor(actor)
        state['status'] = 'PASS' if 'error' not in state and len(state['captures']) == 2 else 'FAIL'
        state['elapsed_seconds'] = time.time() - state['started']
        report()
        unreal.log('MAINSTAGE_AUDIENCE_CAPTURE_' + state['status'])
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()

    def tick(dt):
        if state.get('busy'):
            return
        state['busy'] = True
        try:
            if time.time() - state['started'] > 600:
                raise TimeoutError('Audience capture exceeded ten minutes.')
            if state['phase'] == len(phases):
                finish()
                return
            mode = phases[state['phase']]
            if state['frame'] == 0:
                rig.apply_lighting_state(lighting, True, component)
                lights = rig.get_components_by_class(unreal.LightComponent)
                washes = [light for light in lights if light.get_name() in WASH_NAMES]
                assert len(washes) == 2, 'Expected both authored AudienceWash components: ' + str([light.get_name() for light in lights])
                if mode == 'before':
                    for light in washes:
                        light.set_visibility(False)
                        light.set_intensity(0.0)
                state['light_readback'] = {light.get_name(): {'intensity': float(light.intensity),
                                                              'visible': bool(light.is_visible())} for light in lights}
                capture.set_editor_property('camera_cut_this_frame', True)
                state['phase_started'] = time.time()
            state['frame'] += 1
            elapsed = time.time() - state['phase_started']
            # Repeated warmup captures populate temporal/Lumen history at identical exposure.
            if state['frame'] % 15 == 0 and 'render_requested' not in state:
                capture.capture_scene()
            if state['frame'] >= 70 and elapsed >= (20.0 if state['phase'] == 0 else 4.0) and 'render_requested' not in state:
                capture.capture_scene()
                state['render_requested'] = time.time()
                state['render_frame'] = state['frame']
                return
            if ('render_requested' in state and state['frame'] > state['render_frame'] + 10
                    and time.time() - state['render_requested'] > 0.6):
                filename = mode + '.png'
                unreal.RenderingLibrary.export_render_target(world, texture, str(OUT), filename)
                path = OUT / filename
                assert path.is_file() and path.stat().st_size > 1000, 'Missing capture: ' + str(path)
                state['captures'].append({'mode': mode, 'path': str(path), 'bytes': path.stat().st_size,
                                          'light_readback': state['light_readback'], 'visual_review': 'NOT_REVIEWED'})
                state['phase'] += 1
                state['frame'] = 0
                state.pop('render_requested')
                state.pop('render_frame')
                report()
        except Exception:
            state['error'] = traceback.format_exc()
            unreal.log_error(state['error'])
            finish()
        finally:
            state['busy'] = False

    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    state['callback'] = unreal.register_slate_post_tick_callback(tick)
    report()


if __name__ == '__main__':
    try:
        start()
    except Exception:
        error = traceback.format_exc()
        (OUT / 'capture_error.txt').write_text(error, encoding='utf-8')
        unreal.log_error(error)
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()
