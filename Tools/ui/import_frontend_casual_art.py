"""자체 제작 프런트엔드 아트만 임포트한다. 지구 텍스처는 접근하지 않는다."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
tasks=[]
for file in sorted((root/'SourceArt/UI/FrontendCasual').glob('*.png')):
    task=unreal.AssetImportTask()
    task.filename=str(file);task.destination_path='/Game/FestaFiesta/UI/Frontend/Art'
    task.automated=True;task.replace_existing=True;task.save=False;tasks.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for task in tasks:
    for path in task.imported_object_paths:
        tex=unreal.load_asset(path)
        tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        tex.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
        tex.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property('srgb',True)
        unreal.EditorAssetLibrary.save_loaded_asset(tex)
unreal.log('FF_CASUAL_ART_DONE '+str(len(tasks)))
