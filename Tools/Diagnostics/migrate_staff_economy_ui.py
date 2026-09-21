"""Verify PR 217's staff UI; -FFApplyStaffEconomyUI repairs known old text only.

Run via UnrealEditor-Cmd -run=pythonscript -script=<this file> -unattended -nullrhi.
Preserves widget layout and saved economy data. Does not rebuild any WBP.
"""
import hashlib
import json
import re
import shutil
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/EconomyIntentAudit'
UPDATES = {
    'LiveHUD/Panels/WBP_FF_LivePanelStaff': {
        'Text_AssignedStaff_Label': ('프로그램 배치', '사건 대응'),
        'Text_AssignedStaff': ('5명', '0명'),
        'Text_IdleStaff': ('7명', '12명'),
        'Text_AssignmentsTitle': ('오늘 편성', '사건 대응 현황'),
        'Text_AssignmentsEmpty': (
            '오늘 편성한 유료 프로그램이 없어 인력이 묶여 있지 않습니다.',
            '지금 대응 중인 사건이 없습니다.'),
        'Text_DailyWage': ('오늘 알바 임금 900 / 일', '1명 고용비 450'),
        'Text_HireNote': (
            '알바는 하루짜리라 셋업과 개장 준비에서만 뽑습니다. 라이브 중에는 늘릴 수 없습니다.',
            '고용은 셋업과 개장 준비에서만 합니다. 라이브 중에는 늘릴 수 없습니다.'),
        'Text_IdleNote': (
            '임금은 그대로 나가는데 아무 프로그램에도 묶여 있지 않습니다.',
            '사건이 나면 이 인원이 대응에 들어갑니다.'),
        'Text_StaffBreakdown': ('기본 6 · 알바 6', '기본 6 · 고용 6'),
    },
    'LiveHUD/WBP_FF_LiveHudScreen': {
        'Text_Staff': ('6명 중 4명 사용 가능', '6명'),
    },
    'PrepHUD/Panels/WBP_FF_PrepPanelStaff': {
        'Text_WagePreview': ('1명 x 150 = 150 Gold', '1명 x 450 = 450 Gold'),
        'Text_GoldAfterHire': ('고용 후 잔액 7,450', '고용 후 잔액 7,150'),
    },
    'PrepHUD/Panels/WBP_FF_PrepPanelProgram': {
        'Text_CostPreview': ('250 Gold · 인원 2명', '250 Gold'),
    },
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def widgets(asset):
    return {widget.get_name(): widget for widget in unreal.ObjectIterator(unreal.Widget)
            if widget.get_outer() and widget.get_outer().get_class().get_name() == 'WidgetTree'
            and widget.get_outer().get_outer() == asset}


def snapshot(asset):
    result = {}
    for name, widget in widgets(asset).items():
        row = {'class': widget.get_class().get_path_name(),
               'parent': widget.get_parent().get_name() if widget.get_parent() else None}
        for key in ('visibility', 'render_opacity', 'render_transform'):
            row[key] = str(widget.get_editor_property(key))
        slot = widget.get_editor_property('slot')
        if slot:
            row['slot_class'] = slot.get_class().get_name()
            for key in ('layout_data', 'padding', 'size', 'horizontal_alignment', 'vertical_alignment'):
                try:
                    value = slot.get_editor_property(key)
                    row[key] = value.export_text() if hasattr(value, 'export_text') else str(value)
                except Exception:
                    pass  # Property is not available on every slot class.
        if isinstance(widget, unreal.TextBlock):
            row['text'] = str(widget.get_text())
            for key in ('font', 'color_and_opacity', 'justification', 'auto_wrap_text', 'wrap_text_at'):
                value = widget.get_editor_property(key)
                row[key] = value.export_text() if hasattr(value, 'export_text') else str(value)
        result[name] = row
    assert result, 'Original WidgetTree is empty: ' + asset.get_path_name()
    return result


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    apply = bool(re.search(r'(?:^|\s)-FFApplyStaffEconomyUI(?:\s|$)', unreal.SystemLibrary.get_command_line()))
    assets = {}
    for relative, changes in UPDATES.items():
        package = '/Game/FestaFiesta/UI/' + relative
        asset = unreal.load_asset(package)
        assert asset, 'Missing asset: ' + package
        before = snapshot(asset)
        for name, (old, new) in changes.items():
            assert name in before, 'Missing widget: ' + package + '/' + name
            assert before[name].get('text') in (old, new), 'Preserving unfamiliar authored text: ' + package + '/' + name
            assert apply or before[name]['text'] == new, 'Old economy text remains: ' + package + '/' + name
        assets[relative] = (asset, before)

    allowed = {(ROOT / 'Content/FestaFiesta/UI' / (name + '.uasset')).resolve() for name in UPDATES}
    protected = {path: digest(path) for base in ('Content/FestaFiesta/UI', 'Content/FestaFiesta/Data',
                 'Content/FestaFiesta/Building/FF_WBP_Building', 'Content/FestaFiesta/Building/FF_Data_Building')
                 for path in (ROOT / base).rglob('*.uasset') if path.resolve() not in allowed}
    report = {'mode': 'apply' if apply else 'verify', 'assets': {}, 'protected_packages': len(protected)}
    for relative, (asset, before) in assets.items():
        file = ROOT / 'Content/FestaFiesta/UI' / (relative + '.uasset')
        before_hash = digest(file)
        expected = json.loads(json.dumps(before))
        for name, (_, new) in UPDATES[relative].items():
            expected[name]['text'] = new
        changed = before != expected
        if changed:
            backup = OUT / 'Backups' / (file.stem + '.' + before_hash[:12] + '.uasset')
            backup.parent.mkdir(parents=True, exist_ok=True)
            if not backup.exists():
                shutil.copy2(file, backup)
            asset.modify()
            current = widgets(asset)
            for name, (_, new) in UPDATES[relative].items():
                if str(current[name].get_text()) != new:
                    current[name].modify()
                    # Edit the authored default property, not the runtime SetText API.
                    current[name].set_editor_property('text', unreal.Text(new))
            unreal.BlueprintEditorLibrary.compile_blueprint(asset)
            assert snapshot(asset) == expected, 'Unexpected layout/style change: ' + relative
            assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False), relative
        assert snapshot(asset) == expected, 'Staff UI verification failed: ' + relative
        report['assets'][relative] = {'changed': changed, 'before': before_hash, 'after': digest(file),
                                       'text_changes': UPDATES[relative]}
    assert all(digest(path) == original for path, original in protected.items()), 'Unrelated asset changed'
    report['status'] = 'PASS'
    (OUT / ('staff-ui-' + report['mode'] + '.json')).write_text(
        json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('FF_STAFF_ECONOMY_UI_PASS ' + report['mode'])


main()
