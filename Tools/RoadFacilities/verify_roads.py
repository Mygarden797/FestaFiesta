"""Read saved surface meshes plus authored Road/Tile definitions in a fresh editor."""
import json
import sys
from pathlib import Path
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from import_roads import BASE, OUT, ROADS, CALIBRATION, COST_TABLE, definition_path, thumbnail_path, inspect_mesh, inspect_calibration
from verify_road_tiles import run as verify_split

OUT.mkdir(parents=True, exist_ok=True)
material = unreal.load_asset(BASE + '/Materials/M_Road_Palette')
assert material
table = unreal.load_asset(COST_TABLE)
assert table
costs = {row['Name']: int(row['InstallCost']) for row in json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))}
rows = []
calibration = [inspect_calibration(unreal.load_asset(BASE + '/QA/Calibration/' + name), center, size)
               for name, center, size in CALIBRATION]
for name, label in ROADS:
    definition = unreal.load_asset(definition_path(name))
    assert definition, name
    errors = definition.is_definition_valid()
    assert errors is not None and len(errors) == 0, (name, errors)
    spec = definition.get_editor_property('spec')
    assert str(spec.get_editor_property('facility_definition_id')) == 'Road_Surface_' + name
    assert spec.get_editor_property('category') == unreal.FFZSFacilityCategory.ROAD
    assert not spec.get_editor_property('bAllowFacilityPlacementOnTop')
    # Authored placement size, height, rotation and input policy are independent
    # from the original 400x400x8cm source mesh. Native validation checks them.
    mesh = spec.get_editor_property('visual_mesh')
    assert mesh.get_path_name().split('.')[0] == BASE + '/SM_Road_' + name
    thumbnail = definition.get_editor_property('catalog_thumbnail')
    assert isinstance(thumbnail, unreal.Texture2D)
    assert thumbnail.get_path_name().split('.')[0] == thumbnail_path(name)
    cost_key = str(spec.get_editor_property('install_cost_key'))
    if cost_key in ('', 'None'):
        cost_key = 'Cost.Road.Surface.' + name
    price = costs[cost_key]
    assert price > 0
    rows.append(dict(name=name, definition=definition.get_path_name(), cost=price,
                     enable_drag_placement=bool(definition.get_editor_property('enable_drag_placement')),
                     thumbnail=thumbnail.get_path_name(), **inspect_mesh(mesh, material)))
split_report = verify_split()
(OUT / 'verification_report.json').write_text(json.dumps(dict(status='PASS', roads=rows, surfaces=split_report['surfaces'], calibration=calibration), ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('ROAD_VERIFY_PASS')
