"""Create missing assets in this plugin only; never rebuild designer-authored assets."""
from pathlib import Path
import hashlib
import json
import unreal

root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
folder = root / "Plugins/FF_MapOverview/Content/UI"
before = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in folder.glob("*.uasset")}
unreal.SystemLibrary.execute_console_command(None, "FF.MapOverview.EnsureDesignerAssets")
names = ("WBP_FF_MapOverview", "WBP_FF_MapMarker", "T_MapHex", "T_MapHouse", "T_MapCamera",
         "T_MapFallback", "M_MapTerrain", "MI_MapTerrain", "M_MapCompassRim", "MI_MapCompassRim", "WBP_FF_WholeGridButton")
assets = [unreal.load_asset("/FF_MapOverview/UI/" + name) for name in names]
assert all(assets), [name for name, asset in zip(names, assets) if not asset]
overview = unreal.load_class(None, "/FF_MapOverview/UI/WBP_FF_MapOverview.WBP_FF_MapOverview_C")
marker = unreal.load_class(None, "/FF_MapOverview/UI/WBP_FF_MapMarker.WBP_FF_MapMarker_C")
assert overview and marker
defaults = unreal.get_default_object(overview)
assert defaults.get_editor_property("facility_marker_class") == marker
assert defaults.get_editor_property("camera_marker_class") == marker
after = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in folder.glob("*.uasset")}
assert all(after.get(name) == value for name, value in before.items()), "Existing designer asset was changed"
out = root / "Saved/MapOverview/DesignerAssets"
out.mkdir(parents=True, exist_ok=True)
(out / "report.json").write_text(json.dumps({"assets": list(names), "existing_assets_preserved": True,
    "overview_class": overview.get_path_name(), "marker_class": marker.get_path_name()}, indent=2), encoding="utf-8")
unreal.log("FF_MAP_UMG_AUTHORING_VERIFIED")
unreal.SystemLibrary.execute_console_command(None, "QUIT_EDITOR")
