# repair_import_sources.py
# 역할: 건설 에셋 폴더 이관 작업에서 확인된 데이터 오류만 수정한다.

"""Rebase import filenames for relocated textures without reimporting image pixels."""
import json
import sys
from pathlib import Path
import unreal
sys.path.insert(0, str(Path(__file__).resolve().parent))
from verify_assets import ROOT, OUT, MANIFEST_FILE, BEFORE_FILE, expected_source_path, remap_value, require, write_report


def run():
    manifest = json.loads(MANIFEST_FILE.read_text(encoding='utf-8-sig'))
    before = json.loads(BEFORE_FILE.read_text(encoding='utf-8'))['assets']
    changes = []
    for entry in manifest['assets']:
        record = before[entry['old']]
        asset = unreal.load_asset(entry['new'])
        require(asset is not None, 'Moved asset missing: ' + entry['new'])
        dirty = False
        if record.get('import_sources'):
            data = asset.get_editor_property('asset_import_data')
            current = data.extract_filenames()
            require(len(current) == len(record['import_sources']), 'Import source count changed: ' + entry['new'])
            for index, old in enumerate(record['import_sources']):
                target = expected_source_path(old['resolved_filename'], manifest)
                actual = Path(unreal.Paths.convert_relative_path_to_full(str(current[index]))).resolve()
                if actual != target:
                    if len(current) == 1:
                        data.scripted_add_filename(str(target), 0, '')
                    else:
                        raise RuntimeError('Unexpected multisource texture: ' + entry['new'])
                    dirty = True
        expected_metadata = remap_value(record['metadata'], manifest)
        for key, value in expected_metadata.items():
            if unreal.EditorAssetLibrary.get_metadata_tag(asset, key) != value:
                unreal.EditorAssetLibrary.set_metadata_tag(asset, key, value)
                dirty = True
        if dirty:
            require(unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False), 'Save failed: ' + entry['new'])
            changes.append(entry['new'])
    write_report('import_repair.json', {'status': 'PASS', 'changed_assets': changes})
    unreal.log('FF_BUILD_RELOCATION_IMPORT_REPAIR_PASS changed={}'.format(len(changes)))


if __name__ == '__main__':
    run()
