"""전 세계 지구의 대륙·양극·경도 이음선을 별도 PIE로 확인하고 캡처한다."""
from pathlib import Path
import unreal,time,json,traceback,math
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
DEST='/Game/FestaFiesta/UI/Atlas/Earth/DioramaWorld_v1'
OUT=ROOT/'Saved/DioramaWorldVerify';OUT.mkdir(exist_ok=True)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);level.load_level(DEST+'/L_Diorama_World')
state={'stage':0,'next':time.monotonic()+4};handle=None
views=[('01_Asia',110,27),('02_Europe',15,35),('03_Americas',-80,15),('04_NorthPole',0,85),('05_SouthPole',0,-85),('06_PacificSeam',180,20)]
def finish(error=None):
    state['status']='failed' if error else 'complete'
    if error:state['error']=error
    (OUT/'report.json').write_text(json.dumps(state,indent=2),encoding='utf8')
    unreal.unregister_slate_post_tick_callback(handle);level.editor_request_end_play();unreal.SystemLibrary.quit_editor()
def tick(dt):
    try:
        now=time.monotonic()
        if now<state['next']:return
        step=state['stage']
        if step==0:level.editor_request_begin_play();state.update(stage=1,next=now+10);return
        world=unreal.EditorLevelLibrary.get_game_world()
        camera=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.CameraActor)[0]
        if step==1:
            unreal.GameplayStatics.get_player_controller(world,0).set_view_target_with_blend(camera,0)
            pawn=unreal.GameplayStatics.get_player_pawn(world,0)
            if pawn:pawn.set_actor_hidden_in_game(True)
        index=(step-1)//2
        if index>=len(views):finish();return
        name,lon,lat=views[index]
        if step%2:
            lo=math.radians(lon);la=math.radians(lat)
            loc=unreal.Vector(22000*math.cos(la)*math.cos(lo),-22000*math.cos(la)*math.sin(lo),22000*math.sin(la))
            camera.set_actor_location(loc,False,False);camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(loc,unreal.Vector(0,0,0)),False)
            # 관찰 방향에 맞춘 스튜디오 조명. 실제 게임의 조명 회전은 별도로 검증한다.
            lights=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.DirectionalLight)
            for light in lights:
                if 'Key' in light.get_actor_label():light.set_actor_rotation(unreal.Rotator(-35,180-lon-25,0),False)
            state.update(stage=step+1,next=now+5)
        else:
            unreal.SystemLibrary.execute_console_command(world,'HighResShot 1600x1200 filename="'+str(OUT/(name+'.png')).replace('\\','/')+'"')
            state.update(stage=step+1,next=now+3)
    except Exception:finish(traceback.format_exc())
handle=unreal.register_slate_post_tick_callback(tick)
