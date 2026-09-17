"""P1: 기존 Frontend를 저장하지 않고 실제 설정과 PIE 비교 구도를 기록한다."""
from pathlib import Path
import json
import statistics
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = ROOT / 'Saved/BrightGlobe/baseline'
OUT.mkdir(parents=True, exist_ok=True)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
state = {'stage': 0, 'next': time.monotonic() + 5, 'samples': [], 'report': {'status': 'running'}}
handle = None


def serial(value):
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    if isinstance(value, (str, int, float, bool)) or value is None:
        return value
    try:
        return [serial(v) for v in value]
    except TypeError:
        return str(value)


def props(obj, keys):
    result = {}
    for key in keys:
        try:
            result[key] = serial(obj.get_editor_property(key))
        except Exception as exc:
            result[key] = 'unavailable: ' + str(exc)
    return result


def finish(error=None):
    state['report']['status'] = 'failed' if error else 'complete'
    if error:
        state['report']['error'] = error
        unreal.log_error('BRIGHT_GLOBE_FAIL ' + error)
    (OUT / 'report.json').write_text(json.dumps(state['report'], indent=2, ensure_ascii=False), encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle)
    level.editor_request_end_play()
    unreal.SystemLibrary.quit_editor()


def shot(world, name, globe):
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    camera = pc.player_camera_manager
    state['report'].setdefault('shots', []).append({'name': name,
        'camera_location': str(camera.get_camera_location()), 'camera_rotation': str(camera.get_camera_rotation()),
        'fov': camera.get_fov_angle(), 'globe_location': str(globe.get_actor_location()),
        'root_rotation': str(globe.get_component_by_class(unreal.StaticMeshComponent).get_attach_parent().get_editor_property('relative_rotation'))})
    unreal.SystemLibrary.execute_console_command(world, 'Shot showui filename="' + (OUT / (name+'.png')).as_posix() + '"')


def tick(dt):
    try:
        now = time.monotonic()
        if state['stage'] == 2 and dt > 0:
            state['samples'].append(float(dt)*1000)
        if now < state['next']:
            return
        # 에셋 로드/맵 전환 중 Slate가 재진입할 수 있다. 동일 단계를 두 번 실행하지 않는다.
        state['next'] = now + 100000
        step = state['stage']
        if step == 0:
            assert level.load_level('/Game/FestaFiesta/Maps/Frontend')
            items = []
            for actor in actors.get_all_level_actors():
                name = actor.get_class().get_name()
                if not any(s in name for s in ['Light', 'Camera', 'Globe', 'PostProcess', 'StaticMesh']):
                    continue
                item = {'class': name, 'label': actor.get_actor_label(), 'transform': str(actor.get_actor_transform())}
                if isinstance(actor, unreal.FF_AtlasGlobeActor):
                    item['settings'] = props(actor, ['globe_radius', 'globe_mesh_asset', 'globe_material', 'land_layer_meshes',
                        'catalog_override', 'globe_texture_yaw_offset', 'start_lat_long', 'start_facing_first_country'])
                item['components'] = []
                for comp in actor.get_components_by_class(unreal.ActorComponent):
                    if isinstance(comp, unreal.StaticMeshComponent):
                        data = props(comp, ['static_mesh', 'override_materials', 'relative_scale3d'])
                        data['materials'] = [serial(comp.get_material(i)) for i in range(comp.get_num_materials())]
                    elif isinstance(comp, unreal.SkyLightComponent):
                        data = props(comp, ['intensity', 'light_color', 'cast_shadows', 'sky_distance_threshold'])
                    elif isinstance(comp, unreal.LightComponent):
                        data = props(comp, ['intensity', 'light_color', 'cast_shadows', 'temperature'])
                    elif isinstance(comp, unreal.CameraComponent):
                        data = props(comp, ['field_of_view', 'constrain_aspect_ratio', 'aspect_ratio', 'post_process_blend_weight'])
                    else:
                        continue
                    data['name'] = comp.get_name()
                    item['components'].append(data)
                if isinstance(actor, unreal.PostProcessVolume):
                    settings = actor.get_editor_property('settings')
                    item['postprocess'] = props(settings, ['auto_exposure_method', 'auto_exposure_min_brightness',
                        'auto_exposure_max_brightness', 'auto_exposure_bias', 'bloom_intensity', 'ambient_occlusion_intensity'])
                items.append(item)
            state['report']['actors'] = items
            mpc = unreal.load_asset('/Game/FestaFiesta/UI/Atlas/Earth/DioramaWorld_v1/MPC_Diorama_Controls')
            state['report']['mpc'] = [props(p, ['parameter_name', 'default_value']) for p in mpc.get_editor_property('scalar_parameters')]
            state['report']['quality'] = {key: unreal.SystemLibrary.get_console_variable_int_value(key) for key in
                ['sg.ViewDistanceQuality', 'sg.AntiAliasingQuality', 'sg.ShadowQuality', 'sg.PostProcessQuality',
                 'sg.TextureQuality', 'sg.EffectsQuality', 'sg.FoliageQuality', 'r.ScreenPercentage', 'r.VSync']}
            state['report']['source_triangles'] = 2091252
            state['report']['triangles_basis'] = '현행 제작 보고서 Top8+Coast8; GPU 측정치 아님'
            state['report']['settings_ini'] = 'Config/DefaultGame.ini'
            level.editor_request_begin_play()
            state.update(stage=1, next=now+15)
            return
        world = unreal.EditorLevelLibrary.get_game_world()
        globe = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.FF_AtlasGlobeActor)[0]
        if step == 1:
            globe.set_auto_spin_paused(True)
            globe.focus_on_country('DE', True)
            unreal.SystemLibrary.execute_console_command(world, 'r.SetRes 1600x900w')
            state.update(stage=2, next=now+10)
        elif step == 2:
            shot(world, '01_title_europe', globe)
            samples = sorted(state['samples'])
            state['report']['frame_callback_ms'] = {'samples': len(samples), 'median': statistics.median(samples),
                'p95': samples[min(len(samples)-1, int(len(samples)*.95))],
                'method': 'PIE Slate callback delta; offscreen; GPU시간/출시성능 수치 아님'}
            state.update(stage=3, next=now+3)
        elif step == 3:
            instance = unreal.GameplayStatics.get_game_instance(world)
            flow = next(x for x in unreal.ObjectIterator(unreal.FF_UIFlowSubsystem) if x.get_outer() == instance)
            flow.go_to_screen(unreal.FF_UIFlowScreen.ATLAS_GLOBE)
            globe.set_auto_spin_paused(True)
            globe.focus_on_country('KR', True)
            state.update(stage=4, next=now+5)
        elif step == 4:
            shot(world, '02_korea', globe)
            globe.focus_on_country('DE', True)
            state.update(stage=5, next=now+5)
        elif step == 5:
            shot(world, '03_europe', globe)
            state.update(stage=6, next=now+3)
        else:
            state['report']['frontend_saved'] = False
            finish()
    except Exception:
        finish(traceback.format_exc())


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle = unreal.register_slate_post_tick_callback(tick)
(OUT / 'report.json').write_text(json.dumps({'status': 'running'}), encoding='utf-8')
