# test_surface_split_migration.py
# 역할: 설치 가능한 도로·타일 작업에서 오류 경계와 기존 데이터 보존 동작을 검증한다.

"""Exercise package failure boundaries and authored-data preservation without UE."""
import copy
import importlib.util
import json
import sys
import tempfile
import types
import unittest
from pathlib import Path
from unittest.mock import patch


class Spec:
    def __init__(self, name):
        self.values = dict(facility_definition_id='Road_Surface_' + name, category='ROAD',
                           display_name='Authored ' + name, install_cost_key='None',
                           bAllowFacilityPlacementOnTop=True,
                           footprint_size_cells=types.SimpleNamespace(x=200, y=350),
                           placement_obstacle_height_cm=17.0, authored_extra='retain me')

    def get_editor_property(self, key):
        return self.values[key]

    def set_editor_property(self, key, value):
        if key not in self.values:
            raise KeyError(key)
        self.values[key] = value

    def copy(self):
        return copy.deepcopy(self)

    def export_text(self):
        return json.dumps(self.values, sort_keys=True, default=vars)


class Definition:
    def __init__(self, path, name):
        self.path = path
        self.values = dict(spec=Spec(name), catalog_thumbnail=None,
                           enable_drag_placement=True, show_in_build_catalog=True)

    def get_path_name(self):
        return self.path

    def get_editor_property(self, key):
        return self.values[key]

    def set_editor_property(self, key, value):
        self.values[key] = value

    def is_definition_valid(self):
        return []


class Table:
    def __init__(self, path, rows):
        self.path, self.rows = path, rows

    def get_path_name(self):
        return self.path


class SurfaceSplitTests(unittest.TestCase):
    def setUp(self):
        self.assets, self.disk, self.events = {}, {}, []
        self.save_count = 0
        self.fail_save_number = None
        fake = types.ModuleType('unreal')
        fake.FFZSFacilityDefinition, fake.DataTable = Definition, Table
        fake.FFZSFacilityCategory = types.SimpleNamespace(ROAD='ROAD', TILE='TILE')
        fake.Text = str
        fake.load_asset = self.assets.get
        fake.log = self.events.append
        fake.SystemLibrary = types.SimpleNamespace(get_command_line=lambda: '-unattended -ExecutePythonScript="test.py"')
        fake.EditorAssetLibrary = types.SimpleNamespace(
            does_asset_exist=lambda path: path in self.assets,
            duplicate_asset=self.duplicate, save_loaded_asset=self.save)
        fake.DataTableFunctionLibrary = types.SimpleNamespace(
            export_data_table_to_json_string=lambda table: json.dumps(table.rows),
            fill_data_table_from_json_string=self.fill_table)
        transaction_module = types.ModuleType('ffzs_asset_save_transaction')
        fixture = self

        class Transaction:
            def __init__(self, packages, label):
                fixture.events.append('transaction')
                self.before = copy.deepcopy(fixture.disk)

            def commit(self):
                fixture.events.append('commit')

            def rollback(self):
                fixture.events.append('rollback')
                fixture.disk = self.before

        transaction_module.AssetSaveTransaction = Transaction
        self.modules = patch.dict(sys.modules, {'unreal': fake, 'ffzs_asset_save_transaction': transaction_module})
        self.modules.start()
        self.addCleanup(self.modules.stop)
        for module in ('import_roads', 'enable_drag_placement'):
            sys.modules.pop(module, None)
        script = Path(__file__).resolve().parents[1] / 'split_road_tiles.py'
        spec = importlib.util.spec_from_file_location('surface_split_under_test', script)
        self.migration = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.migration)
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.migration.OUT = Path(self.temp.name)
        for name, _label in self.migration.ROADS:
            path = self.migration.definition_path(name)
            self.assets[path] = Definition(path, name)
        table = Table(self.migration.COST_TABLE, [dict(Name='Cost.Road.Surface.' + name, InstallCost=37,
                                                     AuthoredExtra='preserve entire row')
                                                 for name, _label in self.migration.ROADS])
        self.assets[table.path] = table
        self.disk = copy.deepcopy(self.assets)

    def duplicate(self, source, destination):
        asset = copy.deepcopy(self.assets[source])
        asset.path = destination
        self.assets[destination] = asset
        # Unreal duplicate_asset may save the duplicate before callers edit it.
        self.disk[destination] = copy.deepcopy(asset)
        return asset

    def save(self, asset, only_if_is_dirty):
        self.save_count += 1
        if self.save_count == self.fail_save_number:
            return False
        self.disk[asset.path] = copy.deepcopy(asset)
        return True

    @staticmethod
    def fill_table(table, content):
        table.rows = json.loads(content)
        return True

    @staticmethod
    def disk_snapshot(disk):
        return {path: (asset.rows if isinstance(asset, Table) else asset.values['spec'].export_text())
                for path, asset in disk.items()}

    def test_missing_source_prevents_any_mutation(self):
        self.assets.pop(self.migration.definition_path('Concrete'))
        with self.assertRaisesRegex(RuntimeError, 'Missing Road Definition'):
            self.migration.run()
        self.assertEqual(self.save_count, 0)
        self.assertNotIn('transaction', self.events)
        self.assertTrue(all(asset.values['spec'].values['bAllowFacilityPlacementOnTop']
                            for asset in self.assets.values() if isinstance(asset, Definition)))

    def test_partial_save_failure_restores_existing_and_new_packages(self):
        before = self.disk_snapshot(self.disk)
        self.fail_save_number = 3
        with self.assertRaisesRegex(RuntimeError, 'Could not save'):
            self.migration.run()
        self.assertEqual(self.disk_snapshot(self.disk), before)
        self.assertIn('rollback', self.events)
        self.assertNotIn('commit', self.events)
        self.assertFalse((self.migration.OUT / 'road_tile_split_migration.json').exists())

    def test_split_preserves_authored_fields_and_rerun_preserves_tile_edits(self):
        result = self.migration.run()
        self.assertEqual(len(result['saved_packages']), 13)
        for name, _label in self.migration.ROADS:
            road = self.assets[self.migration.definition_path(name)]
            tile = self.assets[self.migration.tile_definition_path(name)]
            self.assertFalse(road.values['spec'].values['bAllowFacilityPlacementOnTop'])
            self.assertEqual(tile.values['spec'].values['authored_extra'], 'retain me')
            self.assertEqual(tile.values['spec'].values['footprint_size_cells'].y, 350)
            self.assertEqual(tile.values['spec'].values['placement_obstacle_height_cm'], 17)
        tile = self.assets[self.migration.tile_definition_path('Sand')]
        tile.values['enable_drag_placement'] = False
        tile.values['spec'].values['authored_extra'] = 'later Tile edit'
        table = self.assets[self.migration.COST_TABLE]
        for row in table.rows:
            if row['Name'] == 'Cost.Tile.Surface.Sand':
                row['InstallCost'] = 91
        result = self.migration.run()
        self.assertEqual(result['saved_packages'], [])
        self.assertEqual(result['added_cost_rows'], [])
        self.assertEqual(self.save_count, 13)
        self.assertFalse(tile.values['enable_drag_placement'])
        self.assertEqual(tile.values['spec'].values['authored_extra'], 'later Tile edit')
        self.assertEqual(next(row['InstallCost'] for row in table.rows if row['Name'] == 'Cost.Tile.Surface.Sand'), 91)


if __name__ == '__main__':
    unittest.main()
