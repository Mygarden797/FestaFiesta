"""Apply the construction redesign to explicit UI packages, then verify isolation/idempotence.

Use a freshly built UE 5.7 editor in /Engine/Maps/Entry with -ExecutePythonScript.
The backup preserves the working assets present before this revision, including local edits.
"""
import hashlib
import json
import runpy
import shutil
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/BuildModeClarity'
OUT.mkdir(parents=True, exist_ok=True)
NAMES = [
    'Building/WBP_FF_BuildTools',
    'PrepHUD/Panels/WBP_FF_PrepPanelBuild',
    'PrepHUD/Build/WBP_FF_PrepBuildTile',
    'Facility/WBP_FF_FacilityInfoPanel',
    'FacilityCodex/WBP_FF_FacilityCodexScreen',
    'FacilityCodex/WBP_FF_FacilityCodexCard',
    'Building/WBP_FF_BuildSynergy',
    'Facility/WBP_FF_FacilityLightingControls',
    'Facility/WBP_FF_FacilityPaidProgramCard',
    'Facility/WBP_FF_FacilityPaidProgramPicker',
    'FacilityRevenue/WBP_FF_FacilityRevenueRanking',
    'FacilityRevenue/WBP_FF_FacilityRevenueRankRow',
    'FacilityRevenue/WBP_FF_FacilityWorldBadge',
    'FacilityCodex/WBP_FF_FacilityCodexLauncher',
    'Building/WBP_FF_BuildCategoryChoice',
]
COMMANDS = ['FF.ApplyBuildModeClarityTools', 'FF.ApplyBuildModeClarityCatalog',
            'FF.ApplyBuildModeClarityInfo', 'FF.RefineBuildModeClarityInfoFit',
            'FF.ApplyBuildModeClarityCodex', 'FF.ApplyBuildModeClaritySynergy',
            'FF.ApplyBuildModeClarityFacilityV2', 'FF.ApplyBuildModeClarityRevenue',
            'FF.ApplyBuildModeClarityCodexRefinement']

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

paths = [ROOT / ('Content/FestaFiesta/UI/' + name + '.uasset') for name in NAMES]
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world and world.get_path_name().startswith('/Engine/Maps/Entry.'), 'Use Entry world'
protected = {p: digest(p) for p in (ROOT / 'Content/FestaFiesta/UI').rglob('*.uasset')
             if p not in paths and '/Building/Clarity/' not in p.as_posix()}
for path in paths:
    assert path.is_file(), path
    backup = OUT / 'before_assets' / path.relative_to(ROOT / 'Content/FestaFiesta/UI')
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists(): shutil.copy2(path, backup)

runpy.run_path(str(ROOT / 'Tools/BuildModeClarity/create_materials.py'))
runpy.run_path(str(ROOT / 'Tools/BuildModeClarity/create_category_icons.py'))
runpy.run_path(str(ROOT / 'Tools/BuildModeClarity/create_placement_feedback.py'))
for command in COMMANDS:
    unreal.log('FF_BUILD_CLARITY_APPLY ' + command)
    unreal.SystemLibrary.execute_console_command(world, command)

required = {
    NAMES[0]: ['Canvas_BuildClarity', 'Button_Catalog', 'Button_Confirm', 'Text_ContextPing', 'Column_ClarityLeftDock'],
    NAMES[1]: ['Size_ClarityCatalogDock'],
    NAMES[3]: ['Clarity_InfoV1', 'Button_TabRevenue', 'Button_TabPrograms', 'ClarityV2_Info'],
    NAMES[4]: ['Size_Dialog', 'Switcher_Detail', 'Button_DetailRecipe', 'Button_DetailPlacement'],
    NAMES[6]: ['Size_Synergy', 'Border_Synergy', 'Text_SynergyLegend', 'Text_SynergyFooter'],
    NAMES[7]: ['Button_LightingPrecision', 'Panel_LightingBasic', 'Panel_LightingPrecision', 'ClarityV2_Lighting'],
    NAMES[9]: ['Button_BackToPrograms', 'Text_BackToPrograms'],
    NAMES[10]: ['Size_ClarityFrame'],
}
widgets = {}
for name in NAMES:
    bp = unreal.load_asset('/Game/FestaFiesta/UI/' + name)
    assert bp
    owned = [w for w in unreal.ObjectIterator(unreal.Widget)
             if w.get_outer() and w.get_outer().get_class().get_name() == 'WidgetTree'
             and w.get_outer().get_outer() == bp]
    widgets[name] = sorted(w.get_name() for w in owned)
    roots = [w.get_name() for w in owned if not w.get_parent()]
    assert len(roots) == 1, name + ': detached widgets: ' + str(roots)
    for required_name in required.get(name, []):
        assert required_name in widgets[name], name + ': missing ' + required_name
    if name == NAMES[0]:
        for removed_name in ('Button_Radial', 'Panel_Radial', 'Panel_Details', 'Image_SDFWheel'):
            assert removed_name not in widgets[name], 'Retired tool remains: ' + removed_name

after = {str(p.relative_to(ROOT)): digest(p) for p in paths}
for command in COMMANDS: unreal.SystemLibrary.execute_console_command(world, command)
for path in paths:
    assert digest(path) == after[str(path.relative_to(ROOT))], 'Non-idempotent: ' + str(path)
for path, before in protected.items():
    assert digest(path) == before, 'Unexpected UI edit: ' + str(path)
(OUT / 'migration-result.json').write_text(json.dumps({
    'status': 'PASS', 'commands': COMMANDS, 'protected_ui_count': len(protected),
    'idempotent': True, 'asset_hashes': after, 'widgets': widgets,
}, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('FF_BUILD_CLARITY_APPLY_VERIFY_PASS')
