"""Save the reference-inspired codex and verify its bounded, repeatable migration."""
import hashlib
import json
import runpy
import shutil
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
CONTENT = ROOT / 'Content'
OUT = ROOT / 'Saved/FacilityCodexShowcase_20260921'
CODEX = '/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/'
PATHS = [CODEX + 'WBP_FF_FacilityCodexScreen', CODEX + 'WBP_FF_FacilityCodexCard',
         CODEX + 'Materials/M_FF_CodexShowcase', CODEX + 'Materials/MI_FF_CodexShowcase_Stage']
VERSION = 'FF.FacilityCodex.Showcase20260921.2'


def file_for(path):
    return CONTENT / (path.removeprefix('/Game/') + '.uasset')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def widgets(bp):
    return {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget)
            if w.get_outer() and w.get_outer().get_class().get_name() == 'WidgetTree'
            and w.get_outer().get_outer() == bp}


def verify():
    for role in ('Screen', 'Card'):
        bp = unreal.load_asset(CODEX + 'WBP_FF_FacilityCodex' + role)
        assert bp and unreal.EditorAssetLibrary.get_metadata_tag(bp, VERSION) == '1'
        tree = widgets(bp)
        assert sum(not widget.get_parent() for widget in tree.values()) == 1, 'Detached widgets: ' + role
        if role == 'Screen':
            for name in ('Size_Browse', 'Size_Showcase', 'Viewport_Showcase', 'Image_Showcase', 'Combo_Category',
                         'Button_RewardReady', 'Button_PreviewTarget', 'Button_PreviewPartner', 'Button_ShowcaseCatalog',
                         'Button_RotatePreviewLeft', 'Button_RotatePreviewRight', 'Button_ResetPreview',
                         'Button_TargetCatalog', 'Button_PartnerCatalog', 'Scroll_DetailGlass', 'Switcher_Detail',
                         'Button_DetailClaimReward', 'Border_DetailRewardClaimed', 'Border_DetailRewardLocked'):
                assert name in tree, name
            assert tree['Viewport_Showcase'].get_class().get_name() == 'FF_FacilityCodexPreview'
            assert tree['Image_Showcase'].get_editor_property('discovery_material')
            assert tree['Switcher_Detail'].get_children_count() == 3
            assert sum(isinstance(widget, unreal.BackgroundBlur) for widget in tree.values()) == 1
            bounds = tree['Size_DetailRewardState']
        else:
            bounds = tree['Size_RewardState']
        assert bounds.get_editor_property('width_override') == 72.0
        assert bounds.get_editor_property('height_override') == 32.0
    for path in PATHS[2:]:
        material = unreal.load_asset(path)
        assert material and unreal.EditorAssetLibrary.get_metadata_tag(material, 'FF.FacilityCodex.ShowcaseMaterialVersion') == '1'


world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world and world.get_path_name().startswith('/Engine/Maps/Entry.')
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
OUT.mkdir(parents=True, exist_ok=True)
owned = {file_for(path) for path in PATHS}
for path in owned:
    if path.exists():
        backup = OUT / 'before_assets' / path.relative_to(CONTENT)
        backup.parent.mkdir(parents=True, exist_ok=True)
        if not backup.exists():
            shutil.copy2(path, backup)
protected = {path: digest(path) for folder in (CONTENT / 'FestaFiesta', CONTENT / 'Asset/Font')
             for path in folder.rglob('*.uasset') if path not in owned}
for attempt in range(2):
    runpy.run_path(str(ROOT / 'Tools/FacilityCodex/create_showcase_materials.py'), run_name='__main__')
    unreal.SystemLibrary.execute_console_command(world, 'FF.ApplyFacilityCodexShowcase')
    verify()
    current = {path: digest(path) for path in owned}
    if attempt:
        assert current == saved, 'Repeated migration changed saved assets'
    saved = current
for path, original in protected.items():
    assert digest(path) == original, 'Unrelated asset changed: ' + str(path)
(OUT / 'migration-result.json').write_text(json.dumps({'status': 'PASS', 'idempotent': True,
    'protected_assets': len(protected), 'assets': {str(path.relative_to(ROOT)): value for path, value in saved.items()}},
    ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('FF_CODEX_SHOWCASE_VERIFY_PASS')
