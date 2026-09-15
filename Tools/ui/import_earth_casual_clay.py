# -*- coding: utf-8 -*-
"""Casual Clay 4K 텍스처 임포트. 정상 Editor -ExecutePythonScript 전용."""
from pathlib import Path
import json, traceback
import unreal
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
SRC=ROOT/'SourceArt/UI/Title/CasualClay_Albedo_v1/GeographyLocked'
DEST='/Game/FestaFiesta/UI/Atlas/Earth/CasualClay_v1'
report={}
try:
    textures={}
    for key,rel,compression,srgb in [
        ('Albedo','T_FF_Earth_CasualClay_Aligned_4096x2048.png',unreal.TextureCompressionSettings.TC_DEFAULT,True),
        ('Height16','Relief/T_FF_Earth_CasualClay_Height16_4096x2048.png',unreal.TextureCompressionSettings.TC_GRAYSCALE,False),
        ('Normal_DX','Relief/T_FF_Earth_CasualClay_Normal_DX_4096x2048.png',unreal.TextureCompressionSettings.TC_NORMALMAP,False),
        ('Masks','Relief/T_FF_Earth_CasualClay_Masks_4096x2048.png',unreal.TextureCompressionSettings.TC_MASKS,False)]:
        task=unreal.AssetImportTask()
        for k,v in {'filename':str(SRC/rel),'destination_path':DEST+'/Textures','destination_name':'T_FF_Earth_CasualClay_'+key,'factory':unreal.TextureFactory(),'automated':True,'replace_existing':True,'save':False}.items():task.set_editor_property(k,v)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        tex=next(x for x in task.get_objects() if isinstance(x,unreal.Texture2D))
        for k,v in {'compression_settings':compression,'srgb':srgb,'lod_group':unreal.TextureGroup.TEXTUREGROUP_WORLD,'address_x':unreal.TextureAddress.TA_WRAP,'address_y':unreal.TextureAddress.TA_CLAMP}.items():tex.set_editor_property(k,v)
        if key=='Normal_DX':tex.set_editor_property('flip_green_channel',False)
        assert unreal.EditorAssetLibrary.save_loaded_asset(tex,False)
        assert (tex.blueprint_get_size_x(),tex.blueprint_get_size_y())==(4096,2048)
        textures[key]=tex.get_path_name()
    report['textures']=textures
    mesh=unreal.load_asset('/Game/FestaFiesta/UI/Atlas/Earth/CitrusPop_v2/Mesh/SM_FF_Earth_UVSphere_R50cm_256x128')
    assert mesh
    report['mesh']=mesh.get_path_name()
    report['mesh_methods']=[x for x in dir(mesh) if 'descr' in x or 'lod' in x]
    report['geometry_classes']=[x for x in dir(unreal) if 'MeshDescription' in x or 'ProceduralMeshLibrary' in x]
    sub=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    sub.load_level('/Game/FestaFiesta/Maps/Frontend')
    globe=next(x for x in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if isinstance(x,unreal.FF_AtlasGlobeActor))
    report['previous_globe']={k:str(globe.get_editor_property(k)) for k in ['globe_mesh_asset','globe_material','cloud_material','globe_texture_yaw_offset','globe_radius']}
    report['status']='complete'
except Exception:
    report['error']=traceback.format_exc();report['status']='failed';unreal.log_error(report['error'])
(ROOT/'Saved/CasualClay_import_report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
