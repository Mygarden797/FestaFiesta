# unreal_final_qa.py
# 역할: Unreal의 /Game/MainManQA 경로에 캐릭터를 가져오는 검사와 재질 연결 검사를 차례대로 실행한다.

from pathlib import Path
for script in ['unreal_import_test.py','unreal_material_test.py']:
    path=Path(__file__).with_name(script)
    exec(compile(path.read_text(encoding='utf-8'),str(path),'exec'),{'__file__':str(path),'__name__':'__main__'})
