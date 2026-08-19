# -*- coding: utf-8 -*-
"""LightTeal 테마의 사각형 버튼 텍스처만 Unreal에 임포트한다.

실행:
    UnrealEditor-Cmd.exe FestaFiesta.uproject \
        -ExecutePythonScript=Tools/ui/import_live_hud_light_teal_rect_buttons.py \
        -unattended -nopause -nosplash -nullrhi

기존 LightTeal 에셋을 재임포트하지 않고, 이번에 추가한 Rect 버튼 9종만
``/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Buttons``에 생성한다.
"""

import json
import os

import unreal


MANIFEST_REL = "SourceArt/UI/LiveHUD/LightTealParts_v1/asset-manifest.json"
DESTINATION = "/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Buttons"
FILE_PREFIX = "Buttons/T_FF_LH_Button_Rect_"


def project_dir():
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())


def enum_name(value):
    text = str(value)
    if "." in text:
        text = text.split(".")[-1]
    return text.split(":")[0].strip("<> ")


def import_texture(source_file, asset_name):
    task = unreal.AssetImportTask()
    task.filename = source_file
    task.destination_path = DESTINATION
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
    texture.set_editor_property(
        "compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("srgb", bool(entry.get("srgb", True)))
    texture.set_editor_property("never_stream", bool(entry.get("never_stream", True)))
    texture.set_editor_property(
        "mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)


def verify_texture(texture, entry):
    checks = {
        "width": int(texture.blueprint_get_size_x()) == int(entry["width"]),
        "height": int(texture.blueprint_get_size_y()) == int(entry["height"]),
        "compression": enum_name(texture.get_editor_property("compression_settings"))
        == "TC_EDITOR_ICON",
        "group": enum_name(texture.get_editor_property("lod_group")) == "TEXTUREGROUP_UI",
        "srgb": bool(texture.get_editor_property("srgb")) == bool(entry.get("srgb", True)),
        "mip": enum_name(texture.get_editor_property("mip_gen_settings"))
        == "TMGS_NO_MIPMAPS",
        "stream": bool(texture.get_editor_property("never_stream"))
        == bool(entry.get("never_stream", True)),
        "address_x": enum_name(texture.get_editor_property("address_x")) == "TA_CLAMP",
        "address_y": enum_name(texture.get_editor_property("address_y")) == "TA_CLAMP",
    }
    return [name for name, passed in checks.items() if not passed]


def run():
    manifest_path = os.path.join(project_dir(), MANIFEST_REL)
    with open(manifest_path, "r", encoding="utf-8") as handle:
        manifest = json.load(handle)

    entries = [
        entry for entry in manifest.get("assets", [])
        if entry.get("file", "").startswith(FILE_PREFIX)
    ]
    if len(entries) != 9:
        raise RuntimeError(
            "Rect button manifest count mismatch: expected 9, got {0}".format(len(entries)))

    source_root = os.path.dirname(manifest_path)
    imported = 0
    failures = []

    unreal.log("[FF-LH-RECT] Import start: {0}".format(DESTINATION))

    for entry in entries:
        relative_file = entry["file"]
        source_file = os.path.normpath(os.path.join(source_root, relative_file))
        asset_name = os.path.splitext(os.path.basename(relative_file))[0]

        if not os.path.isfile(source_file):
            failures.append(relative_file + " (source missing)")
            continue

        texture = import_texture(source_file, asset_name)
        if texture is None:
            failures.append(relative_file + " (import failed)")
            continue

        set_texture_settings(texture, entry)
        unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)

        reloaded = unreal.EditorAssetLibrary.load_asset(texture.get_path_name())
        bad = verify_texture(reloaded, entry) if isinstance(reloaded, unreal.Texture2D) else ["reload"]
        if bad:
            failures.append(relative_file + " (verification failed: " + ", ".join(bad) + ")")
            continue

        imported += 1
        unreal.log("[FF-LH-RECT] " + texture.get_path_name())

    unreal.log("[FF-LH-RECT] Import complete: {0}/{1}".format(imported, len(entries)))

    if failures:
        for failure in failures:
            unreal.log_error("[FF-LH-RECT] " + failure)
        raise RuntimeError(
            "LightTeal Rect button import failed: {0}".format(len(failures)))


run()
