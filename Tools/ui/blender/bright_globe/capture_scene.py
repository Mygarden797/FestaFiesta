"""Saved/BrightGlobe/capture_request.json의 한 장을 완료 확인 후 정상 종료한다."""
from pathlib import Path
import gc,json,math,re,struct,time,traceback
import unreal

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT=ROOT/'Saved/BrightGlobe'
request=json.loads((OUT/'capture_request.json').read_text(encoding='utf-8'))
IMAGE=(OUT/request['output']).resolve()
assert IMAGE.is_relative_to(OUT.resolve()) and not IMAGE.exists()
IMAGE.parent.mkdir(parents=True,exist_ok=True)
state={'stage':0,'next':time.monotonic()+2,'started':time.monotonic()}
report={'status':'running','request':request,'image':str(IMAGE),'termination':'task 완료 후 callback/keepalive 해제'}
handle=None
def fields(text,keys):return [float(re.search(r'\b'+k+r':\s*(-?[\d.]+)',text).group(1)) for k in keys]
def setup():
    level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert level.load_level(request['level'])
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    globe=next(a for a in actors.get_all_level_actors() if isinstance(a,unreal.FF_AtlasGlobeActor))
    globe.set_actor_tick_enabled(False)
    root=next(c for c in globe.get_components_by_class(unreal.SceneComponent) if c.get_name()=='GlobeRoot')
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if request.get('controls'):
        col=unreal.load_asset(request['collection'])
        for k,v in request['controls'].items():unreal.MaterialLibrary.set_scalar_parameter_value(world,col,k,v)
    if request.get('camera','title')=='title':
        base=json.loads((OUT/'baseline/report.json').read_text(encoding='utf-8'))['shots'][0]
        root.set_relative_rotation(unreal.Rotator(**dict(zip(['pitch','yaw','roll'],fields(base['root_rotation'],['pitch','yaw','roll'])))),False,False)
        loc=unreal.Vector(*fields(base['camera_location'],['x','y','z']))
        rot=unreal.Rotator(**dict(zip(['pitch','yaw','roll'],fields(base['camera_rotation'],['pitch','yaw','roll']))))
        fov=base['fov']
    else:
        lat,lon=map(math.radians,request.get('latlon',[42,12]))
        local=unreal.Vector(5000*math.cos(lat)*math.cos(lon),-5000*math.cos(lat)*math.sin(lon),5000*math.sin(lat))
        center=globe.get_actor_location()
        delta=unreal.MathLibrary.transform_location(root.get_world_transform(),local)-center
        length=math.sqrt(delta.x**2+delta.y**2+delta.z**2)
        loc=center+delta*(request.get('distance',11000.)/length)
        rot=unreal.MathLibrary.find_look_at_rotation(loc,center);fov=request.get('fov',55.)
    cam=actors.spawn_actor_from_class(unreal.CameraActor,loc,rot)
    camera=cam.get_component_by_class(unreal.CameraComponent)
    w,h=request.get('resolution',[1600,900])
    for k,v in dict(field_of_view=fov,aspect_ratio=w/h,constrain_aspect_ratio=True,override_aspect_ratio_axis_constraint=True,aspect_ratio_axis_constraint=unreal.AspectRatioAxisConstraint.ASPECT_RATIO_MAINTAIN_YFOV).items():camera.set_editor_property(k,v)
    unreal.SystemLibrary.execute_console_command(world,'r.ScreenPercentage 100')
    state.update(camera=cam)
    report.update(camera_location=str(loc),camera_rotation=str(rot),root_rotation=str(root.get_editor_property('relative_rotation')),fov=fov)
def finish(error=None):
    report['status']='failed' if error else 'image_saved'
    if error:report['error']=error;unreal.log_error('BRIGHT_GLOBE_FAIL '+error)
    IMAGE.with_suffix('.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle);state.clear();gc.collect()
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
def tick(dt):
    try:
        now=time.monotonic()
        if now<state['next']:return
        state['next']=now+100000
        if now-state['started']>240:raise TimeoutError('캡처 240초 초과')
        if state['stage']==0:setup();state.update(stage=1,next=time.monotonic()+10)
        elif state['stage']==1:
            w,h=request.get('resolution',[1600,900])
            task=unreal.AutomationLibrary.take_high_res_screenshot(w,h,IMAGE.as_posix(),camera=state['camera'],delay=1.)
            assert task.is_valid_task();state.update(task=task,stage=2,next=time.monotonic()+1)
        elif state['stage']==2:
            if not state['task'].is_task_done():state['next']=now+.5;return
            assert IMAGE.exists() and IMAGE.stat().st_size>10000
            with IMAGE.open('rb') as fp:header=fp.read(24)
            assert header[:8]==b'\x89PNG\r\n\x1a\n' and list(struct.unpack('>II',header[16:24]))==request.get('resolution',[1600,900])
            report.update(task_complete=True,bytes=IMAGE.stat().st_size)
            state.pop('task');state.update(stage=3,next=time.monotonic()+2)
        else:finish()
    except Exception:finish(traceback.format_exc())
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle=unreal.register_slate_post_tick_callback(tick)
