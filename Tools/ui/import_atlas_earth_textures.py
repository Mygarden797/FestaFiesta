# -*- coding: utf-8 -*-
"""Festival Atlas 지구 텍스처를 Unreal로 임포트한다 (U3-a).

왜 스크립트인가
---------------
다섯 장이지만 장마다 설정이 다르고, **틀리면 화면에서 원인을 찾기 어렵다.**

  - 낮/밤 맵은 색이므로 **sRGB On**.
  - 구름은 밝기 마스크다 → **sRGB Off**. 감마를 먹으면 구름 두께가 어긋난다.
  - 노멀은 **TC_Normalmap** + sRGB Off. 이걸 놓치면 조명이 뒤집혀 보인다.
  - 스페큘러는 마스크다 → sRGB Off.
  - 전부 등장방형(equirectangular)이라 **Address X = Wrap, Y = Clamp** 여야 한다.
    X를 Clamp로 두면 경도 180 이음선에 세로줄이 생기고,
    Y를 Wrap으로 두면 극점에서 반대쪽 극의 픽셀이 새어 들어온다.

라이선스
--------
원본은 Solar System Scope(INOVE), CC BY 4.0이다. 여기서 임포트하는 파일 중
`_soft_stylized` 3장은 **수정한 파생물**이고 `_original` 2장은 미수정이다.
출처·수정 사실은 `THIRD_PARTY_NOTICES.md`에 기록돼 있다. 게임 내 표기는 U3-c에서 만든다.

🔴 실행 방법 — `-run=pythonscript` 는 **죽는다**
------------------------------------------------
커맨드릿에는 Slate 가 없어서 Interchange 임포트가 그 자리에서 크래시한다
(`Assertion failed: CurrentApplication.IsValid()` — 2026-08-23 실측).
**에디터를 정상 부팅시키는 `-ExecutePythonScript` 를 쓴다.**

**이미 열려 있는 에디터에서 돌리는 것이 가장 편하다.** `Output Log` 의 Cmd 를 `Python` 으로
바꾸고 한 줄만 친다:

    exec(open(r"C:/UE5/8th-Team13-Final-Project/Tools/ui/import_atlas_earth_textures.py").read())

에디터 없이 돌리려면 PowerShell 에서:

    & "C:\\Program Files\\Epic Games\\UE_5.7\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" `
        "C:\\UE5\\8th-Team13-Final-Project\\FestaFiesta.uproject" `
        -ExecutePythonScript="C:/UE5/8th-Team13-Final-Project/Tools/ui/import_atlas_earth_textures.py" `
        -nop4

⚠️ 이 경로는 임포트가 끝나도 **에디터가 열린 채 남는다. 직접 닫는다.**
스크립트가 스스로 닫게 해 봤는데 엔진 종료 어서션(`Object is not packaged:
ModeManagerInteractiveToolsContext`)으로 떨어졌다 — 우리 코드 문제가 아니라 에디터
종료 경로의 문제이고, 그 시점에는 이미 에셋이 전부 저장돼 있다. 자동 종료를 넣지 않는다.
로그에 `지구 텍스처 임포트 완료: 5/5장` 이 찍히면 끝난 것이다.

여러 번 돌려도 안전하다. 이미 있으면 소스에서 다시 읽어 덮어쓴다.
"""

import os

import unreal


SOURCE_DIR = "SourceArt/ThirdParty/EarthTextures_Stylized01"
DESTINATION = "/Game/FestaFiesta/UI/Atlas/Earth"

# 장마다 다른 설정을 한 곳에 모아 둔다. 값을 코드 여러 군데에 흩뿌리지 않는다.
#   srgb      : 색이면 True, 마스크·노멀이면 False
#   normalmap : 노멀 압축을 쓸지
TEXTURES = [
    {
        "file": "earth_day_soft_stylized.png",
        "asset": "T_Atlas_Earth_Day",
        "srgb": True,
        "normalmap": False,
        "note": "낮 표면. 머티리얼의 Base Color.",
    },
    {
        "file": "earth_night_soft_stylized.png",
        "asset": "T_Atlas_Earth_Night",
        "srgb": True,
        "normalmap": False,
        "note": "밤 도시 불빛. Emissive.",
    },
    {
        "file": "earth_clouds_soft_stylized.png",
        "asset": "T_Atlas_Earth_Clouds",
        "srgb": False,
        "normalmap": False,
        "note": "구름 마스크. 알파 채널이 없으므로 R 채널이나 밝기를 쓴다.",
    },
    {
        "file": "earth_normal_original.tif",
        "asset": "T_Atlas_Earth_Normal",
        "srgb": False,
        "normalmap": True,
        "note": "노멀. 원본 미수정.",
    },
    {
        "file": "earth_specular_original.tif",
        "asset": "T_Atlas_Earth_Specular",
        "srgb": False,
        "normalmap": False,
        "note": "스페큘러/러프니스 마스크. 원본 미수정.",
    },
]


def project_dir():
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())


def import_one(source_path, asset_name):
    """한 장을 임포트한다. 실패하면 None."""
    task = unreal.AssetImportTask()
    task.filename = source_path
    task.destination_path = DESTINATION
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


def apply_settings(texture, entry):
    """장마다 다른 설정을 반영한다."""
    if entry["normalmap"]:
        texture.set_editor_property("compression_settings",
                                    unreal.TextureCompressionSettings.TC_NORMALMAP)
    else:
        texture.set_editor_property("compression_settings",
                                    unreal.TextureCompressionSettings.TC_DEFAULT)

    # 월드 오브젝트다. UI 그룹이 아니다 — UI 그룹은 밉을 만들지 않아 지구가 멀 때 지글거린다.
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    texture.set_editor_property("srgb", bool(entry["srgb"]))
    texture.set_editor_property("mip_gen_settings",
                                unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP)

    # 🔴 등장방형 규약. X만 감는다.
    try:
        texture.set_editor_property("address_x", unreal.TextureAddress.TA_WRAP)
        texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    except Exception as error:
        unreal.log_warning(
            "[FF] {0}: Address를 못 바꿨다. 에디터에서 X=Wrap, Y=Clamp로 직접 확인한다. ({1})"
            .format(texture.get_name(), error))


def run():
    root = project_dir()
    source_root = os.path.join(root, SOURCE_DIR)

    if not os.path.isdir(source_root):
        unreal.log_error("[FF] 원본 폴더가 없다: {0}".format(SOURCE_DIR))
        return

    imported = 0
    failed = []

    unreal.log("[FF] --- {0} -> {1} ---".format(SOURCE_DIR, DESTINATION))

    for entry in TEXTURES:
        source_path = os.path.join(source_root, entry["file"])
        if not os.path.exists(source_path):
            failed.append("{0} (원본 없음)".format(entry["file"]))
            continue

        texture = import_one(source_path, entry["asset"])
        if texture is None:
            failed.append("{0} (임포트 실패)".format(entry["file"]))
            continue

        apply_settings(texture, entry)
        unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)

        imported += 1
        unreal.log("[FF]   {0}  srgb={1}  normal={2}  — {3}".format(
            entry["asset"], texture.get_editor_property("srgb"), entry["normalmap"], entry["note"]))

    unreal.log("[FF] 지구 텍스처 임포트 완료: {0}/{1}장".format(imported, len(TEXTURES)))

    if failed:
        # 조용히 넘어가지 않는다. 빠진 텍스처는 머티리얼에서 검은 칸으로만 보여 원인을 못 찾는다.
        unreal.log_error("[FF] 실패 {0}건:".format(len(failed)))
        for item in failed:
            unreal.log_error("[FF]   - {0}".format(item))


run()
