"""P10 첫 캡처 검토 후 파랑과 초록의 대비를 미세 조정한다."""
from pathlib import Path
import json, sys, traceback
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
sys.path.insert(0, str(ROOT / 'Tools/ui/blender/bright_globe'))
import sunny_title
mel = unreal.MaterialEditingLibrary
report = {'status': 'running'}
try:
    changes = {'Backdrop': {'Sky': (.025, .21, .88)},
               'Land': {'GrassColor': (.33, .66, .12), 'ForestColor': (.10, .42, .11)},
               'Water': {'OceanColor': (.003, .155, .90), 'ShallowColor': (.025, .64, .90)}}
    for label, values in changes.items():
        mi = unreal.load_asset(sunny_title.DEST + '/MI_BrightGlobe_Sunny' + label)
        assert mi
        for name, rgb in values.items():
            mel.set_material_instance_vector_parameter_value(mi, name, unreal.LinearColor(*rgb, 1.))
            actual = mel.get_material_instance_vector_parameter_value(mi, name)
            assert max(abs(a-b) for a,b in zip((actual.r,actual.g,actual.b),rgb)) < 1.e-5
        sunny_title.save(mi)
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert level.load_level(sunny_title.BASE + '/L_BrightGlobe_SunnyStudy')
    before = sunny_title.snapshot()
    report['settings'] = sunny_title.configure()
    assert sunny_title.snapshot() == before
    assert level.save_current_level()
    report.update(status='tuned', palette=changes)
except Exception:
    report.update(status='failed', error=traceback.format_exc())
    unreal.log_error('BRIGHT_GLOBE_FAIL ' + report['error'])
(ROOT/'Saved/BrightGlobe/p10_tune.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
