"""실제 PIE의 UI 핸들러 경로 검사. 물리 마우스/게임패드 입력 검사는 아니다.

start(bar) 후 에디터 tick마다 tick()을 호출한다. 완료 전 None, 완료 후 결과 dict.
외부 실행기가 전체 제한시간과 PIE 종료를 소유한다. 저장/경제 직접 변경은 하지 않는다.
"""

import traceback
import time
import unreal

_run = None
_result = None


def _property(obj, name):
    return obj.get_editor_property(name)


def _call(obj, name, *args):
    # UE 5.7 PyWrapperObject::CallMethod: Python glue 없는 UFUNCTION도 호출한다.
    return obj.call_method(name, args=tuple(args))


def _visible(obj):
    return obj.get_visibility() not in (
        unreal.SlateVisibility.COLLAPSED, unreal.SlateVisibility.HIDDEN)


def _panel(bar, cls):
    return next((obj for obj in unreal.ObjectIterator(cls)
                 if obj.get_world() == bar.get_world()
                 and obj.get_owning_bar() == bar and obj.get_parent() and _visible(obj)), None)


def _dialog(bar):
    return next((obj for obj in unreal.ObjectIterator(unreal.FF_ConfirmDialog)
                 if obj.get_world() == bar.get_world() and obj.is_activated()), None)


def _wait_dialog(bar):
    # CommonUI 스택 전환은 여러 프레임에 걸친다. 활성화 전 상태를 실패로 판정하지 않는다.
    deadline = time.monotonic() + 3
    while time.monotonic() < deadline:
        dialog = _dialog(bar)
        if dialog:
            return dialog
        yield
    raise AssertionError('3초 이내 확인창 활성화 실패')


def _open(bar, tab):
    selection = bar.get_selection()
    if not selection.has_selection or selection.tab != tab:
        bar.select_tab(tab)


def _snapshot(vm):
    return dict(gold=int(vm.get_gold()), staff=int(vm.get_total_staff_count()),
                promotion=str(vm.get_purchased_promotion()),
                stage=str(vm.get_current_stage()), day=int(vm.get_current_day()))


def _check(name, passed, **details):
    _result['checks'][name] = dict(status='passed' if passed else 'failed', **details)


def _require(value, message):
    if not value:
        raise AssertionError(message)


def _staff(bar, vm):
    _open(bar, unreal.FF_PrepHudTab.STAFF)
    yield
    panel = _panel(bar, unreal.FF_PrepPanelStaff)
    _require(panel, '실제 인력 패널을 찾지 못함')
    before = _snapshot(vm)
    first = panel.get_quote()
    _call(panel, 'HandleDecreaseClicked')
    lowered = int(panel.get_quote().selected_count)
    _call(panel, 'HandleIncreaseClicked')
    raised = int(panel.get_quote().selected_count)
    _check('staff_stepper', lowered == max(0, first.selected_count - 1)
           and raised == min(first.max_hireable, lowered + 1),
           original=int(first.selected_count), decreased=lowered, increased=raised)
    quote = panel.get_quote()
    _require(quote.selected_count > 0 and quote.hire_cost_total >= 0,
             '현재 원본 상태에서 고용 가능 인원/비용이 없음')
    _require(_property(panel, 'Button_Hire').get_is_enabled(), '실제 고용 버튼 비활성')
    expected_gold = before['gold'] - int(quote.hire_cost_total)
    expected_staff = before['staff'] + int(quote.selected_count)
    _call(panel, 'HandleHireClicked')
    yield
    after = _snapshot(vm)
    _check('staff_hire_result', after['gold'] == expected_gold
           and after['staff'] == expected_staff,
           before=before, after=after, expected_gold=expected_gold, expected_staff=expected_staff)
    _call(panel, 'HandleCloseClicked')
    yield
    _check('staff_close', not bar.get_selection().has_selection)


def _promotion(bar, vm):
    _open(bar, unreal.FF_PrepHudTab.PROMOTION)
    yield
    panel = _panel(bar, unreal.FF_PrepPanelPromotion)
    _require(panel, '실제 홍보 패널을 찾지 못함')
    before = _snapshot(vm)
    options = list(vm.get_promotion_options())
    cards = _property(panel, 'Box_PromotionCards')
    selected = None
    # 패널이 GetPromotionOptions 순서로 만든 카드의 실제 클릭 델리게이트를 거친다.
    for index, option in enumerate(options):
        if option.cost_gold > 0 and option.cost_gold <= before['gold']:
            card = cards.get_child_at(index)
            _require(card and _property(card, 'Button_Root').get_is_enabled(),
                     '구매 가능한 홍보 카드가 비활성')
            _call(card, 'HandleClicked')
            selected = option
            break
    _require(selected, '현재 잔액에서 구매 가능한 홍보 등급이 없음')
    quote = panel.get_quote()
    _check('promotion_card_selection', quote.choice == selected.choice,
           choice=str(quote.choice), cost=int(quote.cost_gold))
    _require(_property(panel, 'Button_Purchase').get_is_enabled(), '홍보 구매 버튼 비활성')
    _call(panel, 'HandlePurchaseClicked')
    dialog = yield from _wait_dialog(bar)
    _require(dialog, '홍보 구매 확인창이 실제 CommonUI 스택에 올라오지 않음')
    _call(dialog, 'HandleButtonClicked', _property(dialog, 'Button_Cancel'))
    # 취소 애니메이션 동안에는 실제 사용자의 다음 버튼도 아직 조작할 수 없다.
    cancelled_at = time.monotonic()
    while time.monotonic() - cancelled_at < .35:
        yield
    cancelled = _snapshot(vm)
    _check('promotion_cancel_unchanged', cancelled == before and _dialog(bar) is None,
           before=before, after=cancelled)
    _call(panel, 'HandlePurchaseClicked')
    dialog = yield from _wait_dialog(bar)
    _require(dialog, '취소 후 확인창 재열기 실패')
    button = _property(dialog, 'Button_Confirm')
    _call(dialog, 'HandleButtonClicked', button)
    # 같은 인스턴스의 두 번째 입력은 bResolved가 막아야 한다.
    _call(dialog, 'HandleButtonClicked', button)
    yield
    after = _snapshot(vm)
    _check('promotion_purchase_once', after['gold'] == before['gold'] - int(quote.cost_gold)
           and vm.get_purchased_promotion() == selected.choice and _dialog(bar) is None,
           before=before, after=after, cost=int(quote.cost_gold))
    _check('promotion_purchase_disabled', not _property(panel, 'Button_Purchase').get_is_enabled())
    _call(panel, 'HandleCloseClicked')
    yield
    _check('promotion_close', not bar.get_selection().has_selection)


def _inspection(bar, vm):
    _open(bar, unreal.FF_PrepHudTab.CHECK)
    yield
    panel = _panel(bar, unreal.FF_PrepPanelCheck)
    _require(panel, '실제 점검 패널을 찾지 못함')
    state = bar.get_cta_state()
    reason = str(bar.get_cta_reason())
    before = _snapshot(vm)
    gate = _property(panel, 'Button_OpenGate')
    if state == unreal.FF_PrepCtaState.BLOCKED_NO_FACILITY:
        _check('required_gate_disabled', not gate.get_is_enabled() and bool(reason),
               state=str(state), reason=reason)
        # 비활성 상태에서도 공용 명령의 방어 검사를 확인한다. 시설/Stage를 주입하지 않는다.
        _call(panel, 'HandleOpenGateClicked')
        yield
        _check('required_gate_keeps_stage', _snapshot(vm) == before,
               before=before, after=_snapshot(vm))
    else:
        _result['checks']['required_gate_disabled'] = dict(
            status='not_applicable', state=str(state), reason=reason,
            detail='현재 원본 상태가 필수시설 차단 상태가 아니므로 백엔드 조건을 조작하지 않음')
    _call(panel, 'HandleContinueBuildClicked')
    yield
    selection = bar.get_selection()
    _check('check_continue_build', selection.has_selection
           and selection.tab == unreal.FF_PrepHudTab.BUILD,
           tab=str(selection.tab), has_selection=bool(selection.has_selection))
    _open(bar, unreal.FF_PrepHudTab.CHECK)
    yield
    panel = _panel(bar, unreal.FF_PrepPanelCheck)
    _require(panel, '배치에서 점검 복귀 실패')
    _call(panel, 'HandleCloseClicked')
    yield
    _check('check_close', not bar.get_selection().has_selection)


def _steps(bar):
    instance = unreal.GameplayStatics.get_game_instance(bar.get_world())
    vm = next((obj for obj in unreal.ObjectIterator(unreal.FF_FestivalViewModelSubsystem)
               if obj.get_outer() == instance), None)
    _require(vm, '실제 PIE GameInstance의 ViewModel이 없음')
    _result['before'] = _snapshot(vm)
    for name, exercise in (('staff', _staff), ('promotion', _promotion), ('check', _inspection)):
        try:
            yield from exercise(bar, vm)
        except Exception:
            _check(name + '_execution', False, error=traceback.format_exc())
            # 실패한 확인창이 다음 검사 입력을 잡지 않도록 정상 취소 경로를 쓴다.
            dialog = _dialog(bar)
            if dialog:
                _call(dialog, 'HandleButtonClicked', _property(dialog, 'Button_Cancel'))
            bar.close_current_panel()
            yield
    _result['after'] = _snapshot(vm)


def start(bar):
    """PIE 준비 HUD가 연결된 뒤 한 번 호출한다."""
    global _run, _result
    if _run is not None:
        raise RuntimeError('이미 실행 중인 UI 동작 검사가 있습니다')
    _result = dict(status='running', input_method='reflected_ui_handlers', checks={})
    _run = _steps(bar)


def tick():
    """한 번의 프레임 경계를 진행하고, 끝났을 때만 결과를 반환한다."""
    global _run
    if _run is None:
        return _result
    try:
        next(_run)
        return None
    except StopIteration:
        _run = None
        _result['status'] = ('failed' if any(check['status'] == 'failed'
                             for check in _result['checks'].values()) else 'passed')
        return _result
    except Exception:
        _run = None
        _result.update(status='failed', error=traceback.format_exc())
        return _result
