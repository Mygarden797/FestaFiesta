"""Read disk-saved icon assets and WBP templates in a NEW editor process."""
import json
from pathlib import Path
import runpy
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
spec = runpy.run_path(str(ROOT / "Tools/BuildModeIcons/apply_icons.py"))
PACKAGES, TOOLS = spec["PACKAGES"], spec["TOOLS"]
widgets = {key: spec["owned_widgets"](unreal.load_asset(path)) for key, path in PACKAGES.items()}
references, textures = [], []
for name in spec["ICONS"]:
    texture = unreal.load_asset(spec["DESTINATION"] + "/" + name)
    assert isinstance(texture, unreal.Texture2D), name
    assert texture.get_editor_property("lod_group") == unreal.TextureGroup.TEXTUREGROUP_UI
    assert texture.get_editor_property("compression_settings") == unreal.TextureCompressionSettings.TC_EDITOR_ICON
    assert texture.get_editor_property("never_stream")
    assert texture.get_editor_property("mip_gen_settings") == unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS
    assert texture.get_editor_property("srgb")
    assert texture.get_editor_property("address_x") == unreal.TextureAddress.TA_CLAMP
    assert texture.get_editor_property("address_y") == unreal.TextureAddress.TA_CLAMP
    assert texture.get_editor_property("max_texture_size") == 256
    size = [int(texture.blueprint_get_size_x()), int(texture.blueprint_get_size_y())]
    assert 0 < size[0] <= 256 and size[0] == size[1], (name, size)
    textures.append({"package": texture.get_path_name(), "built_size": size})
for suffix, (icon, topic) in TOOLS.items():
    widget = widgets["toolbar"]["Guide_" + suffix]
    expected = spec["DESTINATION"] + "/T_HKH_Tool_" + icon
    actual = widget.get_editor_property("icon_brush").get_editor_property("resource_object").get_path_name().split(".")[0]
    assert actual == expected, (suffix, actual)
    assert not widget.get_editor_property("use_icon_atlas")
    assert str(widget.get_editor_property("feature_topic_id")) == topic
    assert widget.get_editor_property("enable_icon_motion")
    references.append({"widget": widget.get_name(), "package": actual, "topic": topic})
for suffix, icon in [("All", "All")] + list(enumerate(spec["CATEGORIES"])):
    widget = widgets["panel"]["Choice_Category" + str(suffix)]
    actual = widget.get_editor_property("authored_icon").get_editor_property("resource_object").get_path_name().split(".")[0]
    assert actual == spec["DESTINATION"] + "/T_HKH_Category_" + icon
    assert widget.get_editor_property("icon_atlas") is None
    assert not widget.get_editor_property("preserve_icon_colours")
    dimensions = widget.get_editor_property("authored_icon").get_editor_property("image_size")
    assert dimensions.get_editor_property("x") == 24 and dimensions.get_editor_property("y") == 24
    references.append({"widget": widget.get_name(), "package": actual})
image = widgets["button"]["Image_GuideIcon"]
assert image.get_editor_property("color_and_opacity").r < .1
result = {"status": "PASS", "fresh_process": True, "textures": textures, "references": references}
(ROOT / "Saved/BuildModeIcons/disk-verification.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
unreal.log("HKH_BUILD_ICONS_DISK_VERIFIED: 33 textures and 33 references")
