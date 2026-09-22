# apply_pearl_codex.py
# 역할: 시설 조합 도감과 보상 표시 작업에서 지정된 대상에 변경을 적용하고 결과를 확인한다.

"""Mix pearl-white content surfaces into the existing forest festival codex."""
import hashlib
import json
import runpy
import shutil
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
CONTENT = ROOT / 'Content'
OUT = ROOT / 'Saved/FacilityCodexPearl_20260921'
CODEX = '/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/'
VERSION = 'FF.FacilityCodex.Pearl20260921.1'
PATHS = [CODEX + 'WBP_FF_FacilityCodex' + role for role in ('Screen', 'Card')]
PATHS += [CODEX + 'Materials/M_FF_CodexGlass']


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
    assert path.is_file(), str(path)
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
    runpy.run_path(str(ROOT / 'Tools/FacilityCodex/apply_pearl_materials.py'), run_name='__main__')
    unreal.SystemLibrary.execute_console_command(world, 'FF.ApplyFacilityCodexContrast')
    for role, bp in blueprints.items():
        assert unreal.EditorAssetLibrary.get_metadata_tag(bp, VERSION) == '1', role
        tree = widgets(bp)
        assert topology(tree) == before_trees[role], 'Style migration changed widget structure: ' + role
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
    'status': 'PASS', 'idempotent': True, 'widget_topology_preserved': True,
    'reward_bounds': [72, 32], 'protected_assets': len(protected), 'assets': saved
}, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('FF_CODEX_PEARL_VERIFY_PASS')
