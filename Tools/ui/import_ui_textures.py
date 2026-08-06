# -*- coding: utf-8 -*-
"""SourceArt의 UI PNG를 Unreal 텍스처로 임포트한다.

왜 스크립트인가
---------------
텍스처 하나마다 Compression·Texture Group·sRGB·Mip·Never Stream을 손으로 넣으면
26장 중 몇 개는 반드시 빠진다. 그리고 빠진 설정은 나중에 "왜 이것만 흐리지"로 돌아오는데
그때는 원인을 찾기 어렵다. 특히 놓치기 쉬운 것:

  - T_BrandMark_ShimmerMask_V3 는 **sRGB Off**여야 한다. 밝기 마스크라 감마를 먹으면 안 된다.
  - 밀도 패턴 3종은 타일러블이라 **Address Wrap**이어야 한다. Clamp면 이음매가 보인다.

설정의 출처는 SourceArt 각 폴더의 asset-manifest*.json이다.
이 스크립트는 매니페스트를 읽기만 하고 값을 자체적으로 정하지 않는다.

실행 (에디터를 닫고)
--------------------
    "C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
        "C:/UE5/8th-Team13-Final-Project/FestaFiesta.uproject" \
        -run=pythonscript -script="Tools/ui/import_ui_textures.py"

여러 번 돌려도 안전하다. 이미 있으면 소스에서 다시 읽어 덮어쓴다.
"""

import json
import os

import unreal


# 매니페스트 위치. 경로를 이 파일 밖에 두지 않는다 — 흩어지면 관리가 안 된다.
MANIFESTS = [
    "SourceArt/UI/Icons/asset-manifest.json",
    "SourceArt/UI/Title/GlassOrbit/asset-manifest-v2.json",
    "SourceArt/UI/Title/GlassOrbit/asset-manifest-v3.json",
]

# 타일러블 패턴. Address를 Wrap으로 두지 않으면 반복할 때 가장자리에 줄이 생긴다.
TILEABLE_ROLE_PREFIX = "tileable_"


def project_dir():
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())


def import_one(png_path, destination_path, asset_name):
    """PNG 한 장을 임포트한다. 실패하면 None."""
    task = unreal.AssetImportTask()
    task.filename = png_path
    task.destination_path = destination_path
    task.destination_name = asset_name
    task.automated = True          # 다이얼로그를 띄우지 않는다. 커맨드릿에서 멈추면 안 된다.
    task.replace_existing = True   # 다시 돌릴 수 있게 한다.
    task.save = False              # 설정을 넣은 뒤에 한 번에 저장한다.

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    for imported in task.get_editor_property("imported_object_paths") or []:
        asset = unreal.EditorAssetLibrary.load_asset(imported)
        if isinstance(asset, unreal.Texture2D):
            return asset

    return None


def apply_settings(texture, entry, defaults):
    """매니페스트 값을 텍스처에 반영한다."""
    role = entry.get("role", "")
    is_tileable = role.startswith(TILEABLE_ROLE_PREFIX)

    # UI 텍스처는 전부 UserInterface2D(무압축 RGBA)다. 아이콘의 얇은 선이 블록 압축에서 뭉갠다.
    texture.set_editor_property("compression_settings",
                                unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)

    # sRGB는 매니페스트가 개별로 지정할 수 있다. 마스크는 False여야 한다.
    texture.set_editor_property("srgb", bool(entry.get("srgb", defaults.get("srgb", True))))

    # 타일러블만 밉을 만든다. 나머지는 UI 고정 크기라 밉이 낭비다.
    texture.set_editor_property(
        "mip_gen_settings",
        unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP if is_tileable
        else unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)

    texture.set_editor_property("never_stream", bool(defaults.get("never_stream", True)))

    if is_tileable:
        # 이 프로퍼티는 엔진 버전에 따라 이름이 다를 수 있다. 없다고 임포트를 실패시키지 않는다.
        try:
            texture.set_editor_property("address_x", unreal.TextureAddress.TA_WRAP)
            texture.set_editor_property("address_y", unreal.TextureAddress.TA_WRAP)
        except Exception as error:
            unreal.log_warning(
                "[FF] {0}: Address를 Wrap으로 못 바꿨다. 에디터에서 직접 확인한다. ({1})"
                .format(texture.get_name(), error))


def run():
    root = project_dir()
    imported_count = 0
    failed = []

    for manifest_rel in MANIFESTS:
        manifest_path = os.path.join(root, manifest_rel)
        if not os.path.exists(manifest_path):
            unreal.log_warning("[FF] 매니페스트 없음: {0}".format(manifest_rel))
            continue

        with open(manifest_path, "r", encoding="utf-8") as handle:
            manifest = json.load(handle)

        source_dir = os.path.dirname(manifest_path)
        destination = manifest["unreal_import_root"]
        defaults = manifest.get("import_settings", {})

        unreal.log("[FF] --- {0} -> {1} ---".format(manifest_rel, destination))

        for entry in manifest.get("assets", []):
            file_name = entry["file"]
            png_path = os.path.join(source_dir, file_name)
            asset_name = os.path.splitext(file_name)[0]

            if not os.path.exists(png_path):
                failed.append("{0} (원본 없음)".format(file_name))
                continue

            texture = import_one(png_path, destination, asset_name)
            if texture is None:
                failed.append("{0} (임포트 실패)".format(file_name))
                continue

            apply_settings(texture, entry, defaults)
            unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)

            imported_count += 1
            unreal.log("[FF]   {0}  srgb={1}  tileable={2}".format(
                asset_name,
                texture.get_editor_property("srgb"),
                entry.get("role", "").startswith(TILEABLE_ROLE_PREFIX)))

    unreal.log("[FF] 임포트 완료: {0}장".format(imported_count))

    if failed:
        # 조용히 넘어가지 않는다. 빠진 텍스처는 화면에서 빈 칸으로만 보여서 원인을 못 찾는다.
        unreal.log_error("[FF] 실패 {0}건:".format(len(failed)))
        for item in failed:
            unreal.log_error("[FF]   - {0}".format(item))


run()
