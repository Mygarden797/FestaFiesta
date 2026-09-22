# verify_project.py
# 역할: 플레이어 캐릭터 모델·애니메이션 작업에서 저장된 결과가 요구 조건에 맞는지 검증한다.

"""Fresh-process readback of the imported project asset; no asset writes."""
from pathlib import Path
import json
import unreal
script = Path(__file__).with_name('import_project.py')
source = script.read_text(encoding='utf-8').split('\ntry:\n', 1)[0]
scope = {'__file__': str(script), '__name__': 'mainman_readback'}
exec(compile(source, str(script), 'exec'), scope)
assert Path(unreal.Paths.project_dir()).resolve() == scope['ROOT']
report = scope['verify_saved']()
report['status'] = 'PASS'
(scope['OUT'] / 'saved_readback.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('MAINMAN_PROJECT_READBACK_PASS ' + json.dumps(report))
