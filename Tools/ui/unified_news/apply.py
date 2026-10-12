"""기존 소식 도크의 위젯 편집을 보존하면서 유형 필터·배지만 적용한다."""
from pathlib import Path
import json
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/UnifiedNews'
OUT.mkdir(parents=True, exist_ok=True)
PACKAGES = (
    '/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LiveSnsFlyout',
    '/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LiveSnsFlyoutPostRow',
    '/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveUtilityDock',
)
REPORT = {'status': 'running', 'visual_check': '사용자 에디터 확인 예정'}


def widgets(bp):
    tree = unreal.find_object(bp, 'WidgetTree')
    return {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget) if w.get_outer() == tree}


def snapshot(widget):
    result = {'class': widget.get_class().get_name(),
              'parent': widget.get_parent().get_name() if widget.get_parent() else None}
    for prop in ('font', 'text', 'brush', 'brush_color', 'widget_style', 'padding',
                 'height_override', 'width_override'):
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
    assert all(assets), '소식 도크 원본 없음'
    before = [{name: snapshot(w) for name, w in widgets(bp).items()} for bp in assets]
    world = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_world()
    unreal.SystemLibrary.execute_console_command(world, 'FF.ApplyUnifiedNews')
    preserved = 0
    for index, bp in enumerate(assets):
        assert unreal.EditorAssetLibrary.get_metadata_tag(bp, 'FF_UnifiedNewsVersion') == '1', '통합 소식 적용 실패'
        after = widgets(bp)
        for name, original in before[index].items():
            assert name in after, '원본 위젯 누락: ' + name
            current = snapshot(after[name])
            for key in set(original) | set(current):
                if index == 0 and key == 'text' and name in (
                    'Text_PanelTitle', 'Text_Subtitle', 'Text_ReadPositionNote', 'Text_FilterPositive', 'Text_FilterNegative'):
                    continue
                if index == 0 and key == 'widget_style' and name.startswith('Button_Filter'):
                    continue
                if index == 1 and name == 'Text_Sentiment' and (key == 'parent' or key.startswith('slot_')):
                    continue
                if index == 2 and name == 'Text_SNSLabel' and key == 'text':
                    continue
                assert original.get(key) == current.get(key), '원본 편집 변경: ' + name + '.' + key
            preserved += 1
    panel = widgets(assets[0])
    assert str(panel['Text_PanelTitle'].get_editor_property('text')) == '소식'
    assert str(panel['Text_FilterPositive'].get_editor_property('text')) == 'SNS'
    assert str(panel['Text_FilterNegative'].get_editor_property('text')) == '운영 알림'
    panel_cdo = unreal.get_default_object(unreal.load_class(None, PACKAGES[0] + '.' + PACKAGES[0].rsplit('/', 1)[1] + '_C'))
    assert panel_cdo.get_editor_property('unified_news'), '운영 원본 연결 꺼짐'
    assert panel_cdo.get_editor_property('preserve_feed_while_closed'), '닫힌 동안 소식 구독 꺼짐'
    row = widgets(assets[1])
    for name in ('Image_Avatar', 'Border_Avatar', 'Border_Sentiment', 'Image_NewDot', 'Text_Author', 'Text_Body'):
        assert name in row, '소식 행 바인딩 누락: ' + name
    REPORT.update(status='passed', preserved_widgets=preserved, packages=list(PACKAGES))
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
    unreal.log_error('UNIFIED_NEWS_FAIL: ' + REPORT['error'])
finally:
    (OUT / 'apply-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    # 통합 실행 스크립트가 runpy로 부르면 종료는 그 스크립트가 담당한다.
    if not globals().get('FF_BATCH_APPLY', False):
        unreal.SystemLibrary.quit_editor()
