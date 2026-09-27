# expansion70_paths.py
# 역할: 시설 제작·마무리·보고서가 같은 Blender 원본 폴더를 사용하도록 경로를 관리한다.
"""공유 기본 경로와 개인 작업 경로를 한 곳에서 결정한다."""
import os
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[2]


def blend_directory():
    """FF_FACILITY_BLEND_DIR 설정을 우선하고, 없으면 저장소의 제작 원본 폴더를 쓴다."""
    configured = os.environ.get("FF_FACILITY_BLEND_DIR")
    return (Path(configured).expanduser() if configured else
            PROJECT_ROOT / "Design/SourceArt/FacilityExpansion70/Blends").resolve()
