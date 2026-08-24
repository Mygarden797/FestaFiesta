# -*- coding: utf-8 -*-
"""Import the generated minigame backdrops with deterministic UI settings.

Run with the editor closed:
    UnrealEditor-Cmd.exe FestaFiesta.uproject \
        -run=pythonscript -script="Tools/ui/import_minigame_aaa_backdrops.py" \
        -unattended -nop4 -nosplash -nullrhi

The two destination assets are owned by this script, so re-running it safely
reimports them from the checked-in PNG sources.
"""

import os

import unreal


DESTINATION = "/Game/FestaFiesta/UI/Minigame/AAA"
BACKDROPS = (
    (
        "SourceAssets/UI/Minigame/T_MG_TargetShooting_Backdrop_AAA.png",
        "T_MG_TargetShooting_Backdrop_AAA",
    ),
    (
        "SourceAssets/UI/Minigame/T_MG_Surfing_Backdrop_AAA.png",
        "T_MG_Surfing_Backdrop_AAA",
    ),
)
DEPRECATED_DUPLICATES = (
    "/Game/FestaFiesta/UI/Minigame/Image/T_MG_TargetShooting_Backdrop_AAA",
    "/Game/FestaFiesta/UI/Minigame/Image/T_MG_Surfing_Backdrop_AAA",
)


def project_dir():
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())


def import_backdrop(source_path, asset_name):
    task = unreal.AssetImportTask()
    task.filename = source_path
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


def apply_ui_settings(texture):
    texture.set_editor_property(
        "compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON
    )
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("srgb", True)
    texture.set_editor_property(
        "mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS
    )
    texture.set_editor_property("never_stream", True)


def validate_ui_settings(texture):
    expected = (
        (
            "compression_settings",
            unreal.TextureCompressionSettings.TC_EDITOR_ICON,
        ),
        ("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI),
        ("srgb", True),
        ("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS),
        ("never_stream", True),
    )
    return [
        "{0}={1!s} (expected {2!s})".format(
            property_name,
            texture.get_editor_property(property_name),
            expected_value,
        )
        for property_name, expected_value in expected
        if texture.get_editor_property(property_name) != expected_value
    ]


def remove_unreferenced_deprecated_duplicates():
    failures = []
    for asset_path in DEPRECATED_DUPLICATES:
        if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            continue

        referencers = unreal.EditorAssetLibrary.find_package_referencers_for_asset(
            asset_path, load_assets_to_confirm=True
        )
        if referencers:
            failures.append(
                "deprecated duplicate still referenced: {0} <- {1}".format(
                    asset_path, ", ".join(referencers)
                )
            )
            continue
        if not unreal.EditorAssetLibrary.delete_asset(asset_path):
            failures.append("delete failed: {0}".format(asset_path))
            continue
        unreal.log("[FF_MINIGAME_AAA] removed unreferenced duplicate {0}".format(asset_path))
    return failures


def run():
    root = project_dir()
    failures = []

    for relative_source, asset_name in BACKDROPS:
        source_path = os.path.join(root, relative_source)
        if not os.path.isfile(source_path):
            failures.append("missing source: {0}".format(relative_source))
            continue

        texture = import_backdrop(source_path, asset_name)
        if texture is None:
            failures.append("import failed: {0}".format(relative_source))
            continue

        apply_ui_settings(texture)
        if not unreal.EditorAssetLibrary.save_loaded_asset(
            texture, only_if_is_dirty=False
        ):
            failures.append("save failed: {0}".format(texture.get_path_name()))
            continue

        width = int(texture.blueprint_get_size_x())
        height = int(texture.blueprint_get_size_y())
        if width <= 0 or height <= 0:
            failures.append("invalid dimensions: {0}".format(texture.get_path_name()))
            continue

        setting_errors = validate_ui_settings(texture)
        if setting_errors:
            failures.append(
                "invalid UI settings for {0}: {1}".format(
                    texture.get_path_name(), ", ".join(setting_errors)
                )
            )
            continue

        unreal.log(
            "[FF_MINIGAME_AAA] imported {0} ({1}x{2})".format(
                texture.get_path_name(), width, height
            )
        )

    failures.extend(remove_unreferenced_deprecated_duplicates())

    if failures:
        for failure in failures:
            unreal.log_error("[FF_MINIGAME_AAA] {0}".format(failure))
        raise RuntimeError(
            "Minigame backdrop import failed ({0} error(s))".format(len(failures))
        )

    unreal.log("[FF_MINIGAME_AAA] imported {0} backdrops".format(len(BACKDROPS)))


run()
