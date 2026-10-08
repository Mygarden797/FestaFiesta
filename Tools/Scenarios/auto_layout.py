# auto_layout.py
# 역할: "어떤 시설을 몇 개, 어느 존에" 만 적은 목록에서 실제 설치 가능한 자리를 찾아
#       prepare_zone_layout.py 가 읽는 좌표 배치 JSON 을 만든다. 맵과 슬롯은 건드리지 않는다.
"""Run through Unreal's Python commandlet.

    UnrealEditor-Cmd.exe FestaFiesta.uproject -run=pythonscript
        -script="<abs>/Tools/Scenarios/auto_layout.py --spec <abs>/Tools/Scenarios/econ_std.spec.json"

결과는 spec 옆에 <slot>.layout.json 으로 쓴다. 그다음 prepare_zone_layout.py --layout <그 파일> --save 로
경로·관람석 검증을 거쳐 슬롯에 저장한다.

--dump-zones 를 주면 존 목록(ID, 이름, 면적, 경계 크기, 이웃)만 출력한다.
"""
import argparse
import json
import sys
from pathlib import Path

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from prepare_zone_layout import (  # noqa: E402
    navigation_portals, register_authored_regions, require, validate_service_navigation)

YAWS = (0, 9000, 18000, 27000)


def point_in_polygon(x, y, polygon):
    inside = False
    j = len(polygon) - 1
    for i in range(len(polygon)):
        xi, yi = polygon[i].x, polygon[i].y
        xj, yj = polygon[j].x, polygon[j].y
        if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / (yj - yi) + xi:
            inside = not inside
        j = i
    return inside


def candidate_points(zone, step_cm):
    """존 경계 안의 격자점. 가운데에서 가까운 순으로 돌려 시설이 경계에 몰리지 않게 한다."""
    polygon = zone.get_boundary_polygon_local_cm()
    xs = [p.x for p in polygon]
    ys = [p.y for p in polygon]
    cx, cy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
    points = []
    x = min(xs)
    while x <= max(xs):
        y = min(ys)
        while y <= max(ys):
            if point_in_polygon(x, y, polygon):
                points.append((int(x), int(y)))
            y += step_cm
        x += step_cm
    points.sort(key=lambda p: (p[0] - cx) ** 2 + (p[1] - cy) ** 2)
    return points


def dump_zones(actors):
    for zone in sorted((a for a in actors if isinstance(a, unreal.FFZSGameplayZone)), key=lambda z: str(z.zone_id)):
        polygon = zone.get_boundary_polygon_local_cm()
        xs = [p.x for p in polygon]
        ys = [p.y for p in polygon]
        loc = zone.get_actor_location()
        unreal.log('FF_ZONE %s | %s | %.0fm2 | %.0fx%.0fm | world(%.0f,%.0f) | neighbors=%s' % (
            zone.zone_id, zone.display_name, zone.get_boundary_area_square_meters(),
            (max(xs) - min(xs)) / 100, (max(ys) - min(ys)) / 100, loc.x / 100, loc.y / 100,
            ','.join(str(n) for n in zone.neighbor_zone_ids)))
    for spawn in (a for a in actors if isinstance(a, unreal.FF_SpawnActor)):
        for gate in spawn.get_editor_property('CrowdGates'):
            anchor = gate.get_editor_property('GateAnchor')
            loc = anchor.get_actor_location() if anchor else gate.get_editor_property('GateLocation')
            inside = [str(z.zone_id) for z in actors if isinstance(z, unreal.FFZSGameplayZone)
                      and z.contains_world_point_with_hysteresis(loc, 3000.0)]
            unreal.log('FF_GATE %s | world(%.0f,%.0f) | zones=%s' % (
                gate.get_editor_property('GateId'), loc.x / 100, loc.y / 100, ','.join(inside)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--spec', type=Path)
    parser.add_argument('--dump-zones', action='store_true')
    parser.add_argument('--step-cm', type=int, default=300)
    args = parser.parse_args()

    map_path = '/Game/FestaFiesta/Maps/Test_Main'
    spec = None
    if args.spec:
        spec = json.loads(args.spec.read_text(encoding='utf-8-sig'))
        map_path = spec.get('map', map_path)

    world = unreal.EditorLoadingAndSavingUtils.load_map(map_path)
    require(world is not None, 'Map could not be loaded')
    require(unreal.FFZSNavigationEditorLibrary.prepare_world_for_navigation(world), 'Geometry is not ready')
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    if args.dump_zones:
        dump_zones(actors)
        return
    require(spec is not None, '--spec is required')

    zones = register_authored_regions(world, actors)
    portals = navigation_portals(world, actors)
    zone_actors = {str(a.zone_id): a for a in actors if isinstance(a, unreal.FFZSGameplayZone)}
    all_zone_ids = sorted(zone_actors)
    points_cache = {}

    placed = []
    failures = []
    zone_turn = 0
    for row in spec['facilities']:
        definition = unreal.load_asset(row['definition'])
        require(isinstance(definition, unreal.FFZSFacilityDefinition), f"Invalid facility: {row['definition']}")
        preferred = row.get('zones') or all_zone_ids
        for index in range(row.get('count', 1)):
            # 같은 종류를 여러 개 놓을 때는 존을 돌아가며 써서 한곳에 몰리지 않게 한다
            order = preferred[zone_turn % len(preferred):] + preferred[:zone_turn % len(preferred)]
            zone_turn += 1
            done = False
            for zone_id in order:
                zone = zone_actors.get(zone_id)
                if zone is None:
                    continue
                if zone_id not in points_cache:
                    points_cache[zone_id] = candidate_points(zone, args.step_cm)
                # 좌표를 적은 행(메인 공연장처럼 자리가 정해진 시설)은 그 자리만 본다
                origins = [tuple(row['origin_cm'])] if 'origin_cm' in row else points_cache[zone_id]
                for origin in origins:
                    for yaw in ([row['yaw_centidegrees']] if 'yaw_centidegrees' in row else YAWS):
                        placement = unreal.FFZSGridPlacement(origin_cell=unreal.IntPoint(*origin), yaw_centidegrees=yaw)
                        command = unreal.FFZSGridLibrary.make_install_command_from_definition(
                            definition, zone_id, placement, zones.get_world_revision())
                        if not unreal.FF_ZoneLayoutAuthoringLibrary.validate_placement(world, command).is_valid:
                            continue
                        before = zones.capture_world_snapshot()
                        executed = unreal.FF_ZoneLayoutAuthoringLibrary.execute_placement(world, command)
                        if not executed.succeeded:
                            continue
                        # 겹침 검사만으로는 대기열 칸이 물 위나 절벽 끝에 걸릴 수 있다. 저장 단계(prepare)가
                        # 보는 대기열·서비스 지점과 입구 경로를 여기서도 보고, 안 되면 되돌리고 다음 자리로 간다.
                        if not row.get('expected_audience_capacity'):
                            try:
                                validate_service_navigation(world, executed.facility_actor, portals)
                            except RuntimeError:
                                require(zones.restore_world_snapshot(before) is not None, 'Could not undo a rejected spot')
                                continue
                        entry = {'definition': row['definition'], 'zone_id': zone_id,
                                 'origin_cm': list(origin), 'yaw_centidegrees': yaw}
                        if row.get('expected_audience_capacity'):
                            entry['expected_audience_capacity'] = row['expected_audience_capacity']
                        placed.append(entry)
                        done = True
                        break
                    if done:
                        break
                if done:
                    break
            if not done:
                failures.append(f"{row['definition']} #{index + 1}")

    out = args.spec.with_name(spec['slot'] + '.layout.json')
    plan = {'schema_version': 1, 'description': spec.get('description', ''), 'map': map_path,
            'slot': spec['slot'], 'facilities': placed}
    out.write_text(json.dumps(plan, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    unreal.log(f'FF_AUTO_LAYOUT placed={len(placed)} failed={len(failures)} out={out}')
    for failure in failures:
        unreal.log_warning(f'FF_AUTO_LAYOUT_FAILED {failure}')


if __name__ == '__main__':
    main()
