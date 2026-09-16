"""Run with UnrealEditor-Cmd -run=pythonscript -script=<this file> after an Editor build.

Creates only a missing table and native UMG Blueprint, verifies starter rules against
the source CSV, then proves repeated commands leave both saved assets byte-for-byte intact.
Existing designer edits are preserved; a seed mismatch fails verification, never repairs it.
"""

import csv
import hashlib
import json
import math
import traceback
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "Saved/FacilitySynergy"
SOURCE = ROOT / "data/unreal/facility_synergies_v0.csv"
ASSET_PATH = "/Game/FestaFiesta/Data/Economy/DT_FacilitySynergies"
ASSET_FILE = ROOT / "Content/FestaFiesta/Data/Economy/DT_FacilitySynergies.uasset"
COMMAND = "FF.CreateFacilitySynergyTable"
EXPECTED_STRUCT = "/Script/FestaFiesta.FF_FacilitySynergyRow"
OPTIONS_FUNCTION = "FestaFiesta.FF_FacilitySynergySettings.GetFacilityIdOptions"
WIDGET_PATH = "/Game/FestaFiesta/UI/Building/WBP_FF_BuildSynergy"
WIDGET_FILE = ROOT / "Content/FestaFiesta/UI/Building/WBP_FF_BuildSynergy.uasset"
WIDGET_COMMAND = "FF.CreateBuildSynergyWidget"


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(path=ASSET_FILE):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_widget():
    existed_before = WIDGET_FILE.exists()
    prior_sha256 = digest(WIDGET_FILE) if existed_before else None
    unreal.SystemLibrary.execute_console_command(None, WIDGET_COMMAND)
    widget = unreal.load_asset(WIDGET_PATH)
    require(widget and widget.generated_class() and WIDGET_FILE.is_file(),
            "Synergy WBP was not created and saved. Check FF_BUILD_SYNERGY_UMG_FAIL or rebuild "
            "FestaFiestaEditor if FF.CreateBuildSynergyWidget is unknown.")
    parent = unreal.load_class(None, "/Script/FestaFiesta.FF_BuildSynergyWidget")
    require(parent and unreal.MathLibrary.class_is_child_of(widget.generated_class(), parent),
            "Synergy WBP must inherit FF_BuildSynergyWidget; existing asset preserved")
    visibility = unreal.get_default_object(widget.generated_class()).get_editor_property("visibility")
    require(visibility not in (unreal.SlateVisibility.COLLAPSED, unreal.SlateVisibility.HIDDEN),
            "New synergy WBP should be visible when opened in UMG Designer")
    first_sha256 = digest(WIDGET_FILE)
    if existed_before:
        require(first_sha256 == prior_sha256, "Authoring command changed an existing designer-owned WBP")
    unreal.SystemLibrary.execute_console_command(None, WIDGET_COMMAND)
    second_sha256 = digest(WIDGET_FILE)
    require(first_sha256 == second_sha256, "Repeated authoring command rewrote the WBP")
    return {
        "asset": WIDGET_PATH,
        "saved_file": str(WIDGET_FILE),
        "generated_class": widget.generated_class().get_path_name(),
        "sha256": first_sha256,
        "second_command_sha256": second_sha256,
        "rerun_preserved_saved_bytes": True,
        "binding_validation": "FF.CreateBuildSynergyWidget checks all nine native widget names and types",
        "designer_visibility": str(visibility),
    }


def main():
    with SOURCE.open(encoding="utf-8-sig", newline="") as source_file:
        source_rows = list(csv.DictReader(source_file))
    expected = {row["Name"]: row for row in source_rows}
    require(len(expected) == len(source_rows) and expected, "Source CSV has missing or duplicate row names")

    existed_before = ASSET_FILE.exists()
    prior_sha256 = digest() if existed_before else None
    unreal.SystemLibrary.execute_console_command(None, COMMAND)
    table = unreal.load_asset(ASSET_PATH)
    require(isinstance(table, unreal.DataTable) and ASSET_FILE.is_file(),
            "Facility synergy DataTable was not created and saved. Check FF_SYNERGY_TABLE_FAIL in the log; "
            "if the command is unknown, rebuild FestaFiestaEditor and restart this verification.")
    first_sha256 = digest()
    if existed_before:
        require(first_sha256 == prior_sha256, "Authoring command changed an existing designer-owned asset")

    row_struct = table.get_editor_property("row_struct")
    require(row_struct and row_struct.get_path_name() == EXPECTED_STRUCT,
            "Wrong DataTable row struct; the existing asset was preserved")
    row_names = [str(name) for name in unreal.DataTableFunctionLibrary.get_data_table_row_names(table)]
    require(set(row_names) == set(expected), "Saved table row names differ from the starter CSV; existing edits were preserved")

    # These APIs return row-map order and raw field names, independent of Korean display labels.
    observed = {row_name: {} for row_name in row_names}
    for property_name in ("bEnabled", "DisplayName", "TargetFacilityId", "PartnerFacilityId",
                          "MaxDistanceMeters", "RevenueBonusPercent", "StackingGroup"):
        column = unreal.DataTableFunctionLibrary.get_data_table_column_as_string(table, property_name)
        require(len(column) == len(row_names), f"Cannot read table column {property_name}")
        for row_name, actual in zip(row_names, column):
            value = str(actual)
            wanted = expected[row_name][property_name]
            observed[row_name][property_name] = value
            if property_name in ("MaxDistanceMeters", "RevenueBonusPercent"):
                matches = math.isclose(float(value), float(wanted), rel_tol=1e-6)
            elif property_name == "bEnabled":
                matches = value.casefold() == wanted.casefold()
            elif property_name == "DisplayName":
                # FText export can contain its NSLOCTEXT/INVTEXT wrapper and namespace.
                matches = wanted in value
            else:
                matches = value == wanted
            require(matches, f"{row_name}.{property_name} differs from source CSV; existing edits were preserved")

    require(unreal.find_object(None, OPTIONS_FUNCTION) is not None,
            "Facility ID dropdown GetOptions function cannot be resolved")
    unreal.SystemLibrary.execute_console_command(None, COMMAND)
    second_sha256 = digest()
    require(first_sha256 == second_sha256, "Repeated authoring command rewrote the DataTable")

    cook_config = (ROOT / "Config/DefaultGame.ini").read_text(encoding="utf-8-sig")
    require('+DirectoriesToAlwaysCook=(Path="/Game/FestaFiesta/Data")' in cook_config,
            "DataTable folder is no longer covered by the existing packaging configuration")
    require('+DirectoriesToAlwaysCook=(Path="/Game/FestaFiesta/UI")' in cook_config,
            "Widget folder is no longer covered by the existing packaging configuration")
    widget_report = verify_widget()
    return {
        "status": "PASS",
        "asset": ASSET_PATH,
        "saved_file": str(ASSET_FILE),
        "row_struct": row_struct.get_path_name(),
        "rows": observed,
        "asset_existed_before": existed_before,
        "sha256": first_sha256,
        "second_command_sha256": second_sha256,
        "rerun_preserved_saved_bytes": True,
        "get_options_function_resolved": True,
        "cook_directory": "/Game/FestaFiesta/Data",
        "widget": widget_report,
    }


OUT.mkdir(parents=True, exist_ok=True)
try:
    report = main()
except Exception:
    report = {"status": "FAIL", "error": traceback.format_exc()}
    (OUT / "asset_verification.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log_error("FACILITY_SYNERGY_ASSET_VERIFICATION_FAIL: " + report["error"])
    raise
else:
    (OUT / "asset_verification.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("FACILITY_SYNERGY_ASSET_VERIFICATION_PASS: " + report["sha256"])
