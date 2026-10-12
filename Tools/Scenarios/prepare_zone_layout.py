# prepare_zone_layout.py
# 역할: 검토 가능한 JSON 배치를 실제 지형·시설·관람 경로로 검증하고 선택한 저장 슬롯을 만든다.
"""Run through Unreal's Python commandlet. Default is validation only; --save writes a slot.

The map, definitions and gameplay policies are never saved or changed. Editor placement
authoring does not charge a player's wallet; the real PIE scenario verifies game integration.
"""
import argparse
import hashlib
import json
import shutil
from datetime import datetime, timezone
from pathlib import Path

import unreal


def sha256(path):
    """큰 맵도 메모리에 통째로 올리지 않고 원본 보존 여부를 확인한다."""
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def require(condition, message):
    """검증 실패 시 저장 단계에 도달하지 않도록 구체적인 이유로 중단한다."""
    if not condition:
        raise RuntimeError(message)


def asset_file(project, package, suffix='.uasset'):
    """계획의 게임 패키지를 실제 프로젝트 파일로 연결한다."""
    require(package.startswith('/Game/') and '..' not in package.split('/'), 'Expected a /Game/ package')
    return project / 'Content' / (package.removeprefix('/Game/') + suffix)


def register_authored_regions(world, actors):
    """Commandlet의 맵 로딩 후 존·건설·보호 영역을 같은 권한 레지스트리에 등록한다."""
    subsystems = [o for o in unreal.ObjectIterator(unreal.FFZSWorldSubsystem) if o.get_outer() == world]
    require(len(subsystems) == 1, 'Expected one ZoneSystem for the loaded world')
    zones = subsystems[0]
    require(not any(isinstance(a, unreal.FFZSFacilityActor) for a in actors),
            'This authoring plan requires an empty facility layout; existing map facilities are preserved')
    for actor in actors:
        if isinstance(actor, unreal.FFZSGameplayZone):
            require(zones.register_zone(actor), f'Invalid or duplicate zone: {actor.get_name()}')
        elif isinstance(actor, unreal.FFZSProtectedArea):
            require(zones.register_protected_area(actor), f'Invalid protected area: {actor.get_name()}')
        elif isinstance(actor, unreal.FFZSBuildArea):
            require(zones.register_build_area(actor), f'Invalid build area: {actor.get_name()}')
    # ProjectZoneIds에는 다른 맵의 프리셋도 있다. 이 맵의 실제 이웃 연결은 별도로 확인한다.
    # 각 존/보호 영역의 데이터 검증은 위의 공용 Register 함수가 이미 수행한다.
    local_zones = {str(a.zone_id): a for a in actors if isinstance(a, unreal.FFZSGameplayZone)}
    for zone_id, zone in local_zones.items():
        for neighbor in zone.neighbor_zone_ids:
            neighbor_id = str(neighbor)
            require(neighbor_id in local_zones, f'{zone_id} references missing neighbor {neighbor_id}')
            require(zone_id in {str(n) for n in local_zones[neighbor_id].neighbor_zone_ids},
                    f'One-way zone connection: {zone_id} -> {neighbor_id}')
    protected_ids = set()
    for area in (a for a in actors if isinstance(a, unreal.FFZSProtectedArea)):
        area_id = str(area.protected_area_id)
        require(area_id not in protected_ids, f'Duplicate protected area: {area_id}')
        protected_ids.add(area_id)
        require(str(area.linked_zone_id) == 'None' or str(area.linked_zone_id) in local_zones,
                f'Protected area references missing zone: {area_id}')
    return zones


def navigation_portals(world, actors):
    """군중의 실제 생성·퇴장 앵커를 투영한다. 별도 위치가 없을 때만 군중의 기존 대체값을 쓴다."""
    spawns = [a for a in actors if isinstance(a, unreal.FF_SpawnActor)]
    navs = [a for a in actors if isinstance(a, unreal.RecastNavMesh)]
    require(len(spawns) == len(navs) == 1, 'Expected one production spawn and one NavMesh')
    spawn, nav = spawns[0], navs[0]
    entry_actor = spawn.get_editor_property('SpawnAchor')
    exit_actor = spawn.get_editor_property('DespawnAchor')
    entry = entry_actor.get_actor_location() if entry_actor else spawn.get_actor_location()
    exit_point = exit_actor.get_actor_location() if exit_actor else (
        spawn.get_actor_location() + spawn.get_editor_property('Temp_ExitOffsetFromOrigin'))
    projected = [unreal.NavigationSystemV1.project_point_to_navigation(
        world, p, nav, None, unreal.Vector(300, 300, 2000)) for p in (entry, exit_point)]
    require(all(p is not None for p in projected), 'A production crowd portal is outside navigation')
    return projected


def require_path(world, start, end, label):
    """부분 경로를 성공으로 인정하지 않고 목적지까지 연결되는지 검사한다."""
    path = unreal.NavigationSystemV1.find_path_to_location_synchronously(world, start, end)
    require(path is not None and path.is_valid() and not path.is_partial(), f'Incomplete path: {label}')


def validate_service_navigation(world, facility, portals):
    """관람석이 없는 프로그램 시설은 실제 입구·서비스·출구와 최대 대기 슬롯을 검사한다."""
    spec = facility.get_facility_spec()
    queue = facility.get_queue_component()
    require(queue is not None, 'Service facility has no queue component')
    extent_cm = spec.queue_rules.navigation_projection_extent_cm
    extent = unreal.Vector(extent_cm, extent_cm, extent_cm)

    def project(point, label):
        projected = unreal.NavigationSystemV1.project_point_to_navigation(world, point, None, None, extent)
        require(projected is not None, f'Service point outside navigation: {label}')
        return projected

    entry = facility.get_anchor_world_transform(unreal.FFZSAnchorType.ENTRY, 0)
    exit_anchor = facility.get_anchor_world_transform(unreal.FFZSAnchorType.EXIT, 0)
    require(entry is not None and exit_anchor is not None, 'Service facility requires entry and exit anchors')
    entry_point = project(entry.translation, 'entry')
    exit_point = project(exit_anchor.translation, 'exit')
    require_path(world, portals[0], entry_point, 'crowd entrance to program facility')
    require_path(world, exit_point, portals[1], 'program facility to crowd exit')
    route_points = [(queue.get_queue_slot_world_location(i), f'queue {i}')
                    for i in range(spec.queue_rules.max_waiting_agents)]
    for i in range(spec.queue_rules.service_capacity):
        service = facility.get_anchor_world_transform(unreal.FFZSAnchorType.SERVICE, i)
        require(service is not None, f'Service anchor {i} is missing')
        route_points.append((service.translation, f'service {i}'))
    for point, label in route_points:
        projected = project(point, label)
        require_path(world, entry_point, projected, f'entry to {label}')
        require_path(world, projected, exit_point, f'{label} to exit')
    return len(route_points)


def validate_open_facilities(world, zones, installed, portals):
    """실제 시설을 잠시 개장해 공용 관람 API로 모든 좌석과 군중 출입 연결을 검사한다."""
    ready_snapshot = zones.capture_world_snapshot()
    result = []
    try:
        for definition, row, instance_id in installed:
            command = unreal.FFZSGridLibrary.make_install_command_from_definition(
                definition, row['zone_id'], unreal.FFZSGridPlacement(), zones.get_world_revision())
            command.set_editor_property('type', unreal.FFZSCommandType.OPEN)
            command.set_editor_property('facility_instance_id', instance_id)
            opened = unreal.FF_ZoneLayoutAuthoringLibrary.execute_placement(world, command)
            require(opened.succeeded, f"Open {row['definition']}: {opened.message}")
            expected_capacity = row.get('expected_audience_capacity', 0)
            require(definition.spec.audience.enabled == (expected_capacity > 0),
                    f"Audience enablement differs from the reviewed plan: {row['definition']}")
            if expected_capacity == 0:
                try:
                    checked_points = validate_service_navigation(world, opened.facility_actor, portals)
                except RuntimeError as error:
                    raise RuntimeError(f"{error} — {row['definition']} in {row['zone_id']} {row['origin_cm']}") from error
                result.append({'definition': row['definition'], 'audience_slots_checked': 0,
                               'service_and_queue_points_checked': checked_points,
                               'crowd_entrance_connected': True, 'crowd_exit_connected': True})
                continue
            output = unreal.FFZSAudienceLibrary.get_facility_audience_snapshot(world, instance_id)
            require(output is not None, f"Audience snapshot unavailable: {row['definition']}")
            snapshot, _ = output
            require(snapshot.capacity == expected_capacity,
                    f'Audience capacity changed: expected {expected_capacity}, actual {snapshot.capacity}')
            require_path(world, portals[0], snapshot.entry_world_cm, 'crowd entrance to audience entrance')
            require_path(world, snapshot.exit_world_cm, portals[1], 'audience exit to crowd exit')
            for first in range(0, snapshot.capacity, 64):
                count = min(64, snapshot.capacity - first)
                checked = unreal.FFZSAudienceLibrary.check_audience_navigation_batch(world, snapshot, first, count)
                require(checked is not None, f'Audience entry/seat/exit path failed in slots {first}..{first+count-1}')
            result.append({'definition': row['definition'], 'audience_slots_checked': snapshot.capacity,
                           'crowd_entrance_connected': True, 'crowd_exit_connected': True})
    finally:
        require(zones.restore_world_snapshot(ready_snapshot) is not None, 'Could not restore unopened layout')
    return result


def main():
    """명시된 배치를 검증한 뒤 --save일 때만 저장하며 기존 슬롯 덮어쓰기는 별도로 요구한다."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--layout', type=Path, default=Path(__file__).with_name('day1.layout.json'))
    parser.add_argument('--save', action='store_true')
    parser.add_argument('--overwrite', action='store_true')
    args = parser.parse_args()
    require(not args.overwrite or args.save, '--overwrite requires --save')
    plan = json.loads(args.layout.read_text(encoding='utf-8-sig'))
    require(plan['schema_version'] == 1 and plan['facilities'], 'Unsupported or empty layout plan')
    slot = plan['slot']
    require(slot and all(c.isalnum() or c in '_-' for c in slot), 'Invalid save slot name')
    project = Path(unreal.Paths.project_dir()).resolve()
    report_dir = project / 'Saved/ScenarioAuthoring' / datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-%f')
    report_dir.mkdir(parents=True)
    report = {'plan': str(args.layout.resolve()), 'slot': slot, 'saved': False, 'facilities': []}
    source_files = [asset_file(project, plan['map'], '.umap')]
    source_files += [asset_file(project, row['definition']) for row in plan['facilities']]
    hashes = {str(p.relative_to(project)): sha256(p) for p in source_files}
    try:
        exists = unreal.GameplayStatics.does_save_game_exist(slot, 0)
        require(not (args.save and exists and not args.overwrite),
                f'Slot {slot} already exists. Validate without --save, or explicitly use --save --overwrite')
        world = unreal.EditorLoadingAndSavingUtils.load_map(plan['map'])
        require(world is not None, 'Map could not be loaded')
        require(unreal.FFZSNavigationEditorLibrary.prepare_world_for_navigation(world), 'Geometry is not ready')
        actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
        zones = register_authored_regions(world, actors)
        installed = []
        for row in plan['facilities']:
            definition = unreal.load_asset(row['definition'])
            require(isinstance(definition, unreal.FFZSFacilityDefinition), f"Invalid facility: {row['definition']}")
            placement = unreal.FFZSGridPlacement(origin_cell=unreal.IntPoint(*row['origin_cm']),
                                                 yaw_centidegrees=row['yaw_centidegrees'])
            command = unreal.FFZSGridLibrary.make_install_command_from_definition(
                definition, row['zone_id'], placement, zones.get_world_revision())
            validation = unreal.FF_ZoneLayoutAuthoringLibrary.validate_placement(world, command)
            require(validation.is_valid, f"Placement {row['definition']}: {validation.summary}")
            executed = unreal.FF_ZoneLayoutAuthoringLibrary.execute_placement(world, command)
            require(executed.succeeded, f"Install {row['definition']}: {executed.message}")
            installed.append((definition, row, command.facility_instance_id))
            report['facilities'].append(dict(row, instance_id=command.facility_instance_id.export_text(),
                world_transform=executed.facility_actor.get_actor_transform().export_text()))
        # 새 시설 장애물을 포함하여 메모리 안에서만 굽는다. 레벨 파일은 저장하지 않는다.
        require(unreal.FFZSNavigationEditorLibrary.prepare_world_for_navigation(world), 'Facility geometry is not ready')
        unreal.SystemLibrary.execute_console_command(world, 'RebuildNavigation')
        portals = navigation_portals(world, actors)
        report['navigation'] = validate_open_facilities(world, zones, installed, portals)
        if args.save:
            slot_file = project / 'Saved/SaveGames' / f'{slot}.sav'
            if exists:
                require(slot_file.is_file(), 'Existing save is not in the expected local save directory')
                backup = report_dir / f'{slot}.before.sav'
                shutil.copy2(slot_file, backup)
                require(sha256(slot_file) == sha256(backup), 'Save backup verification failed')
                report['backup'] = str(backup)
            saved = unreal.FF_ZoneLayoutAuthoringLibrary.save_layout(world, slot, args.overwrite)
            require(saved is not None, 'Save or full snapshot readback failed; inspect the engine log')
            report.update(saved=True, save_file=str(slot_file), save_sha256=sha256(slot_file))
        report['passed'] = True
        unreal.log('FF_LAYOUT_READY ' + str(report_dir / 'report.json'))
    except Exception as error:
        report.update(passed=False, error=str(error))
        raise
    finally:
        report['source_hashes_before'] = hashes
        report['source_hashes_after'] = {str(p.relative_to(project)): sha256(p) for p in source_files}
        report['source_assets_unchanged'] = hashes == report['source_hashes_after']
        (report_dir / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
        require(report['source_assets_unchanged'], 'Source map or definition changed during authoring')


if __name__ == '__main__':
    main()
