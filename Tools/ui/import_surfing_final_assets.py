# -*- coding: utf-8 -*-
"""Import the final ImageGen surfing 2.5D artwork without touching rollback art.

Run with the editor closed:
    UnrealEditor-Cmd.exe FestaFiesta.uproject \
        -ExecutePythonScript="Tools/ui/import_surfing_final_assets.py" \
        -nop4 -nosplash -nullrhi -DDC-ForceMemoryCache
"""

import os

import unreal


DESTINATION = "/Game/FestaFiesta/UI/Minigame/Final"
ASSETS = (
    (
        "SourceAssets/UI/Minigame/Final/T_MG_Surfing_Backdrop_Final.png",
        "T_MG_Surfing_Backdrop_Final",
        (1672, 941),
    ),
    (
        "SourceAssets/UI/Minigame/Final/T_MG_Surfing_Surfer_Final.png",
        "T_MG_Surfing_Surfer_Final",
        (1024, 1536),
    ),
)


def project_dir():
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())


def import_texture(source, asset_name, expected_size):
    source_path = os.path.join(project_dir(), source)
    if not os.path.isfile(source_path):
        raise RuntimeError("missing final surfing source: {0}".format(source))

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
    textures = [
        asset for asset in task.get_objects() if isinstance(asset, unreal.Texture2D)
    ]
    if len(textures) != 1:
        raise RuntimeError(
            "expected one Texture2D, got {0}".format(len(textures))
        )

    texture = textures[0]
    expected_path = DESTINATION + "/" + asset_name
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

    actual_size = (
        int(texture.blueprint_get_size_x()),
        int(texture.blueprint_get_size_y()),
    )
    if actual_size != expected_size:
        raise RuntimeError(
            "invalid final surfing backdrop size: {0}x{1}; expected {2}x{3}".format(
                actual_size[0], actual_size[1], expected_size[0], expected_size[1]
            )
        )

    if not unreal.EditorAssetLibrary.save_loaded_asset(
        texture, only_if_is_dirty=False
    ):
        raise RuntimeError("save failed: {0}".format(texture.get_path_name()))

    return texture, actual_size


def run():
    imported = []
    for source, asset_name, expected_size in ASSETS:
        texture, actual_size = import_texture(source, asset_name, expected_size)
        imported.append(
            "{0} ({1}x{2})".format(
                texture.get_path_name(), actual_size[0], actual_size[1]
            )
        )

    unreal.log("[FF_SURFING_FINAL] imported " + ", ".join(imported))


run()
