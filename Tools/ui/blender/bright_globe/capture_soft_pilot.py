"""사용자 색감 피드백을 적용·저장한 뒤 v002 단일 화면을 검증한다."""
from pathlib import Path
import sys
import unreal

root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
sys.path.insert(0,str(root/'Tools/ui/blender/bright_globe'))
import apply_soft_palette
import capture_single_pilot
