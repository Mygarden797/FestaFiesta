# -*- coding: utf-8 -*-
"""LightTeal Live HUD 텍스처를 현재 매니페스트 기준으로 검증한다."""

import json
import os

import unreal


MANIFEST_REL = "SourceArt/UI/LiveHUD/LightTealParts_v1/asset-manifest.json"
DESTINATION_ROOT = "/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal"
GOLD_COIN_FILES = {
    "Icons/Metrics/T_FF_LH_Icon_Gold_White_256.png",
    "Badges/T_FF_LH_Badge_Gold_256.png",
}


def project_dir():
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())


def enum_name(value):
    text = str(value)
    if "." in text:
        text = text.split(".")[-1]
    return text.split(":")[0].strip("<> ")


def expected_asset_path(relative_file):
    relative_without_extension = os.path.splitext(relative_file)[0].replace("\\", "/")
    return DESTINATION_ROOT + "/" + relative_without_extension


def run():
    manifest_path = os.path.join(project_dir(), MANIFEST_REL)
    with open(manifest_path, "r", encoding="utf-8") as handle:
        manifest = json.load(handle)

    failures = []
    verified = 0

    for entry in manifest.get("assets", []):
        asset_path = expected_asset_path(entry["file"])
        texture = unreal.EditorAssetLibrary.load_asset(asset_path)
        if not isinstance(texture, unreal.Texture2D):
            failures.append(asset_path + " missing/not Texture2D")
            continue

        is_tileable = bool(entry.get("tileable")) or entry["file"].startswith("HeatMap/")
        expected_mip = "TMGS_FROM_TEXTURE_GROUP" if is_tileable else "TMGS_NO_MIPMAPS"
        expected_address = "TA_WRAP" if is_tileable else "TA_CLAMP"

        checks = {
            "width": int(texture.blueprint_get_size_x()) == int(entry["width"]),
            "height": int(texture.blueprint_get_size_y()) == int(entry["height"]),
            "compression": enum_name(texture.get_editor_property("compression_settings")) == "TC_EDITOR_ICON",
            "group": enum_name(texture.get_editor_property("lod_group")) == "TEXTUREGROUP_UI",
            "srgb": bool(texture.get_editor_property("srgb")) == bool(entry.get("srgb", True)),
            "mip": enum_name(texture.get_editor_property("mip_gen_settings")) == expected_mip,
            "stream": bool(texture.get_editor_property("never_stream")) == bool(entry.get("never_stream", True)),
            "address_x": enum_name(texture.get_editor_property("address_x")) == expected_address,
            "address_y": enum_name(texture.get_editor_property("address_y")) == expected_address,
        }
        if entry["file"] in GOLD_COIN_FILES:
            checks["gold_max_size"] = int(texture.get_editor_property("max_texture_size")) == 256
            # Blueprint dimensions reflect the built UI resource, not the RGBA
            # source dimensions recorded by the art generator.
            built_width = int(texture.blueprint_get_size_x())
            built_height = int(texture.blueprint_get_size_y())
            checks["width"] = 0 < built_width <= 256
            checks["height"] = built_height == built_width
        bad = [name for name, passed in checks.items() if not passed]
        if bad:
            failures.append(asset_path + " failed: " + ", ".join(bad))
        else:
            verified += 1

    actual_textures = [
        path for path in unreal.EditorAssetLibrary.list_assets(
            DESTINATION_ROOT, recursive=True, include_folder=False)
        if isinstance(unreal.EditorAssetLibrary.load_asset(path), unreal.Texture2D)
    ]

    unreal.log("[FF-LHV] Verified: {0}/{1}, actual textures: {2}".format(
        verified, len(manifest.get("assets", [])), len(actual_textures)))

    if len(actual_textures) != len(manifest.get("assets", [])):
        failures.append("Texture count mismatch: actual {0}".format(len(actual_textures)))

    if failures:
        for failure in failures:
            unreal.log_error("[FF-LHV] " + failure)
        raise RuntimeError("Live HUD texture verification failed: {0}".format(len(failures)))


run()
