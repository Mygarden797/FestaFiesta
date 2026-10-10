"""추가 고용의 −/+ 정렬과 준비 HUD 스태프 탭 이름만 수정한다. PIE/테스트는 실행하지 않는다."""
from datetime import datetime
from pathlib import Path
import json
import shutil
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/StaffManagement'
OUT.mkdir(parents=True, exist_ok=True)
PANEL = '/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelStaff'
HUD = '/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar'
REPORT = {'status': 'running', 'scope': '추가 고용 −/+와 준비 HUD 스태프 탭만 수정',
          'visual_check': '사용자 다음 플레이에서 확인', 'cpp_build': '실행하지 않음'}


def widgets(bp):
    tree = unreal.find_object(bp, 'WidgetTree')
    return {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget) if w.get_outer() == tree}


def structure(bp):
    return {name: (w.get_class().get_path_name(), w.get_parent().get_name() if w.get_parent() else None)
            for name, w in widgets(bp).items()}


def hud_layouts():
    cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(HUD))
    return {str(k): v.export_text() for k, v in cdo.get_editor_property('panel_layouts').items()}


try:
    assert not unreal.EditorLevelLibrary.get_pie_worlds(False), 'PIE가 실행 중이면 적용하지 않습니다.'
    assets = {p: unreal.load_asset(p) for p in (PANEL, HUD)}
    dirty = {p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    assert not dirty.intersection(assets), '대상 WBP에 미저장 변경이 있어 보존합니다.'
    assert all(assets.values()), '대상 WBP가 없습니다.'
    before_structure = {p: structure(bp) for p, bp in assets.items()}
    before_layouts = hud_layouts()
    backup = ROOT / 'Saved/UIBackups/StaffManagementPolish' / datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    backup.mkdir(parents=True)
    for package in assets:
        source = ROOT / 'Content' / (package[6:] + '.uasset')
        shutil.copy2(source, backup / source.name)
    REPORT['backup'] = str(backup)
    panel = widgets(assets[PANEL])
    glyphs = {}
    for name in ('Button_Decrease', 'Button_Increase'):
        button = panel[name]
        label = button.get_child_at(0)
        assert isinstance(label, unreal.TextBlock), '스테퍼 글자 구조가 다릅니다: ' + name
        font = label.get_editor_property('font')
        font.set_editor_property('size', 20)
        font.set_editor_property('typeface_font_name', 'Medium')
        label.set_font(font)
        label.set_editor_property('justification', unreal.TextJustify.CENTER)
        label.set_editor_property('auto_wrap_text', False)
        slot = label.get_editor_property('slot')
        assert isinstance(slot, unreal.ButtonSlot), '글자가 버튼 슬롯에 없습니다: ' + name
        slot.set_padding(unreal.Margin(0, 0, 0, 0))
        slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
        slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        style = button.get_editor_property('widget_style')
        style.set_editor_property('normal_padding', unreal.Margin(0, 0, 0, 0))
        style.set_editor_property('pressed_padding', unreal.Margin(0, 0, 0, 0))
        button.set_style(style)
        glyphs[name] = {'widget': label.get_name(), 'font_size': 20, 'padding': 0, 'alignment': 'center'}
    tab = widgets(assets[HUD])['Tab_Staff']
    tab.set_editor_property('display_label_override', unreal.Text('스태프'))
    for package, bp in assets.items():
        # 트리 구조와 바인딩은 바꾸지 않고 수정한 템플릿 값을 생성 클래스에 반영한다.
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assert structure(bp) == before_structure[package], '기존 위젯 구조가 변경됐습니다: ' + package
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False), 'WBP 저장 실패: ' + package
    assert hud_layouts() == before_layouts, 'HUD 패널 위치·크기는 보존해야 합니다.'
    assert str(widgets(assets[HUD])['Tab_Staff'].get_editor_property('display_label_override')) == '스태프'
    for name in glyphs:
        button = widgets(assets[PANEL])[name]
        label = button.get_child_at(0)
        assert label.get_editor_property('font').size == 20
        slot = label.get_editor_property('slot')
        assert slot.get_editor_property('horizontal_alignment') == unreal.HorizontalAlignment.H_ALIGN_CENTER
        assert slot.get_editor_property('vertical_alignment') == unreal.VerticalAlignment.V_ALIGN_CENTER
        padding = button.get_editor_property('widget_style').normal_padding
        assert all(padding.get_editor_property(edge) == 0 for edge in ('left', 'top', 'right', 'bottom'))
    REPORT.update(status='passed', modified_assets=list(assets), stepper=glyphs,
                  tab_label='스태프', widget_structure_preserved=True, hud_layouts_preserved=True)
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
    unreal.log_error('STAFF_MANAGEMENT_POLISH_FAIL: ' + REPORT['error'])
finally:
    (OUT / 'polish-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')

started = time.monotonic()


def finish(delta_time):
    if time.monotonic() - started < 2:
        return
    unreal.unregister_slate_post_tick_callback(exit_handle)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()


if not globals().get('FF_BATCH_APPLY', False):
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    exit_handle = unreal.register_slate_post_tick_callback(finish)
