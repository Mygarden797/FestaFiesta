"""Add designer-owned combo accents without replacing existing facility cards."""
import hashlib
import json
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[2]
PACKAGES = {
    "/Game/FestaFiesta/UI/PrepHUD/Build/WBP_FF_PrepBuildTile": "SizeBox_Card",
    "/Game/FestaFiesta/Zone/UI/Building/WBP_FF_BuildFacilityCard": "Root_Size",
}
COMMAND = "FF.AddBuildSynergyCardHighlights"


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(package):
    path = ROOT / ("Content/" + package.removeprefix("/Game/") + ".uasset")
    return hashlib.sha256(path.read_bytes()).hexdigest()


def collect(widget):
    result = {widget.get_name(): widget}
    if isinstance(widget, unreal.PanelWidget):
        for child in widget.get_all_children():
            result.update(collect(child))
    return result


def snapshot(root):
    result = {}
    for name, widget in collect(root).items():
        values = {"type": widget.get_class().get_path_name(),
                  "visibility": str(widget.get_visibility())}
        if widget != root:
            values["parent"] = widget.get_parent().get_name()
            slot = widget.get_editor_property("slot")
            if slot:
                values["slot"] = str(slot.get_class().get_path_name())
                for prop in ("padding", "horizontal_alignment", "vertical_alignment"):
                    try:
                        values["slot_" + prop] = str(slot.get_editor_property(prop))
                    except Exception:
                        pass  # Not every slot has these properties.
        if isinstance(widget, unreal.TextBlock):
            values["text"] = str(widget.get_text())
            values["font"] = str(widget.get_editor_property("font"))
            values["color"] = str(widget.get_editor_property("color_and_opacity"))
        if isinstance(widget, unreal.Border):
            values["padding"] = str(widget.get_editor_property("padding"))
            values["background"] = str(widget.get_editor_property("background"))
        if isinstance(widget, unreal.SizeBox):
            values["width"] = widget.get_editor_property("width_override")
            values["height"] = widget.get_editor_property("height_override")
        result[name] = values
    return result


before = {}
for package, original_root in PACKAGES.items():
    blueprint = unreal.load_asset(package)
    require(blueprint and blueprint.generated_class(), f"Missing existing card: {package}")
    tree = unreal.find_object(blueprint, "WidgetTree")
    root = unreal.find_object(tree, original_root)
    require(root, f"Missing original root {original_root}")
    before[package] = snapshot(root)

unreal.SystemLibrary.execute_console_command(None, COMMAND)
report = {}
for package, original_root in PACKAGES.items():
    blueprint = unreal.load_asset(package)
    tree = unreal.find_object(blueprint, "WidgetTree")
    root = unreal.find_object(tree, original_root)
    require(snapshot(root) == before[package], f"Existing card authoring changed: {package}")
    overlay = unreal.find_object(tree, "Overlay_SynergyAccent")
    require(isinstance(overlay, unreal.Overlay), f"Missing accent overlay: {package}")
    require(root.get_parent() == overlay, f"Original card not preserved inside overlay: {package}")
    widgets = collect(overlay)
    for name, expected in {"Border_SynergyHighlight": unreal.Border,
                           "Border_SynergyBadge": unreal.Border,
                           "Text_SynergyBadge": unreal.TextBlock}.items():
        require(isinstance(widgets.get(name), expected), f"Wrong accent binding: {name}")
    require(widgets["Border_SynergyHighlight"].get_visibility() == unreal.SlateVisibility.COLLAPSED,
            "Highlight must be hidden until a matching facility is selected")
    report[package] = {"preserved_widgets": len(before[package]), "sha256": digest(package)}

unreal.SystemLibrary.execute_console_command(None, COMMAND)
for package in PACKAGES:
    require(digest(package) == report[package]["sha256"], f"Repeat command changed existing card: {package}")

output = ROOT / "Saved/FacilitySynergy/catalog_highlight_verification.json"
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
unreal.log("FACILITY_SYNERGY_CATALOG_HIGHLIGHTS_OK " + str(output))
