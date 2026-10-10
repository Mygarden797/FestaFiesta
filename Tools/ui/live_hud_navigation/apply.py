"""기존 라이브 HUD 편집을 보존하면서 명칭 변경과 매출 순위 탭을 추가한다."""
from pathlib import Path
import json
import re
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/LiveHudNavigation'
OUT.mkdir(parents=True, exist_ok=True)
FOLDER = '/Game/FestaFiesta/UI/LiveHUD/'
PACKAGES = (FOLDER + 'WBP_FF_LiveHudScreen', FOLDER + 'Panels/WBP_FF_LivePanelGuests',
            FOLDER + 'Panels/WBP_FF_LivePanelFacilities')
REPORT = {'status': 'running', 'visual_check': '사용자 에디터 확인 예정'}


def widgets(bp):
    tree = unreal.find_object(bp, 'WidgetTree')
    return {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget) if w.get_outer() == tree}


def snapshot(widget):
    result = {'class': widget.get_class().get_name(),
              'parent': widget.get_parent().get_name() if widget.get_parent() else None}
    for prop in ('font', 'text', 'brush', 'brush_color', 'widget_style', 'padding',
                 'height_override', 'width_override', 'display_label_override', 'designer_icon'):
        try:
            value = widget.get_editor_property(prop)
            result[prop] = value.export_text() if hasattr(value, 'export_text') else str(value)
        except Exception:
            pass
    slot = widget.get_editor_property('slot')
    if slot:
        for prop in ('padding', 'size', 'horizontal_alignment', 'vertical_alignment', 'layout'):
            try:
                value = slot.get_editor_property(prop)
                result['slot_' + prop] = value.export_text() if hasattr(value, 'export_text') else str(value)
            except Exception:
                pass
    return result


try:
    assets = [unreal.load_asset(p) for p in PACKAGES]
    assert all(assets), '라이브 HUD 원본 없음'
    before = [{name: snapshot(w) for name, w in widgets(bp).items()} for bp in assets]
    world = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_world()
    unreal.SystemLibrary.execute_console_command(world, 'FF.ApplyLiveHudNavigation')
    preserved = 0
    for index, bp in enumerate(assets):
        assert unreal.EditorAssetLibrary.get_metadata_tag(bp, 'FF_LiveHudNavigationVersion') == '1', '마이그레이션 실패'
        after = widgets(bp)
        for name, original in before[index].items():
            assert name in after, '원본 위젯 누락: ' + name
            current = snapshot(after[name])
            for key in set(original) | set(current):
                if index == 0 and name == 'Tab_Guests' and key == 'display_label_override':
                    continue
                if index > 0 and name == 'Text_PanelTitle' and key == 'text':
                    continue
                assert original.get(key) == current.get(key), '원본 편집 변경: ' + name + '.' + key
            preserved += 1
    hud = widgets(assets[0])
    assert 'Tab_RevenueRanking' in hud, '매출 순위 탭 누락'
    tabs = hud['Tab_Facilities'].get_parent()
    assert tabs.get_child_index(hud['Tab_RevenueRanking']) == tabs.get_child_index(hud['Tab_Facilities']) + 1
    assert str(hud['Tab_Guests'].get_editor_property('display_label_override')) == '현황'
    assert str(widgets(assets[1])['Text_PanelTitle'].get_editor_property('text')) == '현황'
    assert str(widgets(assets[2])['Text_PanelTitle'].get_editor_property('text')) == '시설 상태'
    REPORT.update(status='passed', preserved_widgets=preserved, packages=list(PACKAGES))
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
    unreal.log_error('LIVE_HUD_NAVIGATION_FAIL: ' + REPORT['error'])
finally:
    (OUT / 'apply-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    # 같은 일회용 에디터에서 이번 입력 경로·라이브 탭 규칙만 확인한다.
    expected = {'FestaFiesta.UI.OutsideClickDismiss.PointerPath', 'FestaFiesta.UI.LiveHud.LiveStageOnly'}
    check_start = time.monotonic()
    log_path = ROOT / 'Saved/BrightGlobe/live_hud_navigation_apply.log'
    if REPORT['status'] == 'passed':
        unreal.SystemLibrary.execute_console_command(world, 'Automation RunTests ' + '+'.join(sorted(expected)))

    def finish_after_startup(delta_time):
        elapsed = time.monotonic() - check_start
        if elapsed < 10:
            return
        if REPORT['status'] == 'passed':
            text = log_path.read_text(encoding='utf-8', errors='replace') if log_path.exists() else ''
            matches = re.findall(r'Test Completed\. Result=\{([^}]+)\}.*?Path=\{([^}]+)\}', text)
            results = {name: state for state, name in matches if name in expected}
            if not expected.issubset(results) and elapsed < 90:
                return
            REPORT['tests'] = results
            if not expected.issubset(results) or any(state != 'Success' for state in results.values()):
                REPORT.update(status='failed', error='입력 경로 또는 라이브 탭 검사 실패/시간 초과')
                unreal.log_error('LIVE_HUD_NAVIGATION_FAIL: ' + REPORT['error'])
            (OUT / 'apply-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
        unreal.unregister_slate_post_tick_callback(exit_handle)
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()

    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    exit_handle = unreal.register_slate_post_tick_callback(finish_after_startup)
