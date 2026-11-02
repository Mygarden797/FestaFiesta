"""Import individual HKH icons and update the active, Designer-owned build UI.

Run with UnrealEditor-Cmd /Engine/Maps/Entry -ExecutePythonScript=<this file>.
Creates new textures only, backs up the three edited WBPs, and verifies all
serialized references. It never rebuilds the UI or changes feature topic IDs.
"""
import hashlib
import json
import shutil
import struct
from datetime import datetime, timezone
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
OUT = ROOT / "Saved/BuildModeIcons"
DESTINATION = "/Game/Asset/Icon_HKH"
BASE = "/Game/FestaFiesta/Building/FF_WBP_Building"
PACKAGES = {
    "toolbar": BASE + "/Controls/FeatureGuide/WBP_FF_BuildFeatureGuideToolbar",
    "button": BASE + "/Controls/FeatureGuide/WBP_FF_BuildFeatureGuideButton",
    "panel": BASE + "/Catalog/WBP_FF_PrepPanelBuild",
}
TOOLS = {
    "select": ("MultiSelect", "MultiSelection"), "grid": ("Grid", "BuildGrid"),
    "snap": ("Snap", "BuildSnap"), "ranges": ("Ranges", "FacilityRanges"),
    "settings": ("Settings", "BuildSettings"), "catalog": ("Catalog", "BuildCatalog"),
    "help": ("Help", "BuildControls"), "lighting": ("Lighting", "LightingControls"),
    "codex": ("Codex", "FacilityCodex"), "revenue": ("Revenue", "FacilityRevenueRanking"),
    "facility": ("Facility", "FacilitySelection"), "group": ("Group", "GroupEditing"),
    "preset": ("Preset", "GroupPreset"), "rotate": ("Rotate", "BuildRotation"),
    "adjust": ("Adjust", "DirectAdjustment"), "synergy": ("Synergy", "SynergyPreview"),
    "queue": ("Queue", "FacilityQueue"), "repeat": ("Repeat", "ContinuousPlacement"),
    "program": ("Program", "FacilityPrograms"), "undo": ("Undo", "BuildUndo"),
    "redo": ("Redo", "BuildRedo"), "camera": ("Camera", "BuildCamera"),
}
CATEGORIES = ["Booth", "Toilet", "Info", "Rest", "Performance", "Safety",
              "PhotoSpot", "Lighting", "Road", "Tile"]
ICONS = ["T_HKH_Tool_" + value[0] for value in TOOLS.values()]
ICONS += ["T_HKH_Category_" + value for value in ["All"] + CATEGORIES]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def disk(package):
    return ROOT / "Content" / (package.removeprefix("/Game/") + ".uasset")


def owned_widgets(blueprint):
    return {widget.get_name(): widget for widget in unreal.ObjectIterator(unreal.Widget)
            if widget.get_outer() and widget.get_outer().get_class().get_name() == "WidgetTree"
            and widget.get_outer().get_outer() == blueprint}


def brush(texture, size=32):
    result = unreal.SlateBrush()
    result.set_editor_property("resource_object", texture)
    dimensions = result.get_editor_property("image_size")
    dimensions.set_editor_property("x", float(size))
    dimensions.set_editor_property("y", float(size))
    result.set_editor_property("image_size", dimensions)
    result.set_editor_property("draw_as", unreal.SlateBrushDrawType.IMAGE)
    return result


def save(blueprint):
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False):
        raise RuntimeError("Save failed: " + blueprint.get_path_name())


def different(previous, desired):
    if hasattr(previous, "export_text") and hasattr(desired, "export_text"):
        return previous.export_text() != desired.export_text()
    return previous != desired


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if world.get_path_name() != "/Engine/Maps/Entry.Entry":
        raise RuntimeError("Use the idle Entry map for this migration")
    # Fail before any changes if the production sources or exact widgets are missing.
    sources = {}
    for name in ICONS:
        source = ROOT / "Content/Asset/Icon_HKH" / (name + ".png")
        header = source.read_bytes()[:33]
        if header[:8] != b"\x89PNG\r\n\x1a\n":
            raise RuntimeError("Invalid PNG: " + str(source))
        width, height, depth, colour = struct.unpack(">IIBB", header[16:26])
        if width != height or width < 256 or depth != 8 or colour != 6:
            raise RuntimeError("Expected square RGBA source: " + str(source))
        sources[name] = {"file": str(source), "size": [width, height], "sha256": digest(source)}
    blueprints = {key: unreal.load_asset(path) for key, path in PACKAGES.items()}
    if not all(blueprints.values()):
        raise RuntimeError("Missing production WBP")
    widgets = {key: owned_widgets(value) for key, value in blueprints.items()}
    for suffix, (_, topic) in TOOLS.items():
        guide = widgets["toolbar"]["Guide_" + suffix]
        if str(guide.get_editor_property("feature_topic_id")) != topic:
            raise RuntimeError("Unexpected topic: " + suffix)
    for suffix in ["All"] + [str(index) for index in range(10)]:
        assert "Choice_Category" + suffix in widgets["panel"]
    assert "Button_Guide" in widgets["button"]
    assert "Image_GuideIcon" in widgets["button"]
    backup = OUT / "backups" / datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    before = {}
    for package in PACKAGES.values():
        original = disk(package)
        before[package] = digest(original)
        target = backup / original.relative_to(ROOT)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(original, target)
    textures = {}
    for name, source in sources.items():
        package = DESTINATION + "/" + name
        existed = unreal.EditorAssetLibrary.does_asset_exist(package)
        if existed:
            texture = unreal.load_asset(package)
            if not isinstance(texture, unreal.Texture2D):
                raise RuntimeError("Existing destination is not a texture: " + package)
        else:
            task = unreal.AssetImportTask()
            task.set_editor_property("filename", source["file"])
            task.set_editor_property("destination_path", DESTINATION)
            task.set_editor_property("destination_name", name)
            task.set_editor_property("automated", True)
            task.set_editor_property("replace_existing", False)
            task.set_editor_property("save", False)
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
            texture = unreal.load_asset(package)
        if not isinstance(texture, unreal.Texture2D):
            raise RuntimeError("Texture import failed: " + package)
        settings = {
            "compression_settings": unreal.TextureCompressionSettings.TC_EDITOR_ICON,
            "lod_group": unreal.TextureGroup.TEXTUREGROUP_UI,
            "srgb": True, "never_stream": True,
            "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
            "address_x": unreal.TextureAddress.TA_CLAMP,
            "address_y": unreal.TextureAddress.TA_CLAMP,
            "max_texture_size": 256,
        }
        changed = not existed
        for key, value in settings.items():
            if texture.get_editor_property(key) != value:
                texture.set_editor_property(key, value)
                changed = True
        if changed and not unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False):
            raise RuntimeError("Could not save " + package)
        for key, value in settings.items():
            assert texture.get_editor_property(key) == value, (package, key)
        textures[name] = texture
    changed_blueprints = set()
    def assign(key, widget, property_name, value):
        if different(widget.get_editor_property(property_name), value):
            widget.set_editor_property(property_name, value)
            changed_blueprints.add(key)

    for suffix, (icon, _) in TOOLS.items():
        guide = widgets["toolbar"]["Guide_" + suffix]
        assign("toolbar", guide, "icon_brush", brush(textures["T_HKH_Tool_" + icon]))
        assign("toolbar", guide, "use_icon_atlas", False)
    # White sources inherit the existing tab's dark/white state tints.
    for suffix, icon in [("All", "All")] + list(enumerate(CATEGORIES)):
        choice = widgets["panel"]["Choice_Category" + str(suffix)]
        # The 32px tab has 3px vertical padding: a 24px square fits without squashing.
        assign("panel", choice, "authored_icon", brush(textures["T_HKH_Category_" + icon], 24))
        assign("panel", choice, "icon_atlas", None)
        assign("panel", choice, "preserve_icon_colours", False)
    # The existing Pearl toolbar is light: tint white artwork to deep teal.
    image = widgets["button"]["Image_GuideIcon"]
    assign("button", image, "color_and_opacity", unreal.LinearColor(.027, .10, .12, 1.0))
    button = widgets["button"]["Button_Guide"]
    style = button.get_editor_property("widget_style")
    original_style = style.export_text()
    hovered = style.get_editor_property("hovered")
    outline = hovered.get_editor_property("outline_settings")
    outline.set_editor_property("width", 1.25)
    outline.set_editor_property("color", unreal.SlateColor(specified_color=unreal.LinearColor(.045, .36, .39, .8)))
    hovered.set_editor_property("outline_settings", outline)
    style.set_editor_property("hovered", hovered)
    if style.export_text() != original_style:
        button.set_editor_property("widget_style", style)
        changed_blueprints.add("button")
    for key in ["button", "toolbar", "panel"]:
        if key in changed_blueprints:
            save(blueprints[key])
    # Independent queries of the serialized WBP templates, including unchanged IDs.
    verification = []
    for suffix, (icon, topic) in TOOLS.items():
        guide = owned_widgets(unreal.load_asset(PACKAGES["toolbar"]))["Guide_" + suffix]
        actual = guide.get_editor_property("icon_brush").get_editor_property("resource_object").get_path_name()
        expected = textures["T_HKH_Tool_" + icon].get_path_name()
        assert actual == expected
        assert not guide.get_editor_property("use_icon_atlas")
        assert str(guide.get_editor_property("feature_topic_id")) == topic
        verification.append({"widget": guide.get_name(), "texture": actual, "topic": topic})
    for suffix, icon in [("All", "All")] + list(enumerate(CATEGORIES)):
        choice = owned_widgets(unreal.load_asset(PACKAGES["panel"]))["Choice_Category" + str(suffix)]
        actual = choice.get_editor_property("authored_icon").get_editor_property("resource_object").get_path_name()
        assert actual == textures["T_HKH_Category_" + icon].get_path_name()
        assert choice.get_editor_property("icon_atlas") is None
        assert not choice.get_editor_property("preserve_icon_colours")
        verification.append({"widget": choice.get_name(), "texture": actual})
    result = {"status": "applied_and_verified", "applied_utc": datetime.now(timezone.utc).isoformat(), "texture_count": len(textures),
              "reference_count": len(verification), "backup": str(backup),
              "sources": sources, "references": verification,
              "wbps": {package: {"before": before[package], "after": digest(disk(package))}
                       for package in PACKAGES.values()}}
    (OUT / "apply-result.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("HKH_BUILD_ICONS_VERIFIED: {} textures, {} references".format(len(textures), len(verification)))


if __name__ == "__main__":
    main()
