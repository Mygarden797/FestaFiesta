# -*- coding: utf-8 -*-
"""타이틀/Atlas용 Casual Relief v1 지구 텍스처 네 장을 UE 5.7에 임포트한다.

기존 Atlas 텍스처를 건드리지 않고 별도의 버전 경로를 사용한다.

이미 열린 에디터의 Output Log를 Python 모드로 바꾼 뒤 실행:
    exec(open(r"C:/UE5/8th-Team13-Final-Project/Tools/ui/import_title_casual_relief_textures.py", encoding="utf-8").read())

또는 정상 에디터 부팅 경로에서 -ExecutePythonScript를 사용한다. UE 5.7에서
-run=pythonscript는 Interchange가 Slate를 만지며 크래시할 수 있으므로 사용하지 않는다.
스크립트는 에디터를 자동 종료하지 않는다.
"""

import os

import unreal


SOURCE_DIR = "SourceArt/UI/Title/CasualReliefParts_v1/Earth"
DESTINATION = "/Game/FestaFiesta/UI/Atlas/Earth/CasualRelief_v1/Textures"

TEXTURES = (
    {
        "file": "T_FF_Earth_CasualRelief_Albedo_2048x1024.png",
        "asset": "T_FF_Earth_CasualRelief_Albedo",
        "srgb": True,
        "compression": unreal.TextureCompressionSettings.TC_DEFAULT,
    },
    {
        "file": "T_FF_Earth_CasualRelief_Height16_2048x1024.png",
        "asset": "T_FF_Earth_CasualRelief_Height16",
        "srgb": False,
        "compression": unreal.TextureCompressionSettings.TC_GRAYSCALE,
    },
    {
        "file": "T_FF_Earth_CasualRelief_Normal_DX_2048x1024.png",
        "asset": "T_FF_Earth_CasualRelief_Normal_DX",
        "srgb": False,
        "compression": unreal.TextureCompressionSettings.TC_NORMALMAP,
    },
    {
        "file": "T_FF_Earth_CasualRelief_Masks_2048x1024.png",
        "asset": "T_FF_Earth_CasualRelief_Masks",
        "srgb": False,
        "compression": unreal.TextureCompressionSettings.TC_MASKS,
    },
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
    task.set_editor_property("replace_existing_settings", True)
    task.set_editor_property("save", False)
    task.set_editor_property("factory", factory)

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    textures = [
        asset for asset in task.get_objects() if isinstance(asset, unreal.Texture2D)
    ]
    if len(textures) != 1:
        raise RuntimeError(
            "TextureFactory returned {0} Texture2D objects for {1}".format(
                len(textures), asset_name
            )
        )
    return textures[0]


def apply_settings(texture, entry):
    texture.set_editor_property("compression_settings", entry["compression"])
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    texture.set_editor_property("srgb", entry["srgb"])
    texture.set_editor_property(
        "mip_gen_settings", unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP
    )
    texture.set_editor_property("never_stream", False)
    texture.set_editor_property("address_x", unreal.TextureAddress.TA_WRAP)
    texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)

    if entry["compression"] == unreal.TextureCompressionSettings.TC_NORMALMAP:
        try:
            texture.set_editor_property("flip_green_channel", False)
        except Exception as error:
            unreal.log_warning(
                "[FF_CASUAL_RELIEF] FlipGreen 확인 생략: {0}".format(error)
            )


def validate_texture(texture, entry):
    expected_path = "{0}/{1}".format(DESTINATION, entry["asset"])
    actual_path = texture.get_path_name().split(".", 1)[0]
    errors = []
    if actual_path != expected_path:
        errors.append("asset path {0} != {1}".format(actual_path, expected_path))

    width = int(texture.blueprint_get_size_x())
    height = int(texture.blueprint_get_size_y())
    if (width, height) != (2048, 1024):
        errors.append("size {0}x{1} != 2048x1024".format(width, height))

    expected = (
        ("compression_settings", entry["compression"]),
        ("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD),
        ("srgb", entry["srgb"]),
        ("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP),
        ("never_stream", False),
        ("address_x", unreal.TextureAddress.TA_WRAP),
        ("address_y", unreal.TextureAddress.TA_CLAMP),
    )
    for property_name, expected_value in expected:
        actual_value = texture.get_editor_property(property_name)
        if actual_value != expected_value:
            errors.append(
                "{0}={1!s} expected {2!s}".format(
                    property_name, actual_value, expected_value
                )
            )
    return errors


def run():
    source_root = os.path.join(project_dir(), SOURCE_DIR)
    failures = []

    for entry in TEXTURES:
        source_path = os.path.join(source_root, entry["file"])
        if not os.path.isfile(source_path):
            failures.append("missing source: {0}".format(source_path))
            continue

        try:
            texture = import_texture(source_path, entry["asset"])
            apply_settings(texture, entry)
            errors = validate_texture(texture, entry)
            if errors:
                failures.append(
                    "{0}: {1}".format(entry["asset"], ", ".join(errors))
                )
                continue
            if not unreal.EditorAssetLibrary.save_loaded_asset(
                texture, only_if_is_dirty=False
            ):
                failures.append("save failed: {0}".format(texture.get_path_name()))
                continue
            unreal.log(
                "[FF_CASUAL_RELIEF] imported {0} (2048x1024)".format(
                    texture.get_path_name()
                )
            )
        except Exception as error:
            failures.append("{0}: {1}".format(entry["asset"], error))

    if failures:
        for failure in failures:
            unreal.log_error("[FF_CASUAL_RELIEF] {0}".format(failure))
        raise RuntimeError(
            "Casual Relief texture import failed ({0} error(s))".format(
                len(failures)
            )
        )

    unreal.log(
        "[FF_CASUAL_RELIEF] import complete: {0}/{0} textures".format(
            len(TEXTURES)
        )
    )


run()
