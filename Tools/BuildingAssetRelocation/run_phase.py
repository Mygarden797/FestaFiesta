"""Run a synchronous native relocation phase, then let the Python command exit UE."""
import json
import re
from pathlib import Path
import unreal

match = re.search(r'-FFBuildingRelocationPhase=(move|fixup)\b', unreal.SystemLibrary.get_command_line())
if not match:
    raise RuntimeError('Specify -FFBuildingRelocationPhase=move or fixup')
phase = match.group(1)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
command = 'FF.MoveBuildingAssets' if phase == 'move' else 'FF.FixupBuildingAssetRedirectors'
unreal.SystemLibrary.execute_console_command(world, command)
report = Path(unreal.Paths.project_saved_dir()) / 'BuildingAssetRelocation_20260921' / (phase + '-result.json')
result = json.loads(report.read_text(encoding='utf-8-sig'))
if not result['success']:
    raise RuntimeError('Relocation {} failed: {}'.format(phase, result['errors']))
unreal.log('FF_BUILD_RELOCATION_PHASE_PASS ' + phase)
