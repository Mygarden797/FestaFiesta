"""Inspect or repair only SunsetMainStage's Crowd entry reservation.

Run with Unreal PythonScript commandlet. Default mode inspects without saving.
FESTAFIESTA_APPLY_MAINSTAGE_QUEUE_REPAIR=1 enables the backed-up single-asset save.
Run again in a fresh process without that variable to verify persisted data.
"""
import json
import os
import re
import shutil
import sys
import traceback
from datetime import datetime, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from repair_facility_queue_reservations import (
    ROOT, body_failures, digest, make_plan, project_definition, require,
)

ASSET_PATH = (
    '/Game/FestaFiesta/Building/FF_Data_Building/Performance/'
    'PDA_FFZS_Performance_MainStage_SunsetMainStage'
)
DEFINITION_ID = 'Performance_MainStage_SunsetMainStage'


def run():
    import unreal
    from ffzs_asset_save_transaction import AssetSaveTransaction

    require(Path(unreal.Paths.project_dir()).resolve() == ROOT, 'Wrong project')
    source = (ROOT / 'Source/FestaFiesta/Public/CrowdSystem/Types/FF_FestivalCrowdTypes.h').read_text(
        encoding='utf-8-sig')
    require(re.search(r'float\s+Radius\s*=\s*34\.f\s*;', source) is not None,
            'Crowd radius changed; review this repair before applying it')
    apply = os.environ.get('FESTAFIESTA_APPLY_MAINSTAGE_QUEUE_REPAIR') == '1'
    output = ROOT / 'Artifacts/MainStageQueueEntryRepair' / datetime.now(timezone.utc).strftime(
        '%Y%m%d-%H%M%S-%f')
    output.mkdir(parents=True, exist_ok=False)
    original = (ROOT / 'Content' / (ASSET_PATH[len('/Game/'):] + '.uasset')).resolve()
    require(original.is_relative_to((ROOT / 'Content').resolve()), 'Asset escaped Content')
    report = {'status': 'running', 'mode': 'apply' if apply else 'inspect',
              'asset': ASSET_PATH, 'changedAssetCount': 0}
    transaction = None
    try:
        report['beforeSha256'] = digest(original)
        asset = unreal.load_asset(ASSET_PATH)
        require(isinstance(asset, unreal.FFZSFacilityDefinition), 'Expected facility definition')
        before = project_definition(asset)
        require(before['id'] == DEFINITION_ID, 'Unexpected definition identity')
        report['before'] = before
        plan = make_plan(before)
        report['plan'] = plan
        if plan:
            # This repair is justified by exactly the production Entry[0] failure.
            # Refuse to widen unrelated waiting lanes or hide other geometry defects.
            require(all(failure['target'] == 'entry-0' for failure in plan['failuresBefore']),
                    'A non-entry target also needs repair; inspect separately')
            require(all(action['kind'] == 'reserve-entry-corridor' for action in plan['actions']),
                    'The proposal changes geometry outside the entry corridor')
            require(plan['afterRects'][:len(plan['beforeRects'])] == plan['beforeRects'],
                    'The proposal modified an existing reservation')
        if apply and plan:
            backup = output / 'Backups' / original.relative_to(ROOT)
            backup.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(original, backup)
            require(digest(backup) == report['beforeSha256'], 'Backup SHA-256 mismatch')
            report['backup'] = str(backup)
            transaction = AssetSaveTransaction([asset], 'MainStageQueueEntry')
            original_spec = asset.get_editor_property('spec').copy()
            new_spec = original_spec.copy()
            rectangles = []
            for coordinates in plan['afterRects']:
                rectangle = unreal.FFZSCellRect()
                rectangle.set_editor_property('min', unreal.IntPoint(*coordinates[:2]))
                rectangle.set_editor_property('max', unreal.IntPoint(*coordinates[2:]))
                rectangles.append(rectangle)
            new_spec.set_editor_property('queue_reserved_rects', rectangles)
            unchanged = new_spec.copy()
            unchanged.set_editor_property('queue_reserved_rects',
                                          original_spec.get_editor_property('queue_reserved_rects'))
            require(unchanged.export_text() == original_spec.export_text(),
                    'A field other than queue reservations changed')
            asset.set_editor_property('spec', new_spec)
            errors = asset.is_definition_valid()
            require(errors is not None and len(errors) == 0,
                    'Native definition validation failed: ' + str(errors))
            require(not body_failures(project_definition(asset)),
                    'The proposed entry reservation does not protect all base/tier Crowd bodies')
            require(unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False),
                    'Single-asset save failed')
            report['after'] = project_definition(asset)
            require(not body_failures(report['after']), 'Post-save geometry audit failed')
            after_spec = asset.get_editor_property('spec').copy()
            after_spec.set_editor_property('queue_reserved_rects',
                                          original_spec.get_editor_property('queue_reserved_rects'))
            require(after_spec.export_text() == original_spec.export_text(),
                    'Saving changed a field other than queue reservations')
            report['unchangedFieldsVerified'] = True
            transaction.commit()
            transaction = None
            report['changedAssetCount'] = 1
        report['remainingRepairCount'] = int(make_plan(project_definition(asset)) is not None)
        report['afterSha256'] = digest(original)
        if not apply:
            require(report['beforeSha256'] == report['afterSha256'], 'Inspection changed the asset')
        report['status'] = 'passed'
    except Exception:
        report['status'] = 'failed'
        report['error'] = traceback.format_exc()
        if transaction is not None:
            transaction.rollback()
            report['rollbackSha256'] = digest(original)
            require(report['rollbackSha256'] == report['beforeSha256'], 'Rollback SHA-256 mismatch')
        raise
    finally:
        report_path = output / 'report.json'
        report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
        unreal.log('MainStage queue entry report: ' + str(report_path))


if __name__ == '__main__':
    run()
