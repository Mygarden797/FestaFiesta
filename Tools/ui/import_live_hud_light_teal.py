# -*- coding: utf-8 -*-
"""LightTealParts_v1 PNG를 Unreal Live HUD 테마 텍스처로 임포트한다.

실행:
    UnrealEditor-Cmd.exe FestaFiesta.uproject \
        -ExecutePythonScript=Tools/ui/import_live_hud_light_teal.py \
        -unattended -nopause -nosplash -nullrhi

SourceArt의 폴더 구조를 아래 Unreal 경로에 그대로 유지한다.
    /Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal

Preview, VectorSource, JSON, README는 asset-manifest.json의 assets 목록에 없으므로
임포트되지 않는다. 반복 실행 시 기존 텍스처를 소스 PNG로 갱신한다.
"""

import json
import os

import unreal


MANIFEST_REL = "SourceArt/UI/LiveHUD/LightTealParts_v1/asset-manifest.json"
DESTINATION_ROOT = "/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal"
TILEABLE_PREFIX = "HeatMap/"
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


def import_texture(source_file, destination_path, asset_name):
    task = unreal.AssetImportTask()
    task.filename = source_file
    task.destination_path = destination_path
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = True
    task.save = False

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    for object_path in task.get_editor_property("imported_object_paths") or []:
        asset = unreal.EditorAssetLibrary.load_asset(object_path)
        if isinstance(asset, unreal.Texture2D):
            return asset
    return None


def set_texture_settings(texture, entry):
    is_tileable = bool(entry.get("tileable")) or entry["file"].startswith(TILEABLE_PREFIX)

    texture.set_editor_property(
        "compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("srgb", bool(entry.get("srgb", True)))
    texture.set_editor_property("never_stream", bool(entry.get("never_stream", True)))
    texture.set_editor_property(
        "mip_gen_settings",
        unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP
        if is_tileable
        else unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)

    address = unreal.TextureAddress.TA_WRAP if is_tileable else unreal.TextureAddress.TA_CLAMP
    texture.set_editor_property("address_x", address)
    texture.set_editor_property("address_y", address)
    if entry["file"] in GOLD_COIN_FILES:
        # Keep the authored RGBA source intact while bounding resident UI size.
        texture.set_editor_property("max_texture_size", 256)


def run():
    manifest_path = os.path.join(project_dir(), MANIFEST_REL)
    with open(manifest_path, "r", encoding="utf-8") as handle:
        manifest = json.load(handle)

    source_root = os.path.dirname(manifest_path)
    imported = 0
    failures = []

    unreal.log("[FF-LH] Import start: {0}".format(DESTINATION_ROOT))

    for entry in manifest.get("assets", []):
        relative_file = entry["file"]
        source_file = os.path.normpath(os.path.join(source_root, relative_file))
        if relative_file in GOLD_COIN_FILES:
            source_file = os.path.join(project_dir(), "Tools/ui/assets/StarEmbossedGoldCoin.png")
        relative_dir = os.path.dirname(relative_file).replace("\\", "/")
        destination = DESTINATION_ROOT
        if relative_dir:
            destination += "/" + relative_dir
        asset_name = os.path.splitext(os.path.basename(relative_file))[0]

        if not os.path.isfile(source_file):
            failures.append(relative_file + " (source missing)")
            continue

        texture = import_texture(source_file, destination, asset_name)
        if texture is None:
            failures.append(relative_file + " (import failed)")
            continue

        set_texture_settings(texture, entry)
        unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
        imported += 1

        unreal.log(
            "[FF-LH] {0} {1}x{2} mip={3} address={4}".format(
                texture.get_path_name(),
                int(texture.blueprint_get_size_x()),
                int(texture.blueprint_get_size_y()),
                enum_name(texture.get_editor_property("mip_gen_settings")),
                enum_name(texture.get_editor_property("address_x"))))

    unreal.log("[FF-LH] Import complete: {0}/{1}".format(
        imported, len(manifest.get("assets", []))))

    if failures:
        for failure in failures:
            unreal.log_error("[FF-LH] " + failure)
        raise RuntimeError("Live HUD texture import failed: {0}".format(len(failures)))


run()
