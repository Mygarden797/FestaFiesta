"""프로그램 피드백 위젯을 추가하고 기존 편집 값 보존을 확인한다."""
from pathlib import Path
from datetime import datetime
import hashlib
import json
import shutil
import traceback
import time
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/PrepProgramFeedback'
OUT.mkdir(parents=True, exist_ok=True)
PANEL = '/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelProgram'
CARD = '/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepProgramCard'
CHOICE = '/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepProgramCatalogChoice'
PROTECTED = (
    ROOT / 'Content/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar.uasset',
    ROOT / 'Content/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelCheck.uasset',
)
REPORT = {'status': 'running', 'visual_check': '사용자 에디터 확인 예정'}

def widgets(bp):
    tree = unreal.find_object(bp, 'WidgetTree')
    return {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget) if w.get_outer() == tree}

def properties(widget):
    result = {'type': widget.get_class().get_name(), 'parent': widget.get_parent().get_name() if widget.get_parent() else None}
    for prop in ('font', 'text', 'brush', 'brush_color', 'widget_style', 'padding', 'height_override', 'width_override', 'min_desired_height'):
        try:
            value = widget.get_editor_property(prop)
            result[prop] = value.export_text() if hasattr(value, 'export_text') else str(value)
        except Exception:
            pass
    slot = widget.get_editor_property('slot')
    if slot:
        for prop in ('padding', 'size', 'horizontal_alignment', 'vertical_alignment'):
            try:
                value = slot.get_editor_property(prop)
                result['slot_' + prop] = value.export_text() if hasattr(value, 'export_text') else str(value)
            except Exception:
                pass
    return result

try:
    backup = OUT / ('Before-' + datetime.now().strftime('%Y%m%d-%H%M%S'))
    backup.mkdir(parents=True, exist_ok=True)
    for package in (PANEL, CARD, CHOICE):
        source = ROOT / 'Content' / (package[6:] + '.uasset')
        if source.exists(): shutil.copy2(source, backup / source.name)
    REPORT['backup'] = str(backup)
    protected_hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in PROTECTED}
    panel, card = unreal.load_asset(PANEL), unreal.load_asset(CARD)
    assert panel and card, '프로그램 패널 또는 카드 WBP 없음'
    panel_before = {name: properties(w) for name, w in widgets(panel).items()}
    card_before = {name: properties(w) for name, w in widgets(card).items()}
    world = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_world()
    unreal.SystemLibrary.execute_console_command(world, 'FF.ApplyPrepProgramFeedback')
    for bp in (panel, card):
        assert unreal.EditorAssetLibrary.get_metadata_tag(bp, 'FF_PrepProgramFeedbackVersion') == '1', '적용 버전 없음'
    after = widgets(panel)
    changed = {}
    for name, old in panel_before.items():
        if name == 'Box_CatalogChoices': continue  # 빈 VBox를 2열 UniformGrid로 교체.
        assert name in after, '원본 위젯 사라짐: ' + name
        new = properties(after[name])
        for key in set(old) | set(new):
            if old.get(key) == new.get(key): continue
            if (name == 'Text_FacilityNotice' or old['parent'] is None) and (key == 'parent' or key.startswith('slot_')):
                continue  # 안내 그룹 이동 및 새 토스트 캔버스의 원본 루트 슬롯만 허용.
            changed.setdefault(name, []).append(key)
    card_after = widgets(card)
    for name, old in card_before.items():
        if name not in card_after or properties(card_after[name]) != old:
            changed['card:' + name] = ['properties']
    assert not changed, '기존 편집 값 변경: ' + str(changed)
    for path, digest in protected_hashes.items():
        assert hashlib.sha256(Path(path).read_bytes()).hexdigest() == digest, '관련 없는 에셋 변경: ' + path
    required = ('Border_FacilityGuide', 'Text_FacilityGuideTitle', 'Box_CatalogChoices', 'Border_ScheduleToast',
        'Text_ScheduleToastTitle', 'Text_ScheduleToastDetail', 'Button_DismissScheduleToast',
        'Text_ProgramCompletion', 'Text_SummaryStatus', 'Text_ScheduleAction')
    assert all(name in after for name in required), '패널 바인딩 누락'
    assert after['Box_CatalogChoices'].get_class().get_name() == 'UniformGridPanel', '시설 바로가기 그리드 없음'
    assert all(name in card_after for name in ('Border_CompletionBadge', 'Text_CompletionBadge')), '카드 완료 표시 없음'
    choice = unreal.load_asset(CHOICE)
    assert choice, '시설 바로가기 WBP 없음'
    assert all(name in widgets(choice) for name in ('Button_Root', 'Text_Label', 'Text_Subtitle', 'Text_Chevron', 'Border_Attention'))
    REPORT.update(status='passed', preserved_panel_widgets=len(panel_before)-1,
        preserved_card_widgets=len(card_before), protected_assets=list(protected_hashes))
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
    unreal.log_error('PREP_PROGRAM_FEEDBACK_FAIL: ' + REPORT['error'])
finally:
    (OUT / 'apply-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    exit_after = time.monotonic() + 10.0
    def finish_after_startup(delta_time):
        if time.monotonic() < exit_after: return
        unreal.unregister_slate_post_tick_callback(exit_handle)
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    exit_handle = unreal.register_slate_post_tick_callback(finish_after_startup)
