# -*- coding: utf-8 -*-
"""직접 제작한 국가 조형물과 준비 중 국가 7개를 에디터에서 등록한다."""
import json
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
SOURCE=ROOT/'SourceArt/UI/Atlas/Landmarks_v1'
DEST='/Game/FestaFiesta/UI/Atlas/Landmarks_v1'
# 위경도는 대표 조형물 근처의 배치용 근삿값이다. 축제 위치와 별도로 관리한다.
COUNTRIES=[
 ('KR','대한민국',37.579,126.977,145,190,90,'경복궁 경회루'),
 ('BR','브라질',-22.952,-43.211,260,250,90,'리우 예수상'),
 ('DE','독일',52.516,13.378,180,220,90,'브란덴부르크 문'),
 ('JP','일본',34.296,132.319,170,195,90,'도리이'),
 ('US','미국',40.689,-74.045,270,230,90,'자유의 여신상'),
 ('ES','스페인',41.404,2.175,195,200,90,'사그라다 파밀리아'),
 ('TH','태국',13.751,100.492,175,205,90,'태국식 사원'),
 ('AU','호주',-33.857,151.215,230,290,90,'시드니 오페라하우스'),
]
COLORS={'stone':(.87,.78,.57),'ivory':(.96,.92,.80),'roof':(.12,.34,.36),'red':(.78,.20,.12),'gold':(1.,.68,.18),'jade':(.22,.61,.47),'dark':(.19,.25,.25),'blue':(.35,.64,.71)}

def material(name,color):
    asset_path=DEST+'/Materials/M_FF_Landmark_'+name
    asset=unreal.load_asset(asset_path) if unreal.EditorAssetLibrary.does_asset_exist(asset_path) else None
    if asset:return asset
    asset=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_FF_Landmark_'+name,DEST+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    rgb=unreal.MaterialEditingLibrary.create_material_expression(asset,unreal.MaterialExpressionConstant3Vector,-300,0)
    # 팔레트는 sRGB 입력, 머터리얼 상수는 선형 색 공간이다.
    lin=[c/12.92 if c<=.04045 else ((c+.055)/1.055)**2.4 for c in color]
    rgb.set_editor_property('constant',unreal.LinearColor(*lin,1.0))
    rough=unreal.MaterialEditingLibrary.create_material_expression(asset,unreal.MaterialExpressionConstant,-300,150)
    rough.set_editor_property('r',.72)
    unreal.MaterialEditingLibrary.connect_material_property(rgb,'',unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.recompile_material(asset)
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset,False):raise RuntimeError('머터리얼 저장 실패')
    return asset

def mesh(code,materials):
    name='SM_FF_Landmark_'+code
    opts=unreal.FbxImportUI()
    opts.set_editor_property('automated_import_should_detect_type',False)
    opts.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_STATIC_MESH)
    opts.set_editor_property('import_mesh',True)
    opts.set_editor_property('import_materials',False)
    opts.set_editor_property('import_textures',False)
    data=opts.get_editor_property('static_mesh_import_data')
    data.set_editor_property('combine_meshes',True)
    data.set_editor_property('generate_lightmap_u_vs',True)
    data.set_editor_property('build_nanite',False)
    data.set_editor_property('auto_generate_collision',False)
    data.set_editor_property('normal_import_method',unreal.FBXNormalImportMethod.FBXNIM_COMPUTE_NORMALS)
    task=unreal.AssetImportTask()
    for key,value in {'filename':str(SOURCE/(name+'.obj')),'destination_path':DEST+'/Meshes','destination_name':name,'factory':unreal.FbxFactory(),'options':opts,'automated':True,'replace_existing':True,'save':False}.items():task.set_editor_property(key,value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    objects=[x for x in task.get_objects() if isinstance(x,unreal.StaticMesh)]
    if len(objects)!=1:raise RuntimeError(name+' 메시 임포트 실패')
    result=objects[0]
    for i,slot in enumerate(result.get_editor_property('static_materials')):
        label=str(slot.get_editor_property('imported_material_slot_name'))
        if label not in materials:raise RuntimeError('알 수 없는 머터리얼 슬롯: '+label)
        result.set_material(i,materials[label])
    if not unreal.EditorAssetLibrary.save_loaded_asset(result,False):raise RuntimeError('메시 저장 실패')
    return result

def run():
    materials={name:material(name,rgb) for name,rgb in COLORS.items()}
    meshes={code:mesh(code,materials) for code,*_ in COUNTRIES}
    catalog=unreal.load_asset('/Game/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas')
    entries=list(catalog.get_editor_property('countries'))
    reports=[]
    for code,name,lat,lon,height,width,heading,landmark in COUNTRIES:
        entry=next((x for x in entries if str(x.get_editor_property('country_id'))==code),None)
        if entry is None:
            entry=unreal.FF_AtlasCountryEntry()
            entry.set_editor_property('country_id',code)
            entry.set_editor_property('display_name',unreal.Text(name))
            entries.append(entry)
        location=entry.get_editor_property('location')
        location.set_editor_property('latitude',lat);location.set_editor_property('longitude',lon)
        entry.set_editor_property('location',location)
        entry.set_editor_property('landmark_mesh',meshes[code])
        for key,value in {'landmark_height_override':height,'landmark_max_width':width,'landmark_heading':heading,'landmark_surface_offset':0.0}.items():entry.set_editor_property(key,value)
        if code!='KR':
            entry.set_editor_property('unlocked',False)
            entry.set_editor_property('lock_reason',unreal.Text('준비 중'))
            entry.set_editor_property('description',unreal.Text(landmark+' · 새로운 축제를 준비하고 있습니다.'))
        reports.append({'country':code,'location':[lat,lon],'height_limit':height,'width_limit':width,'mesh':meshes[code].get_path_name(),'unlocked':entry.get_editor_property('unlocked')})
    catalog.set_editor_property('countries',entries)
    if not unreal.EditorAssetLibrary.save_loaded_asset(catalog,False):raise RuntimeError('카탈로그 저장 실패')
    (ROOT/'Saved/AtlasLandmarks_import_report.json').write_text(json.dumps(reports,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('[FF_LANDMARKS] COMPLETE: 8개 조형물, 한국 외 준비 중 국가 7개')

run()
