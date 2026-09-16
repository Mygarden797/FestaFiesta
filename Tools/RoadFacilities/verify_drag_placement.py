"""Fresh-editor acceptance check for the six shipped drag-enabled roads.

Read-only. This initial-delivery check expects all six roads to be enabled.
Authored footprint and top-placement policy are reported, never overwritten or
compared with the original importer defaults. The complete Spec export is written
only to the verification report under Saved for inspection and preservation proof.
"""
import hashlib
import json
import sys
from pathlib import Path

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from enable_drag_placement import MARKER, inspect_definition
from import_roads import OUT, ROADS


def run():
    rows = []
    for name, _label in ROADS:
        definition, spec_text = inspect_definition(name)
        if not definition.get_editor_property('enable_drag_placement'):
            raise RuntimeError('Drag placement is disabled: ' + definition.get_path_name())
        if unreal.EditorAssetLibrary.get_metadata_tag(definition, MARKER) != '1':
            raise RuntimeError('Migration marker is missing: ' + definition.get_path_name())
        spec = definition.get_editor_property('spec')
        footprint = spec.get_editor_property('footprint_size_cells')
        rows.append(dict(name=name, definition=definition.get_path_name(),
                         enable_drag_placement=True,
                         footprint_cm=[int(footprint.x), int(footprint.y)],
                         allow_facility_placement_on_top=bool(spec.get_editor_property('bAllowFacilityPlacementOnTop')),
                         clearance_cm=int(spec.get_editor_property('clearance_cells')),
                         spec_export_text=spec_text,
                         spec_sha256=hashlib.sha256(spec_text.encode('utf-8')).hexdigest()))
    migration_report = OUT / 'drag_placement_migration.json'
    if migration_report.is_file():
        before = json.loads(migration_report.read_text(encoding='utf-8'))
        before_specs = {row['name']: row['spec_sha256'] for row in before['roads']}
        for row in rows:
            if before_specs.get(row['name']) != row['spec_sha256']:
                raise RuntimeError('Saved facility specification differs from migration preflight: ' + row['name'])
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'drag_placement_verification.json').write_text(
        json.dumps(dict(status='PASS', roads=rows), ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('ROAD_DRAG_VERIFY_PASS roads=' + str(len(rows)))


if __name__ == '__main__':
    run()
