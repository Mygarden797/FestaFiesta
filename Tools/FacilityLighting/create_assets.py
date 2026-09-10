"""Create the editable main-stage lighting rig and connect the facility PDA/UMG.

Run only after compiling the new native lighting classes. Existing rig components
and designer WBP layouts are preserved on subsequent runs.
"""
import json
import shutil
import traceback
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'Saved/FacilityLighting_20260910'
OUT.mkdir(parents=True, exist_ok=True)
STAGE = '/Game/Asset/FacilitiesAsset_HKH/Performance/MainStage_Grand_20260909'
PDA = '/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Performance/PDA_FFZS_Performance_MainStage_SunsetMainStage'
PANEL = '/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityInfoPanel'
CONTROLS = '/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityLightingControls'
RIG = STAGE + '/BP_MainStage_Grand_Lighting'


def backup(package):
    relative = package.removeprefix('/Game/') + '.uasset'
    source = ROOT / 'Content' / relative
    target = OUT / 'before' / relative
    if source.is_file() and not target.exists():
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)


def main():
    report = {'status': 'FAIL', 'rig': RIG, 'pda': PDA, 'controls': CONTROLS}
    for package in (PDA, PANEL, CONTROLS, RIG):
        backup(package)
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    fn = unreal.SubobjectDataBlueprintFunctionLibrary
    source = unreal.load_asset(STAGE + '/BP_MainStage_Grand')
    mesh = unreal.load_asset(STAGE + '/Meshes/SM_MainStage_Grand')
    pda = unreal.load_asset(PDA)
    native_class = unreal.load_class(None, '/Script/FF_ZoneSystem.FFZSFacilityLightingRig')
    assert source and mesh and pda and native_class, 'Required source/native assets missing'
    bp = unreal.load_asset(RIG) if unreal.EditorAssetLibrary.does_asset_exist(RIG) else None
    created = not bp
    if created:
        bp = unreal.BlueprintEditorLibrary.create_blueprint_asset_with_parent(RIG, native_class)
        assert bp
        handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
        parent = next(h for h in handles if fn.is_root_component(fn.get_data(h)))

        def component(cls, name):
            params = unreal.AddNewSubobjectParams(parent_handle=parent, new_class=cls, blueprint_context=bp)
            handle, reason = subsystem.add_new_subobject(params)
            assert fn.is_handle_valid(handle), str(reason)
            assert subsystem.rename_subobject(handle, unreal.Text(name)), 'Cannot rename component to ' + name
            return fn.get_object_for_blueprint(fn.get_data(handle), bp)

        preview = component(unreal.StaticMeshComponent, 'PreviewStageMesh')
        preview.set_static_mesh(mesh)
        preview.set_mobility(unreal.ComponentMobility.MOVABLE)
        preview.set_collision_profile_name('NoCollision')
        preview.set_editor_property('is_editor_only', True)
        preview.set_editor_property('hidden_in_game', True)
        preview.set_editor_property('cast_shadow', False)
        copied = []
        for handle in subsystem.k2_gather_subobject_data_for_blueprint(source):
            original = fn.get_object_for_blueprint(fn.get_data(handle), source)
            if not isinstance(original, unreal.SpotLightComponent):
                continue
            name = original.get_name().removesuffix('_GEN_VARIABLE')
            # SCS handles can include the same inherited/template object more than once.
            if name in copied:
                continue
            lamp = component(unreal.SpotLightComponent, name)
            for key in ('relative_location', 'relative_rotation', 'intensity_units', 'intensity',
                        'light_color', 'attenuation_radius', 'inner_cone_angle', 'outer_cone_angle',
                        'use_inverse_squared_falloff', 'source_radius', 'source_length',
                        'cast_shadows', 'volumetric_scattering_intensity', 'component_tags',
                        'visible', 'affects_world'):
                lamp.set_editor_property(key, original.get_editor_property(key))
            lamp.set_mobility(unreal.ComponentMobility.MOVABLE)
            copied.append(name)
        assert len(copied) == 10, copied
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
        report['copied_lights'] = copied

    assert bp.generated_class(), 'Rig Blueprint must compile'
    spec = pda.get_editor_property('spec')
    report['previous_spec'] = spec.export_text()
    settings = spec.get_editor_property('lighting_settings')
    settings.set_editor_property('lighting_rig_class', bp.generated_class())
    settings.set_editor_property('enabled', True)
    settings.set_editor_property('brightness_multiplier', 1.0)
    settings.set_editor_property('override_color', False)
    spec.set_editor_property('lighting_settings', settings)
    spec.set_editor_property('visual_mesh', mesh)
    pda.set_editor_property('spec', spec)
    # The existing PDA retained a broken thumbnail reference after the /Asset move.
    # Restore the project's previously authored icon only when no texture resolves.
    if not isinstance(pda.get_editor_property('catalog_thumbnail'), unreal.Texture2D):
        image_folder = '/Game/Asset/FacilitiesAsset_HKH/Images/facilities/T_MainStage_Grand_Image'
        image_path = image_folder + '/T_MainStage_Grand_Image'
        texture = unreal.load_asset(image_path) if unreal.EditorAssetLibrary.does_asset_exist(image_path) else None
        if not texture:
            task = unreal.AssetImportTask()
            task.set_editor_property('filename', str(ROOT/'SourceArt/MainStageGrand_FacilityImage_20260910/MainStage_Grand.png'))
            task.set_editor_property('destination_path', image_folder)
            task.set_editor_property('destination_name', 'T_MainStage_Grand_Image')
            task.set_editor_property('automated', True)
            task.set_editor_property('replace_existing', False)
            task.set_editor_property('save', True)
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
            texture = unreal.load_asset(image_path)
            assert isinstance(texture, unreal.Texture2D), 'Facility thumbnail restoration failed'
            texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
            texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
            texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
            texture.set_editor_property('srgb', True)
            unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
        pda.set_editor_property('catalog_thumbnail', texture)
        report['repaired_missing_thumbnail'] = texture.get_path_name()
    assert unreal.EditorAssetLibrary.save_loaded_asset(pda, only_if_is_dirty=False)
    unreal.SystemLibrary.execute_console_command(None, 'FF.BuildFacilityInfoUMG')
    assert unreal.load_asset(CONTROLS), 'Lighting controls WBP was not generated'
    report.update({'status': 'PASS', 'created_rig': created, 'final_spec':spec.export_text(),
                   'designer_layout_preserved': True})
    (OUT/'asset_setup.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('FACILITY_LIGHTING_ASSET_SETUP_PASS')


try:
    main()
except Exception:
    (OUT/'asset_setup_error.txt').write_text(traceback.format_exc(),encoding='utf-8')
    raise
