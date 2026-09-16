"""확인한 P10 햇살 마감을 실제 Frontend에 적용하고 원래 배치를 대조한다."""
from pathlib import Path
import hashlib, json, sys, traceback
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = ROOT / 'Saved/BrightGlobe'
sys.path.insert(0, str(ROOT / 'Tools/ui/blender/bright_globe'))
import sunny_title

report = {'status': 'running', 'saved_assets': []}
def digest(path):
    with path.open('rb') as stream: return hashlib.file_digest(stream, 'sha256').hexdigest()
try:
    prep = json.loads((OUT / 'p10_prepare.json').read_text(encoding='utf-8'))
    assert prep['status'] == 'prepared' and prep['original_assets_unchanged']
    report['backup_directory'] = prep['backup_directory']
    for rel, expected in prep['hashes_before'].items():
        assert digest(ROOT / rel) == expected, '시험 중 기존 파일 변경됨: ' + rel
    capture = json.loads((OUT / 'sunny_review/02_sunny_title_final.json').read_text(encoding='utf-8'))
    process = json.loads((OUT / 'p10_tuned_capture.process.json').read_text(encoding='utf-8-sig'))
    assert capture['status'] == 'image_saved' and capture['task_complete']
    assert process['exit_code'] == process['script_error_count'] == 0
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert level.load_level('/Game/FestaFiesta/Maps/Frontend')
    before = sunny_title.snapshot()
    report['before'] = before
    report['settings'] = sunny_title.configure()
    after = sunny_title.snapshot()
    assert after == before, '카메라 또는 지리 배치가 변경됨'
    assert level.save_current_level()
    report['saved_assets'] = ['/Game/FestaFiesta/Maps/Frontend']
    assert level.load_level('/Game/FestaFiesta/Maps/Frontend')
    assert sunny_title.snapshot() == before
    report['after'] = after
    report['hashes_after'] = {rel: digest(ROOT / rel) for rel in prep['hashes_before']}
    report['unchanged_catalog_and_title'] = all(report['hashes_after'][r] == h
        for r,h in prep['hashes_before'].items() if not r.endswith('Frontend.umap'))
    assert report['unchanged_catalog_and_title']
    report['status'] = 'applied'
except Exception:
    report.update(status='failed', error=traceback.format_exc())
    unreal.log_error('BRIGHT_GLOBE_FAIL ' + report['error'])
(OUT / 'p10_frontend_apply.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
