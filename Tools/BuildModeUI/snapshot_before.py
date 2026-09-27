# snapshot_before.py
# 역할: 승인된 건설 화면 및 데이터 이관 작업에서 변경 전 상태를 기록한다.

"""Read-only pre-migration WBP snapshot for the approved September build UI."""
import hashlib, json
from pathlib import Path
import unreal
root = Path(unreal.Paths.project_dir()).resolve()
out = root / "Saved/BuildModeUI/20260919"
out.mkdir(parents=True, exist_ok=True)
packages = [
"/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityInfoPanel",
"/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexScreen",
"/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexCard",
"/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepPanelBuild",
"/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepBuildTile",
"/Game/FestaFiesta/Building/FF_WBP_Building/Controls/WBP_FF_BuildTools",
"/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar"]
result = {}
for package in packages:
    asset = unreal.EditorAssetLibrary.load_asset(package)
    if not asset:
        raise RuntimeError("Missing baseline WBP: " + package)
    file = root / "Content" / (package.removeprefix("/Game/") + ".uasset")
    widgets = {}
    for w in unreal.ObjectIterator(unreal.Widget):
        outer = w.get_outer()
        if not outer or outer.get_class().get_name() != "WidgetTree" or outer.get_outer() != asset:
            continue
        row = {"class": w.get_class().get_path_name(), "parent": w.get_parent().get_name() if w.get_parent() else None}
        slot = w.get_editor_property("slot")
        if slot:
            row["slot_class"] = slot.get_class().get_name()
            for key in ("layout_data", "padding", "size", "horizontal_alignment", "vertical_alignment"):
                try:
                    value = slot.get_editor_property(key)
                    row[key] = value.export_text() if hasattr(value,"export_text") else str(value)
                except Exception:
                    pass
        widgets[w.get_name()] = row
    result[package] = {"sha256":hashlib.sha256(file.read_bytes()).hexdigest(), "widgets":widgets}
(out / "wbp-baseline.json").write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding="utf-8")
unreal.log("FF_BUILD_UI_BASELINE_OK " + str(out / "wbp-baseline.json"))

