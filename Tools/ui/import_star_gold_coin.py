# -*- coding: utf-8 -*-
# 역할: 기존 골드 아이콘 두 개를 새 이미지로 교체하고, 해당 아이콘을 쓰는 UI의 색 덧씌우기를 흰색으로 맞춘다.
# 원본 에셋은 먼저 백업하고 경로를 유지해 화면의 기존 아이콘 연결을 보존한다.
"""Reimport the project Gold currency artwork without changing asset paths.

Run in an idle Unreal editor after the new Gold PNG has been saved:
  UnrealEditor-Cmd.exe FestaFiesta.uproject /Engine/Maps/Entry \
    -ExecutePythonScript=Tools/ui/import_star_gold_coin.py -unattended -nopause -nosplash -nullrhi

Only the two existing Gold UTexture2D assets are replaced. Their stable package
paths preserve every UMG reference. The six known consumer Widget Blueprints are
inspected, and only UImage widgets whose brush references either Gold texture
receive a white tint, preserving full-colour star artwork. All original assets
are copied into Saved/GoldCoinImport before any import or WBP change.
"""

import hashlib
import json
import shutil
import struct
from datetime import datetime, timezone
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE = ROOT / "Tools/ui/assets/StarEmbossedGoldCoin.png"
BACKUP_ROOT = ROOT / "Saved/GoldCoinImport"
GOLD_TEXTURES = (
    "/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Metrics/T_FF_LH_Icon_Gold_White_256",
    "/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Badges/T_FF_LH_Badge_Gold_256",
)
GOLD_CONSUMER_WBPS = (
    "/Game/FestaFiesta/Building/FF_WBP_Building/Controls/WBP_FF_BuildTools",
    "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexScreen",
    "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexCard",
    "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/Controls/WBP_FF_CodexCollectionReward",
    "/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveHudScreen",
    "/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveHudStatusBar",
)


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def package_file(package):
    assert package.startswith("/Game/"), package
    return ROOT / "Content" / (package[len("/Game/"):] + ".uasset")


def inspect_png(path):
    with path.open("rb") as handle:
        header = handle.read(33)
    if len(header) < 33 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise RuntimeError("Gold source is not a PNG: " + str(path))
    width, height, bit_depth, colour = struct.unpack(">IIBB", header[16:26])
    if width != height or width < 256 or bit_depth != 8 or colour != 6:
        raise RuntimeError("Gold source must be square 8-bit RGBA PNG; got {}x{}, depth {}, colour {}"
                           .format(width, height, bit_depth, colour))
    return width, height


def backup_inputs():
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    folder = BACKUP_ROOT / stamp
    files = [package_file(path) for path in GOLD_TEXTURES + GOLD_CONSUMER_WBPS]
    missing = [str(path) for path in files if not path.is_file()]
    if missing:
        raise RuntimeError("Existing Gold asset missing; no changes made: " + ", ".join(missing))
    for original in files:
        target = folder / original.relative_to(ROOT)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(original, target)
    return folder, {str(path.relative_to(ROOT)).replace("\\", "/"): sha256(path) for path in files}


def import_gold_texture(package):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(SOURCE))
    task.set_editor_property("destination_path", package.rsplit("/", 1)[0])
    task.set_editor_property("destination_name", package.rsplit("/", 1)[1])
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported = [str(path).split(".")[0] for path in
                (task.get_editor_property("imported_object_paths") or [])]
    if package not in imported:
        raise RuntimeError("Gold import did not preserve package path: {} -> {}".format(package, imported))
    texture = unreal.EditorAssetLibrary.load_asset(package)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError("Gold import did not produce UTexture2D: " + package)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("max_texture_size", 256)
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False):
        raise RuntimeError("Could not save Gold texture: " + package)
    # Blueprint size reports the built texture, which max_texture_size can reduce;
    # SOURCE_SIZE above validates the untouched RGBA source before importing.
    size_x = int(texture.blueprint_get_size_x())
    size_y = int(texture.blueprint_get_size_y())
    if size_x <= 0 or size_x != size_y:
        raise RuntimeError("Gold built texture must remain square: " + package)
    if int(texture.get_editor_property("max_texture_size")) != 256:
        raise RuntimeError("Gold UI max size was not saved: " + package)
    return texture


def owned_images(blueprint):
    for widget in unreal.ObjectIterator(unreal.Image):
        tree = widget.get_outer()
        if tree and tree.get_class().get_name() == "WidgetTree" and tree.get_outer() == blueprint:
            yield widget


def white_slate_color():
    # UE 5.7's default FSlateColor is UseColor_Specified, white. Explicitly
    # set the specified value so a previous tint never colours the new coin.
    result = unreal.SlateColor()
    result.set_editor_property("specified_color", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    return result


def patch_gold_image_tints(textures):
    texture_paths = {texture.get_path_name() for texture in textures}
    edited = {}
    references = {}
    for package in GOLD_CONSUMER_WBPS:
        blueprint = unreal.EditorAssetLibrary.load_asset(package)
        if not blueprint:
            raise RuntimeError("Known Gold WBP missing: " + package)
        names = []
        for widget in owned_images(blueprint):
            brush = widget.get_editor_property("brush")
            resource = brush.get_editor_property("resource_object")
            if resource and resource.get_path_name() in texture_paths:
                names.append(widget.get_name())
                widget.set_color_and_opacity(unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
                widget.set_brush_tint_color(white_slate_color())
                # Brush size, slot, visibility, binding and graph stay unchanged.
        references[package] = names
        if names:
            unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
            if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False):
                raise RuntimeError("Could not save Gold consumer WBP: " + package)
            edited[package] = names
    for required in (GOLD_CONSUMER_WBPS[0], GOLD_CONSUMER_WBPS[4]):
        if not references[required]:
            raise RuntimeError("Expected Gold image reference was not found in " + required)
    return edited, references


def run():
    global SOURCE_SIZE
    if not SOURCE.is_file():
        raise RuntimeError("Gold source PNG missing: " + str(SOURCE))
    SOURCE_SIZE = inspect_png(SOURCE)
    source_hash = sha256(SOURCE)
    backup_dir, before = backup_inputs()
    unreal.log("FF_GOLD_COIN_IMPORT_START source={} {}x{} backup={}".format(
        SOURCE, SOURCE_SIZE[0], SOURCE_SIZE[1], backup_dir))
    textures = [import_gold_texture(package) for package in GOLD_TEXTURES]
    edited, references = patch_gold_image_tints(textures)
    after = {key: sha256(ROOT / key) for key in before}
    report = {
        "result": "PASS",
        "source": str(SOURCE),
        "source_sha256": source_hash,
        "source_dimensions": SOURCE_SIZE,
        "texture_packages": GOLD_TEXTURES,
        "max_texture_size": 256,
        "widget_refs": references,
        "patched_widget_blueprints": edited,
        "before_sha256": before,
        "after_sha256": after,
        "backup": str(backup_dir),
    }
    BACKUP_ROOT.mkdir(parents=True, exist_ok=True)
    (BACKUP_ROOT / "latest.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("FF_GOLD_COIN_IMPORT_PASS textures=2 wbp_tints={}".format(len(edited)))


run()
