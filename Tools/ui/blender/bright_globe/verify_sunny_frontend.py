"""P10 저장 Frontend의 햇살 재질과 기존 14개 PIE 동작을 별도로 검증한다.

P08 원본 검증기는 수정하지 않는다. 기존 callback을 즉시 해제한 뒤 동기 사전
검사를 마치고 다시 등록하므로 P06의 옛 Frontend 해시 검사로 진입하지 않는다.
에셋 저장·물리 입력·외형 판정은 하지 않으며 보고서는 P10 경로에만 기록한다.
"""
import importlib.util
import json
from pathlib import Path
import traceback

import unreal


ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
SCRIPT_DIR = ROOT / 'Tools/ui/blender/bright_globe'
spec = importlib.util.spec_from_file_location(
    '_bright_globe_p10_verify_base', SCRIPT_DIR / 'verify_world_frontend.py')
base = importlib.util.module_from_spec(spec)
spec.loader.exec_module(base)
# 원본 모듈은 import 중 보고서를 쓰지 않는다. 이벤트 루프에 돌려주기 전에
# 출력 경로부터 바꾸고, 레벨 로드 중에도 옛 preflight가 실행되지 않게 해제한다.
base.REPORT = base.OUT / 'p10_frontend_verify.json'
layout_path = base.OUT / 'p11_title_layout.json'
layout_report = json.loads(layout_path.read_text(encoding='utf-8-sig')) if layout_path.exists() else None
if layout_report and layout_report.get('status') == 'passed':
    base.REPORT = base.OUT / 'p11_frontend_verify.json'
unreal.unregister_slate_post_tick_callback(base.handle)
base.handle = None
base.report.update(
    scope='P10 햇살 재질·P05 지형 보존 및 저장된 Frontend의 기존 14개 PIE 동작',
    expected_functional_check_count=14,
    p08_report_overwrite_requested=False,
    p10_material_contracts={},
)

SUNNY = '/Game/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1/P10'
FRONTEND_FILE = 'Content/FestaFiesta/Maps/Frontend.umap'
CATALOG_FILE = 'Content/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas.uasset'
TITLE_FILE = 'Content/FestaFiesta/UI/Title/GlassOrbit/WBP_TitleMain_MyCasual.uasset'
MATERIAL_NAMES = ('Water', 'Land', 'Coast', 'Backdrop')
materials = {}
expected_pp = {}


def object_path(folder, name):
    return folder + '/' + name + '.' + name


def current_actors():
    world = base.state.get('world')
    if world:
        return unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()


def layer_contract(globe):
    """메시 자체의 P05 슬롯과 액터 컴포넌트의 P10 override를 각각 검사한다."""
    components = list(globe.get_editor_property('land_layer_components'))
    mesh_assets = list(globe.get_editor_property('land_layer_meshes'))
    assert len(components) == len(mesh_assets) == 16, 'P05 지형 메시 16개 계약 위반'
    root = globe.get_editor_property('globe_root')
    expected = [(f'SM_BrightWorld_{tile:02d}_{part}', material)
                for tile in range(8) for part, material in (('Top', 'Land'), ('Coast', 'Coast'))]
    actual = []
    component_materials = []
    for comp, configured, (name, material) in zip(components, mesh_assets, expected):
        mesh = comp.get_editor_property('static_mesh')
        assert mesh and mesh == configured, comp.get_name()
        assert mesh.get_path_name() == object_path(base.DEST, name), comp.get_name()
        assert comp.get_owner() == globe and comp.get_attach_parent() == root
        original = mesh.get_material(0)
        assert original and original.get_path_name() == object_path(
            base.DEST, 'MI_BrightGlobe_' + material), 'P05 원본 메시 재질 변경: ' + name
        assert comp.get_material(0) == materials[material], 'P10 재질 누락: ' + comp.get_name()
        assert comp.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
        actual.append(mesh.get_path_name())
        component_materials.append(comp.get_material(0).get_path_name())
    ocean = globe.get_editor_property('globe_mesh')
    assert ocean.get_attach_parent() == root
    assert globe.get_editor_property('globe_material') == materials['Water']
    assert ocean.get_material(0) == materials['Water'], 'P10 수면 재질 누락'
    actors = list(current_actors())
    backdrops = [actor for actor in actors if actor.get_actor_label() == 'FF_Clay_Backdrop']
    assert len(backdrops) == 1, '배경 액터가 정확히 1개여야 함'
    backdrop = backdrops[0].get_component_by_class(unreal.StaticMeshComponent)
    assert backdrop and backdrop.get_material(0) == materials['Backdrop'], 'P10 배경 재질 누락'
    volumes = [actor for actor in actors if isinstance(actor, unreal.PostProcessVolume)]
    assert len(volumes) == 1, '후처리 볼륨이 정확히 1개여야 함'
    settings = volumes[0].get_editor_property('settings')
    observed_pp = {}
    for name, value in expected_pp.items():
        observed = float(settings.get_editor_property(name))
        assert settings.get_editor_property('override_' + name), name + ' override 누락'
        assert abs(observed - value) < 1.e-5, name + ' 저장값/실기값 불일치'
        observed_pp[name] = observed
    context = 'pie' if base.state.get('world') else 'saved_frontend'
    base.report['p10_material_contracts'][context] = {
        'terrain_meshes': actual, 'terrain_component_materials': component_materials,
        'water': ocean.get_material(0).get_path_name(),
        'backdrop': backdrop.get_material(0).get_path_name(),
        'post_process': observed_pp, 'original_mesh_materials_preserved': True,
    }
    return actual


def preflight():
    assert not base.level.is_in_play_in_editor(), '다른 PIE가 실행 중'
    preparation_path = base.OUT / 'p10_prepare.json'
    preparation = json.loads(preparation_path.read_text(encoding='utf-8-sig'))
    assert preparation.get('status') == 'prepared', 'P10 시험 준비가 완료되지 않음'
    assert preparation.get('original_assets_unchanged'), 'P10 준비 중 원본 변경 기록'
    integration_path = base.OUT / 'p06_frontend_integration.json'
    integration = json.loads(integration_path.read_text(encoding='utf-8-sig'))
    assert integration.get('status') == 'passed', '기존 Frontend 연결 검증이 완료되지 않음'
    previous = {row['file']: row['sha256'] for row in integration['file_hashes_after']}
    for rel in (CATALOG_FILE, TITLE_FILE):
        current = base.digest(ROOT / rel)
        original = preparation['hashes_before'][rel]
        assert original == previous[rel], rel + ' P06/P10 기준 불일치'
        expected = original
        if rel == TITLE_FILE and layout_report:
            assert layout_report.get('status') == 'passed', 'P11 타이틀 마감 미완료'
            assert layout_report['title_sha256_before'] == original, 'P11 시작 타이틀 불일치'
            expected = layout_report['title_sha256_after']
        assert current == expected, rel + ' 승인된 기준 외 변경'
    # Frontend는 P10 마감을 저장한 현재 파일을 기준으로 삼고, P05 에셋 전체와
    # P10 새 에셋·카탈로그·WBP·P08 결과의 검사 전후 바이트까지 보존 확인한다.
    protected = {ROOT / rel for rel in (FRONTEND_FILE, CATALOG_FILE, TITLE_FILE)}
    for folder in ('P05', 'P10'):
        directory = ROOT / 'Content/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1' / folder
        files = set(directory.rglob('*.uasset'))
        assert files, folder + ' 에셋 없음'
        protected.update(files)
    protected.update((preparation_path, integration_path, SCRIPT_DIR / 'verify_world_frontend.py'))
    previous_report = base.OUT / 'p08_frontend_verify.json'
    if previous_report.exists():
        protected.add(previous_report)
    if layout_report:
        protected.add(layout_path)
    base.report['file_hashes_before'] = {
        path.relative_to(ROOT).as_posix(): base.digest(path) for path in sorted(protected)
    }
    base.report['p10_preparation_report_sha256'] = base.digest(preparation_path)
    base.report['integration_report_sha256'] = base.digest(integration_path)
    base.report['catalog_and_title_unchanged_since_p06_and_p10_preparation'] = not bool(layout_report)
    base.report['p11_title_layout_verified'] = bool(layout_report)
    base.report['p05_hash_scope'] = 'PIE 검사 직전/직후 전체 P05 .uasset 바이트; 제작 이전 비교는 별도'
    for name in MATERIAL_NAMES:
        material = unreal.load_asset(SUNNY + '/MI_BrightGlobe_Sunny' + name)
        assert isinstance(material, unreal.MaterialInstanceConstant), 'P10 MI 없음: ' + name
        materials[name] = material
    config = preparation['settings']
    for property_name, key in (
        ('bloom_intensity', 'bloom'), ('bloom_threshold', 'bloom_threshold'),
        ('lens_flare_intensity', 'lens_flare_intensity'),
        ('vignette_intensity', 'vignette'), ('ambient_occlusion_intensity', 'ao'),
    ):
        expected_pp[property_name] = float(config[key])
    assert base.level.load_level(base.FRONTEND)
    globes = [actor for actor in current_actors() if isinstance(actor, unreal.FF_AtlasGlobeActor)]
    assert len(globes) == 1, '저장 Frontend의 지구 액터 개수 오류'
    layer_contract(globes[0])
    catalog = unreal.load_asset(base.CATALOG)
    assert catalog, '공유 국가 카탈로그 없음'
    base.state['catalog_before'] = {
        key: [row.export_text() for row in catalog.get_editor_property(key)]
        for key in ('countries', 'entries')
    }
    base.state['countries'] = {
        str(row.get_editor_property('country_id')): {
            'unlocked': bool(row.get_editor_property('unlocked')),
            'lock_reason': str(row.get_editor_property('lock_reason')),
        } for row in catalog.get_editor_property('countries')
    }
    countries = base.state['countries']
    assert countries['KR']['unlocked'] and not countries['DE']['unlocked'], 'KR/DE 준비 상태 변경'
    base.report['synchronous_preflight_passed'] = True
    base.state['pie_requested'] = True
    base.level.editor_request_begin_play()
    base.advance('wait_pie', 1.)


original_finish = base.finish


def finish():
    count = len(base.report['checks'])
    base.report['functional_check_count'] = count
    if 'error' not in base.report and count != 14:
        error = f'기존 PIE 동작 검사 개수 부족: {count}/14'
        base.report['error'] = error
        unreal.log_error('BRIGHT_GLOBE_FAIL ' + error)
    original_finish()


base.layer_contract = layer_contract
base.finish = finish
try:
    base.write_report()
    preflight()
except Exception:
    base.start_cleanup(traceback.format_exc())
finally:
    # 이후 단계는 원본 tick의 14개 검사 및 PIE 종료·해시 검증을 그대로 사용한다.
    base.handle = unreal.register_slate_post_tick_callback(base.tick)
