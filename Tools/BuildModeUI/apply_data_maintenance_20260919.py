"""Explicit data repair and additive synergy import; never regenerates a WBP."""
import csv
import hashlib
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
out = root / "Saved/BuildModeUI/20260919_maintenance"
before = json.loads((out / "production-data-before.json").read_text(encoding="utf-8"))
ticket_path = "/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Info/PDA_FFZS_Info_Ticketing_Ticket"
table_path = "/Game/FestaFiesta/Data/Economy/DT_FacilitySynergies"
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world and world.get_path_name().startswith("/Engine/Maps/Entry.")

def digest(package):
    return hashlib.sha256((root / "Content" / (package[6:] + ".uasset")).read_bytes()).hexdigest()

ui_record = json.loads((root / "Saved/BuildModeUI/20260919/migration-result.json").read_text(encoding="utf-8"))
ui_hashes = {package: digest(package) for package in ui_record["packages"] + ui_record["preserved_packages"]}
ticket = unreal.load_asset(ticket_path)
assert ticket and isinstance(ticket, unreal.FFZSFacilityDefinition)
spec = ticket.get_editor_property("spec").copy()
original_spec = spec.export_text()
original_id = str(spec.get_editor_property("facility_definition_id"))
original_name = spec.get_editor_property("display_name")
assert original_id in ("티켓 안내소", "Info_Ticketing_Ticket"), "Unexpected designer ID; no implicit repair"
if original_id == "티켓 안내소":
    spec.set_editor_property("facility_definition_id", unreal.Name("Info_Ticketing_Ticket"))
    # Recover the misplaced Korean label only from the exact known legacy state.
    # An unrelated designer-authored name is preserved.
    if str(original_name) == "Ticket":
        spec.set_editor_property("display_name", unreal.Text("티켓 안내소"))
    restored = spec.copy()
    restored.set_editor_property("facility_definition_id", unreal.Name(original_id))
    restored.set_editor_property("display_name", original_name)
    assert restored.export_text() == original_spec, "Non-identity specification changed"
    ticket.set_editor_property("spec", spec)
    assert unreal.EditorAssetLibrary.save_loaded_asset(ticket, only_if_is_dirty=True)

table = unreal.load_asset(table_path)
assert table
def table_rows():
    return {row["Name"]: row for row in json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))}

unreal.SystemLibrary.execute_console_command(world, "FF.AppendFacilitySynergyRows")
rows = table_rows()
original_rows = {row["Name"]: row for row in before["synergies"]}
with (root / "data/unreal/facility_synergies_v0.csv").open(encoding="utf-8-sig", newline="") as file:
    seeded = list(csv.DictReader(file))
assert len(seeded) == 22 and len({row["Name"] for row in seeded}) == 22
assert len(set(row["Name"] for row in seeded) - set(original_rows)) == 20
for key, old in original_rows.items():
    assert rows.get(key) == old, f"Existing designer row changed: {key}"
for row in seeded:
    assert row["Name"] in rows, f"Missing imported row {row['Name']}"
    if row["Name"] in original_rows:
        continue
    actual = rows[row["Name"]]
    for column in ("TargetFacilityId", "PartnerFacilityId", "StackingGroup"):
        assert actual[column] == row[column], f"{row['Name']}.{column}"
    for column in ("MaxDistanceMeters", "RevenueBonusPercent"):
        assert float(actual[column]) == float(row[column]), f"{row['Name']}.{column}"
    assert actual["bEnabled"] is True
    assert row["DisplayName"] in actual["DisplayName"]

hashes = {package: digest(package) for package in (ticket_path, table_path)}
unreal.SystemLibrary.execute_console_command(world, "FF.AppendFacilitySynergyRows")
assert all(digest(package) == value for package, value in hashes.items()), "Repeat import rewrote saved assets"
assert all(digest(package) == value for package, value in ui_hashes.items()), "Maintenance changed a UI asset"
registry = unreal.AssetRegistryHelpers.get_asset_registry()
referencers = [str(value) for value in registry.get_referencers(ticket_path, unreal.AssetRegistryDependencyOptions())]
report = {
    "status": "PASS", "previous_id": original_id,
    "ticket_id": str(ticket.get_editor_property("spec").get_editor_property("facility_definition_id")),
    "ticket_name": str(ticket.get_editor_property("spec").get_editor_property("display_name")),
    "ticket_original_spec": original_spec,
    "ticket_final_spec": ticket.get_editor_property("spec").export_text(),
    "ticket_asset_referencers": referencers,
    "existing_rows_preserved": list(original_rows),
    "added_rows": sorted(set(row["Name"] for row in seeded) - set(original_rows)),
    "row_count": len(rows), "rows": list(rows.values()),
    "sha256": hashes, "ui_assets_preserved": ui_hashes,
    "idempotence": True,
}
(out / "data-application-result.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("FF_DATA_MAINTENANCE_APPLY_PASS")
