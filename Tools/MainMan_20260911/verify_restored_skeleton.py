# verify_restored_skeleton.py
# 역할: 플레이어 캐릭터 모델·애니메이션 작업에서 저장된 결과가 요구 조건에 맞는지 검증한다.

"""Read-only verification in a fresh Unreal Editor process."""
import json
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from restore_skeleton import verify, OUT, ROOT
import unreal
assert Path(unreal.Paths.project_dir()).resolve() == ROOT
report = verify()
report['status'] = 'PASS'
(OUT / 'fresh_readback.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('MAINMAN_RESTORED_SKELETON_READBACK_PASS ' + json.dumps(report))
