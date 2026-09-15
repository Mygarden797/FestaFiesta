"""Enable drag placement once on the six shipped road Definitions.

Run in an isolated, unattended editor with Run-RoadEditor.ps1 after compiling.
Preflight every known asset before changing any; protect saves with the existing
recoverable package transaction. The marker preserves later Designer choices on
rerun, including deliberately disabling the feature. No meshes, costs, instance
specifications, or unrelated Definitions are changed.
"""
import hashlib
import json
import shlex
import sys
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(ROOT / 'Tools/facilities'))
from import_roads import OUT, ROADS, definition_path
from ffzs_asset_save_transaction import AssetSaveTransaction

MARKER = 'FestaFiesta.RoadDragPlacementV1'


def require_isolated_editor():
    arguments = {arg.casefold() for arg in shlex.split(
        str(unreal.SystemLibrary.get_command_line()), posix=False)}
    if '-unattended' not in arguments or not (
            '-run=pythonscript' in arguments or
            any(arg.startswith('-executepythonscript=') for arg in arguments)):
        raise RuntimeError('Run with Run-RoadEditor.ps1 in an isolated unattended editor; '
                           'a failed save must terminate without saving dirty objects.')


def inspect_definition(name):
    path = definition_path(name)
    definition = unreal.load_asset(path)
    if not isinstance(definition, unreal.FFZSFacilityDefinition):
        raise RuntimeError('Missing road Definition: ' + path)
    spec = definition.get_editor_property('spec')
    if str(spec.get_editor_property('facility_definition_id')) != 'Road_Surface_' + name:
        raise RuntimeError('Unexpected stable facility ID: ' + path)
    if spec.get_editor_property('category') != unreal.FFZSFacilityCategory.ROAD:
        raise RuntimeError('Expected the Road category: ' + path)
    errors = definition.is_definition_valid()
    if errors is None or len(errors) != 0:
        raise RuntimeError('Invalid Definition: ' + path + ': ' + str(errors))
    # export_text includes the whole reflected Spec, so this catches accidental
    # changes beyond a handpicked set of familiar authoring fields.
    return definition, spec.export_text()


def run():
    require_isolated_editor()
    OUT.mkdir(parents=True, exist_ok=True)
    records = []
    candidates = []
    for name, _label in ROADS:
        definition, spec_text = inspect_definition(name)
        enabled = bool(definition.get_editor_property('enable_drag_placement'))
        marker = str(unreal.EditorAssetLibrary.get_metadata_tag(definition, MARKER) or '')
        if marker not in ('', '1'):
            raise RuntimeError('Unexpected migration marker: ' + definition.get_path_name())
        record = dict(name=name, definition=definition.get_path_name(),
                      before_enabled=enabled, after_enabled=enabled,
                      spec_sha256=hashlib.sha256(spec_text.encode('utf-8')).hexdigest(),
                      changed=marker != '1', preserved_authored_choice=marker == '1')
        records.append(record)
        if marker != '1':
            candidates.append((definition, spec_text, record))

    transaction = AssetSaveTransaction(
        [definition for definition, _spec_text, _record in candidates],
        'RoadDragPlacementV1') if candidates else None
    try:
        for definition, spec_text, record in candidates:
            definition.set_editor_property('enable_drag_placement', True)
            unreal.EditorAssetLibrary.set_metadata_tag(definition, MARKER, '1')
            if definition.get_editor_property('spec').export_text() != spec_text:
                raise RuntimeError('Migration changed the facility specification: ' + definition.get_path_name())
            if not definition.get_editor_property('enable_drag_placement'):
                raise RuntimeError('Drag setting was not applied: ' + definition.get_path_name())
            if not unreal.EditorAssetLibrary.save_loaded_asset(definition, only_if_is_dirty=False):
                raise RuntimeError('Could not save ' + definition.get_path_name())
            record['after_enabled'] = True
        if transaction:
            transaction.commit()
    except Exception:
        if transaction:
            transaction.rollback()
        raise

    result = dict(status='PASS', changed_count=len(candidates), roads=records,
                  fresh_session_verification='Run verify_drag_placement.py in a new editor process.')
    (OUT / 'drag_placement_migration.json').write_text(
        json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('ROAD_DRAG_MIGRATION_PASS changed=' + str(len(candidates)))


if __name__ == '__main__':
    run()
