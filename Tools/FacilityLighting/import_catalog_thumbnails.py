# import_catalog_thumbnails.py
# 역할: 시설 조명과 조명 제어 화면 작업에서 제작 결과를 Unreal 에셋으로 가져오고 연결한다.

"""Refresh the six lighting catalog textures using the existing facility format.

Run in a separate unattended Unreal Editor. Set FF_LIGHTING_VERIFY_ONLY=1 for
fresh-session readback without writing any assets. Definitions keep their
existing texture references; only the six Texture2D packages are saved.
"""
import hashlib
import json
import os
import struct
import sys
import traceback
from pathlib import Path

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'facilities'))
from create_buildable_lights import ROOT, DEFINITIONS, FIXTURES
from ffzs_asset_save_transaction import AssetSaveTransaction

OUT = ROOT / 'Saved/FacilityLighting_20260911'
SOURCE = ROOT / 'Content' / DEFINITIONS.removeprefix('/Game/') / 'Thumbnails'
REFERENCE = '/Game/Asset/FacilitiesAsset_HKH/Images/facilities/T_Interactive_Image/T_Interactive_Image'
PROPERTIES = ('srgb', 'compression_settings', 'compression_no_alpha', 'lod_group',
              'mip_gen_settings', 'never_stream', 'filter', 'address_x', 'address_y')


def run(verify_only=False):
    OUT.mkdir(parents=True, exist_ok=True)
    report = {'status': 'FAIL', 'verify_only': verify_only, 'textures': []}
    transaction = None
    try:
        reference = unreal.load_asset(REFERENCE)
        assert isinstance(reference, unreal.Texture2D), REFERENCE
        assert [reference.blueprint_get_size_x(), reference.blueprint_get_size_y()] == [1024, 1024]
        assert not reference.get_editor_property('compression_no_alpha')
        paths = [DEFINITIONS + '/Thumbnails/T_UI_Lighting_' + name for name, _, _ in FIXTURES]
        # Preflight all sources and references before touching a texture.
        sources = []
        for (name, minor, _), path in zip(FIXTURES, paths):
            source = SOURCE / ('T_UI_Lighting_' + name + '.png')
            raw = source.read_bytes()
            assert raw[:8] == b'\x89PNG\r\n\x1a\n', str(source)
            assert struct.unpack('>II', raw[16:24]) == (1024, 1024), str(source)
            assert raw[24:26] == bytes((8, 6)), 'Expected RGBA8 PNG: ' + str(source)
            definition = unreal.load_asset(DEFINITIONS + '/PDA_FFZS_Lighting_' + minor + '_' + name)
            assert definition, name
            thumbnail = definition.get_editor_property('catalog_thumbnail')
            assert thumbnail and thumbnail.get_path_name().split('.')[0] == path, name
            sources.append((source, hashlib.sha256(raw).hexdigest(), definition))
        if not verify_only:
            transaction = AssetSaveTransaction(paths, 'LightingCatalogThumbnails')
        textures = []
        for (name, _, _), path, (source, digest, definition) in zip(FIXTURES, paths, sources):
            if not verify_only:
                task = unreal.AssetImportTask()
                task.filename = str(source)
                task.destination_path = DEFINITIONS + '/Thumbnails'
                task.destination_name = path.rsplit('/', 1)[1]
                task.automated = True
                task.replace_existing = True
                task.replace_existing_settings = False
                task.save = False
                task.factory = unreal.TextureFactory()
                unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
                imported = [obj for obj in task.get_objects() if isinstance(obj, unreal.Texture2D)]
                assert len(imported) == 1 and imported[0].get_path_name().split('.')[0] == path, str(task.imported_object_paths)
            texture = unreal.load_asset(path)
            assert isinstance(texture, unreal.Texture2D), path
            if not verify_only:
                for prop in PROPERTIES:
                    texture.set_editor_property(prop, reference.get_editor_property(prop))
                unreal.EditorAssetLibrary.set_metadata_tag(texture, 'FF.LightingThumbnailSourceSHA256', digest)
                unreal.EditorAssetLibrary.set_metadata_tag(texture, 'FF.LightingThumbnailStyle', 'FacilityOrthographicRGBA1024')
            assert all(texture.get_editor_property(p) == reference.get_editor_property(p) for p in PROPERTIES), name
            assert [texture.blueprint_get_size_x(), texture.blueprint_get_size_y()] == [1024, 1024], name
            assert unreal.EditorAssetLibrary.get_metadata_tag(texture, 'FF.LightingThumbnailSourceSHA256') == digest, name
            assert definition.get_editor_property('catalog_thumbnail') == texture, name
            textures.append(texture)
            if verify_only:
                export = unreal.AssetExportTask()
                export.object = texture
                export.filename = str(OUT / ('Readback_T_UI_Lighting_' + name + '.png'))
                export.automated = True
                export.prompt = False
                export.replace_identical = True
                export.exporter = unreal.TextureExporterPNG()
                assert unreal.Exporter.run_asset_export_task(export), path
            report['textures'].append({'fixture': name, 'texture': path, 'source': str(source),
                                       'source_sha256': digest, 'size': [1024, 1024],
                                       'reference_settings_match': True, 'definition_reference_valid': True})
        if not verify_only:
            for texture in textures:
                assert unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False), texture.get_path_name()
            transaction.commit()
            transaction = None
        report.update(status='PASS', reference=REFERENCE,
                      settings={p: str(reference.get_editor_property(p)) for p in PROPERTIES},
                      saved_texture_count=0 if verify_only else len(textures))
        unreal.log('LIGHTING_CATALOG_THUMBNAILS_PASS ' + str(len(textures)))
    except Exception:
        if transaction:
            transaction.rollback()
        report['error'] = traceback.format_exc()
        raise
    finally:
        filename = 'thumbnail_readback.json' if verify_only else 'thumbnail_import.json'
        (OUT / filename).write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')


if __name__ == '__main__':
    run(verify_only=os.environ.get('FF_LIGHTING_VERIFY_ONLY') == '1')
