# -*- coding: utf-8 -*-
"""Import the non-destructive UIUX v2 minigame art set.

Run with the editor closed. UE 5.7's headless Interchange path touches Slate,
so use the normal editor bootstrap and the explicit native TextureFactory:
    UnrealEditor-Cmd.exe FestaFiesta.uproject \
        -ExecutePythonScript="Tools/ui/import_minigame_uiux_v2_assets.py" \
        -nop4 -nosplash -nullrhi -DDC-ForceMemoryCache

The destination folder is versioned separately from the existing AAA backdrop
assets. Re-running this script only replaces assets owned by this v2 source set.
"""

import os

import unreal


DESTINATION = "/Game/FestaFiesta/UI/Minigame/UIUX_v2"
SOURCE_FOLDER = "SourceAssets/UI/Minigame/UIUX_v2"

ASSETS = (
    ("T_MG_TargetShooting_Backdrop_UIUX_v2.png", False),
    ("T_MG_TargetShooting_WaterBlaster_UIUX_v2.png", True),
    ("T_MG_Surfing_Backdrop_UIUX_v2.png", False),
    ("T_MG_Surfing_Rock_UIUX_v2.png", True),
    ("T_MG_Surfing_Log_UIUX_v2.png", True),
    ("T_MG_Surfing_Buoy_UIUX_v2.png", True),
    ("T_MG_Surfing_Coin_UIUX_v2.png", True),
    ("T_MG_Surfing_Heart_UIUX_v2.png", True),
    ("T_MG_Surfing_BoostPickup_UIUX_v2.png", True),
)


def project_dir():
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())


def import_texture(source_path, asset_name):
    factory = unreal.TextureFactory()
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source_path)
    task.set_editor_property("destination_path", DESTINATION)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("automated", True)
    try:
        task.set_editor_property("async_", False)
    except Exception:
        pass
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", False)
    task.set_editor_property("save", False)
    task.set_editor_property("factory", factory)

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    expected_path = DESTINATION + "/" + asset_name
    textures = [
        asset for asset in task.get_objects() if isinstance(asset, unreal.Texture2D)
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
            "unexpected imported object {0}; expected {1}".format(
                texture.get_path_name(), expected_path
            )
        )
    return texture


def apply_ui_settings(texture, is_sprite):
    texture.set_editor_property(
        "compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON
    )
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("srgb", True)
    texture.set_editor_property(
        "mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS
    )
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("max_texture_size", 512 if is_sprite else 0)


def validate_ui_settings(texture, is_sprite):
    expected = (
        (
            "compression_settings",
            unreal.TextureCompressionSettings.TC_EDITOR_ICON,
        ),
        ("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI),
        ("srgb", True),
        ("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS),
        ("never_stream", True),
        ("max_texture_size", 512 if is_sprite else 0),
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


def validate_dimensions(texture, is_sprite):
    width = int(texture.blueprint_get_size_x())
    height = int(texture.blueprint_get_size_y())
    if is_sprite:
        if width != height or width < 512:
            return width, height, "sprite source must be square and at least 512 px"
    elif width != 1672 or height != 941:
        return width, height, "backdrop source must be 1672x941"
    return width, height, None


def run():
    root = project_dir()
    failures = []

    for filename, is_sprite in ASSETS:
        relative_source = os.path.join(SOURCE_FOLDER, filename)
        source_path = os.path.join(root, relative_source)
        asset_name = os.path.splitext(filename)[0]
        if not os.path.isfile(source_path):
            failures.append("missing source: {0}".format(relative_source))
            continue

        texture = import_texture(source_path, asset_name)
        if texture is None:
            failures.append("import failed: {0}".format(relative_source))
            continue

        apply_ui_settings(texture, is_sprite)
        width, height, dimension_error = validate_dimensions(texture, is_sprite)
        if dimension_error:
            failures.append(
                "invalid dimensions for {0}: {1}x{2}; {3}".format(
                    texture.get_path_name(), width, height, dimension_error
                )
            )
            continue

        setting_errors = validate_ui_settings(texture, is_sprite)
        if setting_errors:
            failures.append(
                "invalid UI settings for {0}: {1}".format(
                    texture.get_path_name(), ", ".join(setting_errors)
                )
            )
            continue

        if not unreal.EditorAssetLibrary.save_loaded_asset(
            texture, only_if_is_dirty=False
        ):
            failures.append("save failed: {0}".format(texture.get_path_name()))
            continue

        unreal.log(
            "[FF_MINIGAME_UIUX_V2] imported {0} ({1}x{2}, sprite={3})".format(
                texture.get_path_name(), width, height, is_sprite
            )
        )

    if failures:
        for failure in failures:
            unreal.log_error("[FF_MINIGAME_UIUX_V2] {0}".format(failure))
        raise RuntimeError(
            "Minigame UIUX v2 import failed ({0} error(s))".format(len(failures))
        )

    unreal.log(
        "[FF_MINIGAME_UIUX_V2] imported {0} textures".format(len(ASSETS))
    )


run()
