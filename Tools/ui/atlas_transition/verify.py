"""실제 Frontend PIE의 지구본↔국가 지도 페이드를 검사한다.

빌드가 끝난 에디터에서 실행한다. 에디터를 실행하거나 에셋/설정을 저장하지 않는다.
기본 속도 왕복은 캡처 없이 검사하고, 별도 왕복에서만 메모리의 시간을 늘려
SHOWUI PNG를 남긴다. 완료/실패 모두 설정 복원, PIE 종료, 콜백 해제를 수행한다.
결과: Saved/AtlasTransition/<UTC 실행시각>/report.json 및 같은 폴더의 PNG.
물리 마우스/키보드/게임패드 조작이나 사람이 판단하는 미감 검사는 포함하지 않는다.
"""

from datetime import datetime, timezone
from pathlib import Path
import gc
import hashlib
import json
import math
import struct
import time
import traceback

import unreal


ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
BASE = ROOT / 'Saved/AtlasTransition'
SETTINGS_FIELDS = (
    'enable_atlas_transition',
    'atlas_transition_fade_out_duration',
    'atlas_transition_opaque_hold_duration',
    'atlas_transition_fade_in_duration',
)
GLOBE = unreal.FF_UIFlowScreen.ATLAS_GLOBE
COUNTRY = unreal.FF_UIFlowScreen.ATLAS_COUNTRY
TITLE = unreal.FF_UIFlowScreen.TITLE
EPSILON = 0.001


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def protected_hashes():
    paths = [ROOT / 'Content/FestaFiesta/Maps/Frontend.umap',
             ROOT / 'Content/FestaFiesta/UI/WBP_FF_UIRootLayout.uasset',
             ROOT / 'Content/FestaFiesta/UI/Title/GlassOrbit/WBP_TitleMain_MyCasual.uasset']
    paths += sorted((ROOT / 'Config').glob('*.ini'))
    for folder in ('Content/FestaFiesta/UI/Core', 'Content/FestaFiesta/UI/Atlas',
                   'Content/FestaFiesta/UI/Frontend', 'Content/FestaFiesta/UI/Lv_Test'):
        paths += sorted((ROOT / folder).rglob('*.uasset'))
    return {path.relative_to(ROOT).as_posix(): digest(path) for path in paths if path.is_file()}


class AtlasTransitionVerifier:
    def __init__(self):
        self.started = time.monotonic()
        self.out = BASE / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S_%fZ')
        self.out.mkdir(parents=True, exist_ok=False)
        self.level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        self.handle = None
        self.busy = False
        self.owns_pie = False
        self.cleaning = False
        self.finished = False
        self.wait = None
        self.trace = None
        self.settings = None
        self.original_settings = {}
        self.world = self.flow = self.layout = self.globe = self.rig = self.controller = None
        self.report = dict(status='running', started_at=datetime.now(timezone.utc).isoformat(),
                           scope='실제 Frontend PIE의 GoToScreen/GoBack과 렌더링되는 루트 페이드',
                           physical_mouse_keyboard_gamepad_tested=False,
                           synthetic_pointer_tested=False, visual_quality_verified=False,
                           asset_or_config_save_requested=False, transitions=[], images=[],
                           checks=[], cleanup_errors=[], output=str(self.out))
        self.scenario = self.run()

    def write_report(self):
        self.report['elapsed_seconds'] = time.monotonic() - self.started
        destination = self.out / 'report.json'
        pending = destination.with_suffix('.writing.json')
        pending.write_text(json.dumps(self.report, ensure_ascii=False, indent=2), encoding='utf-8')
        pending.replace(destination)

    def checked(self, name, **evidence):
        self.report['checks'].append(dict(name=name, passed=True, **evidence))
        self.write_report()

    def until(self, label, predicate, timeout=30.0):
        return dict(label=label, predicate=predicate, deadline=time.monotonic() + timeout)

    def delay(self, label, seconds):
        ready = time.monotonic() + seconds
        return self.until(label, lambda: time.monotonic() >= ready, seconds + 15.0)

    def command(self, command):
        unreal.SystemLibrary.execute_console_command(self.world or self.level.get_world(), command)

    def snapshot(self):
        camera = self.controller.get_editor_property('player_camera_manager')
        position = camera.get_camera_location()
        angle = camera.get_camera_rotation()
        active = self.layout.get_editor_property('ScreenStack').get_active_widget()
        return dict(elapsed=time.monotonic() - self.started,
                    screen=str(self.flow.get_frontend_screen()),
                    transition_active=bool(self.layout.is_atlas_transition_active()),
                    opacity=float(self.layout.get_atlas_transition_opacity()),
                    stack_transitioning=bool(self.layout.is_screen_stack_transitioning()),
                    active_widget=active.get_class().get_name() if active else None,
                    paused=bool(unreal.GameplayStatics.is_game_paused(self.world)),
                    world_seconds=float(unreal.GameplayStatics.get_time_seconds(self.world)),
                    camera_moving=bool(self.rig.is_moving()),
                    camera_position=[position.x, position.y, position.z],
                    camera_rotation=[angle.pitch, angle.yaw, angle.roll])

    def observe(self):
        if self.trace is None:
            return
        row = self.snapshot()
        self.trace['samples'].append(row)
        assert math.isfinite(row['opacity']) and -EPSILON <= row['opacity'] <= 1 + EPSILON, '페이드 알파 범위 오류'
        assert not row['paused'], 'Atlas 전환이 월드를 일시정지함'
        source, target = self.trace['source'], self.trace['target']
        assert row['screen'] in (source, target), '진행 중 중복 요청이 다른 화면을 열었음'
        if row['opacity'] >= 1 - EPSILON:
            self.trace['opaque_observed'] = True
        if row['screen'] == source and row['opacity'] < 1 - EPSILON:
            first = self.trace['samples'][0]
            assert math.dist(first['camera_position'], row['camera_position']) < 0.05, '가려지기 전에 원본 카메라가 움직임'
        if row['screen'] != self.trace['last_screen']:
            assert row['screen'] == target, '진행 중 화면이 다시 되돌아감'
            assert row['opacity'] >= 1 - EPSILON, '완전히 가리기 전에 화면/카메라 교체가 일어남'
            self.trace['screen_changes'].append(row)
            self.trace['last_screen'] = row['screen']
        if row['screen'] == target and row['opacity'] < 1 - EPSILON:
            assert self.trace['opaque_observed'], '불투명 프레임 없이 새 화면이 드러남'
            assert not row['stack_transitioning'], 'CommonUI 스택 전환이 끝나기 전에 페이드 인됨'

    def settled(self):
        return (not self.layout.is_atlas_transition_active()
                and not self.layout.is_screen_stack_transitioning())

    def active_screen(self, cls):
        found = [obj for obj in unreal.ObjectIterator(cls)
                 if obj.get_owning_player() == self.controller and obj.is_activated()]
        assert len(found) == 1, f'{cls.__name__} 활성 인스턴스 수: {len(found)}'
        return found[0]

    def ready_pie(self):
        if not self.level.is_in_play_in_editor():
            return False
        world = unreal.EditorLevelLibrary.get_game_world()
        if not world:
            return False
        instance = unreal.GameplayStatics.get_game_instance(world)
        flows = [obj for obj in unreal.ObjectIterator(unreal.FF_UIFlowSubsystem) if obj.get_outer() == instance]
        if len(flows) != 1 or flows[0].get_frontend_screen() != TITLE:
            return False
        self.world, self.flow = world, flows[0]
        self.layout = self.flow.get_root_layout()
        if self.layout is None:
            return False
        self.controller = unreal.GameplayStatics.get_player_controller(world, 0)
        globes = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.FF_AtlasGlobeActor)
        rigs = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.FF_AtlasCameraRig)
        assert len(globes) == len(rigs) == 1, '실제 Frontend 지구본/카메라 리그 누락'
        self.globe, self.rig = globes[0], rigs[0]
        for api in ('is_atlas_transition_active', 'get_atlas_transition_opacity', 'is_screen_stack_transitioning'):
            assert hasattr(self.layout, api), '새 DLL의 루트 API가 없음: ' + api
        return True

    def set_temporary_settings(self, values):
        # UE 5.7 Python의 NEVER로 SettingsChanged/PostEditChange 저장 경로도 건너뛴다.
        for key, value in values.items():
            self.settings.set_editor_property(key, value, notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)

    def restore_settings(self):
        if self.settings is not None and self.original_settings:
            self.set_temporary_settings(self.original_settings)
            restored = {key: self.settings.get_editor_property(key) for key in self.original_settings}
            assert restored == self.original_settings, '메모리 설정 복원 실패'
            self.report['settings_restored'] = True

    def shot(self, name):
        path = self.out / (name + '.png')
        assert not path.exists(), '이전 캡처 덮어쓰기 금지'
        row = dict(name=name, path=str(path), requested_state=self.snapshot())
        self.report['images'].append(row)
        self.command('Shot SHOWUI filename="' + path.as_posix() + '" -nosuffix')
        return row

    def start_transition(self, label, target, reverse=False, capture=False):
        source = self.flow.get_frontend_screen()
        assert source != target and self.settled(), '전환 시작 전에 안정된 원본 화면이 필요함'
        self.trace = dict(name=label, source=str(source), target=str(target), capture_pass=capture,
                          samples=[], screen_changes=[], last_screen=str(source), opaque_observed=False,
                          settings={key: self.settings.get_editor_property(key) for key in SETTINGS_FIELDS})
        self.report['transitions'].append(self.trace)
        if reverse:
            assert self.flow.go_back(), '국가 지도에서 단일 뒤로 요청 실패'
        else:
            self.flow.go_to_screen(target)
        assert self.flow.get_frontend_screen() == source, 'GoToScreen/GoBack 호출과 동시에 화면을 바꿈'
        assert self.layout.is_atlas_transition_active(), 'Atlas 왕복에서 페이드가 시작되지 않음'
        self.observe()
        # 흐름 API로 직접 들어오는 빠른 재요청도 중복 이력/예약을 만들면 안 된다.
        for _ in range(3):
            self.flow.go_to_screen(COUNTRY)
            assert self.flow.go_back(), '전환 중 뒤로 입력이 소비되지 않음'
        self.observe()
        self.trace['duplicate_forward_and_back_requests'] = 3
        self.write_report()

    def finish_transition(self):
        self.observe()
        trace, final = self.trace, self.trace['samples'][-1]
        assert final['screen'] == trace['target'], '전환 완료 후 목적지 불일치'
        assert not final['transition_active'] and final['opacity'] <= EPSILON, '검은 화면/입력 차단이 남음'
        assert len(trace['screen_changes']) == 1, '왕복 하나에 화면 이력이 여러 번 바뀜'
        assert trace['opaque_observed'], '완전 불투명 상태를 관측하지 못함'
        assert final['world_seconds'] > trace['samples'][0]['world_seconds'], '전환 중 월드 시계가 진행되지 않음'
        trace['passed'] = True
        trace['sample_count'] = len(trace['samples'])
        trace['fade_out_partial_frames'] = sum(trace['source'] == s['screen'] and EPSILON < s['opacity'] < 1 - EPSILON for s in trace['samples'])
        trace['fade_in_partial_frames'] = sum(trace['target'] == s['screen'] and EPSILON < s['opacity'] < 1 - EPSILON for s in trace['samples'])
        if trace['capture_pass']:
            assert trace['fade_out_partial_frames'] > 0 and trace['fade_in_partial_frames'] > 0, '캡처 왕복의 중간 알파 프레임 누락'
        self.trace = None
        self.write_report()

    def transition(self, label, target, reverse=False, capture=False):
        self.start_transition(label, target, reverse, capture)
        if capture:
            yield self.until(label + ': 어두워지는 중', lambda: 0.3 <= self.layout.get_atlas_transition_opacity() <= 0.8)
            self.shot(label + '_01_fade_out')
            yield self.until(label + ': 완전 불투명', lambda: self.layout.get_atlas_transition_opacity() >= 1 - EPSILON)
            self.shot(label + '_02_opaque')
            yield self.until(label + ': 새 화면 드러남', lambda: self.flow.get_frontend_screen() == target
                             and 0.3 <= self.layout.get_atlas_transition_opacity() <= 0.8)
            self.shot(label + '_03_fade_in')
        yield self.until(label + ': 전환 완료', self.settled, 45.0)
        self.finish_transition()
        yield self.delay(label + ': 늦게 실행되는 중복 예약 감시', 0.75)
        assert self.flow.get_frontend_screen() == target and self.settled(), '중복 요청이 완료 후 다시 실행됨'
        assert self.layout.get_atlas_transition_opacity() <= EPSILON, '전환 완료 후 검은 막 재등장'
        if capture:
            self.shot(label + '_04_complete')
            yield self.delay(label + ': 캡처 저장', 0.5)

    def run(self):
        assert not self.level.is_in_play_in_editor(), '기존 PIE 실행 중: 기존 세션은 종료하지 않음'
        self.report['hashes_before'] = protected_hashes()
        self.settings = unreal.get_default_object(unreal.FF_UIFlowSettings)
        self.original_settings = {key: self.settings.get_editor_property(key) for key in SETTINGS_FIELDS}
        self.report['original_settings'] = dict(self.original_settings)
        assert self.original_settings['enable_atlas_transition'], '기본 설정에서 Atlas 전환이 꺼져 있음'
        assert self.original_settings['atlas_transition_fade_out_duration'] > 0, '기본 페이드 아웃 시간이 0임'
        assert self.original_settings['atlas_transition_fade_in_duration'] > 0, '기본 페이드 인 시간이 0임'
        assert self.level.load_level('/Game/FestaFiesta/Maps/Frontend')
        yield self.delay('Frontend 로드', 1.0)
        self.command('r.SetRes 4096x2160w')
        self.command('FF.FrontendBeginPIE 1598 888')
        self.owns_pie = True
        yield self.until('Frontend PIE 타이틀 준비', self.ready_pie, 60.0)
        yield self.until('타이틀 스택 안정화', self.settled)
        assert self.layout.get_atlas_transition_opacity() <= EPSILON, '최초 타이틀에서 검은 막이 보임'
        self.checked('최초 타이틀에 Atlas 페이드 없음', snapshot=self.snapshot())
        # 긴 로딩/셰이더 정리는 관측 구간 밖에서 한 번만 실행한다.
        unreal.AutomationLibrary.finish_loading_before_screenshot()
        self.command('DisableAllScreenMessages')
        self.flow.go_to_screen(GLOBE)
        assert not self.layout.is_atlas_transition_active(), '타이틀→지구본에 Atlas 왕복 페이드가 적용됨'
        yield self.until('최초 지구본/카메라 안정화', lambda: self.settled() and not self.rig.is_moving())
        assert self.flow.get_frontend_screen() == GLOBE
        self.checked('최초 지구본 진입에 Atlas 페이드 없음', snapshot=self.snapshot())
        self.active_screen(unreal.FF_AtlasGlobeScreen).inspect_country('KR')
        self.flow.set_active_country(unreal.Name('KR'))
        yield self.until('대한민국 선택/카메라 안정화', lambda: not self.globe.is_transitioning() and not self.rig.is_moving()
                         and not self.rig.is_country_inspection_moving())
        assert str(self.globe.get_selected_country_id()) == 'KR', '대한민국 선택 실패'
        self.checked('대한민국 선택', snapshot=self.snapshot())

        yield from self.transition('default_forward', COUNTRY)
        self.active_screen(unreal.FF_AtlasCountryScreen)
        yield from self.transition('default_back', GLOBE, reverse=True)
        self.active_screen(unreal.FF_AtlasGlobeScreen)
        self.checked('기본 속도: 뒤로 한 번에 지구본 복귀, 중복 이력 없음')

        self.set_temporary_settings(dict(atlas_transition_fade_out_duration=3.0,
                                         atlas_transition_opaque_hold_duration=2.0,
                                         atlas_transition_fade_in_duration=3.0))
        self.report['capture_note'] = 'PNG는 메모리 시간만 늘린 별도 왕복. 기본 속도 검증에는 캡처 요청/로딩 대기가 없음.'
        self.active_screen(unreal.FF_AtlasGlobeScreen).inspect_country('KR')
        yield self.until('캡처 전 대한민국 재선택 안정화', lambda: not self.globe.is_transitioning() and not self.rig.is_moving()
                         and not self.rig.is_country_inspection_moving())
        self.shot('capture_00_globe')
        yield self.delay('기준 화면 PNG 저장', 1.0)
        yield from self.transition('capture_forward', COUNTRY, capture=True)
        self.active_screen(unreal.FF_AtlasCountryScreen)
        yield from self.transition('capture_back', GLOBE, reverse=True, capture=True)
        self.active_screen(unreal.FF_AtlasGlobeScreen)
        self.restore_settings()

        # 같은 히스토리에서 다음 뒤로가 타이틀에 닿아야 Country 중복 push가 없음을 증명한다.
        assert self.flow.go_back(), '왕복 이후 일반 뒤로가 동작하지 않음'
        assert self.flow.get_frontend_screen() == TITLE, '왕복 후 이력에 중복 화면이 남음'
        assert not self.layout.is_atlas_transition_active(), '지구본→타이틀에 Atlas 페이드가 적용됨'
        yield self.until('최종 타이틀 스택/카메라 안정화', lambda: self.settled() and not self.rig.is_moving())
        self.active_screen(unreal.FF_TitleScreen)
        self.checked('페이드 종료 후 일반 내비게이션 정상, 중복 히스토리 없음', snapshot=self.snapshot())
        # 호출 API만 정상이어도 Slate 입력 전처리기가 남아 있을 수 있으므로 기존
        # 프로젝트 포인터 도구로 실제 Slate 라우팅 경로를 한 번 더 확인한다.
        self.command('FF.FrontendPointer Btn_Play Click')
        yield self.until('페이드 종료 후 Slate 포인터 클릭', lambda: self.flow.get_frontend_screen()
                         == unreal.FF_UIFlowScreen.PLAY_HUB and self.settled(), 15.0)
        self.active_screen(unreal.FF_PlayHubScreen)
        self.report['synthetic_pointer_tested'] = True
        self.checked('페이드 종료 후 Slate 클릭으로 플레이 허브 진입: 입력 차단 해제', snapshot=self.snapshot())
        yield self.until('모든 PNG 저장', lambda: all(Path(image['path']).exists() for image in self.report['images']))
        for image in self.report['images']:
            path = Path(image['path'])
            header = path.read_bytes()[:24]
            assert header[:8] == b'\x89PNG\r\n\x1a\n', '유효한 SHOWUI PNG가 아님: ' + str(path)
            image.update(png_size=list(struct.unpack('>II', header[16:24])), bytes=path.stat().st_size,
                         sha256=digest(path))
        assert len(self.report['transitions']) == 4 and all(t.get('passed') for t in self.report['transitions'])

    def begin_cleanup(self, error=None):
        if error:
            self.report.setdefault('error', error)
            unreal.log_error('ATLAS_TRANSITION_FAIL ' + error)
        self.cleaning = True
        self.trace = None
        self.cleanup_started = time.monotonic()
        try:
            self.restore_settings()
        except Exception:
            self.report['cleanup_errors'].append(traceback.format_exc())
        self.scenario.close()
        self.world = self.flow = self.layout = self.globe = self.rig = self.controller = None
        gc.collect()
        if self.owns_pie:
            self.level.editor_request_end_play()
        self.write_report()

    def finish(self):
        try:
            self.report['pie_ended'] = not self.level.is_in_play_in_editor() if self.owns_pie else None
            if self.owns_pie:
                assert self.report['pie_ended'], 'PIE 종료 제한 시간 초과'
            self.report['hashes_after'] = protected_hashes()
            if 'hashes_before' in self.report:
                self.report['assets_and_config_unchanged'] = self.report['hashes_before'] == self.report['hashes_after']
                assert self.report['assets_and_config_unchanged'], '검사 중 에셋 또는 프로젝트 설정 파일 변경'
        except Exception:
            self.report['cleanup_errors'].append(traceback.format_exc())
        finally:
            self.report['status'] = 'failed' if 'error' in self.report or self.report['cleanup_errors'] else 'passed'
            self.report['finished_at'] = datetime.now(timezone.utc).isoformat()
            self.write_report()
            (BASE / 'latest.json').write_text(json.dumps(dict(status=self.report['status'], report=str(self.out / 'report.json')),
                                                       ensure_ascii=False, indent=2), encoding='utf-8')
            self.finished = True
            if self.handle is not None:
                unreal.unregister_slate_post_tick_callback(self.handle)
                self.handle = None
            unreal.EditorPythonScripting.set_keep_python_script_alive(False)
            unreal.log('ATLAS_TRANSITION_' + self.report['status'].upper() + ' ' + str(self.out / 'report.json'))

    def tick(self, delta):
        if self.busy or self.finished:
            return
        self.busy = True
        try:
            if self.cleaning:
                if not self.owns_pie or not self.level.is_in_play_in_editor() or time.monotonic() - self.cleanup_started > 30:
                    self.finish()
                return
            assert time.monotonic() - self.started < 240, 'Atlas 전환 검사 전체 240초 제한 초과'
            self.observe()
            if self.wait is not None:
                assert time.monotonic() <= self.wait['deadline'], '대기 시간 초과: ' + self.wait['label']
                if not self.wait['predicate']():
                    return
            self.wait = next(self.scenario)
            self.report['last_stage'] = self.wait['label']
            self.write_report()
        except StopIteration:
            self.begin_cleanup()
        except Exception:
            if self.cleaning:
                self.report['cleanup_errors'].append(traceback.format_exc())
                self.finish()
            else:
                self.begin_cleanup(traceback.format_exc())
        finally:
            self.busy = False


previous = getattr(unreal, '_ff_atlas_transition_verifier', None)
assert previous is None or previous.finished, '이미 Atlas 전환 검사가 실행 중이다'
verifier = AtlasTransitionVerifier()
unreal._ff_atlas_transition_verifier = verifier
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
verifier.handle = unreal.register_slate_post_tick_callback(verifier.tick)
unreal.log('ATLAS_TRANSITION_BEGIN ' + str(verifier.out))
