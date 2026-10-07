# create_buildable_lights.py
# 역할: 시설 조명과 조명 제어 화면 작업에서 필요한 에셋 또는 설정을 생성한다.

"""Connect six existing fixtures to construction, instance controls and economy.

Run after compiling the Lighting category. Saves only the six new definitions,
dedicated rigs, opt-in emissive graphs and added economy rows. Reruns preserve
authored definitions, rig layouts and existing prices. Backups go under Saved.
"""
import csv
import json
import math
import shutil
import traceback
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'Saved/FacilityLighting_20260910'
OUT.mkdir(parents=True, exist_ok=True)
BASE = '/Game/Asset/LighiingAsset_HKH/Test1_FestivalLights_20260910'
DEFINITIONS = '/Game/FestaFiesta/Building/FF_Data_Building/Lighting'
FIXTURES = (
    ('CurtainArch', 'Decorative', '커튼 아치 조명'),
    ('Festoon', 'Decorative', '페스툰 조명'),
    ('GoldenTree', 'Decorative', '골든 트리 조명'),
    ('RattanPendant', 'Pendant', '라탄 펜던트 조명'),
    ('StarTree', 'Decorative', '스타 트리 조명'),
    ('FestivalStreetlamp', 'Streetlamp', '축제 가로등'),
)


def package(asset):
    return asset.get_path_name().split('.')[0]


def backup(asset):
    relative = package(asset).removeprefix('/Game/') + '.uasset'
    source = ROOT / 'Content' / relative
    target = OUT / 'before_buildable' / relative
    if source.is_file() and not target.exists():
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)


def save(asset):
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False), asset.get_path_name()


def add_emissive_controls(material):
    """Insert a color lerp and absolute multiplier, preserving masks and strength."""
    lib = unreal.MaterialEditingLibrary
    params = {str(x) for x in lib.get_scalar_parameter_names(material)}
    if 'FF_LightingBrightness' in params:
        return False
    if not params.intersection(('EmissiveStrength', 'GlowStrength')):
        return False
    original = lib.get_material_property_input_node(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert original, material.get_path_name()
    output_pin = lib.get_material_property_input_node_output_name(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    edges, visited, colors = [], set(), []

    def visit(node):
        if not node or node in visited:
            return
        visited.add(node)
        if isinstance(node, unreal.MaterialExpressionVectorParameter) and str(node.get_editor_property('parameter_name')) in ('EmissiveColor', 'GlowColor'):
            colors.append(node)
        names = lib.get_material_expression_input_names(node)
        inputs = lib.get_inputs_for_material_expression(material, node)
        assert len(names) == len(inputs), node.get_name()
        for pin, child in zip(names, inputs):
            if child:
                edges.append((child, node, pin))
                visit(child)

    visit(original)
    assert len(colors) == 1, 'Expected one authored emissive color: ' + material.get_path_name()
    backup(material)

    def node(cls, x, y):
        return lib.create_material_expression(material, cls, x, y)

    brightness = node(unreal.MaterialExpressionScalarParameter, 0, 800)
    brightness.set_editor_property('parameter_name', 'FF_LightingBrightness')
    brightness.set_editor_property('default_value', 1.0)
    use_color = node(unreal.MaterialExpressionScalarParameter, -500, 1000)
    use_color.set_editor_property('parameter_name', 'FF_LightingUseColorOverride')
    use_color.set_editor_property('default_value', 0.0)
    color = node(unreal.MaterialExpressionVectorParameter, -500, 1200)
    color.set_editor_property('parameter_name', 'FF_LightingColor')
    color.set_editor_property('default_value', unreal.LinearColor(1, 1, 1, 1))
    lerp = node(unreal.MaterialExpressionLinearInterpolate, -200, 1000)
    assert lib.connect_material_expressions(colors[0], '', lerp, 'A')
    assert lib.connect_material_expressions(color, '', lerp, 'B')
    assert lib.connect_material_expressions(use_color, '', lerp, 'Alpha')
    for child, consumer, pin in edges:
        if child == colors[0]:
            assert lib.connect_material_expressions(lerp, '', consumer, pin)
    multiply = node(unreal.MaterialExpressionMultiply, 300, 650)
    assert lib.connect_material_expressions(original, output_pin, multiply, 'A')
    assert lib.connect_material_expressions(brightness, '', multiply, 'B')
    assert lib.connect_material_property(multiply, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.recompile_material(material)
    save(material)
    return True


def create_rig(folder, name, source, mesh):
    path = folder + '/BP_FF_' + name + '_Lighting'
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        bp = unreal.load_asset(path)
        assert bp.generated_class()
        return bp
    parent_class = unreal.load_class(None, '/Script/FF_ZoneSystem.FFZSFacilityLightingRig')
    bp = unreal.BlueprintEditorLibrary.create_blueprint_asset_with_parent(path, parent_class)
    assert bp
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    fn = unreal.SubobjectDataBlueprintFunctionLibrary
    parent = next(h for h in sub.k2_gather_subobject_data_for_blueprint(bp) if fn.is_root_component(fn.get_data(h)))

    def component(cls, label):
        handle, reason = sub.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent, new_class=cls, blueprint_context=bp))
        assert fn.is_handle_valid(handle), str(reason)
        assert sub.rename_subobject(handle, unreal.Text(label)), label
        return fn.get_object_for_blueprint(fn.get_data(handle), bp)

    preview = component(unreal.StaticMeshComponent, 'PreviewFixtureMesh')
    preview.set_static_mesh(mesh)
    preview.set_mobility(unreal.ComponentMobility.MOVABLE)
    preview.set_collision_profile_name('NoCollision')
    preview.set_editor_property('is_editor_only', True)
    preview.set_editor_property('hidden_in_game', True)
    preview.set_editor_property('cast_shadow', False)
    copied = set()
    for handle in sub.k2_gather_subobject_data_for_blueprint(source):
        original = fn.get_object_for_blueprint(fn.get_data(handle), source)
        if not isinstance(original, unreal.LocalLightComponent) or original.get_name() in copied:
            continue
        cls = unreal.SpotLightComponent if isinstance(original, unreal.SpotLightComponent) else unreal.PointLightComponent
        lamp = component(cls, original.get_name().removesuffix('_GEN_VARIABLE'))
        keys = ['relative_location', 'relative_rotation', 'intensity_units', 'intensity', 'light_color',
                'attenuation_radius', 'use_inverse_squared_falloff', 'source_radius', 'source_length',
                'cast_shadows', 'volumetric_scattering_intensity', 'component_tags', 'visible',
                'affects_world', 'use_temperature', 'temperature']
        if isinstance(original, unreal.SpotLightComponent):
            keys += ['inner_cone_angle', 'outer_cone_angle']
        for key in keys:
            lamp.set_editor_property(key, original.get_editor_property(key))
        lamp.set_mobility(unreal.ComponentMobility.MOVABLE)
        copied.add(original.get_name())
    assert copied, name + ' has no authored lights'
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert bp.generated_class()
    save(bp)
    return bp


def main():
    rows, changed_materials = [], []
    for name, minor, display in FIXTURES:
        folder = BASE + ('/Lighting/FestivalStreetlamp/Run_b31f2d864298' if name == 'FestivalStreetlamp' else '/' + name)
        source = unreal.load_asset(folder + ('/BP_FestivalStreetlamp' if name == 'FestivalStreetlamp' else '/BP_FF_' + name))
        mesh = unreal.load_asset(folder + ('/SM_FestivalStreetlamp' if name == 'FestivalStreetlamp' else '/Meshes/SM_FF_' + name))
        assert source and mesh, name
        for slot in mesh.static_materials:
            material = slot.material_interface
            if add_emissive_controls(material):
                changed_materials.append(material.get_path_name())
        rig = create_rig(folder, name, source, mesh)
        definition_id = 'Lighting_' + minor + '_' + name
        path = DEFINITIONS + '/PDA_FFZS_' + definition_id
        definition = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
        if not definition:
            factory = unreal.DataAssetFactory()
            factory.set_editor_property('data_asset_class', unreal.load_class(None, '/Script/FF_ZoneSystem.FFZSFacilityDefinition'))
            definition = unreal.AssetToolsHelpers.get_asset_tools().create_asset('PDA_FFZS_' + definition_id, DEFINITIONS, None, factory)
            assert definition
            spec = definition.get_editor_property('spec')
            bounds = mesh.get_bounding_box()
            size = bounds.max - bounds.min
            # Rotate +X-authored fronts toward the facility's +Y direction, then
            # retain original dimensions with sub-centimeter rounding only.
            spec.set_editor_property('facility_definition_id', definition_id)
            spec.set_editor_property('display_name', unreal.Text(display))
            spec.set_editor_property('category', unreal.FFZSFacilityCategory.LIGHTING)
            spec.set_editor_property('footprint_size_cells', unreal.IntPoint(math.ceil(size.y - .001), math.ceil(size.x - .001)))
            spec.set_editor_property('placement_obstacle_height_cm', size.z)
            spec.set_editor_property('clearance_cells', 0)
            spec.set_editor_property('visual_mesh', mesh)
            spec.set_editor_property('visual_sizing_mode', unreal.FFZSVisualSizingMode.MATCH_PLACEMENT_BOUNDS)
            spec.set_editor_property('visual_rotation', unreal.Rotator(pitch=0, yaw=90, roll=0))
            for key in ('queue_reserved_rects', 'queue_reserved_cells', 'queue_spline_points_cm', 'anchors'):
                spec.set_editor_property(key, [])
            rules = spec.get_editor_property('queue_rules')
            rules.set_editor_property('max_waiting_agents', 0)
            rules.set_editor_property('service_capacity', 0)
            spec.set_editor_property('queue_rules', rules)
            settings = spec.get_editor_property('lighting_settings')
            settings.set_editor_property('lighting_rig_class', rig.generated_class())
            settings.set_editor_property('enabled', True)
            settings.set_editor_property('brightness_multiplier', 1.0)
            spec.set_editor_property('lighting_settings', settings)
            definition.set_editor_property('spec', spec)
            if name != 'FestivalStreetlamp':
                definition.set_editor_property('catalog_thumbnail', unreal.load_asset(BASE + '/Concepts/T_Concept_SM_FestivalLighting_' + name))
            definition.set_editor_property('show_in_build_catalog', True)
            errors = definition.is_definition_valid()
            (OUT / ('spec_' + name + '.txt')).write_text(spec.export_text(), encoding='utf-8')
            assert errors is not None and len(errors) == 0, str(errors)
            save(definition)
        rows.append({'name':name, 'definition':package(definition), 'spec':definition.get_editor_property('spec').export_text(), 'rig':package(rig)})
    table = unreal.load_asset('/Game/FestaFiesta/Data/Economy/DT_FacilityCosts')
    costs = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    existing = {row['Name'] for row in costs}
    source_costs = list(csv.DictReader((ROOT / 'data/unreal/facility_costs_v0.csv').read_text(encoding='utf-8-sig').splitlines()))
    additions = [dict(Name=row['Name'], InstallCost=int(row['InstallCost'])) for row in source_costs if row['Name'].startswith('Cost.Lighting.') and row['Name'] not in existing]
    if additions:
        backup(table)
        assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(costs + additions))
        save(table)
    report = {'status':'PASS', 'fixtures':rows, 'materials_added_controls':changed_materials, 'cost_rows_added':additions}
    (OUT / 'buildable_setup.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('BUILDABLE_LIGHTING_SETUP_PASS')


if __name__ == '__main__':
    try:
        main()
    except Exception:
        (OUT / 'buildable_setup_error.txt').write_text(traceback.format_exc(), encoding='utf-8')
        raise
