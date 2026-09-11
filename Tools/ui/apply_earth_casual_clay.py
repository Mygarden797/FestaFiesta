# -*- coding: utf-8 -*-
"""새 머터리얼을 만들고 실제 메시 UV를 지리 좌표에 맞춰 Frontend에 적용한다."""
from pathlib import Path
import json,math,traceback
import unreal
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
DEST='/Game/FestaFiesta/UI/Atlas/Earth/CasualClay_v1'
report={}
try:
 mesh=unreal.load_asset('/Game/FestaFiesta/UI/Atlas/Earth/CitrusPop_v2/Mesh/SM_FF_Earth_UVSphere_R50cm_256x128')
 verts,tris,normals,uvs,tangents=unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh,0,0)
 samples=[]
 for p,uv in zip(verts,uvs):
  length=math.sqrt(p.x*p.x+p.y*p.y+p.z*p.z)
  if abs(p.z/length)>.98:continue
  samples.append((uv.x,uv.y,(math.atan2(-p.y,p.x)/(2*math.pi)+.5)%1,math.acos(max(-1,min(1,p.z/length)))/math.pi))
 def solve(axis):
  best=None
  for sign in (1,-1):
   for bias in (0,.5,1):
    errors=[]
    for s in samples[::31]:
     delta=sign*s[axis]+bias-s[axis+2]
     if axis==0:delta=(delta+.5)%1-.5
     errors.append(abs(delta))
    score=max(errors)
    if best is None or score<best[0]:best=(score,sign,bias)
  return best
 u=solve(0);v=solve(1)
 report['uv_fit']={'u':u,'v':v,'vertices':len(verts),'triangles':len(tris)//3}
 assert max(u[0],v[0])<.0001,report['uv_fit']
 mel=unreal.MaterialEditingLibrary;atl=unreal.AssetToolsHelpers.get_asset_tools()
 path=DEST+'/Materials/M_FF_Earth_CasualClay'
 mat=unreal.load_asset(path)
 if mat:raise RuntimeError('기존 새 머터리얼이 존재함. 덮어쓰기 전에 상태 확인 필요')
 mat=atl.create_asset('M_FF_Earth_CasualClay',DEST+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
 def node(cls,x,y,**props):
  obj=mel.create_material_expression(mat,cls,x,y)
  for k,val in props.items():obj.set_editor_property(k,val)
  return obj
 def wire(a,output,b,input):assert mel.connect_material_expressions(a,output,b,input)
 def dest(a,output,prop):assert mel.connect_material_property(a,output,prop)
 coord=node(unreal.MaterialExpressionTextureCoordinate,-1200,0)
 scale=node(unreal.MaterialExpressionConstant2Vector,-1200,150,r=float(u[1]),g=float(v[1]))
 mult=node(unreal.MaterialExpressionMultiply,-1000,0);wire(coord,'',mult,'A');wire(scale,'',mult,'B')
 offset=node(unreal.MaterialExpressionConstant2Vector,-1000,150,r=float(u[2]),g=float(v[2]))
 uv=node(unreal.MaterialExpressionAdd,-800,0);wire(mult,'',uv,'A');wire(offset,'',uv,'B')
 tex={}
 for index,(key,sampler) in enumerate([('Albedo',unreal.MaterialSamplerType.SAMPLERTYPE_COLOR),('Normal_DX',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL),('Masks',unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)]):
  t=node(unreal.MaterialExpressionTextureSampleParameter2D,-600,index*260,parameter_name=key,texture=unreal.load_asset(DEST+'/Textures/T_FF_Earth_CasualClay_'+key),sampler_type=sampler)
  wire(uv,'',t,'UVs');tex[key]=t
 dest(tex['Albedo'],'RGB',unreal.MaterialProperty.MP_BASE_COLOR)
 # UV 반전 시 기존 메시 탄젠트 기준으로 노멀 XY 방향을 변환한다.
 normal_sign=node(unreal.MaterialExpressionConstant3Vector,-350,350,constant=unreal.LinearColor(float(u[1]),float(v[1]),1,1))
 n=node(unreal.MaterialExpressionMultiply,-100,260);wire(tex['Normal_DX'],'RGB',n,'A');wire(normal_sign,'',n,'B');dest(n,'',unreal.MaterialProperty.MP_NORMAL)
 dest(tex['Masks'],'B',unreal.MaterialProperty.MP_ROUGHNESS);dest(tex['Masks'],'A',unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
 spec=node(unreal.MaterialExpressionScalarParameter,-100,100,parameter_name='SpecularAmount',default_value=.2);dest(spec,'',unreal.MaterialProperty.MP_SPECULAR)
 ht=node(unreal.MaterialExpressionTextureSampleParameter2D,-600,850,parameter_name='HeightMask',texture=unreal.load_asset(DEST+'/Textures/T_FF_Earth_CasualClay_Masks'),sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS,mip_value_mode=unreal.TextureMipValueMode.TMVM_MIP_LEVEL,const_mip_value=0)
 wire(uv,'',ht,'UVs')
 amount=node(unreal.MaterialExpressionScalarParameter,-350,1000,parameter_name='ReliefAmount',default_value=65.0)
 h=node(unreal.MaterialExpressionMultiply,-100,850);wire(ht,'R',h,'A');wire(amount,'',h,'B')
 vn=node(unreal.MaterialExpressionVertexNormalWS,-100,1100)
 wpo=node(unreal.MaterialExpressionMultiply,150,850);wire(h,'',wpo,'A');wire(vn,'',wpo,'B');dest(wpo,'',unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
 mel.recompile_material(mat);assert unreal.EditorAssetLibrary.save_loaded_asset(mat,False)
 mi=atl.create_asset('MI_FF_Earth_CasualClay',DEST+'/Materials',unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
 mel.set_material_instance_parent(mi,mat);mel.set_material_instance_scalar_parameter_value(mi,'ReliefAmount',65.0)
 assert unreal.EditorAssetLibrary.save_loaded_asset(mi,False)
 sub=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);sub.load_level('/Game/FestaFiesta/Maps/Frontend')
 globe=next(x for x in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if isinstance(x,unreal.FF_AtlasGlobeActor))
 report['previous']={k:str(globe.get_editor_property(k)) for k in ['globe_mesh_asset','globe_material','globe_texture_yaw_offset']}
 globe.set_editor_property('globe_mesh_asset',mesh);globe.set_editor_property('globe_material',mi);globe.set_editor_property('globe_texture_yaw_offset',0.0)
 globe.get_editor_property('globe_mesh').set_editor_property('bounds_scale',1.05)
 assert sub.save_current_level()
 report['material']=mi.get_path_name();report['status']='complete'
except Exception:
 report['status']='failed';report['error']=traceback.format_exc();unreal.log_error(report['error'])
(ROOT/'Saved/CasualClay_apply_report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
