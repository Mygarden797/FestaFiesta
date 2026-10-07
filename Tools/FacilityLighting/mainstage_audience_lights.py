# 역할: 메인 무대의 기존 조명을 보존하면서 관객용 워시 조명 두 개를 추가한다.
"""Author the two audience washes in the MainStage mesh/rig coordinate system.

The production mesh is fitted to 2535 x 1625 x 1050 cm and rotated +90 yaw.
These elevated front-truss lamps face mesh +X (facility +Y), covering the
25 x 20 audience layout through its back row at Y=3492.5 cm. Existing lamps
and subsequent designer edits are preserved; only missing washes are added.
"""
import unreal

AUDIENCE_LIGHT_NAMES = {'AudienceWash_Left_Spot', 'AudienceWash_Right_Spot'}


def ensure_audience_lights(bp):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    fn = unreal.SubobjectDataBlueprintFunctionLibrary
    handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
    parent = next(h for h in handles if fn.is_root_component(fn.get_data(h)))
    existing = {fn.get_object_for_blueprint(fn.get_data(h), bp).get_name().removesuffix('_GEN_VARIABLE')
                for h in handles if fn.get_object_for_blueprint(fn.get_data(h), bp)}
    added = []
    for side, sign in (('Left', 1.0), ('Right', -1.0)):
        name = 'AudienceWash_' + side + '_Spot'
        if name in existing:
            continue
        params = unreal.AddNewSubobjectParams(parent_handle=parent,
            new_class=unreal.SpotLightComponent, blueprint_context=bp)
        handle, reason = subsystem.add_new_subobject(params)
        assert fn.is_handle_valid(handle), str(reason)
        assert subsystem.rename_subobject(handle, unreal.Text(name)), name
        lamp = fn.get_object_for_blueprint(fn.get_data(handle), bp)
        lamp.set_editor_property('relative_location', unreal.Vector(800, sign * 1800, 1700))
        lamp.set_editor_property('relative_rotation', unreal.Rotator(pitch=-21, yaw=-sign * 4, roll=0))
        lamp.set_mobility(unreal.ComponentMobility.MOVABLE)
        lamp.set_editor_property('intensity_units', unreal.LightUnits.LUMENS)
        lamp.set_editor_property('intensity', 120000.0)
        lamp.set_editor_property('light_color', unreal.Color(r=160, g=228, b=255, a=255))
        # Runtime applies the fitted mesh scale, giving about 52 m of attenuation.
        # Keep the back row well inside this radius, away from the fade boundary.
        lamp.set_editor_property('attenuation_radius', 10000.0)
        lamp.set_editor_property('inner_cone_angle', 45.0)
        lamp.set_editor_property('outer_cone_angle', 55.0)
        lamp.set_editor_property('use_inverse_squared_falloff', True)
        lamp.set_editor_property('source_radius', 35.0)
        # Broad crowd washes must not add two whole-crowd shadow passes.
        lamp.set_editor_property('cast_shadows', False)
        lamp.set_editor_property('volumetric_scattering_intensity', 0.25)
        lamp.set_editor_property('visible', True)
        added.append(name)
    return added


if __name__ == '__main__':
    from pathlib import Path
    import json
    import shutil

    root = Path(__file__).resolve().parents[2]
    package = '/Game/Asset/FacilitiesAsset_HKH/Performance/MainStage_Grand_20260909/BP_MainStage_Grand_Lighting'
    asset_file = root / 'Content' / (package.removeprefix('/Game/') + '.uasset')
    output = root / 'Saved/MainStageAudienceLighting'
    output.mkdir(parents=True, exist_ok=True)
    backup = output / 'BP_MainStage_Grand_Lighting.before.uasset'
    if not backup.exists():
        shutil.copy2(asset_file, backup)
    bp = unreal.load_asset(package)
    assert bp and bp.generated_class(), 'Production MainStage rig is required'
    added = ensure_audience_lights(bp)
    if added:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
    (output / 'apply.json').write_text(json.dumps({'added': added, 'rig': package}, indent=2), encoding='utf-8')
    unreal.log('MAINSTAGE_AUDIENCE_LIGHTS_PASS ' + str(added))
