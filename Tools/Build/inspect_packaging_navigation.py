# inspect_packaging_navigation.py
# 역할: 지형 준비 완료 후 Test_Main의 존·충돌·NavMesh 설정과 경계를 읽어 기록한다.

"""Read-only Test_Main navigation/terrain inventory. Never saves the level."""
import json
from pathlib import Path
import unreal

world = unreal.EditorLoadingAndSavingUtils.load_map('/Game/FestaFiesta/Maps/Test_Main')
assert world
assert unreal.FFZSNavigationEditorLibrary.prepare_world_for_navigation(world), 'World geometry is not ready'
out = Path(unreal.Paths.project_dir()).resolve() / 'Artifacts/NavigationRepair'
out.mkdir(parents=True, exist_ok=True)

def vec(v):
    return [v.x, v.y, v.z]

rows = []
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    center, extent = actor.get_actor_bounds(False)
    name = actor.get_actor_label()
    cls = actor.get_class().get_name()
    if max(extent.x, extent.y) < 1500 and not any(s in cls for s in ['Zone', 'Spawn', 'NavMesh']):
        continue
    row = {'name': name, 'class': cls, 'location': vec(actor.get_actor_location()),
           'rotation': str(actor.get_actor_rotation()), 'scale': vec(actor.get_actor_scale3d()),
           'center': vec(center), 'extent': vec(extent)}
    if isinstance(actor, unreal.FFZSGameplayZone):
        row['zone_id'] = str(actor.get_editor_property('zone_id'))
        row['entry_exit_anchors'] = [vec(t.translation) for t in actor.get_editor_property('entry_exit_anchors')]
        row['boundary_polygon_local_cm'] = [[p.x, p.y] for p in actor.get_boundary_polygon_local_cm()]
    if isinstance(actor, unreal.RecastNavMesh):
        row['nav_properties'] = {}
        for prop in ['agent_radius', 'agent_height', 'agent_max_slope', 'runtime_generation', 'tile_size_uu']:
            row['nav_properties'][prop] = str(actor.get_editor_property(prop))
        row['nav_properties']['resolution_params'] = [
            {prop: setting.get_editor_property(prop) for prop in ('cell_size', 'cell_height', 'agent_max_step_height')}
            for setting in actor.get_editor_property('nav_mesh_resolution_params')]
    row['meshes'] = []
    for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
        mesh = comp.static_mesh
        row['meshes'].append({'component': comp.get_name(), 'mesh': mesh.get_path_name() if mesh else None,
                              'collision': str(comp.get_collision_enabled())})
    rows.append(row)
(out / 'terrain-inventory.json').write_text(json.dumps(rows, ensure_ascii=False, indent=2), encoding='utf-8')
# 현재 지형 준비가 끝난 뒤 읽은 실제 NavMesh 경계다. 두 좌표가 한 선분을 이룬다.
edges = unreal.FFZSNavigationEditorLibrary.get_navigation_boundary_edges(world)
assert len(edges) % 2 == 0, 'Navigation boundary edges are incomplete'
(out / 'nav-boundaries.json').write_text(json.dumps([vec(point) for point in edges]), encoding='utf-8')
unreal.log('NAV_INVENTORY: {} actors'.format(len(rows)))
