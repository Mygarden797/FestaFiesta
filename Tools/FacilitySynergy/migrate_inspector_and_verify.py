"""Add the inspector synergy tab and verify preserved authoring plus idempotence."""
import hashlib
import json
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "Saved/FacilitySynergy"
PANEL = "/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityInfoPanel"
ENTRY = "/Game/FestaFiesta/UI/Facility/WBP_FF_FacilitySynergyEntry"
COMMAND = "FF.MigrateFacilitySynergyTab"


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def file_for(package):
    return ROOT / ("Content/" + package.removeprefix("/Game/") + ".uasset")


def digest(package):
    return hashlib.sha256(file_for(package).read_bytes()).hexdigest()


def margin(value):
    return [value.left, value.top, value.right, value.bottom]


def collect(root):
    result = {}

    def visit(widget):
        result[widget.get_name()] = widget
        if isinstance(widget, unreal.PanelWidget):
            for child in widget.get_all_children():
                visit(child)

    visit(root)
    return result


def snapshot(widgets):
    result = {}
    for name, widget in widgets.items():
        parent = widget.get_parent()
        values = {"class": widget.get_class().get_path_name(),
                  "parent": parent.get_name() if parent else None,
                  "visibility": str(widget.get_visibility())}
        slot = widget.get_editor_property("slot")
        if isinstance(slot, (unreal.HorizontalBoxSlot, unreal.VerticalBoxSlot, unreal.BorderSlot, unreal.ScrollBoxSlot)):
            values["slot_padding"] = margin(slot.get_editor_property("padding"))
        if isinstance(slot, unreal.CanvasPanelSlot):
            values["canvas_offsets"] = margin(slot.get_offsets())
        if isinstance(widget, unreal.TextBlock):
            font = widget.get_editor_property("font")
            values["text"] = str(widget.get_text())
            values["font_size"] = font.get_editor_property("size")
            values["font_typeface"] = str(font.get_editor_property("typeface_font_name"))
            font_object = font.get_editor_property("font_object")
            values["font_asset"] = font_object.get_path_name() if font_object else None
        if isinstance(widget, unreal.Border):
            values["border_padding"] = margin(widget.get_editor_property("padding"))
        result[name] = values
    return result


panel = unreal.load_asset(PANEL)
require(panel and panel.generated_class(), "Existing facility inspector is missing; no migration was requested")
before_tree = unreal.find_object(panel, "WidgetTree")
before_widgets = collect(unreal.find_object(before_tree, "Root_Canvas"))
before_snapshot = snapshot(before_widgets)
before_sha256 = digest(PANEL)

unreal.SystemLibrary.execute_console_command(None, COMMAND)
panel = unreal.load_asset(PANEL)
entry = unreal.load_asset(ENTRY)
require(entry and entry.generated_class() and file_for(ENTRY).is_file(),
        "Synergy entry WBP was not created; check FF_FACILITY_SYNERGY_ENTRY_FAIL or rebuild the Editor")
entry_parent = unreal.load_class(None, "/Script/FestaFiesta.FF_FacilitySynergyEntryWidget")
require(unreal.MathLibrary.class_is_child_of(entry.generated_class(), entry_parent), "Entry WBP has the wrong native parent")
after_widgets = collect(unreal.find_object(unreal.find_object(panel, "WidgetTree"), "Root_Canvas"))
after_snapshot = snapshot(after_widgets)
for name, values in before_snapshot.items():
    require(after_snapshot.get(name) == values, f"Migration altered existing Designer widget {name}")

for name, expected_type in {
    "Button_TabSynergy": unreal.Button,
    "Panel_SynergySection": unreal.VerticalBox,
    "Text_SynergySummary": unreal.CommonTextBlock,
    "VerticalBox_SynergyEntries": unreal.VerticalBox,
}.items():
    require(isinstance(after_widgets.get(name), expected_type), f"Missing inspector widget contract: {name}")
tabs = [widget.get_name() for widget in after_widgets["Panel_InspectorTabs"].get_all_children()]
overview_index = tabs.index("Button_TabOverview")
require(tabs[overview_index:overview_index + 4] == ["Button_TabOverview", "Button_TabSynergy", "Button_TabOperation", "Button_TabUpgrade"],
        "Expected tab order: facility info, synergy, operation, upgrade")
require(isinstance(after_widgets["Panel_Content"].get_parent(), unreal.ScrollBox), "Synergy entries must use the existing scrolling detail area")

entry_widgets = collect(unreal.find_object(unreal.find_object(entry, "WidgetTree"), "Border_EntrySpacing"))
for name in ("Text_Title", "Text_Partner", "Text_Effect", "Text_Condition", "Text_Status"):
    require(isinstance(entry_widgets.get(name), unreal.TextBlock), f"Missing entry field: {name}")
    require(entry_widgets[name].get_editor_property("auto_wrap_text"), f"Entry field must wrap: {name}")

first_hashes = {package: digest(package) for package in (PANEL, ENTRY)}
unreal.SystemLibrary.execute_console_command(None, COMMAND)
second_hashes = {package: digest(package) for package in (PANEL, ENTRY)}
require(first_hashes == second_hashes, "Repeat migration rewrote a Designer asset")

facility_names = {}
for category, asset_name in (
    ("Booth", "PDA_FFZS_Booth_FoodStall_Burger"),
    ("Booth", "PDA_FFZS_Booth_FoodStall_IceCream"),
    ("Rest", "PDA_FFZS_Rest_Seating_Seating"),
    ("Rest", "PDA_FFZS_Rest_Shade_Shade"),
):
    definition = unreal.load_asset(f"/Game/FestaFiesta/Zone/Data/FacilityDefinitions/{category}/{asset_name}")
    require(definition is not None, f"Missing production facility: {asset_name}")
    spec = definition.get_editor_property("spec")
    facility_names[str(spec.get_editor_property("facility_definition_id"))] = str(spec.get_editor_property("display_name"))

OUT.mkdir(parents=True, exist_ok=True)
report = {"status": "PASS", "existing_widgets_preserved": len(before_snapshot),
          "panel_before_sha256": before_sha256, "saved_sha256": first_hashes,
          "repeat_sha256": second_hashes, "tab_order": tabs,
          "facility_display_names": facility_names}
(OUT / "inspector_verification.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("FACILITY_SYNERGY_INSPECTOR_VERIFICATION_PASS: " + json.dumps(facility_names, ensure_ascii=False))
