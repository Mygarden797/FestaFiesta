"""배경만 밝게 조정하는 인스턴스. 지구 조명/노출은 유지한다."""
from pathlib import Path
import json,unreal
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
DEST='/Game/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1'
mel=unreal.MaterialEditingLibrary
name='MI_BrightGlobe_Backdrop'
mi=unreal.load_asset(DEST+'/'+name) if unreal.EditorAssetLibrary.does_asset_exist(DEST+'/'+name) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,DEST,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
mel.set_material_instance_parent(mi,unreal.load_asset(DEST+'/M_BrightGlobe_Backdrop'))
mel.set_material_instance_scalar_parameter_value(mi,'BackgroundBrightness',4.5)
assert abs(mel.get_material_instance_scalar_parameter_value(mi,'BackgroundBrightness')-4.5)<.001
assert unreal.EditorAssetLibrary.save_loaded_asset(mi,False)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level(DEST+'/L_BrightGlobe_TitleStudy')
dome=next(a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if a.get_actor_label()=='FF_Clay_Backdrop')
dome.static_mesh_component.set_material(0,mi)
assert level.save_current_level()
(ROOT/'Saved/BrightGlobe/p04_background_tune.json').write_text(json.dumps({'status':'saved','material':mi.get_path_name(),'brightness':4.5,'lights_and_exposure_changed':False},indent=2),encoding='utf-8')
