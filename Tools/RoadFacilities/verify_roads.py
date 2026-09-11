"""Read saved road assets, catalog specifications and costs in a fresh editor."""
import json
import sys
from pathlib import Path
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from import_roads import BASE, OUT, ROADS, CALIBRATION, COST_TABLE, definition_path, thumbnail_path, inspect_mesh, inspect_calibration

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
    assert spec.get_editor_property('allow_facility_placement_on_top')
    assert str(spec.get_editor_property('display_name')) == label
    assert definition.get_editor_property('show_in_build_catalog')
    assert spec.get_editor_property('clearance_cells') == 0
    assert spec.get_editor_property('footprint_size_cells') == unreal.IntPoint(400, 400)
    assert spec.get_editor_property('placement_obstacle_height_cm') == 8.0
    assert spec.get_editor_property('visual_sizing_mode') == unreal.FFZSVisualSizingMode.MATCH_PLACEMENT_BOUNDS
    assert spec.get_editor_property('visual_rotation') == unreal.Rotator(pitch=0, yaw=0, roll=0)
    assert not spec.get_editor_property('uses_facility_levels')
    for key in ('anchors', 'queue_reserved_rects', 'queue_spline_points_cm'):
        assert not spec.get_editor_property(key)
    rules = spec.get_editor_property('queue_rules')
    assert rules.get_editor_property('max_waiting_agents') == rules.get_editor_property('service_capacity') == 0
    mesh = spec.get_editor_property('visual_mesh')
    assert mesh.get_path_name().split('.')[0] == BASE + '/SM_Road_' + name
    thumbnail = definition.get_editor_property('catalog_thumbnail')
    assert isinstance(thumbnail, unreal.Texture2D)
    assert thumbnail.get_path_name().split('.')[0] == thumbnail_path(name)
    price = costs['Cost.Road.Surface.' + name]
    assert price > 0
    rows.append(dict(name=name, definition=definition.get_path_name(), cost=price,
                     enable_drag_placement=bool(definition.get_editor_property('enable_drag_placement')),
                     thumbnail=thumbnail.get_path_name(), **inspect_mesh(mesh, material)))
(OUT / 'verification_report.json').write_text(json.dumps(dict(status='PASS', roads=rows, calibration=calibration), ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('ROAD_VERIFY_PASS')
