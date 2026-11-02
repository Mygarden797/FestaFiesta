"""Fresh disk verification followed by the existing build UI automation suite."""
from pathlib import Path
import runpy
import traceback
import unreal

root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
try:
    runpy.run_path(str(root / "Tools/BuildModeIcons/verify_icons.py"))
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.SystemLibrary.execute_console_command(
        world, "Automation RunTests FestaFiesta.Building.FeatureGuide+FestaFiesta.UI.PrepBuild")
except Exception:
    (root / "Saved/BuildModeIcons/verify-error.txt").write_text(traceback.format_exc(), encoding="utf-8")
    unreal.log_error(traceback.format_exc())
    unreal.SystemLibrary.quit_editor()
