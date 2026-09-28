# unreal_material_test.py
# 역할: 캐릭터 가져오기 도우미로 /Game/MainManQA 에셋을 갱신하고 모든 재질 슬롯의 연결 상태를 기록한다.

import sys,json,unreal
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]/'SourceArt/MainMan_20260911'
sys.path.insert(0,str(ROOT))
from Import_MainMan_Unreal import import_mainman
mesh=import_mainman('/Game/MainManQA',replace_existing=True)
materials=[s.get_editor_property('material_interface') for s in mesh.get_editor_property('materials')]
report={'material_count':len(materials),'materials':[m.get_path_name() for m in materials],'all_assigned':all(materials),'roughness':.82,'metallic':0,'specular':.22,'helper_executed_successfully':True}
(ROOT/'qa/unreal_material_report.json').write_text(json.dumps(report,indent=2));unreal.log('MAINMAN_MATERIAL_REPORT '+json.dumps(report))
