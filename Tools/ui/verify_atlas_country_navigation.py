"""Frontend PIE 국가 목록/선택/회전/프랍/재진입 확인 및 UI 캡처."""
from pathlib import Path
import unreal,time,json,traceback
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
out=root/'Saved/AtlasNavigationVerify';out.mkdir(exist_ok=True)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
level.load_level('/Game/FestaFiesta/Maps/Frontend')
state={'stage':0,'next':time.monotonic()+3};handle=None
def finish(error=None):
    state['status']='failed' if error else 'complete'
    if error:state['error']=error
    (out/'report.json').write_text(json.dumps(state,indent=2),encoding='utf8')
    unreal.unregister_slate_post_tick_callback(handle);level.editor_request_end_play();unreal.SystemLibrary.quit_editor()
def shot(world,name):
    unreal.SystemLibrary.execute_console_command(world,'Shot showui filename="'+str(out/(name+'.png')).replace('\\','/')+'"')
def tick(dt):
    try:
        now=time.monotonic()
        if now<state['next']:return
        step=state['stage']
        if step==0:level.editor_request_begin_play();state.update(stage=1,next=now+10);return
        world=unreal.EditorLevelLibrary.get_game_world()
        globe=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.FF_AtlasGlobeActor)[0]
        instance=unreal.GameplayStatics.get_game_instance(world)
        flow=next(x for x in unreal.ObjectIterator(unreal.FF_UIFlowSubsystem) if x.get_outer()==instance)
        if step==1:
            flow.go_to_screen(unreal.FF_UIFlowScreen.ATLAS_GLOBE)
            state.update(stage=2,next=now+5);return
        if step==10:
            flow.go_to_screen(unreal.FF_UIFlowScreen.ATLAS_GLOBE)
            state.update(stage=11,next=now+4);return
        screen=next(x for x in unreal.ObjectIterator(unreal.FF_AtlasGlobeScreen) if x.is_activated())
        if step==2:
            countries=screen.get_editor_property('ScrollBox_Countries')
            assert countries.get_children_count()==8
            assert countries.get_child_at(0).get_editor_property('style').get_name()=='CBS_FF_AtlasTransparent_C'
            assert screen.get_editor_property('CanvasPanel_Pins').get_children_count()==0
            assert screen.get_editor_property('Border_Panel').get_visibility()==unreal.SlateVisibility.COLLAPSED
            state['countries']=8;state['world_labels']=0
            shot(world,'01_CountryList');state.update(stage=3,next=now+3);return
        if step==3:
            row=screen.get_editor_property('ScrollBox_Countries').get_child_at(0)
            row.on_ff_button_clicked.broadcast(row)
            state.update(stage=4,next=now+4);return
        if step==4:
            assert str(globe.get_selected_country_id())=='KR'
            assert str(globe.get_focused_country_id())=='KR'
            assert screen.get_editor_property('Button_SelectCountry').get_is_enabled()
            shot(world,'02_Korea');state.update(stage=5,next=now+3);return
        if step==5:
            screen.inspect_country('JP');state.update(stage=6,next=now+4);return
        if step==6:
            assert str(globe.get_selected_country_id())=='JP'
            assert str(globe.get_focused_country_id())=='JP'
            assert not screen.get_editor_property('Button_SelectCountry').get_is_enabled()
            shot(world,'03_JapanLocked');state.update(stage=7,next=now+3);return
        if step==7:
            screen.inspect_country('US');state.update(stage=8,next=now+4);return
        if step==8:
            assert str(globe.get_focused_country_id())=='US'
            shot(world,'04_America');state.update(stage=9,next=now+3);return
        if step==9:
            globe.add_drag_input(unreal.Vector2D(80,0))
            assert str(globe.get_selected_country_id())=='None'
            flow.go_to_screen(unreal.FF_UIFlowScreen.PLAY_HUB)
            state.update(stage=10,next=now+2);return
        if step==11:
            assert screen.get_editor_property('ScrollBox_Countries').get_children_count()==8
            assert screen.get_editor_property('CanvasPanel_Pins').get_children_count()==0
            assert screen.get_editor_property('Border_Panel').get_visibility()==unreal.SlateVisibility.COLLAPSED
            state['reentry']=True;finish()
    except Exception:finish(traceback.format_exc())
handle=unreal.register_slate_post_tick_callback(tick)
