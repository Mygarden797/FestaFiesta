"""7개국 마스크와 8슬롯 재질을 추가하고 기존 영역 DataAsset만 확장한다."""
from pathlib import Path
import gc
import hashlib
import json
import shutil
import sys
import traceback
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
sys.path.insert(0, str(ROOT/'Tools/ui/atlas_regions'))
from world_material import build
SRC = ROOT/'SourceArt/UI/Atlas/CountryRegions'
OUT = ROOT/'Saved/AtlasRegions'
DEST = '/Game/FestaFiesta/UI/Atlas/CountryRegions'
ORDER = ['KR', 'BR', 'DE', 'JP', 'US', 'ES', 'TH', 'AU']
eal, mel = unreal.EditorAssetLibrary, unreal.MaterialEditingLibrary
report = {'status':'running', 'saved':[]}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def protected_hashes():
    paths = list((ROOT/'Content/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1').rglob('*.uasset'))
    paths += [ROOT/'Content/FestaFiesta/Maps/Frontend.umap',
              ROOT/'Content/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas.uasset']
    paths += [ROOT/f'Content/FestaFiesta/UI/Atlas/CountryRegions/{name}.uasset'
              for name in ('T_FF_AtlasRegion_KR','M_AtlasRegion_Land','MI_AtlasRegion_Land')]
    return {p.relative_to(ROOT).as_posix():digest(p) for p in paths}


def save(obj):
    assert obj.get_path_name().startswith(DEST+'/')
    assert eal.save_loaded_asset(obj, only_if_is_dirty=False)
    report['saved'].append(obj.get_path_name())


def region(shape, mask):
    def ring(points):
        result = unreal.FF_AtlasRegionRing()
        result.set_editor_property('points', [unreal.Vector2D(*p) for p in points])
        return result
    polygons = []
    for polygon in shape['polygons']:
        value = unreal.FF_AtlasRegionPolygon()
        value.set_editor_property('outer', ring(polygon['outer']))
        value.set_editor_property('holes', [ring(h) for h in polygon['holes']])
        polygons.append(value)
    value = unreal.FF_AtlasCountryRegion()
    for key, val in dict(country_id=shape['country_id'], polygons=polygons,
                         bounds=unreal.LinearColor(*shape['bounds']), mask=mask).items():
        value.set_editor_property(key, val)
    return value


def run():
    before = protected_hashes()
    report['protected_before'] = before
    manifest = json.loads((SRC/'World/manifest.json').read_text(encoding='utf-8'))
    validation = json.loads((SRC/'World/validation.json').read_text(encoding='utf-8'))
    assert validation['status'] == 'passed'
    entries = {entry['country_id']:entry for entry in manifest['countries']}
    assert set(entries) == set(ORDER[1:])
    assert 'ne_10m_admin_0_countries.zip' in (ROOT/'THIRD_PARTY_NOTICES.md').read_text(encoding='utf-8')
    names = [f'T_FF_AtlasRegion_{c}' for c in ORDER[1:]] + ['M_AtlasRegion_World','MI_AtlasRegion_World']
    assert all(not eal.does_asset_exist(DEST+'/'+n) for n in names), '기존 World 결과를 자동 덮어쓰지 않습니다.'
    data = unreal.load_asset(DEST+'/DA_AtlasCountryRegions')
    assert data and [str(r.country_id) for r in data.regions] == ['KR']
    backup = OUT/'Before-World-20260921/DA_AtlasCountryRegions.uasset'
    assert not backup.exists(), '최초 백업을 보존합니다.'
    backup.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(ROOT/'Content/FestaFiesta/UI/Atlas/CountryRegions/DA_AtlasCountryRegions.uasset', backup)
    regions = list(data.regions)
    for country in ORDER[1:]:
        entry = entries[country]
        shape = json.loads((SRC/'World'/entry['region_file']).read_text(encoding='utf-8'))
        assert shape['country_id'] == country
        texture_path = SRC/'World'/entry['texture_file']
        assert digest(texture_path) == entry['generated_files_sha256'][texture_path.name]
        task = unreal.AssetImportTask()
        for key, value in dict(filename=str(texture_path), destination_path=DEST,
                              destination_name=f'T_FF_AtlasRegion_{country}', automated=True,
                              replace_existing=False, save=False).items(): task.set_editor_property(key, value)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        mask = task.get_objects()[0]
        for key, value in dict(srgb=False, compression_settings=unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP,
                              address_x=unreal.TextureAddress.TA_CLAMP, address_y=unreal.TextureAddress.TA_CLAMP,
                              filter=unreal.TextureFilter.TF_TRILINEAR).items(): mask.set_editor_property(key, value)
        save(mask)
        regions.append(region(shape, mask))
    material = eal.duplicate_asset(DEST+'/M_AtlasRegion_Land', DEST+'/M_AtlasRegion_World')
    assert material
    report['material'] = build(material, regions)
    save(material)
    instance = eal.duplicate_asset(DEST+'/MI_AtlasRegion_Land', DEST+'/MI_AtlasRegion_World')
    assert instance
    mel.set_material_instance_parent(instance, material)
    save(instance)
    unreal.log('WORLD_REGION_IMPORT: 전체 지구 표면 데이터를 연결합니다.')
    surface = json.loads((SRC/'World/surface.json').read_text(encoding='utf-8'))
    assert surface['country_id'] == 'ALL' and len(surface['vertices']) == len(surface['weights']) == len(surface['uvs'])
    report['surface'] = {'vertices':len(surface['vertices']), 'triangles':len(surface['indices'])//3,
                         'sha256':digest(SRC/'World/surface.json')}
    data.set_editor_property('regions', regions)
    data.set_editor_property('selection_material', instance)
    data.set_editor_property('surface_vertices', [unreal.Vector(*p) for p in surface.pop('vertices')])
    uv_property = 'surface_u_vs' if hasattr(data, 'surface_u_vs') else 'surface_uvs'
    data.set_editor_property(uv_property, [unreal.Vector2D(*p) for p in surface.pop('uvs')])
    data.set_editor_property('surface_weights', surface.pop('weights'))
    data.set_editor_property('surface_indices', surface.pop('indices'))
    del surface
    gc.collect()
    save(data)
    assert protected_hashes() == before, '원본 지구·카탈로그·맵·한국 시범 에셋 변경 감지'
    report.update(status='passed', countries=ORDER, original_assets_unchanged=True,
                  region_data=data.get_path_name(), selection_material=instance.get_path_name())


if __name__ == '__main__':
    OUT.mkdir(parents=True, exist_ok=True)
    try: run()
    except Exception:
        report.update(status='failed', error=traceback.format_exc())
        unreal.log_error('WORLD_REGION_IMPORT_FAIL '+report['error'])
    (OUT/'import_world.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.SystemLibrary.quit_editor()
