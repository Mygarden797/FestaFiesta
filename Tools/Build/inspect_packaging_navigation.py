"""Read-only Test_Main navigation/terrain inventory. Never saves the level."""
import json
from pathlib import Path
import unreal

world = unreal.EditorLoadingAndSavingUtils.load_map('/Game/FestaFiesta/Maps/Test_Main')
assert world
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
    if 'Zone' in cls:
        for prop in ['zone_id', 'entry_exit_anchors', 'boundary_polygon_local_cm']:
            try:
                row[prop] = str(actor.get_editor_property(prop))
            except Exception:
                pass
    if isinstance(actor, unreal.RecastNavMesh):
        row['nav_properties'] = {}
        for prop in ['agent_radius', 'agent_height', 'agent_max_slope', 'agent_max_step_height',
                     'cell_size', 'cell_height', 'runtime_generation', 'tile_size_uu']:
            try:
                row['nav_properties'][prop] = str(actor.get_editor_property(prop))
            except Exception:
                pass
    row['meshes'] = []
    for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
        mesh = comp.static_mesh
        row['meshes'].append({'component': comp.get_name(), 'mesh': mesh.get_path_name() if mesh else None,
                              'collision': str(comp.get_collision_enabled())})
    rows.append(row)
(out / 'terrain-inventory.json').write_text(json.dumps(rows, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('NAV_INVENTORY: {} actors'.format(len(rows)))
