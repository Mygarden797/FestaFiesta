"""저장된 시험 레벨 한 장만 촬영. 작업 완료 확인 후 executor가 정상 종료한다."""
from pathlib import Path
import gc
import json
import math
import struct
import time
import traceback
import unreal

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT=ROOT/'Saved/BrightGlobe/review_v002';OUT.mkdir(parents=True,exist_ok=True)
IMAGE=OUT/'region_default_40_650_final.png'
assert not IMAGE.exists(),'이전 캡처 보존. 새 출력 이름을 지정해야 함'
state={'stage':0,'next':time.monotonic()+2,'started':time.monotonic()}
report={'status':'running','level':'/Game/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1/L_BrightGlobe_Pilot_v002',
    'resolution':[1600,900],'kind':'지역 시험 실제 UE 렌더, UI 없음','termination':'executor keep-alive 해제 후 지연 종료'}
handle=None


def finish(error=None):
    report['status']='failed' if error else 'image_saved'
    if error:report['error']=error;unreal.log_error('BRIGHT_GLOBE_FAIL '+error)
    (OUT/'capture.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle)
    state.clear();gc.collect()
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)


def setup():
    level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert level.load_level(report['level'])
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    globe=next(a for a in actors.get_all_level_actors() if isinstance(a,unreal.FF_AtlasGlobeActor))
    globe.set_actor_tick_enabled(False)
    root=next(c for c in globe.get_components_by_class(unreal.SceneComponent) if c.get_name()=='GlobeRoot')
    cam=actors.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(),unreal.Rotator())
    lat,lon=math.radians(42),math.radians(12)
    local=unreal.Vector(5000*math.cos(lat)*math.cos(lon),-5000*math.cos(lat)*math.sin(lon),5000*math.sin(lat))
    center=globe.get_actor_location()
    surface=unreal.MathLibrary.transform_location(root.get_world_transform(),local)
    delta=surface-center
    length=math.sqrt(delta.x**2+delta.y**2+delta.z**2)
    loc=center+delta*(11000./length)
    cam.set_actor_location_and_rotation(loc,unreal.MathLibrary.find_look_at_rotation(loc,center),False,True)
    camera=cam.get_component_by_class(unreal.CameraComponent)
    for k,v in dict(field_of_view=55.,aspect_ratio=1600/900,constrain_aspect_ratio=True,
        override_aspect_ratio_axis_constraint=True,aspect_ratio_axis_constraint=unreal.AspectRatioAxisConstraint.ASPECT_RATIO_MAINTAIN_YFOV).items():
        camera.set_editor_property(k,v)
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.SystemLibrary.execute_console_command(world,'r.ScreenPercentage 100')
    state.update(camera=cam)
    report.update(camera_location=str(loc),camera_rotation=str(cam.get_actor_rotation()),root_rotation=str(root.get_editor_property('relative_rotation')))


def tick(dt):
    try:
        now=time.monotonic()
        if now<state['next']:return
        state['next']=now+100000
        if now-state['started']>180:raise TimeoutError('단일 캡처 180초 초과')
        stage=state['stage']
        if stage==0:
            setup();state.update(stage=1,next=time.monotonic()+10)
        elif stage==1:
            task=unreal.AutomationLibrary.take_high_res_screenshot(1600,900,IMAGE.as_posix(),camera=state['camera'],delay=1.)
            assert task.is_valid_task()
            state.update(task=task,stage=2,next=time.monotonic()+1)
        elif stage==2:
            if not state['task'].is_task_done():
                state['next']=time.monotonic()+.5;return
            assert IMAGE.exists() and IMAGE.stat().st_size>10000
            with IMAGE.open('rb') as fp:header=fp.read(24)
            assert header[:8]==b'\x89PNG\r\n\x1a\n' and struct.unpack('>II',header[16:24])==(1600,900)
            report.update(image=str(IMAGE),bytes=IMAGE.stat().st_size,task_complete=True)
            state.pop('task',None);state.update(stage=3,next=time.monotonic()+2)
        else:finish()
    except Exception:finish(traceback.format_exc())


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle=unreal.register_slate_post_tick_callback(tick)
