"""P2 전용 신규 UE 에셋. 원본 입력과 Blender 출력의 UE 결과를 직접 대조."""
from pathlib import Path
import sys
import json
import traceback
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
sys.path.insert(0, str(ROOT / 'Tools/ui'))
sys.path.insert(0, str(ROOT / 'Tools/ui/blender/bright_globe'))
import import_diorama_pilot as helper
from validate_roundtrip import validate

DEST = '/Game/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1/P02'
ART = ROOT / 'SourceArt/UI/Atlas/BrightGlobe_v1/P02'
OUT = ROOT / 'Saved/BrightGlobe'
report = {'status': 'running', 'meshes': [], 'frontend_modified': False}


def readback(mesh):
    vertices, triangles, normals, uvs, tangents = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh, 0, 0)
    points = [(p.x, p.y, p.z) for p in vertices]
    texcoords = [(u.x, u.y) for u in uvs]
    faces = [tuple((triangles[i+a], triangles[i+a]) for a in range(3)) for i in range(0, len(triangles), 3)]
    return points, texcoords, faces


def create_mesh(name, group, material):
    path = DEST + '/' + group + '/' + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)  # 중단 후 재개할 때 기존 메시를 재임포트하지 않음
    helper.DEST = DEST + '/' + group
    helper.SRC = ART / ('Inputs' if group == 'Reference' else 'Exports')
    mesh = helper.mesh(name, material)
    assert len(mesh.get_editor_property('static_materials')) == 1
    return mesh


def run():
    helper.DEST = DEST
    helper.SRC = ART / 'Inputs'
    marker = OUT / 'p02_ue_import.json'
    if marker.exists():
        previous = json.loads(marker.read_text(encoding='utf-8'))
        assert previous.get('status') != 'passed', '이미 검증 완료. 결과를 먼저 확인'
    # 현재 제작기는 명시적인 100/350 기준을 사용한다. 새 P03 기준과 혼용하지 않는다.
    col = helper.asset('MPC_BrightP02_Controls', unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
    params = []
    for name, value in [('LandThickness', 100.), ('MountainHeight', 350.)]:
        p = unreal.CollectionScalarParameter()
        p.set_editor_property('parameter_name', name)
        p.set_editor_property('default_value', value)
        params.append(p)
    col.set_editor_property('scalar_parameters', params)
    helper.save(col)
    color = unreal.load_asset('/Game/FestaFiesta/UI/Atlas/Earth/DioramaWorld_v1/T_Diorama_Color')
    height = unreal.load_asset('/Game/FestaFiesta/UI/Atlas/Earth/DioramaWorld_v1/T_Diorama_Height')
    materials = {kind: helper.material('M_BrightP02_' + kind, kind, col, color, height)
                 for kind in ['top', 'wall', 'bottom']}
    for path in sorted((ART / 'Inputs').glob('*.obj')):
        kind = {'Top': 'top', 'Coast': 'wall', 'Bottom': 'bottom'}[path.stem.rsplit('_', 1)[1]]
        a = create_mesh(path.stem, 'Reference', materials[kind])
        b = create_mesh(path.stem, 'Roundtrip', materials[kind])
        a_data, b_data = readback(a), readback(b)
        result = validate(a_data, b_data)
        result.update(name=path.stem, reference=a.get_path_name(), roundtrip=b.get_path_name(), material_slots=1)
        # 북반구+대부분 동경인 시험 영역. 엔진 반전 후 지리 위치와 UV를 함께 기록.
        result['samples'] = [{'xyz': b_data[0][i], 'uv': b_data[1][i]}
            for i in range(0, len(b_data[0]), max(1, len(b_data[0]) // 6))]
        report['meshes'].append(result)
        marker.write_text(json.dumps(report, indent=2), encoding='utf-8')
        unreal.log('BRIGHT_P02_VALIDATED ' + path.stem)
    report.update(status='passed', mpc=col.get_path_name(), radius=5000, base_thickness=100, base_mountain=350,
                  rendered_height_variants_verified=False, saved_asset_reopen_verified=False)


try:
    run()
except Exception:
    report.update(status='failed', error=traceback.format_exc())
    unreal.log_error('BRIGHT_GLOBE_FAIL ' + report['error'])
(OUT / 'p02_ue_import.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.SystemLibrary.quit_editor()
