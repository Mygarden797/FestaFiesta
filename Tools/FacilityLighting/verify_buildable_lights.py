# verify_buildable_lights.py
# 역할: 시설 조명과 조명 제어 화면 작업에서 저장된 결과가 요구 조건에 맞는지 검증한다.

"""Fresh-session readback of the six saved construction definitions and visuals."""
import json
import sys
from pathlib import Path
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from create_buildable_lights import ROOT, BASE, DEFINITIONS, FIXTURES

OUT = ROOT / 'Saved/FacilityLighting_20260910'
native = unreal.load_class(None, '/Script/FF_ZoneSystem.FFZSFacilityLightingRig')
sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
fn = unreal.SubobjectDataBlueprintFunctionLibrary
table = unreal.load_asset('/Game/FestaFiesta/Data/Economy/DT_FacilityCosts')
costs = {row['Name']: row['InstallCost'] for row in json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))}
rows = []
for name, minor, display in FIXTURES:
    definition_id = 'Lighting_' + minor + '_' + name
    definition = unreal.load_asset(DEFINITIONS + '/PDA_FFZS_' + definition_id)
    assert definition
    errors = definition.is_definition_valid()
    assert errors is not None and len(errors) == 0, name
    spec = definition.get_editor_property('spec')
    assert str(spec.get_editor_property('facility_definition_id')) == definition_id
    assert spec.get_editor_property('category') == unreal.FFZSFacilityCategory.LIGHTING
    assert definition.get_editor_property('show_in_build_catalog')
    assert not spec.get_editor_property('anchors') and not spec.get_editor_property('queue_reserved_rects')
    rules = spec.get_editor_property('queue_rules')
    assert rules.get_editor_property('service_capacity') == rules.get_editor_property('max_waiting_agents') == 0
    thumbnail = definition.get_editor_property('catalog_thumbnail')
    assert isinstance(thumbnail, unreal.Texture2D)
    assert thumbnail.get_path_name().startswith(DEFINITIONS + '/Thumbnails/T_UI_Lighting_' + name + '.')
    folder = BASE + ('/Lighting/FestivalStreetlamp/Run_b31f2d864298' if name == 'FestivalStreetlamp' else '/' + name)
    rig = unreal.load_asset(folder + '/BP_FF_' + name + '_Lighting')
    assert unreal.MathLibrary.class_is_child_of(rig.generated_class(), native)
    settings = spec.get_editor_property('lighting_settings')
    assert settings.get_editor_property('lighting_rig_class') == rig.generated_class()
    assert settings.get_editor_property('enabled') and settings.get_editor_property('brightness_multiplier') == 1.0
    lights, previews = {}, []
    for handle in sub.k2_gather_subobject_data_for_blueprint(rig):
        obj = fn.get_object_for_blueprint(fn.get_data(handle), rig)
        if isinstance(obj, unreal.LightComponent):
            assert obj.get_editor_property('mobility') == unreal.ComponentMobility.MOVABLE
            assert obj.get_editor_property('intensity') > 0
            lights[obj.get_name()] = obj.get_editor_property('intensity')
        elif isinstance(obj, unreal.StaticMeshComponent):
            assert obj.get_editor_property('is_editor_only') and obj.get_editor_property('hidden_in_game')
            previews.append(obj.get_name())
    assert lights and len(previews) == 1
    mesh = spec.get_editor_property('visual_mesh')
    controlled_materials = []
    for slot in mesh.static_materials:
        material = slot.material_interface
        params = {str(x) for x in unreal.MaterialEditingLibrary.get_scalar_parameter_names(material)}
        if not params.intersection(('EmissiveStrength', 'GlowStrength')):
            continue
        assert 'FF_LightingBrightness' in params and 'FF_LightingUseColorOverride' in params
        assert 'FF_LightingColor' in {str(x) for x in unreal.MaterialEditingLibrary.get_vector_parameter_names(material)}
        assert unreal.MaterialEditingLibrary.get_material_default_scalar_parameter_value(material, 'FF_LightingBrightness') == 1.0
        assert unreal.MaterialEditingLibrary.get_material_default_scalar_parameter_value(material, 'FF_LightingUseColorOverride') == 0.0
        controlled_materials.append(material.get_path_name())
    assert controlled_materials
    price = costs['Cost.Lighting.' + minor + '.' + name]
    assert price > 0
    rows.append({'fixture':name, 'valid':True, 'lights':lights, 'emissive_materials':controlled_materials,
                 'thumbnail':thumbnail.get_path_name(), 'cost':price})
report = {'status':'PASS', 'fixtures':rows, 'map_changes_required':False}
(OUT / 'buildable_verification.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('BUILDABLE_LIGHTING_VERIFICATION_PASS')
