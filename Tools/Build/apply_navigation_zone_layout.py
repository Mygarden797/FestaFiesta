# apply_navigation_zone_layout.py
# 역할: 승인된 존 분할 계획을 검증하고, 지형을 보존한 채 경계·출입점·이웃 목록을 함께 저장한다.
"""Run through Unreal's Python commandlet. The JSON plan owns map-specific data.

FESTAFIESTA_ZONE_LAYOUT_PLAN selects a plan. By default changes stay in memory.
FESTAFIESTA_APPLY_ZONE_LAYOUT=1 permits a verified backup and map-only save.
Always follow a save with production PIE navigation tests in a fresh process.
"""
import hashlib
import json
import math
import os
import shutil
from datetime import datetime, timezone
from pathlib import Path

import unreal


def vec(value):
    """직렬화할 좌표를 반올림하지 않아 작은 위치 변경도 확인한다."""
    return [value.x, value.y, value.z]


def rectangle(origin, size):
    """원점과 실제 spline 크기를 월드 XY 직사각형으로 변환한다."""
    return [origin[0], origin[1], origin[0] + size[0], origin[1] + size[1]]


def transform_state(value):
    """Unreal repr의 임시 메모리 주소 대신 실제 transform 수치만 비교한다."""
    return {'location': vec(value.translation), 'scale': vec(value.scale3d),
            'rotation': [value.rotation.x, value.rotation.y, value.rotation.z, value.rotation.w]}


def adjacent(a, b, gap):
    """설정 거리 안에서 변을 마주한 존만 이웃으로 삼고 꼭짓점 접촉은 제외한다."""
    overlap_x = min(a[2], b[2]) - max(a[0], b[0])
    overlap_y = min(a[3], b[3]) - max(a[1], b[1])
    return (overlap_x > 0 and overlap_y >= -gap) or (overlap_y > 0 and overlap_x >= -gap)


def split_layout(plan):
    """donor 영역 안에서 두 존을 정확히 분할해 새로운 겹침 영역이 생기지 않게 한다."""
    assert math.isfinite(plan['minimum_side_cm']) and plan['minimum_side_cm'] > 0
    assert math.isfinite(plan['anchor_boundary_margin_cm']) and plan['anchor_boundary_margin_cm'] >= 0
    assert math.isfinite(plan['neighbor_gap_cm']) and plan['neighbor_gap_cm'] >= 0
    assert plan['splits'], 'Empty layout plan'
    result = {}
    for split in plan['splits']:
        donor, recipient = split['donor'], split['recipient']
        assert donor != recipient and donor not in result and recipient not in result, 'Repeated split identity'
        axis = {'x': 0, 'y': 1}[split['axis']]
        origin, size = split['origin'], split['size']
        assert len(origin) == 3 and len(size) == 2 and all(math.isfinite(v) for v in origin + size)
        cut = split['cut_cm']
        minimum = plan['minimum_side_cm']
        assert minimum <= cut <= size[axis] - minimum, 'Split leaves a zone too narrow'
        assert min(size) >= minimum and all(float(v).is_integer() for v in origin[:2] + size + [cut])
        low_size, high_size = list(size), list(size)
        high_origin = list(origin)
        low_size[axis], high_size[axis] = cut, size[axis] - cut
        high_origin[axis] += cut
        assert split['recipient_side'] in ('low', 'high')
        ids = (recipient, donor) if split['recipient_side'] == 'low' else (donor, recipient)
        for zone_id, position, dimensions in zip(ids, (origin, high_origin), (low_size, high_size)):
            anchor = split['anchors_world'][zone_id]
            assert len(anchor) == 3 and all(math.isfinite(v) for v in anchor)
            bounds = rectangle(position, dimensions)
            margin = plan['anchor_boundary_margin_cm']
            assert bounds[0] + margin <= anchor[0] <= bounds[2] - margin, 'Anchor outside safe X range'
            assert bounds[1] + margin <= anchor[1] <= bounds[3] - margin, 'Anchor outside safe Y range'
            result[zone_id] = {'origin': position, 'size': dimensions, 'anchor_world': anchor}
    return result


def zone_state(actor):
    """저장할 경계·앵커·이웃·버전과 보존할 식별/용량 규칙을 읽는다."""
    return {'origin': vec(actor.get_actor_location()),
            'rotation': transform_state(actor.get_actor_transform())['rotation'], 'scale': vec(actor.get_actor_scale3d()),
            'polygon': [[p.x, p.y] for p in actor.get_boundary_polygon_local_cm()],
            'anchors': [transform_state(t) for t in actor.get_editor_property('entry_exit_anchors')],
            'neighbors': sorted(str(n) for n in actor.get_editor_property('neighbor_zone_ids')),
            'version': actor.get_editor_property('content_version'),
            'preserved': {key: str(actor.get_editor_property(key)) for key in (
                'zone_id', 'display_name', 'source_reference_id', 'schema_version',
                'authoritative_gis_boundary', 'vertical_extent_cm', 'allow_all_facility_categories',
                'allowed_facility_categories', 'usable_public_area_square_meters', 'soft_capacity_override')}}


def other_actor_state(actors):
    """지형·충돌·NavMesh 설정 등 존 이외 액터의 보존 여부를 검사한다."""
    result = {}
    for actor in actors:
        if isinstance(actor, unreal.FFZSGameplayZone):
            continue
        row = {'transform': transform_state(actor.get_actor_transform()), 'class': actor.get_class().get_path_name(),
               'meshes': []}
        for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
            mesh = comp.static_mesh
            row['meshes'].append([comp.get_path_name(), mesh.get_path_name() if mesh else None,
                                  transform_state(comp.get_world_transform()), str(comp.get_collision_enabled()),
                                  str(comp.get_collision_profile_name())])
        if isinstance(actor, unreal.RecastNavMesh):
            row['navigation'] = {p: str(actor.get_editor_property(p)) for p in (
                'agent_radius', 'agent_height', 'agent_max_slope',
                'runtime_generation', 'tile_size_uu')}
            row['navigation']['resolution_params'] = [{p: setting.get_editor_property(p) for p in (
                'cell_size', 'cell_height', 'agent_max_step_height')}
                for setting in actor.get_editor_property('nav_mesh_resolution_params')]
        result[actor.get_path_name()] = row
    return result


def main():
    """계획 확인 → 메모리 적용 → 실경로/불변식 검사 → 백업 → 선택적 저장 순서다."""
    project = Path(unreal.Paths.project_dir()).resolve()
    plan = json.loads(Path(os.environ['FESTAFIESTA_ZONE_LAYOUT_PLAN']).read_text(encoding='utf-8'))
    assert plan['schema_version'] == 1 and plan['map'].startswith('/Game/')
    desired = split_layout(plan)
    folder = project / 'Artifacts/NavigationRepair' / datetime.now(timezone.utc).strftime('layout-%Y%m%d-%H%M%S-%f')
    folder.mkdir(parents=True)
    report = {'saved': False, 'plan': plan, 'changed_zones': []}
    try:
        world = unreal.EditorLoadingAndSavingUtils.load_map(plan['map'])
        assert unreal.FFZSNavigationEditorLibrary.prepare_world_for_navigation(world)
        subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        actors = subsystem.get_all_level_actors()
        zone_actors = [a for a in actors if isinstance(a, unreal.FFZSGameplayZone)]
        zones = {str(a.get_editor_property('zone_id')): a for a in zone_actors}
        assert len(zones) == len(zone_actors) == plan['zone_count'], 'Zone identity/count changed'
        assert desired.keys() <= zones.keys(), 'Planned zone is missing'
        source = project / 'Content' / (plan['map'].removeprefix('/Game/') + '.umap')
        source_hash = hashlib.sha256(source.read_bytes()).hexdigest()
        report['before_sha256'] = source_hash
        report['before'] = before = {key: zone_state(a) for key, a in zones.items()}
        (folder / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
        if source_hash == plan['source_sha256']:
            for split in plan['splits']:
                width, height = split['size']
                assert before[split['donor']]['origin'] == split['origin'], 'Donor origin differs from the plan'
                assert before[split['donor']]['polygon'] == [[0, 0], [width, 0], [width, height], [0, height]]
        preserved = other_actor_state(actors)
        assert not any(isinstance(a, unreal.FFZSFacilityActor) for a in actors), 'Placed facilities require an explicit migration'
        for key in desired:
            assert not zones[key].get_attached_actors(), 'Attached actors require an explicit migration'
            assert zones[key].get_editor_property('usable_public_area_square_meters') == 0
            assert zones[key].get_editor_property('soft_capacity_override') == 0
        # 이 도구의 분할 모델은 회전/스케일 없는 선형 직사각형에 한정한다.
        bounds = {}
        for key, actor in zones.items():
            rotation = actor.get_actor_rotation()
            assert vec(actor.get_actor_scale3d()) == [1, 1, 1]
            assert rotation.pitch == rotation.yaw == rotation.roll == 0
            poly = before[key]['polygon']
            width, height = poly[2]
            assert poly == [[0, 0], [width, 0], [width, height], [0, height]]
            geometry = desired.get(key, {'origin': before[key]['origin'], 'size': [width, height]})
            bounds[key] = rectangle(geometry['origin'], geometry['size'])
        neighbors = {key: sorted(other for other in zones if other != key and adjacent(
            bounds[key], bounds[other], plan['neighbor_gap_cm'])) for key in zones}
        visited, pending = set(), [next(iter(zones))]
        while pending:
            key = pending.pop()
            if key not in visited:
                visited.add(key)
                pending.extend(neighbors[key])
        assert visited == zones.keys(), 'Planned neighbor graph has isolated zones'
        for key, actor in zones.items():
            actor.modify()
            if key in desired:
                geometry = desired[key]
                actor.set_actor_location(unreal.Vector(*geometry['origin']), False, True)
                width, height = geometry['size']
                spline = actor.get_editor_property('boundary_spline')
                spline.modify()
                spline.set_spline_points([unreal.Vector(x, y, 0) for x, y in (
                    (0, 0), (width, 0), (width, height), (0, height))], unreal.SplineCoordinateSpace.LOCAL, False)
                for index in range(4):
                    spline.set_spline_point_type(index, unreal.SplinePointType.LINEAR, False)
                spline.set_closed_loop(True, True)
                anchor = unreal.Transform()
                anchor.translation = unreal.MathLibrary.inverse_transform_location(
                    actor.get_actor_transform(), unreal.Vector(*geometry['anchor_world']))
                actor.set_editor_property('entry_exit_anchors', [anchor])
            actor.set_editor_property('neighbor_zone_ids', neighbors[key])
            if zone_state(actor) != before[key]:
                actor.set_editor_property('content_version', before[key]['version'] + 1)
                report['changed_zones'].append(key)
            errors = actor.validate_zone_data()
            assert errors is not None and len(errors) == 0, 'Invalid zone: {}'.format(key)
        # 일부만 적용됐거나 다른 맵 변경이 섞인 작업본에는 이전 계획을 덮어쓰지 않는다.
        if report['changed_zones']:
            assert source_hash == plan['source_sha256'], 'Map changed since this layout was planned; nothing saved'
        report['after'] = after = {key: zone_state(a) for key, a in zones.items()}
        others_after = other_actor_state(subsystem.get_all_level_actors())
        report['non_zone_differences'] = {key: {'before': preserved.get(key), 'after': others_after.get(key)}
                                        for key in preserved.keys() | others_after.keys()
                                        if preserved.get(key) != others_after.get(key)}
        assert not report['non_zone_differences'], 'Non-zone actors changed'
        for key in zones:
            assert after[key]['preserved'] == before[key]['preserved'], 'Zone identity/rules changed'
            if key not in desired:
                assert all(after[key][p] == before[key][p] for p in ('origin', 'rotation', 'scale', 'polygon', 'anchors'))
        # 편집기의 등록/건축 가능 영역 캐시도 새 경계로 갱신한다. 군중 검사는 새 PIE에서 수행한다.
        for actor in actors:
            if isinstance(actor, unreal.FFZSBuildArea):
                actor.rebuild_from_world_regions()
        navs = [a for a in actors if isinstance(a, unreal.RecastNavMesh)]
        spawns = [a for a in actors if isinstance(a, unreal.FF_SpawnActor)]
        assert len(navs) == len(spawns) == 1
        nav, spawn = navs[0], spawns[0]
        exit_actor = spawn.get_editor_property('DespawnAchor')
        exit_world = exit_actor.get_actor_location() if exit_actor else (
            spawn.get_actor_location() + spawn.get_editor_property('Temp_ExitOffsetFromOrigin'))
        extent = unreal.Vector(*plan['anchor_projection_extent_cm'])
        exit_nav = unreal.NavigationSystemV1.project_point_to_navigation(world, exit_world, nav, None, extent)
        assert exit_nav is not None
        projected = {}
        for key, geometry in desired.items():
            point = unreal.Vector(*geometry['anchor_world'])
            location = unreal.NavigationSystemV1.project_point_to_navigation(world, point, nav, None, extent)
            assert location is not None and zones[key].contains_world_point(location), 'Anchor projection escaped its zone'
            projected[key] = location
        failures = []
        targets = {**projected, 'exit': exit_nav}
        for key, location in projected.items():
            for other, target in targets.items():
                if other == key:
                    continue
                path = unreal.NavigationSystemV1.find_path_to_location_synchronously(world, location, target, nav)
                if not (path and path.is_valid() and not path.is_partial() and len(path.path_points) > 0):
                    failures.append([key, other])
        report['path_failures'] = failures
        report['checked_directed_paths'] = len(projected) * len(projected)
        assert not failures, 'Planned anchors are not fully connected'
        assert all(key in neighbors[other] for key in neighbors for other in neighbors[key])
        if report['changed_zones'] and os.environ.get('FESTAFIESTA_APPLY_ZONE_LAYOUT') == '1':
            backup = folder / source.name
            shutil.copy2(source, backup)
            assert hashlib.sha256(backup.read_bytes()).hexdigest() == source_hash
            report['backup'] = str(backup)
            (folder / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
            assert unreal.EditorLoadingAndSavingUtils.save_map(world, plan['map'])
            report['saved'] = True
        report['after_sha256'] = hashlib.sha256(source.read_bytes()).hexdigest()
    except Exception as error:
        report['error'] = '{}: {}'.format(type(error).__name__, error)
        raise
    finally:
        (folder / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
        unreal.log('ZONE_LAYOUT_REPORT: {} changed={} saved={}'.format(folder, len(report['changed_zones']), report['saved']))


if __name__ == '__main__':
    main()
