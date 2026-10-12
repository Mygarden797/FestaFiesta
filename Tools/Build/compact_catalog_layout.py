"""Compact only the saved facility card and its catalog dock in an idle editor.

Run with UnrealEditor-Cmd -ExecutePythonScript using an absolute script path.
The thumbnail and typefaces stay authored; backups and validation results are
written to Saved/CatalogCompact. Repeating a completed migration saves nothing.
"""

from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil
import traceback

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
RUN = ROOT / 'Saved/CatalogCompact' / datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-%f')
BASE = '/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/'
CARD = BASE + 'WBP_FF_PrepBuildTile'
PANEL = BASE + 'WBP_FF_PrepPanelBuild'
REPORT = {'status': 'RUNNING', 'packages': [CARD, PANEL]}


def path_for(package):
    return ROOT / 'Content' / (package.removeprefix('/Game/') + '.uasset')


def ui_hashes():
    return {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for folder in (ROOT / 'Content/FestaFiesta/UI', ROOT / 'Content/FestaFiesta/Building/FF_WBP_Building')
            for p in folder.rglob('*.uasset')}


def widgets(bp):
    return {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget)
            if w.get_outer() and w.get_outer().get_class().get_name() == 'WidgetTree'
            and w.get_outer().get_outer() == bp}


def margin_values(margin):
    return [margin.left, margin.top, margin.right, margin.bottom]


def inspect():
    card, panel = unreal.load_asset(CARD), unreal.load_asset(PANEL)
    assert card and panel, 'Active card and catalog must exist'
    cw, pw = widgets(card), widgets(panel)
    price = cw['Border_AuroraCardGold']
    cost = cw['Text_Cost']
    price_slot = price.get_editor_property('slot')
    defaults = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(PANEL))
    state = {
        'cardHeight': cw['SizeBox_Card'].get_editor_property('height_override'),
        'fixedNameHeight': cw['Size_PearlFacilityName'].get_editor_property('override_height_override'),
        'nameMinHeight': cw['Size_PearlFacilityName'].get_editor_property('min_desired_height'),
        'thumbnailHeight': cw['SizeBox_Thumbnail'].get_editor_property('height_override'),
        'thumbnailWidth': cw['SizeBox_Thumbnail'].get_editor_property('width_override'),
        'nameFontSize': cw['Text_Name'].get_editor_property('font').size,
        'nameLineHeight': cw['Text_Name'].get_editor_property('line_height_percentage'),
        'costFontSize': cost.get_editor_property('font').size,
        'costLineHeight': cost.get_editor_property('line_height_percentage'),
        'pricePadding': margin_values(price.get_editor_property('padding')),
        'priceAlignment': str(price.get_editor_property('horizontal_alignment')),
        'priceWidthRule': str(price_slot.get_editor_property('size').size_rule),
        'priceSlotPadding': margin_values(price_slot.get_editor_property('padding')),
        'costSlotPadding': margin_values(cost.get_editor_property('slot').get_editor_property('padding')),
        'entryHeight': pw['TileView_Facilities'].get_editor_property('entry_height'),
        'entryWidth': pw['TileView_Facilities'].get_editor_property('entry_width'),
        'dockHeight': pw['Size_ClarityCatalogDock'].get_editor_property('height_override'),
        'catalogHeight': defaults.get_editor_property('catalog_height'),
        'cardWidgets': sorted(cw), 'panelWidgets': sorted(pw),
    }
    return card, panel, cw, pw, state


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world and world.get_path_name().startswith('/Engine/Maps/Entry.'), 'Requires idle Entry map'
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), 'Stop PIE first'
    before_hashes = ui_hashes()
    card, panel, cw, pw, before = inspect()
    desired = {'cardHeight': 210.0, 'fixedNameHeight': False, 'nameMinHeight': 56.0,
               'entryHeight': 210.0, 'dockHeight': 324.0, 'catalogHeight': 324.0,
               'nameLineHeight': 1.0, 'costFontSize': 15.0, 'costLineHeight': 1.0,
               'pricePadding': [0.0] * 4, 'priceSlotPadding': [0.0] * 4,
               'costSlotPadding': [0.0] * 4,
               'priceAlignment': str(unreal.HorizontalAlignment.H_ALIGN_LEFT),
               'priceWidthRule': str(unreal.SlateSizeRule.AUTOMATIC)}
    REPORT.update(before=before, desired=desired)
    if any(before[key] != value for key, value in desired.items()):
        for package in (CARD, PANEL):
            source = path_for(package)
            backup = RUN / 'Backups' / source.relative_to(ROOT)
            backup.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, backup)
        cw['SizeBox_Card'].set_height_override(desired['cardHeight'])
        cw['Size_PearlFacilityName'].clear_height_override()
        cw['Size_PearlFacilityName'].set_min_desired_height(desired['nameMinHeight'])
        cw['Text_Name'].set_editor_property('line_height_percentage', desired['nameLineHeight'])
        cost = cw['Text_Cost']
        font = cost.get_editor_property('font')
        font.size = desired['costFontSize']
        cost.set_editor_property('font', font)
        cost.set_editor_property('line_height_percentage', desired['costLineHeight'])
        cost.set_editor_property('justification', unreal.TextJustify.LEFT)
        cost.get_editor_property('slot').set_padding(unreal.Margin(0.0, 0.0, 0.0, 0.0))
        price = cw['Border_AuroraCardGold']
        price.set_padding(unreal.Margin(0.0, 0.0, 0.0, 0.0))
        price.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_LEFT)
        price_slot = price.get_editor_property('slot')
        price_slot.set_size(unreal.SlateChildSize(value=1.0, size_rule=unreal.SlateSizeRule.AUTOMATIC))
        price_slot.set_padding(unreal.Margin(0.0, 0.0, 0.0, 0.0))
        pw['TileView_Facilities'].set_editor_property('entry_height', desired['entryHeight'])
        pw['Size_ClarityCatalogDock'].set_height_override(desired['dockHeight'])
        for bp in (card, panel):
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        defaults = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(PANEL))
        defaults.set_editor_property('catalog_height', desired['catalogHeight'])
        for bp in (card, panel):
            assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False), 'Save failed: ' + bp.get_name()
    _, _, _, _, after = inspect()
    assert all(after[key] == value for key, value in desired.items()), 'Compact dimensions were not saved'
    for key in before.keys() - desired.keys():
        assert after[key] == before[key], 'Unrelated property changed: ' + key
    after_hashes = ui_hashes()
    targets = {path_for(package).relative_to(ROOT).as_posix() for package in (CARD, PANEL)}
    unexpected = [path for path in before_hashes.keys() | after_hashes.keys()
                  if path not in targets and before_hashes.get(path) != after_hashes.get(path)]
    assert not unexpected, 'Unrelated UI assets changed: ' + repr(unexpected)
    REPORT.update(status='PASS', after=after, unrelatedUiUnchanged=True,
                  changedAssets=[path for path in targets if before_hashes[path] != after_hashes[path]])


try:
    main()
except Exception:
    REPORT.update(status='FAILED', error=traceback.format_exc())
    unreal.log_error(REPORT['error'])
finally:
    RUN.mkdir(parents=True, exist_ok=True)
    result = json.dumps(REPORT, ensure_ascii=False, indent=2)
    (RUN / 'report.json').write_text(result, encoding='utf-8')
    (ROOT / 'Saved/CatalogCompact/latest-result.json').write_text(result, encoding='utf-8')
    unreal.log('FF_CATALOG_COMPACT_' + REPORT['status'] + ': ' + str(RUN))
if REPORT['status'] != 'PASS':
    raise RuntimeError('Catalog compact migration failed; inspect latest-result.json and retained backups')
