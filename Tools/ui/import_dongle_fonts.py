# -*- coding: utf-8 -*-
"""Dongle Light·Regular·Bold를 FontFace와 Composite Font로 임포트한다.

실행:
    UnrealEditor-Cmd.exe FestaFiesta.uproject \
        -ExecutePythonScript=Tools/ui/import_dongle_fonts.py \
        -unattended -nopause -nosplash -nullrhi

F_UI_Dongle의 기본 Typeface는 Regular이며 목록 순서는 Regular·Light·Bold다.
"""

import os
import re

import unreal


SOURCE_ROOT_REL = "SourceArt/ThirdParty/GoogleFonts/Dongle"
DESTINATION = "/Game/FestaFiesta/UI/Fonts"
TEMPLATE_FONT_PATH = DESTINATION + "/F_UI_Pretendard"
COMPOSITE_FONT_PATH = DESTINATION + "/F_UI_Dongle"
FACES = (
    ("Dongle-Light.ttf", "Dongle-Light"),
    ("Dongle-Regular.ttf", "Dongle-Regular"),
    ("Dongle-Bold.ttf", "Dongle-Bold"),
)


def project_dir():
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())


def import_font_face(source_name, asset_name):
    source_file = os.path.normpath(
        os.path.join(project_dir(), SOURCE_ROOT_REL, source_name))
    if not os.path.isfile(source_file):
        raise RuntimeError("Dongle source missing: " + source_file)

    task = unreal.AssetImportTask()
    task.filename = source_file
    task.destination_path = DESTINATION
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    font_face = unreal.EditorAssetLibrary.load_asset(DESTINATION + "/" + asset_name)
    if not isinstance(font_face, unreal.FontFace):
        raise RuntimeError("Dongle FontFace import failed: " + asset_name)

    unreal.EditorAssetLibrary.save_loaded_asset(font_face, only_if_is_dirty=False)
    return font_face


def typeface_entry(name, font_face):
    return '(Name="{0}",Font=(FontFaceAsset="/Script/Engine.FontFace\'{1}\'"))'.format(
        name, font_face.get_path_name())


def build_composite(font_faces):
    template = unreal.EditorAssetLibrary.load_asset(TEMPLATE_FONT_PATH)
    if not isinstance(template, unreal.Font):
        raise RuntimeError("Composite font template missing: " + TEMPLATE_FONT_PATH)

    if not unreal.EditorAssetLibrary.does_asset_exist(COMPOSITE_FONT_PATH):
        duplicated = unreal.EditorAssetLibrary.duplicate_asset(
            TEMPLATE_FONT_PATH, COMPOSITE_FONT_PATH)
        if not duplicated:
            raise RuntimeError("Unable to create composite font: " + COMPOSITE_FONT_PATH)

    composite_asset = unreal.EditorAssetLibrary.load_asset(COMPOSITE_FONT_PATH)
    if not isinstance(composite_asset, unreal.Font):
        raise RuntimeError("Composite font missing/not Font: " + COMPOSITE_FONT_PATH)

    # UE 5.7 Python은 FCompositeFont의 내부 UPROPERTY를 직접 노출하지 않으므로,
    # 검증된 Pretendard Composite의 ExportText 구조를 재사용해 세 Typeface만 교체한다.
    exported = template.get_editor_property("composite_font").export_text()
    replacements = {
        "Medium": typeface_entry("Regular", font_faces["Regular"]),
        "SemiBold": typeface_entry("Light", font_faces["Light"]),
        "Regular": typeface_entry("Bold", font_faces["Bold"]),
    }

    matched_names = []

    def replace_typeface(match):
        source_name = match.group(1)
        matched_names.append(source_name)
        return replacements[source_name]

    pattern = r'\(Name="(Medium|SemiBold|Regular)",Font=\(FontFaceAsset="[^"]+"\)\)'
    updated = re.sub(pattern, replace_typeface, exported)
    if matched_names != ["Medium", "SemiBold", "Regular"]:
        raise RuntimeError(
            "Template typeface order mismatch: " + ", ".join(matched_names))

    updated_composite = unreal.CompositeFont()
    updated_composite.import_text(updated)
    composite_asset.set_editor_property("composite_font", updated_composite)
    unreal.EditorAssetLibrary.save_loaded_asset(composite_asset, only_if_is_dirty=False)

    saved = unreal.EditorAssetLibrary.load_asset(COMPOSITE_FONT_PATH)
    saved_text = saved.get_editor_property("composite_font").export_text()
    saved_names = re.findall(r'\(Name="([^"]+)"', saved_text)
    expected_names = ["Regular", "Light", "Bold"]
    if saved_names != expected_names:
        raise RuntimeError(
            "F_UI_Dongle typeface order mismatch: " + ", ".join(saved_names))

    for weight, font_face in font_faces.items():
        if font_face.get_path_name() not in saved_text:
            raise RuntimeError("F_UI_Dongle reference missing: " + weight)

    unreal.log("[FF-DONGLE] F_UI_Dongle typefaces: " + ", ".join(saved_names))


def run():
    imported = {}
    for source_name, asset_name in FACES:
        weight = asset_name.split("-")[-1]
        imported[weight] = import_font_face(source_name, asset_name)
        unreal.log("[FF-DONGLE] FontFace: " + imported[weight].get_path_name())

    build_composite(imported)
    unreal.log("[FF-DONGLE] Import complete: 3 FontFaces + F_UI_Dongle")


run()
