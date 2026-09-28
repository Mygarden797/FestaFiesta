# restore_skeleton.py
# 역할: 플레이어 캐릭터 모델·애니메이션 작업에서 관련 도구가 공유하는 설정과 처리 함수를 제공한다.

"""Restore MainMan's deleted Skeleton from the mesh's retained reference bones."""
import hashlib
import json
import traceback
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[2]
DESTINATION = '/Game/FestaFiesta/Character/Main/Man'
OUT = ROOT / 'Saved/MainMan_SkeletonRestore'
OUT.mkdir(parents=True, exist_ok=True)
report = {'destination': DESTINATION, 'method': 'SkeletonFactory from existing skeletal mesh'}


def mesh_state(mesh):
    component = unreal.SkeletalMeshComponent()
    component.set_skeletal_mesh_asset(mesh)
    names = [str(component.get_bone_name(i)) for i in range(component.get_num_bones())]
    return {
        'bone_names': names,
        'parents': {name: str(component.get_parent_bone(name)) for name in names},
        'materials': [str(slot.get_editor_property('material_interface').get_path_name())
                      for slot in mesh.get_editor_property('materials')],
        'height_cm': mesh.get_bounds().box_extent.z * 2,
    }


def verify():
    mesh = unreal.load_asset(DESTINATION + '/SK_MainMan')
    skeleton = unreal.load_asset(DESTINATION + '/SK_MainMan_Skeleton')
    assert isinstance(mesh, unreal.SkeletalMesh), 'Mesh missing'
    assert isinstance(skeleton, unreal.Skeleton), 'Skeleton missing'
    assert mesh.get_editor_property('skeleton') == skeleton, 'Mesh not connected to restored skeleton'
    state = mesh_state(mesh)
    assert len(state['bone_names']) == 52
    assert len(skeleton.get_editor_property('bone_tree')) == 52, 'Skeleton bone hierarchy incomplete'
    assert all(f'{finger}_{i:02}_{side}' in state['bone_names']
               for side in ['l', 'r'] for finger in ['thumb', 'index', 'middle', 'ring', 'pinky']
               for i in [1, 2, 3])
    assert len(state['materials']) == 9
    assert 169 < state['height_cm'] < 171
    state['skeleton_path'] = skeleton.get_path_name()
    state['skeleton_bone_count'] = len(skeleton.get_editor_property('bone_tree'))
    for name in ['SK_MainMan', 'SK_MainMan_Skeleton']:
        path = ROOT / 'Content/FestaFiesta/Character/Main/Man' / (name + '.uasset')
        assert path.is_file() and path.stat().st_size > 0, str(path)
    return state


def restore():
    assert Path(unreal.Paths.project_dir()).resolve() == ROOT, 'Wrong project'
    mesh = unreal.load_asset(DESTINATION + '/SK_MainMan')
    assert isinstance(mesh, unreal.SkeletalMesh), 'Existing mesh missing'
    assert mesh.get_editor_property('skeleton') is None, 'Mesh already has a skeleton; do not overwrite'
    assert not unreal.EditorAssetLibrary.does_asset_exist(DESTINATION + '/SK_MainMan_Skeleton'), 'Skeleton already exists'
    before = mesh_state(mesh)
    assert len(before['bone_names']) == 52, 'Unexpected source reference bones'
    report['before'] = before
    factory = unreal.SkeletonFactory()
    factory.set_editor_property('target_skeletal_mesh', mesh)
    skeleton = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'SK_MainMan_Skeleton', DESTINATION, unreal.Skeleton, factory)
    assert isinstance(skeleton, unreal.Skeleton), 'Skeleton creation failed'
    assert len(skeleton.get_editor_property('bone_tree')) == 52, 'Created skeleton is incomplete'
    mesh.set_skeleton(skeleton)
    if hasattr(skeleton, 'set_preview_mesh'):
        skeleton.set_preview_mesh(mesh)
    assert mesh_state(mesh) == before, 'Mesh geometry, hierarchy or materials unexpectedly changed'
    for obj in [skeleton, mesh]:
        assert unreal.EditorAssetLibrary.save_loaded_asset(obj, only_if_is_dirty=False), obj.get_path_name()
    report['after'] = verify()
    report['status'] = 'PASS'


if __name__ == '__main__':
    try:
        restore()
        unreal.log('MAINMAN_SKELETON_RESTORE_PASS ' + json.dumps(report))
    except Exception:
        report['status'] = 'FAIL'
        report['error'] = traceback.format_exc()
        unreal.log_error('MAINMAN_SKELETON_RESTORE_FAIL ' + report['error'])
    finally:
        (OUT / 'restore_report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
