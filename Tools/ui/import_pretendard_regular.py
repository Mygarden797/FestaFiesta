# -*- coding: utf-8 -*-
"""Pretendard Regular FontFace를 임포트하고 F_UI_Pretendard에 연결한다.

실행:
    UnrealEditor-Cmd.exe FestaFiesta.uproject \
        -ExecutePythonScript=Tools/ui/import_pretendard_regular.py \
        -unattended -nopause -nosplash -nullrhi

기존 기본 타입페이스가 바뀌지 않도록 Medium을 항상 첫 항목으로 유지한다.
"""

import os
import re

import unreal


SOURCE_REL = "SourceArt/ThirdParty/Pretendard/1.3.9/Pretendard-Regular.otf"
DESTINATION = "/Game/FestaFiesta/UI/Fonts"
FACE_NAME = "Pretendard-Regular"
COMPOSITE_FONT_PATH = DESTINATION + "/F_UI_Pretendard"


def project_dir():
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())


def import_font_face():
    source_file = os.path.normpath(os.path.join(project_dir(), SOURCE_REL))
    if not os.path.isfile(source_file):
        raise RuntimeError("Pretendard Regular source missing: " + source_file)

    task = unreal.AssetImportTask()
    task.filename = source_file
    task.destination_path = DESTINATION
    task.destination_name = FACE_NAME
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    font_face = unreal.EditorAssetLibrary.load_asset(DESTINATION + "/" + FACE_NAME)
    if not isinstance(font_face, unreal.FontFace):
        raise RuntimeError("Pretendard Regular FontFace import failed")

    unreal.EditorAssetLibrary.save_loaded_asset(font_face, only_if_is_dirty=False)
    return font_face


def add_regular_to_composite(font_face):
    composite_asset = unreal.EditorAssetLibrary.load_asset(COMPOSITE_FONT_PATH)
    if not isinstance(composite_asset, unreal.Font):
        raise RuntimeError("Composite font missing: " + COMPOSITE_FONT_PATH)

    # UE 5.7에서는 FCompositeFont의 내부 UPROPERTY가 Python 필드로 노출되지 않는다.
    # 구조체의 정식 ExportText/ImportText 경로로 FontFace 참조만 교체한다.
    composite = composite_asset.get_editor_property("composite_font")
    exported = composite.export_text()
    names = re.findall(r'\(Name="([^"]+)"', exported)
    if not names or names[0] != "Medium":
        raise RuntimeError("F_UI_Pretendard default order is unsafe: " + ", ".join(names))

    face_reference = font_face.get_path_name()
    replacement = '(Name="Regular",Font=(FontFaceAsset="/Script/Engine.FontFace\'{0}\'"))'.format(
        face_reference)
    regular_pattern = r'\(Name="Regular",Font=\(FontFaceAsset="[^"]+"\)\)'

    if re.search(regular_pattern, exported):
        updated = re.sub(regular_pattern, replacement, exported, count=1)
    else:
        marker = ')),FallbackTypeface='
        if marker not in exported:
            raise RuntimeError("Unable to locate DefaultTypeface font list")
        updated = exported.replace(marker, ',' + replacement + marker, 1)

    updated_composite = unreal.CompositeFont()
    updated_composite.import_text(updated)
    composite_asset.set_editor_property("composite_font", updated_composite)
    unreal.EditorAssetLibrary.save_loaded_asset(composite_asset, only_if_is_dirty=False)

    saved_composite = unreal.EditorAssetLibrary.load_asset(COMPOSITE_FONT_PATH)
    saved_text = saved_composite.get_editor_property("composite_font").export_text()
    saved_names = re.findall(r'\(Name="([^"]+)"', saved_text)
    if saved_names[:1] != ["Medium"] or face_reference not in saved_text:
        raise RuntimeError("Composite font verification failed: " + saved_text)

    unreal.log("[FF-FONT] F_UI_Pretendard typefaces: " + ", ".join(saved_names))


def run():
    font_face = import_font_face()
    add_regular_to_composite(font_face)
    unreal.log("[FF-FONT] Pretendard Regular import complete: " + font_face.get_path_name())


run()
