"""매출 순위 패널/행만 중앙 HUD 스타일로 바꾸고 기존 위젯 연결을 확인한다."""
from pathlib import Path
import json
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/RevenueSky'
OUT.mkdir(parents=True, exist_ok=True)
FOLDER = '/Game/FestaFiesta/Building/FF_WBP_Building/FacilityRevenue/'
PACKAGES = [FOLDER + 'WBP_FF_FacilityRevenueRankRow', FOLDER + 'WBP_FF_FacilityRevenueRanking']
REPORT = {'status': 'running', 'visual_check': '사용자 에디터 확인 예정'}


def widgets(bp):
    tree = unreal.find_object(bp, 'WidgetTree')
    return {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget) if w.get_outer() == tree}


try:
    assets = [unreal.load_asset(p) for p in PACKAGES]
    assert all(assets), '기존 매출 순위 에셋 없음'
    before = [{name: w.get_class().get_path_name() for name, w in widgets(bp).items()} for bp in assets]
    world = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_world()
    unreal.SystemLibrary.execute_console_command(world, 'FF.ApplyFacilityRevenueSky')
    preserved = 0
    for bp, original in zip(assets, before):
        assert unreal.EditorAssetLibrary.get_metadata_tag(bp, 'FF.FacilityRevenue.Sky20261010') == '1', '적용 실패'
        after = widgets(bp)
        for name, class_name in original.items():
            assert name in after and after[name].get_class().get_path_name() == class_name, '기존 위젯 소실/타입 변경: ' + name
            preserved += 1
    panel = widgets(assets[1])
    row = widgets(assets[0])
    assert str(panel['Text_Title'].get_editor_property('text')) == '매출 순위'
    assert str(panel['Sky_Text_Unit'].get_editor_property('text')) == '금액 단위: 금화'
    anchors = panel['Size_ClarityFrame'].get_editor_property('slot').get_anchors()
    assert anchors.minimum.x == anchors.maximum.x == 0.5
    assert anchors.minimum.y == anchors.maximum.y == 0.5
    assert panel['Size_ClarityFrame'].get_editor_property('width_override') == 1060.0
    assert panel['Size_ClarityFrame'].get_editor_property('height_override') == 690.0
    assert row['Text_Share'].get_parent().get_name() == 'Sky_Column_Share'
    assert 'Text_SelectionMark' in row
    assert panel['Button_Inspect'].get_parent().get_name() == 'Size_Clarity_Button_Inspect'
    assert 'WBP_FF_FacilityRevenueRankRow_C' in panel['List_Ranking'].get_editor_property('entry_widget_class').get_name()
    REPORT.update(status='passed', preserved_widgets=preserved, packages=PACKAGES,
                  retained='기존 위젯 이름/타입, 목록 행 클래스, 선택 시설 정보 버튼, 처음 사용 안내')
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
    unreal.log_error('REVENUE_SKY_FAIL: ' + REPORT['error'])
finally:
    (OUT / 'apply-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    if not globals().get('FF_BATCH_APPLY', False):
        started = time.monotonic()

        def finish(delta_time):
            if time.monotonic() - started < 5:
                return
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.SystemLibrary.quit_editor()

        handle = unreal.register_slate_post_tick_callback(finish)
