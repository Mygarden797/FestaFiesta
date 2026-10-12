"""1단계 문구 수정: 기존 WBP 4개와 병목 선택지의 표시명만 갱신한다."""
from datetime import datetime
from pathlib import Path
import json
import shutil
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/UiCopyPhase1'
OUT.mkdir(parents=True, exist_ok=True)
PROMOTION = '/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelPromotion'
CHECK = '/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelCheck'
FACILITY = '/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityInfoPanel'
SYNERGY = '/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilitySynergyEntry'
INCIDENT = '/Game/FestaFiesta/Data/Incident/FF_ID_NPCCrowded'
PROMOTION_NOTICE = '홍보 효과는 예상 방문객에 반영됩니다. 실제 입장 인원은 운영 상황에 따라 달라집니다.'
ESTIMATE_NOTICE = '부스 매출과 미니게임 보상은 운영 결과에 따라 달라지므로 이 표에 넣지 않았습니다.'
INCIDENT_LABEL = '스태프 현장 출동'
REPLACEMENTS = {
    '예상 방문객에는 반영되며, 실제 입장 인원 연동은 준비 중입니다.': PROMOTION_NOTICE,
    '예상 방문객에는 반영되고, 실제 입장 인원 연동은 준비 중입니다.': PROMOTION_NOTICE,
    '예상 방문객에 반영됩니다. 실제 입장 인원 연동은 준비 중입니다.': PROMOTION_NOTICE,
    '부스 수수료와 미니게임 보상은 운영 결과에 따라 달라지므로 이 표에 넣지 않았습니다.': ESTIMATE_NOTICE,
    '부스 수수료와 미니게임 보상은 포함되지 않습니다.': '부스 매출과 미니게임 보상은 포함되지 않습니다.',
    '아직 결제가 없습니다. 손님이 이용을 마치면 수수료가 들어옵니다.':
        '아직 결제가 없습니다. 손님이 이용을 마치면 부스 매출이 들어옵니다.',
    'Burger 이용 수수료 +10%': 'Burger 부스 매출 +10%',
    '현재 수수료율·시설 강화·시너지를 반영한 팀별 예상 수익입니다.\n이용 완료 시 실제 적립된 수익이 오늘 매출에 누적됩니다.\n유료 프로그램 참가비는 별도로 정산됩니다.':
        '이용 요금·시설 강화·시너지를 반영한 팀별 예상 부스 매출입니다.\n이용 완료 시 실제 적립된 부스 매출이 오늘 매출에 누적됩니다.\n유료 프로그램 참가비는 별도로 정산됩니다.',
}
REPORT = {'status': 'running', 'changes': [], 'scope': '표시 문구만 수정, 배치·경제·사건 규칙 보존'}


def widgets(bp):
    tree = unreal.find_object(bp, 'WidgetTree')
    return {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget) if w.get_outer() == tree}


def layout(bp):
    result = {}
    for name, widget in widgets(bp).items():
        record = {'class': widget.get_class().get_path_name(),
                  'parent': widget.get_parent().get_name() if widget.get_parent() else None}
        if isinstance(widget, unreal.TextBlock):
            record['font'] = widget.get_editor_property('font').export_text()
            record['wrap'] = widget.get_editor_property('auto_wrap_text')
            record['color'] = widget.get_editor_property('color_and_opacity').export_text()
        result[name] = record
    return result


def change_text(obj, prop, context, replacements=REPLACEMENTS):
    old = str(obj.get_editor_property(prop))
    new = replacements.get(old, old)
    if old == new:
        return False
    obj.set_editor_property(prop, unreal.Text(new))
    REPORT['changes'].append({'target': context, 'property': prop, 'before': old, 'after': new})
    return True


try:
    assert not unreal.EditorLevelLibrary.get_pie_worlds(False), 'PIE 중에는 적용하지 않습니다.'
    paths = (PROMOTION, CHECK, FACILITY, SYNERGY, INCIDENT)
    assets = {p: unreal.load_asset(p) for p in paths}
    assert all(assets.values()), '대상 에셋을 찾지 못했습니다.'
    dirty = {p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    assert not dirty.intersection(paths), '대상에 미저장 변경이 있어 보존합니다.'
    backup = ROOT / 'Saved/UIBackups/CopyPhase1' / datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    backup.mkdir(parents=True)
    for package in paths:
        source = ROOT / 'Content' / (package[6:] + '.uasset')
        shutil.copy2(source, backup / source.name)
    REPORT['backup'] = str(backup)
    saved = []
    for package in paths[:-1]:
        bp = assets[package]
        before_layout = layout(bp)
        changed = False
        for name, widget in widgets(bp).items():
            if isinstance(widget, unreal.TextBlock):
                changed |= change_text(widget, 'text', package + ':' + name)
            changed |= change_text(widget, 'tool_tip_text', package + ':' + name)
        cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(package))
        if package == PROMOTION:
            changed |= change_text(cdo, 'not_applied_notice_text', package + ':CDO')
        elif package == CHECK:
            changed |= change_text(cdo, 'estimate_notice_text', package + ':CDO')
        if changed:
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
            assert layout(bp) == before_layout, '문구 외 구조·폰트·색상 변경: ' + package
            assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False), package
            saved.append(package)
        for widget in widgets(bp).values():
            if isinstance(widget, unreal.TextBlock):
                assert str(widget.get_editor_property('text')) not in REPLACEMENTS, widget.get_name()
            assert str(widget.get_editor_property('tool_tip_text')) not in REPLACEMENTS, widget.get_name()

    incident = assets[INCIDENT]
    options = incident.get_editor_property('options')
    assert len(options) == 1, '병목 선택지 구성이 달라졌습니다.'
    option = options[0]
    # 사용하지 않는 CostStaff 수치까지 바꾸지 않고 선택지 Label만 바꾼다.
    preserved_fields = ('cost_gold', 'cost_staff', 'min_staff', 'work_amount', 'on_resolved_effect')
    before_rules = {k: str(option.get_editor_property(k)) for k in preserved_fields}
    before_label = str(option.get_editor_property('label'))
    assert before_label in ('1', INCIDENT_LABEL), '수정된 선택지 표시명은 덮어쓰지 않습니다: ' + before_label
    if change_text(option, 'label', INCIDENT + ':Options[0]', {'1': INCIDENT_LABEL}):
        assert unreal.EditorAssetLibrary.save_loaded_asset(incident, only_if_is_dirty=False), INCIDENT
        saved.append(INCIDENT)
    assert before_rules == {k: str(option.get_editor_property(k)) for k in preserved_fields}
    assert str(option.get_editor_property('label')) == INCIDENT_LABEL
    promotion_cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(PROMOTION))
    check_cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(CHECK))
    assert str(promotion_cdo.get_editor_property('not_applied_notice_text')) == PROMOTION_NOTICE
    assert str(check_cdo.get_editor_property('estimate_notice_text')) in ('', ESTIMATE_NOTICE)
    REPORT.update(status='passed', saved_assets=saved, existing_widget_style_preserved=True,
                  incident_rules_preserved=True, incident_label=INCIDENT_LABEL,
                  promotion_notice=str(promotion_cdo.get_editor_property('not_applied_notice_text')))
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
    unreal.log_error('UI_COPY_PHASE1_FAIL: ' + REPORT['error'])
finally:
    (OUT / 'apply-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')

started = time.monotonic()


def finish(delta_time):
    if time.monotonic() - started < 2:
        return
    unreal.unregister_slate_post_tick_callback(exit_handle)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
exit_handle = unreal.register_slate_post_tick_callback(finish)
