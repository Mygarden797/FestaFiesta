# migrate_required_badges.py
# 역할: 시설 조합 효과와 표시 작업에서 기존 데이터를 현재 구조로 이관한다.

"""Add required-facility states and verify existing card/synergy authoring is preserved."""
import hashlib
import json
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[2]
PACKAGES = ["/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepBuildTile",
            "/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/Legacy/WBP_FF_BuildFacilityCard"]
COMMAND = "FF.AddBuildRequiredFacilityBadges"


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(package):
    return hashlib.sha256((ROOT / ("Content/" + package.removeprefix("/Game/") + ".uasset")).read_bytes()).hexdigest()


def widgets_for(package):
    blueprint = unreal.load_asset(package)
    require(blueprint and blueprint.generated_class(), f"Missing existing card: {package}")
    tree = unreal.find_object(blueprint, "WidgetTree")
    root = unreal.find_object(tree, "Overlay_SynergyAccent")
    require(root, f"Expected existing synergy overlay: {package}")
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
        if isinstance(widget, unreal.TextBlock):
            for prop in ("text", "font", "color_and_opacity"):
                values[prop] = str(widget.get_editor_property(prop))
        if isinstance(widget, unreal.Border):
            for prop in ("padding", "background"):
                values[prop] = str(widget.get_editor_property(prop))
        slot = widget.get_editor_property("slot")
        if isinstance(slot, (unreal.OverlaySlot, unreal.HorizontalBoxSlot, unreal.VerticalBoxSlot, unreal.BorderSlot)):
            for prop in ("padding", "horizontal_alignment", "vertical_alignment"):
                values["slot_" + prop] = str(slot.get_editor_property(prop))
        result[name] = values
    return result


before = {package: snapshot(widgets_for(package)) for package in PACKAGES}
unreal.SystemLibrary.execute_console_command(None, COMMAND)
report = {}
for package in PACKAGES:
    widgets = widgets_for(package)
    after = snapshot(widgets)
    for name, value in before[package].items():
        require(after.get(name) == value, f"Existing Designer widget changed: {package}/{name}")
    for name in ("Border_RequiredMissing", "Border_RequiredSatisfied"):
        require(isinstance(widgets.get(name), unreal.Border), f"Missing required badge: {name}")
        require(widgets[name].get_visibility() == unreal.SlateVisibility.COLLAPSED, f"Badge visible before data: {name}")
    for name in ("Text_RequiredMissing", "Text_RequiredSatisfied"):
        require(isinstance(widgets.get(name), unreal.TextBlock), f"Missing required text: {name}")
    report[package] = {"preserved_widgets": len(before[package]), "sha256": digest(package)}

unreal.SystemLibrary.execute_console_command(None, COMMAND)
for package in PACKAGES:
    require(digest(package) == report[package]["sha256"], f"Repeat command changed Designer asset: {package}")

festival = unreal.load_asset("/Game/FestaFiesta/Data/Festivals/DA_Festival_Busan")
requirements = []
for requirement in festival.get_editor_property("required_setup_facilities"):
    definition = requirement.get_editor_property("facility_definition")
    if not isinstance(definition, unreal.Object):
        definition = unreal.load_asset(str(definition))
    require(definition, "Required facility reference cannot be resolved")
    spec = definition.get_editor_property("spec")
    requirements.append({"facility": str(spec.get_editor_property("display_name")),
                         "minimum": requirement.get_editor_property("minimum_count"),
                         "maximum": spec.get_editor_property("max_instances_per_world")})
report["festival_requirements"] = requirements
output = ROOT / "Saved/FacilitySynergy/required_badge_verification.json"
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
unreal.log("FACILITY_REQUIRED_BADGES_OK " + str(output))
