# verify_project.py
# 역할: 플레이어 캐릭터 모델·애니메이션 작업에서 저장된 결과가 요구 조건에 맞는지 검증한다.

"""Fresh-process readback of the imported project asset; no asset writes."""
from pathlib import Path
import json
import runpy
import unreal


def main():
    """가져오기 코드를 자르거나 실행하지 않고 공개된 읽기 검증 함수를 재사용한다."""
    script = Path(__file__).with_name('import_project.py')
    scope = runpy.run_path(str(script), run_name='mainman_readback')
    report = scope['verify_saved']()
    report['status'] = 'PASS'
    scope['OUT'].mkdir(parents=True, exist_ok=True)
    (scope['OUT'] / 'saved_readback.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    unreal.log('MAINMAN_PROJECT_READBACK_PASS ' + json.dumps(report))
    return report


if __name__ == '__main__':
    main()
