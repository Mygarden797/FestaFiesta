# -*- coding: utf-8 -*-
"""Verify the production surfing 2.5D artwork and the active FreeRide CDO."""

import unreal


BACKDROP_TEXTURE_PATH = (
    "/Game/FestaFiesta/UI/Minigame/Final/"
    "T_MG_Surfing_Backdrop_Final.T_MG_Surfing_Backdrop_Final"
)
SURFER_TEXTURE_PATH = (
    "/Game/FestaFiesta/UI/Minigame/Final/"
    "T_MG_Surfing_Surfer_Final.T_MG_Surfing_Surfer_Final"
)
SCREEN_CLASS_PATH = (
    "/Game/FestaFiesta/UI/Minigame/WBP_FF_Surfing_FreeRide."
    "WBP_FF_Surfing_FreeRide_C"
)
EXPECTED_ASSETS = (
    (BACKDROP_TEXTURE_PATH, (1672, 941)),
    (SURFER_TEXTURE_PATH, (1024, 1536)),
)


def fail(message):
    unreal.log_error("[FF_SURFING_FINAL_VERIFY] " + message)
    raise RuntimeError(message)


def run():
    expected_properties = (
        (
            "compression_settings",
            unreal.TextureCompressionSettings.TC_EDITOR_ICON,
        ),
        ("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI),
        ("srgb", True),
        ("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS),
        ("never_stream", True),
    )
    loaded_textures = {}
    for texture_path, expected_size in EXPECTED_ASSETS:
        texture = unreal.load_object(None, texture_path)
        if not isinstance(texture, unreal.Texture2D):
            fail("final texture is missing: {0}".format(texture_path))

        actual_size = (
            int(texture.blueprint_get_size_x()),
            int(texture.blueprint_get_size_y()),
        )
        if actual_size != expected_size:
            fail(
                "{0} size is {1}x{2}; expected {3}x{4}".format(
                    texture_path,
                    actual_size[0],
                    actual_size[1],
                    expected_size[0],
                    expected_size[1],
                )
            )
        for property_name, expected_value in expected_properties:
            actual_value = texture.get_editor_property(property_name)
            if actual_value != expected_value:
                fail(
                    "{0}.{1}={2!s}; expected {3!s}".format(
                        texture_path, property_name, actual_value, expected_value
                    )
                )
        loaded_textures[texture_path] = texture

    screen_class = unreal.load_class(None, SCREEN_CLASS_PATH)
    if screen_class is None:
        fail("FreeRide screen class is missing: {0}".format(SCREEN_CLASS_PATH))
    screen_cdo = unreal.get_default_object(screen_class)
    if screen_cdo is None:
        fail("FreeRide screen CDO is unavailable")

    visual_style = screen_cdo.get_editor_property("visual_style")
    backdrop = visual_style.get_editor_property("backdrop_texture")
    if backdrop is None or backdrop.get_path_name() != BACKDROP_TEXTURE_PATH:
        fail(
            "FreeRide VisualStyle.BackdropTexture is {0}; expected {1}".format(
                "None" if backdrop is None else backdrop.get_path_name(),
                BACKDROP_TEXTURE_PATH,
            )
        )
    surfer = visual_style.get_editor_property("surfer_texture")
    if surfer is None or surfer.get_path_name() != SURFER_TEXTURE_PATH:
        fail(
            "FreeRide VisualStyle.SurferTexture is {0}; expected {1}".format(
                "None" if surfer is None else surfer.get_path_name(),
                SURFER_TEXTURE_PATH,
            )
        )

    scene_capture_enabled = None
    for property_name in (
        "enable_scene_capture3_d",
        "enable_scene_capture_3d",
        "enable_scene_capture3d",
    ):
        try:
            scene_capture_enabled = visual_style.get_editor_property(property_name)
            break
        except Exception:
            pass
    if scene_capture_enabled is None:
        fail("VisualStyle.bEnableSceneCapture3D was not exposed to Python")
    if bool(scene_capture_enabled):
        fail("production FreeRide CDO still enables the primitive SceneCapture path")

    unreal.log(
        "[FF_SURFING_FINAL_VERIFY] OK: backdrop + surfer linked, production 2.5D active"
    )


run()
