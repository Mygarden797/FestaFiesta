# -*- coding: utf-8 -*-
"""Import the versioned ImageGen target-shooting fallback plate.

Run with the editor closed:
    UnrealEditor-Cmd.exe FestaFiesta.uproject \
        -ExecutePythonScript="Tools/ui/import_target_shooting_imagegen_v3.py" \
        -nop4 -nosplash -nullrhi -DDC-ForceMemoryCache

The script owns only the ImageGen_v3 package. Existing UIUX_v2 and AAA assets
remain untouched, so a designer can roll back by changing one soft reference.
"""

import os

import unreal


SOURCE_RELATIVE = (
    "SourceAssets/UI/Minigame/TargetShooting/"
    "T_MG_TargetShooting_Backdrop_ImageGen_v3.png"
)
DESTINATION = "/Game/FestaFiesta/UI/Minigame/TargetShooting"
ASSET_NAME = "T_MG_TargetShooting_Backdrop_ImageGen_v3"
EXPECTED_WIDTH = 1672
EXPECTED_HEIGHT = 941


def project_dir():
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())


def import_texture(source_path):
    factory = unreal.TextureFactory()
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source_path)
    task.set_editor_property("destination_path", DESTINATION)
    task.set_editor_property("destination_name", ASSET_NAME)
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

    textures = [
        asset for asset in task.get_objects() if isinstance(asset, unreal.Texture2D)
    ]
    if len(textures) != 1:
        raise RuntimeError(
            "TextureFactory returned {0} Texture2D objects".format(len(textures))
        )
    return textures[0]


def run():
    source_path = os.path.join(project_dir(), SOURCE_RELATIVE)
    if not os.path.isfile(source_path):
        raise RuntimeError("missing ImageGen source: {0}".format(SOURCE_RELATIVE))

    texture = import_texture(source_path)
    expected_path = DESTINATION + "/" + ASSET_NAME
    if texture.get_path_name().split(".")[0] != expected_path:
        raise RuntimeError(
            "unexpected imported object {0}; expected {1}".format(
                texture.get_path_name(), expected_path
            )
        )

    texture.set_editor_property(
        "compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON
    )
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("srgb", True)
    texture.set_editor_property(
        "mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS
    )
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("max_texture_size", 0)

    width = int(texture.blueprint_get_size_x())
    height = int(texture.blueprint_get_size_y())
    if width != EXPECTED_WIDTH or height != EXPECTED_HEIGHT:
        raise RuntimeError(
            "invalid ImageGen backdrop dimensions: {0}x{1}; expected {2}x{3}".format(
                width, height, EXPECTED_WIDTH, EXPECTED_HEIGHT
            )
        )

    if not unreal.EditorAssetLibrary.save_loaded_asset(
        texture, only_if_is_dirty=False
    ):
        raise RuntimeError("save failed: {0}".format(texture.get_path_name()))

    unreal.log(
        "FF_TARGET_SHOOTING_IMAGEGEN_V3_OK {0} ({1}x{2})".format(
            texture.get_path_name(), width, height
        )
    )


run()
