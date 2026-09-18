"""지역 시험 바다 재질만 기존 구체의 +X UV 이음선에 맞춘다."""
from pathlib import Path
import sys
import unreal
root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
sys.path.insert(0, str(root/'Tools/ui/blender/bright_globe'))
from import_pilot import build_material, P03
build_material('M_BrightGlobe_water','water',
    unreal.load_asset(P03+'/MPC_BrightGlobe_Controls'),
    unreal.load_asset('/Game/FestaFiesta/UI/Atlas/Earth/DioramaWorld_v1/T_Diorama_Color'),
    unreal.load_asset('/Game/FestaFiesta/UI/Atlas/Earth/DioramaWorld_v1/T_Diorama_Height'),
    unreal.load_asset(P03+'/T_Bright_LandClass'))
unreal.log('BRIGHT_OCEAN_UV_OFFSET_APPLIED 0.5')
unreal.SystemLibrary.quit_editor()
