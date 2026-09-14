"""Read-only fresh-editor verification of persisted Road/Tile split and prices."""
import json
import sys
from pathlib import Path

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from import_roads import COST_TABLE, OUT, ROADS, definition_path
from enable_drag_placement import require_isolated_editor
from split_road_tiles import tile_definition_path, validate_definition, definition_snapshot


def run():
    require_isolated_editor()
    report_path = OUT / 'road_tile_split_migration.json'
    if not report_path.is_file():
        raise RuntimeError('Run split_road_tiles.py before the fresh-editor preservation check')
    before = json.loads(report_path.read_text(encoding='utf-8'))
    if before.get('status') != 'PASS':
        raise RuntimeError('Migration did not complete successfully')
    expected = {row['name']: row for row in before['surfaces']}
    rows = []
    for name, _label in ROADS:
        row = dict(name=name)
        for kind, path in (('Road', definition_path(name)), ('Tile', tile_definition_path(name))):
            definition = unreal.load_asset(path)
            spec = validate_definition(definition, name, kind)
            if kind == 'Road' and spec.get_editor_property('bAllowFacilityPlacementOnTop'):
                raise RuntimeError('Road retained its obsolete permissive flag: ' + path)
            snapshot = definition_snapshot(definition)
            if snapshot != expected[name][kind.lower() + '_after']:
                raise RuntimeError('Saved Definition differs from migration: ' + path)
            mesh = spec.get_editor_property('visual_mesh')
            thumbnail = definition.get_editor_property('catalog_thumbnail')
            if not isinstance(mesh, unreal.StaticMesh) or not isinstance(thumbnail, unreal.Texture2D):
                raise RuntimeError('Surface visual references did not load: ' + path)
            row[kind.lower()] = snapshot
        rows.append(row)
    table = unreal.load_asset(COST_TABLE)
    if not table:
        raise RuntimeError('Missing facility costs')
    actual_costs = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    if {row['Name']: row for row in actual_costs} != {row['Name']: row for row in before['costs_after']}:
        raise RuntimeError('Persisted prices differ from the migration report')
    result = dict(status='PASS', surfaces=rows, cost_row_count=len(actual_costs))
    (OUT / 'road_tile_split_verification.json').write_text(
        json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('ROAD_TILE_VERIFY_PASS surfaces=' + str(len(rows) * 2))
    return result


if __name__ == '__main__':
    run()
