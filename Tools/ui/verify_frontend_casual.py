"""실제 PIE 화면·Slate 클릭·호버 해제·분류 이동·재진입을 검증한다. 저장 설정은 변경하지 않는다."""
from pathlib import Path
import unreal,time,json,traceback
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
out=root/'Saved/FrontendCasualVerify';out.mkdir(exist_ok=True)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
level.load_level('/Game/FestaFiesta/Maps/Frontend')
state={'checks':[],'stage':0,'start':time.monotonic()};actions=[];handle=None
def world(): return unreal.EditorLevelLibrary.get_game_world()
def flow():
    gi=unreal.GameplayStatics.get_game_instance(world())
    return next(x for x in unreal.ObjectIterator(unreal.FF_UIFlowSubsystem) if x.get_outer()==gi)
def screen(cls):return next(x for x in unreal.ObjectIterator(cls) if x.get_world()==world() and x.is_activated())
def cmd(text):unreal.SystemLibrary.execute_console_command(world(),text)
def pointer(name,click=False):cmd('FF.FrontendPointer '+name+(' Click' if click else ''))
def shot(name):cmd('Shot showui filename="'+str(out/(name+'.png')).replace('\\','/')+'"')
def color(button):
    label=button.get_editor_property('Text_Label')
    c=label.get_editor_property('color_and_opacity').get_editor_property('specified_color')
    return [c.r,c.g,c.b,c.a]
def verify_category(name,title):
    s=screen(unreal.FF_OptionsScreen)
    assert str(s.get_editor_property('Text_CategoryTitle').get_text())==title,(name,str(s.get_editor_property('Text_CategoryTitle').get_text()))
    for suffix in ('Language','Accessibility','Sound','Graphics','Controls','Credits'):
        c=color(s.get_editor_property('Button_'+suffix))
        assert c[0]<.2 and c[1]<.3 and c[2]<.2 and c[3]>.95,(suffix,c)
    state['checks'].append('선택·호버 해제 후 6개 라벨 대비: '+title)
def verify_countries(selected=None):
    s=screen(unreal.FF_AtlasGlobeScreen);rows=s.get_editor_property('ScrollBox_Countries')
    assert rows.get_children_count()==8
    assert s.get_editor_property('CanvasPanel_Pins').get_children_count()==0
    if selected is None:assert s.get_editor_property('Border_Panel').get_visibility()==unreal.SlateVisibility.COLLAPSED
    else:
        assert str(s.get_editor_property('Text_CountryName').get_text())==selected
    state['checks'].append('국가 8개·지구 핀 0개·상세 카드: '+str(selected))
def finish(error=None):
    state['status']='failed' if error else 'complete'
    if error:state['error']=error
    state['elapsed']=time.monotonic()-state['start']
    (out/'report.json').write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf8')
    unreal.unregister_slate_post_tick_callback(handle);level.editor_request_end_play();unreal.SystemLibrary.quit_editor()
actions.append((lambda:unreal.SystemLibrary.execute_console_command(level.get_world(), 'FF.FrontendBeginPIE 1600 900'),10))
actions.append((lambda:shot('00_Title'),2))
actions.append((lambda:pointer('Btn_Play',True),3))
actions.append((lambda:(screen(unreal.FF_PlayHubScreen),shot('01_PlayHub')),2))
actions.append((lambda:pointer('Button_NewFestival',True),5))
actions.append((lambda:(verify_countries(),shot('02_Countries')),2))
actions.append((lambda:screen(unreal.FF_AtlasGlobeScreen).inspect_country('KR'),4))
actions.append((lambda:(verify_countries('대한민국'),shot('03_Korea')),2))
actions.append((lambda:screen(unreal.FF_AtlasGlobeScreen).inspect_country('JP'),4))
actions.append((lambda:(verify_countries('일본'),shot('04_Japan')),2))
actions.append((lambda:flow().go_to_screen(unreal.FF_UIFlowScreen.OPTIONS),3))
for suffix,title in [('Language','언어'),('Accessibility','접근성'),('Sound','사운드'),('Graphics','그래픽'),('Controls','조작'),('Credits','크레딧')]:
    actions.append((lambda suffix=suffix:pointer('Button_'+suffix,True),1))
    actions.append((lambda:pointer('Away'),1))
    actions.append((lambda suffix=suffix,title=title:(verify_category(suffix,title),shot('05_Options_'+suffix)),2))
actions.append((lambda:flow().go_to_screen(unreal.FF_UIFlowScreen.SAVE_LIST),3))
actions.append((lambda:shot('06_Saves'),2))
actions.append((lambda:flow().go_to_screen(unreal.FF_UIFlowScreen.PLAY_HUB),3))
actions.append((lambda:pointer('Button_NewFestival',True),3))
actions.append((lambda:verify_countries(),1))
state['next']=time.monotonic()+3
def tick(dt):
    try:
        if time.monotonic()-state['start']>240:raise RuntimeError('PIE 검증 제한 시간 초과')
        if time.monotonic()<state['next']:return
        i=state['stage']
        if i>=len(actions):finish();return
        action,delay=actions[i];action();state['stage']=i+1;state['next']=time.monotonic()+delay
    except Exception:finish(traceback.format_exc())
handle=unreal.register_slate_post_tick_callback(tick)
