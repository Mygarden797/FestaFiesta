"""P05 검증된 전 세계 Top/Coast를 새 UE 폴더와 별도 레벨로 연결한다."""
from pathlib import Path
import hashlib,json,sys,traceback
import unreal
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
sys.path.insert(0,str(ROOT/'Tools/ui/blender/bright_globe'))
import import_pilot as prior
import import_refined_pilot as refined
helper=prior.helper
OUT=ROOT/'Saved/BrightGlobe'
ART=ROOT/'SourceArt/UI/Atlas/BrightGlobe_v1/P05/Exports'
BASE='/Game/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1'
DEST=BASE+'/P05'
MAP=BASE+'/L_BrightGlobe_World'
MARKER=OUT/'p05_ue_import.json'
mel=unreal.MaterialEditingLibrary
report=json.loads(MARKER.read_text(encoding='utf-8')) if MARKER.exists() else {'status':'running','meshes':[],'frontend_modified':False}
def record():MARKER.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
def run():
    assert report['status']!='passed','이미 완료된 세계판을 다시 덮어쓰지 않는다'
    assert json.loads((OUT/'p05_validation.json').read_text(encoding='utf-8'))['status']=='passed'
    exports=json.loads((OUT/'P05v001/exports.json').read_text(encoding='utf-8'))
    helper.DEST=DEST;helper.SRC=ART;prior.P03=DEST;refined.DEST=DEST
    col=helper.asset('MPC_BrightGlobe_Controls',unreal.MaterialParameterCollection,unreal.MaterialParameterCollectionFactoryNew())
    params=[]
    for name,value in [('LandThickness',40.),('MountainHeight',650.)]:
        p=unreal.CollectionScalarParameter();p.set_editor_property('parameter_name',name);p.set_editor_property('default_value',value);params.append(p)
    col.set_editor_property('scalar_parameters',params);helper.save(col)
    color=unreal.load_asset(BASE+'/P03v2/T_Bright_BiomeSource')
    mask=unreal.load_asset(BASE+'/P03v2/T_Bright_LandClass')
    height=unreal.load_asset('/Game/FestaFiesta/UI/Atlas/Earth/DioramaWorld_v1/T_Diorama_Height')
    assert all((color,mask,height))
    mats={kind:prior.build_material('M_BrightGlobe_'+kind,kind,col,color,height,mask) for kind in ('top','wall')}
    water=refined.water(mask,'M_BrightGlobe_water',regional=False)
    for kind,label,palette in [('top','Land',{'GrassColor':(.32,.64,.12),'ForestColor':(.10,.42,.11),'SandColor':(.96,.75,.37)}),('wall','Coast',{'CoastColor':(.73,.52,.24)})]:
        mi=helper.asset('MI_BrightGlobe_'+label,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi,mats[kind])
        for key,value in palette.items():
            mel.set_material_instance_vector_parameter_value(mi,key,unreal.LinearColor(*value,1))
            a=mel.get_material_instance_vector_parameter_value(mi,key)
            assert max(abs(x-y) for x,y in zip((a.r,a.g,a.b),value))<1.e-6
        helper.save(mi);mats[kind]=mi
    meshes=[]
    for tile in range(8):
        for suffix,kind in [('Top','top'),('Coast','wall')]:
            name=f'SM_BrightWorld_{tile:02d}_{suffix}';path=ART/(name+'.obj')
            sha=hashlib.sha256(path.read_bytes()).hexdigest()
            assert sha==exports[name]['sha256']
            previous=next((x for x in report['meshes'] if x['name']==name),None)
            if previous:
                assert previous['source_sha256']==sha
                mesh=unreal.load_asset(DEST+'/'+name);assert mesh
            else:
                assert not unreal.EditorAssetLibrary.does_asset_exist(DEST+'/'+name),'미확인 부분 임포트 확인 필요'
                mesh=helper.mesh(name,mats[kind])
                v,t,n,u,tan=unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh,0,0)
                assert len(t)//3==exports[name]['triangles'],(name,len(t)//3,exports[name]['triangles'])
                assert len(v)==len(u) and len(mesh.get_editor_property('static_materials'))==1
                report['meshes'].append({'name':name,'triangles':len(t)//3,'source_sha256':sha,'asset':mesh.get_path_name()});record()
                del v,t,n,u,tan
            meshes.append(mesh)
    level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert not unreal.EditorAssetLibrary.does_asset_exist(MAP),'저장된 세계 시험 레벨 확인 필요'
    assert level.new_level_from_template(MAP,BASE+'/L_BrightGlobe_TitleStudy')
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    globe=next(a for a in actors.get_all_level_actors() if isinstance(a,unreal.FF_AtlasGlobeActor))
    globe.set_editor_property('land_layer_meshes',meshes);globe.set_editor_property('globe_material',water)
    for component,mesh in zip(globe.get_editor_property('land_layer_components'),meshes):component.set_static_mesh(mesh)
    globe.get_editor_property('globe_mesh').set_material(0,water)
    assert level.save_current_level()
    report.update(status='passed',level=MAP,collection=col.get_path_name(),land_thickness=40,mountain_height=650,render_verified=False)
    catalog=unreal.load_asset('/Game/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas')
    countries={}
    for row in catalog.get_editor_property('countries'):
        loc=row.get_editor_property('location')
        countries[str(row.get_editor_property('country_id'))]={'latitude':loc.get_editor_property('latitude'),'longitude':loc.get_editor_property('longitude'),'old_offset':row.get_editor_property('landmark_surface_offset')}
    (OUT/'p05_catalog_locations.json').write_text(json.dumps(countries,ensure_ascii=False,indent=2),encoding='utf-8')
try:run()
except Exception:
    report.update(status='failed',error=traceback.format_exc());unreal.log_error('BRIGHT_GLOBE_FAIL '+report['error'])
record()
