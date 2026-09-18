"""저장된 햇살 부모 재질 한 개에만 움직임을 추가한다.

일회용 에디터에서 실행한다. 기존 MI 오버라이드·레벨·조명·카메라는 저장하지
않는다. 수정 전 부모 재질과 MI를 Saved/BrightGlobeBefore에 백업한다.
실제 GPU 컴파일·시각 확인은 후속 PIE 검증과 프로세스 로그로 판정한다.
"""
from datetime import datetime, timezone
from pathlib import Path
import hashlib
import json
import shutil
import sys
import traceback

import unreal


ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = ROOT / 'Saved/BrightGlobe'
OUT.mkdir(parents=True, exist_ok=True)
STAMP = datetime.now().strftime('%Y%m%d-%H%M%S-%f')
REPORT = OUT / ('sun_motion_apply_' + STAMP + '.json')
sys.path.insert(0, str(ROOT / 'Tools/ui/blender/bright_globe'))
import sunny_title

MAT_PATH = sunny_title.DEST + '/M_BrightGlobe_SunnyBackdrop'
MI_PATH = sunny_title.DEST + '/MI_BrightGlobe_SunnyBackdrop'
MATERIAL_FILE = 'Content' + MAT_PATH.removeprefix('/Game') + '.uasset'
INSTANCE_FILE = 'Content' + MI_PATH.removeprefix('/Game') + '.uasset'
mel = unreal.MaterialEditingLibrary
report = dict(status='running', started_at=datetime.now(timezone.utc).isoformat(),
              target_material=MAT_PATH, saved_assets=[], material_instance_saved=False,
              gpu_compile_and_visual_review='후속 렌더링 검증 필요')


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def protected_hashes():
    paths = [ROOT / p for p in (
        'Content/FestaFiesta/Maps/Frontend.umap',
        'Content/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas.uasset',
        'Content/FestaFiesta/UI/Title/GlassOrbit/WBP_TitleMain_MyCasual.uasset',
    )]
    paths += sorted((ROOT / 'Content/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1/P10').glob('*.uasset'))
    return {p.relative_to(ROOT).as_posix(): digest(p)
            for p in paths if p.relative_to(ROOT).as_posix() != MATERIAL_FILE}


def parameters(instance):
    values = {'scalars': {}, 'vectors': {}}
    for name in mel.get_scalar_parameter_names(instance):
        values['scalars'][str(name)] = mel.get_material_instance_scalar_parameter_value(instance, name)
    for name in mel.get_vector_parameter_names(instance):
        value = mel.get_material_instance_vector_parameter_value(instance, name)
        values['vectors'][str(name)] = [value.r, value.g, value.b, value.a]
    return values


def write_report():
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')


try:
    material = unreal.load_asset(MAT_PATH)
    instance = unreal.load_asset(MI_PATH)
    assert isinstance(material, unreal.Material) and isinstance(instance, unreal.MaterialInstanceConstant)
    assert instance.get_editor_property('parent') == material, '현재 배경 부모가 예상과 다름'
    assert material.get_editor_property('shading_model') == unreal.MaterialShadingModel.MSM_UNLIT
    assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), 'PIE 중 적용 금지'
    before = parameters(instance)
    before_hashes = protected_hashes()
    backup = ROOT / 'Saved/BrightGlobeBefore' / ('SunMotion-' + STAMP)
    report['backup_directory'] = str(backup)
    report['backup_hashes'] = {}
    for relative in (MATERIAL_FILE, INSTANCE_FILE):
        source, destination = ROOT / relative, backup / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
        expected = digest(source)
        assert digest(destination) == expected, relative
        report['backup_hashes'][relative] = expected
    report.update(parameters_before=before, protected_hashes_before=before_hashes)
    write_report()
    report['patch'] = sunny_title.add_sun_motion(material)
    mel.layout_material_expressions(material)
    mel.recompile_material(material)
    after = parameters(instance)
    for kind, values in before.items():
        for name, expected in values.items():
            assert after[kind][name] == expected, '기존 파라미터 변경: ' + name
    for name, (default, _, _, _) in sunny_title.SUN_MOTION_PARAMETERS.items():
        assert name in after['scalars'], '새 파라미터 노출 실패: ' + name
        if name not in before['scalars']:
            assert abs(after['scalars'][name] - default) < 1.e-6, name
    sunny_title.save(material)
    report['saved_assets'].append(MAT_PATH)
    after_hashes = protected_hashes()
    assert before_hashes == after_hashes, '부모 재질 외 저장된 에셋이 변경됨'
    report.update(status='applied', parameters_after=after,
                  original_instance_values_preserved=True,
                  protected_assets_unchanged=True,
                  protected_hashes_after=after_hashes,
                  material_hash_after=digest(ROOT / MATERIAL_FILE))
except Exception:
    report.update(status='failed', error=traceback.format_exc())
    unreal.log_error('BRIGHT_GLOBE_FAIL ' + report['error'])
write_report()
unreal.log('BRIGHT_SUN_MOTION_REPORT ' + str(REPORT))
