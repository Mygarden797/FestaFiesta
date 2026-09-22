# audit_project_fonts.py
# 역할: 시설 조합 도감과 보상 표시 작업에서 기존 데이터를 읽어 상태·문제를 점검한다.

"""Read imported game fonts without modifying any source assets."""
import json
from pathlib import Path
import unreal

out = Path(unreal.Paths.project_dir()).resolve() / "Saved/FacilityCodexGlass_20260921"
out.mkdir(parents=True, exist_ok=True)
rows = []
for path in unreal.EditorAssetLibrary.list_assets("/Game/Asset/Font", recursive=True, include_folder=False):
    obj = unreal.load_asset(path)
    row = {"path": path, "class": obj.get_class().get_name()}
    if isinstance(obj, unreal.Font):
        try:
            composite = obj.get_editor_property("composite_font")
            typeface = composite.get_editor_property("default_typeface")
            row["typefaces"] = [str(entry.get_editor_property("name"))
                                for entry in typeface.get_editor_property("fonts")]
        except Exception as error:
            row["reflection_note"] = str(error)
        row["cache_type"] = str(obj.get_editor_property("font_cache_type"))
    rows.append(row)
(out / "font-inventory.json").write_text(json.dumps(rows, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("FF_PROJECT_FONT_AUDIT_PASS: " + str(len(rows)) + " font assets inspected")
