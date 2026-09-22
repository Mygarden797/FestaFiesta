# repair_facility_queue_reservations.py
# 역할: 시설 모델·Data Asset·썸네일 작업에서 확인된 데이터 오류만 수정한다.

"""Repair authored Crowd body reservations without moving any facility targets.

Default Unreal commandlet mode only inspects and writes a proposed-change report.
FESTAFIESTA_APPLY_QUEUE_REPAIR=1 enables guarded, backed-up package saves. Run
again without that variable in a fresh process to verify persisted/idempotent data.

Outside Unreal, --plan <inventory.json> validates the same proposal using the
read-only inventory shape produced by project_definition below.
"""
import copy
import hashlib
import json
import math
import os
import re
import shutil
import sys
import traceback
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFINITION_ROOT = '/Game/FestaFiesta/Building/FF_Data_Building'
CROWD_RADIUS_CM = 34.0
BODY_TOLERANCE_CM = 0.01
ROUNDING_MARGIN_CM = 1.0


def require(value, message):
    if not value:
        raise RuntimeError(message)


def scale_coordinate(value, scale, upper):
    # Mirrors FFZSFacilityUpgradeRules::BuildEffectiveSpec centimeter rounding.
    scaled = value * scale
    nearest = math.floor(scaled + 0.5)
    tolerance = min(0.001, abs(scaled) * (2.0 ** -23) * 0.5)
    if abs(scaled - nearest) <= tolerance:
        scaled = nearest
    return math.ceil(scaled) if upper else math.floor(scaled)


def effective(row, scale):
    if scale == 1:
        return row
    footprint = [scale_coordinate(v, scale, True) for v in row['footprint']]
    rects = []
    for original in row['rects']:
        rect = [scale_coordinate(v, scale, index >= 2) for index, v in enumerate(original)]
        for axis in range(2):
            if original[axis] >= row['footprint'][axis]:
                rect[axis] = max(rect[axis], footprint[axis])
                rect[axis + 2] = max(rect[axis + 2], rect[axis] + 1)
        rects.append(rect)
    return dict(row, footprint=footprint, rects=rects,
                points=[[v * scale for v in p] for p in row['points']],
                anchors=[dict(a, local=[v * scale for v in a['local']]) for a in row['anchors']])


def sample(points, distance):
    require(bool(points), 'Queue has no authored points')
    if distance <= 0:
        return points[0]
    for start, end in zip(points, points[1:]):
        length = math.dist(start, end)
        if length <= 1e-4:
            continue
        if distance <= length:
            return [a + (b - a) * distance / length for a, b in zip(start, end)]
        distance -= length
    return points[-1]


def targets(row):
    result = [('slot-' + str(index), sample(row['points'], row['spacing'] * index))
              for index in range(row['waiting'])]
    result += [('entry-0', a['local']) for a in row['anchors']
               if 'ENTRY:' in a['type'] and a['index'] == 0]
    return result


def levels(row):
    return [(1, 1.0)] + [(t['level'], t['scale']) for t in row['tiers'] if row['usesLevels']]


def body_bounds(row, point):
    x, y = [point[i] + row['footprint'][i] / 2 for i in range(2)]
    radius = CROWD_RADIUS_CM - BODY_TOLERANCE_CM
    return [x - radius, y - radius, x + radius, y + radius]


def union_contains(rects, body):
    # Check the full body bounding square, not merely its center or the union's
    # outer bounds. Adjacent rectangles can protect a body jointly.
    xs = sorted(set([body[0], body[2]] + [max(body[0], min(body[2], r[i])) for r in rects for i in (0, 2)]))
    for left, right in zip(xs, xs[1:]):
        x = (left + right) / 2
        intervals = sorted((r[1], r[3]) for r in rects if r[0] <= x <= r[2])
        covered_to = body[1]
        for low, high in intervals:
            if low > covered_to:
                break
            covered_to = max(covered_to, high)
        if covered_to < body[3]:
            return False
    return True


def overlaps_footprint(rect, footprint):
    return rect[0] < footprint[0] and rect[2] > 0 and rect[1] < footprint[1] and rect[3] > 0


def body_failures(row):
    failures = []
    if row['waiting'] <= 0:
        return failures
    for level, scale in levels(row):
        scaled = effective(row, scale)
        for name, point in targets(scaled):
            body = body_bounds(scaled, point)
            if not union_contains(scaled['rects'], body):
                failures.append({'level': level, 'scale': scale, 'target': name, 'body': body})
    return failures


def make_plan(row):
    before_failures = body_failures(row)
    if not before_failures:
        return None
    proposal = copy.deepcopy(row)
    require(all(math.isfinite(s) and s >= 1 for _, s in levels(row)),
            'Shrink-tier geometry needs a separate reviewed repair: ' + row['id'])
    require(row['waiting'] > 0 and bool(row['rects']), 'Missing queue authoring: ' + row['id'])
    actions = []
    for name, point in targets(row):
        if name != 'entry-0' or union_contains(proposal['rects'], body_bounds(proposal, point)):
            continue
        # Reserve only the short entry-to-queue-head corridor. Extending the
        # entire long waiting lane sideways would waste placement area.
        head = row['points'][0]
        radius = CROWD_RADIUS_CM + ROUNDING_MARGIN_CM
        corridor = [math.floor(min(head[i], point[i]) + row['footprint'][i] / 2 - radius) for i in range(2)]
        corridor += [math.ceil(max(head[i], point[i]) + row['footprint'][i] / 2 + radius) for i in range(2)]
        require(not overlaps_footprint(corridor, row['footprint']), 'Entry corridor crosses building: ' + row['id'])
        proposal['rects'].append(corridor)
        actions.append({'kind': 'reserve-entry-corridor', 'rectangle': corridor})

    # Widen only a lane that is actually narrower than the Crowd body. Preserve
    # existing external boundaries and reject changes that cross the building.
    for level, scale in levels(row):
        scaled = effective(proposal, scale)
        for name, point in targets(scaled):
            body = body_bounds(scaled, point)
            if union_contains(scaled['rects'], body):
                continue
            require(name.startswith('slot-'), 'Entry remains exposed after corridor repair: ' + row['id'])
            center = [(body[i] + body[i + 2]) / 2 / scale for i in range(2)]
            candidates = [(index, rect) for index, rect in enumerate(proposal['rects'])
                          if rect[0] <= center[0] <= rect[2] and rect[1] <= center[1] <= rect[3]]
            require(bool(candidates), 'Queue target center has no authored reservation: ' + row['id'])
            index, original = min(candidates, key=lambda pair: (pair[1][2]-pair[1][0]) * (pair[1][3]-pair[1][1]))
            expanded = [min(original[i], math.floor(body[i] / scale - ROUNDING_MARGIN_CM)) for i in range(2)]
            expanded += [max(original[i + 2], math.ceil(body[i + 2] / scale + ROUNDING_MARGIN_CM)) for i in range(2)]
            require(not overlaps_footprint(expanded, row['footprint']), 'Lane widening crosses building: ' + row['id'])
            proposal['rects'][index] = expanded
            actions.append({'kind': 'widen-waiting-lane', 'index': index, 'before': original, 'after': expanded})
            scaled = effective(proposal, scale)
    require(len(proposal['rects']) <= 64, 'Reservation rectangle limit exceeded')
    require(not body_failures(proposal), 'Proposed base/tier reservation still exposes a Crowd body: ' + row['id'])
    return {'id': row['id'], 'path': row['path'], 'beforeRects': row['rects'],
            'afterRects': proposal['rects'], 'failuresBefore': before_failures, 'actions': actions}


def project_definition(asset):
    spec = asset.get_editor_property('spec')
    footprint = spec.get_editor_property('footprint_size_cells')
    rules = spec.get_editor_property('queue_rules')
    return {
        'path': asset.get_path_name(), 'id': str(spec.get_editor_property('facility_definition_id')),
        'category': str(spec.get_editor_property('category')), 'footprint': [footprint.x, footprint.y],
        'clearance': spec.get_editor_property('clearance_cells'),
        'rects': [[r.get_editor_property('min').x, r.get_editor_property('min').y,
                   r.get_editor_property('max').x, r.get_editor_property('max').y]
                  for r in spec.get_editor_property('queue_reserved_rects')],
        'points': [[p.x, p.y, p.z] for p in spec.get_editor_property('queue_spline_points_cm')],
        'waiting': rules.get_editor_property('max_waiting_agents'),
        'spacing': rules.get_editor_property('slot_spacing_cm'),
        'service': rules.get_editor_property('service_capacity'),
        'anchors': [{'type': str(a.get_editor_property('type')), 'index': a.get_editor_property('index'),
                     'local': [a.get_editor_property('local_transform').translation.x,
                               a.get_editor_property('local_transform').translation.y,
                               a.get_editor_property('local_transform').translation.z]}
                    for a in spec.get_editor_property('anchors')],
        'usesLevels': spec.get_editor_property('uses_facility_levels'),
        'tiers': [{'level': t.get_editor_property('target_level'), 'scale': t.get_editor_property('size_multiplier')}
                  for t in spec.get_editor_property('upgrade_tiers')],
    }


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run_unreal():
    import unreal
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from ffzs_asset_save_transaction import AssetSaveTransaction
    require(Path(unreal.Paths.project_dir()).resolve() == ROOT, 'Wrong project')
    source = (ROOT / 'Source/FestaFiesta/Public/CrowdSystem/Types/FF_FestivalCrowdTypes.h').read_text(encoding='utf-8-sig')
    require(re.search(r'float\s+Radius\s*=\s*34\.f\s*;', source) is not None,
            'Crowd body radius changed; update the reviewed repair radius')
    apply = os.environ.get('FESTAFIESTA_APPLY_QUEUE_REPAIR') == '1'
    out = ROOT / 'Artifacts/FacilityQueueReservationRepair' / datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-%f')
    out.mkdir(parents=True, exist_ok=False)
    report = {'mode': 'apply' if apply else 'inspect', 'status': 'running', 'changes': []}
    transaction = None
    try:
        definitions = []
        for path in sorted(unreal.EditorAssetLibrary.list_assets(DEFINITION_ROOT, True, False)):
            asset = unreal.load_asset(path)
            if isinstance(asset, unreal.FFZSFacilityDefinition):
                definitions.append(asset)
        pending = []
        for asset in definitions:
            row = project_definition(asset)
            plan = make_plan(row)
            if plan:
                report['changes'].append(plan)
                pending.append((asset, plan))
        report['definitionCount'] = len(definitions)
        report['changedAssetCount'] = 0
        if apply and pending:
            for asset, plan in pending:
                package = asset.get_path_name().split('.', 1)[0]
                require(package.startswith(DEFINITION_ROOT + '/'), 'Asset escaped definition root')
                original = (ROOT / 'Content' / (package[len('/Game/'):] + '.uasset')).resolve()
                require(original.is_relative_to(ROOT / 'Content'), 'Asset escaped Content root')
                backup = out / 'Backups' / original.relative_to(ROOT)
                backup.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(original, backup)
                require(digest(original) == digest(backup), 'Backup digest mismatch')
                plan['backup'] = str(backup)
                plan['beforeSha256'] = digest(original)
            for asset, plan in pending:
                original_spec = asset.get_editor_property('spec').copy()
                new_spec = original_spec.copy()
                rectangles = []
                for coordinates in plan['afterRects']:
                    rect = unreal.FFZSCellRect()
                    rect.set_editor_property('min', unreal.IntPoint(*coordinates[:2]))
                    rect.set_editor_property('max', unreal.IntPoint(*coordinates[2:]))
                    rectangles.append(rect)
                new_spec.set_editor_property('queue_reserved_rects', rectangles)
                unchanged = new_spec.copy()
                unchanged.set_editor_property('queue_reserved_rects', original_spec.get_editor_property('queue_reserved_rects'))
                require(unchanged.export_text() == original_spec.export_text(), 'A field other than queue reservations changed')
                asset.set_editor_property('spec', new_spec)
                # A bool + out-array UFUNCTION is exposed as Array[Text] on
                # success and None on failure, rather than a Python tuple.
                errors = asset.is_definition_valid()
                require(errors is not None and len(errors) == 0, 'Native definition validation failed: ' + str(errors))
                require(not body_failures(project_definition(asset)), 'In-memory geometry audit failed')
            # Every proposal is validated before the first disk save.
            transaction = AssetSaveTransaction([a for a, _ in pending], 'QueueBodyReservationRepair')
            require(Path(transaction._backup_root).resolve().is_relative_to(ROOT / 'Saved'), 'Transaction backup escaped Saved')
            for asset, plan in pending:
                require(unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False), 'Asset save failed: ' + asset.get_path_name())
                require(not body_failures(project_definition(asset)), 'Post-save geometry audit failed')
                plan['saved'] = True
            transaction.commit()
            transaction = None
            report['changedAssetCount'] = len(pending)
        report['remainingRepairCount'] = len(pending) if not apply else 0
        report['status'] = 'passed'
    except Exception:
        report['status'] = 'failed'
        report['error'] = traceback.format_exc()
        if transaction:
            transaction.rollback()
        raise
    finally:
        (out / 'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')


if __name__ == '__main__':
    if len(sys.argv) > 2 and sys.argv[1] == '--plan':
        rows = json.loads(Path(sys.argv[2]).read_text(encoding='utf-8'))
        plans = [plan for row in rows if (plan := make_plan(row))]
        output = Path(sys.argv[2]).with_name('facility_queue_repair_plan.json')
        output.write_text(json.dumps(plans, indent=2), encoding='utf-8')
        print(json.dumps({'definitions': len(rows), 'changedDefinitions': len(plans),
                          'exposedBodiesBefore': sum(len(p['failuresBefore']) for p in plans), 'plan': str(output)}))
    else:
        run_unreal()
