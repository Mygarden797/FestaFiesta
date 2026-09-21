"""Fresh-session read-only checks for the narrowly scoped warning repairs."""
import json
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "Artifacts" / "WarningRepair"
BASE = "/Game/FestaFiesta/Building/FF_Data_Building/Booth/"
legacy = unreal.load_asset(BASE + "PDA_FFZS_Booth_FoodTruck_Taco")
current = unreal.load_asset(BASE + "PDA_FFZS_Booth_FoodStall_Taco")
assert legacy and current
assert not legacy.get_editor_property("show_in_build_catalog")
assert current.get_editor_property("show_in_build_catalog")
assert str(legacy.get_editor_property("spec").get_editor_property("facility_definition_id")) == "Booth_FoodTruck_Taco"
assert str(current.get_editor_property("spec").get_editor_property("facility_definition_id")) == "Booth_FoodStall_Taco"
validation = {}
for label, asset in [("legacy", legacy), ("current", current)]:
    # Unreal Python exposes this bool + out-array function as Array[Text] or None.
    errors = asset.is_definition_valid()
    assert errors is not None and len(errors) == 0, str(errors)
    validation[label] = True

guide = unreal.load_asset("/Game/FestaFiesta/Building/FF_WBP_Building/Guide/WBP_FF_BuildModeGuide")
assert guide and guide.generated_class()
assert unreal.MathLibrary.class_is_child_of(
    guide.generated_class(), unreal.load_class(None, "/Script/FestaFiesta.FF_BuildModeGuide"))
scene_class = unreal.load_class(None, "/Script/FestaFiesta.FF_SurfingSceneActor")
scene_default = unreal.get_default_object(scene_class)
assert scene_default
report = {
    "definition_validation": validation,
    "legacy_hidden_and_identity_preserved": True,
    "replacement_available": True,
    "guide_loads_with_expected_parent": True,
    "surfing_class_default_loads": True,
}
OUTPUT.mkdir(parents=True, exist_ok=True)
(OUTPUT / "owned-asset-verification.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
unreal.log("FF_OWNED_WARNING_VERIFY_OK " + json.dumps(report))
