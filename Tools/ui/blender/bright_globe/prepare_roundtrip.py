"""P1 백업과 P2 무변형 왕복 입력. 기존 에셋을 수정하지 않는다."""
from pathlib import Path
from datetime import datetime
import hashlib
import json
import math
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[4]
ART = ROOT / 'SourceArt/UI/Atlas/BrightGlobe_v1'
OUT = ROOT / 'Saved/BrightGlobe'


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def parse_obj(path):
    vertices, uv, faces = [], [], []
    for line in path.read_text(encoding='utf-8').splitlines():
        words = line.split()
        if not words:
            continue
        if words[0] == 'v':
            vertices.append(tuple(map(float, words[1:4])))
        elif words[0] == 'vt':
            uv.append(tuple(map(float, words[1:3])))
        elif words[0] == 'f':
            assert len(words) == 4, '삼각형 원본만 허용'
            faces.append(tuple(tuple(int(v) - 1 for v in w.split('/')[:2]) for w in words[1:]))
    return vertices, uv, faces


def crop_obj(source, dest):
    verts, uvs, faces = parse_obj(source)
    kept = []
    for face in faces:
        center = [sum(verts[v][a] for v, _ in face) / 3 for a in range(3)]
        lon = math.degrees(math.atan2(center[1], center[0]))
        lat = math.degrees(math.atan2(center[2], math.hypot(center[0], center[1])))
        if -20 <= lon <= 55 and 20 <= lat <= 65:
            kept.append(face)
    vi = sorted({v for f in kept for v, _ in f})
    ui = sorted({u for f in kept for _, u in f})
    vm = {v: i + 1 for i, v in enumerate(vi)}
    um = {u: i + 1 for i, u in enumerate(ui)}
    lines = ['# P2 unchanged faces; geographic crop has intentionally open cut boundaries',
             'o GEO-' + dest.stem, 's 1']
    lines += ['v %.6f %.6f %.6f' % verts[v] for v in vi]
    lines += ['vt %.9f %.9f' % uvs[u] for u in ui]
    lines += ['f ' + ' '.join(f'{vm[v]}/{um[u]}' for v, u in f) for f in kept]
    dest.write_text('\n'.join(lines) + '\n', encoding='utf-8')
    return {'source': str(source.relative_to(ROOT)), 'source_sha256': digest(source),
            'input': str(dest.relative_to(ROOT)), 'input_sha256': digest(dest),
            'triangles': len(kept), 'vertices': len(vi)}


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    assert not (OUT / 'preparation.json').exists(), '기존 준비 기록이 있음: 먼저 재개 상태 확인'
    stamp = datetime.now().strftime('%Y%m%d-%H%M%S')
    backup = ROOT / 'Saved/BrightGlobeBefore' / stamp
    paths = [ROOT / p for p in [
        'Content/FestaFiesta/Maps/Frontend.umap',
        'Content/FestaFiesta/UI/Atlas/Earth/DioramaWorld_v1',
        'Content/FestaFiesta/UI/Atlas/Earth/CasualClay_v1',
        'Content/FestaFiesta/UI/Atlas/Earth/CitrusPop_v2',
        'Content/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas.uasset',
        'Content/FestaFiesta/UI/Title/GlassOrbit/WBP_TitleMain_MyCasual.uasset',
        'Config', 'SourceArt/UI/Atlas/DioramaWorld_v1',
        'SourceArt/ThirdParty/SolarSystemScope/PlanetTextures/Earth/2k_earth_daymap.jpg',
        'SourceArt/ThirdParty/NOAA/ETOPO1/global_10arcmin.csv',
        'SourceArt/UI/Title/CasualClay_Albedo_v1/GeographyLocked',
        'Source/FestaFiesta/Public/UI/LiveHUD/FF_MetricsHudWidget.h',
        'Source/FestaFiesta/Public/UI/ViewModel/FF_FestivalViewModelSubsystem.h',
        'Source/FestaFiesta/Public/UI/Atlas/FF_AtlasGlobeActor.h',
        'Source/FestaFiesta/Private/UI/Atlas/FF_AtlasGlobeActor.cpp']]
    entries = []
    for path in paths:
        assert path.exists(), path
        for source in sorted(path.rglob('*')) if path.is_dir() else [path]:
            if not source.is_file():
                continue
            relative = source.relative_to(ROOT)
            target = backup / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
            sha = digest(source)
            assert digest(target) == sha, target
            entries.append({'path': str(relative), 'bytes': source.stat().st_size, 'sha256': sha})
    (backup / 'manifest.json').write_text(json.dumps(entries, indent=2), encoding='utf-8')
    inputs = ART / 'P02/Inputs'
    inputs.mkdir(parents=True, exist_ok=True)
    meshes = []
    for tile in (3, 4, 5):
        for suffix in ('Top', 'Coast', 'Bottom'):
            name = f'SM_Diorama_{tile:02d}_{suffix}.obj'
            meshes.append(crop_obj(ROOT / 'SourceArt/UI/Atlas/DioramaWorld_v1' / name, inputs / name))
    for name in ('T_Diorama_Color.png', 'T_Diorama_Height.png'):
        shutil.copy2(ROOT / 'SourceArt/UI/Atlas/DioramaWorld_v1' / name, inputs / name)
    report = {'status': 'complete', 'backup': str(backup), 'backed_up_files': len(entries),
              'backed_up_bytes': sum(e['bytes'] for e in entries),
              'branch': subprocess.check_output(['git', 'branch', '--show-current'], cwd=ROOT, text=True).strip(),
              'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
              'preexisting_changes': ['Source/FestaFiesta/Public/UI/LiveHUD/FF_MetricsHudWidget.h',
                                      'Source/FestaFiesta/Public/UI/ViewModel/FF_FestivalViewModelSubsystem.h'],
              'bounds_lon_lat': [-20, 55, 20, 65], 'meshes': meshes,
              'contract': '원본 삼각형/UV/방향 유지; 영역 밖 면만 제외. 절단 경계는 의도적으로 열림. 새 해안 생성 아님.',
              'blender_version': '5.2.1 LTS', 'addon_protocol': 5, 'server_protocol': 7}
    (OUT / 'preparation.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({k: v for k, v in report.items() if k != 'meshes'}, ensure_ascii=False))


if __name__ == '__main__':
    main()
