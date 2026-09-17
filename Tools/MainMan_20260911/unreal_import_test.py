import unreal,json,traceback,hashlib
from pathlib import Path
OUT=Path(__file__).resolve().parents[2]/'SourceArt/MainMan_20260911'
report={'engine':unreal.SystemLibrary.get_engine_version(),'isolated_project':True,'fbx_sha256':hashlib.sha256((OUT/'MainMan_Unreal.fbx').read_bytes()).hexdigest()}
try:
    options=unreal.FbxImportUI()
    for k,v in dict(automated_import_should_detect_type=False,import_mesh=True,import_as_skeletal=True,import_materials=True,import_textures=False,import_animations=False,create_physics_asset=False,mesh_type_to_import=unreal.FBXImportType.FBXIT_SKELETAL_MESH).items():options.set_editor_property(k,v)
    data=options.get_editor_property('skeletal_mesh_import_data')
    data.set_editor_property('normal_import_method',unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    data.set_editor_property('convert_scene',True)
    data.set_editor_property('convert_scene_unit',True)
    data.set_editor_property('force_front_x_axis',True)
    task=unreal.AssetImportTask();task.set_editor_property('filename',str(OUT/'MainMan_Unreal.fbx'));task.set_editor_property('destination_path','/Game/MainManQA');task.set_editor_property('destination_name','SK_MainMan');task.set_editor_property('automated',True);task.set_editor_property('replace_existing',True);task.set_editor_property('save',True);task.set_editor_property('options',options);task.set_editor_property('factory',unreal.FbxFactory())
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    paths=list(task.get_editor_property('imported_object_paths'));report['imported_paths']=paths
    meshes=[unreal.load_asset(p) for p in paths if isinstance(unreal.load_asset(p),unreal.SkeletalMesh)]
    report['skeletal_mesh_count']=len(meshes)
    if not meshes:raise RuntimeError('No skeletal mesh imported')
    mesh=meshes[0];skeleton=mesh.get_editor_property('skeleton');report['skeleton']=skeleton.get_path_name() if skeleton else None
    component=unreal.SkeletalMeshComponent();component.set_skeletal_mesh_asset(mesh)
    report['bone_count']=component.get_num_bones();report['bone_names']=[str(component.get_bone_name(i)) for i in range(component.get_num_bones())]
    report['material_slots']=len(mesh.get_editor_property('materials'))
    bounds=mesh.get_bounds();report['extent_cm']=[bounds.box_extent.x,bounds.box_extent.y,bounds.box_extent.z];report['height_cm']=bounds.box_extent.z*2
    report['all_finger_bones']=all(f'{f}_{i:02}_{s}' in report['bone_names'] for s in ['l','r'] for f in ['thumb','index','middle','ring','pinky'] for i in [1,2,3])
    report['pass']=len(meshes)==1 and bool(skeleton) and report['all_finger_bones'] and 169<report['height_cm']<171
except Exception:
    report['pass']=False;report['error']=traceback.format_exc()
(OUT/'qa/unreal_import_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8');unreal.log('MAINMAN_IMPORT_REPORT '+json.dumps(report))
