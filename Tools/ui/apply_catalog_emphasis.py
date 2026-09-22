"""Apply and verify the one-time UMG catalog emphasis migration.

Run in a freshly built, full Unreal Editor with -ExecutePythonScript (not a
Python commandlet). Only the two facility cards and the Prep build panel are
saved. Backups, structural preservation checks, and a second idempotent run are
included. The C++ command preserves all later Designer edits by version tag.
"""

import hashlib
import json
import shutil
import time
import traceback
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Artifacts" / "Validation" / "BuildCatalogEmphasis"
OUT.mkdir(parents=True, exist_ok=True)
BACKUP = OUT / "Backups" / time.strftime("%Y%m%d-%H%M%S")
COMMAND = "FF.RefreshBuildCatalogEmphasisUMG"
VERSION_KEY = "FF.BuildCatalog.EmphasisVersion"
VERSION = "20260920.v1"
GUIDE_VERSION_KEY = "FF.BuildCatalog.RequiredGuideVersion"
PACKAGES = {
    "/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepBuildTile": "SizeBox_Card",
    "/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/Legacy/WBP_FF_BuildFacilityCard": "Root_Size",
    "/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepPanelBuild": "Border_ApprovedCatalogV9",
}
MIGRATED_WIDGETS = {
    "Overlay_RequiredStatus", "Border_RequiredMissing", "Border_RequiredSatisfied",
    "Text_RequiredMissing", "Text_RequiredSatisfied", "Text_StudioHint",
}
_state = {"next": time.monotonic() + 3.0, "phase": 0, "assets": {}, "busy": False}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def asset_file(package):
    return ROOT / "Content" / (package.removeprefix("/Game/") + ".uasset")


def digest(package):
    return hashlib.sha256(asset_file(package).read_bytes()).hexdigest()


def load_widgets(package):
    blueprint = unreal.load_asset(package)
    require(blueprint and blueprint.generated_class(), "Missing compiled WBP: " + package)
    tree = unreal.find_object(blueprint, "WidgetTree")
    root = unreal.find_object(tree, PACKAGES[package])
    require(root, "Missing preserved root: " + package)
    while root.get_parent():
        root = root.get_parent()
    widgets = {}

    def visit(widget):
        widgets[widget.get_name()] = widget
        if isinstance(widget, unreal.PanelWidget):
            for child in widget.get_all_children():
                visit(child)

    visit(root)
    return blueprint, widgets


def snapshot(widgets):
    result = {}
    for name, widget in widgets.items():
        parent = widget.get_parent()
        values = {
            "type": widget.get_class().get_path_name(),
            "parent": parent.get_name() if parent else None,
            "visibility": str(widget.get_visibility()),
        }
        for widget_type, properties in (
            (unreal.TextBlock, ("text", "font", "color_and_opacity")),
            (unreal.Border, ("padding", "background")),
            (unreal.SizeBox, ("width_override", "height_override")),
            (unreal.Button, ("widget_style",)),
        ):
            if isinstance(widget, widget_type):
                for prop in properties:
                    values[prop] = str(widget.get_editor_property(prop))
        slot = widget.get_editor_property("slot")
        if isinstance(slot, (unreal.OverlaySlot, unreal.HorizontalBoxSlot,
                             unreal.VerticalBoxSlot, unreal.BorderSlot)):
            for prop in ("padding", "horizontal_alignment", "vertical_alignment"):
                values["slot_" + prop] = str(slot.get_editor_property(prop))
        result[name] = values
    return result


def verify(package):
    blueprint, widgets = load_widgets(package)
    before = _state["assets"][package]
    require(unreal.EditorAssetLibrary.get_metadata_tag(blueprint, VERSION_KEY) == VERSION,
            "Migration did not finish: " + package)
    after = snapshot(widgets)
    preserved = 0
    for name, original in before["widgets"].items():
        if not before["already_migrated"] and name in MIGRATED_WIDGETS:
            continue
        if not before["guide_flow_migrated"] and name in (
            "Border_CatalogRequiredGuide", "Border_CatalogProgramLegend"
        ):
            # Only the inter-chip gap changes owners: the required instruction
            # takes the trailing gap so both collapse together after installation.
            unchanged_before = {k: v for k, v in original.items() if k != "slot_padding"}
            unchanged_after = {k: v for k, v in after[name].items() if k != "slot_padding"}
            require(unchanged_before == unchanged_after, "Guide styling changed: " + name)
            continue
        require(after.get(name) == original, "Unrelated Designer widget changed: " + package + "/" + name)
        preserved += 1

    if package.endswith("WBP_FF_PrepPanelBuild"):
        require(unreal.EditorAssetLibrary.get_metadata_tag(blueprint, GUIDE_VERSION_KEY) == "1",
                "Collapsible guide migration did not finish")
        require(widgets["Border_CatalogProgramLegend"].get_editor_property("slot").get_editor_property("padding").left == 0,
                "Program legend must align left when the requirement collapses")
        expected = {
            "HorizontalBox_CatalogEmphasisGuide": unreal.HorizontalBox,
            "Border_CatalogRequiredGuide": unreal.Border,
            "Border_CatalogProgramLegend": unreal.Border,
            "Text_CatalogProgramLegend": unreal.TextBlock,
            "Text_CatalogProgramGuide": unreal.TextBlock,
        }
    else:
        expected = {
            "Border_RequiredMissing": unreal.Border,
            "Border_RequiredSatisfied": unreal.Border,
            "Border_ProgramBadge": unreal.Border,
            "Text_RequiredMissing": unreal.TextBlock,
            "Text_RequiredSatisfied": unreal.TextBlock,
            "Text_ProgramBadge": unreal.TextBlock,
        }
    for name, expected_type in expected.items():
        require(isinstance(widgets.get(name), expected_type), "Missing emphasis widget: " + package + "/" + name)

    if not package.endswith("WBP_FF_PrepPanelBuild"):
        # UWidget::bIsVariable is not exposed to Unreal's Python reflection.
        # The native migration validates it before any save; compiled widget
        # contract tests verify that the resulting runtime bindings resolve.
        if not before["already_migrated"]:
            for name in ("Border_RequiredMissing", "Border_RequiredSatisfied", "Border_ProgramBadge"):
                require(widgets[name].get_visibility() == unreal.SlateVisibility.COLLAPSED,
                        "Badge must wait for facility data: " + name)
            for name, caption in {
                "Text_RequiredMissing": "1일차 필수 설치",
                "Text_RequiredSatisfied": "1일차 필수 완료",
                "Text_ProgramBadge": "프로그램",
            }.items():
                require(str(widgets[name].get_text()) == caption, "Incorrect default badge caption: " + name)
    before["preserved_widgets"] = preserved
    before["after_sha256"] = digest(package)


def finish(error=None):
    unreal.unregister_slate_post_tick_callback(_callback)
    report = {
        "command": COMMAND,
        "version": VERSION,
        "error": error,
        "backup": str(BACKUP),
        "assets": _state["assets"],
        "verification": "Run PrepBuild automation with a rendered RHI after restarting the editor.",
    }
    (OUT / "asset-migration.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    if error:
        unreal.log_error("FF_BUILD_CATALOG_EMPHASIS_MIGRATION_FAIL: " + error)
    else:
        unreal.log("FF_BUILD_CATALOG_EMPHASIS_MIGRATION_OK: " + str(OUT / "asset-migration.json"))
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)


def tick(_delta):
    if _state["busy"] or time.monotonic() < _state["next"]:
        return
    _state["busy"] = True
    try:
        if _state["phase"] == 0:
            for package in PACKAGES:
                source = asset_file(package)
                require(source.is_file(), "Required asset is missing: " + package)
                blueprint, widgets = load_widgets(package)
                _state["assets"][package] = {
                    "before_sha256": digest(package),
                    "already_migrated": unreal.EditorAssetLibrary.get_metadata_tag(blueprint, VERSION_KEY) == VERSION,
                    "guide_flow_migrated": unreal.EditorAssetLibrary.get_metadata_tag(blueprint, GUIDE_VERSION_KEY) == "1",
                    "widgets": snapshot(widgets),
                }
                backup = BACKUP / source.relative_to(ROOT)
                backup.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, backup)
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            unreal.SystemLibrary.execute_console_command(world, COMMAND)
            for package in PACKAGES:
                verify(package)
            _state["phase"] = 1
            _state["next"] = time.monotonic() + 1.0
        else:
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            unreal.SystemLibrary.execute_console_command(world, COMMAND)
            for package in PACKAGES:
                require(digest(package) == _state["assets"][package]["after_sha256"],
                        "Second command changed saved Designer asset: " + package)
            finish()
    except Exception:
        finish(traceback.format_exc())
    finally:
        _state["busy"] = False


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
_callback = unreal.register_slate_post_tick_callback(tick)
