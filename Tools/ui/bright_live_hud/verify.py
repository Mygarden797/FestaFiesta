"""저장된 Live HUD를 렌더링하고 PIE에서 배속·탭·단계 변경을 확인한다. 에셋 저장 없음."""
from pathlib import Path
import hashlib
import json
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = ROOT/'Saved/BrightLiveHud/Runtime'
OUT.mkdir(parents=True,exist_ok=True)
LOG = ROOT/'Saved/BrightGlobe/bright_live_hud_verify.log'
LEVEL = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
STATE = dict(stage='layout',next=time.monotonic()+1,start=time.monotonic(),busy=False)
REPORT = dict(status='running',images=[],checks=[],physical_input_tested=False,
              fixture='PIE Live 진입 순간에만 필수시설 조건을 메모리에서 비우고 즉시 복원. 게임 규칙 검증 아님.')
LAYOUT_DONE = globals().get('FF_LIVE_LAYOUT_DONE',False)
PROTECTED = list((ROOT/'Content/FestaFiesta/UI/LiveHUD').rglob('*.uasset'))
PROTECTED += list((ROOT/'Content/FestaFiesta/Building/FF_WBP_Building/Shared').glob('*.uasset'))
PROTECTED += list((ROOT/'Config').glob('*.ini'))
BEFORE = {str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in PROTECTED}
HANDLE = None

def advance(stage,delay=1): STATE.update(stage=stage,next=time.monotonic()+delay)
def world(): return unreal.EditorLevelLibrary.get_game_world()
def subsystem(cls):
    gi=unreal.GameplayStatics.get_game_instance(world())
    return next((o for o in unreal.ObjectIterator(cls) if o.get_outer()==gi),None)
def widget(cls):
    return next((o for o in unreal.ObjectIterator(cls) if o.get_world()==world()
                 and o.get_parent() is not None
                 and o.get_visibility() not in (unreal.SlateVisibility.COLLAPSED,unreal.SlateVisibility.HIDDEN)),None)
def child(hud,name):
    # 런타임 복제 트리 이름은 고정되지 않는다. 실제 BindWidget 필드를 읽는다.
    return hud.get_editor_property(name)
def command(value): unreal.SystemLibrary.execute_console_command(world() or LEVEL.get_world(),value)
def capture(name):
    path=OUT/(name+'.png')
    command('Shot SHOWUI filename="'+path.as_posix()+'" -nosuffix')
    REPORT['images'].append(str(path))

def tick(delta):
    if STATE['busy'] or time.monotonic()<STATE['next']: return
    STATE['busy']=True
    try:
        STATE['next']=time.monotonic()+.5
        stage=STATE['stage']
        if stage not in ('stop','done'): assert time.monotonic()-STATE['start']<300,'검증 시간 초과'
        if stage=='layout':
            if not LAYOUT_DONE:
                command('Automation RunTests FestaFiesta.UI.LiveHUD.BrightBottom.RenderedLayout')
            advance('layout_done',3)
        elif stage=='layout_done':
            lines=LOG.read_text(encoding='utf-8',errors='replace').splitlines()
            result=[x for x in lines if 'Test Completed. Result=' in x and 'BrightBottom.RenderedLayout' in x]
            if LAYOUT_DONE:
                previous=json.loads((OUT/'report.json').read_text(encoding='utf-8'))
                result=[previous['layout_test']]
                REPORT['layout_result_reused']=True
            if not result: return
            REPORT['layout_test']=result[-1]
            assert 'Result={Success}' in result[-1],result[-1]
            atlas=unreal.load_asset('/Game/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas')
            entry=next(e for e in atlas.get_editor_property('entries') if e.get_editor_property('unlocked')
                       and '다대포' in str(e.get_editor_property('festival_name')))
            STATE['festival']=str(entry.get_editor_property('festival_id'))
            assert LEVEL.load_level('/Game/FestaFiesta/Maps/Frontend')
            command('r.SetRes 4096x2160w')
            advance('pie')
        elif stage=='pie':
            command('FF.FrontendBeginPIE 1598 888')
            advance('festival',5)
        elif stage=='festival':
            if not world(): return
            flow=subsystem(unreal.FF_UIFlowSubsystem)
            if not flow:return
            assert flow.start_festival(unreal.Name(STATE['festival']))
            advance('live',8)
        elif stage=='live':
            if not world() or not widget(unreal.FF_PrepHudBar): return
            progression=subsystem(unreal.FF_FestivalProgressionSubsystem)
            definition=progression.get_definition()
            original=list(definition.get_editor_property('required_setup_facilities'))
            try:
                definition.set_editor_property('required_setup_facilities',[])
                subsystem(unreal.FF_FestivalStageSubsystem).request_transition(unreal.FF_FestivalStage.LIVE)
            finally:
                definition.set_editor_property('required_setup_facilities',original)
            advance('live_capture',5)
        elif stage=='live_capture':
            hud=widget(unreal.FF_LiveHudScreen)
            assert hud,'실제 Live HUD 진입 실패'
            assert abs(hud.get_bottom_status_row_height()-36)<.1
            assert abs(hud.get_bottom_bar_height()-68)<.1
            vm=subsystem(unreal.FF_FestivalViewModelSubsystem)
            assert vm.get_current_stage()==unreal.FF_FestivalStage.LIVE
            REPORT['checks'].append('실제 Live HUD 진입, 메뉴32/상태36')
            unreal.AutomationLibrary.finish_loading_before_screenshot()
            command('DisableAllScreenMessages')
            capture('01-live')
            advance('set_speed',2)
        elif stage=='set_speed':
            vm=subsystem(unreal.FF_FestivalViewModelSubsystem)
            vm.request_set_time_scale(2.)
            STATE['remaining']=vm.get_remaining_seconds()
            advance('speed',3)
        elif stage=='speed':
            hud=widget(unreal.FF_LiveHudScreen)
            vm=subsystem(unreal.FF_FestivalViewModelSubsystem)
            assert abs(vm.get_time_scale()-2.)<.01
            assert vm.get_remaining_seconds()<STATE['remaining']
            button=child(hud,'Button_Speed2')
            style=button.get_editor_property('widget_style')
            assert style.get_editor_property('normal').get_editor_property('outline_settings').get_editor_property('width')==2.
            REPORT['checks'].append('배속x2 원본 동기화 및 선택 테두리, 시간 흐름')
            hud.toggle_tab(unreal.FF_LiveTab.SNS)
            STATE['remaining']=vm.get_remaining_seconds()
            advance('panel',3)
        elif stage=='panel':
            assert widget(unreal.FF_LivePanelSNS),'SNS 탭 연결'
            assert subsystem(unreal.FF_FestivalViewModelSubsystem).get_remaining_seconds()<STATE['remaining']
            REPORT['checks'].append('SNS 패널 노출 중 Live 시간 유지')
            capture('02-live-sns')
            advance('set_finale',2)
        elif stage=='set_finale':
            hud=widget(unreal.FF_LiveHudScreen)
            hud.close_current_panel()
            subsystem(unreal.FF_FestivalViewModelSubsystem).request_set_time_scale(1.)
            subsystem(unreal.FF_FestivalStageSubsystem).request_transition(unreal.FF_FestivalStage.FINALE)
            advance('finale',2)
        elif stage=='finale':
            hud=widget(unreal.FF_LiveHudScreen)
            assert hud
            assert abs(hud.get_bottom_bar_height()-36)<.1,'탭 없는 단계에서 상태 바 높이 유지'
            expected=hud.get_cta_state()==unreal.FF_LiveCtaState.READY_FINALE_SKIP
            assert (child(hud,'Button_Cta').get_visibility()!=unreal.SlateVisibility.COLLAPSED)==expected
            REPORT['checks'].append('피날레 메뉴 행 숨김 및 상태36/기존 진행 버튼 규칙 유지')
            capture('03-finale')
            advance('stop',2)
        elif stage=='stop':
            LEVEL.editor_request_end_play()
            advance('done',3)
        elif stage=='done':
            if LEVEL.is_in_play_in_editor():return
            assert BEFORE=={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in PROTECTED},'검증 중 에셋/설정 변경'
            REPORT['protected_files_unchanged']=len(BEFORE)
            REPORT['status']='failed' if 'error' in REPORT else 'passed'
            REPORT['elapsed_seconds']=round(time.monotonic()-STATE['start'],2)
            (OUT/'report.json').write_text(json.dumps(REPORT,ensure_ascii=False,indent=2),encoding='utf-8')
            unreal.unregister_slate_post_tick_callback(HANDLE)
            unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    except Exception:
        REPORT['error']=traceback.format_exc()
        unreal.log_error(REPORT['error'])
        if STATE['stage']=='done':
            (OUT/'report.json').write_text(json.dumps(REPORT,ensure_ascii=False,indent=2),encoding='utf-8')
            unreal.unregister_slate_post_tick_callback(HANDLE)
            unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        else:advance('stop')
    finally: STATE['busy']=False

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
HANDLE=unreal.register_slate_post_tick_callback(tick)
