# -*- coding: utf-8 -*-
"""Import the minimal circular ImageGen target used by Image_Target."""

import os

import unreal


SOURCE_RELATIVE = (
    "SourceAssets/UI/Minigame/TargetShooting/"
    "T_MG_TargetShooting_Target_Simple_ImageGen_v1.png"
)
DESTINATION = "/Game/FestaFiesta/UI/Minigame/TargetShooting"
ASSET_NAME = "T_MG_TargetShooting_Target_Simple_ImageGen_v1"
EXPECTED_SIZE = 1254


def run():
    source_path = os.path.join(
        unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()),
        SOURCE_RELATIVE,
    )
    if not os.path.isfile(source_path):
        raise RuntimeError("missing ImageGen target: {0}".format(SOURCE_RELATIVE))

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
    task.set_editor_property("factory", unreal.TextureFactory())
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    textures = [
        asset for asset in task.get_objects() if isinstance(asset, unreal.Texture2D)
    ]
    if len(textures) != 1:
        raise RuntimeError("TextureFactory returned {0} textures".format(len(textures)))

    texture = textures[0]
    width = int(texture.blueprint_get_size_x())
    height = int(texture.blueprint_get_size_y())
    if width != EXPECTED_SIZE or height != EXPECTED_SIZE:
        raise RuntimeError(
            "target must be square {0}x{0}; got {1}x{2}".format(
                EXPECTED_SIZE, width, height
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
    texture.set_editor_property("max_texture_size", 512)
    if not unreal.EditorAssetLibrary.save_loaded_asset(
        texture, only_if_is_dirty=False
    ):
        raise RuntimeError("save failed: {0}".format(texture.get_path_name()))

    unreal.log(
        "FF_TARGET_SHOOTING_TARGET_IMAGEGEN_V1_OK {0} ({1}x{2})".format(
            texture.get_path_name(), width, height
        )
    )


run()
