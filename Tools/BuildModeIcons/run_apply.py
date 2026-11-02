"""One-shot wrapper for an isolated Entry editor; always closes that editor."""
from pathlib import Path
import runpy
import traceback
import unreal

root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
try:
    runpy.run_path(str(root / "Tools/BuildModeIcons/apply_icons.py"), run_name="__main__")
    (root / "Saved/BuildModeIcons/apply-error.txt").unlink(missing_ok=True)
except Exception:
    (root / "Saved/BuildModeIcons/apply-error.txt").write_text(traceback.format_exc(), encoding="utf-8")
    unreal.log_error(traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
