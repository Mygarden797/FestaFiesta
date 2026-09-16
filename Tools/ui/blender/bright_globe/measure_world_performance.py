"""P09 같은 Frontend PIE에서 구/신 육지·바다만 바꾸는 CSV 성능 비교.

에셋 저장 없음. 고정 타이틀 구도, 같은 UI·조명·랜드마크·실제 뷰포트에서
old/new/new/old 순서로 각각 10초를 기록한다. 출시 PC/콘솔 성능 검사가 아니다.
설치 UE 5.7 CSV의 FrameTime/GPUTime을 사용하며 Slate callback 간격은 측정하지 않는다.
"""
from datetime import datetime, timezone
from pathlib import Path
import csv
import gc
import hashlib
import io
import json
import math
import re
import statistics
import time
import traceback

import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = ROOT / 'Saved/BrightGlobe'
REPORT = OUT / 'p09_world_performance.json'
CSV_DIR = ROOT / 'Saved/Profiling/CSV'
RUN_ID = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%f')
ORDER = ('old', 'new', 'new', 'old')
WINDOW_SECONDS = 10.
WARMUP_SECONDS = 12.
METRICS = ('FrameTime', 'GPUTime', 'GameThreadTime', 'RenderThreadTime', 'RHIThreadTime')
CVARS = {'r.VSync': 0, 'r.VSyncEditor': 0, 't.MaxFPS': 0,
         't.IdleWhenNotForeground': 0, 'Slate.bAllowThrottling': 0,
         'r.DynamicRes.OperationMode': 0, 'r.ScreenPercentage': 100,
         'csv.CompressionMode': 0}
QUALITY = ('sg.ViewDistanceQuality', 'sg.AntiAliasingQuality', 'sg.ShadowQuality',
           'sg.PostProcessQuality', 'sg.TextureQuality', 'sg.EffectsQuality',
           'sg.FoliageQuality', 'sg.ShadingQuality', 'sg.GlobalIlluminationQuality',
           'sg.ReflectionQuality')
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
handle = None
state = {'stage': 'preflight', 'started': time.monotonic(), 'next': time.monotonic() + 2,
         'pie_requested': False, 'capturing': False, 'round_index': 0}
report = {'status': 'running', 'run_id': RUN_ID, 'asset_save_requested': False,
          'scope': '같은 Frontend PIE 타이틀에서 육지 16개 메시/재질과 바다 재질만 교체',
          'limits': ['에디터·오프스크린 환경의 상대 비교이며 출시 성능 또는 콘솔 성능 보장이 아님',
                     'UI·조명·배경·랜드마크는 현재 버전을 양쪽에 공통 사용',
                     '메시 원본 삼각형 합은 GPU가 실제 그린 삼각형 수가 아님',
                     '두 에셋 세트를 함께 미리 로드하므로 메모리 증감 비교에는 사용하지 않음'],
          'order': list(ORDER), 'window_seconds': WINDOW_SECONDS,
          'warmup_seconds_per_round': WARMUP_SECONDS, 'rounds': [], 'cleanup_errors': [],
          'process_exit_code': None}


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def write_report():
    report['last_stage'] = state.get('stage')
    pending = REPORT.with_suffix('.writing.json')
    pending.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    pending.replace(REPORT)


def advance(stage, delay=.25):
    state.update(stage=stage, next=time.monotonic() + delay)
    write_report()


def command(text):
    unreal.SystemLibrary.execute_console_command(state['world'], text)


def fields(text, keys):
    return [float(re.search(r'\b' + key + r':\s*(-?[\d.]+)', text).group(1)) for key in keys]


def camera_evidence():
    controller = state['controller']
    manager = controller.get_editor_property('player_camera_manager')
    return {'viewport': list(controller.get_viewport_size()),
            'view_target': controller.get_view_target().get_path_name(),
            'location': manager.get_camera_location().export_text(),
            'rotation': manager.get_camera_rotation().export_text(),
            'fov': float(manager.get_fov_angle()),
            'globe_root': state['root'].get_world_transform().export_text(),
            'cvars': {name: unreal.SystemLibrary.get_console_variable_float_value(name)
                      for name in (*CVARS, *QUALITY)}}


def switch_visuals(variant):
    for comp, mesh in zip(state['layers'], state[variant + '_meshes']):
        if comp.get_editor_property('static_mesh') != mesh:
            assert comp.set_static_mesh(mesh), comp.get_name()
        assert comp.get_editor_property('static_mesh') == mesh, comp.get_name()
        # 이전 오버라이드를 제거하고 해당 메시의 원래 재질을 사용한다.
        comp.set_editor_property('override_materials', [])
        assert comp.get_material(0) == mesh.get_material(0), comp.get_name()
    state['ocean'].set_material(0, state[variant + '_water'])
    # 스크린샷을 요청하지 않는다. 이 함수는 shader/asset compilation과 streaming을 완료한다.
    unreal.AutomationLibrary.finish_loading_before_screenshot()


def percentile(values, fraction):
    values = sorted(values)
    point = (len(values) - 1) * fraction
    low = int(point)
    high = min(low + 1, len(values) - 1)
    return values[low] + (values[high] - values[low]) * (point - low)


def summarize(values):
    return {'samples': len(values), 'mean_ms': statistics.fmean(values),
            'median_ms': statistics.median(values), 'p95_ms': percentile(values, .95),
            'min_ms': min(values), 'max_ms': max(values)}


def read_finished_csv(path):
    if not path.exists():
        return None
    raw = path.read_text(encoding='utf-8-sig')
    if '[HasHeaderRowAtEnd]' not in raw or '[EndTimestamp]' not in raw:
        return None
    rows = list(csv.reader(io.StringIO(raw)))
    headers = [(i, row) for i, row in enumerate(rows) if row and row[0] == 'EVENTS']
    assert headers, 'CSV 최종 헤더가 없음'
    end_index, header = headers[-1]
    # CSV는 수집 중 열이 늘 수 있으므로 맨 뒤의 완성된 헤더를 사용한다.
    series = {name: [] for name in METRICS if name in header}
    assert 'FrameTime' in series, '엔진 CSV FrameTime 열 없음'
    data_rows = [row for row in rows[:end_index]
                 if row and row[0] != 'EVENTS' and not row[0].startswith('[')]
    # 시작/정지 명령이 걸치는 양끝 5프레임을 제외한다.
    assert len(data_rows) >= 120, '유효 CSV 프레임이 120개 미만'
    for row in data_rows[5:-5]:
        for name in series:
            index = header.index(name)
            value = float(row[index]) if index < len(row) and row[index] else 0.
            assert math.isfinite(value), name
            series[name].append(value)
    return {'raw_rows': len(data_rows), 'trim_each_end': 5,
            'metrics': {name: summarize(values) for name, values in series.items()},
            'gpu_available': ('GPUTime' in series and
                              sum(v > 0 for v in series['GPUTime']) / len(series['GPUTime']) > .95),
            'series': series}


def build_comparison():
    grouped = {variant: {} for variant in ('old', 'new')}
    for row in report['rounds']:
        for name, values in row.pop('_series').items():
            grouped[row['variant']].setdefault(name, []).extend(values)
    report['summary'] = {variant: {name: summarize(values) for name, values in group.items()}
                         for variant, group in grouped.items()}
    report['delta_new_vs_old_percent'] = {
        name: {key: (report['summary']['new'][name][key] / report['summary']['old'][name][key] - 1) * 100
               for key in ('mean_ms', 'median_ms', 'p95_ms')
               if report['summary']['old'][name][key] > 0}
        for name in grouped['old'].keys() & grouped['new'].keys()}
    report['gpu_available'] = all(row['gpu_available'] for row in report['rounds'])
    if not report['gpu_available']:
        report['limits'].append('GPUTime이 없거나 0인 프레임이 많아 GPU 상대 성능 판정 불가')
        report['delta_new_vs_old_percent'].pop('GPUTime', None)
    report['repeat_median_spread_percent'] = {}
    for variant in ('old', 'new'):
        runs = [row for row in report['rounds'] if row['variant'] == variant]
        report['repeat_median_spread_percent'][variant] = {
            name: (max(values) / min(values) - 1) * 100
            for name in METRICS
            if all(name in row['metrics'] for row in runs)
            for values in [[row['metrics'][name]['median_ms'] for row in runs]]
            if min(values) > 0}


def start_cleanup(error=None):
    if error:
        report.setdefault('error', error)
        unreal.log_error('BRIGHT_GLOBE_FAIL ' + error)
    state['cleanup_started'] = time.monotonic()
    if state.get('capturing'):
        command('CsvProfile STOP')
        state['capturing'] = False
    advance('cleanup', 2.)


def finish():
    global handle
    report['pie_ended'] = not level.is_in_play_in_editor()
    if not report['pie_ended']:
        report.setdefault('error', 'PIE 종료 확인 실패')
    after = {rel: digest(ROOT / rel) for rel in report.get('file_hashes_before', {})}
    report['assets_unchanged'] = after == report.get('file_hashes_before', {})
    if not report['assets_unchanged']:
        report.setdefault('error', '측정 중 저장된 에셋 바이트가 달라짐')
    report['file_hashes_after'] = after
    report['status'] = 'failed' if 'error' in report else 'measured'
    report['elapsed_seconds'] = time.monotonic() - state['started']
    write_report()
    unreal.unregister_slate_post_tick_callback(handle)
    handle = None
    state.clear()
    gc.collect()
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)


def tick(delta_seconds):
    try:
        now = time.monotonic()
        if now < state['next']:
            return
        stage = state['stage']
        state['next'] = now + .25
        if stage not in ('cleanup', 'wait_end', 'settle_end') and now - state['started'] > 300:
            raise TimeoutError('성능 비교 300초 초과')
        if stage == 'preflight':
            assert not level.is_in_play_in_editor(), '기존 PIE가 실행 중'
            CSV_DIR.mkdir(parents=True, exist_ok=True)
            baseline = json.loads((OUT / 'baseline/report.json').read_text(encoding='utf-8'))
            integration = json.loads((OUT / 'p06_frontend_integration.json').read_text(encoding='utf-8-sig'))
            imported = json.loads((OUT / 'p05_ue_import.json').read_text(encoding='utf-8'))
            assert integration['status'] == imported['status'] == 'passed'
            report['file_hashes_before'] = {row['file']: digest(ROOT / row['file'])
                                            for row in integration['file_hashes_after']}
            report['source_triangle_counts'] = {'old': baseline['source_triangles'],
                                                'new': sum(row['triangles'] for row in imported['meshes'])}
            report['engine_version'] = unreal.SystemLibrary.get_engine_version()
            old = next(actor for actor in baseline['actors'] if actor['class'] == 'FF_AtlasGlobeActor')
            state['old_meshes'] = [unreal.load_asset(path) for path in old['settings']['land_layer_meshes']]
            state['old_water'] = unreal.load_asset(old['settings']['globe_material'])
            assert len(state['old_meshes']) == 16 and all(state['old_meshes']) and state['old_water']
            state['base_shot'] = baseline['shots'][0]
            assert level.load_level('/Game/FestaFiesta/Maps/Frontend')
            state['pie_requested'] = True
            level.editor_request_begin_play()
            advance('wait_pie', 1.)
        elif stage == 'wait_pie':
            if not level.is_in_play_in_editor():
                return
            world = unreal.EditorLevelLibrary.get_game_world()
            if not world:
                return
            controller = unreal.GameplayStatics.get_player_controller(world, 0)
            globes = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.FF_AtlasGlobeActor)
            rigs = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.FF_AtlasCameraRig)
            if not controller or len(globes) != 1 or len(rigs) != 1:
                return
            globe, rig = globes[0], rigs[0]
            rig.skip_camera_move()
            globe.set_auto_spin_paused(True)
            globe.set_actor_tick_enabled(False)
            rig.set_actor_tick_enabled(False)
            root = globe.get_editor_property('globe_root')
            base = state['base_shot']
            root.set_relative_rotation(unreal.Rotator(**dict(zip(('pitch', 'yaw', 'roll'),
                fields(base['root_rotation'], ('pitch', 'yaw', 'roll'))))), False, False)
            cam = rig.get_editor_property('title_camera')
            assert cam
            controller.set_view_target_with_blend(cam, 0.)
            layers = list(globe.get_editor_property('land_layer_components'))
            assert len(layers) == 16
            state.update(world=world, controller=controller, globe=globe, root=root, rig=rig,
                         layers=layers, ocean=globe.get_editor_property('globe_mesh'))
            state['new_meshes'] = [comp.get_editor_property('static_mesh') for comp in layers]
            assert all('/BrightGlobe_v1/P05/' in mesh.get_path_name() for mesh in state['new_meshes'])
            state['new_water'] = state['ocean'].get_material(0)
            state['cvars_before'] = {name: unreal.SystemLibrary.get_console_variable_float_value(name)
                                     for name in CVARS}
            report['cvars_before'] = state['cvars_before']
            for name, value in CVARS.items():
                command(f'{name} {value}')
            # 양쪽 에셋을 미리 로드하고 첫 샘플 전에 컴파일/스트리밍을 기다린다.
            unreal.AutomationLibrary.finish_loading_before_screenshot()
            report['assets'] = {variant: {'meshes': [m.get_path_name() for m in state[variant + '_meshes']],
                                          'water': state[variant + '_water'].get_path_name()}
                                for variant in ('old', 'new')}
            advance('prepare_round', 2.)
        elif stage == 'prepare_round':
            index = state['round_index']
            variant = ORDER[index]
            switch_visuals(variant)
            state['csv_path'] = CSV_DIR / f'p09_{RUN_ID}_{index + 1:02d}_{variant}.csv'
            assert not state['csv_path'].exists()
            state['round'] = {'index': index + 1, 'variant': variant,
                              'csv': str(state['csv_path'])}
            advance('capture_start', WARMUP_SECONDS)
        elif stage == 'capture_start':
            evidence = camera_evidence()
            assert min(evidence['viewport']) > 0, 'PIE 실제 뷰포트가 없음'
            if 'common_conditions' not in report:
                report['common_conditions'] = evidence
                report['actual_viewport_is_1600x900'] = evidence['viewport'] == [1600, 900]
                if not report['actual_viewport_is_1600x900']:
                    report['limits'].append('실제 PIE 뷰포트는 1600×900이 아님; actual viewport를 기준으로만 해석')
            else:
                assert evidence == report['common_conditions'], '라운드 사이 카메라/구도/뷰포트/CVar 변경'
            state['round']['conditions_before'] = evidence
            command('CsvProfile STARTFILE=' + state['csv_path'].name)
            command('CsvProfile START')
            state['capturing'] = True
            state['capture_started'] = time.monotonic()
            advance('capture_stop', WINDOW_SECONDS)
        elif stage == 'capture_stop':
            state['round']['conditions_after'] = camera_evidence()
            assert state['round']['conditions_after'] == report['common_conditions'], '측정 중 조건 변경'
            command('CsvProfile STOP')
            state['capturing'] = False
            state['round']['requested_window_elapsed_seconds'] = time.monotonic() - state['capture_started']
            state['csv_wait_started'] = time.monotonic()
            state['stable_size'] = None
            advance('wait_csv', 1.)
        elif stage == 'wait_csv':
            assert now - state['csv_wait_started'] < 30, 'CSV 완료 30초 초과'
            path = state['csv_path']
            size = path.stat().st_size if path.exists() else None
            if not size or size != state.get('stable_size'):
                state['stable_size'] = size
                return
            parsed = read_finished_csv(path)
            if parsed is None:
                return
            series = parsed.pop('series')
            state['round'].update(parsed, csv_sha256=digest(path), _series=series)
            report['rounds'].append(state.pop('round'))
            state['round_index'] += 1
            if state['round_index'] < len(ORDER):
                advance('prepare_round', 1.)
            else:
                build_comparison()
                start_cleanup()
        elif stage == 'cleanup':
            if state.get('world'):
                if state.get('layers') and state.get('new_meshes'):
                    switch_visuals('new')
                for name, value in state.get('cvars_before', {}).items():
                    command(f'{name} {value}')
            # PIE 객체 강참조를 놓고 우리가 시작한 PIE만 종료한다.
            for key in ('world', 'controller', 'globe', 'root', 'rig', 'layers', 'ocean'):
                state.pop(key, None)
            if state['pie_requested']:
                level.editor_request_end_play()
            advance('wait_end', .5)
        elif stage == 'wait_end':
            if level.is_in_play_in_editor():
                assert now - state['cleanup_started'] < 40, 'PIE 종료 40초 초과'
                return
            advance('settle_end', 2.)
        elif stage == 'settle_end':
            finish()
    except Exception:
        error = traceback.format_exc()
        if state.get('stage') in ('cleanup', 'wait_end', 'settle_end'):
            report['cleanup_errors'].append(error)
            report.setdefault('error', error)
            unreal.log_error('BRIGHT_GLOBE_FAIL ' + error)
            if level.is_in_play_in_editor() and state.get('pie_requested'):
                level.editor_request_end_play()
            finish()
        else:
            start_cleanup(error)


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle = unreal.register_slate_post_tick_callback(tick)
