# prepare_relocation.py
# 역할: 건설 에셋 폴더 이관 작업에서 후속 변경에 필요한 기준 자료를 준비한다.

"""Record the authorized building asset relocation before Unreal renames packages.

This historical mapping intentionally retains old paths. Run with system Python.
The editor command FF.MoveBuildingAssets performs the actual uasset moves.
"""
import hashlib
import json
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CONTENT = ROOT / 'Content'
REPORT = ROOT / 'Saved/BuildingAssetRelocation_20260921'
BASE = 'FestaFiesta/Building/FF_WBP_Building'
MAPPINGS = {
    'FestaFiesta/UI/Building': BASE + '/Controls',
    'FestaFiesta/UI/PrepHUD/Build': BASE + '/Catalog',
    'FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelBuild': BASE + '/Catalog/WBP_FF_PrepPanelBuild',
    'FestaFiesta/UI/PrepHUD/WBP_FF_PrepHudBar': BASE + '/Shared/WBP_FF_PrepHudBar',
    'FestaFiesta/UI/PrepHUD/WBP_FF_PrepHudTabButton': BASE + '/Shared/WBP_FF_PrepHudTabButton',
    'FestaFiesta/UI/PrepHUD/WBP_FF_PrepChoiceButton': BASE + '/Shared/WBP_FF_PrepChoiceButton',
    'FestaFiesta/Zone/UI/Building': BASE + '/Catalog/Legacy',
    'FestaFiesta/Zone/Test_Asset/image': BASE + '/Catalog/Images',
    'FestaFiesta/UI/Facility': BASE + '/Facility',
    'FestaFiesta/UI/FacilityRevenue': BASE + '/FacilityRevenue',
    'FestaFiesta/UI/FacilityCodex': BASE + '/FacilityCodex',
    'FestaFiesta/Building/Guide': BASE + '/Guide',
    'FestaFiesta/Building/Tooltips': BASE + '/Tooltips',
    'FestaFiesta/Building/Feedback': BASE + '/Feedback',
    'FestaFiesta/Building/Debug': BASE + '/Debug',
    'FestaFiesta/Building/Image': BASE + '/Images',
    'FestaFiesta/Zone/Data/FacilityDefinitions': 'FestaFiesta/Building/FF_Data_Building',
}


def main():
    REPORT.mkdir(parents=True, exist_ok=True)
    if (REPORT / 'manifest.json').exists():
        raise RuntimeError('Manifest already exists; preserve the original relocation snapshot.')
    assets, sources = [], []
    for old, new in MAPPINGS.items():
        source = CONTENT / old
        files = sorted(source.rglob('*')) if source.is_dir() else [source.with_suffix('.uasset')]
        for file in files:
            if not file.is_file():
                continue
            target = CONTENT / new / file.relative_to(source) if source.is_dir() else (CONTENT / new).with_suffix(file.suffix)
            assert target.resolve().is_relative_to(CONTENT.resolve())
            if target.exists():
                raise RuntimeError(f'Destination already exists: {target}')
            backup = REPORT / 'backup' / file.relative_to(ROOT)
            backup.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(file, backup)
            record = {'old_file': file.relative_to(ROOT).as_posix(), 'new_file': target.relative_to(ROOT).as_posix(),
                      'sha256': hashlib.sha256(file.read_bytes()).hexdigest()}
            if file.suffix == '.uasset':
                record.update(old='/Game/' + file.relative_to(CONTENT).with_suffix('').as_posix(),
                              new='/Game/' + target.relative_to(CONTENT).with_suffix('').as_posix())
                assets.append(record)
            else:
                sources.append(record)
    assert len({a['old'] for a in assets}) == len(assets) == 255
    manifest = {'assets': assets, 'source_files': sources, 'folder_mappings': MAPPINGS}
    (REPORT / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
    print(f'Prepared {len(assets)} assets and {len(sources)} source files; all backed up.')


if __name__ == '__main__':
    main()
