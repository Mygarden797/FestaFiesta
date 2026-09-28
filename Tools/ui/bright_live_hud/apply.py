"""라이브 HUD 두 WBP만 명시적으로 마감한다. 셋업 HUD 및 다른 패널은 보존한다."""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import re
import shutil
import traceback
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = ROOT / 'Saved/BrightLiveHud'
OUT.mkdir(parents=True, exist_ok=True)
REPORT_PATH = OUT / 'apply-report.json'
REPORT = {'status': 'running'}
FOLDER = '/Game/FestaFiesta/UI/LiveHUD/'
TARGETS = [FOLDER+'WBP_FF_LiveHudScreen', FOLDER+'WBP_FF_LiveTabButton']

def path_for(package):
    return ROOT / 'Content' / (package[6:]+'.uasset')

def protected():
    paths = set((ROOT/'Content/FestaFiesta/UI/LiveHUD').rglob('*.uasset'))
    paths.update((ROOT/'Content/FestaFiesta/Building/FF_WBP_Building/Shared').glob('*.uasset'))
    paths -= {path_for(p) for p in TARGETS}
    return {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(paths)}

def settings_snapshot(cdo, names):
    # Unreal Python struct repr에는 매번 달라지는 래퍼 주소가 있다. 값 비교에서 제외한다.
    return {n: re.sub(r'0x[0-9A-Fa-f]+', '<address>', str(cdo.get_editor_property(n))) for n in names}

try:
    if REPORT_PATH.exists():
        assert json.loads(REPORT_PATH.read_text(encoding='utf-8')).get('status') != 'passed', '이미 적용했다. Designer 편집값을 보존한다.'
    before = protected()
    backup = OUT / ('Before-'+datetime.now().strftime('%Y%m%d-%H%M%S'))
    for package in TARGETS:
        src = path_for(package)
        dst = backup / src.relative_to(ROOT)
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
    REPORT['backup'] = str(backup)
    # 기존 클래스 설정은 트리와 별개다. 패널별 도킹과 클래스 참조를 전후 대조한다.
    bp = unreal.load_asset(TARGETS[0])
    cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(TARGETS[0]))
    names = ('panel_layouts', 'default_panel_layout', 'panel_classes', 'panel_class_folder', 'b_allow_panel_drag')
    # Python에서는 bool의 b_ 접두사를 제거한다.
    names = names[:-1]+('allow_panel_drag',)
    defaults_before = settings_snapshot(cdo,names)
    REPORT['settings_before'] = defaults_before
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.SystemLibrary.execute_console_command(world, 'FF.ApplyLiveHudBrightBottomDesign')
    bp = unreal.load_asset(TARGETS[0])
    tree = unreal.find_object(bp, 'WidgetTree')
    assert tree
    REPORT['row_heights'] = {}
    for name, expected in [('SizeBox_MenuRow',32.), ('SizeBox_StatusRow',36.)]:
        item = unreal.find_object(tree, name, unreal.SizeBox)
        assert item and item.get_editor_property('height_override') == expected, name
        REPORT['row_heights'][name] = expected
    for name in ('Tab_Operations','Tab_Facilities','Tab_Guests','Tab_Queues','Tab_SNS','Tab_Staff','Tab_Density',
                 'Tab_Report','Tab_Finale','Button_Speed1','Button_Speed2','Button_Speed4','Button_Cta','Panel_PanelHost'):
        assert unreal.find_object(tree,name), name
    cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(TARGETS[0]))
    REPORT['settings_after'] = settings_snapshot(cdo,names)
    assert defaults_before == REPORT['settings_after'], '기존 패널 설정 변경'
    assert protected() == before, '셋업 HUD 또는 다른 Live 패널 변경'
    REPORT.update(status='passed', protected_assets_unchanged=len(before), panel_settings_preserved=True)
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
    unreal.log_error(REPORT['error'])
finally:
    REPORT_PATH.write_text(json.dumps(REPORT,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('FF_LIVE_BRIGHT_APPLY '+REPORT['status'])
    unreal.SystemLibrary.quit_editor()
