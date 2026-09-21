"""Apply clear gold/checked/grayscale reward states and success feedback."""
import hashlib
import json
import runpy
import shutil
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
CONTENT = ROOT / 'Content'
OUT = ROOT / 'Saved/FacilityCodexRewardAffordance_20260921'
CODEX = '/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/'
VERSION = 'FF.FacilityCodex.RewardAffordance20260921.1'
PATHS = [CODEX + 'WBP_FF_FacilityCodex' + role for role in ('Screen', 'Card')]
PATHS += [CODEX + 'Materials/' + name for name in ('M_FF_CodexRewardIridescent', 'MI_FF_CodexReward_LockedGift')]


def file_for(path):
    return CONTENT / (path.removeprefix('/Game/') + '.uasset')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def widgets(bp):
    return {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget)
            if w.get_outer() and w.get_outer().get_class().get_name() == 'WidgetTree'
            and w.get_outer().get_outer() == bp}


def topology(tree):
    return {name: (widget.get_class().get_name(), widget.get_parent().get_name() if widget.get_parent() else None)
            for name, widget in tree.items()}


world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world and world.get_path_name().startswith('/Engine/Maps/Entry.')
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
OUT.mkdir(parents=True, exist_ok=True)
owned = {file_for(path) for path in PATHS}
for path in owned:
    if not path.exists():
        assert path.name == "MI_FF_CodexReward_LockedGift.uasset", str(path)
        continue
    backup = OUT / 'before_assets' / path.relative_to(CONTENT)
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(path, backup)
for name in ('FacilityCodexShowcaseDiscovered1080.png', 'ProductionCodex720.png', 'FacilityCodexRewardStates1080.png'):
    source = ROOT / 'Saved/Automation/FacilityCodex' / name
    target = OUT / ('before_' + name)
    if source.exists() and not target.exists():
        shutil.copy2(source, target)
protected = {p: digest(p) for folder in (CONTENT / 'FestaFiesta', CONTENT / 'Asset/Font')
             for p in folder.rglob('*.uasset') if p not in owned}
blueprints = {role: unreal.load_asset(CODEX + 'WBP_FF_FacilityCodex' + role) for role in ('Screen', 'Card')}
before_trees = {role: topology(widgets(bp)) for role, bp in blueprints.items()}
for attempt in range(2):
    runpy.run_path(str(ROOT / 'Tools/FacilityCodex/apply_reward_affordance_materials.py'), run_name='__main__')
    unreal.SystemLibrary.execute_console_command(world, 'FF.ApplyFacilityCodexRewardAffordance')
    for role, bp in blueprints.items():
        assert unreal.EditorAssetLibrary.get_metadata_tag(bp, VERSION) == '1', role
        tree = widgets(bp)
        after_tree = topology(tree)
        expected = dict(before_trees[role])
        if role == 'Screen':
            expected.update({
                'Text_DetailRewardHint': ('TextBlock', 'Vertical_ShowcaseRewardInfo'),
                'Size_RewardFeedback': ('SizeBox', 'Overlay_GlassScreen'),
                'Border_RewardFeedback': ('Border', 'Size_RewardFeedback'),
                'Horizontal_RewardFeedback': ('HorizontalBox', 'Border_RewardFeedback'),
                'Image_RewardFeedbackCheck': ('Image', 'Horizontal_RewardFeedback'),
                'Text_RewardFeedback': ('TextBlock', 'Horizontal_RewardFeedback'),
            })
        assert after_tree == expected, 'Unexpected widget structure change: ' + role
        bounds = tree['Size_DetailRewardState' if role == 'Screen' else 'Size_RewardState']
        assert bounds.get_editor_property('width_override') == 72.0
        assert bounds.get_editor_property('height_override') == 32.0
    hashes = {str(p.relative_to(ROOT)): digest(p) for p in owned}
    if attempt:
        assert hashes == saved, 'Repeat migration changed saved assets'
    saved = hashes
for path, original in protected.items():
    assert digest(path) == original, 'Unrelated asset changed: ' + str(path)
(OUT / 'migration-result.json').write_text(json.dumps({
    'status': 'PASS', 'idempotent': True, 'only_reward_guidance_added': True,
    'reward_bounds': [72, 32], 'protected_assets': len(protected), 'assets': saved
}, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('FF_CODEX_REWARD_AFFORDANCE_VERIFY_PASS')
