"""별도 동아시아 시험 레벨과 육지/바다 에셋을 만든다. 기존 Frontend는 수정하지 않는다."""
from pathlib import Path
import json, math, traceback
import unreal

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
SRC=ROOT/'SourceArt/UI/Atlas/DioramaPilot_v1'
DEST='/Game/FestaFiesta/UI/Atlas/Earth/DioramaPilot_v1'
MAP=DEST+'/L_Diorama_EastAsia_Pilot'
mel=unreal.MaterialEditingLibrary; atl=unreal.AssetToolsHelpers.get_asset_tools()
report={}

def asset(name,cls,factory):
    path=DEST+'/'+name
    existing=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    return existing or atl.create_asset(name,DEST,cls,factory)

def save(a):assert unreal.EditorAssetLibrary.save_loaded_asset(a,False)

def texture(name,srgb):
    task=unreal.AssetImportTask()
    for k,v in dict(filename=str(SRC/(name+'.png')),destination_path=DEST,destination_name=name,factory=unreal.TextureFactory(),automated=True,replace_existing=True,save=False).items():task.set_editor_property(k,v)
    atl.import_asset_tasks([task]);t=task.get_objects()[0]
    t.set_editor_property('srgb',srgb)
    t.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_DEFAULT if srgb else unreal.TextureCompressionSettings.TC_GRAYSCALE)
    t.set_editor_property('address_x',unreal.TextureAddress.TA_CLAMP);t.set_editor_property('address_y',unreal.TextureAddress.TA_CLAMP)
    save(t);return t

def material(name,kind,collection,color,ht):
    m=asset(name,unreal.Material,unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(m)
    def n(cls,**props):
        o=mel.create_material_expression(m,cls,0,0)
        for k,v in props.items():o.set_editor_property(k,v)
        return o
    def w(a,o,b,i):assert mel.connect_material_expressions(a,o,b,i)
    def dest(a,o,p):assert mel.connect_material_property(a,o,p)
    def binary(cls,a,b):
        o=n(cls);w(a,'',o,'A');w(b,'',o,'B');return o
    rough=n(unreal.MaterialExpressionConstant,r=.82 if kind!='water' else .4)
    dest(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
    spec=n(unreal.MaterialExpressionConstant,r=.15);dest(spec,'',unreal.MaterialProperty.MP_SPECULAR)
    if kind=='top':
        c=n(unreal.MaterialExpressionTextureSample,texture=color,sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        dest(c,'RGB',unreal.MaterialProperty.MP_BASE_COLOR)
    else:
        rgb=(.62,.47,.20) if kind=='wall' else (.025,.56,.64) if kind=='water' else (.30,.26,.10)
        c=n(unreal.MaterialExpressionConstant3Vector,constant=unreal.LinearColor(*rgb,1))
        dest(c,'',unreal.MaterialProperty.MP_BASE_COLOR)
    if kind in ('top','wall'):
        uv=n(unreal.MaterialExpressionTextureCoordinate)
        if kind=='top':
            t=n(unreal.MaterialExpressionTextureSample,texture=ht,sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE,mip_value_mode=unreal.TextureMipValueMode.TMVM_MIP_LEVEL,const_mip_value=0)
            h=n(unreal.MaterialExpressionComponentMask,r=True,g=False,b=False,a=False);w(t,'R',h,'')
            weight=n(unreal.MaterialExpressionConstant,r=1.)
        else:
            h=n(unreal.MaterialExpressionComponentMask,r=True,g=False,b=False,a=False);w(uv,'',h,'')
            weight=n(unreal.MaterialExpressionComponentMask,r=False,g=True,b=False,a=False);w(uv,'',weight,'')
        base=n(unreal.MaterialExpressionCollectionParameter,collection=collection,parameter_name='LandThickness')
        mountain=n(unreal.MaterialExpressionCollectionParameter,collection=collection,parameter_name='MountainHeight')
        bd=binary(unreal.MaterialExpressionSubtract,base,n(unreal.MaterialExpressionConstant,r=100.))
        md=binary(unreal.MaterialExpressionSubtract,mountain,n(unreal.MaterialExpressionConstant,r=350.))
        delta=binary(unreal.MaterialExpressionMultiply,binary(unreal.MaterialExpressionAdd,bd,binary(unreal.MaterialExpressionMultiply,h,md)),weight)
        wp=n(unreal.MaterialExpressionWorldPosition,world_position_shader_offset=unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
        origin=n(unreal.MaterialExpressionActorPositionWS)
        radial=n(unreal.MaterialExpressionNormalize);w(binary(unreal.MaterialExpressionSubtract,wp,origin),'',radial,'VectorInput')
        dest(binary(unreal.MaterialExpressionMultiply,radial,delta),'',unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    mel.layout_material_expressions(m);mel.recompile_material(m);save(m);return m

def mesh(name,mat):
    opts=unreal.FbxImportUI()
    for k,v in dict(automated_import_should_detect_type=False,mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH,import_mesh=True,import_materials=False,import_textures=False).items():opts.set_editor_property(k,v)
    data=opts.get_editor_property('static_mesh_import_data')
    for k,v in dict(combine_meshes=True,generate_lightmap_u_vs=False,build_nanite=False,auto_generate_collision=False,normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_COMPUTE_NORMALS).items():data.set_editor_property(k,v)
    task=unreal.AssetImportTask()
    for k,v in dict(filename=str(SRC/(name+'.obj')),destination_path=DEST,destination_name=name,factory=unreal.FbxFactory(),options=opts,automated=True,replace_existing=True,save=False).items():task.set_editor_property(k,v)
    atl.import_asset_tasks([task]);m=next(x for x in task.get_objects() if isinstance(x,unreal.StaticMesh))
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    settings=sm.get_lod_build_settings(m,0);settings.set_editor_property('use_full_precision_u_vs',True)
    sm.set_lod_build_settings(m,0,settings)
    m.set_material(0,mat);save(m);return m

def run():
    assert not unreal.EditorAssetLibrary.does_asset_exist(MAP),'시험 레벨 존재: 수동 튜닝 보존을 위해 중단'
    col=asset('MPC_Diorama_Controls',unreal.MaterialParameterCollection,unreal.MaterialParameterCollectionFactoryNew())
    params=[]
    for name,value in [('LandThickness',100.),('MountainHeight',350.)]:
        p=unreal.CollectionScalarParameter();p.set_editor_property('parameter_name',name);p.set_editor_property('default_value',value);params.append(p)
    col.set_editor_property('scalar_parameters',params);save(col)
    color=texture('T_Diorama_Color',True);ht=texture('T_Diorama_Height',False)
    mats={k:material('M_Diorama_'+k,k,col,color,ht) for k in ['top','wall','bottom','water']}
    meshes={k:mesh('SM_Diorama_'+name,mats[k]) for k,name in [('top','Top'),('wall','Coast'),('bottom','Bottom')]}
    # 메시의 실제 임포트 좌표/UV를 검증하기 위한 표본.
    v,t,n,u,tan=unreal.ProceduralMeshLibrary.get_section_from_static_mesh(meshes['top'],0,0)
    report['imported_top']={'vertices':len(v),'triangles':len(t)//3,'sample':[{'p':[p.x,p.y,p.z],'uv':[uv.x,uv.y]} for p,uv in list(zip(v,u))[::max(1,len(v)//8)]]}
    level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert level.new_level(MAP)
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    def spawn(cls,label,loc=unreal.Vector(0,0,0),rot=unreal.Rotator(0,0,0)):
        a=actors.spawn_actor_from_class(cls,loc,rot);a.set_actor_label(label);return a
    def static(label,m,mat=None,scale=1):
        a=spawn(unreal.StaticMeshActor,label);c=a.static_mesh_component;c.set_mobility(unreal.ComponentMobility.MOVABLE)
        c.set_static_mesh(m)
        if mat:c.set_material(0,mat)
        c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);c.set_editor_property('bounds_scale',1.3)
        a.set_actor_scale3d(unreal.Vector(scale,scale,scale));return a
    for k,m in meshes.items():static('Diorama_'+k,m)
    sphere=unreal.load_asset('/Game/FestaFiesta/UI/Atlas/Earth/CitrusPop_v2/Mesh/SM_FF_Earth_UVSphere_R50cm_256x128')
    static('Diorama_Ocean',sphere,mats['water'],100)
    backdrop=static('Diorama_Backdrop',unreal.load_asset('/Engine/BasicShapes/Sphere'),unreal.load_asset('/Game/FestaFiesta/UI/Atlas/Earth/CasualClay_v1/Materials/MI_FF_ClayBackdrop'),2000)
    backdrop.static_mesh_component.set_cast_shadow(False)
    backdrop.static_mesh_component.set_editor_property('affect_distance_field_lighting',False)
    key=spawn(unreal.DirectionalLight,'Diorama_Key',rot=unreal.Rotator(-35,55,0))
    kc=key.get_component_by_class(unreal.DirectionalLightComponent);kc.set_mobility(unreal.ComponentMobility.MOVABLE);kc.set_intensity(6.);kc.set_editor_property('light_source_angle',8.)
    kc.set_editor_property('forward_shading_priority',1)
    fill=spawn(unreal.DirectionalLight,'Diorama_Fill',rot=unreal.Rotator(-15,145,0))
    fc=fill.get_component_by_class(unreal.DirectionalLightComponent);fc.set_mobility(unreal.ComponentMobility.MOVABLE);fc.set_intensity(1.2);fc.set_editor_property('cast_shadows',False)
    sky=spawn(unreal.SkyLight,'Diorama_Sky');sc=sky.get_component_by_class(unreal.SkyLightComponent);sc.set_mobility(unreal.ComponentMobility.MOVABLE);sc.set_intensity(.15);sc.set_editor_property('sky_distance_threshold',20000.)
    pp=spawn(unreal.PostProcessVolume,'Diorama_Exposure');pp.set_editor_property('unbound',True);s=pp.get_editor_property('settings')
    for k,value in dict(override_auto_exposure_method=True,auto_exposure_method=unreal.AutoExposureMethod.AEM_BASIC,override_auto_exposure_min_brightness=True,override_auto_exposure_max_brightness=True,auto_exposure_min_brightness=1.,auto_exposure_max_brightness=1.,override_auto_exposure_bias=True,auto_exposure_bias=1.).items():s.set_editor_property(k,value)
    pp.set_editor_property('settings',s)
    lon=math.radians(110);lat=math.radians(27)
    loc=unreal.Vector(22000*math.cos(lat)*math.cos(lon),-22000*math.cos(lat)*math.sin(lon),22000*math.sin(lat))
    rot=unreal.MathLibrary.find_look_at_rotation(loc,unreal.Vector(0,0,0))
    camera=spawn(unreal.CameraActor,'CAM_Diorama_Pilot',loc,rot)
    cc=camera.get_component_by_class(unreal.CameraComponent);cc.set_editor_property('constrain_aspect_ratio',False);cc.set_editor_property('field_of_view',60.)
    cc.set_editor_property('override_aspect_ratio_axis_constraint',True)
    cc.set_editor_property('aspect_ratio_axis_constraint',unreal.AspectRatioAxisConstraint.ASPECT_RATIO_MAINTAIN_YFOV)
    camera.set_editor_property('auto_activate_for_player',unreal.AutoReceiveInput.PLAYER0)
    unreal.EditorLevelLibrary.set_level_viewport_camera_info(loc,rot)
    # 시험 레벨에서 기존 프런트엔드 UI/플로를 시작하지 않는다.
    world=unreal.EditorLevelLibrary.get_editor_world();world.get_world_settings().set_editor_property('default_game_mode',unreal.GameModeBase.static_class())
    sc.recapture_sky();assert level.save_current_level()
    report.update(status='complete',map=MAP,controls=col.get_path_name())

if __name__=='__main__':
    try:run()
    except Exception:
        report.update(status='failed',error=traceback.format_exc());unreal.log_error(report['error'])
    (ROOT/'Saved/DioramaPilot_import.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
