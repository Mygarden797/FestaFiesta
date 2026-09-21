"""Create a missing revenue table; preserve existing editor-authored values on reruns.

Run with UnrealEditor-Cmd -run=pythonscript -script=<this file> after building.
Optional -FFVerifyRevenueSeed checks every amount against the initial balance CSV.
One-time -FFMigrateFacilityIncome fills newly added income fields on a legacy table.
For a table already migrated before minigames, -FFMigrateMinigameIncome only adds that portion.
Migration preserves all existing names/base revenues and backs up the original asset.
The CSV seeds creation only; gameplay reads the saved DataTable directly.
"""

import csv
import hashlib
import json
import math
import re
import shutil
import traceback
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "Saved/FacilityIncomeTable_20260920"
SOURCE = ROOT / "data/unreal/facility_revenues_v0.csv"
ASSET_PATH = "/Game/FestaFiesta/Data/Economy/DT_FacilityRevenues"
ASSET_FILE = ROOT / "Content/FestaFiesta/Data/Economy/DT_FacilityRevenues.uasset"
STRUCT_PATH = "/Script/FestaFiesta.FF_FacilityRevenueTableRow"
DEFINITION_ROOT = "/Game/FestaFiesta/Building/FF_Data_Building"
FESTIVAL_PATH = "/Game/FestaFiesta/Data/Festivals/DA_Festival_Busan"


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest():
    return hashlib.sha256(ASSET_FILE.read_bytes()).hexdigest()


def read_rows(table):
    rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    # UE 5.7 exports scalar map values wrapped in the map property's name, while
    # its JSON importer expects scalar values directly. Normalize that asymmetry.
    for row in rows:
        for field in ("RevenueMultiplierByLevel", "ProgramFeesPerPerson"):
            row[field] = {key: value[field] if isinstance(value, dict) else value
                          for key, value in row[field].items()}
    return rows


def normalized(value):
    if isinstance(value, float):
        return round(value, 6)
    if isinstance(value, dict):
        return {key: normalized(item) for key, item in value.items()}
    if isinstance(value, list):
        return [normalized(item) for item in value]
    return value


def main():
    with SOURCE.open(encoding="utf-8-sig", newline="") as handle:
        source_rows = list(csv.DictReader(handle))
    expected = {row["Name"]: int(row["RevenuePerUseGold"]) for row in source_rows}
    require(len(expected) == len(source_rows) and expected, "Seed rows are empty or duplicated")
    require(all(name and amount >= 0 for name, amount in expected.items()), "Invalid seed row")
    row_struct = unreal.load_object(None, STRUCT_PATH)
    require(row_struct, "Build FestaFiestaEditor first: revenue row struct is missing")
    command_line = unreal.SystemLibrary.get_command_line()
    migrate = bool(re.search(r"(?:^|\s)-FFMigrateFacilityIncome(?:\s|$)", command_line))
    migrate_minigames = bool(re.search(r"(?:^|\s)-FFMigrateMinigameIncome(?:\s|$)", command_line))
    require(not (migrate and migrate_minigames), "Use only one explicit migration mode")
    staging = unreal.new_object(unreal.DataTable)
    require(unreal.DataTableFunctionLibrary.fill_data_table_from_csv_file(staging, str(SOURCE), row_struct),
            "Cannot parse the revenue seed CSV")
    seed_rows = {row["Name"]: row for row in read_rows(staging)}
    require({name: int(row["RevenuePerUseGold"]) for name, row in seed_rows.items()} == expected,
            "CSV import did not preserve all seed amounts")

    existed_before = ASSET_FILE.exists()
    previous_digest = digest() if existed_before else None
    table = unreal.load_asset(ASSET_PATH) if existed_before else None
    migrated = False
    if not existed_before:
        factory = unreal.DataTableFactory()
        factory.set_editor_property("struct", row_struct)
        table = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "DT_FacilityRevenues", "/Game/FestaFiesta/Data/Economy", unreal.DataTable, factory)
        require(table, "Could not create facility revenue DataTable")
        require(unreal.DataTableFunctionLibrary.fill_data_table_from_csv_file(table, str(SOURCE), row_struct),
                "Could not populate facility revenue DataTable")
        require(unreal.EditorAssetLibrary.save_loaded_asset(table, only_if_is_dirty=False),
                "Could not save facility revenue DataTable")

    require(isinstance(table, unreal.DataTable), "Existing package is not a DataTable; preserved")
    require(table.get_editor_property("row_struct").get_path_name() == STRUCT_PATH,
            "Existing row struct differs; preserved")
    if existed_before and (migrate or migrate_minigames):
        original_rows = read_rows(table)
        minigame_fields = ("bOverrideMinigameRewardGold", "MinigameRewardAtMaxScoreGold")
        income_fields = minigame_fields if migrate_minigames else (
            "bOverrideCommissionRate", "CommissionRate", "RevenueMultiplierByLevel", "ProgramFeesPerPerson", *minigame_fields)
        # Refuse to guess whether a designer deliberately cleared a map/override.
        # The explicit migration is only for a wholly pristine legacy table.
        require(migrate_minigames or all(not row["bOverrideCommissionRate"] and not row["RevenueMultiplierByLevel"]
                    and not row["ProgramFeesPerPerson"] and abs(row["CommissionRate"] - 0.12) < 1e-6
                    for row in original_rows),
                "Income fields are already authored; run without -FFMigrateFacilityIncome to preserve them")
        require(all(not row["bOverrideMinigameRewardGold"] and row["MinigameRewardAtMaxScoreGold"] == 0
                    for row in original_rows), "Minigame income is already authored; use verification only")
        original_ids = {row["Name"] for row in original_rows}
        require(original_ids <= set(seed_rows), "Migration would discard an authored facility row")
        merged = [{**row, **{key: seed_rows[row["Name"]][key] for key in income_fields}}
                  for row in original_rows]
        merged.extend(row for name, row in seed_rows.items() if name not in original_ids)
        payload = json.dumps(merged, ensure_ascii=False)
        require(unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(staging, payload, row_struct),
                "Cannot validate migrated income fields")
        staged_rows = {row["Name"]: row for row in read_rows(staging)}
        merged_rows = {row["Name"]: row for row in merged}
        if normalized(staged_rows) != normalized(merged_rows):
            (OUT / "migration_comparison.json").write_text(json.dumps(
                {"expected": merged_rows, "actual": staged_rows}, ensure_ascii=False, indent=2), encoding="utf-8")
        require(normalized(staged_rows) == normalized(merged_rows), "Migration changed authored values")
        backup = OUT / ("DT_FacilityRevenues.before_income_" + previous_digest[:12] + ".uasset")
        if not backup.exists():
            shutil.copy2(ASSET_FILE, backup)
        require(unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, payload, row_struct),
                "Cannot apply migrated income fields")
        require(unreal.EditorAssetLibrary.save_loaded_asset(table, only_if_is_dirty=False), "Cannot save migrated table")
        migrated = True
    rows = read_rows(table)
    actual = {row["Name"]: int(row["RevenuePerUseGold"]) for row in rows}
    require(len(actual) == len(rows) and actual, "Saved table has duplicate or missing rows")
    require(all(amount >= 0 for amount in actual.values()), "Negative saved revenue value")
    for row in rows:
        require(math.isfinite(row["CommissionRate"]) and 0 <= row["CommissionRate"] <= 1,
                f"Invalid commission: {row['Name']}")
        require(all(int(level) >= 1 and math.isfinite(multiplier) and multiplier >= 0
                    for level, multiplier in row["RevenueMultiplierByLevel"].items()),
                f"Invalid level multiplier: {row['Name']}")
        require(all(program_id and program_id != "None" and fee >= 0
                    for program_id, fee in row["ProgramFeesPerPerson"].items()),
                f"Invalid program fee: {row['Name']}")
        require(row["MinigameRewardAtMaxScoreGold"] >= 0, f"Invalid minigame cap: {row['Name']}")

    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous([DEFINITION_ROOT], force_rescan=True)
    definitions = []
    for record in registry.get_assets_by_path(DEFINITION_ROOT, recursive=True):
        if str(record.asset_class_path.asset_name) != "FFZSFacilityDefinition":
            continue
        asset = record.get_asset()
        require(asset, "Could not load facility definition")
        facility_id = str(asset.get_editor_property("spec").get_editor_property("facility_definition_id"))
        require(facility_id in actual, f"Revenue table misses {facility_id}")
        definitions.append(facility_id)
    require(definitions and len(set(definitions)) == len(definitions), "Facility IDs are missing or duplicated")
    minigames = []
    minigame_targets = set()
    for record in registry.get_assets_by_path("/Game/FestaFiesta/Minigame/Data", recursive=True):
        asset = record.get_asset()
        if not isinstance(asset, unreal.FF_MinigameDefinition):
            continue
        for facility_id in asset.get_editor_property("compatible_facility_definition_ids"):
            facility_id = str(facility_id)
            minigame_targets.add(facility_id)
            require(facility_id in actual, f"Missing minigame target row: {facility_id}")
            row = next(item for item in rows if item["Name"] == facility_id)
            require(row["bOverrideMinigameRewardGold"], f"Minigame reward is not table-controlled: {facility_id}")
            minigames.append({"facility": facility_id, "minigame": str(asset.get_editor_property("minigame_id")),
                              "cap": row["MinigameRewardAtMaxScoreGold"]})
    require(minigames, "No minigame definitions found")
    require(set(actual) == set(definitions) | minigame_targets,
            "Table has rows without facility definitions or registered minigame targets")

    festival = unreal.load_asset(FESTIVAL_PATH)
    require(festival, "Cannot load the production festival")
    configured_table = festival.get_editor_property("facility_revenue_table")
    require(configured_table == table, "Production festival does not use DT_FacilityRevenues")
    actual_rows = {row["Name"]: row for row in rows}
    # Text localization namespaces depend on the destination package. The seed
    # comparison covers every income field without replacing localized names.
    def income_rows(items):
        return {name: {key: value for key, value in row.items() if key != "DisplayName"}
                for name, row in items.items()}
    seed_matches = normalized(income_rows(actual_rows)) == normalized(income_rows(seed_rows))
    if re.search(r"(?:^|\s)-FFVerifyRevenueSeed(?:\s|$)", command_line):
        require(seed_matches, "Saved table differs from initial balance seed; existing edits preserved")
    program_pairs = []
    for card in festival.get_editor_property("program_catalog"):
        program_id = str(card.get_editor_property("program_id"))
        allowed = {str(item) for item in card.get_editor_property("allowed_facility_types")}
        for facility_id in definitions:
            facility_type = "_".join(facility_id.split("_")[:2])
            if not allowed or facility_type in allowed:
                require(program_id in actual_rows[facility_id]["ProgramFeesPerPerson"],
                        f"Program fee missing: {facility_id}/{program_id}")
                program_pairs.append({"facility": facility_id, "program": program_id,
                                      "fee": actual_rows[facility_id]["ProgramFeesPerPerson"][program_id]})
    cook_config = (ROOT / "Config/DefaultGame.ini").read_text(encoding="utf-8-sig")
    require('+DirectoriesToAlwaysCook=(Path="/Game/FestaFiesta/Data")' in cook_config,
            "Revenue table directory is not included for packaging")
    if existed_before and not migrated:
        require(digest() == previous_digest, "Verification rewrote an existing editor-authored asset")
    return {
        "status": "PASS", "asset": ASSET_PATH, "row_struct": STRUCT_PATH,
        "row_count": len(actual), "paid_count": sum(value > 0 for value in actual.values()),
        "free_count": sum(value == 0 for value in actual.values()),
        "facility_definition_count": len(definitions),
        "reserved_minigame_target_count": len(minigame_targets - set(definitions)), "minigames": minigames,
        "production_festival": festival.get_path_name(),
        "festival_table": configured_table.get_path_name(),
        "seed_matches": seed_matches, "asset_existed_before": existed_before,
        "existing_values_preserved": not existed_before or migrated or digest() == previous_digest,
        "migrated_income_fields": migrated, "program_pairs": program_pairs,
        "sha256": digest(), "rows": rows,
    }


OUT.mkdir(parents=True, exist_ok=True)
try:
    result = main()
except Exception:
    result = {"status": "FAIL", "error": traceback.format_exc()}
    (OUT / "asset_verification.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log_error("FF_FACILITY_REVENUE_TABLE_FAIL: " + result["error"])
    raise
else:
    (OUT / "asset_verification.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("FF_FACILITY_REVENUE_TABLE_PASS: " + result["sha256"])
