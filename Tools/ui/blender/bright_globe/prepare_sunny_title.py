"""P10 원본 Frontend를 백업하고 별도 시험 레벨에 햇살 마감을 저장한다."""
from datetime import datetime
from pathlib import Path
import hashlib, json, shutil, sys, traceback
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
sys.path.insert(0, str(ROOT / 'Tools/ui/blender/bright_globe'))
import sunny_title
from sunny_water import build_water

OUT = ROOT / 'Saved/BrightGlobe'
REPORT = OUT / 'p10_prepare.json'
STUDY = sunny_title.BASE + '/L_BrightGlobe_SunnyStudy'
FILES = ['Content/FestaFiesta/Maps/Frontend.umap',
         'Content/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas.uasset',
         'Content/FestaFiesta/UI/Title/GlassOrbit/WBP_TitleMain_MyCasual.uasset']
report = {'status': 'running', 'frontend_modified': False}
def digest(path):
    with path.open('rb') as stream: return hashlib.file_digest(stream, 'sha256').hexdigest()
try:
    assert not REPORT.exists(), '시험 준비 기록이 이미 있음: 부분 작업 상태부터 확인'
    backup = ROOT / 'Saved/BrightGlobeBefore' / ('P10-' + datetime.now().strftime('%Y%m%d-%H%M%S'))
    hashes = {}
    for rel in FILES:
        src, dst = ROOT / rel, backup / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
        hashes[rel] = digest(src)
        assert digest(dst) == hashes[rel]
    report.update(backup_directory=str(backup), hashes_before=hashes)
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    sunny_title.build_background()
    sunny_title.build_land()
    water = build_water(sunny_title.DEST)
    assert water
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert not unreal.EditorAssetLibrary.does_asset_exist(STUDY)
    assert level.new_level_from_template(STUDY, '/Game/FestaFiesta/Maps/Frontend')
    before = sunny_title.snapshot()
    report['settings'] = sunny_title.configure()
    assert sunny_title.snapshot() == before, '카메라/지리 배치가 변경됨'
    assert level.save_current_level()
    report.update(status='prepared', study=STUDY, preserved=before,
                  original_assets_unchanged=all(digest(ROOT/r) == h for r,h in hashes.items()))
    assert report['original_assets_unchanged']
except Exception:
    report.update(status='failed', error=traceback.format_exc())
    unreal.log_error('BRIGHT_GLOBE_FAIL ' + report['error'])
REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
