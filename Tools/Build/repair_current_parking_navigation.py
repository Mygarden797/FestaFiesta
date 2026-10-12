"""Audit the current 13-zone release map; optionally repair ParkingLot_V1.

Run in a fresh UnrealEditor-Cmd Python commandlet. The default writes only an
audit under Artifacts/CurrentParkingNavigation. Explicitly set
FESTAFIESTA_REPAIR_CURRENT_PARKING=1 to back up and transactionally save only
ParkingLot_V1 and Test_Main. Then repeat the default audit in a fresh process
and run the production PIE tests. Never run concurrently with an editor/build.

The parking mesh's convex collision fills empty space above its pavement. The
repair uses its existing triangles for static collision and removes the old
simple shapes before baking navigation. It never moves gates, zones or actors,
adds connectors, changes navigation-agent settings, or edits other meshes.
"""
import hashlib
import json
import os
import shutil
import sys
import traceback
from datetime import datetime, timezone
from pathlib import Path

import unreal

MAP = '/Game/FestaFiesta/Maps/Test_Main'
MESH = '/Game/Level/Demo_Matereal/Mesh/기본건축물/주차장3/ParkingLot_V1'
EXPECTED_ZONE_IDS = {
    'GZ_Beach01', 'GZ_Beach02', 'GZ_Beach03', 'GZ_Beach04', 'GZ_Beach05',
    'GZ_DadaYajang', 'GZ_DadaepoForest', 'GZ_FountainPirates', 'GZ_FleaMarket',
    'GZ_PublicParkingWest', 'GZ_TemporaryParkingEast', 'GZ_Operations', 'GZ_Tutorial',
}
SHAPES = ('sphere_elems', 'box_elems', 'sphyl_elems', 'convex_elems', 'tapered_capsule_elems')
# Measured open pavement points, avoiding the curtain arch above the entrance.
PHYSICAL_POINTS = ((89800, 50100), (89800, 50400), (89800, 50850), (89800, 51000))


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def vec(value):
    return [float(value.x), float(value.y), float(value.z)]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def mesh_state(mesh):
    body = mesh.get_editor_property('body_setup')
    require(body is not None, 'Parking mesh has no body setup')
    geom = body.get_editor_property('agg_geom')
    return {
        'collision': str(body.get_editor_property('collision_trace_flag')),
        'simple_shapes': {name: len(geom.get_editor_property(name)) for name in SHAPES},
        'render_geometry': [{'vertices': mesh.get_num_vertices(lod), 'triangles': mesh.get_num_triangles(lod)}
                            for lod in range(mesh.get_num_lods())],
        'materials': [{'slot': str(slot.get_editor_property('material_slot_name')),
                       'material': slot.get_editor_property('material_interface').get_path_name()
                       if slot.get_editor_property('material_interface') else None}
                      for slot in mesh.get_editor_property('static_materials')],
    }


def run():
    root = Path(unreal.Paths.project_dir()).resolve()
    require((root / 'FestaFiesta.uproject').is_file(), 'Wrong project')
    sys.path.insert(0, str(root / 'Tools/facilities'))
    from ffzs_asset_save_transaction import AssetSaveTransaction

    apply = os.environ.get('FESTAFIESTA_REPAIR_CURRENT_PARKING') == '1'
    folder = root / 'Artifacts/CurrentParkingNavigation' / datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-%f')
    folder.mkdir(parents=True, exist_ok=False)
    report = {'schema_version': 1, 'mode': 'apply' if apply else 'inspect', 'status': 'running',
              'saved': False, 'target_packages': [MESH, MAP]}
    transaction, files, before_hashes = None, [], {}

    def write():
        (folder / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')

    try:
        world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
        require(world and unreal.FFZSNavigationEditorLibrary.prepare_world_for_navigation(world),
                'Release world geometry is not ready')
        subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        actors = list(subsystem.get_all_level_actors())
        navs = [actor for actor in actors if isinstance(actor, unreal.RecastNavMesh)]
        spawns = [actor for actor in actors if isinstance(actor, unreal.FF_SpawnActor)]
        zones = sorted((actor for actor in actors if isinstance(actor, unreal.FFZSGameplayZone)),
                       key=lambda actor: str(actor.get_editor_property('zone_id')))
        zone_ids = [str(zone.get_editor_property('zone_id')) for zone in zones]
        require(len(zone_ids) == 13 and set(zone_ids) == EXPECTED_ZONE_IDS, 'Current release-map zone identities changed')
        require(len(navs) == len(spawns) == 1, 'Expected one navigation mesh and one crowd spawn actor')
        nav, spawn = navs[0], spawns[0]
        gates = list(spawn.get_editor_property('crowd_gates'))
        require(len(gates) == 4 and {str(gate.get_editor_property('gate_id')) for gate in gates} == {'1', '2', '3', '4'},
                'Current release map must retain its four distinct authored gates')
        mesh = unreal.load_asset(MESH)
        require(mesh is not None, 'ParkingLot_V1 mesh is missing')
        mesh_actors = [actor for actor in actors if any(
            component.static_mesh == mesh for component in actor.get_components_by_class(unreal.StaticMeshComponent))]
        require(len(mesh_actors) == 1 and mesh_actors[0].get_actor_label() == 'ParkingLot_V1',
                'Parking mesh usage changed; review the repair scope')
        parking_actor = mesh_actors[0]
        exit_anchor = spawn.get_editor_property('DespawnAchor')
        exit_point = exit_anchor.get_actor_location() if exit_anchor else (
            spawn.get_actor_location() + spawn.get_editor_property('Temp_ExitOffsetFromOrigin'))

        def authored():
            current_actors = list(subsystem.get_all_level_actors())
            return {
                'actors': {actor.get_path_name(): {
                    'location': vec(actor.get_actor_location()), 'scale': vec(actor.get_actor_scale3d()),
                    'rotation': [actor.get_actor_rotation().pitch, actor.get_actor_rotation().yaw, actor.get_actor_rotation().roll],
                    'tags': [str(tag) for tag in actor.get_editor_property('tags')],
                } for actor in current_actors},
                'components': {component.get_path_name(): {
                    'mesh': component.static_mesh.get_path_name() if component.static_mesh else None,
                    'materials': [component.get_material(index).get_path_name() if component.get_material(index) else None
                                  for index in range(component.get_num_materials())],
                } for actor in current_actors for component in actor.get_components_by_class(unreal.StaticMeshComponent)},
                'zones': {str(zone.get_editor_property('zone_id')): {
                    'polygon': [[point.x, point.y] for point in zone.get_boundary_polygon_local_cm()],
                    'anchors': [anchor.export_text() for anchor in zone.get_editor_property('entry_exit_anchors')],
                    'neighbors': sorted(str(value) for value in zone.get_editor_property('neighbor_zone_ids')),
                    'vertical_extent_cm': float(zone.get_editor_property('vertical_extent_cm')),
                } for zone in zones},
                'gates': [gate.export_text() for gate in spawn.get_editor_property('crowd_gates')],
                'exit_requested': vec(exit_point),
                'navigation': {name: str(nav.get_editor_property(name)) for name in (
                    'agent_radius', 'agent_height', 'agent_max_slope', 'tile_size_uu', 'runtime_generation')},
                'resolution': [{name: float(item.get_editor_property(name)) for name in (
                    'cell_size', 'cell_height', 'agent_max_step_height')}
                    for item in nav.get_editor_property('nav_mesh_resolution_params')],
                'query_node_limit': unreal.FFZSNavigationEditorLibrary.get_navigation_query_node_limit(world),
            }

        def project(point, extent=(300, 300, 2000)):
            return unreal.NavigationSystemV1.project_point_to_navigation(world, point, nav, None, unreal.Vector(*extent))

        def reachable(start, end):
            if start is None or end is None:
                return False
            path = unreal.NavigationSystemV1.find_path_to_location_synchronously(world, start, end, nav)
            return path is not None and path.is_valid() and not path.is_partial() and len(path.path_points) > 0

        # Select each representative once before editing. A post-repair fallback
        # must never hide the loss of a previously reachable point.
        representatives = {}

        def audit():
            target = project(exit_point)
            rows, zone_points = [], []
            for zone in zones:
                zone_id = str(zone.get_editor_property('zone_id'))
                polygon = zone.get_boundary_polygon_local_cm()
                require(len(polygon) >= 3, 'Invalid polygon: ' + zone_id)
                lo = [min(getattr(point, axis) for point in polygon) for axis in ('x', 'y')]
                hi = [max(getattr(point, axis) for point in polygon) for axis in ('x', 'y')]
                samples = []
                for iy in range(9):
                    for ix in range(9):
                        local = unreal.Vector(lo[0] + (hi[0] - lo[0]) * (1 / 16 + ix * 7 / 64),
                                              lo[1] + (hi[1] - lo[1]) * (1 / 16 + iy * 7 / 64), 0)
                        point = unreal.MathLibrary.transform_location(zone.get_actor_transform(), local)
                        inside = zone.contains_world_point(point)
                        projected = project(point) if inside else None
                        samples.append({'sample_id': '{}:{}'.format(ix, iy), 'requested': vec(point), 'inside': inside,
                            'projected': vec(projected) if projected is not None else None,
                            'reachable': inside and reachable(projected, target)})
                anchors = list(zone.get_editor_property('entry_exit_anchors'))
                anchor = unreal.MathLibrary.transform_location(zone.get_actor_transform(), anchors[0].translation) if anchors else zone.get_actor_location()
                anchor_nav = project(anchor)
                anchor_row = {'requested': vec(anchor), 'projected': vec(anchor_nav) if anchor_nav is not None else None,
                              'to_exit': reachable(anchor_nav, target), 'from_exit': reachable(target, anchor_nav)}
                if zone_id not in representatives:
                    if anchor_row['to_exit'] and anchor_row['from_exit']:
                        representatives[zone_id] = {'kind': 'authored_anchor', 'requested': vec(anchor)}
                    else:
                        candidate = next((sample for sample in samples if sample['reachable'] and
                            reachable(target, unreal.Vector(*sample['projected']))), None)
                        require(candidate is not None, 'No baseline representative for ' + zone_id)
                        representatives[zone_id] = {'kind': 'lattice:' + candidate['sample_id'], 'requested': candidate['requested']}
                representative = representatives[zone_id]
                selected = project(unreal.Vector(*representative['requested']))
                zone_points.append((zone_id, selected))
                rows.append({'zone': zone_id, 'samples': samples, 'anchor': anchor_row,
                    'reachable_sample_count': sum(sample['reachable'] for sample in samples),
                    'representative': dict(representative, projected=vec(selected) if selected is not None else None,
                        to_exit=reachable(selected, target), from_exit=reachable(target, selected))})
            gate_rows, gate_points = [], []
            sim_origin = project(spawn.get_actor_location())
            for gate in gates:
                anchor = gate.get_editor_property('gate_anchor')
                point = anchor.get_actor_location() if anchor else gate.get_editor_property('gate_location')
                projected = project(point, (50, 50, 250))
                gate_id = str(gate.get_editor_property('gate_id'))
                gate_points.append(('gate:' + gate_id, projected))
                gate_rows.append({'gate_id': gate_id, 'requested': vec(point),
                    'projected': vec(projected) if projected is not None else None,
                    'to_exit': reachable(projected, target), 'from_exit': reachable(target, projected),
                    'to_sim_origin': reachable(projected, sim_origin), 'from_sim_origin': reachable(sim_origin, projected)})
            pairs = [{'from': name_a, 'to': name_b, 'complete': reachable(point_a, point_b)}
                     for name_a, point_a in zone_points + gate_points
                     for name_b, point_b in zone_points + gate_points if name_a != name_b]
            return {'exit': vec(target) if target is not None else None, 'zones': rows, 'gates': gate_rows,
                    'directed_pairs': pairs, 'failed_directed_pairs': [row for row in pairs if not row['complete']]}

        def trace(point, complex_trace):
            hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(point[0], point[1], 4000),
                unreal.Vector(point[0], point[1], -2500), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
                complex_trace, [], unreal.DrawDebugTrace.NONE, True)
            if isinstance(hit, tuple):
                hit = next((value for value in hit if isinstance(value, unreal.HitResult)), None)
            if hit is None:
                return None
            values = hit.to_tuple()
            if not values[0]:
                return None
            return {'point': vec(values[5]), 'normal': vec(values[7]),
                    'actor': values[9].get_path_name() if values[9] else None}

        def physical():
            rows = []
            for point in PHYSICAL_POINTS:
                simple, complex_hit = trace(point, False), trace(point, True)
                projected = project(unreal.Vector(point[0], point[1], complex_hit['point'][2]), (50, 50, 250)) if complex_hit else None
                rows.append({'xy': list(point), 'simple': simple, 'complex': complex_hit,
                             'nav': vec(projected) if projected is not None else None})
            return rows

        def validate_after(result, physics):
            require(result['exit'] is not None, 'Exit did not project')
            require(all(row['reachable_sample_count'] > 0 for row in result['zones']), 'A zone lost all reachable lattice samples')
            require(all(row['representative']['to_exit'] and row['representative']['from_exit'] for row in result['zones']),
                    'A fixed baseline zone representative is disconnected')
            require(not result['failed_directed_pairs'], 'A directed zone/zone, zone/gate or gate/gate route is incomplete')
            require(all(all(row[key] for key in ('to_exit', 'from_exit', 'to_sim_origin', 'from_sim_origin'))
                        for row in result['gates']), 'An authored gate is disconnected')
            for row in physics:
                simple, complex_hit, projected = row['simple'], row['complex'], row['nav']
                require(simple and complex_hit and projected, 'Parking pavement probe has no collision/navigation')
                require(simple['actor'] == complex_hit['actor'] == parking_actor.get_path_name(), 'Parking probe hits another object')
                require(abs(simple['point'][2] - complex_hit['point'][2]) <= 1.0,
                        'Simple collision still fills empty space above the rendered pavement')
                require(min(simple['normal'][2], complex_hit['normal'][2]) >= 0.98 and
                        abs(projected[2] - complex_hit['point'][2]) <= 30.0, 'Parking navigation is not on physical pavement')

        before_authored = authored()
        report['authored_before'] = before_authored
        report['mesh_before'] = mesh_state(mesh)
        for package in (MESH, MAP):
            base = root / 'Content' / package[len('/Game/'):]
            found = list(base.parent.glob(base.name + '.*'))
            require(any(path.suffix in ('.umap', '.uasset') for path in found), 'Package files missing: ' + package)
            files.extend(found)
        before_hashes = {str(path): digest(path) for path in files}
        report['before_sha256'] = before_hashes
        report['before'] = audit()
        report['physical_before'] = physical()
        report['repair_needed'] = bool(report['before']['failed_directed_pairs']) or any(report['mesh_before']['simple_shapes'].values())
        write()
        if apply:
            report['backups'] = []
            for path in files:
                backup = folder / 'Backups' / path.relative_to(root)
                backup.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, backup)
                require(digest(backup) == before_hashes[str(path)], 'Permanent backup hash mismatch')
                report['backups'].append({'source': str(path), 'backup': str(backup), 'sha256': digest(backup)})
            transaction = AssetSaveTransaction([MESH, MAP], 'CurrentParkingNavigation')
            require(unreal.FFZSNavigationEditorLibrary.apply_triangle_collision_for_navigation(world, mesh, True),
                    'Parking collision refresh failed')
            unreal.SystemLibrary.execute_console_command(world, 'RebuildNavigation')
            report['after'] = audit()
            report['physical_after'] = physical()
            report['mesh_after'] = mesh_state(mesh)
            report['authored_unchanged'] = authored() == before_authored
            require(report['authored_unchanged'], 'An actor, zone, gate, component or navigation setting changed')
            require(report['mesh_after']['render_geometry'] == report['mesh_before']['render_geometry'] and
                    report['mesh_after']['materials'] == report['mesh_before']['materials'], 'Render geometry or materials changed')
            require(not any(report['mesh_after']['simple_shapes'].values()), 'Simple collision shapes remain')
            regressions = []
            for old, new in zip(report['before']['zones'], report['after']['zones']):
                require(old['zone'] == new['zone'], 'Zone order changed')
                for a, b in zip(old['samples'], new['samples']):
                    require((a['sample_id'], a['requested'], a['inside']) == (b['sample_id'], b['requested'], b['inside']),
                            'Lattice inputs changed')
                    if a['reachable'] and not b['reachable']:
                        regressions.append(old['zone'] + ':' + a['sample_id'])
                for key in ('to_exit', 'from_exit'):
                    if old['anchor'][key] and not new['anchor'][key]:
                        regressions.append(old['zone'] + ':anchor:' + key)
            report['coverage_regressions'] = regressions
            require(not regressions, 'Previously reachable zone samples/anchors regressed')
            for old, new in zip(report['physical_before'], report['physical_after']):
                require(old['complex'] and new['complex'] and old['complex']['actor'] == new['complex']['actor'] and
                        abs(old['complex']['point'][2] - new['complex']['point'][2]) <= 1.0,
                        'The independently measured physical pavement changed')
            validate_after(report['after'], report['physical_after'])
            write()
            require(unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False), 'Parking mesh save failed')
            require(unreal.EditorLoadingAndSavingUtils.save_map(world, MAP), 'Map save failed')
            require(authored() == before_authored, 'Saving changed authored inputs')
            transaction.commit()
            transaction = None
            report['saved'], report['repair_needed'], report['status'] = True, False, 'applied'
        else:
            report['status'] = 'inspection-complete'
            try:
                validate_after(report['before'], report['physical_before'])
                report['current_validation_passed'] = True
            except RuntimeError as error:
                report['current_validation_passed'] = False
                report['current_validation_failure'] = str(error)
        report['after_sha256'] = {str(path): digest(path) for path in files}
        if not report['saved']:
            require(report['after_sha256'] == before_hashes, 'Read-only audit modified a source package')
    except Exception:
        report['status'], report['error'] = 'failed', traceback.format_exc()
        if transaction is not None:
            if {str(path): digest(path) for path in files} == before_hashes:
                transaction.commit()
                report['rollback_not_needed'] = 'Only in-memory objects changed; no package bytes were written'
            else:
                try:
                    transaction.rollback()
                except Exception:
                    report['rollback_error'] = traceback.format_exc()
                    raise
            report['rollback_sha256'] = {str(path): digest(path) for path in files}
            require(report['rollback_sha256'] == before_hashes, 'Rollback bytes differ from original packages')
        raise
    finally:
        write()
        unreal.log('CURRENT_PARKING_NAVIGATION_REPORT: ' + str(folder / 'report.json'))


if __name__ == '__main__':
    run()
