# -*- coding: utf-8 -*-
"""일회용 에디터에서 PIE 지구본을 확인한다. 레벨이나 에셋을 저장하지 않는다.

-ExecCmds="py C:/UE5/8th-Team13-Final-Project/Tools/ui/verify_atlas_landmarks_pie.py"
검증이 끝나면 이 에디터 세션을 정상 종료한다. 사용자의 기존 에디터에서 실행하지 않는다.
"""
import json
import time
import traceback
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT=ROOT/'Saved/AtlasLandmarksPIE'
OUT.mkdir(parents=True,exist_ok=True)
sub=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
sub.load_level('/Game/FestaFiesta/Maps/Frontend')
state={'stage':0,'next':time.monotonic()+3,'started':time.monotonic(),'countries':[],'samples':[]}
handle=None

def finish(error=None):
    if error:state['error']=error
    state['status']='failed' if error else 'complete'
    (OUT/'report.json').write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle)
    sub.editor_request_end_play()
    unreal.SystemLibrary.quit_editor()

def screenshot(world,name):
    unreal.SystemLibrary.execute_console_command(world,'HighResShot 1600x900 filename="'+str(OUT/(name+'.png')).replace('\\','/')+'"')

def click_country(world,code):
    pin=next(x for x in unreal.ObjectIterator(unreal.FF_AtlasPinButton)
             if x.get_world() == world and str(x.get_pin_id()) == code)
    pin.get_editor_property('on_ff_button_clicked').broadcast(pin)

def tick(delta):
    global handle
    try:
        now=time.monotonic()
        if now-state['started']>180:finish('PIE 검증 시간 초과');return
        if now<state['next']:return
        stage=state['stage']
        if stage==0:
            catalog=unreal.load_asset('/Game/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas')
            for entry in catalog.get_editor_property('countries'):
                mesh=entry.get_editor_property('landmark_mesh')
                state['countries'].append({'id':str(entry.get_editor_property('country_id')),'mesh':str(mesh),'unlocked':entry.get_editor_property('unlocked')})
            for code in ['KR','BR','DE','JP','US','ES','TH','AU']:
                mesh=unreal.load_asset('/Game/FestaFiesta/UI/Atlas/Landmarks_v1/Meshes/SM_FF_Landmark_'+code)
                state.setdefault('bounds',{})[code]=str(mesh.get_bounds())
            sub.editor_request_begin_play()
            state['stage']=1;state['next']=now+8;return
        world=unreal.EditorLevelLibrary.get_game_world()
        if not world:state['next']=now+2;return
        globe=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.FF_AtlasGlobeActor)[0]
        if stage==1:
            instance=unreal.GameplayStatics.get_game_instance(world)
            flow=next(x for x in unreal.ObjectIterator(unreal.FF_UIFlowSubsystem) if x.get_outer()==instance)
            flow.go_to_screen(unreal.FF_UIFlowScreen.ATLAS_GLOBE)
            globe.set_auto_spin_paused(True)
            globe.focus_on_country('KR',True)
            state['stage']=2;state['next']=now+5;return
        if stage==2:
            screenshot(world,'01_Korea_Unselected')
            state['stage']=3;state['next']=now+3;return
        if stage==3:
            click_country(world,'KR')
            state['stage']=4;state['next']=now+3;return
        if stage==4:
            screenshot(world,'02_Korea_Selected')
            state['samples'].append({'case':'KR_selected','orientation':str(globe.get_orientation()),'selected':str(globe.get_selected_country_id())})
            state['stage']=5;state['next']=now+3;return
        if stage==5:
            click_country(world,'DE')
            state['stage']=6;state['next']=now+4;return
        if stage==6:
            screenshot(world,'03_Europe_Selected')
            screen=next(x for x in unreal.ObjectIterator(unreal.FF_AtlasGlobeScreen) if x.get_world() == world)
            button=screen.get_editor_property('button_select_country')
            state['locked_country_entry_button_enabled']=button.get_is_enabled()
            assert not state['locked_country_entry_button_enabled']
            state['samples'].append({'case':'DE_locked_selected','selected':str(globe.get_selected_country_id())})
            state['stage']=6.5;state['next']=now+3;return
        if stage==6.5:
            unreal.SystemLibrary.execute_console_command(world,'Shot showui filename="'+str(OUT/'03_Europe_UI.png').replace('\\','/')+'"')
            state['stage']=7;state['next']=now+3;return
        if stage==7:
            click_country(world,'AU')
            state['stage']=8;state['next']=now+4;return
        if stage==8:
            screenshot(world,'04_Australia_Selected')
            state['stage']=9;state['next']=now+3;return
        finish()
    except Exception:
        message=traceback.format_exc();unreal.log_error(message);finish(message)

handle=unreal.register_slate_post_tick_callback(tick)
unreal.log('[FF_LANDMARKS_PIE] STARTED')



