"""태극기 내부 여백을 고친 PNG만 재임포트한다. 위젯과 다른 국기는 저장하지 않는다.

먼저 node Tools/ui/generate_frontend_casual_art.js --flag KR 를 실행한다.
이미 존재하는 에셋 경로를 그대로 사용하므로 목록과 상세 카드에 함께 반영된다.
"""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import shutil
import traceback
import unreal


ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
SOURCE = ROOT / 'SourceArt/UI/FrontendCasual/T_FF_Flag_KR.png'
ASSET = '/Game/FestaFiesta/UI/Frontend/Art/T_FF_Flag_KR'
FILE = ROOT / 'Content/FestaFiesta/UI/Frontend/Art/T_FF_Flag_KR.uasset'
OUT = ROOT / 'Saved/AtlasRegions/FlagPadding'
REPORT = {'status': 'running', 'saved': []}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def protected_hashes():
    files = list((ROOT / 'Content/FestaFiesta/UI/Frontend/Art').glob('*.uasset'))
    files += list((ROOT / 'Content/FestaFiesta/UI/Atlas').glob('*.uasset'))
    return {p.relative_to(ROOT).as_posix(): sha(p) for p in files if p != FILE}


try:
    OUT.mkdir(parents=True, exist_ok=True)
    assert SOURCE.is_file() and FILE.is_file()
    before = protected_hashes()
    REPORT['source_sha256'] = sha(SOURCE)
    REPORT['before_sha256'] = sha(FILE)
    backup = OUT / ('before_' + datetime.now().strftime('%Y%m%d_%H%M%S_%f') + '.uasset')
    shutil.copy2(FILE, backup)
    REPORT['backup'] = str(backup)
    task = unreal.AssetImportTask()
    for name, value in dict(filename=str(SOURCE), destination_path=ASSET.rsplit('/', 1)[0],
                            destination_name=ASSET.rsplit('/', 1)[1], automated=True,
                            replace_existing=True, save=False).items():
        task.set_editor_property(name, value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported = task.get_objects()
    assert len(imported) == 1 and isinstance(imported[0], unreal.Texture2D)
    texture = imported[0]
    assert texture.get_path_name() == ASSET + '.T_FF_Flag_KR'
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('srgb', True)
    assert unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
    REPORT['saved'].append(texture.get_path_name())
    REPORT['after_sha256'] = sha(FILE)
    REPORT['protected_assets_unchanged'] = protected_hashes() == before
    assert REPORT['protected_assets_unchanged']
    REPORT['status'] = 'passed'
except Exception:
    REPORT['status'] = 'failed'
    REPORT['error'] = traceback.format_exc()
    unreal.log_error(REPORT['error'])
finally:
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('FF_KOREA_FLAG_PADDING ' + REPORT['status'])
    unreal.SystemLibrary.quit_editor()
