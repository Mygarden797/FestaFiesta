"""실제 타이틀 카메라 수치를 사용한 1600x900 장면 비교. UI 없는 지형 검토용."""
from pathlib import Path
import json
import math
import re
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = ROOT/'Saved/BrightGlobe/review'; OUT.mkdir(parents=True,exist_ok=True)
BASE = json.loads((ROOT/'Saved/BrightGlobe/baseline/report.json').read_text(encoding='utf-8'))['shots'][0]
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
state = {'stage':0,'next':time.monotonic()+2,'report':{'status':'running','resolution':[1600,900],
    'kind':'실제 타이틀 카메라 수치를 재현한 장면 렌더; UI는 포함하지 않음','shots':[]}}
handle = None


def fields(text, keys):
    return [float(re.search(r'\b'+key+r':\s*(-?[\d.]+)',text).group(1)) for key in keys]


def load_scene(path):
    assert level.load_level(path)
    globe = next(a for a in actors.get_all_level_actors() if isinstance(a,unreal.FF_AtlasGlobeActor))
    globe.set_actor_tick_enabled(False)
    root = next(c for c in globe.get_components_by_class(unreal.SceneComponent) if c.get_name()=='GlobeRoot')
    cam = actors.spawn_actor_from_class(unreal.CameraActor,
        unreal.Vector(*fields(BASE['camera_location'],['x','y','z'])),
        unreal.Rotator(**dict(zip(['pitch','yaw','roll'],fields(BASE['camera_rotation'],['pitch','yaw','roll'])))))
    camera = cam.get_component_by_class(unreal.CameraComponent)
    camera.set_editor_property('field_of_view',BASE['fov'])
    camera.set_editor_property('aspect_ratio',1600/900)
    camera.set_editor_property('constrain_aspect_ratio',True)
    camera.set_editor_property('override_aspect_ratio_axis_constraint',True)
    camera.set_editor_property('aspect_ratio_axis_constraint',unreal.AspectRatioAxisConstraint.ASPECT_RATIO_MAINTAIN_YFOV)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.SystemLibrary.execute_console_command(world,'r.ScreenPercentage 100')
    state.update(camera=cam,world=world,root=root,globe=globe)


def shot(name, close=False):
    pitch,yaw,roll=fields(BASE['root_rotation'],['pitch','yaw','roll'])
    state['root'].set_relative_rotation(unreal.Rotator(pitch=pitch,yaw=yaw,roll=roll),False,False)
    if close:
        # 동일한 지형 위경도를 바라보는 근접 카메라. 월드 회전이나 축을 추측하지 않는다.
        lat,lon=math.radians(42),math.radians(12)
        local=unreal.Vector(5000*math.cos(lat)*math.cos(lon),-5000*math.cos(lat)*math.sin(lon),5000*math.sin(lat))
        center=state['globe'].get_actor_location()
        surface=unreal.MathLibrary.transform_location(state['root'].get_world_transform(),local)
        delta=surface-center
        length=math.sqrt(delta.x**2+delta.y**2+delta.z**2)
        location=center+delta*(9000./length)
        rotation=unreal.MathLibrary.find_look_at_rotation(location,center)
        state['camera'].set_actor_location_and_rotation(location,rotation,False,True)
        state['camera'].get_component_by_class(unreal.CameraComponent).set_editor_property('field_of_view',55.)
    state['task'] = unreal.AutomationLibrary.take_high_res_screenshot(1600,900,(OUT/(name+'.png')).as_posix(),
        camera=state['camera'],delay=1.)
    state['report']['shots'].append({'name':name,'root':str(state['root'].get_editor_property('relative_rotation'))})


def controls(thick,mountain):
    col = unreal.load_asset('/Game/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1/P03/MPC_BrightGlobe_Controls')
    for name,value in [('LandThickness',thick),('MountainHeight',mountain)]:
        unreal.MaterialLibrary.set_scalar_parameter_value(state['world'],col,name,value)
        assert abs(unreal.MaterialLibrary.get_scalar_parameter_value(state['world'],col,name)-value)<.001


def finish(error=None):
    state['report']['status']='failed' if error else 'complete'
    if error:
        state['report']['error']=error;unreal.log_error('BRIGHT_GLOBE_FAIL '+error)
    (OUT/'report.json').write_text(json.dumps(state['report'],ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def tick(dt):
    try:
        now=time.monotonic()
        if now<state['next']:return
        state['next']=now+100000
        step=state['stage']
        if step==0:
            load_scene('/Game/FestaFiesta/Maps/Frontend');state.update(stage=1,next=now+8)
        elif step==1:
            shot('01_original_title_camera');state.update(stage=2,next=time.monotonic()+8)
        elif step==2:
            load_scene('/Game/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1/L_BrightGlobe_Pilot');state.update(stage=3,next=time.monotonic()+10)
        elif step==3:
            controls(40.,650.);shot('02_pilot_default_40_650');state.update(stage=4,next=time.monotonic()+8)
        elif step==4:
            controls(75.,650.);shot('03_pilot_thickness_75');state.update(stage=5,next=time.monotonic()+8)
        elif step==5:
            controls(40.,1000.);shot('04_pilot_mountain_1000');state.update(stage=6,next=time.monotonic()+8)
        elif step==6:
            controls(40.,650.);shot('05_region_default_40_650',True);state.update(stage=7,next=time.monotonic()+8)
        elif step==7:
            controls(75.,650.);shot('06_region_thickness_75',True);state.update(stage=8,next=time.monotonic()+8)
        elif step==8:
            controls(40.,1000.);shot('07_region_mountain_1000',True);state.update(stage=9,next=time.monotonic()+8)
        else:
            controls(40.,650.)
            finish()
    except Exception:finish(traceback.format_exc())


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle=unreal.register_slate_post_tick_callback(tick)
