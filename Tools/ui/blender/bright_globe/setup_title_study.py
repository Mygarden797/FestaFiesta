"""P4: 별도 타이틀 시험 레벨에 화면 기준 크림→하늘 배경을 저장한다."""
from pathlib import Path
import json
import unreal

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
DEST='/Game/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1'
MAP=DEST+'/L_BrightGlobe_TitleStudy'
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
mel=unreal.MaterialEditingLibrary
assert not unreal.EditorAssetLibrary.does_asset_exist(MAP),'기존 시험 레벨을 덮어쓰지 않는다'
assert level.new_level_from_template(MAP,DEST+'/L_BrightGlobe_Pilot_v002')
name='M_BrightGlobe_Backdrop'
assert not unreal.EditorAssetLibrary.does_asset_exist(DEST+'/'+name)
mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,DEST,unreal.Material,unreal.MaterialFactoryNew())
mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property('two_sided',True)
def node(cls,**props):
    obj=mel.create_material_expression(mat,cls,0,0)
    for k,v in props.items():obj.set_editor_property(k,v)
    return obj
def wire(a,b,pin,output=''):assert mel.connect_material_expressions(a,output,b,pin)
def constant(v):return node(unreal.MaterialExpressionConstant,r=v)
def op(cls,a,b):
    n=node(cls);wire(a,n,'A');wire(b,n,'B');return n
def mul(a,b):return op(unreal.MaterialExpressionMultiply,a,b)
def sub(a,b):return op(unreal.MaterialExpressionSubtract,a,b)
screen=node(unreal.MaterialExpressionScreenPosition)
x=node(unreal.MaterialExpressionComponentMask,r=True,g=False,b=False,a=False)
wire(screen,x,'','ViewportUV')
t=node(unreal.MaterialExpressionSaturate)
wire(mul(sub(x,constant(.25)),constant(1/.65)),t,'')
smooth=mul(mul(t,t),sub(constant(3),mul(constant(2),t)))
cream=node(unreal.MaterialExpressionVectorParameter,parameter_name='CreamColor',default_value=unreal.LinearColor(1.,.955,.84,1))
sky=node(unreal.MaterialExpressionVectorParameter,parameter_name='SkyColor',default_value=unreal.LinearColor(.32,.65,.94,1))
blend=node(unreal.MaterialExpressionLinearInterpolate)
wire(cream,blend,'A');wire(sky,blend,'B');wire(smooth,blend,'Alpha')
brightness=node(unreal.MaterialExpressionScalarParameter,parameter_name='BackgroundBrightness',default_value=2.4)
assert mel.connect_material_property(mul(blend,brightness),'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.layout_material_expressions(mat);mel.recompile_material(mat)
assert unreal.EditorAssetLibrary.save_loaded_asset(mat,False)
dome=next(a for a in actors.get_all_level_actors() if a.get_actor_label()=='FF_Clay_Backdrop')
dome.static_mesh_component.set_material(0,mat)
# 배경은 발광만 사용한다. 저장된 SkyLight 환경은 화면 그라데이션으로 다시 캡처하지 않는다.
assert level.save_current_level()
(ROOT/'Saved/BrightGlobe/p04_title_study.json').write_text(json.dumps({'status':'saved','level':MAP,'background':mat.get_path_name(),'gradient':'viewport horizontal smoothstep 0.25..0.90','lights_changed':False,'frontend_modified':False},ensure_ascii=False,indent=2),encoding='utf-8')
