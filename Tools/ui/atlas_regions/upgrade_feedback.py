"""기존 한국 영역 재질만 백업 후 갱신한다. 지도/카메라/위젯 에셋은 저장하지 않는다."""
from pathlib import Path
from datetime import datetime
import hashlib
import json
import shutil
import sys
import traceback
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
sys.path.insert(0, str(ROOT/'Tools/ui/atlas_regions'))
from region_feedback import upgrade

REL = 'Content/FestaFiesta/UI/Atlas/CountryRegions/M_AtlasRegion_Land.uasset'
report = {'status': 'running', 'saved': []}


def hashes():
    return {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for folder in ['Content/FestaFiesta/UI/Atlas', 'Content/FestaFiesta/Maps', 'Content/FestaFiesta/UI/Lv_Test']
            for p in (ROOT/folder).rglob('*') if p.suffix in ('.uasset', '.umap')}


try:
    assert 'nullrhi' not in unreal.SystemLibrary.get_command_line().lower()
    before = hashes()
    backup = ROOT/'Saved/AtlasRegions/Backups'/datetime.now().strftime('Feedback-%Y%m%d-%H%M%S-%f')
    backup.mkdir(parents=True)
    shutil.copy2(ROOT/REL, backup/'M_AtlasRegion_Land.uasset')
    report['backup'] = str(backup)
    mat = unreal.load_asset('/Game/FestaFiesta/UI/Atlas/CountryRegions/M_AtlasRegion_Land')
    assert mat
    report['patch'] = upgrade(mat)
    unreal.MaterialEditingLibrary.layout_material_expressions(mat)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False)
    report['saved'].append(REL)
    after = hashes()
    changed = [k for k in before if before[k] != after[k]]
    assert set(changed).issubset({REL}), changed
    report.update(status='passed', changed=changed, other_assets_unchanged=True)
except Exception:
    report.update(status='failed', error=traceback.format_exc())
    unreal.log_error('ATLAS_REGION_FEEDBACK_FAIL '+report['error'])
(ROOT/'Saved/AtlasRegions/upgrade_feedback.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.SystemLibrary.quit_editor()
