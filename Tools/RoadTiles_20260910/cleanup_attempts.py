# cleanup_attempts.py
# 역할: 도로·타일 제작 원본과 납품 작업에서 해당 제작 작업에서 실패한 임시 결과만 정리한다.

"""Remove only this road job's failed preliminary packages after final validation."""
import json
from pathlib import Path
import time
import traceback

import unreal

TOOLS = Path(__file__).resolve().parent
ROOT = TOOLS.parent.parent
OUT = TOOLS / 'qa'
BASE = '/Game/Asset/RoadAsset_HKH/RoadTiles_20260910'
report = {'status': 'RUNNING', 'timestamp': time.time(), 'deleted': [], 'kept_final': [], 'checks': []}


def run():
    verified = json.loads((OUT / 'unreal_reload_report.json').read_text(encoding='utf-8'))
    imported = json.loads((OUT / 'unreal_import_report.json').read_text(encoding='utf-8'))
    if imported['status'] != 'PASS' or verified['status'] != 'PASS':
        raise RuntimeError('Successful import and fresh-process verification required')
    final = imported['destination']
    if not final.startswith(BASE + '/Run_'):
        raise RuntimeError('Expected isolated final Run folder')
    packages = [str(value).split('.', 1)[0] for value in unreal.EditorAssetLibrary.list_assets(BASE, True, False)]
    candidates = sorted(set(path for path in packages if not path.startswith(final + '/')))
    report['kept_final'] = sorted(path for path in packages if path.startswith(final + '/'))
    source_prefix = str(ROOT / 'SourceArt/RoadTiles_20260910').replace('\\', '/').casefold()
    for package in candidates:
        if not package.startswith(BASE + '/'):
            raise RuntimeError('Package outside this task: ' + package)
        asset = unreal.load_asset(package)
        source = str(unreal.EditorAssetLibrary.get_metadata_tag(asset, 'RoadTiles.Source')).replace('\\', '/').casefold()
        contract = str(unreal.EditorAssetLibrary.get_metadata_tag(asset, 'RoadTiles.MaterialContract'))
        owned = source.startswith(source_prefix + '/') or (
            asset.get_name() == 'M_RoadPalette' and contract == 'PaletteUV0;Roughness=0.78;Metallic=0;OneSided')
        if not owned:
            raise RuntimeError('Missing road source ownership metadata: ' + package)
        referencers = [str(item).split('.', 1)[0] for item in
                       unreal.EditorAssetLibrary.find_package_referencers_for_asset(package, True)]
        external = [item for item in referencers if item not in candidates]
        report['checks'].append({'package': package, 'ownership_verified': True,
                                 'referencers': referencers, 'external_referencers': external})
        if external:
            raise RuntimeError('Package referenced outside failed attempts: ' + package)
    # Remove geometry first, materials next and textures last.
    def order(package):
        name = package.rsplit('/', 1)[-1]
        return (2 if name.startswith('T_') else 1 if name.startswith('M_') else 0, package)
    for package in sorted(candidates, key=order):
        if not unreal.EditorAssetLibrary.delete_asset(package):
            raise RuntimeError('Could not delete owned failed package: ' + package)
        report['deleted'].append(package)
    report['status'] = 'PASS'
    unreal.log('ROAD_CLEANUP_PASS: removed {} owned preliminary packages'.format(len(report['deleted'])))


try:
    run()
except Exception:
    report['status'] = 'FAIL'
    report['error'] = traceback.format_exc()
    unreal.log_error('ROAD_CLEANUP_FAIL: ' + report['error'])
finally:
    payload = json.dumps(report, indent=2)
    (OUT / 'unreal_cleanup_report.json').write_text(payload, encoding='utf-8')
    (ROOT / 'SourceArt/RoadTiles_20260910/qa/unreal_cleanup_report.json').write_text(payload, encoding='utf-8')
    unreal.SystemLibrary.quit_editor()
