"""Split the six authored Road definitions into Road and buildable Tile assets.

Run only after compiling the Tile category, through Run-RoadEditor.ps1. Existing
Road IDs, authored sizes and all other Spec fields survive. New Tiles copy the
complete Road Definition, then receive their own identity and cost key. Existing
Tiles and cost rows are preserved on rerun. A recoverable package transaction
covers all six Roads, six Tiles and the shared price table before mutation.
"""
import copy
import hashlib
import json
import sys
import traceback
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(ROOT / 'Tools/facilities'))
from import_roads import COST_TABLE, OUT, ROADS, definition_path
from enable_drag_placement import require_isolated_editor
from ffzs_asset_save_transaction import AssetSaveTransaction

TILE_DEFINITIONS = '/Game/FestaFiesta/Building/FF_Data_Building/Tile'
TILE_LABELS = dict(Sand='모래 타일', Asphalt='아스팔트 타일', Wood='나무 데크 타일',
                   Stone='석재 타일', Herringbone='헤링본 벽돌 타일', Concrete='콘크리트 타일')


def tile_definition_path(name):
    return TILE_DEFINITIONS + '/PDA_FFZS_Tile_Surface_' + name


def spec_digest(spec):
    return hashlib.sha256(spec.export_text().encode('utf-8')).hexdigest()


def validate_definition(definition, name, kind):
    path = definition_path(name) if kind == 'Road' else tile_definition_path(name)
    if not isinstance(definition, unreal.FFZSFacilityDefinition):
        raise RuntimeError('Missing ' + kind + ' Definition: ' + path)
    spec = definition.get_editor_property('spec')
    if str(spec.get_editor_property('facility_definition_id')) != kind + '_Surface_' + name:
        raise RuntimeError('Unexpected stable facility ID: ' + path)
    if spec.get_editor_property('category') != getattr(unreal.FFZSFacilityCategory, kind.upper()):
        raise RuntimeError('Unexpected category: ' + path)
    errors = definition.is_definition_valid()
    if errors is None or len(errors) != 0:
        raise RuntimeError('Invalid Definition: ' + path + ': ' + str(errors))
    return spec


def definition_snapshot(definition):
    spec = definition.get_editor_property('spec')
    footprint = spec.get_editor_property('footprint_size_cells')
    thumbnail = definition.get_editor_property('catalog_thumbnail')
    return dict(definition=definition.get_path_name(),
                spec_sha256=spec_digest(spec), spec_export_text=spec.export_text(),
                footprint_cm=[int(footprint.x), int(footprint.y)],
                enable_drag_placement=bool(definition.get_editor_property('enable_drag_placement')),
                show_in_build_catalog=bool(definition.get_editor_property('show_in_build_catalog')),
                thumbnail=thumbnail.get_path_name() if thumbnail else None)


def save(definition):
    if not unreal.EditorAssetLibrary.save_loaded_asset(definition, only_if_is_dirty=False):
        raise RuntimeError('Could not save ' + definition.get_path_name())


def run():
    require_isolated_editor()
    if not hasattr(unreal.FFZSFacilityCategory, 'TILE'):
        raise RuntimeError('Compile the Tile category before migrating assets')
    OUT.mkdir(parents=True, exist_ok=True)
    table = unreal.load_asset(COST_TABLE)
    if not isinstance(table, unreal.DataTable):
        raise RuntimeError('Missing facility cost table: ' + COST_TABLE)
    cost_rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    costs = {row['Name']: row for row in cost_rows}
    if len(costs) != len(cost_rows):
        raise RuntimeError('Duplicate facility cost row names')

    pending = []
    additions = []
    records = []
    # Preflight every source and existing target before changing any loaded asset.
    for name, _label in ROADS:
        road = unreal.load_asset(definition_path(name))
        road_spec = validate_definition(road, name, 'Road').copy()
        # Hidden compatibility UPROPERTY is accessible by its native name only.
        # Prove reflection and assignment work on a detached copy before backup
        # or mutation, so an uncompiled/incompatible editor cannot partially save.
        compatibility_probe = road_spec.copy()
        compatibility_probe.set_editor_property('bAllowFacilityPlacementOnTop',
                                                road_spec.get_editor_property('bAllowFacilityPlacementOnTop'))
        tile_path = tile_definition_path(name)
        tile = unreal.load_asset(tile_path) if unreal.EditorAssetLibrary.does_asset_exist(tile_path) else None
        if tile:
            validate_definition(tile, name, 'Tile')
        source_key = str(road_spec.get_editor_property('install_cost_key'))
        if source_key in ('', 'None'):
            source_key = 'Cost.Road.Surface.' + name
        target_key = ('Cost.Tile.Surface.' + name if not tile else
                      str(tile.get_editor_property('spec').get_editor_property('install_cost_key')))
        if target_key in ('', 'None'):
            target_key = 'Cost.Tile.Surface.' + name
        if source_key not in costs:
            raise RuntimeError('Missing authored Road cost row: ' + source_key)
        if target_key not in costs:
            if tile and target_key != 'Cost.Tile.Surface.' + name:
                raise RuntimeError('Existing Tile has an unresolved authored cost key: ' + target_key)
            row = copy.deepcopy(costs[source_key])
            row['Name'] = target_key
            additions.append(row)
            costs[target_key] = row
        record = dict(name=name, road_before=definition_snapshot(road),
                      tile_before=definition_snapshot(tile) if tile else None,
                      created_tile=tile is None, road_cost_key=source_key, tile_cost_key=target_key)
        records.append(record)
        pending.append((name, road, road_spec, tile, record))

    packages = [COST_TABLE]
    packages += [definition_path(name) for name, _label in ROADS]
    packages += [tile_definition_path(name) for name, _label in ROADS]
    transaction = AssetSaveTransaction(packages, 'RoadTileSplitV1')
    saved = []
    try:
        for name, road, original_spec, tile, record in pending:
            road_spec = original_spec.copy()
            road_spec.set_editor_property('bAllowFacilityPlacementOnTop', False)
            if road_spec.export_text() != original_spec.export_text():
                road.set_editor_property('spec', road_spec)
                validate_definition(road, name, 'Road')
                unchanged = road.get_editor_property('spec').copy()
                unchanged.set_editor_property('bAllowFacilityPlacementOnTop',
                                              original_spec.get_editor_property('bAllowFacilityPlacementOnTop'))
                if unchanged.export_text() != original_spec.export_text():
                    raise RuntimeError('Road fields changed beyond the retired placement flag: ' + name)
                save(road)
                saved.append(road.get_path_name())
            if tile is None:
                tile = unreal.EditorAssetLibrary.duplicate_asset(definition_path(name), tile_definition_path(name))
                if not tile:
                    raise RuntimeError('Could not duplicate Road into Tile: ' + name)
                spec = tile.get_editor_property('spec').copy()
                edits = dict(facility_definition_id='Tile_Surface_' + name,
                             display_name=unreal.Text(TILE_LABELS[name]),
                             category=unreal.FFZSFacilityCategory.TILE,
                             install_cost_key='Cost.Tile.Surface.' + name,
                             bAllowFacilityPlacementOnTop=True)
                for key, value in edits.items():
                    spec.set_editor_property(key, value)
                tile.set_editor_property('spec', spec)
                validate_definition(tile, name, 'Tile')
                unchanged = tile.get_editor_property('spec').copy()
                for key in edits:
                    unchanged.set_editor_property(key, road_spec.get_editor_property(key))
                if unchanged.export_text() != road_spec.export_text():
                    raise RuntimeError('Tile copy changed additional authored fields: ' + name)
                save(tile)
                saved.append(tile.get_path_name())
            record['road_after'] = definition_snapshot(road)
            record['tile_after'] = definition_snapshot(tile)
            if record['tile_before'] and record['tile_before'] != record['tile_after']:
                raise RuntimeError('Existing authored Tile changed: ' + name)
        if additions:
            if not unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(cost_rows + additions)):
                raise RuntimeError('Could not add Tile prices')
            save(table)
            saved.append(table.get_path_name())
        after_rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
        if {row['Name']: row for row in after_rows} != costs:
            raise RuntimeError('Facility prices changed beyond missing Tile rows')
        transaction.commit()
    except Exception:
        transaction.rollback()
        (OUT / 'road_tile_split_error.txt').write_text(traceback.format_exc(), encoding='utf-8')
        raise

    result = dict(status='PASS', surfaces=records, added_cost_rows=additions,
                  saved_packages=saved, costs_after=after_rows,
                  fresh_session_verification='Run verify_road_tiles.py in a new editor process.')
    (OUT / 'road_tile_split_migration.json').write_text(
        json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('ROAD_TILE_SPLIT_PASS saved=' + str(len(saved)))
    return result


if __name__ == '__main__':
    run()
