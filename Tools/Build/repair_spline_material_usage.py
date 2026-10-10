"""Inspect or repair the three authored Test_Main spline material instances.

Run in a fresh UnrealEditor-Cmd PythonScript commandlet without loading a map:
map loading can enable material usage transiently and conceal an unsaved flag.
Default is read-only. Set FESTAFIESTA_REPAIR_SPLINE_MATERIAL=1 to apply.
The exact allowlist comes from the active SplineMeshComponent inventory; it does
not include similarly named unused materials elsewhere in the project.
"""
import hashlib
import json
import os
import shutil
import sys
from datetime import datetime, timezone
from pathlib import Path

import unreal

root = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(root / 'Tools/facilities'))
from ffzs_asset_save_transaction import AssetSaveTransaction

MATERIAL_PACKAGES = (
    '/Game/Level/Demo_Matereal/Mesh/기본건축물/도로/Mat/M_S5_PaverRed',
    '/Game/Level/Demo_Matereal/Mesh/기본건축물/도로/Mat/M_S5_Curb',
    '/Game/Level/Demo_Matereal/Mesh/기본건축물/축구장/M_PL_Wood',
)
BASE_PACKAGE = '/Game/FestaFiesta/Environment/Materials/M_FF_PaverSplineBase'
BASE_OBJECT = BASE_PACKAGE + '.M_FF_PaverSplineBase'
ORIGINAL_PARENT = '/InterchangeAssets/Materials/FBXLegacyPhongSurfaceMaterial.FBXLegacyPhongSurfaceMaterial'
TARGET_PACKAGES = MATERIAL_PACKAGES + (BASE_PACKAGE,)
ENGINE_BASE_SHA256 = 'c87741529a3d4fbea413f778d2c3f4866f719f207fcb1dc2c0882632e6c8859b'
engine_source = Path(unreal.Paths.engine_dir()).resolve() / (
    'Plugins/Interchange/Assets/Content/Materials/FBXLegacyPhongSurfaceMaterial.uasset')
folder = root / 'Artifacts/SplineMaterialRepair' / datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-%f')
folder.mkdir(parents=True)


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def package_files(package_names=TARGET_PACKAGES):
    files = []
    for name in package_names:
        base = root / 'Content' / name[len('/Game/'):]
        files.extend(sorted(base.parent.glob(base.name + '.*')))
    return files


def package_hashes(package_names=TARGET_PACKAGES):
    # Include sidecars and presence/absence of the project base: duplicate_asset
    # can save a new package even before the explicit save calls below.
    return {str(path): sha256(path) for path in package_files(package_names)}


def parameter_snapshot(material):
    """Capture exposed override values/identities and resolved static switches.

    Export struct contents, not wrapper repr strings containing memory addresses.
    Read these before ANY mutation, so unavailable reflection fails safely.
    """
    result = {}
    for name in (
        'ScalarParameterValues', 'VectorParameterValues', 'DoubleVectorParameterValues',
        'TextureParameterValues', 'TextureCollectionParameterValues',
        'ParameterCollectionParameterValues', 'RuntimeVirtualTextureParameterValues',
        'SparseVolumeTextureParameterValues', 'FontParameterValues',
        'UserSceneTextureOverrides',
    ):
        result[name] = [item.export_text() for item in material.get_editor_property(name)]
    for name in ('BasePropertyOverrides', 'NaniteOverrideMaterial'):
        result[name] = material.get_editor_property(name).export_text()
    # StaticParametersRuntime/EditorOnlyData.StaticParameters are protected from
    # Python reads in 5.7. Use the public native getter for the direct FBX base's
    # global switches. SetMaterialInstanceParent calls SetParentEditorOnly; it
    # does not clear/recreate any authored override arrays.
    result['ResolvedStaticSwitches'] = {
        str(name): bool(unreal.MaterialEditingLibrary.get_material_instance_static_switch_parameter_value(material, name))
        for name in unreal.MaterialEditingLibrary.get_static_switch_parameter_names(material)
    }
    return result


def material_state(material):
    parent = material.get_editor_property('parent')
    assert isinstance(parent, unreal.Material), 'Expected direct base Material: ' + material.get_path_name()
    parent_path = parent.get_path_name()
    assert parent_path in (ORIGINAL_PARENT, BASE_OBJECT), 'Unexpected parent; review before repair: ' + parent_path
    usage = bool(parent.get_editor_property('used_with_spline_meshes'))
    return {'parent': parent_path, 'used_with_spline_meshes': usage,
            'valid': parent_path == BASE_OBJECT and usage,
            'parameter_overrides': parameter_snapshot(material)}


apply = os.environ.get('FESTAFIESTA_REPAIR_SPLINE_MATERIAL') == '1'
report = {'schema_version': 3, 'mode': 'apply' if apply else 'inspect', 'status': 'running',
          'target_packages': list(TARGET_PACKAGES), 'saved': False, 'saved_packages': [],
          'engine_source': str(engine_source), 'expected_engine_sha256': ENGINE_BASE_SHA256}
transaction = None
before_package_hashes = None
try:
    report['before_engine_sha256'] = sha256(engine_source)
    assert report['before_engine_sha256'] == ENGINE_BASE_SHA256, 'Engine parent differs from the audited original'
    before_package_hashes = package_hashes()
    before_by_package = {package: package_hashes((package,)) for package in TARGET_PACKAGES}
    report['before_package_sha256'] = before_package_hashes
    materials = {}
    before = {}
    for package in MATERIAL_PACKAGES:
        material = unreal.load_asset(package)
        assert isinstance(material, unreal.MaterialInstanceConstant), 'Expected authored MaterialInstanceConstant: ' + package
        materials[package] = material
        before[package] = material_state(material)
    report['before_materials'] = before
    report['all_materials_valid_before'] = all(state['valid'] for state in before.values())
    report['repair_needed_before'] = not report['all_materials_valid_before']

    base_exists = unreal.EditorAssetLibrary.does_asset_exist(BASE_PACKAGE)
    base = unreal.load_asset(BASE_PACKAGE) if base_exists else None
    if base_exists:
        assert isinstance(base, unreal.Material), 'Existing project base has an unexpected class'
    report['project_base_existed_before'] = base_exists

    if apply and report['repair_needed_before']:
        # Permanent, hash-verified backups survive transaction commit and cover
        # every existing allowlisted package/sidecar, including unchanged ones.
        report['package_backups'] = []
        for path in package_files():
            backup = folder / 'Backups' / path.relative_to(root)
            backup.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, backup)
            backup_hash = sha256(backup)
            assert backup_hash == before_package_hashes[str(path)], 'Backup hash mismatch'
            report['package_backups'].append({'source': str(path), 'backup': str(backup), 'sha256': backup_hash})
        transaction = AssetSaveTransaction(TARGET_PACKAGES, 'SplineMaterialUsage')

        # The engine plugin asset is never edited or saved. Reuse the existing
        # project copy for all siblings, retaining the imported instance values.
        base_changed = False
        if base is None:
            base = unreal.EditorAssetLibrary.duplicate_asset(ORIGINAL_PARENT.split('.')[0], BASE_PACKAGE)
            assert isinstance(base, unreal.Material), 'Base material duplication failed'
            base_changed = True
        if not base.get_editor_property('used_with_spline_meshes'):
            unreal.MaterialEditingLibrary.set_material_usage(base, unreal.MaterialUsage.MATUSAGE_SPLINE_MESH)
            base_changed = True
        assert unreal.MaterialEditingLibrary.has_material_usage(base, unreal.MaterialUsage.MATUSAGE_SPLINE_MESH), 'Material usage was not enabled'
        if base_changed:
            unreal.MaterialEditingLibrary.recompile_material(base)
            assert unreal.EditorAssetLibrary.save_loaded_asset(base, only_if_is_dirty=False), 'Base material save failed'
            report['saved_packages'].append(BASE_PACKAGE)

        for package, material in materials.items():
            if before[package]['parent'] != BASE_OBJECT:
                unreal.MaterialEditingLibrary.set_material_instance_parent(material, base)
                assert parameter_snapshot(material) == before[package]['parameter_overrides'], 'Reparenting changed overrides: ' + package
                assert unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False), 'Material instance save failed: ' + package
                report['saved_packages'].append(package)

    after = {package: material_state(material) for package, material in materials.items()}
    report['after_materials'] = after
    report['all_materials_valid_after'] = all(state['valid'] for state in after.values())
    report['parameter_overrides_preserved'] = all(
        after[package]['parameter_overrides'] == before[package]['parameter_overrides'] for package in MATERIAL_PACKAGES)
    assert report['parameter_overrides_preserved'], 'Authored instance overrides changed'
    if apply:
        assert report['all_materials_valid_after'], 'A target material still needs repair'
    report['after_package_sha256'] = package_hashes()
    report['after_engine_sha256'] = sha256(engine_source)
    assert report['after_engine_sha256'] == report['before_engine_sha256'], 'Engine material bytes changed'
    for package in TARGET_PACKAGES:
        if package not in report['saved_packages']:
            assert package_hashes((package,)) == before_by_package[package], 'An unsaved target changed: ' + package
    if not report['saved_packages']:
        assert report['after_package_sha256'] == before_package_hashes, 'Read-only/idempotent run modified package bytes'
    # Commit only after all scope, parent, usage, parameter and hash checks pass.
    if transaction:
        transaction.commit()
        transaction = None
    report['saved'] = bool(report['saved_packages'])
    report['status'] = 'applied' if report['saved'] else 'inspection-complete'
except Exception as error:
    report['status'] = 'failed'
    report['error'] = str(error)
    if transaction:
        # Unreal may retain a read handle on the original package. Avoid copying
        # an unchanged file over that handle when failure preceded any save.
        if package_hashes() == before_package_hashes:
            transaction.commit()
            report['rollback_not_needed'] = 'No package bytes changed'
        else:
            try:
                transaction.rollback()
            except Exception as rollback_error:
                report['rollback_error'] = str(rollback_error)
                raise
        report['rollback_package_sha256'] = package_hashes()
        assert report['rollback_package_sha256'] == before_package_hashes, 'Rollback differs from original package bytes'
    raise
finally:
    if engine_source.exists():
        report['final_engine_sha256'] = sha256(engine_source)
        report['engine_unchanged'] = report['final_engine_sha256'] == report.get('before_engine_sha256')
    (folder / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('SPLINE_MATERIAL_REPORT: ' + str(folder / 'report.json'))

