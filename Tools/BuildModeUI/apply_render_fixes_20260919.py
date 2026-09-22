# apply_render_fixes_20260919.py
# 역할: 승인된 건설 화면 및 데이터 이관 작업에서 지정된 대상에 변경을 적용하고 결과를 확인한다.

"""Apply only follow-up fixes verified by the 720p/1080p render tests."""
import hashlib
import json
from pathlib import Path
import runpy
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
out=root/"Saved/BuildModeUI/20260919"
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world and world.get_path_name().startswith("/Engine/Maps/Entry.")
commands=["FF.FixFacilityInfoV2Layout", "FF.ApplyFacilityCodexCardFit20260919", "FF.ApplyFacilityCodexScreenFit72020260919", "FF.ApplyFacilityWorldBadgeAmountFit20260919"]
for command in commands:
    unreal.SystemLibrary.execute_console_command(world,command)
runpy.run_path(str(Path(__file__).parent/"tune_revenue_after_render.py"))
unreal.SystemLibrary.execute_console_command(world,"FF.FinalizeFacilityRevenueLayout20260919")
packages=[
    "/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityInfoPanel",
    "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexCard",
    "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexScreen",
    "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityRevenue/WBP_FF_FacilityWorldBadge",
    "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityRevenue/WBP_FF_FacilityRevenueRanking",
    "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityRevenue/WBP_FF_FacilityRevenueRankRow",
]
def digest(package):
    return hashlib.sha256((root/"Content"/(package[6:]+".uasset")).read_bytes()).hexdigest()
hashes={package:digest(package) for package in packages}
for command in commands:
    unreal.SystemLibrary.execute_console_command(world,command)
runpy.run_path(str(Path(__file__).parent/"tune_revenue_after_render.py"))
unreal.SystemLibrary.execute_console_command(world,"FF.FinalizeFacilityRevenueLayout20260919")
assert all(digest(package)==value for package,value in hashes.items()), "Fix command rewrote authored assets on second run"
(out/"render-fixes-result.json").write_text(json.dumps({"status":"PASS", "commands":commands + ["tune_revenue_after_render.py", "FF.FinalizeFacilityRevenueLayout20260919"], "sha256":hashes},indent=2),encoding="utf-8")
# Revalidate original preservation contracts and update the final WBP inventory.
runpy.run_path(str(Path(__file__).parent/"apply_approved_design_20260919.py"))
unreal.log("FF_BUILD_UI_RENDER_FIXES_PASS")
