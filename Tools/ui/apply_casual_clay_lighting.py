# -*- coding: utf-8 -*-
"""Frontend 캐주얼 지구용 밝은 배경과 조명 구성. 기존 지구/선택 로직은 유지."""
from pathlib import Path
import json,traceback,unreal
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
report={}
try:
 level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);level.load_level('/Game/FestaFiesta/Maps/Frontend')
 actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);allactors=actors.get_all_level_actors()
 globe=next(a for a in allactors if isinstance(a,unreal.FF_AtlasGlobeActor));center=globe.get_actor_location()
 mel=unreal.MaterialEditingLibrary;atl=unreal.AssetToolsHelpers.get_asset_tools()
 dest='/Game/FestaFiesta/UI/Atlas/Earth/CasualClay_v1/Materials'
 path=dest+'/M_FF_ClayBackdrop'
 mat=unreal.load_asset(path)
 if not mat:
  mat=atl.create_asset('M_FF_ClayBackdrop',dest,unreal.Material,unreal.MaterialFactoryNew())
  mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT);mat.set_editor_property('two_sided',True)
  def node(cls,x,y,**props):
   n=mel.create_material_expression(mat,cls,x,y)
   for k,v in props.items():n.set_editor_property(k,v)
   return n
  def wire(a,o,b,i):assert mel.connect_material_expressions(a,o,b,i)
  wp=node(unreal.MaterialExpressionWorldPosition,-900,0)
  z=node(unreal.MaterialExpressionComponentMask,-700,0,r=False,g=False,b=True,a=False);wire(wp,'',z,'')
  bias=node(unreal.MaterialExpressionConstant,-700,180,r=float(center.z))
  sub=node(unreal.MaterialExpressionSubtract,-500,0);wire(z,'',sub,'A');wire(bias,'',sub,'B')
  div=node(unreal.MaterialExpressionDivide,-300,0,const_b=65000.0);wire(sub,'',div,'A')
  add=node(unreal.MaterialExpressionAdd,-100,0,const_b=.5);wire(div,'',add,'A')
  sat=node(unreal.MaterialExpressionSaturate,100,0);wire(add,'',sat,'')
  low=node(unreal.MaterialExpressionVectorParameter,100,200,parameter_name='MintColor',default_value=unreal.LinearColor(.52,.89,.82,1))
  high=node(unreal.MaterialExpressionVectorParameter,100,400,parameter_name='CreamColor',default_value=unreal.LinearColor(.98,.94,.78,1))
  lerp=node(unreal.MaterialExpressionLinearInterpolate,350,0);wire(low,'RGB',lerp,'A');wire(high,'RGB',lerp,'B');wire(sat,'',lerp,'Alpha')
  bright=node(unreal.MaterialExpressionScalarParameter,350,300,parameter_name='BackgroundBrightness',default_value=1.35)
  mult=node(unreal.MaterialExpressionMultiply,550,0);wire(lerp,'',mult,'A');wire(bright,'',mult,'B');assert mel.connect_material_property(mult,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
  mel.recompile_material(mat);assert unreal.EditorAssetLibrary.save_loaded_asset(mat,False)
 mi=unreal.load_asset(dest+'/MI_FF_ClayBackdrop')
 if not mi:mi=atl.create_asset('MI_FF_ClayBackdrop',dest,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
 mel.set_material_instance_parent(mi,mat)
 mel.set_material_instance_scalar_parameter_value(mi,'BackgroundBrightness',2.4)
 assert unreal.EditorAssetLibrary.save_loaded_asset(mi,False)
 def find_or_spawn(cls,label):
  existing=next((a for a in allactors if a.get_actor_label()==label),None)
  a=existing or actors.spawn_actor_from_class(cls,center)
  a.set_actor_label(label);a.set_folder_path('Atlas/CasualClayLighting');return a
 dome=find_or_spawn(unreal.StaticMeshActor,'FF_Clay_Backdrop')
 c=dome.static_mesh_component;c.set_mobility(unreal.ComponentMobility.MOVABLE)
 c.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Sphere'));c.set_material(0,mi)
 c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);c.set_cast_shadow(False)
 c.set_editor_property('affect_distance_field_lighting',False)
 dome.set_actor_scale3d(unreal.Vector(2000,2000,2000))
 key=next(a for a in allactors if isinstance(a,unreal.DirectionalLight))
 key.set_actor_rotation(unreal.Rotator(-38,-28,0),False)
 kc=key.get_component_by_class(unreal.DirectionalLightComponent);kc.set_mobility(unreal.ComponentMobility.MOVABLE)
 kc.set_intensity(2.5);kc.set_light_color(unreal.LinearColor(1,.96,.87,1))
 kc.set_editor_property('light_source_angle',4.0)
 kc.set_editor_property('forward_shading_priority',1)
 fill=find_or_spawn(unreal.DirectionalLight,'FF_Clay_FillLight');fill.set_actor_rotation(unreal.Rotator(-12,48,0),False)
 fc=fill.get_component_by_class(unreal.DirectionalLightComponent);fc.set_mobility(unreal.ComponentMobility.MOVABLE);fc.set_intensity(.18);fc.set_editor_property('cast_shadows',False);fc.set_light_color(unreal.LinearColor(.86,.95,1,1))
 fc.set_editor_property('forward_shading_priority',0)
 sky=find_or_spawn(unreal.SkyLight,'FF_Clay_SkyFill');sc=sky.get_component_by_class(unreal.SkyLightComponent);sc.set_mobility(unreal.ComponentMobility.MOVABLE)
 sc.set_intensity(.10);sc.set_editor_property('sky_distance_threshold',20000.0);sc.set_editor_property('lower_hemisphere_is_black',False)
 sc.recapture_sky()
 pp=next(a for a in allactors if isinstance(a,unreal.PostProcessVolume));pp.set_editor_property('unbound',True)
 s=pp.get_editor_property('settings')
 for k,v in {'override_auto_exposure_method':True,'auto_exposure_method':unreal.AutoExposureMethod.AEM_BASIC,'override_auto_exposure_min_brightness':True,'override_auto_exposure_max_brightness':True,'auto_exposure_min_brightness':1.0,'auto_exposure_max_brightness':1.0,'override_auto_exposure_bias':True,'auto_exposure_bias':1.0,'override_bloom_intensity':True,'bloom_intensity':.12}.items():s.set_editor_property(k,v)
 pp.set_editor_property('settings',s)
 report['previous_cloud_material']=str(globe.get_editor_property('cloud_material'));globe.set_editor_property('cloud_material',None)
 assert level.save_current_level()
 report.update(status='complete',key_intensity=2.5,fill_intensity=.18,sky_intensity=.10,background=mi.get_path_name(),clouds_hidden=True)
except Exception:
 report['status']='failed';report['error']=traceback.format_exc();unreal.log_error(report['error'])
(root/'Saved/CasualClay_lighting_apply.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')


