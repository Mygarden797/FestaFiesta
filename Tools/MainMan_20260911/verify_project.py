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
