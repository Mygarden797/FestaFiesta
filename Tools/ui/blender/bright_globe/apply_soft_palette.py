"""사용자 피드백: 바다는 유지하고 육지 팔레트만 더 밝고 부드럽게."""
from pathlib import Path
import json
import unreal

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
DEST='/Game/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1/P03v2'
mel=unreal.MaterialEditingLibrary
atl=unreal.AssetToolsHelpers.get_asset_tools()
palette={'GrassColor':(.32,.64,.12),'ForestColor':(.10,.42,.11),'SandColor':(.96,.75,.37),'CoastColor':(.73,.52,.24)}
report={'status':'running','user_feedback':'육지색을 조금 더 밝고 부드럽게','palette_linear_rgb':palette,'water_changed':False,'instances':[]}
for kind,suffix,keys in [('top','Land',['GrassColor','ForestColor','SandColor']),('wall','Coast',['CoastColor'])]:
    name='MI_BrightGlobe_'+suffix+'_Soft_v002'
    path=DEST+'/'+name
    instance=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else atl.create_asset(name,DEST,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(instance,unreal.load_asset(DEST+'/M_BrightGlobe_'+kind+'_v002'))
    for key in keys:
        # UE 5.7 구현은 값을 설정해도 반환 bool이 false이므로 실제 값을 다시 읽어 검증한다.
        mel.set_material_instance_vector_parameter_value(instance,key,unreal.LinearColor(*palette[key],1))
        actual=mel.get_material_instance_vector_parameter_value(instance,key)
        assert max(abs(a-b) for a,b in zip((actual.r,actual.g,actual.b),palette[key]))<1.e-6, key
    mel.update_material_instance(instance)
    assert unreal.EditorAssetLibrary.save_loaded_asset(instance,False)
    for tile in (3,4,5):
        mesh=unreal.load_asset(DEST+'/SM_BrightP03v2_'+str(tile).zfill(2)+'_'+('Top' if kind=='top' else 'Coast'))
        mesh.set_material(0,instance)
        assert unreal.EditorAssetLibrary.save_loaded_asset(mesh,False)
    report['instances'].append(path)
report['status']='saved'
(ROOT/'Saved/BrightGlobe/p03_v002_soft_palette.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
