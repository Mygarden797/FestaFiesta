# import_project.py
# 역할: 플레이어 캐릭터 모델·애니메이션 작업에서 제작 결과를 Unreal 에셋으로 가져오고 연결한다.

"""Import the approved MainMan asset into the user-requested project folder."""
import hashlib
import json
import sys
import traceback
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'SourceArt/MainMan_20260911'
OUT = ROOT / 'Saved/MainMan_ProjectImport'
DESTINATION = '/Game/FestaFiesta/Character'


def require_project():
    """다른 프로젝트를 열어 둔 상태에서는 캐릭터 조회·가져오기를 시작하지 않는다."""
    if Path(unreal.Paths.project_dir()).resolve() != ROOT:
        raise RuntimeError('Wrong project')


def verify_saved():
    """저장된 메시·뼈대·재질과 디스크 파일을 확인한다. 에셋을 수정하거나 저장하지 않는다."""
    require_project()
    mesh = unreal.load_asset(DESTINATION + '/SK_MainMan')
    if not isinstance(mesh, unreal.SkeletalMesh):
        raise RuntimeError('Skeletal mesh missing')
    skeleton = mesh.get_editor_property('skeleton')
    if not skeleton or not skeleton.get_path_name().startswith(DESTINATION + '/'):
        raise RuntimeError('Invalid skeleton location')
    component = unreal.SkeletalMeshComponent()
    component.set_skeletal_mesh_asset(mesh)
    names = [str(component.get_bone_name(i)) for i in range(component.get_num_bones())]
    if len(names) != 52:
        raise RuntimeError('Expected 52 bones: ' + ', '.join(names))
    if not all(f'{finger}_{i:02}_{side}' in names for side in ['l', 'r']
               for finger in ['thumb', 'index', 'middle', 'ring', 'pinky'] for i in [1, 2, 3]):
        raise RuntimeError('Finger bones missing')
    slots = list(mesh.get_editor_property('materials'))
    if len(slots) != 9:
        raise RuntimeError('Expected 9 material slots, got ' + str(len(slots)))
    materials = [slot.get_editor_property('material_interface') for slot in slots]
    if not all(m and m.get_path_name().startswith(DESTINATION + '/M_MainMan_') for m in materials):
        raise RuntimeError('Missing material or material outside the character folder')
    assets = [mesh, skeleton] + materials
    paths = [obj.get_path_name() for obj in assets]
    disk_files = []
    for path in paths:
        package = path.split('.')[0]
        disk = ROOT / 'Content' / (package.removeprefix('/Game/') + '.uasset')
        if not disk.is_file() or disk.stat().st_size <= 0:
            raise RuntimeError('Saved asset file missing or empty: ' + str(disk))
        disk_files.append({'file': str(disk), 'bytes': disk.stat().st_size,
                           'sha256': hashlib.sha256(disk.read_bytes()).hexdigest()})
    bounds = mesh.get_bounds()
    height = bounds.box_extent.z * 2
    if not 169 < height < 171:
        raise RuntimeError('Expected character height near 170 cm, got ' + str(height))
    return {'asset_paths': paths, 'files': disk_files, 'bone_count': len(names),
            'material_count': len(materials), 'height_cm': height, 'all_fingers_present': True}


def main():
    """명시적으로 실행했을 때만 가져온다. 읽기 검증에서 이 파일을 불러오면 함수만 제공한다."""
    OUT.mkdir(parents=True, exist_ok=True)
    report = {'destination': DESTINATION, 'engine': unreal.SystemLibrary.get_engine_version()}
    try:
        require_project()
        palette = json.loads((SOURCE / 'Materials.json').read_text(encoding='utf-8'))
        expected = ['SK_MainMan', 'SK_MainMan_Skeleton'] + list(palette)
        collisions = [name for name in expected if unreal.EditorAssetLibrary.does_asset_exist(DESTINATION + '/' + name)]
        if collisions:
            raise RuntimeError('Existing assets require separate handling: ' + ', '.join(collisions))
        report['fbx_sha256'] = hashlib.sha256((SOURCE / 'MainMan_Unreal.fbx').read_bytes()).hexdigest()
        sys.path.insert(0, str(SOURCE))
        from Import_MainMan_Unreal import import_mainman
        mesh = import_mainman(DESTINATION, replace_existing=False)
        skeleton = mesh.get_editor_property('skeleton')
        for obj in [mesh, skeleton] + [s.get_editor_property('material_interface') for s in mesh.get_editor_property('materials')]:
            # 저장 호출은 assert 안에 두면 Python 최적화 옵션(-O)에서 실행 자체가 생략된다.
            if not unreal.EditorAssetLibrary.save_loaded_asset(obj, only_if_is_dirty=False):
                raise RuntimeError('Could not save asset: ' + obj.get_path_name())
        report.update(verify_saved())
        report['status'] = 'PASS'
        unreal.log('MAINMAN_PROJECT_IMPORT_PASS ' + json.dumps(report))
    except Exception:
        report['status'] = 'FAIL'
        report['error'] = traceback.format_exc()
        unreal.log_error('MAINMAN_PROJECT_IMPORT_FAIL ' + report['error'])
    finally:
        (OUT / 'import_report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    return report


if __name__ == '__main__':
    main()
