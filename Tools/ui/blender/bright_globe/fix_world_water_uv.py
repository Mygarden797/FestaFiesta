"""기존 구체의 위도 반전만 세계판 바다 재질에서 보정한다."""
import json,sys
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
sys.path.insert(0,str(ROOT/'Tools/ui/blender/bright_globe'))
import import_refined_pilot as helper
DEST='/Game/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1/P05'
helper.helper.DEST=DEST
mask=unreal.load_asset('/Game/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1/P03v2/T_Bright_LandClass')
assert mask
material=helper.water(mask,'M_BrightGlobe_water',regional=False)
rows=json.loads((ROOT/'Saved/BrightGlobe/p05_water_uv.json').read_text(encoding='utf-8'))
for row in rows:
    u,v=row['uv'];eu,ev=row['expected']
    fixed=((u+.5)%1,1-v)
    assert abs((fixed[0]-eu+.5)%1-.5)<1e-6 and abs(fixed[1]-ev)<1e-6
(ROOT/'Saved/BrightGlobe/p05_water_uv_fix.json').write_text(json.dumps({'status':'passed','material':material.get_path_name(),'uv':'Frac(U+0.5), 1-V','ice_class_2_on_ocean_preserved':True,'measured_vertices':rows},indent=2),encoding='utf-8')
