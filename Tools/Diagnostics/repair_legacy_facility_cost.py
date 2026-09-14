"""Audit/fix the hidden legacy Taco's price override without migrating its identity.

Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>.
Default is read-only; -FFApplyLegacyFacilityCost saves this one asset.
-FFVerifyLegacyFacilityCost requires the repaired state in a fresh process.
"""
import datetime
import hashlib
import json
from pathlib import Path
import re

import unreal


ROOT = Path(__file__).resolve().parents[2]
BASE = "/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Booth/"
LEGACY = BASE + "PDA_FFZS_Booth_FoodTruck_Taco"
CURRENT = BASE + "PDA_FFZS_Booth_FoodStall_Taco"
COSTS = "/Game/FestaFiesta/Data/Economy/DT_FacilityCosts"
EXPECTED_COST_KEY = "Cost.Booth.FoodStall.Taco"


def has_switch(name):
    return re.search(r"(?:^|\s)-" + re.escape(name) + r"(?:\s|$)",
                     unreal.SystemLibrary.get_command_line(), re.IGNORECASE) is not None


def run():
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    output = ROOT / "Artifacts" / "Validation" / ("LegacyFacilityCost-" + stamp)
    output.mkdir(parents=True, exist_ok=False)
    apply = has_switch("FFApplyLegacyFacilityCost")
    verify = has_switch("FFVerifyLegacyFacilityCost")
    assert not (apply and verify), "Choose apply or fresh-session verification, not both."

    legacy = unreal.load_asset(LEGACY)
    current = unreal.load_asset(CURRENT)
    costs = unreal.load_asset(COSTS)
    assert legacy and current and costs, "Legacy, canonical definition, and cost table must load."
    legacy_spec = legacy.get_editor_property("spec")
    current_spec = current.get_editor_property("spec")
    assert str(legacy_spec.get_editor_property("facility_definition_id")) == "Booth_FoodTruck_Taco"
    assert str(current_spec.get_editor_property("facility_definition_id")) == "Booth_FoodStall_Taco"
    assert not legacy.get_editor_property("show_in_build_catalog"), "Legacy compatibility entry must stay hidden."
    assert current.get_editor_property("show_in_build_catalog"), "Canonical entry must remain buildable."
    assert legacy_spec.get_editor_property("visual_mesh") == current_spec.get_editor_property("visual_mesh")
    assert str(current_spec.get_editor_property("install_cost_key")) == "None", "Canonical entry must retain automatic pricing."

    rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(costs))
    cost_rows = {row["Name"]: row for row in rows}
    assert EXPECTED_COST_KEY in cost_rows, "The exact canonical cost row must exist; no default fallback."
    assert cost_rows[EXPECTED_COST_KEY]["InstallCost"] > 0, "Taco installation price must be positive."
    previous_key = str(legacy_spec.get_editor_property("install_cost_key"))
    assert previous_key in ("None", EXPECTED_COST_KEY), "An unrelated explicit price override must not be overwritten."
    before_spec = legacy_spec.export_text()
    asset_file = ROOT / "Content" / (LEGACY.removeprefix("/Game/") + ".uasset")
    before_hash = hashlib.sha256(asset_file.read_bytes()).hexdigest()
    report = {
        "mode": "apply" if apply else ("verify" if verify else "inspect"),
        "asset": LEGACY,
        "canonical_asset": CURRENT,
        "preserved_legacy_id": "Booth_FoodTruck_Taco",
        "previous_cost_key": previous_key,
        "required_cost_key": EXPECTED_COST_KEY,
        "canonical_cost_row": cost_rows[EXPECTED_COST_KEY],
        "referencers": sorted(str(path) for path in
            unreal.EditorAssetLibrary.find_package_referencers_for_asset(LEGACY, False)),
        "before_sha256": before_hash,
        "before_spec": before_spec,
        "changed": False,
    }

    if apply and previous_key != EXPECTED_COST_KEY:
        backup = output / "Before" / asset_file.name
        backup.parent.mkdir(parents=True)
        backup.write_bytes(asset_file.read_bytes())
        legacy_spec.set_editor_property("install_cost_key", unreal.Name(EXPECTED_COST_KEY))
        legacy.set_editor_property("spec", legacy_spec)

        # Restore the changed field on a detached struct copy to verify every other field.
        audit = legacy.get_editor_property("spec").copy()
        audit.set_editor_property("install_cost_key", unreal.Name(previous_key))
        assert audit.export_text() == before_spec, "A non-price specification field changed."
        assert str(legacy.get_editor_property("spec").get_editor_property("install_cost_key")) == EXPECTED_COST_KEY
        assert unreal.EditorAssetLibrary.save_loaded_asset(legacy, only_if_is_dirty=False)
        report["changed"] = True
        report["backup"] = str(backup)

    if apply or verify:
        assert str(legacy.get_editor_property("spec").get_editor_property("install_cost_key")) == EXPECTED_COST_KEY
        assert not legacy.get_editor_property("show_in_build_catalog")
        assert str(legacy.get_editor_property("spec").get_editor_property("facility_definition_id")) == "Booth_FoodTruck_Taco"
    report["after_sha256"] = hashlib.sha256(asset_file.read_bytes()).hexdigest()
    report["after_spec"] = legacy.get_editor_property("spec").export_text()
    (output / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("FF_LEGACY_FACILITY_COST_OK " + str(output / "report.json"))


run()
