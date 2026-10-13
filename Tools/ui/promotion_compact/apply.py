"""기존 홍보 두 에셋만 간소화한다. PIE·자동화 테스트는 실행하지 않는다."""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import shutil
import time
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/PromotionCompact'
OUT.mkdir(parents=True, exist_ok=True)
PANEL = '/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelPromotion'
CARD = '/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepPromotionCard'
HUD = '/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar'
PACKAGES = (PANEL, CARD)
COMMAND = 'FF.ApplyPrepPromotionCompactDesign'
SIZE_FIELDS = (
    'width_override', 'height_override', 'min_desired_width', 'min_desired_height',
    'max_desired_width', 'max_desired_height', 'min_aspect_ratio', 'max_aspect_ratio',
    'override_width_override', 'override_height_override', 'override_min_desired_width',
    'override_min_desired_height', 'override_max_desired_width', 'override_max_desired_height',
    'override_min_aspect_ratio', 'override_max_aspect_ratio',
)
HEIGHT_FIELDS = ('height_override', 'min_desired_height', 'max_desired_height',
                 'override_height_override', 'override_min_desired_height', 'override_max_desired_height')
REPORT = {'status': 'running', 'command': COMMAND, 'saved_assets': [],
          'verification': '에셋 구조·기존 치수·재실행 보존 확인. 실제 화면은 사용자 에디터 확인 예정.'}
PROTECTED = {}


def asset_file(package):
    return ROOT / 'Content' / (package[6:] + '.uasset')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def widgets(bp):
    tree = unreal.find_object(bp, 'WidgetTree')
    assert tree, 'WidgetTree 누락: ' + bp.get_name()
    items = [widget for widget in unreal.ObjectIterator(unreal.Widget) if widget.get_outer() == tree]
    result = {widget.get_name(): widget for widget in items}
    assert len(result) == len(items), '중복 위젯 이름: ' + bp.get_name()
    return result


def values(widget, names):
    return {name: widget.get_editor_property(name) for name in names}


def child_names(widget):
    return [child.get_name() for child in widget.get_all_children()]


def execute():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.SystemLibrary.execute_console_command(world, COMMAND)


def verify(original, original_sizes):
    result = {}
    for package in PACKAGES:
        bp = unreal.load_asset(package)
        compiled = unreal.EditorAssetLibrary.load_blueprint_class(package)
        assert bp and compiled, '컴파일된 홍보 클래스 누락: ' + package
        current = widgets(bp)
        for name, widget in original[package].items():
            assert current.get(name) == widget, '기존 위젯 인스턴스 변경: ' + name
        result[package] = current
    panel, card = result[PANEL], result[CARD]
    defaults = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(PANEL))
    assert defaults.get_editor_property('use_compact_layout'), '패널 간소화 옵션 미적용'
    body = panel['VerticalBox_Body']
    assert child_names(body)[:4] == ['Text_OnceNotice', 'Text_ForecastBasis',
                                    'Box_PromotionCards', 'Text_NotAppliedNotice'], '본문 순서 불일치'
    assert isinstance(panel.get('Text_ForecastBasis'), unreal.TextBlock), '공통 입장료 기준 행 누락'
    assert panel['Text_ForecastBasis'].get_parent() == body, '공통 기준 행 부모 불일치'
    assert child_names(panel['VerticalBox_Footer'])[:2] == ['HorizontalBox_Actions',
                                                         'Border_PurchaseSummary'], '상태 안내·요약 순서 불일치'
    summary = panel['HorizontalBox_PurchaseSummary']
    assert panel['SizeBox_PurchaseAction'].get_parent() == summary, '구매 버튼의 요약 행 이동 실패'
    assert panel['VerticalBox_GoldAfter'].get_parent() == summary, '잔액 그룹 위치 불일치'
    assert panel['Text_BlockReason'].get_parent() == panel['HorizontalBox_Actions'], '실패 안내 누락'
    assert panel['Text_PurchaseAction'].get_parent() == panel['Button_Purchase'], '구매 라벨 바인딩 위치 불일치'
    for name in ('VerticalBox_SelectedPromotion', 'VerticalBox_PurchaseCost', 'Border_SummaryDivider_Size'):
        assert panel[name].get_visibility() == unreal.SlateVisibility.COLLAPSED, '중복 그룹 미축소: ' + name
    assert card['Text_Effect'].get_visibility() == unreal.SlateVisibility.COLLAPSED, '배율 행 미축소'
    column = card['VerticalBox_Card']
    order = child_names(column)
    assert card['Text_Forecast'].get_parent() == column and card['Text_Buzz'].get_parent() == column
    assert order.index('Text_Forecast') < order.index('Text_Buzz'), '방문객·화제성 순서 불일치'
    assert values(card['SizeBox_Card'], SIZE_FIELDS) == original_sizes['card'], '기존 카드 치수 변경'
    assert values(panel['SizeBox_PurchaseAction'], HEIGHT_FIELDS) == original_sizes['purchase_height'], '기존 구매 버튼 높이 변경'
    return {'body_order': child_names(body), 'card_order': order,
            'panel_widget_count': len(panel), 'card_widget_count': len(card),
            'existing_widgets_preserved': sum(len(items) for items in original.values()),
            'compact_layout': True}


try:
    assert not unreal.EditorLevelLibrary.get_pie_worlds(False), 'PIE 중에는 적용하지 않습니다.'
    targets = {asset_file(package) for package in PACKAGES}
    assert all(path.is_file() for path in targets), '기존 홍보 에셋 파일 누락'
    # 확인 범위를 같은 PrepHUD 폴더와 실제 호스트 HUD로 한정한다.
    neighbors = set((ROOT / 'Content/FestaFiesta/UI/PrepHUD').rglob('*.uasset')) - targets
    neighbors.add(asset_file(HUD))
    PROTECTED = {path: digest(path) for path in neighbors}
    dirty = {package.get_name() for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    assert not dirty.intersection(PACKAGES), '홍보 에셋의 미저장 변경을 먼저 저장해야 합니다.'
    original = {package: widgets(unreal.load_asset(package)) for package in PACKAGES}
    original_sizes = {'card': values(original[CARD]['SizeBox_Card'], SIZE_FIELDS),
                      'purchase_height': values(original[PANEL]['SizeBox_PurchaseAction'], HEIGHT_FIELDS)}
    backup = OUT / 'Backups' / datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    backup.mkdir(parents=True, exist_ok=False)
    hashes_before = {}
    for package in PACKAGES:
        source = asset_file(package)
        destination = backup / source.name
        hashes_before[package] = digest(source)
        shutil.copy2(source, destination)
        assert digest(destination) == hashes_before[package], '백업 해시 불일치: ' + package
    REPORT.update(backup=str(backup), protected_count=len(PROTECTED), original_sizes=original_sizes,
                  widget_names_before={package: sorted(items) for package, items in original.items()},
                  hashes_before=hashes_before)

    execute()
    REPORT['applied'] = verify(original, original_sizes)
    hashes_after = {package: digest(asset_file(package)) for package in PACKAGES}
    REPORT['saved_assets'] = [package for package in PACKAGES if hashes_after[package] != hashes_before[package]]
    execute()
    REPORT['repeated'] = verify(original, original_sizes)
    assert {package: digest(asset_file(package)) for package in PACKAGES} == hashes_after, '재실행 시 에셋 파일이 변경됨'
    REPORT.update(status='passed', hashes_after=hashes_after, repeat_file_hashes_unchanged=True)
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
finally:
    changed = [str(path.relative_to(ROOT)) for path, value in PROTECTED.items()
               if not path.is_file() or digest(path) != value]
    REPORT['protected_changes'] = changed
    if changed:
        REPORT.update(status='failed', protected_error='홍보 두 에셋 밖의 보호 파일 변경')
    (OUT / 'apply-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    if REPORT['status'] == 'passed':
        unreal.log('PROMOTION_COMPACT_APPLY_PASSED')
    else:
        unreal.log_error('PROMOTION_COMPACT_FAIL: ' + str(REPORT.get('error') or REPORT.get('protected_error')))

started = time.monotonic()


def finish(delta_time):
    if time.monotonic() - started < 2:
        return
    unreal.unregister_slate_post_tick_callback(exit_handle)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
exit_handle = unreal.register_slate_post_tick_callback(finish)
