# update_references.py
# 역할: 건설 에셋 폴더 이관 작업에서 연결 정보나 설정을 현재 기준에 맞게 갱신한다.

"""Update textual asset references and move backed-up import sources after UE rename.

Asset packages themselves are moved exclusively by FF.MoveBuildingAssets.
"""
import argparse
import hashlib
import json
from pathlib import Path
from prepare_relocation import ROOT, REPORT, MAPPINGS


def rewrite_text():
    replacements = []
    mappings = dict(MAPPINGS)
    mappings['FestaFiesta/Zone/Test_Asset/Image'] = MAPPINGS['FestaFiesta/Zone/Test_Asset/image']
    for old, new in sorted(mappings.items(), key=lambda item: -len(item[0])):
        for prefix in ['/Game/', 'Content/']:
            replacements.append(((prefix + old).encode(), (prefix + new).encode()))
        replacements.append((('Content/' + old).replace('/', '\\').encode(), ('Content/' + new).replace('/', '\\').encode()))
    changed = []
    for top in ['Source', 'Plugins', 'Config', 'Tools', 'Design', 'data']:
        for path in (ROOT / top).rglob('*'):
            if not path.is_file() or path.suffix.lower() not in {'.cpp', '.h', '.cs', '.ini', '.py', '.ps1', '.md', '.csv', '.json', '.txt'}:
                continue
            if 'BuildingAssetRelocation' in str(path) or path.name == 'FF_BuildingAssetRelocation.cpp':
                continue
            if any(p in {'Intermediate', 'Binaries', '__pycache__', '.git'} for p in path.parts):
                continue
            original = path.read_bytes()
            updated = original
            for old, new in replacements:
                updated = updated.replace(old, new)
            if updated != original:
                backup = REPORT / 'text_backup' / path.relative_to(ROOT)
                backup.parent.mkdir(parents=True, exist_ok=True)
                if not backup.exists():
                    backup.write_bytes(original)
                path.write_bytes(updated)
                changed.append(path.relative_to(ROOT).as_posix())
    (REPORT / 'text_references.json').write_text(json.dumps(changed, indent=2), encoding='utf-8')
    print(f'Updated {len(changed)} text files.')


def move_sources():
    manifest = json.loads((REPORT / 'manifest.json').read_text(encoding='utf-8'))
    for entry in manifest['assets']:
        if not (ROOT / entry['new_file']).is_file():
            raise RuntimeError('Unreal asset move has not finished: ' + entry['new_file'])
    for entry in manifest['source_files']:
        source, target = (ROOT / entry['old_file']).resolve(), (ROOT / entry['new_file']).resolve()
        assert source.is_relative_to((ROOT / 'Content').resolve()) and target.is_relative_to((ROOT / 'Content').resolve())
        if target.exists():
            assert not source.exists() and hashlib.sha256(target.read_bytes()).hexdigest() == entry['sha256']
            continue
        assert hashlib.sha256(source.read_bytes()).hexdigest() == entry['sha256']
        target.parent.mkdir(parents=True, exist_ok=True)
        source.rename(target)
    print(f'Moved {len(manifest["source_files"])} original PNG files with identical hashes.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('phase', choices=['text', 'sources'])
    args = parser.parse_args()
    rewrite_text() if args.phase == 'text' else move_sources()
