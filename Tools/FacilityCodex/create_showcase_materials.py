# create_showcase_materials.py
# 역할: 시설 조합 도감과 보상 표시 작업에서 필요한 에셋 또는 설정을 생성한다.

"""Create the festival codex's restrained coastal showcase stage.

The two new assets are independent of the existing glass/reward/discovery
materials. Run this script inside Unreal before applying the showcase WBP.
It uses real UI translucency, a quiet center for the facility model, and only
a few tidal contours and festival glints around the lower display margin.

Creation is versioned and idempotent: existing owned assets are validated and
returned without recompilation, appended nodes, parameter writes or saves.
"""
import unreal


FOLDER = "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/Materials"
BASE_NAME = "M_FF_CodexShowcase"
STAGE_NAME = "MI_FF_CodexShowcase_Stage"
VERSION_KEY = "FF.FacilityCodex.ShowcaseMaterialVersion"
VERSION = "1"
DEFAULTS = {
    "CenterOpacity": 0.24,
    "EdgeOpacity": 0.48,
    "CornerRadiusPixels": 18.0,
    "EdgeWidthPixels": 1.2,
    "RimStrength": 0.72,
    "TideStrength": 0.70,
    "GlintStrength": 0.68,
    "MotionStrength": 1.0,
}
EXPRESSION_COUNT = len(DEFAULTS) + 5  # UV, Time, custom shader, RGB mask, alpha mask.

SHADER = r"""
float2 uv = saturate(UV);
float pixelU = max(length(float2(ddx(UV.x), ddy(UV.x))), 0.00001);
float pixelV = max(length(float2(ddx(UV.y), ddy(UV.y))), 0.00001);
float2 sizePixels = clamp(1.0 / float2(pixelU, pixelV), 1.0, 16384.0);
float2 halfSize = sizePixels * 0.5;
float radius = clamp(CornerRadiusPixels, 0.0, min(halfSize.x, halfSize.y));
float2 q = abs((uv - 0.5) * sizePixels) - (halfSize - radius);
float distance = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
float aa = max(fwidth(distance), 0.75);
float shape = 1.0 - smoothstep(-aa, aa, distance);
float inward = -distance;
float edgeWidth = max(EdgeWidthPixels, 0.65);
float rim = 1.0 - smoothstep(edgeWidth - aa * 0.45, edgeWidth + aa * 0.65, inward);
float bevel = 1.0 - smoothstep(edgeWidth, edgeWidth + 7.0, inward);

// White sea glass: the center remains transparent and visually unoccupied.
// Opacity never animates, so the displayed facility and its caption stay stable.
float2 centered = abs(uv * 2.0 - 1.0);
float edgeFade = smoothstep(0.30, 0.98, max(centered.x, centered.y));
float opacity = lerp(saturate(CenterOpacity), saturate(EdgeOpacity), edgeFade);
float3 color = lerp(float3(0.975, 0.977, 0.967), float3(0.925, 0.940, 0.915), uv.y * 0.55);
color -= float3(0.016, 0.012, 0.024) * edgeFade;

float phase = Clock * 0.085 * saturate(MotionStrength);
float filmA = 0.5 + 0.5 * sin(uv.x * 5.4 + uv.y * 3.3 + phase);
float filmB = 0.5 + 0.5 * sin(uv.x * 4.0 - uv.y * 5.5 + phase * 0.83 + 1.5);
float3 pearl = lerp(float3(0.80, 0.88, 0.87), float3(0.88, 0.92, 0.80), filmA);
pearl = lerp(pearl, float3(0.96, 0.88, 0.70), filmB * 0.43);
float trim = saturate(RimStrength);
color += pearl * bevel * trim * 0.025;
color = lerp(color, pearl * 0.82, rim * trim);
opacity = lerp(opacity, max(opacity, 0.64), rim * trim * 0.70);

// Irregular coastal contours, shown only along the near shore below the model.
// They are neither a radar grid nor a full circular HUD around the facility.
float2 lagoon = (uv - float2(0.51, 0.68)) / float2(0.355, 0.18);
float lagoonR = length(lagoon);
float angle = atan2(lagoon.y, lagoon.x);
float coastline = 1.0 + sin(angle * 3.0 + 0.7) * 0.052 + sin(angle * 5.0 - 1.1) * 0.021;
float nearShore = smoothstep(-0.06, 0.42, lagoon.y);
float shoreSides = smoothstep(0.07, 0.18, uv.x) * (1.0 - smoothstep(0.84, 0.95, uv.x));
float coast = lagoonR - coastline;
float coastPixel = max(fwidth(coast), 0.002);
float arc1 = exp(-pow(coast / (coastPixel * 0.95), 2.0));
float arc2 = exp(-pow((coast - 0.10) / (coastPixel * 0.90), 2.0));
float arc3 = exp(-pow((coast - 0.20) / (coastPixel * 0.78), 2.0));
float irregularFade = 0.60 + 0.40 * smoothstep(-0.80, 0.65, sin(angle * 2.3 + 0.8));
float shore = (arc1 * 0.24 + arc2 * 0.12 + arc3 * 0.055) * nearShore * shoreSides * irregularFade;
float3 foam = lerp(float3(0.54, 0.66, 0.60), float3(0.70, 0.73, 0.57), filmA * 0.45);
color = lerp(color, foam, shore * saturate(TideStrength) * 0.90);

// Three small warm glints suggest distant festival lights reflected by water.
// Their positions stay fixed; a very slow change in radiance avoids flicker.
float2 lightA = (uv - float2(0.225, 0.818)) * sizePixels;
float2 lightB = (uv - float2(0.792, 0.770)) * sizePixels;
float2 lightC = (uv - float2(0.710, 0.895)) * sizePixels;
float a2 = dot(lightA, lightA);
float b2 = dot(lightB, lightB);
float c2 = dot(lightC, lightC);
float glintA = exp(-a2 * 0.80) * 0.68 + exp(-a2 * 0.060) * 0.048;
float glintB = exp(-b2 * 0.95) * 0.50 + exp(-b2 * 0.075) * 0.035;
float glintC = exp(-c2 * 1.15) * 0.36 + exp(-c2 * 0.090) * 0.028;
float quietLight = 0.88 + sin(phase * 0.62 + 0.8) * 0.12;
color = lerp(color, float3(0.72, 0.62, 0.43), (glintA + glintB + glintC) * quietLight * saturate(GlintStrength) * 0.50);

// The broad optical reflection stays at the margin, leaving the model center matte.
float broad = exp(-pow((uv.x + uv.y * 0.20 - 0.42 - sin(phase * 0.41) * 0.12) * 3.2, 2.0));
color += pearl * broad * edgeFade * trim * 0.015;
return float4(pow(saturate(color), 2.2), shape * saturate(opacity));
"""


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def existing_owned(name, expected_class):
    path = FOLDER + "/" + name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        return None
    asset = unreal.load_asset(path)
    require(asset is not None and isinstance(asset, expected_class), "Unexpected showcase asset class: " + path)
    require(unreal.EditorAssetLibrary.get_metadata_tag(asset, VERSION_KEY) == VERSION,
            "Existing showcase asset lacks the expected ownership marker: " + path)
    return asset


def save_owned(asset):
    unreal.EditorAssetLibrary.set_metadata_tag(asset, VERSION_KEY, VERSION)
    require(unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False), "Could not save " + asset.get_path_name())


def validate_base(material, lib):
    require(material.get_editor_property("material_domain") == unreal.MaterialDomain.MD_UI, "Showcase must remain a UI material")
    require(material.get_editor_property("blend_mode") == unreal.BlendMode.BLEND_TRANSLUCENT, "Showcase must remain genuinely translucent")
    require(lib.get_num_material_expressions(material) == EXPRESSION_COUNT,
            "Unexpected showcase expression count; preserving the existing material for inspection")


def create_base(asset_tools, lib):
    material = existing_owned(BASE_NAME, unreal.Material)
    if material:
        validate_base(material, lib)
        return material
    material = asset_tools.create_asset(BASE_NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    require(material is not None, "Could not create showcase material")
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    sources = {
        "UV": lib.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -760, -240),
        "Clock": lib.create_material_expression(material, unreal.MaterialExpressionTime, -760, -110),
    }
    for index, (name, default) in enumerate(DEFAULTS.items()):
        expression = lib.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -760, 70 + index * 130)
        expression.set_editor_property("parameter_name", name)
        expression.set_editor_property("default_value", default)
        sources[name] = expression
    custom = lib.create_material_expression(material, unreal.MaterialExpressionCustom, -300, -170)
    custom.set_editor_property("description", "Coastal showcase: quiet sea glass, pearl rim, near-shore tidal contours")
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    inputs = []
    for name in sources:
        pin = unreal.CustomInput()
        pin.set_editor_property("input_name", name)
        inputs.append(pin)
    custom.set_editor_property("inputs", inputs)
    custom.set_editor_property("code", SHADER)
    for name, expression in sources.items():
        require(lib.connect_material_expressions(expression, "", custom, name), "Could not connect " + name)
    rgb = lib.create_material_expression(material, unreal.MaterialExpressionComponentMask, 140, -170)
    opacity = lib.create_material_expression(material, unreal.MaterialExpressionComponentMask, 140, 10)
    for key in ("r", "g", "b", "a"):
        rgb.set_editor_property(key, key != "a")
        opacity.set_editor_property(key, key == "a")
    require(lib.connect_material_expressions(custom, "", rgb, ""), "Could not connect showcase RGB")
    require(lib.connect_material_expressions(custom, "", opacity, ""), "Could not connect showcase alpha")
    require(lib.connect_material_property(rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR), "Could not output showcase color")
    require(lib.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY), "Could not output actual showcase opacity")
    validate_base(material, lib)
    lib.recompile_material(material)
    save_owned(material)
    return material


def create_materials():
    # Fail before creation if either destination belongs to someone else.
    existing_owned(BASE_NAME, unreal.Material)
    existing_owned(STAGE_NAME, unreal.MaterialInstanceConstant)
    unreal.EditorAssetLibrary.make_directory(FOLDER)
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    lib = unreal.MaterialEditingLibrary
    material = create_base(asset_tools, lib)
    stage = existing_owned(STAGE_NAME, unreal.MaterialInstanceConstant)
    if stage is None:
        stage = asset_tools.create_asset(STAGE_NAME, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        require(stage is not None, "Could not create showcase stage instance")
        lib.set_material_instance_parent(stage, material)
        for name, value in DEFAULTS.items():
            lib.set_material_instance_scalar_parameter_value(stage, name, float(value))
        save_owned(stage)
    require(stage.get_editor_property("parent") == material, "Showcase stage parent changed")
    for name, expected in DEFAULTS.items():
        actual = lib.get_material_instance_scalar_parameter_value(stage, name)
        require(abs(actual - expected) < 0.0001, "Showcase stage parameter changed: " + name)
    validate_base(material, lib)
    unreal.log("FF_CODEX_SHOWCASE_MATERIALS_PASS nodes={} center_alpha={} edge_alpha={}".format(
        EXPRESSION_COUNT, DEFAULTS["CenterOpacity"], DEFAULTS["EdgeOpacity"]))
    return {"Base": material, "Stage": stage}


if __name__ == "__main__":
    create_materials()
