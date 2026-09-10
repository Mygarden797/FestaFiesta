"""Verify the saved assets in a second Unreal process without mutating them."""
import json
from pathlib import Path
import sys
import time

import unreal

TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))
from import_roads import check, verify_mesh, measurements, CALIBRATION


def run():
    imported = json.loads((TOOLS / 'qa/unreal_import_report.json').read_text(encoding='utf-8'))
    check(imported['status'] == 'PASS', 'The import must pass first')
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    material = unreal.load_asset(imported['material'])
    check(material is not None, 'Saved palette material missing')
    entries = [verify_mesh(unreal.load_asset(item['asset']), material, subsystem) for item in imported['entries']]
    calibrations = []
    for item, (_, center, size) in zip(imported['calibration'], CALIBRATION):
        actual = measurements(unreal.load_asset(item['asset']))
        ok = all(abs(a-b) < .1 for a, b in zip(actual['center_cm'], center)) and all(abs(a-b) < .1 for a, b in zip(actual['size_cm'], size))
        calibrations.append({'asset': item['asset'], **actual, 'status': 'PASS' if ok else 'FAIL'})
    status = 'PASS' if all(item['status'] == 'PASS' for item in entries + calibrations) else 'FAIL'
    report = {'status': status, 'fresh_process': True, 'timestamp': time.time(),
              'entries': entries, 'calibration': calibrations}
    (TOOLS / 'qa/unreal_reload_report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    check(status == 'PASS', 'ROAD_VERIFY_FAIL: saved asset measurements failed')
    unreal.log('ROAD_VERIFY_PASS: all six saved StaticMeshes passed')


try:
    run()
finally:
    unreal.SystemLibrary.quit_editor()
