"""P03 v002 형상/재질을 별도 에셋과 레벨로 저장한다. v001과 Frontend 보존."""
from pathlib import Path
import json
import sys
import traceback
import unreal

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
sys.path.insert(0,str(ROOT/'Tools/ui/blender/bright_globe'))
import import_pilot as prior
helper=prior.helper
mel=unreal.MaterialEditingLibrary
DEST='/Game/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1/P03v2'
MAP='/Game/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1/L_BrightGlobe_Pilot_v002'
ART=ROOT/'SourceArt/UI/Atlas/BrightGlobe_v1/P03v2/Exports'
OUT=ROOT/'Saved/BrightGlobe'
report={'status':'running','frontend_modified':False}


def texture(name,path,data=False):
    target=DEST+'/'+name
    if unreal.EditorAssetLibrary.does_asset_exist(target):
        tex=unreal.load_asset(target)
    else:
        task=unreal.AssetImportTask()
        for key,value in dict(filename=str(path),destination_path=DEST,destination_name=name,
            automated=True,replace_existing=False,save=False).items():task.set_editor_property(key,value)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        tex=next(x for x in task.get_objects() if isinstance(x,unreal.Texture2D))
    tex.set_editor_property('srgb',not data)
    tex.set_editor_property('never_stream',True)
    tex.set_editor_property('address_x',unreal.TextureAddress.TA_WRAP)
    tex.set_editor_property('address_y',unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_GRAYSCALE if data else unreal.TextureCompressionSettings.TC_DEFAULT)
    if data:
        # 원값 0/1/2인 8bit 분류 데이터. 밉 생성 시 반올림으로 해안이 뭉개지는 것을 피한다.
        tex.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    helper.save(tex)
    return tex


def water(mask,name='M_BrightGlobe_water_v002',regional=True):
    m=helper.asset(name,unreal.Material,unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(m)
    def n(cls,**props):
        obj=mel.create_material_expression(m,cls,0,0)
        for k,v in props.items():obj.set_editor_property(k,v)
        return obj
    def c(v):return n(unreal.MaterialExpressionConstant,r=v)
    def w(a,b,pin):assert mel.connect_material_expressions(a,'',b,pin)
    def op(cls,a,b):
        x=n(cls);w(a,x,'A');w(b,x,'B');return x
    def add(a,b):return op(unreal.MaterialExpressionAdd,a,b)
    def mul(a,b):return op(unreal.MaterialExpressionMultiply,a,b)
    def sub(a,b):return op(unreal.MaterialExpressionSubtract,a,b)
    def sat(a):
        x=n(unreal.MaterialExpressionSaturate);w(a,x,'');return x
    def ch(a,i):
        x=n(unreal.MaterialExpressionComponentMask,r=i==0,g=i==1,b=False,a=False);w(a,x,'');return x
    uv=n(unreal.MaterialExpressionTextureCoordinate)
    # 실제 기존 구체는 OBJ 임포트 후 북극 V=1이다. 육지와 같은 북극 V=0으로 보정한다.
    # U만 경도 이음선에서 반복하고 V는 극점에서 clamp하도록 Frac를 적용하지 않는다.
    u=n(unreal.MaterialExpressionFrac);w(add(ch(uv,0),c(.5)),u,'')
    v=sub(c(1.),ch(uv,1))
    wrap=op(unreal.MaterialExpressionAppendVector,u,v)
    # 정규화 후 공간 평균: 작은 원값의 밉 반올림 대신 원본 픽셀을 직접 보간한다.
    taps=[]
    ice=None
    for x,y in [(0,0),(-2,0),(2,0),(0,-2),(0,2),(-2,-2),(2,-2),(-2,2),(2,2)]:
        coords=add(wrap,n(unreal.MaterialExpressionConstant2Vector,r=x/2048,g=y/1024))
        t=n(unreal.MaterialExpressionTextureSample,texture=mask,
            sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE,
            mip_value_mode=unreal.TextureMipValueMode.TMVM_MIP_LEVEL,const_mip_value=0.)
        w(coords,t,'UVs')
        classification=mul(ch(t,0),c(255.))
        taps.append(sat(classification))
        if x==0 and y==0:
            # 0=바다, 1=육지, 2=얼음. 바다 위 북극 얼음도 원본 범위를 유지한다.
            ice=sat(sub(classification,c(1.)))
    total=taps[0]
    for sample in taps[1:]:total=add(total,sample)
    coast=mul(total,c(1/9))
    # 시험 범위 밖 지형을 시안 덩어리로 그리지 않는다. 전 세계판에서는 이 제한 제거.
    if regional:
        u,v=ch(wrap,0),ch(wrap,1)
        for expr in [sub(u,c(160/360)),sub(c(235/360),u),sub(v,c(25/180)),sub(c(70/180),v)]:
            coast=mul(coast,sat(mul(expr,c(2048.))))
    deep=n(unreal.MaterialExpressionVectorParameter,parameter_name='OceanColor',default_value=unreal.LinearColor(.008,.24,.68,1))
    shallow=n(unreal.MaterialExpressionVectorParameter,parameter_name='ShallowColor',default_value=unreal.LinearColor(.04,.62,.78,1))
    color=n(unreal.MaterialExpressionLinearInterpolate);w(deep,color,'A');w(shallow,color,'B');w(coast,color,'Alpha')
    frozen=n(unreal.MaterialExpressionVectorParameter,parameter_name='PolarIceColor',default_value=unreal.LinearColor(.91,.95,1.,1))
    surface=n(unreal.MaterialExpressionLinearInterpolate);w(color,surface,'A');w(frozen,surface,'B');w(ice,surface,'Alpha')
    roughness=add(c(.5),mul(ice,c(.28)))
    for expr,prop in [(surface,unreal.MaterialProperty.MP_BASE_COLOR),(roughness,unreal.MaterialProperty.MP_ROUGHNESS),
        (c(.16),unreal.MaterialProperty.MP_SPECULAR),(mul(surface,c(.035)),unreal.MaterialProperty.MP_EMISSIVE_COLOR)]:
        assert mel.connect_material_property(expr,'',prop)
    mel.layout_material_expressions(m);mel.recompile_material(m);helper.save(m)
    return m


def run():
    assert not unreal.EditorAssetLibrary.does_asset_exist(MAP),'v002 저장 레벨을 임의로 덮지 않는다'
    assert len(list(ART.glob('*.obj')))==9
    prior.P03=DEST;helper.DEST=DEST
    col=helper.asset('MPC_BrightGlobe_Controls_v002',unreal.MaterialParameterCollection,unreal.MaterialParameterCollectionFactoryNew())
    params=[]
    for name,value in [('LandThickness',40.),('MountainHeight',650.)]:
        p=unreal.CollectionScalarParameter();p.set_editor_property('parameter_name',name);p.set_editor_property('default_value',value);params.append(p)
    col.set_editor_property('scalar_parameters',params);helper.save(col)
    source=ROOT/'SourceArt/UI/Title/CasualClay_Albedo_v1/GeographyLocked'
    color=texture('T_Bright_BiomeSource',source/'T_FF_Earth_CasualClay_Aligned_2048x1024.png')
    mask=texture('T_Bright_LandClass',source/'Geography_Classes_2048x1024.png',True)
    height=unreal.load_asset('/Game/FestaFiesta/UI/Atlas/Earth/DioramaWorld_v1/T_Diorama_Height')
    mats={k:prior.build_material('M_BrightGlobe_'+k+'_v002',k,col,color,height,mask) for k in ('top','wall','bottom')}
    mats['water']=water(mask)
    helper.DEST=DEST;helper.SRC=ART
    meshes=[];counts=[]
    for path in sorted(ART.glob('*.obj')):
        kind={'Top':'top','Coast':'wall','Bottom':'bottom'}[path.stem.rsplit('_',1)[1]]
        target=DEST+'/'+path.stem
        mesh=unreal.load_asset(target) if unreal.EditorAssetLibrary.does_asset_exist(target) else helper.mesh(path.stem,mats[kind])
        if kind!='bottom':meshes.append(mesh)
        verts,tris,normals,uvs,tangents=unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh,0,0)
        assert len(verts)==len(uvs) and len(tris)>0
        counts.append({'asset':target,'vertices':len(verts),'triangles':len(tris)//3})
    level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert level.new_level_from_template(MAP,'/Game/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1/L_BrightGlobe_Pilot')
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    globe=next(a for a in actors.get_all_level_actors() if isinstance(a,unreal.FF_AtlasGlobeActor))
    globe.set_editor_property('land_layer_meshes',meshes)
    globe.set_editor_property('globe_material',mats['water'])
    globe.set_editor_property('start_facing_first_country',False)
    globe.set_editor_property('start_lat_long',unreal.FF_AtlasLatLong(latitude=42.,longitude=12.))
    for index,component in enumerate(globe.get_editor_property('land_layer_components')):
        component.set_static_mesh(meshes[index] if index<len(meshes) else None)
    globe.get_editor_property('globe_mesh').set_material(0,mats['water'])
    assert level.save_current_level()
    report.update(status='saved',level=MAP,controls={'LandThickness':40,'MountainHeight':650},meshes=counts,
        color_source=str(source/'T_FF_Earth_CasualClay_Aligned_2048x1024.png'),water_mask_mips='none, 9 normalized taps',render_verified=False)

if __name__=='__main__':
    try:run()
    except Exception:
        report.update(status='failed',error=traceback.format_exc());unreal.log_error('BRIGHT_GLOBE_FAIL '+report['error'])
    (OUT/'p03_v002_ue_import.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
# 비동기 작업이 없는 임포트는 Python executor의 정상 종료 경로를 따른다.
