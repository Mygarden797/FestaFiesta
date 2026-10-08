# 역할: 잘못된 지형 convex 충돌과 실제 접속 단차를 검증 후 국소 복구한다.
"""Inspect Test_Main, or apply its measured walkable-geometry repair.

Default is read-only. FESTAFIESTA_APPLY_WALKABLE_GEOMETRY_REPAIR=1 enables
transactional saving of four static meshes and Test_Main. Re-run without the
variable in a fresh editor process, then run the production PIE navigation and
crowd grounding tests. Preserves gates, zone geometry, and existing transforms;
authors three previously empty zone entrance arrays on measured public ground.
"""
import copy
import hashlib
import json
import math
import os
import shutil
import sys
import traceback
from datetime import datetime, timezone
from pathlib import Path

import unreal

MAP = '/Game/FestaFiesta/Maps/Test_Main'
LABELS = ('Deck_Flat_StairNormal', 'Parking211_Optimized', 'SM_Road_Roundabout', 'Roads_V1')
TAG = 'FF_CrowdGroundRefinement'
SHAPES = ('sphere_elems', 'box_elems', 'sphyl_elems', 'convex_elems', 'tapered_capsule_elems')
# Endpoints are top-surface centers in cm. They were measured from the original
# geometry, then tested physically and against the complete navigation network.
RAMPS = (
    {'label': 'FF_CrowdWalkway_DeckDescent', 'source': 'Deck_Flat_StairNormal',
     'material_name': 'M_Stair_Wood_Master', 'source_material_slot': 7, 'width': 400.0, 'thickness': 30.0,
     'high': [88532.32671300192, 37674.83762699837, 569.6989135742188],
     'low': [88978.83414715617, 37899.85120713294, 439.3327767819439]},
    {'label': 'FF_CrowdWalkway_ParkingGate3', 'source': 'Parking211_Optimized',
     'material_name': 'M211_Asphalt', 'source_material_slot': 0, 'width': 600.0, 'thickness': 30.0,
     'high': [62232.9359564566, 23116.427139348558, 602.0],
     'low': [62369.729129542975, 22740.54467394619, 473.49373031183404]},
)
# Revision 1 passed full-network queries but its 100 cm straight approaches
# overlapped the elevated road. Only this exact known transform may be upgraded.
LEGACY_PARKING = dict(RAMPS[1], high=[62219.42932233542, 23229.65192505997, 592.0],
                      low=[62292.75585958279, 22684.561823393, 474.0501939969099])
LEGACY_DECK_MATERIAL = dict(RAMPS[0], material_name='Material_004', source_material_slot=0)
# Empty arrays previously fell back to each zone's lower-left actor origin.
# Those origins select isolated road/terrain surfaces. These measured Landscape
# points favor the zone center, zero horizontal projection, and boundary margin.
# They are existing ground, not relocated exits or navigation-only geometry.
ENTRANCES = {
    'GZ_TestMain_23': [31860.0, 11714.166666666668, 376.0309645247729],
    'GZ_TestMain_24': [41840.0, 15150.0, 383.6832744467227],
    'GZ_TestMain_29': [49665.0, 17916.666666666664, 420.91401787358285],
}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def vec(value):
    return [float(value.x), float(value.y), float(value.z)]


def geometry(spec):
    high, low = spec['high'], spec['low']
    delta = [low[i] - high[i] for i in range(3)]
    horizontal = math.hypot(delta[0], delta[1])
    length = math.sqrt(sum(value * value for value in delta))
    angle = math.atan2(-delta[2], horizontal)
    direction = [delta[0] / horizontal, delta[1] / horizontal]
    normal = [direction[0] * math.sin(angle), direction[1] * math.sin(angle), math.cos(angle)]
    return {'location': [(high[i] + low[i]) / 2 - normal[i] * spec['thickness'] / 2 for i in range(3)],
            'rotation': [-math.degrees(angle), math.degrees(math.atan2(delta[1], delta[0])), 0.0],
            'scale': [length / 100, spec['width'] / 100, spec['thickness'] / 100],
            'slope_degrees': math.degrees(angle), 'length_cm': length,
            'across': [-direction[1], direction[0]]}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def close(a, b, tolerance=0.001):
    return len(a) == len(b) and all(abs(x - y) <= tolerance for x, y in zip(a, b))


def boundary_distance(point, polygon):
    distances = []
    for a, b in zip(polygon, polygon[1:] + polygon[:1]):
        delta = [b[i] - a[i] for i in range(2)]
        length_sq = sum(v * v for v in delta)
        t = max(0, min(1, sum((point[i] - a[i]) * delta[i] for i in range(2)) / length_sq)) if length_sq else 0
        distances.append(math.hypot(*(point[i] - a[i] - delta[i] * t for i in range(2))))
    return min(distances)


def run():
    root = Path(unreal.Paths.project_dir()).resolve()
    require((root / 'FestaFiesta.uproject').is_file(), 'Wrong project')
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    sys.path.insert(0, str(root / 'Tools/facilities'))
    from ffzs_asset_save_transaction import AssetSaveTransaction
    from rebuild_packaging_navigation import coverage_regressions
    apply = os.environ.get('FESTAFIESTA_APPLY_WALKABLE_GEOMETRY_REPAIR') == '1'
    out = root / 'Artifacts/WalkableGeometryRepair' / datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-%f')
    out.mkdir(parents=True, exist_ok=False)
    report = {'schema_version': 3, 'geometry_revision': 2, 'material_revision': 2,
              'mode': 'apply' if apply else 'inspect', 'saved': False,
              'status': 'running', 'plan': [dict(spec, geometry=geometry(spec)) for spec in RAMPS],
              'entrance_plan': ENTRANCES}
    transaction, files, before_hashes = None, [], {}

    def write():
        (out / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')

    def transform(actor):
        rotation = actor.get_actor_rotation()
        return {'location': vec(actor.get_actor_location()), 'rotation': [rotation.pitch, rotation.yaw, rotation.roll],
                'scale': vec(actor.get_actor_scale3d())}

    def mesh_state(mesh):
        body = mesh.get_editor_property('body_setup')
        geom = body.get_editor_property('agg_geom')
        return {'asset': mesh.get_path_name(), 'collision': str(body.get_editor_property('collision_trace_flag')),
                'simple_shapes': {name: len(geom.get_editor_property(name)) for name in SHAPES},
                'render_geometry': [{'vertices': mesh.get_num_vertices(lod), 'triangles': mesh.get_num_triangles(lod)}
                                    for lod in range(mesh.get_num_lods())],
                'materials': [{'slot': str(slot.get_editor_property('material_slot_name')),
                               'material': slot.get_editor_property('material_interface').get_path_name()
                               if slot.get_editor_property('material_interface') else None}
                              for slot in mesh.get_editor_property('static_materials')]}

    try:
        world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
        require(world and unreal.FFZSNavigationEditorLibrary.prepare_world_for_navigation(world), 'World is not ready')
        subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        actors = list(subsystem.get_all_level_actors())
        targets, meshes, components = {}, {}, {}
        for label in LABELS:
            matches = [actor for actor in actors if actor.get_actor_label() == label]
            require(len(matches) == 1, 'Expected one source actor: ' + label)
            targets[label] = matches[0]
            values = [comp for comp in matches[0].get_components_by_class(unreal.StaticMeshComponent) if comp.static_mesh]
            require(len(values) == 1, 'Expected one static mesh component: ' + label)
            components[label], meshes[label] = values[0], values[0].static_mesh
            require(meshes[label].get_name() == label and meshes[label].get_path_name().startswith('/Game/Level/Demo_Matereal/Mesh/'),
                    'Source mesh identity changed: ' + label)
        require(len({mesh.get_path_name() for mesh in meshes.values()}) == 4, 'Repair meshes must be distinct')
        navs = [actor for actor in actors if isinstance(actor, unreal.RecastNavMesh)]
        spawns = [actor for actor in actors if isinstance(actor, unreal.FF_SpawnActor)]
        zones = sorted((actor for actor in actors if isinstance(actor, unreal.FFZSGameplayZone)),
                       key=lambda actor: str(actor.get_editor_property('zone_id')))
        require(len(navs) == 1 and len(spawns) == 1 and len(zones) == 31, 'Release-map actor identities changed')
        require(len({str(zone.get_editor_property('zone_id')) for zone in zones}) == 31, 'Duplicate zone identity')
        nav, spawn = navs[0], spawns[0]
        report['search_node_limit'] = unreal.FFZSNavigationEditorLibrary.get_navigation_query_node_limit(world)
        exit_anchor = spawn.get_editor_property('DespawnAchor')
        exit_point = exit_anchor.get_actor_location() if exit_anchor else (
            spawn.get_actor_location() + spawn.get_editor_property('Temp_ExitOffsetFromOrigin'))
        gates = list(spawn.get_editor_property('crowd_gates'))
        require(len(gates) == 4 and {str(g.get_editor_property('gate_id')) for g in gates} == {'1', '2', '3', '4'},
                'Expected all four authored gates')

        def authored():
            return {'actors': {actor.get_path_name(): transform(actor) for actor in actors},
                    'components': {comp.get_path_name(): {
                        'mesh': comp.static_mesh.get_path_name() if comp.static_mesh else None,
                        'materials': [comp.get_material(index).get_path_name() if comp.get_material(index) else None
                                      for index in range(comp.get_num_materials())]}
                        for actor in actors for comp in actor.get_components_by_class(unreal.StaticMeshComponent)},
                    'zones': {str(zone.get_editor_property('zone_id')): {
                        'polygon': [[p.x, p.y] for p in zone.get_boundary_polygon_local_cm()],
                        'neighbors': sorted(str(value) for value in zone.get_editor_property('neighbor_zone_ids')),
                        'vertical_extent_cm': float(zone.get_editor_property('vertical_extent_cm')),
                        'anchors': [anchor.export_text() for anchor in zone.get_editor_property('entry_exit_anchors')]}
                        for zone in zones},
                    'gates': [gate.export_text() for gate in spawn.get_editor_property('crowd_gates')],
                    'exit_requested': vec(exit_point),
                    'navigation': {name: str(nav.get_editor_property(name)) for name in (
                        'agent_radius', 'agent_height', 'agent_max_slope', 'tile_size_uu', 'runtime_generation')},
                    'resolution': [{name: float(item.get_editor_property(name)) for name in (
                        'cell_size', 'cell_height', 'agent_max_step_height')}
                        for item in nav.get_editor_property('nav_mesh_resolution_params')]}

        before_authored = authored()
        expected_authored = copy.deepcopy(before_authored)
        report['authored_before'] = before_authored
        report['meshes_before'] = {label: mesh_state(mesh) for label, mesh in meshes.items()}
        packages = [MAP] + [mesh.get_path_name().split('.', 1)[0] for mesh in meshes.values()]
        for package in packages:
            base = root / 'Content' / package[len('/Game/'):]
            matches = list(base.parent.glob(base.name + '.*'))
            require(any(path.suffix in ('.umap', '.uasset') for path in matches), 'Source package missing: ' + package)
            files.extend(matches)
        before_hashes = {str(path): digest(path) for path in files}
        report['before_sha256'] = before_hashes

        def project(point, extent=(300, 300, 2000)):
            return unreal.NavigationSystemV1.project_point_to_navigation(world, point, nav, None, unreal.Vector(*extent))

        def reachable(a, b):
            if a is None or b is None:
                return False
            path = unreal.NavigationSystemV1.find_path_to_location_synchronously(world, a, b, nav)
            return path is not None and path.is_valid() and not path.is_partial() and len(path.path_points) > 0

        def audit():
            target = project(exit_point)
            rows, anchor_points = [], []
            statuses = ('reachable', 'unreachable', 'unprojected', 'outside_polygon', 'target_unprojected')
            for zone in zones:
                polygon = zone.get_boundary_polygon_local_cm()
                require(len(polygon) >= 3, 'Zone polygon missing')
                minimum = [min(getattr(p, name) for p in polygon) for name in ('x', 'y')]
                maximum = [max(getattr(p, name) for p in polygon) for name in ('x', 'y')]
                row = {'zone': str(zone.get_editor_property('zone_id')), 'polygon_local': [[p.x, p.y] for p in polygon],
                       'samples': [], **{status: [] for status in statuses}}
                for iy in range(9):
                    for ix in range(9):
                        local = unreal.Vector(minimum[0] + (maximum[0] - minimum[0]) * (1 / 16 + ix * 7 / 64),
                                              minimum[1] + (maximum[1] - minimum[1]) * (1 / 16 + iy * 7 / 64), 0)
                        point = unreal.MathLibrary.transform_location(zone.get_actor_transform(), local)
                        inside = zone.contains_world_point(point)
                        projected = project(point) if inside else None
                        status = ('outside_polygon' if not inside else 'unprojected' if projected is None else
                                  'target_unprojected' if target is None else 'reachable' if reachable(projected, target) else 'unreachable')
                        sample = {'sample_id': '{}:{}'.format(ix, iy), 'local': vec(local), 'requested_world': vec(point),
                                  'inside_polygon': inside, 'projected_world': vec(projected) if projected is not None else None, 'status': status}
                        row['samples'].append(sample)
                        row[status].append(sample['projected_world'] or sample['requested_world'])
                anchors = list(zone.get_editor_property('entry_exit_anchors'))
                point = unreal.MathLibrary.transform_location(zone.get_actor_transform(), anchors[0].translation) if anchors else zone.get_actor_location()
                projected = project(point)
                row['anchor'] = {'requested': vec(point), 'projected': vec(projected) if projected is not None else None,
                                 'to_exit': reachable(projected, target), 'from_exit': reachable(target, projected)}
                anchor_points.append((row['zone'], projected))
                rows.append(row)
            gate_rows, gate_points = [], []
            for gate in gates:
                anchor = gate.get_editor_property('gate_anchor')
                point = anchor.get_actor_location() if anchor else gate.get_editor_property('gate_location')
                projected = project(point, (50, 50, 250))
                gate_points.append((str(gate.get_editor_property('gate_id')), projected))
                gate_rows.append({'gate_id': str(gate.get_editor_property('gate_id')), 'requested': vec(point),
                                  'projected': vec(projected) if projected is not None else None,
                                  'to_exit': reachable(projected, target), 'from_exit': reachable(target, projected)})
            failed_pairs = [{'from': a, 'to': b} for a, start in anchor_points for b, end in anchor_points
                            if a != b and not reachable(start, end)]
            failed_gate_pairs = []
            for zone_id, zone_point in anchor_points:
                for gate_id, gate_point in gate_points:
                    if not reachable(zone_point, gate_point):
                        failed_gate_pairs.append({'from_zone': zone_id, 'to_gate': gate_id})
                    if not reachable(gate_point, zone_point):
                        failed_gate_pairs.append({'from_gate': gate_id, 'to_zone': zone_id})
            return {'exit_requested': vec(exit_point), 'exit': vec(target) if target is not None else None,
                    'reachable_zones': sum(bool(row['reachable']) for row in rows),
                    'sample_counts': {status: sum(len(row[status]) for row in rows) for status in statuses},
                    'zones': rows, 'gates': gate_rows, 'anchor_directed_pair_count': len(anchor_points) * (len(anchor_points) - 1),
                    'failed_anchor_pairs': failed_pairs, 'anchor_gate_directed_pair_count': len(anchor_points) * len(gate_points) * 2,
                    'failed_anchor_gate_pairs': failed_gate_pairs}

        def strict(result):
            require(result['reachable_zones'] == 31, 'Not all 31 zones have a usable sample')
            require(all(row['anchor']['to_exit'] and row['anchor']['from_exit'] for row in result['zones']),
                    'An authored zone anchor is disconnected')
            require(not result['failed_anchor_pairs'], 'A directed production-anchor pair is disconnected')
            require(not result['failed_anchor_gate_pairs'], 'A zone entrance and original gate are disconnected')
            require(len(result['gates']) == 4 and all(row['to_exit'] and row['from_exit'] for row in result['gates']),
                    'An original authored gate is unprojected or disconnected')

        def trace(point, ignore, complex_trace=True, top=700):
            hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(point[0], point[1], top),
                unreal.Vector(point[0], point[1], -2500), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
                complex_trace, ignore, unreal.DrawDebugTrace.NONE, True)
            if hit is None:
                return None
            if isinstance(hit, tuple):
                hit = next((value for value in hit if isinstance(value, unreal.HitResult)), None)
            if hit is None:
                return None
            values = hit.to_tuple()
            if not values[0]:
                return None
            return {'point': vec(values[5]), 'normal': vec(values[7]),
                    'actor': values[9].get_actor_label() if values[9] else None}

        def verify_entrances():
            evidence = []
            for zone_id, point in ENTRANCES.items():
                zone = next(zone for zone in zones if str(zone.get_editor_property('zone_id')) == zone_id)
                anchors = list(zone.get_editor_property('entry_exit_anchors'))
                require(len(anchors) == 1, 'Expected one measured zone entrance: ' + zone_id)
                requested = unreal.MathLibrary.transform_location(zone.get_actor_transform(), anchors[0].translation)
                require(close(vec(requested), point), 'Measured entrance has changed: ' + zone_id)
                require(close(vec(anchors[0].scale3d), [1, 1, 1])
                        and close([anchors[0].rotation.x, anchors[0].rotation.y, anchors[0].rotation.z, anchors[0].rotation.w],
                                  [0, 0, 0, 1]), 'Entrance rotation or scale changed')
                simple, complex_hit = trace(point, [], False, 4000), trace(point, [], True, 4000)
                projected = project(requested)
                polygon = [vec(unreal.MathLibrary.transform_location(zone.get_actor_transform(), unreal.Vector(p.x, p.y, 0)))
                           for p in zone.get_boundary_polygon_local_cm()]
                require(simple and complex_hit and simple['actor'] == complex_hit['actor'] == 'Landscape'
                        and abs(simple['point'][2] - point[2]) < 1 and abs(complex_hit['point'][2] - point[2]) < 1
                        and min(simple['normal'][2], complex_hit['normal'][2]) >= 0.98,
                        'Entrance is not on the measured unobstructed public ground: ' + zone_id)
                require(projected is not None and zone.contains_world_point(requested)
                        and zone.contains_world_point(projected)
                        and zone.contains_world_point(unreal.Vector(*complex_hit['point'])),
                        'Entrance physical/navigation point escaped its zone: ' + zone_id)
                projected_point = vec(projected)
                margin = min(boundary_distance(point, polygon), boundary_distance(projected_point, polygon))
                require(margin >= 300 and math.hypot(projected.x - point[0], projected.y - point[1]) <= 50
                        and 0 <= projected.z - point[2] <= 30, 'Entrance projection or margin changed: ' + zone_id)
                errors = zone.validate_zone_data()
                require(errors is not None and len(errors) == 0, 'Invalid zone data: ' + zone_id)
                evidence.append({'zone': zone_id, 'requested': point, 'local': vec(anchors[0].translation),
                    'projected': projected_point, 'boundary_margin_cm': margin,
                    'world_top_simple': simple, 'world_top_complex': complex_hit})
            return evidence

        cube = unreal.load_asset('/Engine/BasicShapes/Cube')
        require(cube is not None, 'Built-in ramp mesh unavailable')
        ramps = {}
        for spec in RAMPS:
            matches = [actor for actor in actors if actor.get_actor_label() == spec['label']]
            require(len(matches) <= 1, 'Duplicate repair actor: ' + spec['label'])
            if matches:
                ramps[spec['label']] = matches[0]
        require(len(ramps) in (0, 2), 'Partial repair already exists; refuse to create duplicates')

        def verify_ramps(plans=RAMPS):
            evidence = []
            current_actors = list(subsystem.get_all_level_actors())
            for spec in plans:
                actor, shape = ramps[spec['label']], geometry(spec)
                component = actor.get_component_by_class(unreal.StaticMeshComponent)
                material = components[spec['source']].get_material(spec['source_material_slot'])
                require(material and material.get_name() == spec['material_name'], 'Source ramp material changed')
                actual = transform(actor)
                require(all(close(actual[name], shape[name]) for name in ('location', 'rotation', 'scale')),
                        'Existing repair actor transform mismatch: ' + spec['label'])
                require(component and component.static_mesh == cube and component.get_material(0) == material,
                        'Existing repair actor geometry/material mismatch: ' + spec['label'])
                require(str(component.get_collision_profile_name()) == 'BlockAll', 'Ramp is not physically blocking')
                require(TAG in [str(tag) for tag in actor.get_editor_property('tags')], 'Ramp is missing fine-grounding tag')
                require(not actor.get_editor_property('hidden') and component.get_editor_property('visible'), 'Ramp is hidden')
                checks = []
                for fraction in (0.03, 0.25, 0.5, 0.75, 0.97):
                    point = [spec['high'][i] + (spec['low'][i] - spec['high'][i]) * fraction for i in range(3)]
                    hit = trace(point, [value for value in current_actors if value != actor])
                    checks.append({'fraction': fraction, 'expected': point, 'hit': hit})
                    require(hit and abs(hit['point'][2] - point[2]) < 1, 'Ramp physical surface transform mismatch')
                evidence.append({'label': spec['label'], 'transform': actual, 'mesh': cube.get_path_name(),
                                 'material': material.get_path_name(), 'source_material_slot': spec['source_material_slot'],
                                 'tags': [str(tag) for tag in actor.get_editor_property('tags')],
                                 'surface_checks': checks})
            return evidence

        def verify_parking_approaches():
            spec = RAMPS[1]
            shape = geometry(spec)
            length = math.hypot(spec['low'][0] - spec['high'][0], spec['low'][1] - spec['high'][1])
            along = [(spec['low'][i] - spec['high'][i]) / length for i in range(2)]
            across = shape['across']
            records, routes = [], []
            for side in (-180, 0, 180):
                previous = None
                for index in range(math.ceil((length + 200) / 10) + 1):
                    distance = -100 + (length + 200) * index / math.ceil((length + 200) / 10)
                    point = [spec['high'][i] + along[i] * distance + across[i] * side for i in range(2)] + [0]
                    simple, complex_hit = trace(point, [], False, 4000), trace(point, [], True, 700)
                    row = {'along_from_upper_cm': distance, 'side_cm': side, 'simple_from_4000': simple,
                           'complex_from_700': complex_hit}
                    records.append(row)
                    require(simple and complex_hit and simple['actor'] in (
                        spec['label'], 'Parking211_Optimized', 'Landscape'), 'Parking approach intersects an overhead road or obstruction')
                    # Long complex rays exactly on the engine cube's center seam
                    # can miss a triangle. Near complex rays agree with the exact
                    # box; +/-0.1 cm long-ray probes independently verified this.
                    require(abs(simple['point'][2] - complex_hit['point'][2]) <= 5,
                            'Parking simple/near-complex surfaces disagree')
                    if previous is not None:
                        require(abs(simple['point'][2] - previous) <= 35, 'Parking connection exceeds the original walkable step')
                    previous = simple['point'][2]
                endpoints = []
                for distance in (-100, length + 100):
                    point = [spec['high'][i] + along[i] * distance + across[i] * side for i in range(2)] + [0]
                    hit = trace(point, [], False, 4000)
                    projected = project(unreal.Vector(*hit['point']), (100, 100, 60)) if hit else None
                    endpoints.append(projected)
                forward, reverse = reachable(*endpoints), reachable(*reversed(endpoints))
                routes.append({'side_cm': side, 'forward': forward, 'reverse': reverse,
                               'endpoints': [vec(p) if p is not None else None for p in endpoints]})
                require(forward and reverse, 'Parking 100 cm approach route is incomplete')
            return {'surface_samples': records, 'directed_approach_routes': routes}

        report['before'] = audit()
        # This one lattice sample previously selected the large convex hull's
        # artificial top face. Preserve independent physical evidence rather than
        # silently treating every former lattice projection as real ground.
        edge_zone = next(row for row in report['before']['zones'] if row['zone'] == 'GZ_TestMain_27')
        edge_sample = next(sample for sample in edge_zone['samples'] if sample['sample_id'] == '0:0')
        deck_actor = targets['Deck_Flat_StairNormal']
        edge_evidence = []
        for kind, point in (('requested', edge_sample['requested_world']), ('projected', edge_sample['projected_world'])):
            if point is not None:
                only_deck = [actor for actor in actors if actor != deck_actor]
                edge_evidence.append({'kind': kind, 'point': point,
                    'deck_simple': trace(point, only_deck, False), 'deck_complex': trace(point, only_deck, True),
                    'world_simple': trace(point, [], False), 'world_complex': trace(point, [], True)})
        report['zone27_edge_physics_before'] = edge_evidence
        collision_ready = all(mesh.get_editor_property('body_setup').get_editor_property('collision_trace_flag')
                              == unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
                              and not any(mesh_state(mesh)['simple_shapes'].values()) for mesh in meshes.values())
        if ramps:
            require(collision_ready, 'Existing ramps with incomplete mesh collision repair')
            report['entrances'] = verify_entrances()
            strict(report['before'])
            parking_actor = ramps[RAMPS[1]['label']]
            old_shape = geometry(LEGACY_PARKING)
            actual = transform(parking_actor)
            legacy = all(close(actual[name], old_shape[name]) for name in ('location', 'rotation', 'scale'))
            deck_component = ramps[RAMPS[0]['label']].get_component_by_class(unreal.StaticMeshComponent)
            legacy_deck_material = deck_component.get_material(0) == components[RAMPS[0]['source']].get_material(0)
            if legacy or legacy_deck_material:
                report['ramps_before'] = verify_ramps((LEGACY_DECK_MATERIAL if legacy_deck_material else RAMPS[0],
                                                       LEGACY_PARKING if legacy else RAMPS[1]))
                report['repair_needed'] = True
                report['status'] = 'walkway-revision-upgrade-needed'
                if apply:
                    report['backups'] = []
                    for path in files:
                        backup = out / 'Backups' / path.relative_to(root)
                        backup.parent.mkdir(parents=True, exist_ok=True)
                        shutil.copy2(path, backup)
                        require(digest(backup) == before_hashes[str(path)], 'Revision backup verification failed')
                        report['backups'].append({'source': str(path), 'backup': str(backup), 'sha256': digest(backup)})
                    transaction = AssetSaveTransaction([MAP], 'WalkwayRevision3')
                    write()
                    report['changed_actor_transforms'] = []
                    report['changed_component_materials'] = []
                    if legacy:
                        shape = geometry(RAMPS[1])
                        parking_actor.modify()
                        parking_actor.set_actor_location(unreal.Vector(*shape['location']), False, True)
                        parking_actor.set_actor_rotation(unreal.Rotator(pitch=shape['rotation'][0], yaw=shape['rotation'][1], roll=0), False)
                        parking_actor.set_actor_scale3d(unreal.Vector(*shape['scale']))
                        expected_authored['actors'][parking_actor.get_path_name()] = transform(parking_actor)
                        report['changed_actor_transforms'].append(parking_actor.get_path_name())
                        require(unreal.FFZSNavigationEditorLibrary.prepare_world_for_navigation(world), 'Updated parking physics not ready')
                        unreal.SystemLibrary.execute_console_command(world, 'RebuildNavigation')
                    if legacy_deck_material:
                        material = components[RAMPS[0]['source']].get_material(RAMPS[0]['source_material_slot'])
                        require(material and material.get_name() == RAMPS[0]['material_name'], 'Deck wood material identity changed')
                        deck_component.modify()
                        deck_component.set_material(0, material)
                        expected_authored['components'][deck_component.get_path_name()]['materials'][0] = material.get_path_name()
                        report['changed_component_materials'].append({'component': deck_component.get_path_name(),
                            'slot': 0, 'source_slot': RAMPS[0]['source_material_slot'], 'material': material.get_path_name()})
                    report['ramps'] = verify_ramps()
                    report['parking_approaches'] = verify_parking_approaches()
                    report['after'] = audit()
                    strict(report['after'])
                    report['coverage_comparison'] = coverage_regressions(report['before'], report['after'])
                    report['authored_geometry_gates_and_other_anchors_unchanged'] = authored() == expected_authored
                    require(report['authored_geometry_gates_and_other_anchors_unchanged'], 'Revision changed an unrelated authored input')
                    report['meshes_after'] = {label: mesh_state(mesh) for label, mesh in meshes.items()}
                    require(report['meshes_after'] == report['meshes_before'], 'Revision changed a source mesh')
                    write()
                    require(unreal.EditorLoadingAndSavingUtils.save_map(world, MAP), 'Walkway revision map save failed')
                    require(authored() == expected_authored, 'Saving walkway revision changed authored inputs')
                    transaction.commit()
                    transaction = None
                    report['saved'] = True
                    report['repair_needed'] = False
                    report['status'] = 'walkway-revision-upgraded'
            else:
                report['ramps'] = verify_ramps()
                report['parking_approaches'] = verify_parking_approaches()
                report['repair_needed'] = False
                report['status'] = 'already-applied'
        else:
            report['repair_needed'] = True
            require(not collision_ready, 'Mesh-only partial repair requires manual review')
            require(all(not before_authored['zones'][zone_id]['anchors'] for zone_id in ENTRANCES),
                    'Expected three empty entrance arrays; refuse to overwrite existing authored entrances')
            endpoint_checks = []
            for spec in RAMPS:
                source = targets[spec['source']]
                shape = geometry(spec)
                records = []
                for endpoint in ('high', 'low'):
                    for side in (-spec['width'] / 2, 0, spec['width'] / 2):
                        point = list(spec[endpoint])
                        point[:2] = [point[i] + shape['across'][i] * side for i in range(2)]
                        ignore = [actor for actor in actors if actor != source] if endpoint == 'high' else [source]
                        hit = trace(point, ignore)
                        records.append({'endpoint': endpoint, 'side_cm': side, 'requested': point, 'hit': hit})
                        require(hit and hit['normal'][2] >= 0.98 and abs(hit['point'][2] - (point[2] - 2)) < 15,
                                'Measured ramp endpoint has changed: ' + spec['label'])
                endpoint_checks.append({'label': spec['label'], 'checks': records})
            report['endpoint_checks'] = endpoint_checks
            write()
            if apply:
                require(report['search_node_limit'] >= 8192, 'Repair requires the validated 8192-node navigation budget')
                report['backups'] = []
                for path in files:
                    backup = out / 'Backups' / path.relative_to(root)
                    backup.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(path, backup)
                    require(digest(backup) == before_hashes[str(path)], 'Permanent backup verification failed')
                    report['backups'].append({'source': str(path), 'backup': str(backup), 'sha256': digest(backup)})
                transaction = AssetSaveTransaction(packages, 'WalkableGeometryRepair')
                write()
                for mesh in meshes.values():
                    require(unreal.FFZSNavigationEditorLibrary.apply_triangle_collision_for_navigation(world, mesh, True),
                            'Precise collision refresh failed: ' + mesh.get_path_name())
                for spec in RAMPS:
                    shape = geometry(spec)
                    actor = subsystem.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*shape['location']),
                        unreal.Rotator(pitch=shape['rotation'][0], yaw=shape['rotation'][1], roll=shape['rotation'][2]))
                    require(actor is not None, 'Failed to create visible connector')
                    actor.set_actor_label(spec['label'])
                    actor.set_editor_property('tags', [unreal.Name(TAG)])
                    component = actor.static_mesh_component
                    require(component.set_static_mesh(cube), 'Failed to assign ramp geometry')
                    component.set_collision_profile_name('BlockAll')
                    component.set_material(0, components[spec['source']].get_material(spec['source_material_slot']))
                    actor.set_actor_scale3d(unreal.Vector(*shape['scale']))
                    ramps[spec['label']] = actor
                require(unreal.FFZSNavigationEditorLibrary.prepare_world_for_navigation(world), 'Ramp physics not ready')
                unreal.SystemLibrary.execute_console_command(world, 'RebuildNavigation')
                report['ramps'] = verify_ramps()
                report['parking_approaches'] = verify_parking_approaches()
                report['entrance_changes'] = []
                for zone_id, point in ENTRANCES.items():
                    zone = next(zone for zone in zones if str(zone.get_editor_property('zone_id')) == zone_id)
                    anchor = unreal.Transform()
                    anchor.translation = unreal.MathLibrary.inverse_transform_location(zone.get_actor_transform(), unreal.Vector(*point))
                    zone.modify()
                    zone.set_editor_property('entry_exit_anchors', [anchor])
                    expected_authored['zones'][zone_id]['anchors'] = [anchor.export_text()]
                    report['entrance_changes'].append({'zone': zone_id, 'before': [], 'after': [anchor.export_text()]})
                report['entrances'] = verify_entrances()
                report['after'] = audit()
                report['coverage_comparison'] = coverage_regressions(report['before'], report['after'])
                report['authored_geometry_gates_and_other_anchors_unchanged'] = authored() == expected_authored
                require(report['authored_geometry_gates_and_other_anchors_unchanged'],
                        'An authored transform, zone boundary, neighbor, other entrance, gate, or navigation shape changed')
                strict(report['after'])
                report['meshes_after'] = {label: mesh_state(mesh) for label, mesh in meshes.items()}
                for label in LABELS:
                    old, new = report['meshes_before'][label], report['meshes_after'][label]
                    require(old['render_geometry'] == new['render_geometry'] and old['materials'] == new['materials'],
                            'Repair modified source render geometry or material slots')
                write()
                for mesh in meshes.values():
                    require(unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False), 'Static mesh save failed')
                require(unreal.EditorLoadingAndSavingUtils.save_map(world, MAP), 'Map save failed')
                require(authored() == expected_authored, 'Saving changed authored inputs')
                transaction.commit()
                transaction = None
                report['saved'] = True
                report['repair_needed'] = False
                report['status'] = 'applied'
            else:
                report['status'] = 'inspection-complete'
        report['after_sha256'] = {str(path): digest(path) for path in files}
        if not report['saved']:
            require(report['after_sha256'] == before_hashes, 'Read-only/idempotent run modified a package')
    except Exception:
        report['status'] = 'failed'
        report['error'] = traceback.format_exc()
        if transaction is not None:
            current_hashes = {str(path): digest(path) for path in files}
            if current_hashes == before_hashes:
                # A pre-save validation failure changed only loaded UObjects.
                # Do not overwrite unchanged packages held open by the loader.
                transaction.commit()
                report['rollback_not_needed'] = 'No source package bytes were written'
            else:
                try:
                    transaction.rollback()
                except Exception:
                    report['rollback_error'] = traceback.format_exc()
                    raise
            report['rollback_sha256'] = {str(path): digest(path) for path in files}
            require(report['rollback_sha256'] == before_hashes, 'Rollback bytes differ from the original packages')
        raise
    finally:
        write()
        unreal.log('WALKABLE_GEOMETRY_REPAIR_REPORT: ' + str(out / 'report.json'))


if __name__ == '__main__':
    run()
