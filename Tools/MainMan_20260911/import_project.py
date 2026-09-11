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
OUT.mkdir(parents=True, exist_ok=True)
report = {'destination': DESTINATION, 'engine': unreal.SystemLibrary.get_engine_version()}

def verify_saved():
    mesh = unreal.load_asset(DESTINATION + '/SK_MainMan')
    assert isinstance(mesh, unreal.SkeletalMesh), 'Skeletal mesh missing'
    skeleton = mesh.get_editor_property('skeleton')
    assert skeleton and skeleton.get_path_name().startswith(DESTINATION + '/'), 'Invalid skeleton location'
    component = unreal.SkeletalMeshComponent()
    component.set_skeletal_mesh_asset(mesh)
    names = [str(component.get_bone_name(i)) for i in range(component.get_num_bones())]
    assert len(names) == 52, names
    assert all(f'{finger}_{i:02}_{side}' in names for side in ['l', 'r']
               for finger in ['thumb', 'index', 'middle', 'ring', 'pinky'] for i in [1, 2, 3])
    slots = list(mesh.get_editor_property('materials'))
    assert len(slots) == 9, len(slots)
    materials = [slot.get_editor_property('material_interface') for slot in slots]
    assert all(m and m.get_path_name().startswith(DESTINATION + '/M_MainMan_') for m in materials)
    assets = [mesh, skeleton] + materials
    paths = [obj.get_path_name() for obj in assets]
    disk_files = []
    for path in paths:
        package = path.split('.')[0]
        disk = ROOT / 'Content' / (package.removeprefix('/Game/') + '.uasset')
        assert disk.is_file() and disk.stat().st_size > 0, str(disk)
        disk_files.append({'file': str(disk), 'bytes': disk.stat().st_size,
                           'sha256': hashlib.sha256(disk.read_bytes()).hexdigest()})
    bounds = mesh.get_bounds()
    height = bounds.box_extent.z * 2
    assert 169 < height < 171, height
    return {'asset_paths': paths, 'files': disk_files, 'bone_count': len(names),
            'material_count': len(materials), 'height_cm': height, 'all_fingers_present': True}

try:
    assert Path(unreal.Paths.project_dir()).resolve() == ROOT, 'Wrong project'
    palette = json.loads((SOURCE / 'Materials.json').read_text(encoding='utf-8'))
    expected = ['SK_MainMan', 'SK_MainMan_Skeleton'] + list(palette)
    collisions = [name for name in expected if unreal.EditorAssetLibrary.does_asset_exist(DESTINATION + '/' + name)]
    assert not collisions, 'Existing assets require separate handling: ' + ', '.join(collisions)
    report['fbx_sha256'] = hashlib.sha256((SOURCE / 'MainMan_Unreal.fbx').read_bytes()).hexdigest()
    sys.path.insert(0, str(SOURCE))
    from Import_MainMan_Unreal import import_mainman
    mesh = import_mainman(DESTINATION, replace_existing=False)
    skeleton = mesh.get_editor_property('skeleton')
    for obj in [mesh, skeleton] + [s.get_editor_property('material_interface') for s in mesh.get_editor_property('materials')]:
        assert unreal.EditorAssetLibrary.save_loaded_asset(obj, only_if_is_dirty=False), obj.get_path_name()
    report.update(verify_saved())
    report['status'] = 'PASS'
    unreal.log('MAINMAN_PROJECT_IMPORT_PASS ' + json.dumps(report))
except Exception:
    report['status'] = 'FAIL'
    report['error'] = traceback.format_exc()
    unreal.log_error('MAINMAN_PROJECT_IMPORT_FAIL ' + report['error'])
finally:
    (OUT / 'import_report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
