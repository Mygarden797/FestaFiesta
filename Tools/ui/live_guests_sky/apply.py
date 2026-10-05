"""P4 관객 패널·불편 행·관객 탭 크기만 적용. 렌더/PIE 검증은 하지 않는다."""
from datetime import datetime
from pathlib import Path
import json
import shutil
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/LiveGuestsSky'
OUT.mkdir(parents=True, exist_ok=True)
PANEL = '/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelGuests'
ROW = '/Game/FestaFiesta/UI/LiveHUD/Rows/WBP_FF_LiveNeedRow'
HUD = '/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveHudScreen'
ICONS = '/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/'
KEY = 'FF.LiveGuestsSky.Version'
REPORT = dict(status='running', visual_check='사용자 에디터 확인 예정', changed_assets=[])


def defaults(path):
    return unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(path))


def check_names(bp, required):
    tree = unreal.find_object(bp, 'WidgetTree')
    names = {w.get_name() for w in unreal.ObjectIterator(unreal.Widget) if w.get_outer() == tree}
    assert required <= names, '필수 위젯 누락: ' + str(required - names)
    return sorted(required)


try:
    backup = OUT / ('Before-' + datetime.now().strftime('%Y%m%d-%H%M%S'))
    for path in (PANEL, ROW, HUD):
        source = ROOT / 'Content' / (path[6:] + '.uasset')
        if source.exists():
            destination = backup / source.relative_to(ROOT)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
    REPORT['backup'] = str(backup)
    bp, hud_bp = unreal.load_asset(PANEL), unreal.load_asset(HUD)
    before = {str(k): v.export_text() for k, v in defaults(HUD).get_editor_property('panel_layouts').items()}
    was_applied = unreal.EditorAssetLibrary.get_metadata_tag(bp, KEY) == '1'
    unreal.SystemLibrary.execute_console_command(None, 'FF.ApplyLiveGuestsSkyDesign')
    assert unreal.EditorAssetLibrary.get_metadata_tag(bp, KEY) == '1', '관객 패널 제작 실패'
    row_bp = unreal.load_asset(ROW)
    assert row_bp, '불편 행 WBP 없음'
    REPORT['panel_bindings'] = check_names(bp, {'Button_Close', 'Text_VisitorCount', 'Text_VisitorNote',
        'Text_AdmittedCount', 'Text_AdmittedNote', 'Text_SampleCount', 'Text_Summary', 'Text_Empty',
        'Text_MissingNote', 'Box_Needs', 'Scroll_Needs'})
    REPORT['row_bindings'] = check_names(row_bp, {'Text_Name', 'Text_Percent', 'Text_Status',
        'Text_Remedy', 'Progress_Unmet', 'Border_Status', 'Image_NeedIcon'})
    if not was_applied:
        REPORT['changed_assets'] += [PANEL, ROW]

    if unreal.EditorAssetLibrary.get_metadata_tag(bp, KEY + '.Settings') != '1':
        cdo = defaults(PANEL)
        if not cdo.get_editor_property('row_widget_class'):
            cdo.set_editor_property('row_widget_class', unreal.EditorAssetLibrary.load_blueprint_class(ROW))
        unreal.EditorAssetLibrary.set_metadata_tag(bp, KEY + '.Settings', '1')
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
    assert defaults(PANEL).get_editor_property('row_widget_class'), '행 클래스 연결 없음'

    if unreal.EditorAssetLibrary.get_metadata_tag(row_bp, KEY + '.Icons') != '1':
        cdo = defaults(ROW)
        icons = dict(cdo.get_editor_property('need_icons'))
        paths = {
            unreal.FF_NeedType.HUNGER: 'Needs/T_FF_LH_Icon_NeedFood_White_256',
            unreal.FF_NeedType.THIRST: 'Facilities/T_FF_LH_Icon_FacilityDrink_White_256',
            unreal.FF_NeedType.RESTROOM: 'Needs/T_FF_LH_Icon_NeedRestroom_White_256',
            unreal.FF_NeedType.FATIGUE: 'Needs/T_FF_LH_Icon_NeedRest_White_256',
            unreal.FF_NeedType.STIMULATION: 'Needs/T_FF_LH_Icon_NeedEntertainment_White_256',
        }
        for need, path in paths.items():
            if not icons.get(need):
                icons[need] = unreal.load_asset(ICONS + path)
            assert icons[need], '기존 아이콘 없음: ' + path
        cdo.set_editor_property('need_icons', icons)
        unreal.EditorAssetLibrary.set_metadata_tag(row_bp, KEY + '.Icons', '1')
        unreal.BlueprintEditorLibrary.compile_blueprint(row_bp)
        assert unreal.EditorAssetLibrary.save_loaded_asset(row_bp, only_if_is_dirty=False)

    if unreal.EditorAssetLibrary.get_metadata_tag(hud_bp, KEY) != '1':
        cdo = defaults(HUD)
        table = dict(cdo.get_editor_property('panel_layouts'))
        if unreal.FF_LiveTab.GUESTS not in table:
            layout = unreal.FF_LivePanelLayout()
            layout.set_editor_property('width', 1100.0)
            layout.set_editor_property('height', 640.0)
            current = cdo.get_editor_property('default_panel_layout')
            layout.set_editor_property('dock', current.get_editor_property('dock'))
            layout.set_editor_property('offset', current.get_editor_property('offset'))
            table[unreal.FF_LiveTab.GUESTS] = layout
            cdo.set_editor_property('panel_layouts', table)
        unreal.EditorAssetLibrary.set_metadata_tag(hud_bp, KEY, '1')
        unreal.BlueprintEditorLibrary.compile_blueprint(hud_bp)
        assert unreal.EditorAssetLibrary.save_loaded_asset(hud_bp, only_if_is_dirty=False)
        REPORT['changed_assets'].append(HUD)
    after = {str(k): v.export_text() for k, v in defaults(HUD).get_editor_property('panel_layouts').items()}
    for key, value in before.items():
        assert after.get(key) == value, '기존 패널 편집값 변경: ' + key
    REPORT.update(status='passed', layouts=after,
        need_icon_count=len(defaults(ROW).get_editor_property('need_icons')))
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
    unreal.log_error(REPORT['error'])
finally:
    (OUT / 'apply-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.SystemLibrary.quit_editor()
