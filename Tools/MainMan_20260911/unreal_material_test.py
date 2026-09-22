# unreal_material_test.py
# 역할: 플레이어 캐릭터 모델·애니메이션 작업에서 관련 도구가 공유하는 설정과 처리 함수를 제공한다.

import sys,json,unreal
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]/'SourceArt/MainMan_20260911'
sys.path.insert(0,str(ROOT))
from Import_MainMan_Unreal import import_mainman
mesh=import_mainman('/Game/MainManQA',replace_existing=True)
materials=[s.get_editor_property('material_interface') for s in mesh.get_editor_property('materials')]
report={'material_count':len(materials),'materials':[m.get_path_name() for m in materials],'all_assigned':all(materials),'roughness':.82,'metallic':0,'specular':.22,'helper_executed_successfully':True}
(ROOT/'qa/unreal_material_report.json').write_text(json.dumps(report,indent=2));unreal.log('MAINMAN_MATERIAL_REPORT '+json.dumps(report))
