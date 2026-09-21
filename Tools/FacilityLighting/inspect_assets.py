"""Read-only record of the main-stage PDA, lighting templates and panel asset."""
import json
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'Saved/FacilityLighting_20260910'
OUT.mkdir(parents=True, exist_ok=True)
pda_path = '/Game/FestaFiesta/Building/FF_Data_Building/Performance/PDA_FFZS_Performance_MainStage_SunsetMainStage'
bp_path = '/Game/Asset/FacilitiesAsset_HKH/Performance/MainStage_Grand_20260909/BP_MainStage_Grand'
panel_path = '/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityInfoPanel'
pda = unreal.load_asset(pda_path)
bp = unreal.load_asset(bp_path)
panel = unreal.load_asset(panel_path)
assert pda and bp and panel, 'Required existing project assets are missing'
spec = pda.get_editor_property('spec')
records = []
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
fn = unreal.SubobjectDataBlueprintFunctionLibrary
for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
    obj = fn.get_object_for_blueprint(fn.get_data(handle), bp)
    if isinstance(obj, unreal.LightComponent):
        keys = ['relative_location', 'relative_rotation', 'intensity', 'intensity_units',
                'light_color', 'attenuation_radius', 'cast_shadows', 'mobility',
                'component_tags', 'inner_cone_angle', 'outer_cone_angle']
        records.append({'name': obj.get_name(), **{key: str(obj.get_editor_property(key)) for key in keys}})
result = {'pda':pda_path, 'spec':str(spec), 'spec_export': spec.export_text(),
          'blueprint':bp_path, 'lights':records, 'panel':panel.get_path_name()}
(OUT/'before_assets.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('FACILITY_LIGHTING_INSPECTION_PASS ' + str(len(records)) + ' lights')
