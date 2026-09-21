"""Read production definitions and retain the exact pre-maintenance data assets."""
import hashlib
import json
import shutil
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
out = root / "Saved/BuildModeUI/20260919_maintenance"
out.mkdir(parents=True, exist_ok=True)
packages = [
    "/Game/FestaFiesta/Building/FF_Data_Building/Info/PDA_FFZS_Info_Ticketing_Ticket",
    "/Game/FestaFiesta/Data/Economy/DT_FacilitySynergies",
]
backup = {}
for package in packages:
    path = root / "Content" / (package[6:] + ".uasset")
    saved = out / "before_assets" / path.relative_to(root)
    saved.parent.mkdir(parents=True, exist_ok=True)
    if not saved.exists():
        shutil.copy2(path, saved)
    backup[package] = hashlib.sha256(saved.read_bytes()).hexdigest()

definitions = []
for path in unreal.EditorAssetLibrary.list_assets("/Game/FestaFiesta/Building/FF_Data_Building", recursive=True, include_folder=False):
    if "/PDA_" not in path:
        continue
    asset = unreal.load_asset(path)
    if not isinstance(asset, unreal.FFZSFacilityDefinition):
        continue
    spec = asset.get_editor_property("spec")
    definitions.append({
        "path": asset.get_path_name(),
        "id": str(spec.get_editor_property("facility_definition_id")),
        "name": str(spec.get_editor_property("display_name")),
        "category": str(spec.get_editor_property("category")),
        "in_catalog": asset.get_editor_property("show_in_build_catalog"),
    })
table = unreal.load_asset(packages[1])
rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
report = {"definitions": sorted(definitions, key=lambda item: item["id"]), "synergies": rows, "before_sha256": backup}
(out / "production-data-before.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("FF_DATA_MAINTENANCE_AUDIT_PASS")
