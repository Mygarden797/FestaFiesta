"""전 세계 디오라마 별도 에셋과 검토용 레벨 생성. 기본 높이 100/350 유지."""
from pathlib import Path
import sys,json,traceback
import unreal
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
sys.path.insert(0,str(ROOT/'Tools/ui'))
import import_diorama_pilot as helper
helper.SRC=ROOT/'SourceArt/UI/Atlas/DioramaWorld_v1'
helper.DEST='/Game/FestaFiesta/UI/Atlas/Earth/DioramaWorld_v1'
DEST=helper.DEST;MAP=DEST+'/L_Diorama_World'
report={}

def run():
    assert not unreal.EditorAssetLibrary.does_asset_exist(MAP),'완성된 레벨을 자동으로 덮어쓰지 않음'
    if unreal.EditorAssetLibrary.does_asset_exist(DEST+'/SM_Diorama_07_Bottom'):
        # 임포트가 끝난 뒤 레벨 생성에서 중단된 경우 이미 저장된 메시를 재사용한다.
        col=unreal.load_asset(DEST+'/MPC_Diorama_Controls')
        mats={k:unreal.load_asset(DEST+'/M_Diorama_'+k) for k in ['top','wall','bottom','water']}
        meshes=[(f'SM_Diorama_{i:02d}_{suffix}',unreal.load_asset(DEST+f'/SM_Diorama_{i:02d}_{suffix}')) for i in range(8) for suffix in ['Top','Coast','Bottom']]
        assert all(m for _,m in meshes)
        finish_level(meshes,mats,col);return
    col=helper.asset('MPC_Diorama_Controls',unreal.MaterialParameterCollection,unreal.MaterialParameterCollectionFactoryNew())
    params=[]
    for name,value in [('LandThickness',100.),('MountainHeight',350.)]:
        p=unreal.CollectionScalarParameter();p.set_editor_property('parameter_name',name);p.set_editor_property('default_value',value);params.append(p)
    col.set_editor_property('scalar_parameters',params);helper.save(col)
    color=helper.texture('T_Diorama_Color',True);ht=helper.texture('T_Diorama_Height',False)
    for t in (color,ht):t.set_editor_property('address_x',unreal.TextureAddress.TA_WRAP);helper.save(t)
    mel=unreal.MaterialEditingLibrary
    mats={k:helper.material('M_Diorama_'+k,k,col,color,ht) for k in ['top','wall','bottom','water']}
    # 고정 그림자를 색상에 굽지 않고 약한 환경색 보조로 뒷면의 검정을 줄인다.
    for kind,m in mats.items():
        base=mel.get_material_property_input_node(m,unreal.MaterialProperty.MP_BASE_COLOR)
        emit=mel.create_material_expression(m,unreal.MaterialExpressionMultiply,700,500)
        emit.set_editor_property('const_b',.16 if kind in ('top','wall') else .08)
        assert mel.connect_material_expressions(base,'RGB' if kind=='top' else '',emit,'A')
        assert mel.connect_material_property(emit,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        if kind=='wall':base.set_editor_property('constant',unreal.LinearColor(.85,.73,.43,1))
        if kind=='water':
            base.set_editor_property('constant',unreal.LinearColor(.035,.64,.72,1))
            rough=mel.get_material_property_input_node(m,unreal.MaterialProperty.MP_ROUGHNESS);rough.set_editor_property('r',.64)
        mel.layout_material_expressions(m);mel.recompile_material(m);helper.save(m)
    meshes=[]
    for i in range(8):
        for suffix,kind in [('Top','top'),('Coast','wall'),('Bottom','bottom')]:
            name=f'SM_Diorama_{i:02d}_{suffix}'
            meshes.append((name,helper.mesh(name,mats[kind])))
        unreal.log(f'디오라마 임포트 {i+1}/8 완료')
    finish_level(meshes,mats,col)

def finish_level(meshes,mats,col):
    # World 에셋의 직접 복제 대신 레벨 템플릿 API로 카메라·조명을 복사한다.
    level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert level.new_level_from_template(MAP,'/Game/FestaFiesta/UI/Atlas/Earth/DioramaPilot_v1/L_Diorama_EastAsia_Pilot')
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in actors.get_all_level_actors():
        label=a.get_actor_label()
        if label in ('Diorama_top','Diorama_wall','Diorama_bottom'):actors.destroy_actor(a)
        elif label=='Diorama_Ocean':a.static_mesh_component.set_material(0,mats['water'])
    for name,m in meshes:
        a=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(0,0,0));a.set_actor_label(name);a.set_folder_path('Diorama/Land')
        c=a.static_mesh_component;c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_static_mesh(m);c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);c.set_editor_property('bounds_scale',1.15)
        if name.endswith('Bottom'):c.set_cast_shadow(False)
    assert level.save_current_level()
    report.update(status='complete',map=MAP,mesh_count=len(meshes),controls=col.get_path_name())

try:run()
except Exception:report.update(status='failed',error=traceback.format_exc());unreal.log_error(report['error'])
(ROOT/'Saved/DioramaWorld_import.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
