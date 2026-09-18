"""극지 분류가 저장된 실제 UE 텍스처를 읽기 내보내고 재질 연결을 확인한다."""
from pathlib import Path
import json,unreal
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
mask=unreal.load_asset('/Game/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1/P03v2/T_Bright_LandClass')
task=unreal.AssetExportTask()
for key,value in dict(object=mask,filename=str(root/'Saved/BrightGlobe/ice_mask_export.png'),automated=True,prompt=False,replace_identical=True,exporter=unreal.TextureExporterPNG()).items():task.set_editor_property(key,value)
assert unreal.Exporter.run_asset_export_task(task)
print('ICE_MASK_INFO',mask.get_editor_property('srgb'),mask.get_editor_property('compression_settings'),mask.get_editor_property('asset_import_data').extract_filenames())
material=unreal.load_asset('/Game/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1/P05/M_BrightGlobe_water')
print('ICE_MATERIAL_PARAMETERS',unreal.MaterialEditingLibrary.get_vector_parameter_names(material))
