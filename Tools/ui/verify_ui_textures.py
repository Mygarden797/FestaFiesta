# -*- coding: utf-8 -*-
"""임포트된 UI 텍스처의 설정을 덤프한다.

import_ui_textures.py 를 돌린 뒤 실제로 설정이 먹었는지 확인하는 용도다.
"설정을 넣었다"와 "설정이 들어갔다"는 다르다. 특히 확인해야 하는 것:

  - T_BrandMark_ShimmerMask_V3 : sRGB 가 False 인가
  - T_UI_Density_*             : Address 가 Wrap 인가
  - 전부                        : UserInterface2D / UI 그룹 / NeverStream

실행 (에디터를 닫고)
    "…/UnrealEditor-Cmd.exe" "…/FestaFiesta.uproject" \
        -ExecutePythonScript="Tools/ui/verify_ui_textures.py" -unattended -nopause -nosplash -nullrhi
"""

import unreal


ROOTS = [
    "/Game/FestaFiesta/UI/Icons",
    "/Game/FestaFiesta/UI/Title/GlassOrbitV2",
    "/Game/FestaFiesta/UI/Title/GlassOrbitV3",
]


def short(value):
    """열거형 이름에서 접두어를 떼어 읽기 좋게 만든다."""
    text = str(value)
    return text.split(".")[-1] if "." in text else text


def run():
    for root in ROOTS:
        unreal.log("[FFV] === {0} ===".format(root))

        for asset_path in sorted(unreal.EditorAssetLibrary.list_assets(root, recursive=True)):
            asset = unreal.EditorAssetLibrary.load_asset(asset_path)
            if not isinstance(asset, unreal.Texture2D):
                continue

            try:
                address = short(asset.get_editor_property("address_x"))
            except Exception:
                address = "?"

            unreal.log("[FFV] {0:<32} {1:>4}x{2:<4} srgb={3:<5} comp={4:<14} group={5:<16} mip={6:<22} stream={7:<5} addr={8}".format(
                asset.get_name(),
                int(asset.blueprint_get_size_x()),
                int(asset.blueprint_get_size_y()),
                str(asset.get_editor_property("srgb")),
                short(asset.get_editor_property("compression_settings")),
                short(asset.get_editor_property("lod_group")),
                short(asset.get_editor_property("mip_gen_settings")),
                str(asset.get_editor_property("never_stream")),
                address))


run()
