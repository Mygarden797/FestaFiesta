# unreal_final_qa.py
# 역할: 플레이어 캐릭터 모델·애니메이션 작업에서 관련 도구가 공유하는 설정과 처리 함수를 제공한다.

from pathlib import Path
for script in ['unreal_import_test.py','unreal_material_test.py']:
    path=Path(__file__).with_name(script)
    exec(compile(path.read_text(encoding='utf-8'),str(path),'exec'),{'__file__':str(path),'__name__':'__main__'})
