"""공감 행과 두 사건의 확정 보상을 기존 에셋에 추가한다."""
import json
import runpy
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUTPUT = ROOT / 'Saved/NewsRewards'
OUTPUT.mkdir(parents=True, exist_ok=True)
report = {'status': 'running'}
try:
    for name in ('apply_sns_reaction.py', 'apply_incident_rewards.py'):
        runpy.run_path(str(ROOT / 'Tools/ui/news_rewards' / name), run_name='__main__')
    report['status'] = 'passed'
except Exception as error:
    report.update(status='failed', error=str(error))
    unreal.log_error('NEWS_REWARDS_APPLY_FAIL: ' + str(error))
finally:
    (OUTPUT / 'apply-report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.SystemLibrary.quit_editor()
