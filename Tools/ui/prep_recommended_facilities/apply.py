"""기존 점검창에 권장 시설 안내를 추가한다. PIE/전체 검사 없이 대상 에셋만 확인한다."""
from pathlib import Path
from datetime import datetime
import hashlib
import json
import shutil
import traceback
import time
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/PrepRecommended'
OUT.mkdir(parents=True, exist_ok=True)
PANEL = '/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelCheck'
ROW = '/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepRecommendedFacilityRow'
HUD = ROOT / 'Content/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar.uasset'
REPORT = {'status': 'running', 'visual_check': '사용자 에디터 확인 예정'}

def widgets(bp):
    tree = unreal.find_object(bp, 'WidgetTree')
    return {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget) if w.get_outer() == tree}

def properties(widget):
    result = {'type': widget.get_class().get_name(), 'parent': widget.get_parent().get_name() if widget.get_parent() else None}
    for prop in ('font','text','brush','brush_color','widget_style','padding','height_override','width_override','min_desired_height'):
        try:
            value = widget.get_editor_property(prop)
            result[prop] = value.export_text() if hasattr(value, 'export_text') else str(value)
        except Exception:
            pass
    slot = widget.get_editor_property('slot')
    if slot:
        for prop in ('padding','size','horizontal_alignment','vertical_alignment'):
            try:
                value = slot.get_editor_property(prop)
                result['slot_'+prop] = value.export_text() if hasattr(value, 'export_text') else str(value)
            except Exception:
                pass
    return result

try:
    backup = OUT / ('Before-' + datetime.now().strftime('%Y%m%d-%H%M%S'))
    backup.mkdir(parents=True, exist_ok=True)
    for package in (PANEL, ROW):
        source = ROOT / 'Content' / (package[6:] + '.uasset')
        if source.exists(): shutil.copy2(source, backup / source.name)
    REPORT['backup'] = str(backup)
    hud_hash = hashlib.sha256(HUD.read_bytes()).hexdigest()
    bp = unreal.load_asset(PANEL)
    assert bp, '점검 WBP 없음'
    before = {name: properties(w) for name, w in widgets(bp).items()}
    for kind in ('Lighting','FirstAid','FireSafety'):
        package = '/Game/FestaFiesta/UI/PrepHUD/Icons/T_FF_PrepRecommended_' + kind
        if not unreal.EditorAssetLibrary.does_asset_exist(package):
            task = unreal.AssetImportTask()
            task.filename = str(ROOT / 'SourceArt/UI/PrepRecommended' / (kind + '.png'))
            task.destination_path = '/Game/FestaFiesta/UI/PrepHUD/Icons'
            task.destination_name = 'T_FF_PrepRecommended_' + kind
            task.automated = True
            task.save = False
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
            texture = unreal.load_asset(package)
            assert texture, '아이콘 임포트 실패: ' + kind
            texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
            texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
            texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
            texture.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
            texture.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
            unreal.EditorAssetLibrary.save_loaded_asset(texture)
    world = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_world()
    unreal.SystemLibrary.execute_console_command(world, 'FF.ApplyPrepRecommendedFacilities')
    assert unreal.EditorAssetLibrary.get_metadata_tag(bp, 'FF_PrepRecommendedFacilitiesVersion') == '1', '적용 버전 없음'
    after_widgets = widgets(bp)
    altered = [name for name, props in before.items() if name not in after_widgets or properties(after_widgets[name]) != props]
    assert not altered, '기존 위젯 속성 변경: ' + str(altered)
    assert hashlib.sha256(HUD.read_bytes()).hexdigest() == hud_hash, 'HUD 파일 변경'
    required = ('Border_RecommendedFacilities','VerticalBox_RecommendedRows','Text_RecommendedCount','Text_RecommendedSummary')
    assert all(name in after_widgets for name in required), '점검 바인딩 누락'
    rows = after_widgets['VerticalBox_RecommendedRows'].get_all_children()
    assert len(rows) == 8, '권장 시설 행 개수 오류'
    kinds = [str(w.get_editor_property('kind')) for w in rows]
    assert len(set(kinds)) == 8, '시설 종류 중복'
    for row in rows:
        assert row.get_editor_property('icon_brush').get_editor_property('resource_object'), '행 아이콘 누락'
    row_bp = unreal.load_asset(ROW)
    assert row_bp, '행 WBP 없음'
    assert all(name in widgets(row_bp) for name in ('Button_Place','Text_Title','Text_Description','Text_Action','Image_Icon'))
    REPORT.update(status='passed', preserved_widget_count=len(before), kinds=kinds, hud_preserved=True)
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
    unreal.log_error('PREP_RECOMMENDED_FAIL: ' + REPORT['error'])
finally:
    (OUT / 'apply-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    # 초기 슬레이트 작업이 남은 프레임에서 바로 종료하면 ModeManager가 먼저 소멸한다.
    exit_after = time.monotonic() + 10.0
    def finish_after_startup(delta_time):
        if time.monotonic() < exit_after:
            return
        unreal.unregister_slate_post_tick_callback(exit_handle)
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    exit_handle = unreal.register_slate_post_tick_callback(finish_after_startup)
