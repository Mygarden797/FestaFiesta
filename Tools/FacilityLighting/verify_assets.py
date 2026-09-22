"""Fresh-session, read-only asset/bundle checks plus idempotent WBP migration check."""
import hashlib
import json
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT/'Saved/FacilityLighting_20260910'
PDA = '/Game/FestaFiesta/Building/FF_Data_Building/Performance/PDA_FFZS_Performance_MainStage_SunsetMainStage'
RIG = '/Game/Asset/FacilitiesAsset_HKH/Performance/MainStage_Grand_20260909/BP_MainStage_Grand_Lighting'
EXPECTED_RUNTIME_LIGHTS = {
    'Overhead_00_Spot', 'Overhead_02_Spot', 'Overhead_04_Spot',
    'Overhead_06_Spot', 'Overhead_10_Spot',
    'DeckSpot_Spot', 'DeckSpot_Spot_001',
    'RunwaySpot_Spot', 'RunwaySpot_Spot_001',
}
pda = unreal.load_asset(PDA)
bp = unreal.load_asset(RIG)
assert pda and bp and bp.generated_class()
settings = pda.get_editor_property('spec').get_editor_property('lighting_settings')
assert settings.get_editor_property('lighting_rig_class') == bp.generated_class()
assert settings.get_editor_property('enabled')
assert settings.get_editor_property('brightness_multiplier') == 1.0
assert isinstance(pda.get_editor_property('catalog_thumbnail'), unreal.Texture2D)
# UE 5.7 moves bundle metadata out of generic tags. The native production test
# verifies FAssetData::TaggedAssetBundles; Python cannot expose that typed field.
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
fn = unreal.SubobjectDataBlueprintFunctionLibrary
lights = {}
preview = None
for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
    obj = fn.get_object_for_blueprint(fn.get_data(handle), bp)
    if isinstance(obj, unreal.SpotLightComponent):
        lights[obj.get_name()] = {'intensity':obj.get_editor_property('intensity'),
            'movable':obj.get_editor_property('mobility') == unreal.ComponentMobility.MOVABLE}
    elif isinstance(obj, unreal.StaticMeshComponent):
        preview = obj
# The source preview has ten lights, but the artist-authored runtime rig has
# exactly these nine since revision 67483cfb. Reject both missing and extra lamps.
actual_light_names = {name.removesuffix('_GEN_VARIABLE') for name in lights}
assert actual_light_names == EXPECTED_RUNTIME_LIGHTS, {
    'missing': sorted(EXPECTED_RUNTIME_LIGHTS - actual_light_names),
    'unexpected': sorted(actual_light_names - EXPECTED_RUNTIME_LIGHTS),
}
assert all(x['intensity'] > 0 and x['movable'] for x in lights.values())
assert preview and preview.get_editor_property('is_editor_only') and preview.get_editor_property('hidden_in_game')
files = [ROOT/'Content/FestaFiesta/Building/FF_WBP_Building/Facility'/name for name in
         ('WBP_FF_FacilityInfoPanel.uasset','WBP_FF_FacilityLightingControls.uasset')]
before = [hashlib.sha256(path.read_bytes()).hexdigest() for path in files]
unreal.SystemLibrary.execute_console_command(None, 'FF.BuildFacilityInfoUMG')
after = [hashlib.sha256(path.read_bytes()).hexdigest() for path in files]
assert before == after, 'Repeated build changed designer assets'
report = {'status':'PASS','lights':lights,'editor_only_preview':True,
          'bundle_check':'native ProductionMainStageInstallation test',
          'wbp_rebuild_preserves_saved_assets':True,'wbp_sha256':before}
(OUT/'asset_verification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('FACILITY_LIGHTING_ASSET_VERIFICATION_PASS')
