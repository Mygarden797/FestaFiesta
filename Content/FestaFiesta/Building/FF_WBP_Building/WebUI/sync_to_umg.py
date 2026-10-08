"""Synchronize catalog-ribbon.html into the existing Building catalog widget.

Designer workflow: edit the HTML beside this file, stop PIE, and execute this
script through Unreal's Python console or a rendering-enabled editor commandlet.
The approved visual reference is Design/UXUI/Building_Review_20261007_v1.
Only WBP_FF_PrepPanelBuild is saved. Its original native widgets are retained.
Backups, embedded-data reports, and results go to Saved/BuildingWebUI20261008.
The native FF_BuildCatalogWebView editor helpers must be compiled first.
"""

import base64
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil
import struct
import traceback

import unreal


# Thumbnail authoring settings. These bound embedded PNGs, not the card layout;
# the visible image frame and card dimensions are edited in catalog-ribbon.html.
THUMBNAIL_MAX_DIMENSION = 256
THUMBNAIL_MIN_DIMENSION = 1
THUMBNAIL_MAX_DATA_URI_CHARACTERS = 2 * 1024 * 1024
THUMBNAIL_PNG_PREFIX = "data:image/png;base64,"
THUMBNAIL_PNG_HEADER_BYTES = 24
FONT_MAX_DATA_URI_CHARACTERS = 32 * 1024 * 1024
REFRESH_INTERVAL_SECONDS = 0.1

CATALOG_PACKAGE = "/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepPanelBuild"
DEFINITION_ROOT = "/Game/FestaFiesta/Building/FF_Data_Building"
DEFINITION_CLASS_PACKAGE = "/Script/FF_ZoneSystem"
DEFINITION_CLASS_NAME = "FFZSFacilityDefinition"
FONT_PACKAGE = "/Game/Asset/Font/NEXON_Lv2_Gothic_OTF_Font"
FONT_TYPEFACE = "None"
GOLD_ICON_TOKEN = "__FF_HUD_GOLD_ICON__"
# Shared authoring source also imported by Tools/ui/import_star_gold_coin.py.
GOLD_ICON_RELATIVE = "Tools/ui/assets/StarEmbossedGoldCoin.png"
WEB_NAME = "Web_CatalogRibbon"
HOST_NAME = "Overlay_CatalogCards"
EMPTY_NAME = "Panel_Empty"
FALLBACK_NAME = "TileView_Facilities"
SYNC_VERSION = "2"
METADATA_KEY = "FF.Building.WebCatalog.SyncVersion"
REFERENCE_RELATIVE = "Design/UXUI/Building_Review_20261007_v1"
PACKAGE_SUFFIXES = (".uasset", ".uexp", ".ubulk", ".uptnl")

WEB_DIRECTORY = Path(__file__).resolve().parent
ROOT = WEB_DIRECTORY.parents[4]
OUTPUT = ROOT / "Saved" / "BuildingWebUI20261008"
HTML_FILE = WEB_DIRECTORY / "catalog-ribbon.html"
CATALOG_FILE = ROOT / "Content" / (CATALOG_PACKAGE[len("/Game/"):] + ".uasset")
RUN_DIRECTORY = OUTPUT / datetime.now(timezone.utc).strftime("sync-%Y%m%d-%H%M%S-%f")


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def write_json(path, value):
    path = Path(path).resolve()
    require(path.is_relative_to(OUTPUT.resolve()), "Report path escaped the assigned Saved directory")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2, sort_keys=True), encoding="utf-8")


def protected_hashes():
    """Hash all project Content, including teammate UI, plus implementation inputs."""
    paths = set()
    for relative in ("Content", "Source", "Config"):
        paths.update(path for path in (ROOT / relative).rglob("*") if path.is_file())
    project_file = ROOT / "FestaFiesta.uproject"
    if project_file.is_file():
        paths.add(project_file)
    # Exclude generated plugin Binaries/Intermediate while protecting its inputs.
    for plugin in (ROOT / "Plugins").rglob("*.uplugin"):
        paths.add(plugin)
        for folder in ("Source", "Content", "Config"):
            paths.update(path for path in (plugin.parent / folder).rglob("*") if path.is_file())
    return {path.relative_to(ROOT).as_posix(): digest_file(path) for path in sorted(paths)}


def target_package_files():
    return [CATALOG_FILE.with_suffix(suffix) for suffix in PACKAGE_SUFFIXES
            if CATALOG_FILE.with_suffix(suffix).is_file()]


def backup_package():
    backups = {}
    for original in target_package_files():
        destination = RUN_DIRECTORY / "Backup" / original.relative_to(ROOT)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(original, destination)
        require(digest_file(destination) == digest_file(original), "Backup hash mismatch: " + str(original))
        backups[original] = destination
    require(CATALOG_FILE in backups, "The existing catalog package was not backed up")
    return backups


def restore_target_if_changed(backups):
    restored = []
    for original, backup in backups.items():
        if not original.is_file() or digest_file(original) != digest_file(backup):
            shutil.copy2(backup, original)
            require(digest_file(original) == digest_file(backup), "Catalog backup restore failed")
            restored.append(original.relative_to(ROOT).as_posix())
    for created in target_package_files():
        if created not in backups:
            # Exact package sidecars only; never remove directories or other files.
            require(created.parent == CATALOG_FILE.parent and created.stem == CATALOG_FILE.stem,
                    "Unexpected recovery path")
            created.unlink()
            restored.append(created.relative_to(ROOT).as_posix())
    return restored


def soft_path(value):
    if value is None:
        return ""
    try:
        result = unreal.SystemLibrary.conv_soft_object_reference_to_string(value)
    except (TypeError, AttributeError):
        result = value.get_path_name() if hasattr(value, "get_path_name") else str(value)
    return "" if str(result) in ("", "None") else str(result)


def current_definitions():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(synchronous_search=True)
    registry.wait_for_completion()
    definitions = registry.get_assets_by_class(
        unreal.TopLevelAssetPath(DEFINITION_CLASS_PACKAGE, DEFINITION_CLASS_NAME),
        search_sub_classes=True,
    )
    records = []
    seen = set()
    for data in sorted(definitions, key=lambda entry: str(entry.package_name)):
        if not str(data.package_name).startswith(DEFINITION_ROOT + "/"):
            continue
        definition = data.get_asset()
        require(isinstance(definition, unreal.FFZSFacilityDefinition),
                "Cannot load facility definition: " + str(data.package_name))
        identity = str(definition.get_editor_property("spec").get_editor_property("facility_definition_id"))
        require(identity not in ("", "None"), "Facility definition has no ID: " + definition.get_path_name())
        require(identity.casefold() not in seen, "Duplicate facility ID: " + identity)
        seen.add(identity.casefold())
        records.append({
            "facility_id": identity,
            "definition": definition.get_path_name(),
            "show_in_build_catalog": bool(definition.get_editor_property("show_in_build_catalog")),
            "catalog_thumbnail": soft_path(definition.get_editor_property("catalog_thumbnail")),
        })
    require(records, "No current Building facility definitions were found")
    return records


def embedded_thumbnails(records):
    cache, uri_map = {}, {}
    for record in records:
        if not record["show_in_build_catalog"]:
            record["status"] = "hidden"
            continue
        path = record["catalog_thumbnail"]
        if not path:
            record["status"] = "missing_optional_thumbnail"
            continue
        texture = unreal.load_asset(path)
        require(isinstance(texture, unreal.Texture2D), "Thumbnail is not a Texture2D: " + path)
        canonical_path = texture.get_path_name()
        if canonical_path not in cache:
            uri = unreal.FF_BuildCatalogWebView.get_embedded_thumbnail_data_uri(texture, THUMBNAIL_MAX_DIMENSION)
            require(uri.startswith(THUMBNAIL_PNG_PREFIX), "Native thumbnail helper returned no PNG: " + path)
            require(len(uri) <= THUMBNAIL_MAX_DATA_URI_CHARACTERS, "Embedded thumbnail exceeds native URI limit: " + path)
            png = base64.b64decode(uri[len(THUMBNAIL_PNG_PREFIX):], validate=True)
            require(len(png) >= THUMBNAIL_PNG_HEADER_BYTES and png[:8] == b"\x89PNG\r\n\x1a\n"
                    and png[12:16] == b"IHDR", "Invalid embedded PNG: " + path)
            width, height = struct.unpack(">II", png[16:24])
            require(THUMBNAIL_MIN_DIMENSION <= min(width, height)
                    and max(width, height) <= THUMBNAIL_MAX_DIMENSION,
                    "Native helper did not bound thumbnail dimensions: " + path)
            cache[canonical_path] = (uri, {
                "texture": canonical_path, "width": width, "height": height,
                "png_bytes": len(png), "data_uri_characters": len(uri),
                "png_sha256": hashlib.sha256(png).hexdigest(),
            })
        uri, image_record = cache[canonical_path]
        uri_map[record["facility_id"]] = uri
        record.update(image_record)
        record["status"] = "embedded"
    # Catalog membership can evolve; do not freeze a count from an older export.
    require(any(record["show_in_build_catalog"] for record in records), "The current catalog has no visible definitions")
    return uri_map, cache


def embedded_document():
    source = HTML_FILE.read_text(encoding="utf-8-sig")
    require(source.count(GOLD_ICON_TOKEN) == 1, "HTML must reference the shared HUD gold icon once")
    gold_png = (ROOT / GOLD_ICON_RELATIVE).read_bytes()
    require(gold_png.startswith(b"\x89PNG\r\n\x1a\n"), "Shared HUD gold icon is not a PNG")
    source = source.replace(GOLD_ICON_TOKEN, "data:image/png;base64," + base64.b64encode(gold_png).decode("ascii"))
    require("font-src data:" in source and "FFBuildingFont" in source and "<style>" in source,
            "HTML must declare FFBuildingFont, a style block, and font-src data: in its CSP")
    require("BEGIN FF_BUILDING_EMBEDDED_FONT" not in source,
            "Edit the source HTML; do not replace it with an already embedded DocumentHTML copy")
    font = unreal.load_asset(FONT_PACKAGE)
    require(isinstance(font, unreal.Font), "The existing project font could not be loaded")
    font_uri = unreal.FF_BuildCatalogWebView.get_embedded_font_data_uri(font, unreal.Name(FONT_TYPEFACE))
    require(font_uri.startswith("data:font/otf;base64,")
            and len(font_uri) <= FONT_MAX_DATA_URI_CHARACTERS, "Project font bytes are unavailable or too large")
    embedded = ("\n/* BEGIN FF_BUILDING_EMBEDDED_FONT */\n"
                '@font-face{font-family:"FFBuildingFont";src:url("' + font_uri
                + '");font-weight:400;font-style:normal;}\n'
                "/* END FF_BUILDING_EMBEDDED_FONT */\n")
    return source.replace("<style>", "<style>" + embedded, 1), font_uri


def widgets_for(blueprint):
    return {
        widget.get_name(): widget for widget in unreal.ObjectIterator(unreal.Widget)
        if widget.get_outer() and widget.get_outer().get_class().get_name() == "WidgetTree"
        and widget.get_outer().get_outer() == blueprint
    }


def native_structure(widgets):
    """Capture existing native names, parents, order, classes, and visibility."""
    result = {}
    for name, widget in widgets.items():
        if name == WEB_NAME:
            continue
        parent = widget.get_parent()
        record = {"class": widget.get_class().get_path_name(),
                  "parent": parent.get_name() if parent else None,
                  "visibility": str(widget.get_visibility())}
        if isinstance(widget, unreal.PanelWidget):
            record["children"] = [child.get_name() for child in widget.get_all_children()
                                  if child.get_name() != WEB_NAME]
        result[name] = record
    return result


def property_matches(widget, name, expected):
    actual = widget.get_editor_property(name)
    if name == "thumbnail_data_uris":
        return dict(actual) == expected
    if name == "refresh_interval":
        # Unreal stores float32; Python's 0.1 is a different float64 value.
        return abs(float(actual) - float(expected)) <= 0.000001
    return actual == expected


def apply_document(html, uri_map):
    blueprint = unreal.load_asset(CATALOG_PACKAGE)
    require(isinstance(blueprint, unreal.WidgetBlueprint), "Catalog is not an existing WidgetBlueprint")
    widgets = widgets_for(blueprint)
    require(all(name in widgets for name in (HOST_NAME, EMPTY_NAME, FALLBACK_NAME)),
            "Expected catalog host, empty panel, or native fallback widget is missing")
    host, empty = widgets[HOST_NAME], widgets[EMPTY_NAME]
    require(isinstance(host, unreal.Overlay) and empty.get_parent() == host,
            "Catalog overlay hierarchy changed; adapt the sync script before applying")
    native_before = native_structure(widgets)
    web = widgets.get(WEB_NAME)
    require(web is None or isinstance(web, unreal.FF_BuildCatalogWebView), "Web widget name is occupied by another class")
    expected = {
        "document_html": html, "thumbnail_data_uris": uri_map,
        "supports_transparency": True, "refresh_interval": REFRESH_INTERVAL_SECONDS,
        "native_fallback_widget_name": unreal.Name(FALLBACK_NAME), "use_native_fallback_on_error": True,
    }
    changed = web is None or (web is not None and web.get_parent() != host)
    if web is not None:
        changed |= any(not property_matches(web, key, value) for key, value in expected.items())
        changed |= web.get_visibility() != unreal.SlateVisibility.VISIBLE
        if web.get_parent() == host:
            slot = web.get_editor_property("slot")
            changed |= slot.get_editor_property("horizontal_alignment") != unreal.HorizontalAlignment.H_ALIGN_FILL
            changed |= slot.get_editor_property("vertical_alignment") != unreal.VerticalAlignment.V_ALIGN_FILL
            require(host.get_child_index(web) < host.get_child_index(empty),
                    "Existing web ribbon must stay below the native empty-state overlay")
    changed |= str(unreal.EditorAssetLibrary.get_metadata_tag(blueprint, METADATA_KEY)) != SYNC_VERSION
    if not changed:
        return {"asset_saved": False, "change": "already_current", "native_widget_count": len(native_before)}

    blueprint.modify()
    if web is None:
        web = unreal.new_object(unreal.FF_BuildCatalogWebView, outer=host.get_outer(), name=WEB_NAME)
    if web.get_parent() != host:
        require(host.get_child_at(host.get_children_count() - 1) == empty,
                "The native empty-state overlay must be the last existing child")
        empty_slot = empty.get_editor_property("slot")
        saved_empty_slot = {name: empty_slot.get_editor_property(name)
                            for name in ("padding", "horizontal_alignment", "vertical_alignment")}
        empty.remove_from_parent()
        if web.get_parent():
            web.remove_from_parent()
        host.add_child_to_overlay(web)
        restored_slot = host.add_child_to_overlay(empty)
        for name, value in saved_empty_slot.items():
            restored_slot.set_editor_property(name, value)
    web_slot = web.get_editor_property("slot")
    web_slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
    web_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
    for name, value in expected.items():
        web.set_editor_property(name, value)
    web.set_visibility(unreal.SlateVisibility.VISIBLE)
    unreal.EditorAssetLibrary.set_metadata_tag(blueprint, METADATA_KEY, SYNC_VERSION)
    require(native_structure(widgets_for(blueprint)) == native_before, "A pre-existing native widget changed")
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    # Compilation can replace WidgetTree instances. Never inspect cached widgets.
    blueprint = unreal.load_asset(CATALOG_PACKAGE)
    fresh_widgets = widgets_for(blueprint)
    require(native_structure(fresh_widgets) == native_before, "Compile changed pre-existing native widget structure")
    require(WEB_NAME in fresh_widgets and isinstance(fresh_widgets[WEB_NAME], unreal.FF_BuildCatalogWebView),
            "The compiled catalog lost its web ribbon")
    require(blueprint.generated_class() is not None, "The catalog has no generated class after compile")
    require(unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False), "Catalog save failed")
    return {"asset_saved": True, "change": "updated", "native_widget_count": len(native_before),
            "generated_class": blueprint.generated_class().get_path_name()}


def run():
    require(WEB_DIRECTORY == (ROOT / "Content/FestaFiesta/Building/FF_WBP_Building/WebUI").resolve(),
            "Run the sync script from its assigned Building WebUI directory")
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
    require(project == ROOT, "The active editor project does not match this script")
    require(CATALOG_FILE.is_file() and HTML_FILE.is_file(), "The existing catalog asset or editable HTML is missing")
    require(not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Stop PIE before synchronizing")
    require(hasattr(unreal.FF_BuildCatalogWebView, "get_embedded_thumbnail_data_uri")
            and hasattr(unreal.FF_BuildCatalogWebView, "get_embedded_font_data_uri"),
            "Compile the native Building WebUI editor helpers before synchronizing")
    report = {
        "status": "RUNNING", "asset": CATALOG_PACKAGE, "source": str(HTML_FILE),
        "approved_reference": REFERENCE_RELATIVE, "thumbnail_max_dimension": THUMBNAIL_MAX_DIMENSION,
        "backup_directory": str(RUN_DIRECTORY / "Backup"),
    }
    before, backups = {}, {}
    try:
        before = protected_hashes()
        write_json(RUN_DIRECTORY / "before-hashes.json", before)
        backups = backup_package()
        records = current_definitions()
        uri_map, cache = embedded_thumbnails(records)
        html, font_uri = embedded_document()
        report.update(apply_document(html, uri_map))
        after = protected_hashes()
        write_json(RUN_DIRECTORY / "after-hashes.json", after)
        allowed = {CATALOG_FILE.with_suffix(suffix).relative_to(ROOT).as_posix() for suffix in PACKAGE_SUFFIXES}
        unexpected = [name for name in sorted(before.keys() | after.keys())
                      if name not in allowed and before.get(name) != after.get(name)]
        report["unexpected_changes"] = unexpected
        require(not unexpected, "Protected files changed during synchronization: " + ", ".join(unexpected))
        report.update({
            "status": "PASS", "definition_count": len(records),
            "visible_definition_count": sum(record["show_in_build_catalog"] for record in records),
            "thumbnail_count": len(uri_map), "unique_thumbnail_count": len(cache),
            "unique_thumbnail_uri_characters": sum(len(item[0]) for item in cache.values()),
            "font_uri_characters": len(font_uri), "html_characters": len(html),
            "html_source_sha256": digest_file(HTML_FILE), "uasset_bytes": CATALOG_FILE.stat().st_size,
            "protected_file_count": len(set(before) - allowed),
            "missing_optional_thumbnails": [record["facility_id"] for record in records
                                            if record["status"] == "missing_optional_thumbnail"],
            "entries": records,
        })
        write_json(RUN_DIRECTORY / "thumbnail-data-uris.json", uri_map)
    except Exception:
        report.update(status="FAIL", error=traceback.format_exc())
        if backups:
            try:
                report["restored_catalog_files"] = restore_target_if_changed(backups)
            except Exception:
                report["recovery_error"] = traceback.format_exc()
        unreal.log_error(report["error"])
    finally:
        write_json(RUN_DIRECTORY / "report.json", report)
        write_json(OUTPUT / "latest-sync.json", report)
    require(report["status"] == "PASS", report.get("error", "Building WebUI synchronization failed"))
    unreal.log("FF_BUILDING_WEB_SYNC " + json.dumps({
        key: report[key] for key in ("status", "change", "asset_saved", "thumbnail_count",
                                    "unique_thumbnail_count", "protected_file_count", "unexpected_changes")
    }))


if __name__ == "__main__":
    run()
