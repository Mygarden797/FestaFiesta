# -*- coding: utf-8 -*-
"""Refresh only the 22 generated Notion facility catalog Texture2D assets.

This intentionally never loads, imports, configures, or saves a StaticMesh.  It
uses an explicit native TextureFactory so UE 5.7 commandlets do not enter the
Interchange/Slate path.  All 22 imports are validated in memory before any
package is saved.
"""

import json
import os
import struct

import unreal


TEXTURE_DESTINATION = "/Game/FestaFiesta/Zone/Test_Asset/image"
MANIFEST_RELATIVE = os.path.join(
    "Tools", "facilities", "notion_facility_examples.json"
)
PNG_RELATIVE = os.path.join(
    "Content", "FestaFiesta", "Zone", "Test_Asset", "image"
)
REPORT_NAME = "NotionFacilityThumbnailRefreshReport.json"
EXPECTED_REFRESH_COUNT = 22


def project_dir():
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())


def saved_dir():
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())


def try_set(target, property_name, value):
    try:
        target.set_editor_property(property_name, value)
        return True
    except Exception as error:
        unreal.log_warning(
            "[FF_THUMB_REFRESH] Optional property {0} was not set: {1}".format(
                property_name, error
            )
        )
        return False


def png_header(path):
    with open(path, "rb") as handle:
        header = handle.read(26)
    if len(header) != 26 or header[:8] != b"\x89PNG\r\n\x1a\n":
        raise RuntimeError("Invalid PNG header: " + path)
    width, height = struct.unpack(">II", header[16:24])
    return width, height, header[24], header[25]


def texture_name(definition_id):
    return "T_UI_BuildCatalog_" + definition_id


def configure_texture_in_memory(texture, definition_id, source_rel):
    texture.set_editor_property(
        "compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON
    )
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property(
        "mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS
    )
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    unreal.EditorAssetLibrary.set_metadata_tag(
        texture, "FFZS.GeneratedSource", source_rel.replace("\\", "/")
    )
    unreal.EditorAssetLibrary.set_metadata_tag(
        texture, "FFZS.NotionFacilityDefinitionId", definition_id
    )


def validate_texture_settings(texture):
    checks = (
        (
            "compression_settings",
            unreal.TextureCompressionSettings.TC_EDITOR_ICON,
        ),
        ("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI),
        ("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS),
        ("srgb", True),
        ("never_stream", True),
        ("address_x", unreal.TextureAddress.TA_CLAMP),
        ("address_y", unreal.TextureAddress.TA_CLAMP),
    )
    failures = []
    for property_name, expected in checks:
        actual = texture.get_editor_property(property_name)
        if actual != expected:
            failures.append(
                "{0} expected {1}, got {2}".format(
                    property_name, expected, actual
                )
            )
    if failures:
        raise RuntimeError("; ".join(failures))


def import_texture(source, asset_name):
    factory = unreal.TextureFactory()
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", TEXTURE_DESTINATION)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("automated", True)
    try_set(task, "async_", False)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", False)
    task.set_editor_property("save", False)
    task.set_editor_property("factory", factory)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    expected_path = TEXTURE_DESTINATION + "/" + asset_name
    textures = [
        obj for obj in task.get_objects() if isinstance(obj, unreal.Texture2D)
    ]
    if len(textures) != 1:
        raise RuntimeError(
            "TextureFactory returned {0} Texture2D objects for {1}".format(
                len(textures), expected_path
            )
        )
    texture = textures[0]
    if texture.get_path_name().split(".")[0] != expected_path:
        raise RuntimeError(
            "Unexpected imported object {0}; expected {1}".format(
                texture.get_path_name(), expected_path
            )
        )
    return texture


def write_report(report):
    path = os.path.join(saved_dir(), REPORT_NAME)
    with open(path, "w", encoding="utf-8") as handle:
        json.dump(report, handle, ensure_ascii=False, indent=2)
    return path


def run():
    root = project_dir()
    manifest_path = os.path.join(root, MANIFEST_RELATIVE)
    with open(manifest_path, "r", encoding="utf-8") as handle:
        manifest = json.load(handle)

    existing_ids = set(manifest["existing_representatives"])
    entries = [
        entry
        for entry in manifest["examples"]
        if entry["definition_id"] not in existing_ids
    ]
    definition_ids = [entry["definition_id"] for entry in entries]
    if len(existing_ids) != 7:
        raise RuntimeError("Expected exactly 7 protected representatives")
    if len(entries) != EXPECTED_REFRESH_COUNT:
        raise RuntimeError(
            "Expected {0} refresh entries, got {1}".format(
                EXPECTED_REFRESH_COUNT, len(entries)
            )
        )
    if len(set(definition_ids)) != len(definition_ids):
        raise RuntimeError("Duplicate definition IDs in refresh allowlist")
    if existing_ids.intersection(definition_ids):
        raise RuntimeError("Protected representative leaked into refresh allowlist")

    preflight = []
    for entry in entries:
        definition_id = entry["definition_id"]
        asset_name = texture_name(definition_id)
        asset_path = TEXTURE_DESTINATION + "/" + asset_name
        source_rel = os.path.join(PNG_RELATIVE, asset_name + ".png")
        source_path = os.path.join(root, source_rel)
        if not os.path.isfile(source_path):
            raise RuntimeError("Missing PNG source: " + source_path)
        width, height, bit_depth, color_type = png_header(source_path)
        if (width, height, bit_depth, color_type) != (324, 260, 8, 2):
            raise RuntimeError(
                "PNG must be 324x260 8-bit RGB: {0} is {1}x{2}, depth={3}, type={4}".format(
                    source_path, width, height, bit_depth, color_type
                )
            )
        current = unreal.EditorAssetLibrary.load_asset(asset_path)
        if not isinstance(current, unreal.Texture2D):
            raise RuntimeError("Expected existing Texture2D: " + asset_path)
        preflight.append(
            {
                "definition_id": definition_id,
                "asset_name": asset_name,
                "asset_path": asset_path,
                "source_rel": source_rel,
                "source_path": source_path,
            }
        )

    refreshed = []
    failures = []
    pending_saves = []
    for item in preflight:
        try:
            texture = import_texture(item["source_path"], item["asset_name"])
            configure_texture_in_memory(
                texture, item["definition_id"], item["source_rel"]
            )
            validate_texture_settings(texture)
            pending_saves.append(texture)
            refreshed.append(
                {
                    "definition_id": item["definition_id"],
                    "texture": texture.get_path_name(),
                    "source": item["source_rel"].replace("\\", "/"),
                    "png_size": [324, 260],
                    "pixel_format": "8-bit RGB",
                }
            )
            unreal.log(
                "[FF_THUMB_REFRESH] Validated in memory " + item["definition_id"]
            )
        except Exception as error:
            failure = "{0}: {1}".format(item["definition_id"], error)
            failures.append(failure)
            unreal.log_error("[FF_THUMB_REFRESH] " + failure)

    report = {
        "source": manifest["source"],
        "protected_existing_representatives": sorted(existing_ids),
        "expected_refresh_count": EXPECTED_REFRESH_COUNT,
        "validated_in_memory": len(refreshed),
        "saved": 0,
        "assets": refreshed,
        "failures": failures,
    }
    if failures or len(pending_saves) != EXPECTED_REFRESH_COUNT:
        write_report(report)
        raise RuntimeError(
            "Thumbnail refresh aborted before saving ({0}/{1}, {2} failures)".format(
                len(pending_saves), EXPECTED_REFRESH_COUNT, len(failures)
            )
        )

    saved = 0
    for texture in pending_saves:
        if not unreal.EditorAssetLibrary.save_loaded_asset(
            texture, only_if_is_dirty=True
        ):
            failures.append("Failed to save " + texture.get_path_name())
            unreal.log_error("[FF_THUMB_REFRESH] " + failures[-1])
        else:
            saved += 1
    report["saved"] = saved
    report["failures"] = failures
    report_path = write_report(report)
    if failures or saved != EXPECTED_REFRESH_COUNT:
        raise RuntimeError(
            "Thumbnail refresh save failed ({0}/{1}, {2} failures)".format(
                saved, EXPECTED_REFRESH_COUNT, len(failures)
            )
        )
    unreal.log(
        "[FF_THUMB_REFRESH] Complete: {0} Texture2D assets refreshed; report={1}".format(
            saved, report_path
        )
    )


run()
