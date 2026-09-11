# -*- coding: utf-8 -*-
"""타이틀/Atlas용 Citrus Pop v2 지구 텍스처 네 장을 UE 5.7에 임포트한다.

기존 Atlas 텍스처를 건드리지 않고 별도의 버전 경로를 사용한다.

이미 열린 에디터의 Output Log를 Python 모드로 바꾼 뒤 실행:
    exec(open(r"C:/UE5/8th-Team13-Final-Project/Tools/ui/import_earth_citrus_v2.py", encoding="utf-8").read())

또는 정상 에디터 부팅 경로에서 -ExecutePythonScript를 사용한다. UE 5.7에서
-run=pythonscript는 Interchange가 Slate를 만지며 크래시할 수 있으므로 사용하지 않는다.
스크립트는 에디터를 자동 종료하지 않는다.
"""

import os
import json
import traceback

import unreal


SOURCE_DIR = "SourceArt/UI/Title/CasualReliefParts_v2/Earth"
DESTINATION = "/Game/FestaFiesta/UI/Atlas/Earth/CitrusPop_v2/Textures"

REPORT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Saved/CitrusPop_v2_import_report.json")

TEXTURES = (
    {
        "file": "T_FF_Earth_CitrusPop_v2_Albedo_2048x1024.png",
        "asset": "T_FF_Earth_CitrusPop_v2_Albedo",
        "srgb": True,
        "compression": unreal.TextureCompressionSettings.TC_DEFAULT,
    },
    {
        "file": "T_FF_Earth_CitrusPop_v2_Height16_2048x1024.png",
        "asset": "T_FF_Earth_CitrusPop_v2_Height16",
        "srgb": False,
        "compression": unreal.TextureCompressionSettings.TC_GRAYSCALE,
    },
    {
        "file": "T_FF_Earth_CitrusPop_v2_Normal_DX_2048x1024.png",
        "asset": "T_FF_Earth_CitrusPop_v2_Normal_DX",
        "srgb": False,
        "compression": unreal.TextureCompressionSettings.TC_NORMALMAP,
    },
    {
        "file": "T_FF_Earth_CitrusPop_v2_Masks_2048x1024.png",
        "asset": "T_FF_Earth_CitrusPop_v2_Masks",
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
            # 비동기 텍스처 컴파일 중에는 32×32 자리표시 리소스가 조회된다.
            # 저장이 컴파일 완료를 기다린 뒤 원래 크기를 검증한다.
            if not unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False):
                raise RuntimeError("텍스처 저장 실패: " + texture.get_path_name())
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


def import_sphere():
    """기존 지구본을 교체하지 않고 고밀도 OBJ만 별도 경로에 저장한다."""
    name = "SM_FF_Earth_UVSphere_R50cm_256x128"
    path = os.path.join(project_dir(), "SourceArt/UI/Title/CasualReliefParts_v2/Mesh", name + ".obj")
    options = unreal.FbxImportUI()
    options.set_editor_property("automated_import_should_detect_type", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    data = options.get_editor_property("static_mesh_import_data")
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("generate_lightmap_u_vs", False)
    data.set_editor_property("build_nanite", False)
    data.set_editor_property("auto_generate_collision", False)
    data.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    task = unreal.AssetImportTask()
    for key, value in {
        "filename": path, "destination_path": "/Game/FestaFiesta/UI/Atlas/Earth/CitrusPop_v2/Mesh",
        "destination_name": name, "automated": True, "replace_existing": True,
        "save": False, "factory": unreal.FbxFactory(), "options": options,
    }.items():
        task.set_editor_property(key, value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    meshes = [item for item in task.get_objects() if isinstance(item, unreal.StaticMesh)]
    if len(meshes) != 1:
        raise RuntimeError("구체 임포트 결과가 1개가 아닙니다: " + str(task.get_editor_property("imported_object_paths")))
    mesh = meshes[0]
    if not unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False):
        raise RuntimeError("구체 저장 실패")
    return {"path": mesh.get_path_name(), "bounds": str(mesh.get_bounds())}


def execute():
    result = {"status": "running", "textures": [], "countries": []}
    try:
        run()
        for entry in TEXTURES:
            texture = unreal.load_asset(DESTINATION + "/" + entry["asset"])
            errors = validate_texture(texture, entry)
            if errors:
                raise RuntimeError(str(errors))
            result["textures"].append({"path": texture.get_path_name(), "size": [texture.blueprint_get_size_x(), texture.blueprint_get_size_y()], "srgb": texture.get_editor_property("srgb"), "compression": str(texture.get_editor_property("compression_settings"))})
        result["mesh"] = import_sphere()
        catalog = unreal.load_asset("/Game/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas")
        for country in catalog.get_editor_property("countries"):
            location = country.get_editor_property("location")
            result["countries"].append({"id": str(country.get_editor_property("country_id")), "name": str(country.get_editor_property("display_name")), "latitude": location.get_editor_property("latitude"), "longitude": location.get_editor_property("longitude"), "mesh": str(country.get_editor_property("landmark_mesh"))})
        result["status"] = "complete"
    except Exception:
        result["status"] = "failed"
        result["error"] = traceback.format_exc()
        unreal.log_error(result["error"])
    finally:
        with open(REPORT, "w", encoding="utf-8") as handle:
            json.dump(result, handle, ensure_ascii=False, indent=2)
        unreal.log("[FF_CITRUS_V2] " + result["status"] + " " + REPORT)


execute()



