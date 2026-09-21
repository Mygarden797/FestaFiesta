"""Apply and verify the catalog's partial second row in an idle Entry editor."""
import hashlib
import json
import shutil
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
UI_ROOTS = (ROOT / 'Content/FestaFiesta/UI', ROOT / 'Content/FestaFiesta/Building/FF_WBP_Building')
OUT = ROOT / 'Saved/CatalogRowPreview_20260921'
PACKAGE = '/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepPanelBuild'
PANEL_FILE = ROOT / 'Content/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepPanelBuild.uasset'
COMMAND = 'FF.ApplyBuildModeClarityCatalog'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inspect_panel():
    blueprint = unreal.load_asset(PACKAGE)
    assert blueprint, 'Missing production catalog WBP'
    default = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(PACKAGE))
    widgets = {widget.get_name(): widget for widget in unreal.ObjectIterator(unreal.Widget)
               if widget.get_outer() and widget.get_outer().get_class().get_name() == 'WidgetTree'
               and widget.get_outer().get_outer() == blueprint}
    tiles = widgets['TileView_Facilities']
    return dict(
        catalog_height=default.get_editor_property('catalog_height'),
        dock_height=widgets['Size_ClarityCatalogDock'].get_editor_property('height_override'),
        cards_per_page=default.get_editor_property('cards_per_page'),
        pagination=default.get_editor_property('use_pagination'),
        entry_height=tiles.get_editor_property('entry_height'),
        row_spacing=tiles.get_editor_property('vertical_entry_spacing'),
        clipping=str(tiles.get_editor_property('clipping')),
    )


world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world and world.get_path_name().startswith('/Engine/Maps/Entry.'), 'Use an idle Entry world'
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
OUT.mkdir(parents=True, exist_ok=True)
backup = OUT / 'before_assets/WBP_FF_PrepPanelBuild.uasset'
backup.parent.mkdir(parents=True, exist_ok=True)
if not backup.exists():
    shutil.copy2(PANEL_FILE, backup)
protected = {path: digest(path) for base in UI_ROOTS for path in base.rglob('*.uasset')
             if path != PANEL_FILE}
before = inspect_panel()
unreal.SystemLibrary.execute_console_command(world, COMMAND)
after = inspect_panel()
assert after['catalog_height'] == after['dock_height'] == 454.0, after
assert after['cards_per_page'] == 6 and after['pagination'], after
assert after['entry_height'] == 204.0 and after['row_spacing'] == 0.0, after
assert 'CLIP_TO_BOUNDS' in after['clipping'], after
saved_hash = digest(PANEL_FILE)
unreal.SystemLibrary.execute_console_command(world, COMMAND)
assert digest(PANEL_FILE) == saved_hash, 'Migration rewrote the panel on its second run'
assert inspect_panel() == after, 'Migration changed panel settings on its second run'
for path, original in protected.items():
    assert digest(path) == original, 'Unexpected UI asset edit: ' + str(path)
(OUT / 'migration-result.json').write_text(json.dumps(dict(
    status='PASS', command=COMMAND, package=PACKAGE, backup=str(backup.relative_to(ROOT)),
    before=before, after=after, sha256=saved_hash,
    protected_ui_count=len(protected), idempotent=True,
), ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('FF_CATALOG_ROW_PREVIEW_APPLY_VERIFY_PASS')
