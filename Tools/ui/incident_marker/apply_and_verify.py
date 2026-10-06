# -*- coding: utf-8 -*-
"""사건 월드 마커의 Unreal 에셋을 만든다: 기호 텍스처 3장 임포트 + 남은 시간 링 UI 머티리얼.

실행 (에디터를 닫고):
    "C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" FestaFiesta.uproject \
        -ExecutePythonScript=Tools/ui/incident_marker/apply_and_verify.py \
        -unattended -nopause -nosplash -nullrhi

입력은 Design/UXUI/IncidentMarker_LightTeal/asset-manifest.json (generate.js가 만든다).
텍스처는 반복 실행 시 소스 PNG로 갱신한다. 머티리얼은 이미 있으면 덮지 않고 검증만 한다 —
에디터에서 손본 그래프를 지우지 않기 위해서다. 새로 만들려면 에셋을 지우고 다시 돌린다.
링 계산은 Custom(HLSL) 노드 두 개에 담았다. 파라미터는 전부 노드 밖에 있어 MID로 바꾼다.
결과는 Saved/IncidentMarker/report.json.
"""

import json
import os
import traceback

import unreal


MANIFEST_REL = "Design/UXUI/IncidentMarker_LightTeal/asset-manifest.json"
MATERIAL_DIR = "/Game/FestaFiesta/UI/Material"
MATERIAL_NAME = "M_UI_IncidentTimerRing"
MATERIAL_PATH = MATERIAL_DIR + "/" + MATERIAL_NAME

ANGLE_HLSL = "frac(atan2(D.x, -D.y) / 6.28318530718 + 1.0)"

COLOR_HLSL = """float2 D = UV - Center.xy;
float A = {angle};
float Sweep = step(A, Progress) * step(0.0001, Progress);
return lerp(TrackColor.rgb, FillColor.rgb, Sweep);""".format(angle=ANGLE_HLSL)

OPACITY_HLSL = """float2 D = UV - Center.xy;
float R = length(D);
float AA = max(fwidth(R), 0.0001);
float Ring = smoothstep(Inner - AA, Inner, R) * (1.0 - smoothstep(Outer, Outer + AA, R));
float A = {angle};
float Sweep = step(A, Progress) * step(0.0001, Progress);
return Ring * lerp(TrackOpacity, 1.0, Sweep);""".format(angle=ANGLE_HLSL)


def project_dir():
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())


def enum_name(value):
    text = str(value)
    if "." in text:
        text = text.split(".")[-1]
    return text.split(":")[0].strip("<> ")


def import_texture(source_file, destination_path, asset_name):
    task = unreal.AssetImportTask()
    task.filename = source_file
    task.destination_path = destination_path
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    for object_path in task.get_editor_property("imported_object_paths") or []:
        asset = unreal.EditorAssetLibrary.load_asset(object_path)
        if isinstance(asset, unreal.Texture2D):
            return asset
    return None


def apply_texture_settings(texture, entry):
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("srgb", bool(entry.get("srgb", True)))
    texture.set_editor_property("never_stream", bool(entry.get("never_stream", True)))
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)


def import_textures(manifest, manifest_dir, report):
    for entry in manifest["assets"]:
        source = os.path.normpath(os.path.join(manifest_dir, entry["file"]))
        destination = entry["unreal_destination"]
        folder, name = destination.rsplit("/", 1)
        row = {"asset": destination, "source": entry["file"]}
        report["textures"].append(row)

        if not os.path.isfile(source):
            row["status"] = "FAIL: source missing"
            continue

        texture = import_texture(source, folder, name)
        if texture is None:
            row["status"] = "FAIL: import"
            continue

        apply_texture_settings(texture, entry)
        unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)

        row.update({
            "status": "OK",
            "size": [int(texture.blueprint_get_size_x()), int(texture.blueprint_get_size_y())],
            "compression": enum_name(texture.get_editor_property("compression_settings")),
            "lod_group": enum_name(texture.get_editor_property("lod_group")),
            "mips": enum_name(texture.get_editor_property("mip_gen_settings")),
            "address": enum_name(texture.get_editor_property("address_x")),
            "srgb": bool(texture.get_editor_property("srgb")),
        })
        expected = (entry["width"], entry["height"])
        if tuple(row["size"]) != expected:
            row["status"] = "FAIL: size {0} != {1}".format(row["size"], list(expected))


def scalar_param(material, name, value, x, y, group="Ring"):
    node = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", value)
    node.set_editor_property("group", group)
    return node


def vector_param(material, name, color, x, y, group="Ring"):
    node = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVectorParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", color)
    node.set_editor_property("group", group)
    return node


def custom_node(material, description, code, output_type, input_names, x, y):
    node = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionCustom, x, y)
    node.set_editor_property("description", description)
    node.set_editor_property("code", code)
    node.set_editor_property("output_type", output_type)
    inputs = []
    for input_name in input_names:
        custom_input = unreal.CustomInput()
        custom_input.set_editor_property("input_name", input_name)
        inputs.append(custom_input)
    node.set_editor_property("inputs", inputs)
    return node


def connect(src, dst, pin, report):
    if not unreal.MaterialEditingLibrary.connect_material_expressions(src, "", dst, pin):
        report["material"]["errors"].append("connect failed: {0} -> {1}".format(src.get_name(), pin))


def build_material(manifest, report):
    runtime = manifest["runtime"]["ring_material"]["params"]
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = asset_tools.create_asset(MATERIAL_NAME, MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        report["material"]["errors"].append("create_asset failed")
        return None

    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

    uv = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureCoordinate, -900, -100)
    center = vector_param(material, "CenterUV", unreal.LinearColor(*runtime["CenterUV"], 0.0, 0.0), -900, 0)
    progress = scalar_param(material, "Progress", 1.0, -900, 150)
    inner = scalar_param(material, "InnerRadius", runtime["InnerRadius"], -900, 250)
    outer = scalar_param(material, "OuterRadius", runtime["OuterRadius"], -900, 350)
    fill = vector_param(material, "FillColor", unreal.LinearColor(1.0, 1.0, 1.0, 1.0), -900, 450)
    track = vector_param(material, "TrackColor", unreal.LinearColor(1.0, 1.0, 1.0, 1.0), -900, 650)
    track_opacity = scalar_param(material, "TrackOpacity", runtime["TrackOpacity"], -900, 850)

    color = custom_node(material, "IncidentRingColor", COLOR_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                        ["UV", "Center", "Progress", "FillColor", "TrackColor"], -400, 0)
    opacity = custom_node(material, "IncidentRingOpacity", OPACITY_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT1,
                          ["UV", "Center", "Progress", "Inner", "Outer", "TrackOpacity"], -400, 400)

    for src, pin in ((uv, "UV"), (center, "Center"), (progress, "Progress"), (fill, "FillColor"), (track, "TrackColor")):
        connect(src, color, pin, report)
    for src, pin in ((uv, "UV"), (center, "Center"), (progress, "Progress"), (inner, "Inner"),
                     (outer, "Outer"), (track_opacity, "TrackOpacity")):
        connect(src, opacity, pin, report)

    lib = unreal.MaterialEditingLibrary
    if not lib.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        report["material"]["errors"].append("connect failed: color -> Final Color")
    if not lib.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY):
        report["material"]["errors"].append("connect failed: opacity -> Opacity")

    lib.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    report["material"]["created"] = True
    return material


def verify_material(material, report):
    lib = unreal.MaterialEditingLibrary
    info = report["material"]
    info["domain"] = enum_name(material.get_editor_property("material_domain"))
    info["blend"] = enum_name(material.get_editor_property("blend_mode"))
    info["scalar_params"] = sorted(str(n) for n in lib.get_scalar_parameter_names(material))
    info["vector_params"] = sorted(str(n) for n in lib.get_vector_parameter_names(material))
    info["expressions"] = lib.get_num_material_expressions(material)

    expected_scalars = {"Progress", "InnerRadius", "OuterRadius", "TrackOpacity"}
    expected_vectors = {"CenterUV", "FillColor", "TrackColor"}
    if info["domain"] != "MD_UI":
        info["errors"].append("domain is " + info["domain"])
    if info["blend"] != "BLEND_TRANSLUCENT":
        info["errors"].append("blend is " + info["blend"])
    missing = (expected_scalars - set(info["scalar_params"])) | (expected_vectors - set(info["vector_params"]))
    if missing:
        info["errors"].append("missing params: " + ", ".join(sorted(missing)))

    try:
        stats = lib.get_statistics(material)
        info["pixel_instructions"] = int(stats.get_editor_property("num_pixel_shader_instructions"))
        if info["pixel_instructions"] <= 0:
            info["errors"].append("no pixel shader instructions (compile failed?)")
    except Exception as error:
        info["stats_error"] = str(error)


def run():
    report = {"status": "running", "textures": [], "material": {"path": MATERIAL_PATH, "created": False, "errors": []}}
    out_dir = os.path.join(project_dir(), "Saved/IncidentMarker")
    os.makedirs(out_dir, exist_ok=True)
    try:
        manifest_path = os.path.join(project_dir(), MANIFEST_REL)
        with open(manifest_path, "r", encoding="utf-8") as handle:
            manifest = json.load(handle)

        import_textures(manifest, os.path.dirname(manifest_path), report)

        if unreal.EditorAssetLibrary.does_asset_exist(MATERIAL_PATH):
            material = unreal.EditorAssetLibrary.load_asset(MATERIAL_PATH)
            report["material"]["note"] = "existing asset kept"
        else:
            material = build_material(manifest, report)
        if material is not None:
            verify_material(material, report)

        failed = [t for t in report["textures"] if t.get("status") != "OK"] or report["material"]["errors"]
        report["status"] = "FAIL" if failed or material is None else "PASS"
    except Exception:
        report["status"] = "FAIL"
        report["exception"] = traceback.format_exc()

    with open(os.path.join(out_dir, "report.json"), "w", encoding="utf-8") as handle:
        json.dump(report, handle, ensure_ascii=False, indent=2)
    unreal.log("[FF-IncidentMarker] {0}\n{1}".format(report["status"], json.dumps(report, ensure_ascii=False, indent=2)))


run()
