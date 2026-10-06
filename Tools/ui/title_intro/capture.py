"""타이틀 인트로 실제 PIE 캡처(v2: 아무 키 없이 자동으로 메뉴). 일회용 에디터에서 실행한다 (`run.ps1 -Mode Capture`). 에셋을 저장하지 않는다.

프론트엔드 레벨을 PIE 로 열어 인트로 단계마다 한 장씩 찍는다.
시작 화면 → 영상(우주·폭죽·변신·밝아짐) → 지구 공개 → (자동) 타이틀로 블렌드 중 → 타이틀.
단계는 벽시계가 아니라 화면의 `DescribeState()` 로 맞춘다 — 오프스크린 PIE 는 시작 시간이 들쭉날쭉하다.
보고서·PNG: Saved/TitleIntro/<output>/
"""
from pathlib import Path
import gc
import hashlib
import json
import re
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = ROOT / 'Saved/TitleIntro' / time.strftime('capture_%Y%m%d_%H%M%S')
OUT.mkdir(parents=True, exist_ok=True)
WIDTH, HEIGHT = 1600, 900
# (이름, 단계, 그 단계에 들어온 뒤 몇 초)
SHOTS = [('01_splash', 'Splash', 1.6), ('02_space', 'Video', 1.0), ('03_bursts', 'Video', 3.3),
         ('04_transform', 'Video', 5.9), ('05_sky', 'Video', 7.1), ('06_reveal', 'Reveal', 0.35), ('07_hold', 'Reveal', 1.4)]
AFTER_SKIP = [('08_to_title_mid', 0.7), ('09_title', 2.6)]

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
state = dict(stage='setup', next=time.monotonic() + 2, started=time.monotonic(), shot=0)
report = dict(status='running', shots=[], states=[])
handle = None


def hashes():
    paths = [ROOT / 'Content/FestaFiesta/Maps/Frontend.umap',
             ROOT / 'Content/FestaFiesta/UI/Title/Intro/WBP_FF_TitleIntro.uasset',
             ROOT / 'Content/FestaFiesta/UI/Title/GlassOrbit/WBP_TitleMain_MyCasual.uasset']
    return {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths if p.exists()}


def write():
    report['stage'] = state['stage']
    (OUT / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')


def advance(stage, delay=1.0):
    state.update(stage=stage, next=time.monotonic() + delay)
    write()


def intro_screen(world):
    controller = unreal.GameplayStatics.get_player_controller(world, 0)
    for screen in unreal.ObjectIterator(unreal.FF_TitleIntroScreen):
        if screen.get_owning_player() == controller and screen.is_activated():
            return screen
    return None


def phase_of(screen):
    text = screen.describe_state() if screen else ''
    match = re.match(r'Phase=(\w+) t=([\d.]+)', text)
    return (match.group(1), float(match.group(2)), text) if match else (None, 0.0, text)


def shoot(name):
    image = OUT / (name + '.png')
    unreal.SystemLibrary.execute_console_command(state['world'], 'Shot SHOWUI filename="' + image.as_posix() + '" -nosuffix')
    # 리그 상태는 C++ 전용이라 콘솔로 로그에 남긴다 (FF.Atlas.Camera).
    unreal.SystemLibrary.execute_console_command(state['world'], 'FF.Atlas.Camera')
    camera = unreal.GameplayStatics.get_player_camera_manager(state['world'], 0)
    report['shots'].append(dict(name=name, image=image.relative_to(ROOT).as_posix(),
                                wall=round(time.monotonic() - state['pie_started'], 2),
                                intro=phase_of(intro_screen(state['world']))[2],
                                camera_location=str(camera.get_camera_location()) if camera else None))


def cleanup(error=None):
    if error:
        report['error'] = error
        unreal.log_error('BRIGHT_GLOBE_FAIL TITLE_INTRO_CAPTURE ' + error)
    state.pop('world', None)
    gc.collect()
    if level.is_in_play_in_editor():
        level.editor_request_end_play()
    advance('final', 3)


def tick(delta):
    try:
        if time.monotonic() < state['next']:
            return
        state['next'] = time.monotonic() + 0.05
        assert time.monotonic() - state['started'] < 240, '캡처 제한 시간 초과'
        stage = state['stage']
        if stage == 'setup':
            assert not level.is_in_play_in_editor()
            report['hashes_before'] = hashes()
            assert level.load_level('/Game/FestaFiesta/Maps/Frontend')
            advance('prepare_window')
        elif stage == 'prepare_window':
            unreal.SystemLibrary.execute_console_command(level.get_world(), 'r.SetRes 4096x2160w')
            advance('begin', 2)
        elif stage == 'begin':
            # 실측 보정값은 capture_title_ui.py(P11) 와 같다.
            unreal.SystemLibrary.execute_console_command(level.get_world(), f'FF.FrontendBeginPIE {WIDTH - 2} {HEIGHT - 12}')
            advance('wait_pie', 0.5)
        elif stage == 'wait_pie':
            world = unreal.EditorLevelLibrary.get_game_world()
            if not world:
                return
            state['world'] = world
            state['pie_started'] = time.monotonic()
            unreal.SystemLibrary.execute_console_command(world, 'DisableAllScreenMessages')
            advance('shots', 0.0)
        elif stage == 'shots':
            screen = intro_screen(state['world'])
            phase, seconds, text = phase_of(screen)
            if not report['states'] or report['states'][-1]['phase'] != phase:
                report['states'].append(dict(phase=phase, wall=round(time.monotonic() - state['pie_started'], 2), text=text))
            assert screen or time.monotonic() - state['pie_started'] < 8, '인트로 화면이 뜨지 않았다'
            name, want_phase, at = SHOTS[state['shot']]
            order = ['Splash', 'Video', 'Reveal', 'Finished']
            if phase in order and order.index(phase) > order.index(want_phase):
                report['shots'].append(dict(name=name, skipped='단계를 지나쳤다 (' + text + ')'))
                state['shot'] += 1
            elif phase == want_phase and seconds >= at:
                shoot(name)
                state['shot'] += 1
            if state['shot'] >= len(SHOTS):
                advance('skip', 0.0)
        elif stage == 'skip':
            # 아무 키 없이 스스로 끝나기를 기다린다(지구를 RevealHoldSeconds 보여 준 뒤).
            screen = intro_screen(state['world'])
            phase = phase_of(screen)[0] if screen else 'Finished'
            if phase != 'Finished':
                assert time.monotonic() - state['pie_started'] < 40, '인트로가 스스로 끝나지 않았다'
                return
            report['states'].append(dict(phase='Finished', wall=round(time.monotonic() - state['pie_started'], 2)))
            state['skip_at'] = time.monotonic()
            state['after'] = 0
            advance('after_skip', 0.0)
        elif stage == 'after_skip':
            name, at = AFTER_SKIP[state['after']]
            if time.monotonic() - state['skip_at'] >= at:
                shoot(name)
                state['after'] += 1
                if state['after'] >= len(AFTER_SKIP):
                    # 흐름 상태는 콘솔로 로그에 남긴다(FF.Flow.Status). 서브시스템 조회 함수는 Python 에 노출되지 않는다.
                    unreal.SystemLibrary.execute_console_command(state['world'], 'FF.Flow.Status')
                    report['intro_screen_after'] = bool(intro_screen(state['world']))
                    advance('end', 2.0)
        elif stage == 'end':
            state.pop('world', None)
            gc.collect()
            level.editor_request_end_play()
            advance('final', 3)
        elif stage == 'final':
            if level.is_in_play_in_editor():
                return
            report['hashes_after'] = hashes()
            report['assets_unchanged'] = report.get('hashes_before') == report['hashes_after']
            report['missing_images'] = [s['name'] for s in report['shots'] if 'image' in s and not (ROOT / s['image']).exists()]
            report['elapsed_seconds'] = round(time.monotonic() - state['started'], 1)
            report['status'] = 'failed' if 'error' in report else 'captured'
            write()
            unreal.unregister_slate_post_tick_callback(handle)
            gc.collect()
            unreal.EditorPythonScripting.set_keep_python_script_alive(False)
            unreal.SystemLibrary.quit_editor()
    except Exception:
        cleanup(traceback.format_exc())


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle = unreal.register_slate_post_tick_callback(tick)
