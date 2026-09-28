"""32/36 높이의 셋업 HUD에서 예전 44/52용 세로 여백만 줄인다."""
from datetime import datetime
from pathlib import Path
import json
import shutil
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/HudSkyOutline'
BASE = '/Game/FestaFiesta/Building/FF_WBP_Building/Shared/'


def apply():
    report = OUT / 'spacing-report.json'
    if report.exists() and json.loads(report.read_text(encoding='utf-8')).get('status') == 'passed':
        return
    backup = OUT / ('SpacingBefore-' + datetime.now().strftime('%Y%m%d-%H%M%S'))
    for name in ('WBP_FF_PrepHudTabButton', 'WBP_FF_PrepHudBar'):
        source = ROOT / 'Content' / (BASE[6:] + name + '.uasset')
        target = backup / source.relative_to(ROOT)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
    tab = unreal.load_asset(BASE + 'WBP_FF_PrepHudTabButton')
    button = unreal.find_object(unreal.find_object(tab, 'WidgetTree'), 'Button_Root', unreal.Button)
    style = button.get_editor_property('widget_style')
    for key in ('normal_padding', 'pressed_padding'):
        padding = style.get_editor_property(key)
        padding.set_editor_property('top', 2.)
        padding.set_editor_property('bottom', 2.)
        style.set_editor_property(key, padding)
    button.set_style(style)
    unreal.BlueprintEditorLibrary.compile_blueprint(tab)
    assert unreal.EditorAssetLibrary.save_loaded_asset(tab, only_if_is_dirty=False)

    hud = unreal.load_asset(BASE + 'WBP_FF_PrepHudBar')
    tree = unreal.find_object(hud, 'WidgetTree')
    for name in ('Build', 'Program', 'Staff', 'Promotion', 'Finale', 'Check'):
        item = unreal.find_object(tree, 'Tab_' + name)
        slot = item.get_editor_property('slot')
        padding = slot.get_editor_property('padding')
        padding.set_editor_property('top', 0.)
        padding.set_editor_property('bottom', 0.)
        slot.set_padding(padding)
    day = unreal.find_object(tree, 'Border_DayStage', unreal.Border)
    padding = day.get_editor_property('padding')
    padding.set_editor_property('top', 2.)
    padding.set_editor_property('bottom', 2.)
    day.set_padding(padding)
    unreal.BlueprintEditorLibrary.compile_blueprint(hud)
    assert unreal.EditorAssetLibrary.save_loaded_asset(hud, only_if_is_dirty=False)
    report.write_text(json.dumps({'status': 'passed', 'backup': str(backup),
                                 'tab_vertical_padding': 2, 'tab_slot_vertical_padding': 0,
                                 'day_chip_vertical_padding': 2,
                                 'row_heights_unchanged': [32, 36]}, ensure_ascii=False, indent=2), encoding='utf-8')


if __name__ == '__main__':
    try:
        apply()
    finally:
        unreal.SystemLibrary.quit_editor()
