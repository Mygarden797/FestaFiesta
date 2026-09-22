# rebuild_packaging_navigation.py
# 역할: 에셋·물리 준비를 마치고 NavMesh를 재생성한 뒤 기존 연결을 보존하는지 검사한다.

"""Audit/rebuild Test_Main's offline navigation without changing terrain or agents.

Default: read-only audit. FESTAFIESTA_REBUILD_NAVIGATION=1 rebuilds in memory,
backs up the map and saves only if every previously reachable sample/zone remains
reachable. The 9x9 samples diagnose the offline bake; the PIE automation separately
checks runtime zone anchors, normalized gates, and the actual crowd arrival radius.
"""
import hashlib
import json
import os
import shutil
from datetime import datetime, timezone
from pathlib import Path

import unreal


def vec(value):
    # Keep full coordinates in the save gate; display rounding could hide movement.
    return [value.x, value.y, value.z]


def coverage_regressions(before, after):
    """Pure comparison, keyed by authored zone identity and stable lattice index."""
    before_zones = {row['zone']: row for row in before['zones']}
    after_zones = {row['zone']: row for row in after['zones']}
    before_samples = {(row['zone'], sample['sample_id']): sample
                      for row in before['zones'] for sample in row['samples']}
    after_samples = {(row['zone'], sample['sample_id']): sample
                     for row in after['zones'] for sample in row['samples']}
    successful_before = {key for key, sample in before_samples.items() if sample['status'] == 'reachable'}
    successful_after = {key for key, sample in after_samples.items() if sample['status'] == 'reachable'}
    successful_zones_before = {key for key, row in before_zones.items() if row['reachable']}
    successful_zones_after = {key for key, row in after_zones.items() if row['reachable']}
    changed_samples = []
    for key in before_samples.keys() | after_samples.keys():
        old, new = before_samples.get(key), after_samples.get(key)
        if old is None or new is None or any(old[field] != new[field]
                                            for field in ('local', 'requested_world', 'inside_polygon')):
            changed_samples.append(key)
    return {
        'lost_reachable_zones': sorted(successful_zones_before - successful_zones_after),
        'lost_reachable_samples': sorted(successful_before - successful_after),
        'changed_sample_inputs': sorted(changed_samples),
        'changed_zone_boundaries': sorted(key for key in before_zones.keys() | after_zones.keys()
                                          if key not in before_zones or key not in after_zones
                                          or before_zones[key]['polygon_local'] != after_zones[key]['polygon_local']),
        'changed_exit_input': before['exit_requested'] != after['exit_requested'],
        'exit_unprojected_after': after['exit'] is None,
        'no_reachable_samples_after': not successful_after,
    }


def main():
    project = Path(unreal.Paths.project_dir()).resolve()
    folder = project / 'Artifacts/NavigationRepair' / datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-%f')
    folder.mkdir(parents=True)
    report_path = folder / 'report.json'
    report = {'schema_version': 2, 'saved': False}

    def write_report():
        report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')

    try:
        world = unreal.EditorLoadingAndSavingUtils.load_map('/Game/FestaFiesta/Maps/Test_Main')
        assert world, 'Release map did not load'
        # 비동기 메시 컴파일 중에는 바닥의 물리/내비게이션 정보가 아직 없을 수 있다.
        assert unreal.FFZSNavigationEditorLibrary.prepare_world_for_navigation(world), 'World geometry is not ready'
        actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
        zones = sorted((actor for actor in actors if isinstance(actor, unreal.FFZSGameplayZone)),
                       key=lambda actor: str(actor.get_editor_property('zone_id')))
        zone_ids = [str(zone.get_editor_property('zone_id')) for zone in zones]
        assert zones and len(zone_ids) == len(set(zone_ids)), 'Expected nonempty, uniquely identified zones'
        assert all(zone_id not in ('', 'None') for zone_id in zone_ids), 'A zone identity is missing'
        spawns = [actor for actor in actors if isinstance(actor, unreal.FF_SpawnActor)]
        assert len(spawns) == 1, 'Expected exactly one production crowd spawn actor'
        spawn = spawns[0]
        navmeshes = [actor for actor in actors if isinstance(actor, unreal.RecastNavMesh)]
        assert len(navmeshes) == 1, 'Expected exactly one production Recast navigation mesh'
        nav = navmeshes[0]

        def get_exit_point():
            # Match AFF_SpawnActor::BeginPlay's fallback destination. PIE automation
            # also checks entry-gate destinations resolved by the running crowd system.
            override = spawn.get_editor_property('DespawnAchor')
            return override.get_actor_location() if override else (
                spawn.get_actor_location() + spawn.get_editor_property('Temp_ExitOffsetFromOrigin'))

        def project_point(point):
            # Same projection extent as FFF_CrowdNav::GetAnchorProjectionExtentCm.
            return unreal.NavigationSystemV1.project_point_to_navigation(
                world, point, nav, None, unreal.Vector(300, 300, 2000))

        def audit():
            exit_point = get_exit_point()
            target = project_point(exit_point)
            records = []
            for zone in zones:
                polygon = zone.get_boundary_polygon_local_cm()
                assert len(polygon) >= 3, 'Zone {} has no usable boundary polygon'.format(zone.get_actor_label())
                min_x, max_x = min(p.x for p in polygon), max(p.x for p in polygon)
                min_y, max_y = min(p.y for p in polygon), max(p.y for p in polygon)
                assert max_x > min_x and max_y > min_y, 'Zone boundary has zero extent'
                transform = zone.get_actor_transform()
                row = {'zone': str(zone.get_editor_property('zone_id')),
                       'origin': vec(zone.get_actor_location()),
                       'polygon_local': [[p.x, p.y] for p in polygon],
                       'reachable': [], 'unreachable': [], 'unprojected': [],
                       'outside_polygon': [], 'target_unprojected': [], 'samples': []}
                for iy in range(9):
                    for ix in range(9):
                        # Preserve the old 10,000 cm square's 625..9,375 grid for that
                        # shape. Bounds and actor transform support other sizes/rotations.
                        # Concave areas use the zone's own ContainsWorldPoint predicate.
                        local = unreal.Vector(min_x + (max_x - min_x) * (1 / 16 + ix * 7 / 64),
                                              min_y + (max_y - min_y) * (1 / 16 + iy * 7 / 64), 0)
                        point = unreal.MathLibrary.transform_location(transform, local)
                        inside = zone.contains_world_point(point)
                        sample = {'sample_id': '{}:{}'.format(ix, iy), 'ix': ix, 'iy': iy,
                                  'local': vec(local), 'requested_world': vec(point),
                                  'inside_polygon': inside, 'projected_world': None}
                        if not inside:
                            sample['status'] = 'outside_polygon'
                        else:
                            projected = project_point(point)
                            if projected is None:
                                sample['status'] = 'unprojected'
                            else:
                                sample['projected_world'] = vec(projected)
                                if target is None:
                                    sample['status'] = 'target_unprojected'
                                else:
                                    path = unreal.NavigationSystemV1.find_path_to_location_synchronously(
                                        world, projected, target, nav)
                                    reachable = (path is not None and path.is_valid() and not path.is_partial()
                                                 and len(path.path_points) > 0)
                                    sample['status'] = 'reachable' if reachable else 'unreachable'
                        row['samples'].append(sample)
                        # Preserve summary coordinate arrays; indexed samples are
                        # authoritative for before/after comparisons.
                        row[sample['status']].append(sample['projected_world'] or sample['requested_world'])
                row['sample_counts'] = {status: len(row[status]) for status in (
                    'reachable', 'unreachable', 'unprojected', 'outside_polygon', 'target_unprojected')}
                records.append(row)
            return {'exit_requested': vec(exit_point), 'exit': vec(target) if target is not None else None,
                    'target_kind': 'BeginPlay fallback exit; runtime gate routes verified by PIE automation',
                    'reachable_zones': sum(bool(row['reachable']) for row in records),
                    'sample_counts': {status: sum(row['sample_counts'][status] for row in records)
                                      for status in ('reachable', 'unreachable', 'unprojected',
                                                     'outside_polygon', 'target_unprojected')},
                    'zones': records}

        before = audit()
        report['before'] = before
        write_report()
        if os.environ.get('FESTAFIESTA_REBUILD_NAVIGATION') == '1':
            source = project / 'Content/FestaFiesta/Maps/Test_Main.umap'
            backup = folder / source.name
            report['before_sha256'] = hashlib.sha256(source.read_bytes()).hexdigest()
            shutil.copy2(source, backup)
            assert hashlib.sha256(backup.read_bytes()).hexdigest() == report['before_sha256'], 'Backup verification failed'
            report['backup'] = str(backup)
            write_report()
            # UNavigationSystemV1::Build waits for EnsureBuildCompletion on each NavData.
            unreal.SystemLibrary.execute_console_command(world, 'RebuildNavigation')
            after = audit()
            report['after'] = after
            report['save_gate'] = coverage_regressions(before, after)
            write_report()
            assert not any(report['save_gate'].values()), 'Navigation coverage/input regression; map was not saved (see report.json)'
            assert unreal.EditorLoadingAndSavingUtils.save_map(world, '/Game/FestaFiesta/Maps/Test_Main'), 'Map save failed'
            report['saved'] = True
            report['after_sha256'] = hashlib.sha256(source.read_bytes()).hexdigest()
    except Exception as error:
        report['error'] = '{}: {}'.format(type(error).__name__, error)
        raise
    finally:
        write_report()
        unreal.log('NAVIGATION_REPAIR_REPORT: {} before={} after={} saved={}'.format(
            folder, report.get('before', {}).get('reachable_zones'),
            report.get('after', {}).get('reachable_zones'), report['saved']))


if __name__ == '__main__':
    main()
