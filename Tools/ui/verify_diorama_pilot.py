"""별도 에디터에서 시험 레벨 기본/두께/산 높이 비교 캡처 후 정상 종료."""
from pathlib import Path
import unreal, time, json, traceback, math
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
DEST='/Game/FestaFiesta/UI/Atlas/Earth/DioramaPilot_v1'
OUT=ROOT/'Saved/DioramaPilotVerify';OUT.mkdir(exist_ok=True)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
level.load_level(DEST+'/L_Diorama_EastAsia_Pilot')
sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
for name in ['Top','Coast','Bottom']:
    mesh=unreal.load_asset(DEST+'/SM_Diorama_'+name)
    s=sm.get_lod_build_settings(mesh,0)
    if not s.get_editor_property('use_full_precision_u_vs'):
        s.set_editor_property('use_full_precision_u_vs',True);sm.set_lod_build_settings(mesh,0,s);unreal.EditorAssetLibrary.save_loaded_asset(mesh,False)
col=unreal.load_asset(DEST+'/MPC_Diorama_Controls')
state={'stage':0,'next':time.monotonic()+5};handle=None

def finish(error=None):
    state['status']='failed' if error else 'complete'
    if error:state['error']=error
    (OUT/'report.json').write_text(json.dumps(state,indent=2),encoding='utf8')
    unreal.unregister_slate_post_tick_callback(handle);level.editor_request_end_play();unreal.SystemLibrary.quit_editor()

def set_values(world,base,mountain):
    # 런타임 변경은 에셋 기본값을 저장하지 않는다.
    unreal.MaterialLibrary.set_scalar_parameter_value(world,col,'LandThickness',base)
    unreal.MaterialLibrary.set_scalar_parameter_value(world,col,'MountainHeight',mountain)

def shot(world,name):
    unreal.SystemLibrary.execute_console_command(world,'HighResShot 1600x1200 filename="'+str(OUT/(name+'.png')).replace('\\','/')+'"')

def tick(dt):
    try:
        now=time.monotonic()
        if now<state['next']:return
        step=state['stage']
        if step==0:
            level.editor_request_begin_play();state.update(stage=1,next=now+10);return
        world=unreal.EditorLevelLibrary.get_game_world()
        if step==1:
            camera=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.CameraActor)[0]
            pc=unreal.GameplayStatics.get_player_controller(world,0);pc.set_view_target_with_blend(camera,0)
            pawn=unreal.GameplayStatics.get_player_pawn(world,0)
            if pawn:pawn.set_actor_hidden_in_game(True)
            state['controls']={'LandThickness':100,'MountainHeight':350}
            set_values(world,100,350);state.update(stage=2,next=now+5);return
        if step==2:shot(world,'01_Default_100_350');state.update(stage=3,next=now+3);return
        if step==3:set_values(world,220,350);state.update(stage=4,next=now+4);return
        if step==4:shot(world,'02_Thickness_220_350');state.update(stage=5,next=now+3);return
        if step==5:set_values(world,100,650);state.update(stage=6,next=now+4);return
        if step==6:shot(world,'03_Mountains_100_650');state.update(stage=7,next=now+3);return
        finish()
    except Exception:finish(traceback.format_exc())
handle=unreal.register_slate_post_tick_callback(tick)
