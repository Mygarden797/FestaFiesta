"""Create the requested green UI materials, with real UV-driven mesh and thin-film light.

No source images, game-world materials, or global palette are recolored.
"""
import unreal

FOLDER = "/Game/FestaFiesta/UI/Facility/Aurora"
NAME = "M_UI_FacilityAurora"
unreal.EditorAssetLibrary.make_directory(FOLDER)
tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.MaterialEditingLibrary
material = unreal.EditorAssetLibrary.load_asset(FOLDER + "/" + NAME) if unreal.EditorAssetLibrary.does_asset_exist(FOLDER + "/" + NAME) else None
if not material:
    material = tools.create_asset(NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    uv = lib.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -650, 0)
    mode = lib.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -650, 180)
    mode.set_editor_property("parameter_name", "SurfaceRole")
    mode.set_editor_property("default_value", 0.0)
    aspect = lib.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -650, 320)
    aspect.set_editor_property("parameter_name", "Aspect")
    aspect.set_editor_property("default_value", 4.0)
    phase = lib.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -650, 440)
    phase.set_editor_property("parameter_name", "FilmPhase")
    phase.set_editor_property("default_value", 0.0)
    custom = lib.create_material_expression(material, unreal.MaterialExpressionCustom, -280, 0)
    custom.set_editor_property("description", "V7 Emerald / mint / leaf thin-film and aurora mesh")
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    inputs = []
    for name in ("UV", "Role", "Aspect", "Phase"):
        pin = unreal.CustomInput()
        pin.set_editor_property("input_name", name)
        inputs.append(pin)
    custom.set_editor_property("inputs", inputs)
    custom.set_editor_property("code", r'''
float2 p = saturate(UV);
float a = exp(-dot((p-float2(0.02,0.02))*float2(1.6,1.0),(p-float2(0.02,0.02))*float2(1.6,1.0))*2.6);
float b = exp(-dot((p-float2(0.95,0.03))*float2(1.5,1.0),(p-float2(0.95,0.03))*float2(1.5,1.0))*2.8);
float c = exp(-dot(p-float2(0.68,1.15),p-float2(0.68,1.15))*3.1);
float film = 0.5+0.5*sin((p.x*0.8+p.y*0.48)*7.4+Phase);
float3 mint = float3(0.65,0.90,0.73);
float3 leaf = float3(0.84,0.94,0.65);
float3 emerald = float3(0.35,0.73,0.58);
float3 paper = float3(0.953,0.973,0.933);
float3 color = paper;
float x = p.x+p.y*0.11;
float sheen = exp(-pow((x-0.25)*68,2))*0.33 + exp(-pow((x-0.70)*80,2))*0.20;
if (Role > 0.5 && Role < 2.5) {
  color = lerp(paper,mint,a*0.54);
  color = lerp(color,leaf,b*0.51);
  color = lerp(color,emerald,c*(Role>1.5 ? 0.29 : 0.10));
  color = lerp(color,lerp(mint,leaf,film),Role>1.5 ? 0.38 : 0.12);
  color = lerp(color,float3(0.985,1,0.94),sheen);
}
if(Role > 2.5 && Role < 4.5) {
  color = lerp(float3(0.13,0.33,0.23),float3(0.23,0.47,0.33),film*0.72+a*0.12);
  color += sheen*float3(0.11,0.16,0.07);
  if(Role>3.5) color *= 0.78;
}
if(Role>4.5) {
  color = lerp(float3(0.93,0.83,0.56),float3(0.99,0.95,0.78),a*0.70+film*0.24);
  color = lerp(color,float3(1,0.98,0.87),sheen);
}
float ratio=max(Aspect,0.2);
float radius=0.045;
float2 q=abs((p-0.5)*float2(ratio,1.0))-(float2(ratio,1.0)*0.5-radius);
float distance=length(max(q,0))+min(max(q.x,q.y),0)-radius;
float aa=max(fwidth(distance),0.001);
float alpha=1-smoothstep(-aa,aa,distance);
float rim=1-smoothstep(0.001,0.007,-distance);
float3 edge=lerp(emerald,leaf,film);
color=lerp(color,edge,rim*0.58);
color=lerp(color,float3(1,1,0.96),exp(-pow((p.y-0.013)*130,2))*0.35);
return float4(pow(saturate(color),2.2),alpha);
''')
    for expr, target in ((uv, "UV"), (mode, "Role"), (aspect, "Aspect"), (phase, "Phase")):
        assert lib.connect_material_expressions(expr, "", custom, target), target
    rgb = lib.create_material_expression(material, unreal.MaterialExpressionComponentMask, 0, 0)
    for key in ("r", "g", "b"):
        rgb.set_editor_property(key, True)
    rgb.set_editor_property("a", False)
    alpha = lib.create_material_expression(material, unreal.MaterialExpressionComponentMask, 0, 150)
    for key in ("r", "g", "b"):
        alpha.set_editor_property(key, False)
    alpha.set_editor_property("a", True)
    assert lib.connect_material_expressions(custom, "", rgb, "")
    assert lib.connect_material_expressions(custom, "", alpha, "")
    assert lib.connect_material_property(rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert lib.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)

for name, role, aspect in (("Paper",0,1), ("Header",1,8), ("Accent",2,3), ("Action",3,6), ("Dark",4,4), ("Gold",5,4)):
    asset_name = "MI_UI_FacilityAurora_" + name
    instance = unreal.EditorAssetLibrary.load_asset(FOLDER + "/" + asset_name) if unreal.EditorAssetLibrary.does_asset_exist(FOLDER + "/" + asset_name) else None
    if not instance:
        instance = tools.create_asset(asset_name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        lib.set_material_instance_parent(instance, material)
        lib.set_material_instance_scalar_parameter_value(instance, "SurfaceRole", float(role))
        lib.set_material_instance_scalar_parameter_value(instance, "Aspect", float(aspect))
        unreal.EditorAssetLibrary.save_loaded_asset(instance, only_if_is_dirty=False)
unreal.log("FF_FACILITY_AURORA_MATERIALS_PASS")
