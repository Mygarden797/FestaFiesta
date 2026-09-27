# test_drag_placement_migration.py
# 역할: 설치 가능한 도로·타일 작업에서 오류 경계와 기존 데이터 보존 동작을 검증한다.

"""Exercise migration failure boundaries without starting Unreal or touching assets."""
import copy
import importlib.util
import json
import sys
import tempfile
import types
import unittest
from pathlib import Path
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[1] / 'enable_drag_placement.py'


class Definition:
    def __init__(self, path, name):
        self.path, self.name = path, name
        self.enabled = False
        self.marker = ''

    def get_path_name(self):
        return self.path

    def get_editor_property(self, key):
        if key == 'enable_drag_placement':
            return self.enabled
        if key == 'spec':
            return types.SimpleNamespace(
                get_editor_property=lambda prop: {
                    'facility_definition_id': 'Road_Surface_' + self.name,
                    'category': 'ROAD'}[prop],
                export_text=lambda: 'unchanged full spec: ' + self.name)
        raise KeyError(key)

    def set_editor_property(self, key, value):
        if key != 'enable_drag_placement':
            raise AssertionError('Unexpected mutation: ' + key)
        self.enabled = value

    def is_definition_valid(self):
        return []


class DragMigrationTests(unittest.TestCase):
    def setUp(self):
        self.assets, self.disk, self.events = {}, {}, []
        self.fail_save_number = None
        self.save_count = 0
        self.command_line = '-unattended -ExecutePythonScript="test.py"'
        fake_unreal = types.ModuleType('unreal')
        fake_unreal.FFZSFacilityDefinition = Definition
        fake_unreal.FFZSFacilityCategory = types.SimpleNamespace(ROAD='ROAD')
        fake_unreal.SystemLibrary = types.SimpleNamespace(get_command_line=lambda: self.command_line)
        fake_unreal.load_asset = self.assets.get
        fake_unreal.log = lambda message: self.events.append(message)
        fake_unreal.EditorAssetLibrary = types.SimpleNamespace(
            get_metadata_tag=lambda asset, key: asset.marker,
            set_metadata_tag=lambda asset, key, value: setattr(asset, 'marker', value),
            save_loaded_asset=self.save)
        transaction_module = types.ModuleType('ffzs_asset_save_transaction')
        fixture = self

        class Transaction:
            def __init__(self, assets, label):
                fixture.events.append('transaction')
                self.before = copy.deepcopy(fixture.disk)

            def commit(self):
                fixture.events.append('commit')

            def rollback(self):
                fixture.events.append('rollback')
                fixture.disk = self.before

        transaction_module.AssetSaveTransaction = Transaction
        self.modules = patch.dict(sys.modules, {
            'unreal': fake_unreal, 'ffzs_asset_save_transaction': transaction_module})
        self.modules.start()
        self.addCleanup(self.modules.stop)
        # import_roads contains only constant/function declarations, so this loads
        # the real six-target contract while keeping every engine call in the fake.
        sys.modules.pop('import_roads', None)
        spec = importlib.util.spec_from_file_location('drag_migration_under_test', SCRIPT)
        self.migration = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.migration)
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.migration.OUT = Path(self.temp.name)
        for name, _label in self.migration.ROADS:
            path = self.migration.definition_path(name)
            self.assets[path] = Definition(path, name)
            self.disk[path] = dict(enabled=False, marker='')

    def save(self, asset, only_if_is_dirty):
        self.save_count += 1
        if self.save_count == self.fail_save_number:
            return False
        self.disk[asset.path] = dict(enabled=asset.enabled, marker=asset.marker)
        return True

    def test_all_assets_preflight_before_first_write(self):
        self.assets.pop(self.migration.definition_path('Concrete'))
        with self.assertRaisesRegex(RuntimeError, 'Missing road Definition'):
            self.migration.run()
        self.assertEqual(self.save_count, 0)
        self.assertNotIn('transaction', self.events)
        self.assertTrue(all(not asset.enabled for asset in self.assets.values()))

    def test_partial_save_failure_rolls_back_and_never_reports_success(self):
        before = copy.deepcopy(self.disk)
        self.fail_save_number = 3
        with self.assertRaisesRegex(RuntimeError, 'Could not save'):
            self.migration.run()
        self.assertEqual(self.disk, before)
        self.assertIn('rollback', self.events)
        self.assertNotIn('commit', self.events)
        self.assertFalse((self.migration.OUT / 'drag_placement_migration.json').exists())

    def test_success_and_rerun_preserve_later_manual_disable(self):
        self.migration.run()
        self.assertEqual(self.save_count, 6)
        self.assertTrue(all(row == dict(enabled=True, marker='1') for row in self.disk.values()))
        self.assets[self.migration.definition_path('Sand')].enabled = False
        self.events.clear()
        self.migration.run()
        self.assertEqual(self.save_count, 6)
        self.assertNotIn('transaction', self.events)
        self.assertFalse(self.assets[self.migration.definition_path('Sand')].enabled)
        report = json.loads((self.migration.OUT / 'drag_placement_migration.json').read_text(encoding='utf-8'))
        self.assertEqual(report['changed_count'], 0)
        self.assertTrue(all(row['preserved_authored_choice'] for row in report['roads']))

    def test_interactive_editor_is_rejected_before_mutation(self):
        self.command_line = ''
        with self.assertRaisesRegex(RuntimeError, 'isolated unattended editor'):
            self.migration.run()
        self.assertEqual(self.save_count, 0)
        self.assertFalse(self.events)


if __name__ == '__main__':
    unittest.main()
