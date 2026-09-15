"""Read the authored fixture meshes/lights and shared economy table before wiring."""
import json
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'Saved/FacilityLighting_20260910'
OUT.mkdir(parents=True, exist_ok=True)
BASE = '/Game/Asset/LighiingAsset_HKH/Test1_FestivalLights_20260910'
sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
fn = unreal.SubobjectDataBlueprintFunctionLibrary
records = []
for name in ('CurtainArch', 'Festoon', 'GoldenTree', 'RattanPendant', 'StarTree', 'FestivalStreetlamp'):
    folder = BASE + ('/Lighting/FestivalStreetlamp/Run_b31f2d864298' if name == 'FestivalStreetlamp' else '/' + name)
    bp = unreal.load_asset(folder + ('/BP_FestivalStreetlamp' if name == 'FestivalStreetlamp' else '/BP_FF_' + name))
    assert bp, name
    components = {}
    for handle in sub.k2_gather_subobject_data_for_blueprint(bp):
        obj = fn.get_object_for_blueprint(fn.get_data(handle), bp)
        if not isinstance(obj, unreal.SceneComponent):
            continue
        row = {'class': obj.get_class().get_name(), 'location': str(obj.get_editor_property('relative_location')), 'scale':str(obj.get_editor_property('relative_scale3d'))}
        if isinstance(obj, unreal.LightComponent):
            row.update(intensity=obj.get_editor_property('intensity'), color=str(obj.get_editor_property('light_color')))
        if isinstance(obj, unreal.StaticMeshComponent) and obj.static_mesh:
            mesh = obj.static_mesh
            bounds = mesh.get_bounding_box()
            row.update(mesh=mesh.get_path_name(), bounds=str(bounds), materials=[])
            for slot in mesh.static_materials:
                mat = slot.material_interface
                row['materials'].append({'path': mat.get_path_name(), 'scalars': [str(x) for x in unreal.MaterialEditingLibrary.get_scalar_parameter_names(mat)], 'vectors': [str(x) for x in unreal.MaterialEditingLibrary.get_vector_parameter_names(mat)]})
        components[obj.get_name()] = row
    records.append({'name':name, 'bp':bp.get_path_name(), 'components':components})
table = unreal.load_asset('/Game/FestaFiesta/Data/Economy/DT_FacilityCosts')
report = {'assets':records, 'costs':json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))}
(OUT / 'buildable_inspection.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('BUILDABLE_LIGHTING_INSPECTION_PASS')
