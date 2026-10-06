"""P7 스태프 패널과 Staff 탭 크기만 적용한다. PIE/렌더는 실행하지 않는다."""
from datetime import datetime
from pathlib import Path
import json
import shutil
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/LiveStaffSky'
OUT.mkdir(parents=True, exist_ok=True)
PANEL = '/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelStaff'
HUD = '/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveHudScreen'
KEY = 'FF.LiveStaffSky.Version'
ICONS = '/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Status/T_FF_LH_Icon_'
REPORT = dict(status='running', visual_check='사용자 에디터 확인 예정', changed_assets=[])


def defaults(path):
    return unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(path))


try:
    backup = OUT / ('Before-' + datetime.now().strftime('%Y%m%d-%H%M%S'))
    for path in (PANEL, HUD):
        source = ROOT / 'Content' / (path[6:] + '.uasset')
        assert source.exists(), '기존 에셋 없음: ' + path
        destination = backup / source.relative_to(ROOT)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
    REPORT['backup'] = str(backup)
    bp, hud_bp = unreal.load_asset(PANEL), unreal.load_asset(HUD)
    before = {str(k): v.export_text() for k, v in defaults(HUD).get_editor_property('panel_layouts').items()}
    was_applied = unreal.EditorAssetLibrary.get_metadata_tag(bp, KEY) == '1'
    unreal.SystemLibrary.execute_console_command(None, 'FF.ApplyLiveStaffSkyDesign')
    assert unreal.EditorAssetLibrary.get_metadata_tag(bp, KEY) == '1', '스태프 패널 제작 실패'
    tree = unreal.find_object(bp, 'WidgetTree')
    names = {w.get_name() for w in unreal.ObjectIterator(unreal.Widget) if w.get_outer() == tree}
    required = {'Button_Close', 'Text_TotalStaff', 'Text_StaffBreakdown', 'Text_AssignedStaff',
        'Text_IdleStaff', 'Text_IdleNote', 'Box_Assignments', 'Text_AssignmentsEmpty',
        'Text_DailyWage', 'Text_HireNote', 'Text_MissingNote', 'Text_Capacity', 'Text_BaseStaff',
        'Text_HiredStaff', 'Text_UnassignedStaff', 'Text_CapacityNote',
        'Progress_StaffCapacity', 'Progress_BaseCapacity', 'Image_ResponseStatus', 'Staff_Scroll_Details'}
    assert required <= names, '필수 위젯 누락: ' + str(required - names)
    REPORT['panel_bindings'] = sorted(required)
    if not was_applied:
        REPORT['changed_assets'].append(PANEL)
    properties = {'response_idle_icon': 'StatusCheck', 'response_active_icon': 'StatusPending',
        'response_unknown_icon': 'StatusWarning'}
    if unreal.EditorAssetLibrary.get_metadata_tag(bp, KEY + '.Icons') != '1':
        cdo = defaults(PANEL)
        for prop, name in properties.items():
            if not cdo.get_editor_property(prop):
                texture = unreal.load_asset(ICONS + name + '_White_256')
                assert texture, '기존 상태 아이콘 없음: ' + name
                cdo.set_editor_property(prop, texture)
        unreal.EditorAssetLibrary.set_metadata_tag(bp, KEY + '.Icons', '1')
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
    REPORT['response_icons'] = {prop: defaults(PANEL).get_editor_property(prop).get_path_name()
        for prop in properties}

    if unreal.EditorAssetLibrary.get_metadata_tag(hud_bp, KEY) != '1':
        cdo = defaults(HUD)
        table = dict(cdo.get_editor_property('panel_layouts'))
        if unreal.FF_LiveTab.STAFF not in table:
            layout = unreal.FF_LivePanelLayout()
            layout.set_editor_property('width', 1040.0)
            layout.set_editor_property('height', 660.0)
            current = cdo.get_editor_property('default_panel_layout')
            layout.set_editor_property('dock', current.get_editor_property('dock'))
            layout.set_editor_property('offset', current.get_editor_property('offset'))
            table[unreal.FF_LiveTab.STAFF] = layout
            cdo.set_editor_property('panel_layouts', table)
        unreal.EditorAssetLibrary.set_metadata_tag(hud_bp, KEY, '1')
        unreal.BlueprintEditorLibrary.compile_blueprint(hud_bp)
        assert unreal.EditorAssetLibrary.save_loaded_asset(hud_bp, only_if_is_dirty=False)
        REPORT['changed_assets'].append(HUD)
    after = {str(k): v.export_text() for k, v in defaults(HUD).get_editor_property('panel_layouts').items()}
    for key, value in before.items():
        assert after.get(key) == value, '기존 패널 편집값 변경: ' + key
    REPORT.update(status='passed', layouts=after)
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
    unreal.log_error(REPORT['error'])
finally:
    (OUT / 'apply-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.SystemLibrary.quit_editor()
