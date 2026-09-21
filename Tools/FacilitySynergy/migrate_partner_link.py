"""Add the partner-name button while preserving the existing row's authored layout."""
import hashlib
import json
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[2]
PACKAGE = "/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilitySynergyEntry"
COMMAND = "FF.MigrateFacilitySynergyPartnerLink"


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def load_widgets():
    blueprint = unreal.load_asset(PACKAGE)
    require(blueprint and blueprint.generated_class(), "Existing synergy row WBP is missing")
    tree = unreal.find_object(blueprint, "WidgetTree")
    root = unreal.find_object(tree, "Border_EntrySpacing")
    require(root, "Existing synergy row root is missing")
    result = {}

    def visit(widget):
        result[widget.get_name()] = widget
        if isinstance(widget, unreal.PanelWidget):
            for child in widget.get_all_children():
                visit(child)
    visit(root)
    return result


def slot_values(widget):
    slot = widget.get_editor_property("slot")
    if not slot:
        return None
    return {prop: str(slot.get_editor_property(prop))
            for prop in ("padding", "horizontal_alignment", "vertical_alignment")}


def snapshot(widgets):
    result = {}
    for name, widget in widgets.items():
        parent = widget.get_parent()
        values = {"class": widget.get_class().get_path_name(),
                  "parent": parent.get_name() if parent else None,
                  "visibility": str(widget.get_visibility()), "slot": slot_values(widget)}
        if isinstance(widget, unreal.TextBlock):
            for prop in ("text", "font", "color_and_opacity"):
                values[prop] = str(widget.get_editor_property(prop))
        if isinstance(widget, unreal.Border):
            for prop in ("padding", "background"):
                values[prop] = str(widget.get_editor_property(prop))
        result[name] = values
    return result


def digest():
    return hashlib.sha256((ROOT / ("Content/" + PACKAGE.removeprefix("/Game/") + ".uasset")).read_bytes()).hexdigest()


before = snapshot(load_widgets())
unreal.SystemLibrary.execute_console_command(None, COMMAND)
widgets = load_widgets()
after = snapshot(widgets)
button = widgets.get("Button_OpenPartnerCatalog")
require(isinstance(button, unreal.Button), "Partner button was not added")
require(button.get_content() == widgets["Text_Partner"], "Partner name is not button content")
for name, original in before.items():
    current = dict(after[name])
    if name == "Text_Partner" and original["parent"] != "Button_OpenPartnerCatalog":
        require(slot_values(button) == original["slot"], "Partner's original layout slot changed")
        current["parent"], current["slot"] = original["parent"], original["slot"]
    if name == "Border_EntrySpacing":
        require(widgets[name].get_visibility() == unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE,
                "Root still blocks child input")
        current["visibility"] = original["visibility"]
    require(current == original, f"Existing Designer settings changed: {name}")
first_hash = digest()
unreal.SystemLibrary.execute_console_command(None, COMMAND)
require(digest() == first_hash, "Repeat migration changed the existing WBP")
report = {"package": PACKAGE, "preserved_widgets": len(before), "sha256": first_hash,
          "allowed_changes": ["Text_Partner wrapped in Button_OpenPartnerCatalog", "Root allows child input"],
          "repeat_preserves_asset": True}
output = ROOT / "Saved/FacilitySynergy/partner_link_verification.json"
output.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
unreal.log("FACILITY_PARTNER_LINK_OK " + str(output))
