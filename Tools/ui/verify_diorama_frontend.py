"""실제 Frontend에서 독립 육지 소유·회전·숨김과 국가 선택 회귀를 확인한다."""
from pathlib import Path
import unreal,time,json,traceback
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT=ROOT/'Saved/DioramaFrontendVerify';OUT.mkdir(exist_ok=True)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);level.load_level('/Game/FestaFiesta/Maps/Frontend')
state={'stage':0,'next':time.monotonic()+4};handle=None
def finish(error=None):
    state['status']='failed' if error else 'complete'
    if error:state['error']=error
    (OUT/'report.json').write_text(json.dumps(state,indent=2),encoding='utf8')
    unreal.unregister_slate_post_tick_callback(handle);level.editor_request_end_play();unreal.SystemLibrary.quit_editor()
def shot(world,name):
    unreal.SystemLibrary.execute_console_command(world,'Shot showui filename="'+str(OUT/(name+'.png')).replace('\\','/')+'"')
def tick(dt):
    try:
        now=time.monotonic()
        if now<state['next']:return
        step=state['stage']
        if step==0:level.editor_request_begin_play();state.update(stage=1,next=now+10);return
        world=unreal.EditorLevelLibrary.get_game_world()
        globe=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.FF_AtlasGlobeActor)[0]
        if step==1:
            layers=list(globe.get_editor_property('land_layer_components'))
            assert len(layers)==16 and all(c.get_owner()==globe and c.get_editor_property('static_mesh') for c in layers)
            parent=layers[0].get_attach_parent();assert parent.get_name()=='GlobeRoot'
            assert all(c.get_attach_parent()==parent for c in layers)
            state['layers']=16;state['shared_rotation_root']=parent.get_name()
            shot(world,'01_Title');state.update(stage=2,next=now+3);return
        if step==2:
            instance=unreal.GameplayStatics.get_game_instance(world)
            flow=next(x for x in unreal.ObjectIterator(unreal.FF_UIFlowSubsystem) if x.get_outer()==instance)
            flow.go_to_screen(unreal.FF_UIFlowScreen.ATLAS_GLOBE);globe.set_auto_spin_paused(True);globe.focus_on_country('KR',True)
            state.update(stage=3,next=now+5);return
        if step==3:shot(world,'02_Korea');state.update(stage=4,next=now+3);return
        if step==4:
            globe.focus_on_country('DE',True);state.update(stage=5,next=now+5);return
        if step==5:
            shot(world,'03_Germany');state.update(stage=6,next=now+3);return
        if step==6:
            globe.set_globe_active(False)
            assert globe.get_editor_property('hidden'),'국가 화면 전환 시 지구 숨김 실패'
            assert all(c.get_owner().get_editor_property('hidden') for c in globe.get_editor_property('land_layer_components'))
            state['hide_show_verified']=True
            state.update(stage=7,next=now+2);return
        if step==7:
            shot(world,'04_Hidden');globe.set_globe_active(True)
            assert not globe.get_editor_property('hidden')
            state.update(stage=8,next=now+2);return
        finish()
    except Exception:finish(traceback.format_exc())
handle=unreal.register_slate_post_tick_callback(tick)
