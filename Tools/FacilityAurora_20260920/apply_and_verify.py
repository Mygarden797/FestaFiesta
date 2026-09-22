"""Apply the requested facility UI revision to explicit saved assets and verify it."""
import hashlib
import json
import runpy
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
UI_ROOTS = (ROOT / 'Content/FestaFiesta/UI', ROOT / 'Content/FestaFiesta/Building/FF_WBP_Building')
OUT = ROOT / 'Saved/FacilityAurora_20260920'
OUT.mkdir(parents=True, exist_ok=True)
PACKAGES = [
    '/Game/FestaFiesta/Building/FF_WBP_Building/' + name for name in (
        'Facility/WBP_FF_FacilityInfoPanel',
        'Facility/WBP_FF_FacilityLightingControls',
        'Facility/WBP_FF_FacilitySynergyEntry',
        'Facility/WBP_FF_FacilityPaidProgramCard',
        'Facility/WBP_FF_FacilityPaidProgramPicker',
        'FacilityRevenue/WBP_FF_FacilityWorldBadge',
        'FacilityRevenue/WBP_FF_FacilityRevenueRanking',
        'FacilityRevenue/WBP_FF_FacilityRevenueRankRow',
        'FacilityCodex/WBP_FF_FacilityCodexScreen',
        'FacilityCodex/WBP_FF_FacilityCodexCard',
        'FacilityCodex/WBP_FF_FacilityCodexLauncher',
        'Catalog/WBP_FF_PrepPanelBuild',
        'Catalog/WBP_FF_PrepBuildTile',
        'Controls/WBP_FF_BuildTools',
        'Controls/WBP_FF_BuildSynergy',
    )
]
COMMANDS = [
    'FF.ApplyFacilityAuroraInfo',
    'FF.ApplyFacilityAuroraBuild',
    'FF.ApplyFacilityAuroraRevenueAndPrograms',
    'FF.ApplyFacilityAuroraWorldBadgeAmountFit',
    'FF.ApplyFacilityAuroraLighting',
    'FF.RefineFacilityAuroraLighting',
    'FF.ApplyFacilityAuroraTheme',
]

def path(package):
    return ROOT / 'Content' / (package.removeprefix('/Game/') + '.uasset')

def digest(file):
    return hashlib.sha256(file.read_bytes()).hexdigest()

def snapshot(package):
    asset = unreal.EditorAssetLibrary.load_asset(package)
    assert asset, 'Missing saved asset: ' + package
    widgets = {}
    for widget in unreal.ObjectIterator(unreal.Widget):
        outer = widget.get_outer()
        if not outer or outer.get_class().get_name() != 'WidgetTree' or outer.get_outer() != asset:
            continue
        row = {'class': widget.get_class().get_name(),
               'parent': widget.get_parent().get_name() if widget.get_parent() else None}
        if isinstance(widget, unreal.Border):
            brush = widget.get_editor_property('background')
            resource = brush.get_editor_property('resource_object')
            row['resource'] = resource.get_path_name() if resource else None
        elif isinstance(widget, unreal.Button):
            brush = widget.get_editor_property('widget_style').get_editor_property('normal')
            resource = brush.get_editor_property('resource_object')
            row['resource'] = resource.get_path_name() if resource else None
        widgets[widget.get_name()] = row
    return {'sha256': digest(path(package)), 'widgets': widgets}

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world and world.get_path_name().startswith('/Engine/Maps/Entry.'), 'Use Entry world.'
allowed = {path(package).resolve() for package in PACKAGES}
protected = {file: digest(file) for base in UI_ROOTS for file in base.rglob('*.uasset')
             if file.resolve() not in allowed and '/Aurora/' not in file.as_posix()}
runpy.run_path(str(ROOT / 'Tools/FacilityAurora_20260920/create_materials.py'))
for command in COMMANDS:
    unreal.log('FF_AURORA_APPLY ' + command)
    unreal.SystemLibrary.execute_console_command(world, command)
after = {package: snapshot(package) for package in PACKAGES}
required = {
    PACKAGES[0]: ['Aurora_InfoV7', 'Text_QueueCapacityValue', 'Text_ServiceCapacityValue', 'Button_TabLighting'],
    PACKAGES[11]: ['Button_PlaceSelected', 'Text_SelectedCost', 'Text_AfterPlacementGold'],
    PACKAGES[13]: ['Canvas_AuroraBuildV7', 'Border_PlacementInfo', 'Border_PlacementHelp'],
}
for package, names in required.items():
    for name in names:
        assert name in after[package]['widgets'], 'Missing widget: ' + package + '/' + name
for package in PACKAGES:
    assert any('/Aurora/MI_UI_FacilityAurora_' in (row.get('resource') or '')
               for row in after[package]['widgets'].values()), 'Missing green material: ' + package
for file, original in protected.items():
    assert digest(file) == original, 'Unexpected UI package edit: ' + str(file)
for command in COMMANDS:
    unreal.SystemLibrary.execute_console_command(world, command)
for package, data in after.items():
    assert digest(path(package)) == data['sha256'], 'Non-idempotent migration: ' + package
(OUT / 'wbp-after.json').write_text(json.dumps(after, ensure_ascii=False, indent=2), encoding='utf-8')
(OUT / 'migration-result.json').write_text(json.dumps({
    'status': 'PASS', 'commands': COMMANDS, 'packages': PACKAGES,
    'protected_package_count': len(protected), 'idempotence': True,
}, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('FF_FACILITY_AURORA_APPLY_VERIFY_PASS')
